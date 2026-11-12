#include "ui/ChannelRack.h"
#include "ui/Dialogs.h"
#include "app/CommandIds.h"
#include "engine/AudioEngine.h"
#include "app/Preferences.h"
#include "model/Automation.h"

namespace harmony::ui
{
namespace
{
    constexpr int headerHeight = 40;
    constexpr int rowHeight = 36;
    constexpr int stepKey = 60;

    bool isStepNote (const NoteData& n)
    {
        return n.key == stepKey && n.length <= ticksPerStep && n.start % ticksPerStep == 0;
    }
}

class ChannelRow final : public juce::Component
{
public:
    ChannelRow (ChannelRack& r, juce::ValueTree ch)
        : rack (r), channel (std::move (ch))
    {
        auto* undo = rack.ctx.project.um();
        pan.bind (channel, id::pan, undo);
        pan.describe = [] (double v) { return v == 0.0 ? juce::String ("Centre") : juce::String (std::abs (v) * 100.0, 0) + (v < 0 ? "% L" : "% R"); };
        volume.bind (channel, id::volume, undo);
        volume.describe = [] (double v) { return formatDecibels ((float) v); };
        addAndMakeVisible (pan);
        addAndMakeVisible (volume);

        pan.onContextMenu = [this] { showAutomationMenu (automation::Kind::channelPan); };
        volume.onContextMenu = [this] { showAutomationMenu (automation::Kind::channelVolume); };

        insertBox.setValue ((int) channel[id::insert], juce::dontSendNotification);
        insertBox.setTooltip ("Mixer insert this channel is routed to (0 = Master). Drag or scroll to change.");
        insertBox.onGestureStart = [this] { rack.ctx.project.beginTransaction ("Route channel"); };
        insertBox.onChange = [this] (double v) { channel.setProperty (id::insert, (int) v, rack.ctx.project.um()); };
        addAndMakeVisible (insertBox);
    }

    int uid() const { return channel[id::uid]; }

    bool isSoundFont() const { return channel[id::kind].toString() == kind::soundfont; }

    juce::Rectangle<int> presetArea() const
    {
        auto r = nameArea();
        return r.withTrimmedTop (r.getHeight() / 2).withTrimmedLeft (16).withTrimmedRight (6);
    }

    juce::String presetName() const
    {
        const auto presets = rack.ctx.sync.soundFontPresets (uid());
        const int index = juce::jmax (0, (int) channel[id::sfPreset]);
        if (presets.isEmpty())
            return "Loading presets...";
        return juce::isPositiveAndBelow (index, presets.size()) ? presets[index] : "Preset " + juce::String (index + 1);
    }

    void showPresetMenu()
    {
        const auto presets = rack.ctx.sync.soundFontPresets (uid());
        juce::PopupMenu menu;
        menu.addItem ("Search presets...", [this] { if (rack.ctx.openChannelEditor) rack.ctx.openChannelEditor (uid()); });
        menu.addSeparator();

        const int current = (int) channel[id::sfPreset];
        constexpr int perPage = 32;
        for (int page = 0; page * perPage < presets.size(); ++page)
        {
            juce::PopupMenu sub;
            for (int i = page * perPage; i < juce::jmin (presets.size(), (page + 1) * perPage); ++i)
                sub.addItem (juce::String (i + 1).paddedLeft ('0', 3) + "  " + presets[i], true, i == current, [this, i]
                {
                    rack.ctx.project.beginTransaction ("Change preset");
                    channel.setProperty (id::sfPreset, i, rack.ctx.project.um());
                    auto& engine = rack.ctx.engine;
                    engine.sendPreviewNote (uid(), 60, 0.8f, true);
                    juce::Timer::callAfterDelay (300, [&engine, u = uid()] { engine.sendPreviewNote (u, 60, 0.0f, false); });
                });

            const int first = page * perPage + 1;
            const int last = juce::jmin (presets.size(), (page + 1) * perPage);
            const bool containsCurrent = current >= first - 1 && current < last;
            menu.addSubMenu (juce::String (first) + " - " + juce::String (last) + (containsCurrent ? "   (current)" : ""), sub);
        }

        if (presets.isEmpty())
            menu.addItem ("The SoundFont is still loading", false, false, nullptr);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withTargetScreenArea (localAreaToGlobal (presetArea())));
    }

