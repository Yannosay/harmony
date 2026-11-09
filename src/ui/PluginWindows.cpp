#include "ui/PluginWindows.h"
#include "engine/AudioEngine.h"
#include "model/MusicTheory.h"

namespace harmony::ui
{
class WindowManager::PluginWindow final : public juce::DocumentWindow
{
public:
    PluginWindow (const juce::String& title, juce::AudioPluginInstance& p, std::function<void()> closed)
        : juce::DocumentWindow (title, colours::panel, juce::DocumentWindow::closeButton | juce::DocumentWindow::minimiseButton),
          plugin (p), onClosed (std::move (closed))
    {
        setUsingNativeTitleBar (false);
        setTitleBarHeight (32);

        juce::AudioProcessorEditor* editor = plugin.hasEditor() ? plugin.createEditorAndMakeActive() : nullptr;
        if (editor == nullptr)
            editor = new juce::GenericAudioProcessorEditor (plugin);

        setContentOwned (editor, true);
        setResizable (editor->isResizable(), false);
        setAlwaysOnTop (false);

        const auto mouse = juce::Desktop::getInstance().getMainMouseSource().getScreenPosition();
        setTopLeftPosition ((int) mouse.x - getWidth() / 2, juce::jmax (40, (int) mouse.y - 60));
        if (auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForPoint (mouse))
            setBounds (getBounds().constrainedWithin (display->userBounds.toNearestInt()));

        setVisible (true);
    }

    ~PluginWindow() override
    {
        clearContentComponent();
    }

    void closeButtonPressed() override
    {
        if (onClosed)
            juce::MessageManager::callAsync (onClosed);
    }

    juce::AudioPluginInstance& plugin;

private:
    std::function<void()> onClosed;
};

WindowManager::WindowManager (Context& c)
    : ctx (c)
{
    startTimer (500);
}

WindowManager::~WindowManager()
{
    closeAll();
}

void WindowManager::closeAll()
{
    windows.clear();
}

void WindowManager::openPlugin (const juce::String& key, juce::AudioPluginInstance& plugin, const juce::String& title)
{
    if (auto it = windows.find (key); it != windows.end())
    {
        auto* existing = dynamic_cast<PluginWindow*> (it->second.get());
        if (existing != nullptr && &existing->plugin == &plugin)
        {
            existing->toFront (true);
            return;
        }
        windows.erase (it);
    }

    windows[key] = std::make_unique<PluginWindow> (title, plugin, [this, key] { windows.erase (key); });
}

void WindowManager::openChannel (int channelUid)
{
    auto channel = ctx.project.findChannel (channelUid);
    if (! channel.isValid())
        return;

    const auto kindName = channel[id::kind].toString();

    if (kindName == kind::plugin)
    {
        if (auto* plugin = ctx.sync.pluginForChannel (channelUid))
            openPlugin ("c" + juce::String (channelUid), *plugin, channel[id::name].toString());
        else if (ctx.notify)
            ctx.notify (channel[id::name].toString() + " is still loading or failed to load", true);
        return;
    }

    if (ctx.showPanel)
    {
        if (kindName == kind::soundfont)
            ctx.showPanel (createSoundFontEditor (ctx, channelUid), channel[id::name].toString());
        else
            ctx.showPanel (createSamplerEditor (ctx, channelUid), channel[id::name].toString());
    }
}

void WindowManager::openSlot (int slotUid)
{
    if (auto* plugin = ctx.sync.pluginForSlot (slotUid))
        openPlugin ("s" + juce::String (slotUid), *plugin, plugin->getName());
    else if (ctx.notify)
        ctx.notify ("This effect is still loading or failed to load", true);
}

void WindowManager::timerCallback()
{
    for (auto it = windows.begin(); it != windows.end();)
    {
        const auto& key = it->first;
        const int uid = key.substring (1).getIntValue();
        auto* window = dynamic_cast<PluginWindow*> (it->second.get());
        auto* current = key.startsWith ("c") ? ctx.sync.pluginForChannel (uid) : ctx.sync.pluginForSlot (uid);

        if (window == nullptr || current != &window->plugin)
            it = windows.erase (it);
        else
            ++it;
    }
}

namespace
{
    class SampleView final : public juce::Component
    {
    public:
        SampleView (Context& c, int uid) : ctx (c), channelUid (uid) {}

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat();
            g.setColour (colours::background);
            g.fillRoundedRectangle (r, 8.0f);

