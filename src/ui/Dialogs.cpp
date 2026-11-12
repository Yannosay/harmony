#include "ui/Dialogs.h"
#include "app/History.h"
#include "model/ChordGenerator.h"
#include "model/DrumGenerator.h"
#include "model/FollowGenerator.h"
#include "model/MusicTheory.h"
#include "engine/AudioEngine.h"
#include "update/Version.h"
#include "app/Preferences.h"
#include "ui/SmoothView.h"
#include "engine/PluginManager.h"
#include <set>

namespace harmony::ui
{
void askForText (const juce::String& title, const juce::String& label, const juce::String& initial,
                 std::function<void (const juce::String&)> onOk)
{
    auto* window = new juce::AlertWindow (title, {}, juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("text", initial, label);
    window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    if (auto* editor = window->getTextEditor ("text"))
    {
        editor->setFont (font (12.0f));
        editor->selectAll();
    }

    window->enterModalState (true, juce::ModalCallbackFunction::create ([window, onOk] (int result)
    {
        if (result == 1 && onOk)
            onOk (window->getTextEditorContents ("text"));
    }), true);
}

namespace
{
    class Panel : public juce::Component
    {
    protected:
        struct Row
        {
            juce::String label;
            juce::Component* component;
            int height;
        };

        void addRow (const juce::String& label, juce::Component& c, int height = 30)
        {
            rows.push_back ({ label, &c, height });
            addAndMakeVisible (c);
        }

        void layoutRows (juce::Rectangle<int> area, int labelWidth = 150)
        {
            for (auto& r : rows)
            {
                auto row = area.removeFromTop (r.height);
                row.removeFromLeft (labelWidth);
                r.component->setBounds (row);
                area.removeFromTop (8);
            }
        }

        void paintRowLabels (juce::Graphics& g, int labelWidth = 150)
        {
            g.setColour (colours::textDim);
            g.setFont (font (11.0f));
            for (auto& r : rows)
                if (r.label.isNotEmpty())
                    g.drawText (r.label, r.component->getBounds().withX (r.component->getX() - labelWidth).withWidth (labelWidth - 12),
                                juce::Justification::centredLeft, false);
        }

        static void fill (juce::ComboBox& box, const juce::StringArray& items, int selectedIndex)
        {
            box.addItemList (items, 1);
            box.setSelectedItemIndex (juce::jlimit (0, items.size() - 1, selectedIndex), juce::dontSendNotification);
        }

        std::vector<Row> rows;
    };

    class FollowPanel final : public Panel
    {
    public:
        FollowPanel (Context& c, int channel, std::function<void (const std::vector<NoteData>&, bool)> a)
            : ctx (c), channelUid (channel), apply (std::move (a))
        {
            setSize (560, 510);

            sourceBox.addItem ("All other channels", 1);
            sourceIds.add (-1);
            std::map<int, int> notesPerChannel;
            for (auto n : ctx.project.currentPattern())
                ++notesPerChannel[(int) n[id::channel]];

            for (auto ch : ctx.project.channelsTree())
            {
                const int uid = ch[id::uid];
                if (notesPerChannel[uid] == 0)
                    continue;
                sourceBox.addItem (ch[id::name].toString() + (uid == channelUid ? "  (this channel)" : juce::String()), sourceBox.getNumItems() + 1);
                sourceIds.add (uid);
            }

            sourceBox.onChange = [this] { analyse(); };
            sourceBox.setSelectedItemIndex (0, juce::dontSendNotification);
            analyse();

            if (! hasHarmony() && sourceIds.contains (channelUid))
            {
                sourceBox.setSelectedItemIndex (sourceIds.indexOf (channelUid), juce::dontSendNotification);
                analyse();
            }

            fill (styleBox, follow::styleNames(), 1);
            fill (octaveBox, { "1", "2", "3", "4", "5", "6" }, 1);
            styleBox.onChange = [this]
            {
                const int style = styleBox.getSelectedItemIndex();
                octaveBox.setSelectedItemIndex (style <= 1 ? 1 : (style == 2 ? 3 : 4), juce::dontSendNotification);
                repaint();
            };

            for (auto* s : { &density, &velocity })
            {
                s->setSliderStyle (juce::Slider::LinearHorizontal);
                s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
                s->setRange (0.0, 1.0, 0.01);
            }
            density.setValue (0.5, juce::dontSendNotification);
            velocity.setValue (0.8, juce::dontSendNotification);
            replace.setButtonText ("Replace the channel's notes in this pattern");
            replace.setToggleState (true, juce::dontSendNotification);

            addRow ("Chords from", sourceBox);
            addRow ("Write a", styleBox);
            addRow ("Octave", octaveBox);
            addRow ("Movement", density);
            addRow ("Velocity", velocity);
            addRow ({}, replace, 24);

            ideaButton.onClick = [this]
            {
                seed = (juce::uint32) juce::Random::getSystemRandom().nextInt (1 << 30) + 1;
                write();
            };
            writeButton.setColour (juce::TextButton::buttonColourId, colours::accent);
            writeButton.setColour (juce::TextButton::textColourOffId, colours::background);
            writeButton.onClick = [this] { write(); };
            writeButton.setEnabled (! chords.empty());
            ideaButton.setEnabled (! chords.empty());
            addAndMakeVisible (ideaButton);
            analyse();
            addAndMakeVisible (writeButton);
        }

        bool hasHarmony() const
        {
            for (auto& c : chords)
                if (c.keys.size() >= 2)
                    return true;
            return false;
        }

        void analyse()
        {
            const int source = sourceIds[juce::jmax (0, sourceBox.getSelectedItemIndex())];
            std::vector<NoteData> notes;
            auto pattern = ctx.project.currentPattern();
            for (auto n : pattern)
            {
                const int uid = n[id::channel];
                if ((source < 0 && uid != channelUid) || uid == source)
                    notes.push_back (Project::readNote (n));
            }

            chords = follow::analyse (notes, ctx.project.patternLength (pattern), ctx.project.ticksPerBeat());
            replace.setToggleState (source != channelUid, juce::dontSendNotification);
            replace.setEnabled (source != channelUid);
            writeButton.setEnabled (! chords.empty());
            ideaButton.setEnabled (! chords.empty());
            repaint();
        }

        void write()
        {
            follow::Settings s;
            s.style = (follow::Style) styleBox.getSelectedItemIndex();
            s.octave = octaveBox.getText().getIntValue();
            s.density = (float) density.getValue();
            s.velocity = (float) velocity.getValue();
            s.scaleRoot = ctx.project.getState()[id::scaleRoot];
            s.scale = ctx.project.getState()[id::scaleType].toString();
            s.seed = seed;
            s.channel = channelUid;

            const auto notes = follow::generate (chords, s, ctx.project.ticksPerBeat());
            apply (notes, replace.getToggleState());
            if (ctx.notify)
                ctx.notify ("Wrote " + juce::String ((int) notes.size()) + " notes following " + juce::String ((int) chords.size()) + " chords. Press New idea for another take.", false);
        }

        void paint (juce::Graphics& g) override
        {
            paintRowLabels (g);

            auto strip = previewArea.toFloat();
            g.setColour (colours::background);
            g.fillRoundedRectangle (strip, 8.0f);

            if (chords.empty())
            {
                g.setColour (colours::textDim);
                g.setFont (font (11.0f));
                g.drawFittedText ("No chords found in this pattern yet. Put chords on another channel first, for example with the chord generator (Ctrl+G).",
                                  previewArea.reduced (16, 6), juce::Justification::centred, 2);
                return;
            }

            const auto total = (float) juce::jmax ((juce::int64) 1, ctx.project.patternLength (ctx.project.currentPattern()));
            for (auto& c : chords)
            {
                auto cell = juce::Rectangle<float> (strip.getX() + strip.getWidth() * (float) c.start / total, strip.getY(),
                                                    strip.getWidth() * (float) (c.end - c.start) / total, strip.getHeight()).reduced (3.0f);
                g.setColour (palette::notePink.withAlpha (0.14f));
                g.fillRoundedRectangle (cell, 6.0f);
                g.setColour (colours::accent);
                g.setFont (font (juce::jmin (13.0f, cell.getWidth() * 0.4f), Weight::semibold));
                g.drawText (c.name, cell, juce::Justification::centred, false);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (22, 16);
            previewArea = r.removeFromTop (64);
            r.removeFromTop (16);
            auto buttons = r.removeFromBottom (36);
            writeButton.setBounds (buttons.removeFromRight (150));
            buttons.removeFromRight (8);
            ideaButton.setBounds (buttons.removeFromRight (130));
            layoutRows (r);
        }

    private:
        Context& ctx;
        int channelUid;
        std::function<void (const std::vector<NoteData>&, bool)> apply;
        std::vector<follow::ChordSegment> chords;
        juce::ComboBox sourceBox, styleBox, octaveBox;
        juce::Array<int> sourceIds;
        juce::Slider density, velocity;
        juce::ToggleButton replace;
        juce::TextButton ideaButton { "New idea" }, writeButton { "Write notes" };
        juce::Rectangle<int> previewArea;
        juce::uint32 seed = 7;
    };

    class ChordGeneratorPanel final : public Panel
    {
    public:
        ChordGeneratorPanel (Context& c, std::function<void (const std::vector<NoteData>&, bool)> a)
            : ctx (c), apply (std::move (a))
        {
            setSize (560, 560);
            const auto& state = ctx.project.getState();

            juce::StringArray roots;
            for (int i = 0; i < 12; ++i)
                roots.add (theory::pitchClassName (i));
            fill (rootBox, roots, state[id::scaleRoot]);

            juce::StringArray scales;
            int scaleIndex = 1;
            for (auto& s : theory::allScales())
            {
                if (s.isHeptatonic())
                {
                    if (s.name == theory::findScale (state[id::scaleType].toString()).name)
                        scaleIndex = scales.size();
                    scales.add (s.name);
                }
            }
            fill (scaleBox, scales, scaleIndex);

            juce::StringArray progressions;
            for (auto& p : chordProgressions())
                progressions.add (p.name);
            fill (progressionBox, progressions, 1);
            fill (sizeBox, { "Triads", "Seventh chords", "Ninth chords" }, 0);
            fill (octaveBox, { "2", "3", "4", "5", "6" }, 2);
            fill (barsBox, { "1 bar", "2 bars", "4 bars" }, 0);
            fill (rhythmBox, chordRhythmNames(), 0);

            velocity.setRange (0.1, 1.0, 0.01);
            velocity.setValue (0.8, juce::dontSendNotification);
            velocity.setSliderStyle (juce::Slider::LinearHorizontal);
            velocity.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

            voiceLeading.setButtonText ("Smooth voice leading");
            voiceLeading.setToggleState (true, juce::dontSendNotification);
            bass.setButtonText ("Add bass notes");
            bass.setToggleState (true, juce::dontSendNotification);
            replace.setButtonText ("Replace the channel's notes in this pattern");
            replace.setToggleState (true, juce::dontSendNotification);

            addRow ("Key", rootBox);
            addRow ("Scale", scaleBox);
            addRow ("Progression", progressionBox);
            addRow ("Chords", sizeBox);
            addRow ("Octave", octaveBox);
            addRow ("Length", barsBox);
            addRow ("Rhythm", rhythmBox);
            addRow ("Velocity", velocity);
            addRow ({}, voiceLeading, 24);
            addRow ({}, bass, 24);
            addRow ({}, replace, 24);

            for (auto* box : { &rootBox, &scaleBox, &progressionBox, &sizeBox, &octaveBox, &barsBox, &rhythmBox })
                box->onChange = [this] { updatePreview(); };
            for (auto* t : { &voiceLeading, &bass })
                t->onClick = [this] { updatePreview(); };

            randomButton.onClick = [this]
            {
                seed = juce::Random::getSystemRandom().nextInt (1 << 30) + 1;
                progressionBox.setSelectedItemIndex (0, juce::dontSendNotification);
                updatePreview();
            };
            generateButton.setColour (juce::TextButton::buttonColourId, colours::accent);
            generateButton.setColour (juce::TextButton::textColourOffId, colours::background);
            generateButton.onClick = [this]
            {
                const auto result = generate();
                apply (result.notes, replace.getToggleState());
                if (ctx.notify)
                    ctx.notify ("Generated " + result.chordNames.joinIntoString ("  "), false);
                if (ctx.closePanel)
                    ctx.closePanel();
            };
            addAndMakeVisible (randomButton);
            addAndMakeVisible (generateButton);
            updatePreview();
        }

        ChordGenResult generate() const
        {
            ChordGenSettings s;
            s.root = rootBox.getSelectedItemIndex();
            s.scale = scaleBox.getText();
            s.progression = progressionBox.getSelectedItemIndex();
            s.chordSize = 3 + sizeBox.getSelectedItemIndex();
            s.octave = octaveBox.getText().getIntValue();
            s.barsPerChord = 1 << barsBox.getSelectedItemIndex();
            s.rhythm = (ChordRhythm) rhythmBox.getSelectedItemIndex();
            s.voiceLeading = voiceLeading.getToggleState();
            s.addBass = bass.getToggleState();
            s.velocity = (float) velocity.getValue();
            s.seed = (juce::uint32) seed;
            return generateChords (s, ctx.project.ticksPerBar(), ctx.project.ticksPerBeat());
        }

        void updatePreview()
        {
            const auto result = generate();
            chords = result.chordNames;
            numerals = result.numerals;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            paintRowLabels (g);

            auto strip = previewArea.toFloat();
            g.setColour (colours::background);
            g.fillRoundedRectangle (strip, 8.0f);

            const int n = juce::jmax (1, chords.size());
            const float w = strip.getWidth() / (float) n;
            for (int i = 0; i < chords.size(); ++i)
            {
                auto cell = juce::Rectangle<float> (strip.getX() + w * (float) i, strip.getY(), w, strip.getHeight()).reduced (4.0f);
                g.setColour (palette::notePink.withAlpha (0.12f + 0.04f * (float) (i % 2)));
                g.fillRoundedRectangle (cell, 6.0f);
                g.setColour (colours::accent);
                g.setFont (font (13.0f, Weight::semibold));
                g.drawText (chords[i], cell.withTrimmedBottom (cell.getHeight() * 0.45f), juce::Justification::centredBottom, false);
                g.setColour (colours::textDim);
                g.setFont (font (10.0f));
                g.drawText (numerals[i], cell.withTrimmedTop (cell.getHeight() * 0.58f), juce::Justification::centredTop, false);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (22, 16);
            previewArea = r.removeFromTop (64);
            r.removeFromTop (16);
            auto buttons = r.removeFromBottom (36);
            generateButton.setBounds (buttons.removeFromRight (150));
            buttons.removeFromRight (8);
            randomButton.setBounds (buttons.removeFromRight (130));
            layoutRows (r);
        }

    private:
        Context& ctx;
        std::function<void (const std::vector<NoteData>&, bool)> apply;
        juce::ComboBox rootBox, scaleBox, progressionBox, sizeBox, octaveBox, barsBox, rhythmBox;
        juce::Slider velocity;
        juce::ToggleButton voiceLeading, bass, replace;
        juce::TextButton randomButton { "Surprise me" }, generateButton { "Generate" };
        juce::StringArray chords, numerals;
        juce::Rectangle<int> previewArea;
        int seed = 7;
    };

    class DrumGeneratorPanel final : public Panel
    {
    public:
        explicit DrumGeneratorPanel (Context& c) : ctx (c)
        {
            setSize (600, 640);
            fill (styleBox, drumStyleNames(), 0);
            fill (barsBox, { "1 bar", "2 bars", "4 bars" }, 1);

            for (auto* s : { &density, &humanize, &variation })
            {
                s->setRange (0.0, 1.0, 0.01);
                s->setSliderStyle (juce::Slider::LinearHorizontal);
                s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            }
            density.setValue (0.5, juce::dontSendNotification);
            humanize.setValue (0.2, juce::dontSendNotification);
            variation.setValue (0.4, juce::dontSendNotification);

            addRow ("Style", styleBox);
            addRow ("Length", barsBox);
            addRow ("Density", density);
            addRow ("Humanize", humanize);
            addRow ("Variation", variation);

            juce::StringArray channelNames { "None" };
            for (auto ch : ctx.project.channelsTree())
            {
                channelNames.add (ch[id::name].toString());
                channelIds.add (ch[id::uid]);
            }

            for (int r = 0; r < (int) DrumRole::count; ++r)
            {
                auto* box = roleBoxes.add (new juce::ComboBox());
                box->addItemList (channelNames, 1);
                box->setSelectedItemIndex (0, juce::dontSendNotification);

                for (int i = 0; i < channelIds.size(); ++i)
                {
                    bool matched = false;
                    if (guessDrumRole (channelNames[i + 1], matched) == (DrumRole) r && matched)
                    {
                        box->setSelectedItemIndex (i + 1, juce::dontSendNotification);
                        break;
                    }
                }
                addRow (drumRoleName ((DrumRole) r), *box, 28);
            }

            randomButton.onClick = [this] { seed = juce::Random::getSystemRandom().nextInt (1 << 30) + 1; generate(); };
            generateButton.setColour (juce::TextButton::buttonColourId, colours::accent);
            generateButton.setColour (juce::TextButton::textColourOffId, colours::background);
            generateButton.onClick = [this] { generate(); };
            addAndMakeVisible (randomButton);
            addAndMakeVisible (generateButton);
        }

        void generate()
        {
            DrumGenSettings s;
            s.style = styleBox.getText();
            s.bars = 1 << barsBox.getSelectedItemIndex();
            s.density = (float) density.getValue();
            s.humanize = (float) humanize.getValue();
            s.variation = (float) variation.getValue();
            s.seed = (juce::uint32) seed;

            std::array<int, (size_t) DrumRole::count> mapping {};
            bool any = false;
            for (int r = 0; r < (int) DrumRole::count; ++r)
            {
                const int idx = roleBoxes[r]->getSelectedItemIndex();
                mapping[(size_t) r] = idx > 0 ? channelIds[idx - 1] : 0;
                any = any || idx > 0;
            }

            if (! any)
            {
                if (ctx.notify)
                    ctx.notify ("Assign at least one channel to a drum role", true);
                return;
            }

            auto& project = ctx.project;
            auto pattern = project.currentPattern();
            project.beginTransaction ("Generate drums");

            for (int i = pattern.getNumChildren(); --i >= 0;)
            {
                const int ch = pattern.getChild (i)[id::channel];
                if (std::find (mapping.begin(), mapping.end(), ch) != mapping.end() && ch != 0)
                    pattern.removeChild (i, project.um());
            }

            int count = 0;
            for (auto& hit : generateDrums (s, ticksPerStep))
            {
                const int ch = mapping[(size_t) hit.role];
                if (ch == 0)
                    continue;

                NoteData n;
                n.channel = ch;
                n.key = 60;
                n.start = hit.start;
                n.length = hit.length;
                n.velocity = hit.velocity;
                project.addNote (pattern, n);
                ++count;
            }

            pattern.setProperty (id::steps, s.bars * 16, project.um());
            if (ctx.notify)
                ctx.notify (s.style + " groove: " + juce::String (count) + " hits over " + juce::String (s.bars) + (s.bars == 1 ? " bar" : " bars"), false);
        }

        void paint (juce::Graphics& g) override
        {
            paintRowLabels (g);
            if (channelIds.isEmpty())
            {
                g.setColour (colours::warning);
                g.setFont (font (11.0f));
                g.drawText ("Add drum channels (samples or a drum plugin) first, then map them to roles.", getLocalBounds().reduced (22).removeFromBottom (70),
                            juce::Justification::centredTop, true);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (22, 16);
            auto buttons = r.removeFromBottom (36);
            generateButton.setBounds (buttons.removeFromRight (150));
            buttons.removeFromRight (8);
            randomButton.setBounds (buttons.removeFromRight (130));
            layoutRows (r);
        }

    private:
        Context& ctx;
        juce::ComboBox styleBox, barsBox;
        juce::Slider density, humanize, variation;
        juce::OwnedArray<juce::ComboBox> roleBoxes;
        juce::Array<int> channelIds;
        juce::TextButton randomButton { "New variation" }, generateButton { "Generate" };
        int seed = 11;
    };

    class RenderPanel final : public Panel
    {
    public:
        RenderPanel (Context& c, std::function<void (engine::RenderSettings, bool)> s)
            : ctx (c), start (std::move (s))
        {
            setSize (560, 520);
            fill (formatBox, engine::Renderer::formatNames(), 0);
            formatBox.onChange = [this] { updateBits(); updatePath(); };
            fill (rateBox, { "44100 Hz", "48000 Hz", "88200 Hz", "96000 Hz" }, 1);
            fill (rangeBox, { "Song", "Current pattern", "Loop region" }, ctx.project.songLength() > 0 ? 0 : 1);
            tail.setRange (0.0, 10.0, 0.5);
            tail.setValue (2.0, juce::dontSendNotification);
            tail.setSliderStyle (juce::Slider::LinearHorizontal);
            tail.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 22);
            tail.setTextValueSuffix (" s");

            normalize.setButtonText ("Normalize to -0.3 dBFS");
            stems.setButtonText ("Also export stems (one file per used mixer insert)");
            minimise.setButtonText ("Render in background and minimise Harmony");

            addRow ("Format", formatBox);
            addRow ("Bit depth", bitsBox);
            addRow ("Sample rate", rateBox);
            addRow ("Range", rangeBox);
            addRow ("Tail", tail);
            addRow ("Output", pathButton);
            addRow ({}, normalize, 24);
            addRow ({}, stems, 24);
            addRow ({}, minimise, 24);

            pathButton.onClick = [this] { choosePath(); };
            renderButton.setColour (juce::TextButton::buttonColourId, colours::accent);
            renderButton.setColour (juce::TextButton::textColourOffId, colours::background);
            renderButton.onClick = [this] { begin(); };
            addAndMakeVisible (renderButton);

            const auto defaultDir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Harmony Renders");
            output = defaultDir.getChildFile (juce::File::createLegalFileName (ctx.project.getDisplayName()) + ".wav");
            updateBits();
            updatePath();
        }

        void updateBits()
        {
            const auto depths = engine::Renderer::bitDepthsFor (formatBox.getText());
            bitsBox.clear (juce::dontSendNotification);
            for (auto d : depths)
                bitsBox.addItem (juce::String (d) + (d == 32 ? "-bit float" : "-bit"), d);
            bitsBox.setSelectedId (depths.contains (24) ? 24 : depths.getFirst(), juce::dontSendNotification);
        }

        void updatePath()
        {
            output = output.withFileExtension (engine::Renderer::extensionFor (formatBox.getText()));
            pathButton.setButtonText (output.getFullPathName());
        }

        void choosePath()
        {
            chooser = std::make_unique<juce::FileChooser> ("Render to", output, "*" + engine::Renderer::extensionFor (formatBox.getText()));
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult() != juce::File())
                {
                    output = fc.getResult();
                    updatePath();
                }
            });
        }

        void begin()
        {
            engine::RenderSettings s;
            s.output = output;
            s.format = formatBox.getText();
            s.bitDepth = bitsBox.getSelectedId();
            s.sampleRate = rateBox.getText().getDoubleValue();
            s.bpm = ctx.project.getBpm();
            s.tailSeconds = tail.getValue();
            s.normalize = normalize.getToggleState();

            const auto& state = ctx.project.getState();
            switch (rangeBox.getSelectedItemIndex())
            {
                case 1:
                    s.songMode = false;
                    s.startTick = 0;
                    s.endTick = ctx.project.patternLength (ctx.project.currentPattern());
                    break;
                case 2:
                    s.songMode = true;
                    s.startTick = state[id::loopStart];
                    s.endTick = state[id::loopEnd];
                    break;
                default:
                    s.songMode = true;
                    s.startTick = 0;
                    s.endTick = ctx.project.songLength();
                    break;
            }

            if (s.endTick <= s.startTick)
            {
                if (ctx.notify)
                    ctx.notify (rangeBox.getSelectedItemIndex() == 2 ? "Set a loop region on the playlist ruler first" : "There is nothing to render in this range", true);
                return;
            }

            if (stems.getToggleState())
            {
                std::set<int> used;
                for (auto ch : ctx.project.channelsTree())
                    used.insert ((int) ch[id::insert]);
                for (int index : used)
                    if (index > 0)
                        s.stemInserts.push_back ({ index, ctx.project.findInsert (index)[id::name].toString() });
            }

            start (s, minimise.getToggleState());
        }

        void paint (juce::Graphics& g) override { paintRowLabels (g); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (22, 16);
            renderButton.setBounds (r.removeFromBottom (38).removeFromRight (160));
            layoutRows (r, 130);
        }

    private:
        Context& ctx;
        std::function<void (engine::RenderSettings, bool)> start;
        juce::ComboBox formatBox, bitsBox, rateBox, rangeBox;
        juce::Slider tail;
        juce::ToggleButton normalize, stems, minimise;
        juce::TextButton pathButton, renderButton { "Render" };
        juce::File output;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    class SettingsPage : public Panel
    {
    public:
        explicit SettingsPage (Context& c) : ctx (c) {}

        void paint (juce::Graphics& g) override
        {
            paintRowLabels (g, labelWidth);
            for (auto& note : notes)
            {
                g.setColour (colours::textFaint);
                g.setFont (font (9.5f));
                g.drawText (note.text, note.anchor->getBounds().translated (0, note.anchor->getHeight()).withHeight (16),
                            juce::Justification::centredLeft, true);
            }
        }

        void resized() override
        {
            layoutRows (getLocalBounds().reduced (24, 18), labelWidth);
        }

    protected:
        struct Note
        {
            juce::Component* anchor;
            juce::String text;
        };

        void addToggle (juce::ToggleButton& button, const prefs::BoolSetting& setting, const juce::String& text)
        {
            button.setButtonText (text);
            button.setToggleState (setting.get (ctx.settings), juce::dontSendNotification);
            button.onClick = [this, &button, setting] { setting.set (ctx.settings, button.getToggleState()); };
            addRow ({}, button, 26);
        }

        void addNote (juce::Component& anchor, const juce::String& text, int extraHeight = 14)
        {
            notes.push_back ({ &anchor, text });
            rows.back().height += extraHeight;
        }

        Context& ctx;
        std::vector<Note> notes;
        int labelWidth = 170;
    };

    class GeneralSettings final : public SettingsPage
    {
    public:
        explicit GeneralSettings (Context& c) : SettingsPage (c)
        {
            juce::StringArray scales;
            int scaleIndex = 2;
            for (int i = 0; i < (int) std::size (prefs::uiScaleChoices); ++i)
            {
                scales.add (juce::String (prefs::uiScaleChoices[i]) + "%");
                if (prefs::uiScaleChoices[i] == prefs::uiScalePercent.get (ctx.settings))
                    scaleIndex = i;
            }
            fill (scaleBox, scales, scaleIndex);
            scaleBox.onChange = [this] { prefs::uiScalePercent.set (ctx.settings, prefs::uiScaleChoices[scaleBox.getSelectedItemIndex()]); };
            addRow ("Interface size", scaleBox);

            tempoBox.setLabel ("BPM");
            tempoBox.setValue (prefs::defaultTempo.get (ctx.settings), juce::dontSendNotification);
            tempoBox.onChange = [this] (double v) { prefs::defaultTempo.set (ctx.settings, v); };
            addRow ("New project tempo", tempoBox);

            tracksBox.setLabel ("TRACKS");
            tracksBox.setValue (prefs::defaultTrackCount.get (ctx.settings), juce::dontSendNotification);
            tracksBox.onChange = [this] (double v) { prefs::defaultTrackCount.set (ctx.settings, (int) v); };
            addRow ("New project tracks", tracksBox);

            folderButton.setButtonText (prefs::projectFolder (ctx.settings).getFullPathName());
            folderButton.onClick = [this]
            {
                chooser = std::make_unique<juce::FileChooser> ("Default project folder", prefs::projectFolder (ctx.settings));
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
                {
                    if (fc.getResult().isDirectory())
                    {
                        ctx.settings.setValue ("projectFolder", fc.getResult().getFullPathName());
                        folderButton.setButtonText (fc.getResult().getFullPathName());
                    }
                });
            };
            addRow ("Project folder", folderButton);

            const auto sf = prefs::soundFontFolder (ctx.settings);
            soundFontButton.setButtonText (sf.isDirectory() ? sf.getFullPathName() : juce::String ("Choose a folder..."));
            soundFontButton.onClick = [this]
            {
                const auto current = prefs::soundFontFolder (ctx.settings);
                chooser = std::make_unique<juce::FileChooser> ("SoundFont folder", current.isDirectory() ? current : juce::File::getSpecialLocation (juce::File::userMusicDirectory));
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this] (const juce::FileChooser& fc)
                {
                    if (fc.getResult().isDirectory())
                    {
                        ctx.settings.setValue ("soundFontFolder", fc.getResult().getFullPathName());
                        soundFontButton.setButtonText (fc.getResult().getFullPathName());
                    }
                });
            };
            addRow ("SoundFont folder", soundFontButton);
            addNote (soundFontButton, "Shown in the browser and opened first when you add a SoundFont.");

            fill (autosaveBox, { "Off", "Every minute", "Every 3 minutes", "Every 5 minutes", "Every 10 minutes" }, prefs::autosaveChoice.get (ctx.settings));
            autosaveBox.onChange = [this] { prefs::autosaveChoice.set (ctx.settings, autosaveBox.getSelectedItemIndex()); };
            addRow ("Time machine", autosaveBox);
            addNote (autosaveBox, "Automatic snapshots you can restore from File > Time machine.");

            addToggle (reopen, prefs::openLastProject, "Open the last project when Harmony starts");
            addToggle (autoUpdate, prefs::autoUpdate, "Download updates automatically and install them on restart");

            checkButton.onClick = [this] { ctx.commands.invokeDirectly (cmd::checkUpdates, true); };
            dataButton.onClick = [this] { ctx.settings.getFile().getParentDirectory().revealToUser(); };
            addRow ({}, checkButton, 30);
            addRow ({}, dataButton, 30);
        }