    void showAutomationMenu (automation::Kind kind)
    {
        juce::PopupMenu menu;
        const bool isVolume = kind == automation::Kind::channelVolume;
        menu.addItem ("Create automation clip", rack.ctx.createAutomation != nullptr, false, [this, kind, isVolume]
        {
            if (rack.ctx.createAutomation)
                rack.ctx.createAutomation ({ kind, uid(), -1 }, channel[id::name].toString() + (isVolume ? " volume" : " pan"));
        });
        menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
    }

    void showAutomatedValues()
    {
        if (auto* node = rack.ctx.engine.findChannelNode (uid()))
        {
            const float v = node->gain.automatedVolume.load();
            const float p = node->gain.automatedPan.load();
            volume.showTransientValue (v >= 0.0f ? v : (float) channel.getProperty (id::volume, 0.78f));
            pan.showTransientValue (p >= -1.0f ? p : (float) channel.getProperty (id::pan, 0.0f));
        }
    }

    void refresh()
    {
        insertBox.setValue ((int) channel[id::insert], juce::dontSendNotification);
        repaint();
    }

    void setActivity (float level)
    {
        const float v = juce::jlimit (0.0f, 1.0f, level);
        activity = v > activity ? v : activity * 0.8f;
        if (activity > 0.01f || v > 0.0f)
            repaint (ledArea().expanded (2));
    }

    void setPlayStep (int step)
    {
        if (step != playStep)
        {
            playStep = step;
            repaint (stepsArea());
        }
    }

    juce::Rectangle<int> ledArea() const    { return { 10, (getHeight() - 12) / 2, 12, 12 }; }
    juce::Rectangle<int> nameArea() const   { return { 152, 5, 156, getHeight() - 10 }; }
    juce::Rectangle<int> stepsArea() const  { return getLocalBounds().withTrimmedLeft (318).withTrimmedRight (10).reduced (0, 6); }

    void resized() override
    {
        pan.setBounds (28, 4, 28, 28);
        volume.setBounds (58, 4, 28, 28);
        insertBox.setBounds (92, 7, 50, 22);
    }

    int numSteps() const
    {
        const auto& project = rack.ctx.project;
        const auto pattern = project.currentPattern();
        const int fromProp = juce::jmax (1, (int) pattern.getProperty (id::steps, 16));
        const int fromLength = (int) (project.patternLength (pattern) / ticksPerStep);
        return juce::jlimit (1, 128, juce::jmax (fromProp, juce::jmin (fromLength, 128)));
    }

    std::vector<juce::ValueTree> notes() const
    {
        std::vector<juce::ValueTree> result;
        for (auto n : rack.ctx.project.currentPattern())
            if ((int) n[id::channel] == uid())
                result.push_back (n);
        return result;
    }

    bool isStepView() const
    {
        for (auto& n : notes())
            if (! isStepNote (Project::readNote (n)))
                return false;
        return true;
    }

    juce::Rectangle<float> stepRect (int index, int count) const
    {
        const auto area = stepsArea().toFloat();
        const int groups = (count + 3) / 4;
        const float groupGap = 6.0f;
        const float w = juce::jmin (28.0f, (area.getWidth() - groupGap * (float) (groups - 1)) / (float) count);
        const float x = area.getX() + (float) index * w + groupGap * (float) (index / 4);
        return { x, area.getY(), juce::jmax (2.0f, w - 3.0f), area.getHeight() };
    }

    int stepAt (juce::Point<float> p) const
    {
        const int count = numSteps();
        for (int i = 0; i < count; ++i)
            if (stepRect (i, count).expanded (1.5f, 0.0f).contains (p))
                return i;
        return -1;
    }

    juce::ValueTree findStep (int step) const
    {
        for (auto& n : notes())
        {
            const auto d = Project::readNote (n);
            if (isStepNote (d) && d.start == (juce::int64) step * ticksPerStep)
                return n;
        }
        return {};
    }