            auto sample = ctx.sync.sampleForChannel (channelUid);
            if (sample == nullptr)
            {
                g.setColour (colours::textFaint);
                g.setFont (font (11.0f));
                g.drawText ("Drop an audio file here or choose one below", r, juce::Justification::centred, false);
                return;
            }

            const int n = sample->audio.getNumSamples();
            const float mid = r.getCentreY();
            const float amp = r.getHeight() * 0.45f;
            g.setColour (colours::accent);

            for (int x = 0; x < getWidth(); ++x)
            {
                const int s0 = (int) ((juce::int64) n * x / getWidth());
                const int s1 = juce::jmax (s0 + 1, (int) ((juce::int64) n * (x + 1) / getWidth()));
                const auto range = juce::FloatVectorOperations::findMinAndMax (sample->audio.getReadPointer (0, s0), juce::jmin (s1, n) - s0);
                g.fillRect ((float) x, mid - range.getEnd() * amp, 1.0f, juce::jmax (1.0f, (range.getEnd() - range.getStart()) * amp));
            }

            g.setColour (colours::textDim);
            g.setFont (font (10.0f));
            g.drawText (juce::String (n / sample->sampleRate, 2) + " s   " + juce::String (sample->sampleRate / 1000.0, 1) + " kHz",
                        r.reduced (10.0f, 6.0f), juce::Justification::bottomRight, false);
        }

    private:
        Context& ctx;
        int channelUid;
    };

    class SamplerEditor final : public juce::Component, private juce::Timer
    {
    public:
        SamplerEditor (Context& c, int uid)
            : ctx (c), channel (c.project.findChannel (uid)), view (c, uid)
        {
            setSize (560, 330);
            addAndMakeVisible (view);

            auto* undo = ctx.project.um();
            attack.bind (channel, id::attack, undo);
            attack.describe = [] (double v) { return "Attack " + juce::String (v * 1000.0, 1) + " ms"; };
            release.bind (channel, id::release, undo);
            release.describe = [] (double v) { return "Release " + juce::String (v * 1000.0, 0) + " ms"; };
            addAndMakeVisible (attack);
            addAndMakeVisible (release);

            root.setValue ((int) channel.getProperty (id::rootNote, 60), juce::dontSendNotification);
            root.setLabel ("ROOT");
            root.onGestureStart = [this] { ctx.project.beginTransaction ("Root note"); };
            root.onChange = [this] (double v) { channel.setProperty (id::rootNote, (int) v, ctx.project.um()); repaint(); };
            addAndMakeVisible (root);

            oneShot.setButtonText ("One-shot (ignore note length)");
            oneShot.setToggleState ((bool) channel[id::oneShot], juce::dontSendNotification);
            oneShot.onClick = [this]
            {
                ctx.project.beginTransaction ("One-shot");
                channel.setProperty (id::oneShot, oneShot.getToggleState(), ctx.project.um());
            };
            addAndMakeVisible (oneShot);

            chooseButton.onClick = [this]
            {
                chooser = std::make_unique<juce::FileChooser> ("Choose a sample", juce::File (channel[id::samplePath].toString()).getParentDirectory(),
                                                               ctx.engine.getFormats().getWildcardForAllFormats());
                chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
                {
                    const auto f = fc.getResult();
                    if (! f.existsAsFile())
                        return;
                    ctx.project.beginTransaction ("Replace sample");
                    channel.setProperty (id::samplePath, f.getFullPathName(), ctx.project.um());
                    channel.setProperty (id::name, f.getFileNameWithoutExtension(), ctx.project.um());
                });
            };
            addAndMakeVisible (chooseButton);
            startTimerHz (4);
        }

        void timerCallback() override { view.repaint(); }