        void resized() override
        {
            SettingsPage::resized();
            for (auto* b : { &checkButton, &dataButton })
                b->setSize (230, b->getHeight());
            for (auto* box : { (juce::Component*) &tempoBox, (juce::Component*) &tracksBox, (juce::Component*) &scaleBox, (juce::Component*) &autosaveBox })
                box->setSize (200, box->getHeight());
        }

    private:
        juce::ComboBox scaleBox, autosaveBox;
        NumberBox tempoBox { prefs::defaultTempo.minimum, prefs::defaultTempo.maximum, 0.01, 2 };
        NumberBox tracksBox { (double) prefs::defaultTrackCount.minimum, (double) prefs::defaultTrackCount.maximum, 1.0, 0 };
        juce::TextButton folderButton, soundFontButton;
        juce::ToggleButton reopen, autoUpdate;
        juce::TextButton checkButton { "Check for updates now" }, dataButton { "Open user data folder" };
        std::unique_ptr<juce::FileChooser> chooser;
    };

    class EditingSettings final : public SettingsPage
    {
    public:
        explicit EditingSettings (Context& c) : SettingsPage (c)
        {
            juce::StringArray snaps;
            for (auto& s : snapOptions())
                snaps.add (s.name);

            fill (pianoSnapBox, snaps, prefs::pianoRollSnap.get (ctx.settings));
            pianoSnapBox.onChange = [this] { prefs::pianoRollSnap.set (ctx.settings, pianoSnapBox.getSelectedItemIndex()); };
            addRow ("Piano roll snap", pianoSnapBox);

            fill (playlistSnapBox, snaps, prefs::playlistSnap.get (ctx.settings));
            playlistSnapBox.onChange = [this] { prefs::playlistSnap.set (ctx.settings, playlistSnapBox.getSelectedItemIndex()); };
            addRow ("Playlist snap", playlistSnapBox);

            addToggle (preview, prefs::previewNotes, "Play notes while drawing, moving and transposing them");
            addToggle (previewPlaying, prefs::previewWhilePlaying, "Keep playing note previews while the transport runs");
            addToggle (ghosts, prefs::ghostNotes, "Show other channels as ghost notes in the piano roll");
            addToggle (names, prefs::noteNames, "Show note names on piano roll notes");
            addToggle (snapHead, prefs::snapPlayhead, "Snap the playhead to the grid when clicking a ruler (hold Alt to bypass)");
            addToggle (follow, prefs::followPlayhead, "Scroll the playlist to follow the playhead");
            addToggle (returnToStart, prefs::returnToStartOnStop, "Stop returns the playhead to where playback started");
        }

        void resized() override
        {
            SettingsPage::resized();
            pianoSnapBox.setSize (200, pianoSnapBox.getHeight());
            playlistSnapBox.setSize (200, playlistSnapBox.getHeight());
        }

    private:
        juce::ComboBox pianoSnapBox, playlistSnapBox;
        juce::ToggleButton preview, previewPlaying, ghosts, names, snapHead, follow, returnToStart;
    };

    class AudioSettings final : public SettingsPage
    {
    public:
        explicit AudioSettings (Context& c)
            : SettingsPage (c), selector (c.engine.getDeviceManager(), 0, 2, 2, 2, true, false, true, false)
        {
            metronome.setSliderStyle (juce::Slider::LinearHorizontal);
            metronome.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 22);
            metronome.setRange (0.0, 100.0, 1.0);
            metronome.setTextValueSuffix ("%");
            metronome.setValue (prefs::metronomePercent.get (ctx.settings), juce::dontSendNotification);
            metronome.onValueChange = [this] { prefs::metronomePercent.set (ctx.settings, (int) metronome.getValue()); };
            addRow ("Metronome volume", metronome, 28);
            addRow ({}, selector, 380);
        }

        void resized() override
        {
            SettingsPage::resized();
            selector.setBounds (selector.getBounds().withX (getLocalBounds().reduced (24, 0).getX()).withRight (getWidth() - 24));
        }

    private:
        juce::Slider metronome;
        juce::AudioDeviceSelectorComponent selector;
    };

    class PluginSettings final : public SettingsPage, private juce::ChangeListener
    {
    public:
        PluginSettings (Context& c, std::function<void()> scan) : SettingsPage (c), onScan (std::move (scan))
        {
            folders.setMultiLine (true);
            folders.setReturnKeyStartsNewLine (true);
            folders.setText (ctx.settings.getValue ("pluginFolders"), false);
            folders.setTextToShowWhenEmpty ("Extra VST / VST3 folders, one per line", colours::textFaint);
            folders.onTextChange = [this] { ctx.settings.setValue ("pluginFolders", folders.getText()); };
            addRow ("Extra plugin folders", folders, 120);
            addNote (folders, "Harmony always searches the standard VST3 and VST folders as well.");

            scanButton.onClick = [this] { if (onScan) onScan(); };
            addRow ({}, scanButton, 30);

            clearButton.onClick = [this]
            {
                ctx.plugins.getKnownPlugins().clear();
                ctx.plugins.saveList();
                repaint();
            };
            addRow ({}, clearButton, 30);
            addNote (clearButton, "Forget every scanned plugin. Projects keep their plugins and reload them after the next scan.");

            ctx.plugins.getKnownPlugins().addChangeListener (this);
        }

        ~PluginSettings() override
        {
            ctx.plugins.getKnownPlugins().removeChangeListener (this);
        }

        void paint (juce::Graphics& g) override
        {
            SettingsPage::paint (g);
            g.setColour (colours::textDim);
            g.setFont (font (10.0f));
            const int count = ctx.plugins.getKnownPlugins().getNumTypes();
            g.drawText (juce::String (count) + " plugins known, plus " + juce::String ((int) engine::PluginManager::getBuiltinEffects().size()) + " built-in effects",
                        scanButton.getBounds().translated (scanButton.getWidth() + 14, 0).withWidth (360), juce::Justification::centredLeft, false);
        }

        void resized() override
        {
            SettingsPage::resized();
            scanButton.setSize (230, scanButton.getHeight());
            clearButton.setSize (230, clearButton.getHeight());
        }

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override { repaint(); }

        std::function<void()> onScan;
        juce::TextEditor folders;
        juce::TextButton scanButton { "Scan for plugins" }, clearButton { "Clear plugin list" };
    };

    class SettingsPanel final : public juce::Component
    {
    public:
        SettingsPanel (Context& ctx, std::function<void()> scan, int initialPage)
        {
            setSize (820, 560);
            pages.push_back (std::make_unique<GeneralSettings> (ctx));
            pages.push_back (std::make_unique<EditingSettings> (ctx));
            pages.push_back (std::make_unique<AudioSettings> (ctx));
            pages.push_back (std::make_unique<PluginSettings> (ctx, std::move (scan)));

            const char* names[] = { "General", "Editing", "Audio & MIDI", "Plugins" };
            for (int i = 0; i < (int) pages.size(); ++i)
            {
                auto button = std::make_unique<juce::TextButton> (names[i]);
                button->setClickingTogglesState (false);
                button->setColour (juce::TextButton::buttonOnColourId, colours::accent);
                button->onClick = [this, i] { showPage (i); };
                addAndMakeVisible (*button);
                tabs.push_back (std::move (button));
                addChildComponent (*pages[(size_t) i]);
            }
            showPage (juce::jlimit (0, (int) pages.size() - 1, initialPage));
        }

        void showPage (int index)
        {
            for (size_t i = 0; i < pages.size(); ++i)
            {
                pages[i]->setVisible ((int) i == index);
                tabs[i]->setToggleState ((int) i == index, juce::dontSendNotification);
            }
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (colours::line);
            g.fillRect (tabWidth, 8, 1, getHeight() - 16);
        }

        void resized() override
        {
            auto r = getLocalBounds();
            auto column = r.removeFromLeft (tabWidth).reduced (14, 12);
            for (auto& t : tabs)
            {
                t->setBounds (column.removeFromTop (32));
                column.removeFromTop (6);
            }
            for (auto& p : pages)
                p->setBounds (r.withTrimmedLeft (1));
        }

    private:
        static constexpr int tabWidth = 170;
        std::vector<std::unique_ptr<juce::TextButton>> tabs;
        std::vector<std::unique_ptr<juce::Component>> pages;
    };

    class HistoryPanel final : public juce::Component, private juce::ListBoxModel
    {
    public:
        HistoryPanel (Context& c, std::function<void (const juce::File&)> r)
            : ctx (c), restore (std::move (r)), entries (c.history.list())
        {
            setSize (560, 520);
            list.setModel (this);
            list.setRowHeight (40);
            addAndMakeVisible (list);
            restoreButton.setColour (juce::TextButton::buttonColourId, colours::accent);
            restoreButton.setColour (juce::TextButton::textColourOffId, colours::background);
            restoreButton.onClick = [this]
            {
                const int row = list.getSelectedRow();
                if (juce::isPositiveAndBelow (row, entries.size()) && restore)
                    restore (entries[row].file);
            };
            snapshotButton.onClick = [this]
            {
                ctx.history.snapshot ("Manual snapshot");
                entries = ctx.history.list();
                list.updateContent();
                list.repaint();
            };
            addAndMakeVisible (restoreButton);
            addAndMakeVisible (snapshotButton);
        }

        int getNumRows() override { return entries.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (! juce::isPositiveAndBelow (row, entries.size()))
                return;

            const auto& e = entries.getReference (row);
            if (selected)
            {
                g.setColour (colours::controlHover);
                g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 2.0f, (float) w, (float) h - 4.0f), 6.0f);
            }

            g.setColour (colours::text);
            g.setFont (font (11.5f, Weight::medium));
            g.drawText (e.project, 14, 4, w - 28, h / 2, juce::Justification::bottomLeft, true);
            g.setColour (colours::textDim);
            g.setFont (font (10.0f));
            g.drawText (e.time.formatted ("%d %b %Y  %H:%M:%S") + "   " + juce::File::descriptionOfSizeInBytes (e.file.getSize()),
                        14, h / 2, w - 28, h / 2 - 4, juce::Justification::topLeft, true);
        }

        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
        {
            if (juce::isPositiveAndBelow (row, entries.size()) && restore)
                restore (entries[row].file);
        }

        void paint (juce::Graphics& g) override
        {
            if (entries.isEmpty())
            {
                g.setColour (colours::textDim);
                g.setFont (font (11.5f));
                g.drawText ("No snapshots yet. Harmony saves a version of your project every few minutes while you work.",
                            getLocalBounds().reduced (30), juce::Justification::centred, true);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (16, 12);
            auto buttons = r.removeFromBottom (36);
            restoreButton.setBounds (buttons.removeFromRight (180));
            buttons.removeFromRight (8);
            snapshotButton.setBounds (buttons.removeFromRight (170));
            r.removeFromBottom (10);
            list.setBounds (r);
        }

    private:
        Context& ctx;
        std::function<void (const juce::File&)> restore;
        juce::Array<History::Entry> entries;
        juce::ListBox list;
        juce::TextButton restoreButton { "Open this version" }, snapshotButton { "Snapshot now" };
    };

    class AboutPanel final : public juce::Component
    {
    public:
        AboutPanel()
        {
            setSize (520, 460);
            website.setURL (juce::URL ("https://harmony.yannosay.com"));
            website.setButtonText ("harmony.yannosay.com");
            website.setFont (font (11.5f, Weight::medium), false);
            addAndMakeVisible (website);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().reduced (30, 20);
            if (auto* mark = brandMark())
                mark->drawWithin (g, r.removeFromTop (90).toFloat(), juce::RectanglePlacement::centred, 1.0f);

            g.setColour (colours::text);
            g.setFont (font (22.0f, Weight::semibold));
            g.drawText ("Yannosay Harmony", r.removeFromTop (40), juce::Justification::centred, false);
            g.setColour (colours::accent);
            g.setFont (font (11.5f, Weight::medium));
            g.drawText (update::currentVersion().display(), r.removeFromTop (22), juce::Justification::centred, false);
            g.setColour (colours::textDim);
            g.setFont (font (10.5f));
            g.drawText (juce::String::fromUTF8 ("\xc2\xa9 Yannosay Productions"), r.removeFromTop (22), juce::Justification::centred, false);
            r.removeFromTop (54);
            g.setColour (colours::textFaint);
            g.setFont (font (9.5f));
            g.drawFittedText ("Built with JUCE (AGPLv3), TinySoundFont (MIT) and FST (GPLv3) for VST2 hosting.\n"
                              "VST is a trademark of Steinberg Media Technologies GmbH.\nUnbounded typeface under the SIL Open Font License.",
                              r.removeFromTop (70), juce::Justification::centred, 4);
        }

        void resized() override
        {
            website.setBounds (getLocalBounds().withSizeKeepingCentre (220, 24).translated (0, 40));
        }

    private:
        juce::HyperlinkButton website;
    };

    class ShortcutsPanel final : public juce::Component, private juce::ListBoxModel
    {
    public:
        explicit ShortcutsPanel (juce::ApplicationCommandManager& commands)
        {
            setSize (560, 600);
            auto* mappings = commands.getKeyMappings();

            for (auto category : commands.getCommandCategories())
                for (auto commandId : commands.getCommandsInCategory (category))
                    if (auto* info = commands.getCommandForID (commandId))
                    {
                        juce::StringArray keys;
                        for (auto& k : mappings->getKeyPressesAssignedToCommand (commandId))
                            keys.add (k.getTextDescriptionWithIcons());
                        if (! keys.isEmpty())
                            rows.push_back ({ category, info->shortName, keys.joinIntoString ("  /  ") });
                    }

            rows.push_back ({ "Editors", "Draw / select / slice / mute tool", "P / E / C / T" });
            rows.push_back ({ "Editors", "Transpose selection (octave)", "Up / Down (+Shift)" });
            rows.push_back ({ "Editors", "Move selection by snap (bar)", "Left / Right (+Shift)" });
            rows.push_back ({ "Editors", "Clone while dragging", "Shift + drag" });
            rows.push_back ({ "Editors", "Bypass snapping", "Alt + drag" });
            rows.push_back ({ "Editors", "Zoom / scroll / vertical zoom", "Ctrl+wheel / wheel / Alt+wheel" });
            rows.push_back ({ "Editors", "Pan with inertia", "Middle mouse drag" });

            list.setModel (this);
            list.setRowHeight (30);
            addAndMakeVisible (list);
        }

        int getNumRows() override { return (int) rows.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool) override
        {
            if (! juce::isPositiveAndBelow (row, (int) rows.size()))
                return;

            const auto& r = rows[(size_t) row];
            g.setColour (colours::textFaint);
            g.setFont (font (9.0f, Weight::medium));
            g.drawText (r.category.toUpperCase(), 12, 0, 90, h, juce::Justification::centredLeft, true);
            g.setColour (colours::text);
            g.setFont (font (11.0f));
            g.drawText (r.name, 104, 0, w - 300, h, juce::Justification::centredLeft, true);
            g.setColour (colours::accent);
            g.setFont (font (10.5f, Weight::medium));
            g.drawText (r.keys, w - 200, 0, 188, h, juce::Justification::centredRight, true);
        }

        void resized() override { list.setBounds (getLocalBounds().reduced (12)); }

    private:
        struct Item
        {
            juce::String category, name, keys;
        };

        std::vector<Item> rows;
        juce::ListBox list;
    };
}