    void setStep (int step, bool on)
    {
        auto existing = findStep (step);
        auto& project = rack.ctx.project;

        if (on && ! existing.isValid())
        {
            NoteData n;
            n.channel = uid();
            n.key = stepKey;
            n.start = (juce::int64) step * ticksPerStep;
            n.length = ticksPerStep;
            project.addNote (project.currentPattern(), n);

            auto& engine = rack.ctx.engine;
            const auto& settings = rack.ctx.settings;
            if (prefs::previewNotes.get (settings) && (! engine.isPlaying() || prefs::previewWhilePlaying.get (settings)))
            {
                engine.sendPreviewNote (uid(), stepKey, 0.78f, true);
                juce::Timer::callAfterDelay (120, [&engine, u = uid()] { engine.sendPreviewNote (u, stepKey, 0.0f, false); });
            }
        }
        else if (! on && existing.isValid())
        {
            project.currentPattern().removeChild (existing, project.um());
        }
    }

    void paint (juce::Graphics& g) override
    {
        const bool selected = rack.ctx.project.selectedChannel() == uid();
        const auto colour = juce::Colour::fromString (channel[id::colour].toString());
        auto r = getLocalBounds();

        g.setColour (selected ? colours::raised.brighter (0.04f) : colours::panel);
        g.fillRect (r);
        g.setColour (colours::lineSoft);
        g.fillRect (r.removeFromBottom (1));

        if (selected)
        {
            g.setColour (colours::accent);
            g.fillRoundedRectangle (0.0f, 6.0f, 3.0f, (float) getHeight() - 12.0f, 1.5f);
        }

        const bool muted = channel[id::mute];
        const bool soloed = channel[id::solo];
        const auto led = ledArea().toFloat();
        g.setColour (muted ? colours::control.brighter (0.2f) : (soloed ? colours::warning : colours::success.withAlpha (0.5f + activity * 0.5f)));
        g.fillEllipse (led);
        if (! muted)
        {
            g.setColour (colours::success.withAlpha (activity * 0.35f));
            g.fillEllipse (led.expanded (2.0f));
        }

        auto name = nameArea().toFloat();
        g.setColour (colour.withAlpha (selected ? 0.24f : 0.14f));
        g.fillRoundedRectangle (name, 6.0f);
        g.setColour (colour.withAlpha (0.9f));
        g.fillRoundedRectangle (name.removeFromLeft (4.0f).withTrimmedTop (4.0f).withTrimmedBottom (4.0f).translated (4.0f, 0.0f), 2.0f);
        g.setColour (muted ? colours::textFaint : colours::text);
        g.setFont (font (11.5f, Weight::medium));
        if (isSoundFont())
        {
            auto top = nameArea().withTrimmedLeft (16).withTrimmedRight (8);
            g.drawFittedText (channel[id::name].toString(), top.removeFromTop (top.getHeight() / 2 + 2), juce::Justification::bottomLeft, 1);
            auto preset = presetArea();
            g.setColour (hoverPreset ? colours::accent : colours::textDim);
            g.setFont (font (9.0f, Weight::medium));
            g.drawText (presetName(), preset.withTrimmedRight (14), juce::Justification::centredLeft, true);
            drawIcon (g, icons::chevronDown(), preset.removeFromRight (12).toFloat().withSizeKeepingCentre (10.0f, 10.0f),
                      hoverPreset ? colours::accent : colours::textFaint);
        }
        else
        {
            g.drawFittedText (channel[id::name].toString(), nameArea().withTrimmedLeft (16).withTrimmedRight (8), juce::Justification::centredLeft, 1);
        }

        const bool loaded = rack.ctx.engine.findChannelNode (uid()) != nullptr || rack.ctx.sync.signatureFor (channel).isEmpty();
        if (! loaded)
        {
            g.setColour (colours::warning);
            g.fillEllipse (nameArea().getRight() - 12.0f, nameArea().getCentreY() - 2.5f, 5.0f, 5.0f);
        }

        if (isStepView())
            paintSteps (g, colour);
        else
            paintPreview (g, colour);
    }