        void paint (juce::Graphics& g) override
        {
            g.setColour (colours::textDim);
            g.setFont (font (10.5f));
            g.drawText (juce::File (channel[id::samplePath].toString()).getFileName(), getLocalBounds().reduced (20, 0).removeFromTop (30),
                        juce::Justification::centredLeft, true);
            g.drawText ("Root " + theory::noteName ((int) channel.getProperty (id::rootNote, 60)), root.getBounds().translated (0, 26).withHeight (16),
                        juce::Justification::centred, false);
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (20, 0);
            r.removeFromTop (30);
            view.setBounds (r.removeFromTop (150));
            r.removeFromTop (14);
            auto row = r.removeFromTop (44);
            attack.setBounds (row.removeFromLeft (44));
            row.removeFromLeft (8);
            release.setBounds (row.removeFromLeft (44));
            row.removeFromLeft (20);
            root.setBounds (row.removeFromLeft (110).withSizeKeepingCentre (110, 28));
            row.removeFromLeft (20);
            oneShot.setBounds (row.withSizeKeepingCentre (row.getWidth(), 24));
            r.removeFromTop (30);
            chooseButton.setBounds (r.removeFromTop (32).removeFromLeft (180));
        }

    private:
        Context& ctx;
        juce::ValueTree channel;
        SampleView view;
        Knob attack { 0.0005, 2.0, 0.001, "Attack" };
        Knob release { 0.002, 4.0, 0.08, "Release" };
        NumberBox root { 0.0, 127.0, 1.0, 0 };
        juce::ToggleButton oneShot;
        juce::TextButton chooseButton { "Replace sample..." };
        std::unique_ptr<juce::FileChooser> chooser;
    };

    class SoundFontEditor final : public juce::Component, private juce::ListBoxModel, private juce::Timer
    {
    public:
        SoundFontEditor (Context& c, int uid)
            : ctx (c), channelUid (uid), channel (c.project.findChannel (uid))
        {
            setSize (460, 460);
            filter.setTextToShowWhenEmpty ("Filter presets", colours::textFaint);
            filter.onTextChange = [this] { rebuild(); };
            addAndMakeVisible (filter);
            list.setModel (this);
            list.setRowHeight (28);
            addAndMakeVisible (list);
            rebuild();
            if (presets.isEmpty())
                startTimer (300);
        }

        void timerCallback() override
        {
            rebuild();
            if (! presets.isEmpty())
                stopTimer();
        }

        void rebuild()
        {
            presets = ctx.sync.soundFontPresets (channelUid);
            visible.clear();
            for (int i = 0; i < presets.size(); ++i)
                if (filter.getText().isEmpty() || presets[i].containsIgnoreCase (filter.getText()))
                    visible.add (i);
            list.updateContent();
            list.repaint();
        }

        int getNumRows() override { return visible.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool) override
        {
            if (! juce::isPositiveAndBelow (row, visible.size()))
                return;

            const int index = visible[row];
            const bool active = (int) channel[id::sfPreset] == index;
            if (active)
            {
                g.setColour (colours::accent.withAlpha (0.16f));
                g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 1.0f, (float) w, (float) h - 2.0f), 5.0f);
            }
            g.setColour (active ? colours::accent : colours::text);
            g.setFont (font (11.0f, active ? Weight::semibold : Weight::regular));
            g.drawText (juce::String (index + 1).paddedLeft ('0', 3) + "   " + presets[index], 12, 0, w - 24, h, juce::Justification::centredLeft, true);
        }

        void listBoxItemClicked (int row, const juce::MouseEvent&) override
        {
            if (! juce::isPositiveAndBelow (row, visible.size()))
                return;

            ctx.project.beginTransaction ("Change preset");
            channel.setProperty (id::sfPreset, visible[row], ctx.project.um());
            ctx.engine.sendPreviewNote (channelUid, 60, 0.8f, true);
            juce::Timer::callAfterDelay (300, [&engine = ctx.engine, u = channelUid] { engine.sendPreviewNote (u, 60, 0.0f, false); });
            list.repaint();
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (16, 10);
            filter.setBounds (r.removeFromTop (32));
            r.removeFromTop (8);
            list.setBounds (r);
        }

    private:
        Context& ctx;
        int channelUid;
        juce::ValueTree channel;
        juce::TextEditor filter;
        juce::ListBox list;
        juce::StringArray presets;
        juce::Array<int> visible;
    };
}

std::unique_ptr<juce::Component> createSamplerEditor (Context& ctx, int channelUid)
{
    return std::make_unique<SamplerEditor> (ctx, channelUid);
}

std::unique_ptr<juce::Component> createSoundFontEditor (Context& ctx, int channelUid)
{
    return std::make_unique<SoundFontEditor> (ctx, channelUid);
}
}