std::unique_ptr<juce::Component> createChordGeneratorPanel (Context& ctx, std::function<void (const std::vector<NoteData>&, bool)> apply)
{
    return std::make_unique<ChordGeneratorPanel> (ctx, std::move (apply));
}

std::unique_ptr<juce::Component> createFollowPanel (Context& ctx, int channelUid, std::function<void (const std::vector<NoteData>&, bool)> apply)
{
    return std::make_unique<FollowPanel> (ctx, channelUid, std::move (apply));
}

std::unique_ptr<juce::Component> createTextReportPanel (const juce::String& text)
{
    auto editor = std::make_unique<juce::TextEditor>();
    editor->setMultiLine (true);
    editor->setReadOnly (true);
    editor->setScrollbarsShown (true);
    editor->setCaretVisible (false);
    editor->setFont (font (11.0f));
    editor->setColour (juce::TextEditor::backgroundColourId, colours::panel);
    editor->setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    editor->setText (text, false);
    editor->setSize (620, 440);
    return editor;
}

std::unique_ptr<juce::Component> createDrumGeneratorPanel (Context& ctx)
{
    return std::make_unique<DrumGeneratorPanel> (ctx);
}

std::unique_ptr<juce::Component> createRenderPanel (Context& ctx, std::function<void (engine::RenderSettings, bool)> start)
{
    return std::make_unique<RenderPanel> (ctx, std::move (start));
}

std::unique_ptr<juce::Component> createSettingsPanel (Context& ctx, std::function<void()> scanPlugins, int page)
{
    return std::make_unique<SettingsPanel> (ctx, std::move (scanPlugins), page);
}

std::unique_ptr<juce::Component> createHistoryPanel (Context& ctx, std::function<void (const juce::File&)> restore)
{
    return std::make_unique<HistoryPanel> (ctx, std::move (restore));
}

std::unique_ptr<juce::Component> createAboutPanel()
{
    return std::make_unique<AboutPanel>();
}

std::unique_ptr<juce::Component> createShortcutsPanel (juce::ApplicationCommandManager& commands)
{
    return std::make_unique<ShortcutsPanel> (commands);
}
}