    void paintSteps (juce::Graphics& g, juce::Colour colour)
    {
        const int count = numSteps();
        std::vector<bool> on ((size_t) count, false);
        std::vector<float> velocity ((size_t) count, 0.0f);

        for (auto& n : notes())
        {
            const auto d = Project::readNote (n);
            const int s = (int) (d.start / ticksPerStep);
            if (s >= 0 && s < count && ! d.muted)
            {
                on[(size_t) s] = true;
                velocity[(size_t) s] = d.velocity;
            }
        }

        for (int i = 0; i < count; ++i)
        {
            auto r = stepRect (i, count);
            const bool darkGroup = (i / 4) % 2 == 1;
            juce::Colour fill = on[(size_t) i] ? colour.withMultipliedBrightness (0.7f + 0.3f * velocity[(size_t) i])
                                               : (darkGroup ? juce::Colour (0xff1b1b20) : juce::Colour (0xff232329));
            if (i == playStep)
                fill = on[(size_t) i] ? fill.brighter (0.35f) : fill.brighter (0.12f);

            g.setColour (fill);
            g.fillRoundedRectangle (r, 4.0f);

            if (on[(size_t) i])
            {
                g.setColour (juce::Colours::white.withAlpha (0.25f));
                g.fillRoundedRectangle (r.withHeight (3.0f).reduced (3.0f, 0.0f).translated (0.0f, 3.0f), 1.5f);
            }
        }
    }

    void paintPreview (juce::Graphics& g, juce::Colour colour)
    {
        const auto area = stepsArea().toFloat();
        g.setColour (juce::Colour (0xff17171b));
        g.fillRoundedRectangle (area, 5.0f);

        auto list = notes();
        int lo = 127, hi = 0;
        for (auto& n : list)
        {
            lo = juce::jmin (lo, (int) n[id::key]);
            hi = juce::jmax (hi, (int) n[id::key]);
        }

        const double length = (double) rack.ctx.project.patternLength (rack.ctx.project.currentPattern());
        const float range = (float) juce::jmax (12, hi - lo + 1);
        const auto inner = area.reduced (4.0f, 4.0f);

        g.setColour (colour.withAlpha (0.9f));
        for (auto& n : list)
        {
            const auto d = Project::readNote (n);
            const float x = inner.getX() + inner.getWidth() * (float) (d.start / length);
            const float w = juce::jmax (2.0f, inner.getWidth() * (float) (d.length / length));
            const float y = inner.getBottom() - inner.getHeight() * ((float) (d.key - lo + 1) / range);
            g.fillRect (x, y, w, juce::jmax (1.5f, inner.getHeight() / range));
        }

        if (playStep >= 0)
        {
            const float x = inner.getX() + inner.getWidth() * (float) ((double) playStep * ticksPerStep / length);
            g.setColour (colours::text.withAlpha (0.5f));
            g.fillRect (x, area.getY(), 1.0f, area.getHeight());
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        auto& project = rack.ctx.project;
        const auto p = e.position;

        if (ledArea().expanded (4).contains (p.toInt()))
        {
            project.beginTransaction ("Mute channel");
            if (e.mods.isPopupMenu() || e.mods.isCtrlDown())
                channel.setProperty (id::solo, ! (bool) channel[id::solo], project.um());
            else
                channel.setProperty (id::mute, ! (bool) channel[id::mute], project.um());
            return;
        }

        project.setSelectedChannel (uid());

        if (isSoundFont() && presetArea().contains (p.toInt()) && ! e.mods.isPopupMenu())
        {
            showPresetMenu();
            return;
        }

        if (nameArea().contains (p.toInt()))
        {
            if (e.mods.isPopupMenu())
                showMenu();
            else if (rack.ctx.openChannelEditor)
                rack.ctx.openChannelEditor (uid());
            return;
        }

        if (stepsArea().contains (p.toInt()))
        {
            if (! isStepView())
            {
                if (rack.ctx.openPianoRoll)
                    rack.ctx.openPianoRoll (uid());
                return;
            }

            const int step = stepAt (p);
            if (step < 0)
                return;

            project.beginTransaction ("Edit steps");
            paintMode = e.mods.isPopupMenu() ? 0 : (findStep (step).isValid() ? 0 : 1);
            setStep (step, paintMode == 1);
            lastPaintedStep = step;
        }
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (paintMode < 0)
            return;

        const int step = stepAt (e.position);
        if (step >= 0 && step != lastPaintedStep)
        {
            setStep (step, paintMode == 1);
            lastPaintedStep = step;
        }
    }

    void mouseUp (const juce::MouseEvent&) override
    {
        paintMode = -1;
        lastPaintedStep = -1;
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (nameArea().contains (e.position.toInt()))
            rename();
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        if (hoverPreset)
        {
            hoverPreset = false;
            repaint (nameArea());
        }
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        const bool overPreset = isSoundFont() && presetArea().contains (e.position.toInt());
        if (overPreset != hoverPreset)
        {
            hoverPreset = overPreset;
            setMouseCursor (overPreset ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
            repaint (nameArea());
        }

        if (rack.ctx.setHint == nullptr)
            return;

        if (overPreset)
        {
            rack.ctx.setHint ("Click to choose a SoundFont preset");
            return;
        }

        if (stepsArea().contains (e.position.toInt()))
            rack.ctx.setHint (isStepView() ? "Click or drag to toggle steps, right-drag to erase" : "Click to edit these notes in the Piano Roll");
        else if (nameArea().contains (e.position.toInt()))
            rack.ctx.setHint ("Click to open, right-click for options, double-click to rename");
        else if (ledArea().expanded (4).contains (e.position.toInt()))
            rack.ctx.setHint ("Click to mute, right-click to solo");
    }

    void rename()
    {
        askForText ("Rename channel", "Name", channel[id::name].toString(), [safe = juce::Component::SafePointer<ChannelRow> (this)] (const juce::String& text)
        {
            if (safe != nullptr && text.trim().isNotEmpty())
            {
                safe->rack.ctx.project.beginTransaction ("Rename channel");
                safe->channel.setProperty (id::name, text.trim(), safe->rack.ctx.project.um());
            }
        });
    }

    void showMenu()
    {
        auto& project = rack.ctx.project;
        juce::PopupMenu menu;
        const int u = uid();

        menu.addItem ("Piano roll", [this, u] { if (rack.ctx.openPianoRoll) rack.ctx.openPianoRoll (u); });
        menu.addItem (isSoundFont() ? "Presets and settings" : "Open editor", [this, u] { if (rack.ctx.openChannelEditor) rack.ctx.openChannelEditor (u); });

        juce::PopupMenu automate;
        automate.addItem ("Volume", [this] { if (rack.ctx.createAutomation) rack.ctx.createAutomation ({ automation::Kind::channelVolume, uid(), -1 }, channel[id::name].toString() + " volume"); });
        automate.addItem ("Pan", [this] { if (rack.ctx.createAutomation) rack.ctx.createAutomation ({ automation::Kind::channelPan, uid(), -1 }, channel[id::name].toString() + " pan"); });
        if (auto* plugin = rack.ctx.sync.pluginForChannel (u))
        {
            juce::PopupMenu params;
            const auto& list = plugin->getParameters();
            for (int i = 0; i < juce::jmin (list.size(), 512); ++i)
                if (list[i]->isAutomatable())
                    params.addItem (list[i]->getName (48), [this, plugin, i] { if (rack.ctx.automateParameter) rack.ctx.automateParameter (*plugin, i); });
            automate.addSubMenu ("Plugin parameter", params, params.getNumItems() > 0);
        }
        menu.addSubMenu ("Automate", automate);
        menu.addSeparator();
        menu.addItem ("Rename", [this] { rename(); });

        juce::PopupMenu colourMenu;
        for (int i = 0; i < 10; ++i)
        {
            const auto c = palette::channelColour (i);
            colourMenu.addColouredItem (100 + i, "Colour " + juce::String (i + 1), c, true, false, nullptr);
        }
        menu.addSubMenu ("Colour", colourMenu);

        juce::PopupMenu fill;
        for (int every : { 1, 2, 4, 8 })
            fill.addItem ("Every " + juce::String (every) + (every == 1 ? " step" : " steps"), [this, every]
            {
                rack.ctx.project.beginTransaction ("Fill steps");
                const int count = numSteps();
                for (int s = 0; s < count; ++s)
                    setStep (s, s % every == 0);
            });
        menu.addSubMenu ("Fill", fill);
        menu.addItem ("Clear notes", [this]
        {
            rack.ctx.project.beginTransaction ("Clear notes");
            for (auto& n : notes())
                rack.ctx.project.currentPattern().removeChild (n, rack.ctx.project.um());
        });
        menu.addSeparator();
        menu.addItem ("Route to free insert", [this]
        {
            const int free = rack.ctx.project.nextFreeInsert();
            if (free > 0)
            {
                rack.ctx.project.beginTransaction ("Route channel");
                channel.setProperty (id::insert, free, rack.ctx.project.um());
            }
        });
        menu.addItem ("Clone", [this, u] { rack.ctx.project.beginTransaction ("Clone channel"); rack.ctx.project.cloneChannel (u); });
        menu.addItem ("Move up", channel.getParent().indexOf (channel) > 0, false, [this]
        {
            auto parent = channel.getParent();
            const int idx = parent.indexOf (channel);
            rack.ctx.project.beginTransaction ("Move channel");
            parent.moveChild (idx, idx - 1, rack.ctx.project.um());
        });
        menu.addItem ("Move down", channel.getParent().indexOf (channel) < channel.getParent().getNumChildren() - 1, false, [this]
        {
            auto parent = channel.getParent();
            const int idx = parent.indexOf (channel);
            rack.ctx.project.beginTransaction ("Move channel");
            parent.moveChild (idx, idx + 1, rack.ctx.project.um());
        });
        menu.addSeparator();
        menu.addItem ("Delete channel", [&project, u]
        {
            project.beginTransaction ("Delete channel");
            project.removeChannel (u);
        });

        menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [this] (int result)
        {
            if (result >= 100 && result < 110)
            {
                rack.ctx.project.beginTransaction ("Channel colour");
                channel.setProperty (id::colour, palette::channelColour (result - 100).toString(), rack.ctx.project.um());
            }
        });
    }

    ChannelRack& rack;
    juce::ValueTree channel;
    Knob pan { -1.0, 1.0, 0.0, "Pan" };
    Knob volume { 0.0, 1.25, 0.78, "Volume" };
    NumberBox insertBox { 0.0, (double) numMixerInserts, 1.0, 0 };
    float activity = 0.0f;
    bool hoverPreset = false;
    int playStep = -1;
    int paintMode = -1;
    int lastPaintedStep = -1;
};

ChannelRack::ChannelRack (Context& c)
    : ctx (c)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&rowsHolder, false);
    viewport.setScrollBarsShown (true, false);
    viewport.setScrollBarThickness (10);

    addAndMakeVisible (patternButton);
    patternButton.setTooltip ("Current pattern");
    keepFocusOnEditors ({ &patternButton });
    patternButton.onClick = [this] { showPatternMenu(); };

    addAndMakeVisible (newPatternButton);
    newPatternButton.setFramed (true);
    newPatternButton.onClick = [this]
    {
        ctx.project.beginTransaction ("New pattern");
        auto p = ctx.project.addPattern();
        ctx.project.setCurrentPattern (p[id::uid]);
    };

    stepsBox.setLabel ("STEPS");
    stepsBox.setTooltip ("Pattern length in steps");
    stepsBox.onGestureStart = [this] { ctx.project.beginTransaction ("Pattern length"); };
    stepsBox.onChange = [this] (double v) { ctx.project.currentPattern().setProperty (id::steps, (int) v, ctx.project.um()); };
    addAndMakeVisible (stepsBox);

    swingKnob.bind (ctx.project.getState(), id::swing, ctx.project.um());
    swingKnob.describe = [] (double v) { return "Swing " + juce::String (v * 100.0, 0) + "%"; };
    addAndMakeVisible (swingKnob);

    addAndMakeVisible (addButton);
    addButton.setFramed (true);
    addButton.onClick = [this] { showAddMenu (&addButton); };

    addAndMakeVisible (drumButton);
    drumButton.setFramed (true);
    drumButton.onClick = [this] { showDrumGenerator(); };

    ctx.project.getState().addListener (this);
    rebuildRows();
    refreshHeader();
    startTimerHz (30);
}

ChannelRack::~ChannelRack()
{
    ctx.project.getState().removeListener (this);
}

void ChannelRack::refreshHeader()
{
    auto pattern = ctx.project.currentPattern();
    patternButton.setButtonText (pattern[id::name].toString());
    stepsBox.setValue (juce::jmax (1, (int) pattern.getProperty (id::steps, 16)), juce::dontSendNotification);
}

void ChannelRack::rebuildRows()
{
    rows.clear();

    for (auto ch : ctx.project.channelsTree())
    {
        auto row = std::make_unique<ChannelRow> (*this, ch);
        rowsHolder.addAndMakeVisible (*row);
        rows.push_back (std::move (row));
    }
    resized();
    repaint();
}

void ChannelRack::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::line);
    g.fillRect (0, headerHeight - 1, getWidth(), 1);

    if (rows.empty())
    {
        auto area = getLocalBounds().withTrimmedTop (headerHeight);
        g.setColour (colours::textDim);
        g.setFont (font (13.0f, Weight::medium));
        g.drawText ("Drop an instrument plugin, SoundFont or sample here", area.withTrimmedBottom (20), juce::Justification::centred, false);
        g.setColour (colours::textFaint);
        g.setFont (font (10.5f));
        g.drawText ("or press + to add a channel. Harmony ships no built-in sounds: bring your own VST, VST3, LV2, SF2 or samples.",
                    area.withTrimmedTop (30), juce::Justification::centred, false);
    }
}

void ChannelRack::resized()
{
    auto header = getLocalBounds().removeFromTop (headerHeight).reduced (8, 6);
    patternButton.setBounds (header.removeFromLeft (160));
    header.removeFromLeft (4);
    newPatternButton.setBounds (header.removeFromLeft (28));
    header.removeFromLeft (10);
    stepsBox.setBounds (header.removeFromLeft (96));
    header.removeFromLeft (8);
    swingKnob.setBounds (header.removeFromLeft (28));
    addButton.setBounds (header.removeFromRight (28));
    header.removeFromRight (4);
    drumButton.setBounds (header.removeFromRight (28));

    viewport.setBounds (getLocalBounds().withTrimmedTop (headerHeight));
    const int width = viewport.getWidth() - (viewport.isVerticalScrollBarShown() ? viewport.getScrollBarThickness() : 0);
    rowsHolder.setSize (juce::jmax (1, width), juce::jmax (1, (int) rows.size() * rowHeight));

    int y = 0;
    for (auto& row : rows)
    {
        row->setBounds (0, y, rowsHolder.getWidth(), rowHeight);
        y += rowHeight;
    }
}

void ChannelRack::timerCallback()
{
    int step = -1;
    if (ctx.engine.isPlaying())
    {
        const double tick = ctx.engine.getDisplayPositionTicks();
        if (! ctx.engine.isSongMode())
            step = (int) (tick / ticksPerStep);
        else if (auto local = ctx.project.patternPositionAt (ctx.project.currentPattern()[id::uid], tick))
            step = (int) (*local / ticksPerStep);
    }

    for (auto& row : rows)
    {
        float level = 0.0f;
        if (auto* node = ctx.engine.findChannelNode (row->uid()))
            level = juce::jmax (node->meter.takeLeft(), node->meter.takeRight());

        row->setActivity (level);
        row->setPlayStep (step);
        row->showAutomatedValues();
    }
}

void ChannelRack::showPatternMenu()
{
    auto& project = ctx.project;
    juce::PopupMenu menu;
    menu.addSectionHeader ("Patterns");

    const int current = project.currentPattern()[id::uid];
    for (auto p : project.patternsTree())
    {
        const int uid = p[id::uid];
        menu.addItem (p[id::name].toString(), true, uid == current, [&project, uid] { project.setCurrentPattern (uid); });
    }

    menu.addSeparator();
    menu.addItem ("New pattern", [&project] { project.beginTransaction ("New pattern"); project.setCurrentPattern (project.addPattern()[id::uid]); });
    menu.addItem ("Clone pattern", [&project, current] { project.beginTransaction ("Clone pattern"); project.setCurrentPattern (project.clonePattern (current)[id::uid]); });
    menu.addItem ("Rename pattern", [&project, current]
    {
        auto p = project.findPattern (current);
        askForText ("Rename pattern", "Name", p[id::name].toString(), [&project, current] (const juce::String& text)
        {
            if (text.trim().isNotEmpty())
            {
                project.beginTransaction ("Rename pattern");
                project.findPattern (current).setProperty (id::name, text.trim(), project.um());
            }
        });
    });
    menu.addItem ("Delete pattern", project.patternsTree().getNumChildren() > 1, false, [&project, current]
    {
        project.beginTransaction ("Delete pattern");
        project.removePattern (current);
    });

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (patternButton));
}

void ChannelRack::showAddMenu (juce::Component* target)
{
    juce::PopupMenu menu;
    juce::PopupMenu instruments;
    auto list = ctx.plugins.getPlugins (true);

    std::map<juce::String, juce::PopupMenu> byVendor;
    for (auto& d : list)
    {
        const auto identifier = d.createIdentifierString();
        byVendor[d.manufacturerName.isNotEmpty() ? d.manufacturerName : juce::String ("Other")]
            .addItem (d.name + "  (" + d.pluginFormatName + ")", [this, identifier] { if (ctx.addInstrument) ctx.addInstrument (identifier); });
    }

    for (auto& [vendor, sub] : byVendor)
        instruments.addSubMenu (vendor, sub);

    if (list.isEmpty())
        instruments.addItem ("No instruments found. Scan for plugins...", [this] { ctx.commands.invokeDirectly (cmd::scanPlugins, true); });

    menu.addSectionHeader ("Add channel");
    menu.addSubMenu ("Instrument plugin", instruments);
    menu.addItem ("SoundFont (.sf2)...", [this] { ctx.commands.invokeDirectly (cmd::addSoundFont, true); });
    menu.addItem ("Sampler (audio file)...", [this] { ctx.commands.invokeDirectly (cmd::addSampler, true); });

    auto options = juce::PopupMenu::Options();
    menu.showMenuAsync (target != nullptr ? options.withTargetComponent (target) : options.withMousePosition());
}

void ChannelRack::showDrumGenerator()
{
    if (ctx.showPanel)
        ctx.showPanel (createDrumGeneratorPanel (ctx), "Drum Generator");
}

bool ChannelRack::isInterestedInDragSource (const SourceDetails& details)
{
    const auto d = details.description.toString();
    return d.startsWith ("file:") || d.startsWith ("plugin:");
}

void ChannelRack::itemDropped (const SourceDetails& details)
{
    const auto d = details.description.toString();
    if (d.startsWith ("plugin:") && ctx.addInstrument)
        ctx.addInstrument (d.fromFirstOccurrenceOf ("plugin:", false, false));
    else if (d.startsWith ("file:") && ctx.addFileAsChannel)
        ctx.addFileAsChannel (juce::File (d.fromFirstOccurrenceOf ("file:", false, false)));
}

bool ChannelRack::isInterestedInFileDrag (const juce::StringArray&)
{
    return true;
}

void ChannelRack::filesDropped (const juce::StringArray& files, int, int)
{
    for (auto& f : files)
        if (ctx.addFileAsChannel)
            ctx.addFileAsChannel (juce::File (f));
}

void ChannelRack::valueTreePropertyChanged (juce::ValueTree& t, const juce::Identifier& p)
{
    if (t.hasType (id::PROJECT) && (p == id::currentPattern || p == id::selectedChannel))
    {
        refreshHeader();
        for (auto& r : rows)
            r->repaint();
        return;
    }

    if (t.hasType (id::PATTERN))
        refreshHeader();

    if (t.hasType (id::CHANNEL))
    {
        for (auto& r : rows)
            if (r->uid() == (int) t[id::uid])
                r->refresh();
        return;
    }

    if (t.hasType (id::NOTE) || t.hasType (id::PATTERN))
        for (auto& r : rows)
            r->repaint();
}

void ChannelRack::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (parent.hasType (id::CHANNELS) || child.hasType (id::CHANNELS))
        rebuildRows();
    else if (parent.hasType (id::PATTERN))
        for (auto& r : rows)
            r->repaint();
}

void ChannelRack::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int)
{
    valueTreeChildAdded (parent, child);
}

void ChannelRack::valueTreeChildOrderChanged (juce::ValueTree& parent, int, int)
{
    if (parent.hasType (id::CHANNELS))
        rebuildRows();
}
}
