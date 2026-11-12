#include "app/MainComponent.h"
#include "app/Preferences.h"
#include "ui/Dialogs.h"
#include "ui/EffectEditor.h"
#include "ui/AutomationEditor.h"
#include "engine/BuiltinEffects.h"
#include "model/FlpImport.h"
#include "model/MidiIO.h"
#include "model/ChordGenerator.h"
#include "model/DrumGenerator.h"

namespace harmony
{
using namespace ui;

namespace
{
    constexpr int topBarHeight = 54;
    constexpr int statusHeight = 26;
    constexpr int dockBarHeight = 36;
}

class MainComponent::DockBar final : public juce::Component
{
public:
    explicit DockBar (MainComponent& o) : owner (o), patterns (o.ctx)
    {
        addAndMakeVisible (patterns);
        for (auto* b : { &rackButton, &pianoButton, &mixerButton })
        {
            b->setClickingTogglesState (false);
            b->setColour (juce::TextButton::buttonOnColourId, colours::accent);
            addAndMakeVisible (b);
        }
        keepFocusOnEditors ({ &rackButton, &pianoButton, &mixerButton });
        rackButton.setTooltip ("Channel Rack (F6): your instruments and step sequencer");
        pianoButton.setTooltip ("Piano Roll (F7): edit the notes of the selected channel");
        mixerButton.setTooltip ("Mixer (F9): effects, volume and routing. Drop effects onto a strip");
        rackButton.onClick = [this] { owner.showDock (Dock::rack); };
        pianoButton.onClick = [this] { owner.showDock (Dock::pianoRoll); };
        mixerButton.onClick = [this] { owner.showDock (Dock::mixer); };
    }

    void update (Dock dock)
    {
        rackButton.setToggleState (dock == Dock::rack, juce::dontSendNotification);
        pianoButton.setToggleState (dock == Dock::pianoRoll, juce::dontSendNotification);
        mixerButton.setToggleState (dock == Dock::mixer, juce::dontSendNotification);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::panel);
        g.setColour (colours::line);
        g.fillRect (0, 0, getWidth(), 1);
        g.fillRect (0, getHeight() - 1, getWidth(), 1);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (8, 5);
        for (auto* b : { &rackButton, &pianoButton, &mixerButton })
        {
            b->setBounds (r.removeFromLeft (124));
            r.removeFromLeft (6);
        }
        r.removeFromLeft (10);
        patterns.setBounds (r);
    }

private:
    MainComponent& owner;
    ui::PatternBar patterns;
    juce::TextButton rackButton { "Channel Rack" }, pianoButton { "Piano Roll" }, mixerButton { "Mixer" };
};

class MainComponent::StatusBar final : public juce::Component
{
public:
    void setHint (const juce::String& text)
    {
        if (text != hint)
        {
            hint = text;
            repaint();
        }
    }

    void setInfo (const juce::String& text)
    {
        if (text != info)
        {
            info = text;
            repaint();
        }
    }

    void setProgress (float p, const juce::String& text)
    {
        progress = p;
        progressText = text;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (colours::panel);
        g.setColour (colours::line);
        g.fillRect (0, 0, getWidth(), 1);

        auto r = getLocalBounds().reduced (12, 0);
        g.setFont (font (10.0f));
        g.setColour (colours::textFaint);
        g.drawText (info, r, juce::Justification::centredRight, true);

        if (progress >= 0.0f)
        {
            auto bar = r.removeFromLeft (220).withSizeKeepingCentre (220, 6).toFloat();
            g.setColour (colours::control);
            g.fillRoundedRectangle (bar, 3.0f);
            juce::ColourGradient grad (colours::accent, bar.getX(), 0.0f, colours::lilac, bar.getRight(), 0.0f, false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (bar.withWidth (juce::jmax (6.0f, bar.getWidth() * progress)), 3.0f);
            r.removeFromLeft (10);
            g.setColour (colours::textDim);
            g.drawText (progressText, r, juce::Justification::centredLeft, true);
            return;
        }

        g.setColour (colours::textDim);
        g.drawText (hint, r.withTrimmedRight (360), juce::Justification::centredLeft, true);
    }

private:
    juce::String hint, info, progressText;
    float progress = -1.0f;
};

MainComponent::MainComponent (Context& c)
    : ctx (c),
      topBar (c, *this),
      browser (c),
      playlist (c),
      rack (c),
      pianoRoll (c),
      mixer (c),
      windows (c)
{
    bindContext();

    dockBar = std::make_unique<DockBar> (*this);
    statusBar = std::make_unique<StatusBar>();

    horizontalLayout.setItemLayout (0, 180, 420, 250);
    horizontalLayout.setItemLayout (1, 6, 6, 6);
    horizontalLayout.setItemLayout (2, 400, -1.0, -1.0);
    verticalLayout.setItemLayout (0, 120, -1.0, -0.52);
    verticalLayout.setItemLayout (1, 6, 6, 6);
    verticalLayout.setItemLayout (2, 160, -1.0, -0.48);

    browserResizer = std::make_unique<juce::StretchableLayoutResizerBar> (&horizontalLayout, 1, true);
    dockResizer = std::make_unique<juce::StretchableLayoutResizerBar> (&verticalLayout, 1, false);

    for (juce::Component* comp : { (juce::Component*) &topBar, (juce::Component*) &browser, (juce::Component*) &playlist,
                                   (juce::Component*) &rack, (juce::Component*) &pianoRoll, (juce::Component*) &mixer,
                                   (juce::Component*) dockBar.get(), (juce::Component*) statusBar.get(),
                                   (juce::Component*) browserResizer.get(), (juce::Component*) dockResizer.get() })
        addAndMakeVisible (comp);

    addAndMakeVisible (toasts);
    toasts.setAlwaysOnTop (true);

    topBar.onUpdateClicked = [this]
    {
        confirmDiscard ([this] { ctx.updater.installAndQuit (true); });
    };

    ctx.updater.onStateChanged = [this]
    {
        using State = update::Updater::State;
        switch (ctx.updater.getState())
        {
            case State::ready:
                topBar.setUpdateReady (ctx.updater.getAvailableVersion());
                showToast ("Harmony " + ctx.updater.getAvailableVersion() + " is ready. It installs when you close Harmony, or click the update icon.");
                break;
            case State::idle:
                if (ctx.updater.getAvailableVersion().isNotEmpty())
                    showToast ("Harmony " + ctx.updater.getAvailableVersion() + " is available. Enable automatic updates in Preferences or check again.");
                break;
            case State::failed:
                statusBar->setHint ("Update check failed: " + ctx.updater.getError());
                break;
            case State::checking:
            case State::downloading:
            case State::upToDate:
                break;
        }
    };

    ctx.sync.onError = [this] (const juce::String& message) { queueError (message); };
    ctx.sync.onNodesChanged = [this]
    {
        rack.repaint();
        mixer.repaint();
        playlist.repaint();
    };

    ctx.project.onStateReplaced = [this]
    {
        ctx.sync.clearFailures();
        windows.closeAll();
        ctx.engine.stop();
        ctx.engine.clearAutomation();
        updateTitle();
    };

    ui::installEffectEditors (ctx);
    ctx.settings.addChangeListener (this);

    showDock (Dock::rack);
    setSize (1500, 920);
    startTimerHz (20);
}

MainComponent::~MainComponent()
{
    ctx.settings.removeChangeListener (this);
    ui::removeEffectEditors();
    ctx.project.onStateReplaced = nullptr;
    ctx.sync.onError = nullptr;
    ctx.sync.onNodesChanged = nullptr;
    ctx.updater.onStateChanged = nullptr;
    windows.closeAll();
}

void MainComponent::bindContext()
{
    ctx.notify = [this] (const juce::String& m, bool e) { showToast (m, e); };
    ctx.setHint = [this] (const juce::String& h) { if (statusBar) statusBar->setHint (h); };
    ctx.openPianoRoll = [this] (int uid)
    {
        if (uid > 0)
            ctx.project.setSelectedChannel (uid);
        pianoRoll.setChannel (ctx.project.selectedChannel());
        showDock (Dock::pianoRoll);
    };
    ctx.openChannelEditor = [this] (int uid) { windows.openChannel (uid); };
    ctx.openSlotEditor = [this] (int uid) { windows.openSlot (uid); };
    ctx.focusMixerInsert = [this] (int index) { showDock (Dock::mixer); mixer.focusInsert (index); };
    ctx.showPanel = [this] (std::unique_ptr<juce::Component> p, const juce::String& t) { showPanel (std::move (p), t); };
    ctx.closePanel = [this] { Overlay::closeAll (*this); };
    ctx.importAudio = [this] (const juce::File& f, int track, juce::int64 tick) { importAudio (f, track, tick); };
    ctx.addInstrument = [this] (const juce::String& identifier) { addInstrumentChannel (identifier); };
    ctx.addFileAsChannel = [this] (const juce::File& f) { addFileAsChannel (f); };
    ctx.createAutomation = [this] (const automation::Target& t, const juce::String& name)
    {
        createAutomationClip (t, name, automation::currentNormalisedValue (ctx.project, t));
    };
    ctx.automateParameter = [this] (juce::AudioProcessor& p, int index) { automatePluginParameter (p, index); };
    ctx.effectTargetInsert = [this] { return effectTargetInsert(); };
    ctx.toggleEditorMaximised = [this] { setEditorMaximised (! editorMaximised); };
    ctx.isEditorMaximised = [this] { return editorMaximised; };
    ctx.addEffect = [this] (int insert, const juce::String& identifier, int slot) { return addEffectToInsert (insert, identifier, slot); };
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    applySettings();
}

void MainComponent::applySettings()
{
    auto& s = ctx.settings;
    ctx.plugins.extraFolders = juce::StringArray::fromLines (s.getValue ("pluginFolders"));
    ctx.plugins.extraFolders.removeEmptyStrings();
    ctx.history.setIntervalMinutes (prefs::autosaveMinutes[prefs::autosaveChoice.get (s)]);
    ctx.engine.setMetronomeLevel ((float) prefs::metronomePercent.get (s) / 100.0f);
    playlist.setFollowPlayhead (prefs::followPlayhead.get (s));
    pianoRoll.setGhostsVisible (prefs::ghostNotes.get (s));
    pianoRoll.repaint();

    const float scale = (float) prefs::uiScalePercent.get (s) / 100.0f;
    if (std::abs (juce::Desktop::getInstance().getGlobalScaleFactor() - scale) > 0.001f)
        juce::Desktop::getInstance().setGlobalScaleFactor (scale);
}

int MainComponent::effectTargetInsert() const
{
    if (currentDock == Dock::mixer)
        return mixer.getSelectedInsert();

    auto channel = ctx.project.findChannel (ctx.project.selectedChannel());
    return channel.isValid() ? (int) channel[id::insert] : 0;
}

bool MainComponent::addEffectToInsert (int insertIndex, const juce::String& identifier, int slotIndex)
{
    auto desc = ctx.plugins.findByIdentifier (identifier);
    auto insert = ctx.project.findInsert (insertIndex);
    if (! desc.has_value() || ! insert.isValid())
    {
        showToast ("That effect is no longer available. Rescan your plugins.", true);
        return false;
    }

    if (! engine::PluginManager::isUsableEffect (*desc))
    {
        showToast (desc->name + " is an instrument. Drop it on the Channel Rack instead.", true);
        return false;
    }

    if (slotIndex < 0)
    {
        for (int s = 0; s < numFxSlots && slotIndex < 0; ++s)
        {
            bool used = false;
            for (auto slot : insert)
                used = used || (int) slot[id::index] == s;
            if (! used)
                slotIndex = s;
        }
    }

    if (slotIndex < 0)
    {
        showToast ("All " + juce::String (numFxSlots) + " effect slots on " + insert[id::name].toString() + " are in use", true);
        return false;
    }

    ctx.project.beginTransaction ("Add effect");
    ctx.project.addSlot (insertIndex, slotIndex, engine::PluginManager::toXmlString (*desc));
    showDock (Dock::mixer);
    mixer.focusInsert (insertIndex);
    showToast ("Added " + desc->name + " to " + insert[id::name].toString());
    return true;
}

void MainComponent::createAutomationClip (const automation::Target& target, const juce::String& name, float initialValue)
{
    auto& project = ctx.project;
    const auto bar = project.ticksPerBar();
    const juce::int64 loopStart = project.getState()[id::loopStart], loopEnd = project.getState()[id::loopEnd];
    const bool hasLoop = loopEnd > loopStart;

    const auto start = hasLoop ? loopStart : ((juce::int64) (ctx.engine.isSongMode() ? ctx.engine.getDisplayPositionTicks() : 0.0) / bar) * bar;
    const auto length = hasLoop ? loopEnd - loopStart : bar * 4;

    juce::ValueTree track;
    for (auto t : project.tracksTree())
    {
        bool free = true;
        for (auto clip : t)
            free = free && ((juce::int64) clip[id::start] >= start + length || (juce::int64) clip[id::start] + (juce::int64) clip[id::length] <= start);
        if (free)
        {
            track = t;
            break;
        }
    }

    project.beginTransaction ("Add automation");
    if (! track.isValid())
    {
        track = juce::ValueTree (id::TRACK);
        track.setProperty (id::uid, project.allocateUid(), nullptr);
        track.setProperty (id::name, "Automation", nullptr);
        track.setProperty (id::mute, false, nullptr);
        project.tracksTree().appendChild (track, project.um());
    }

    auto clip = automation::addClip (project, track, target, start, length, initialValue, name);
    ctx.engine.setSongMode (true);
    showToast ("Automation for " + name + " added to " + track[id::name].toString() + ". Drag its points in the playlist or double-click it.");
    showPanel (ui::createAutomationEditor (ctx, clip), "Automation");
}

void MainComponent::automatePluginParameter (juce::AudioProcessor& processor, int parameterIndex)
{
    const auto& params = processor.getParameters();
    if (! juce::isPositiveAndBelow (parameterIndex, params.size()))
        return;

    auto* param = params[parameterIndex];
    const auto name = processor.getName() + " " + param->getName (40);

    for (auto insert : ctx.project.mixerTree())
        for (auto slot : insert)
            if (ctx.sync.pluginForSlot (slot[id::uid]) == &processor)
            {
                createAutomationClip ({ automation::Kind::slotParameter, (int) slot[id::uid], parameterIndex }, name, param->getValue());
                return;
            }

    for (auto channel : ctx.project.channelsTree())
        if (ctx.sync.pluginForChannel (channel[id::uid]) == &processor)
        {
            createAutomationClip ({ automation::Kind::channelParameter, (int) channel[id::uid], parameterIndex },
                                  channel[id::name].toString() + " " + param->getName (40), param->getValue());
            return;
        }

    showToast ("This plugin is not part of the project anymore", true);
}

void MainComponent::queueError (const juce::String& message)
{
    pendingErrors.addIfNotAlreadyThere (message);
    if (pendingErrors.size() > 1)
        return;

    juce::Timer::callAfterDelay (400, [safe = juce::Component::SafePointer<MainComponent> (this)]
    {
        if (safe == nullptr || safe->pendingErrors.isEmpty())
            return;

        auto& errors = safe->pendingErrors;
        if (errors.size() == 1)
            safe->showToast (errors[0], true);
        else
            safe->showToast (juce::String (errors.size()) + " items could not be loaded: " + errors[0]
                             + (errors.size() > 1 ? ", " + errors[1] : juce::String()) + (errors.size() > 2 ? " and more" : ""), true);
        errors.clear();
    });
}

void MainComponent::showSettings (int page)
{
    showPanel (createSettingsPanel (ctx, [this] { startPluginScan(); }, page), "Settings");
}

void MainComponent::startup()
{
    applySettings();

    if (ctx.plugins.getKnownPlugins().getNumTypes() == 0 && ! ctx.settings.getBoolValue ("scannedOnce", false))
        juce::Timer::callAfterDelay (1200, [safe = juce::Component::SafePointer<MainComponent> (this)] { if (safe) safe->startPluginScan(); });

    if (prefs::autoUpdate.get (ctx.settings))
        juce::Timer::callAfterDelay (4000, [safe = juce::Component::SafePointer<MainComponent> (this)] { if (safe) safe->ctx.updater.check (true); });

    if (ctx.history.previousSessionCrashed())
    {
        const auto latest = ctx.history.latestSnapshot();
        if (latest.existsAsFile())
        {
            auto options = juce::MessageBoxOptions::makeOptionsYesNo (juce::MessageBoxIconType::QuestionIcon, "Restore your work?",
                                                                     "Harmony did not close properly last time. Open the most recent time machine snapshot from "
                                                                     + latest.getLastModificationTime().formatted ("%H:%M on %d %b") + "?",
                                                                     "Restore", "Start fresh", this);
            juce::AlertWindow::showAsync (options, [this, latest] (int result)
            {
                if (result == 1 && ctx.project.loadFrom (latest).wasOk())
                {
                    ctx.project.setFile ({});
                    updateTitle();
                    showToast ("Restored the latest snapshot. Save it to keep it.");
                }
            });
            updateTitle();
            return;
        }
    }

    if (prefs::openLastProject.get (ctx.settings) && ! ctx.project.getFile().existsAsFile())
    {
        auto recent = juce::StringArray::fromLines (ctx.settings.getValue ("recentProjects"));
        recent.removeEmptyStrings();
        if (! recent.isEmpty() && juce::File (recent[0]).existsAsFile())
            openProjectFile (juce::File (recent[0]));
    }

    updateTitle();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void MainComponent::setEditorMaximised (bool shouldMaximise)
{
    editorMaximised = shouldMaximise;
    resized();
    pianoRoll.refreshMaximiseButton();
    if (editorMaximised && currentDock == Dock::pianoRoll)
        pianoRoll.grabKeyboardFocus();
}

void MainComponent::layoutMiddle()
{
    auto r = getLocalBounds();
    r.removeFromTop (topBarHeight);
    r.removeFromBottom (statusHeight);

    playlist.setVisible (! editorMaximised);
    dockResizer->setVisible (! editorMaximised);

    if (editorMaximised)
    {
        browser.setVisible (false);
        browserResizer->setVisible (false);
        dockBar->setBounds (r.removeFromTop (dockBarHeight));
        rack.setBounds (r);
        pianoRoll.setBounds (r);
        mixer.setBounds (r);
        return;
    }

    browser.setVisible (browserVisible);
    browserResizer->setVisible (browserVisible);

    if (browserVisible)
    {
        juce::Component* items[] = { &browser, browserResizer.get(), &editorArea };
        horizontalLayout.layOutComponents (items, 3, r.getX(), r.getY(), r.getWidth(), r.getHeight(), false, true);
        r = editorArea.getBounds();
    }

    juce::Component* vertical[] = { &playlist, dockResizer.get(), &dockArea };
    verticalLayout.layOutComponents (vertical, 3, r.getX(), r.getY(), r.getWidth(), r.getHeight(), true, true);

    auto dock = dockArea.getBounds();
    dockBar->setBounds (dock.removeFromTop (dockBarHeight));
    rack.setBounds (dock);
    pianoRoll.setBounds (dock);
    mixer.setBounds (dock);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    topBar.setBounds (r.removeFromTop (topBarHeight));
    statusBar->setBounds (r.removeFromBottom (statusHeight));
    layoutMiddle();
    toasts.setBounds (getLocalBounds().withTrimmedBottom (statusHeight + 10).withTrimmedTop (topBarHeight));

    for (int i = 0; i < getNumChildComponents(); ++i)
        if (auto* overlay = dynamic_cast<Overlay*> (getChildComponent (i)))
            overlay->setBounds (getLocalBounds());
}

void MainComponent::showDock (Dock dock)
{
    currentDock = dock;
    rack.setVisible (dock == Dock::rack);
    pianoRoll.setVisible (dock == Dock::pianoRoll);
    mixer.setVisible (dock == Dock::mixer);
    dockBar->update (dock);

    if (dock == Dock::pianoRoll)
        pianoRoll.grabKeyboardFocus();
}

void MainComponent::showToast (const juce::String& message, bool error)
{
    toasts.toFront (false);
    toasts.show (message, error);
}

void MainComponent::showPanel (std::unique_ptr<juce::Component> panel, const juce::String& title)
{
    auto* overlay = Overlay::show (*this, std::move (panel), title);
    overlay->toFront (true);
    toasts.toFront (false);
}

void MainComponent::timerCallback()
{
    drainRecording();

    if (renderer != nullptr)
        statusBar->setProgress (renderer->getProgress(), "Rendering " + renderer->getSettings().output.getFileName() + "  "
                                                           + juce::String (juce::roundToInt (renderer->getProgress() * 100.0f)) + "%");

    juce::String info;
    if (auto* device = ctx.engine.getDeviceManager().getCurrentAudioDevice())
        info << device->getName() << "   " << juce::String (device->getCurrentSampleRate() / 1000.0, 1) << " kHz   "
             << device->getCurrentBufferSizeSamples() << " smp   "
             << juce::String (device->getOutputLatencyInSamples() * 1000.0 / juce::jmax (1.0, device->getCurrentSampleRate()), 1) << " ms";
    else
        info << "No audio device";

    if (ctx.sync.isLoading())
        info = "Loading instruments...   " + info;

    info << "   " << update::currentVersion().display();
    statusBar->setInfo (info);
    updateTitle();
}

void MainComponent::updateTitle()
{
    const auto title = ctx.project.getDisplayName() + (ctx.project.isDirty() ? " *" : "") + juce::String::fromUTF8 ("  \xc2\xb7  Yannosay Harmony");
    if (title != lastTitle)
    {
        lastTitle = title;
        if (onTitleChanged)
            onTitleChanged (title);
    }
}

void MainComponent::drainRecording()
{
    recorded.clear();
    ctx.engine.drainRecorded (recorded);
    if (recorded.empty())
        return;

    auto pattern = ctx.project.currentPattern();
    const auto length = juce::jmax ((juce::int64) 1, ctx.project.patternLength (pattern));
    bool began = false;

    for (auto& e : recorded)
    {
        const double tick = std::fmod (e.tick, (double) length);
        const int key = e.channel * 128 + e.key;

        if (e.on)
        {
            heldRecording[key] = { tick, e.velocity };
            continue;
        }

        auto it = heldRecording.find (key);
        if (it == heldRecording.end())
            continue;

        if (! began)
        {
            ctx.project.beginTransaction ("Record notes");
            began = true;
        }

        NoteData n;
        n.channel = e.channel;
        n.key = e.key;
        n.start = (juce::int64) it->second.first;
        n.length = juce::jmax ((juce::int64) 10, (juce::int64) (tick >= it->second.first ? tick - it->second.first : (double) length - it->second.first + tick));
        n.velocity = it->second.second;
        ctx.project.addNote (pattern, n);
        heldRecording.erase (it);
    }
}

void MainComponent::handleTapTempo()
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (! taps.isEmpty() && now - taps.getLast() > 2000.0)
        taps.clear();

    taps.add (now);
    if (taps.size() > 8)
        taps.remove (0);

    if (taps.size() >= 3)
    {
        const double avg = (taps.getLast() - taps.getFirst()) / (taps.size() - 1);
        ctx.project.beginTransaction ("Tap tempo");
        ctx.project.setBpm (std::round (60000.0 / avg * 10.0) / 10.0);
    }
}

ui::EditTarget* MainComponent::selectionEditor()
{
    const bool piano = pianoRoll.isVisible() && pianoRoll.hasSelection();
    const bool clips = playlist.hasSelection();
    if (piano != clips)
        return piano ? static_cast<ui::EditTarget*> (&pianoRoll) : &playlist;
    return activeEditor();
}

ui::EditTarget* MainComponent::activeEditor()
{
    const auto mouse = getMouseXYRelative();
    if (pianoRoll.isVisible() && pianoRoll.getBounds().contains (mouse))
        return &pianoRoll;
    if (playlist.getBounds().contains (mouse))
        return &playlist;

    auto* focused = juce::Component::getCurrentlyFocusedComponent();
    if (focused == &pianoRoll || pianoRoll.isParentOf (focused))
        return &pianoRoll;
    if (focused == &playlist || playlist.isParentOf (focused))
        return &playlist;
    return currentDock == Dock::pianoRoll ? static_cast<ui::EditTarget*> (&pianoRoll) : &playlist;
}

void MainComponent::openPatternOffset (int delta)
{
    auto patterns = ctx.project.patternsTree();
    const int index = patterns.indexOf (ctx.project.currentPattern());
    const int next = juce::jlimit (0, patterns.getNumChildren() - 1, index + delta);
    ctx.project.setCurrentPattern (patterns.getChild (next)[id::uid]);
}

void MainComponent::getAllCommands (juce::Array<juce::CommandID>& c)
{
    using namespace cmd;
    c.addArray ({ newProject, openProject, saveProject, saveProjectAs, exportAudio, exportMidi, importFile, projectHistory, quit, maximiseEditor,
                  undo, redo, cut, copy, paste, deleteSelection, selectAll, duplicate, quantize, humanize, toggleMuteSelection,
                  playPause, stop, record, songMode, metronome, returnToStart, panic, tapTempo,
                  showPlaylist, showChannelRack, showPianoRoll, showMixer, toggleBrowser, commandPalette, fullscreen,
                  addInstrument, addSoundFont, addSampler, newPattern, clonePattern, nextPattern, previousPattern,
                  chordGenerator, drumGenerator, scanPlugins, audioSettings, preferences,
                  toolDraw, toolSelect, toolSlice, toolMute,
                  shortcuts, checkUpdates, about, website });
}

void MainComponent::getCommandInfo (juce::CommandID command, juce::ApplicationCommandInfo& info)
{
    using namespace cmd;
    using K = juce::KeyPress;
    using M = juce::ModifierKeys;
    const auto ctrl = M::commandModifier;
    const auto shift = M::shiftModifier;
    const auto alt = M::altModifier;

    auto set = [&info] (const char* name, const char* category, const char* description)
    {
        info.setInfo (name, description, category, 0);
    };

    switch (command)
    {
        case newProject:      set ("New project", "File", "Start a new empty project"); info.addDefaultKeypress ('n', ctrl); break;
        case openProject:     set ("Open project...", "File", "Open a Harmony project"); info.addDefaultKeypress ('o', ctrl); break;
        case saveProject:     set ("Save", "File", "Save the project"); info.addDefaultKeypress ('s', ctrl); break;
        case saveProjectAs:   set ("Save as...", "File", "Save the project under a new name"); info.addDefaultKeypress ('s', ctrl | shift); break;
        case exportAudio:     set ("Render / export audio...", "File", "Render to WAV, FLAC, OGG or AIFF, with optional stems"); info.addDefaultKeypress ('r', ctrl); break;
        case exportMidi:      set ("Export MIDI...", "File", "Save the song or the current pattern as a MIDI file"); info.addDefaultKeypress ('m', ctrl | shift); break;
        case importFile:      set ("Import FL Studio project or MIDI...", "File", "Convert an .flp project, or bring a MIDI file in as a new pattern"); info.addDefaultKeypress ('i', ctrl | shift); break;
        case projectHistory:  set ("Time machine...", "File", "Browse and restore automatic project snapshots"); info.addDefaultKeypress ('h', ctrl); break;
        case quit:            set ("Quit", "File", "Quit Harmony"); info.addDefaultKeypress (juce::KeyPress::F4Key, alt); break;

        case undo:            set ("Undo", "Edit", "Undo the last change"); info.addDefaultKeypress ('z', ctrl); info.setActive (ctx.project.getUndo().canUndo()); break;
        case redo:            set ("Redo", "Edit", "Redo"); info.addDefaultKeypress ('y', ctrl); info.addDefaultKeypress ('z', ctrl | shift); info.setActive (ctx.project.getUndo().canRedo()); break;
        case cut:             set ("Cut", "Edit", "Cut the selection"); info.addDefaultKeypress ('x', ctrl); break;
        case copy:            set ("Copy", "Edit", "Copy the selection"); info.addDefaultKeypress ('c', ctrl); break;
        case paste:           set ("Paste", "Edit", "Paste at the mouse or playhead"); info.addDefaultKeypress ('v', ctrl); break;
        case deleteSelection: set ("Delete", "Edit", "Delete the selection"); info.addDefaultKeypress (K::deleteKey, 0); info.addDefaultKeypress (K::backspaceKey, 0); break;
        case selectAll:       set ("Select all", "Edit", "Select everything in the active editor"); info.addDefaultKeypress ('a', ctrl); break;
        case duplicate:       set ("Duplicate", "Edit", "Duplicate the selection after itself"); info.addDefaultKeypress ('b', ctrl); info.addDefaultKeypress ('d', ctrl); break;
        case quantize:        set ("Quantize", "Edit", "Snap note starts to the grid"); info.addDefaultKeypress ('q', ctrl); break;
        case humanize:        set ("Humanize", "Edit", "Add subtle timing and velocity variation"); info.addDefaultKeypress ('h', alt); break;
        case toggleMuteSelection: set ("Mute selection", "Edit", "Mute or unmute selected notes or clips"); info.addDefaultKeypress ('m', shift); break;

        case playPause:       set ("Play / pause", "Transport", "Start or pause playback"); info.addDefaultKeypress (K::spaceKey, 0); break;
        case stop:            set ("Stop", "Transport", "Stop and return to the start position"); info.addDefaultKeypress (K::spaceKey, ctrl); break;
        case record:          set ("Record", "Transport", "Arm MIDI recording into the current pattern"); info.addDefaultKeypress ('r', 0); break;
        case songMode:        set ("Toggle pattern / song mode", "Transport", "Switch between looping a pattern and playing the playlist"); info.addDefaultKeypress ('l', 0); break;
        case metronome:       set ("Metronome", "Transport", "Toggle the metronome click"); info.addDefaultKeypress ('m', ctrl); break;
        case returnToStart:   set ("Return to start", "Transport", "Move the playhead to the beginning"); info.addDefaultKeypress (K::homeKey, 0); break;
        case panic:           set ("Panic (all notes off)", "Transport", "Silence stuck notes on every instrument"); info.addDefaultKeypress ('.', ctrl); break;
        case tapTempo:        set ("Tap tempo", "Transport", "Tap repeatedly to set the tempo"); info.addDefaultKeypress ('t', ctrl); break;

        case showPlaylist:    set ("Focus playlist", "View", "Focus the playlist"); info.addDefaultKeypress (K::F5Key, 0); break;
        case showChannelRack: set ("Channel rack", "View", "Show the channel rack"); info.addDefaultKeypress (K::F6Key, 0); break;
        case showPianoRoll:   set ("Piano roll", "View", "Show the piano roll"); info.addDefaultKeypress (K::F7Key, 0); break;
        case toggleBrowser:   set ("Toggle browser", "View", "Show or hide the browser"); info.addDefaultKeypress (K::F8Key, 0); break;
        case showMixer:       set ("Mixer", "View", "Show the mixer"); info.addDefaultKeypress (K::F9Key, 0); break;
        case commandPalette:  set ("Command palette", "View", "Search and run any command"); info.addDefaultKeypress ('k', ctrl); info.addDefaultKeypress ('p', ctrl | shift); break;
        case fullscreen:      set ("Full screen", "View", "Toggle full screen"); info.addDefaultKeypress (K::F11Key, 0); break;
        case maximiseEditor:  set ("Maximise editor", "View", "Give the piano roll, channel rack or mixer the whole window"); info.addDefaultKeypress (K::F12Key, 0); info.setTicked (editorMaximised); break;

        case addInstrument:   set ("Add instrument plugin...", "Add", "Add a VST, VST3 or LV2 instrument channel"); info.addDefaultKeypress ('i', ctrl); break;
        case addSoundFont:    set ("Add SoundFont...", "Add", "Add a channel that plays an .sf2 SoundFont"); break;
        case addSampler:      set ("Add sampler...", "Add", "Add a channel that plays an audio sample"); break;
        case newPattern:      set ("New pattern", "Add", "Create an empty pattern"); info.addDefaultKeypress (K::F4Key, 0); break;
        case clonePattern:    set ("Clone pattern", "Add", "Duplicate the current pattern"); info.addDefaultKeypress (K::F4Key, shift); break;
        case nextPattern:     set ("Next pattern", "Add", "Select the next pattern"); info.addDefaultKeypress (K::numberPadAdd, 0); info.addDefaultKeypress (K::rightKey, ctrl | alt); break;
        case previousPattern: set ("Previous pattern", "Add", "Select the previous pattern"); info.addDefaultKeypress (K::numberPadSubtract, 0); info.addDefaultKeypress (K::leftKey, ctrl | alt); break;

        case chordGenerator:  set ("Chord generator...", "Tools", "Generate chord progressions in key"); info.addDefaultKeypress ('g', ctrl); break;
        case drumGenerator:   set ("Drum generator...", "Tools", "Generate drum grooves for your drum channels"); info.addDefaultKeypress ('g', ctrl | shift); break;
        case scanPlugins:     set ("Scan for plugins", "Tools", "Find VST, VST3 and LV2 plugins"); break;
        case audioSettings:   set ("Audio & MIDI settings...", "Tools", "Choose audio and MIDI devices"); info.addDefaultKeypress (K::F10Key, 0); break;
        case preferences:     set ("Settings...", "Tools", "Interface, editing, audio, plugins and updates"); info.addDefaultKeypress (',', ctrl); break;

        case toolDraw:        set ("Draw tool", "Tools", "Draw notes and clips"); break;
        case toolSelect:      set ("Select tool", "Tools", "Select with a rectangle"); break;
        case toolSlice:       set ("Slice tool", "Tools", "Split notes and clips"); break;
        case toolMute:        set ("Mute tool", "Tools", "Mute notes and clips"); break;

        case shortcuts:       set ("Keyboard shortcuts", "Help", "Show every shortcut"); info.addDefaultKeypress (K::F1Key, 0); break;
        case checkUpdates:    set ("Check for updates", "Help", "Look for a newer version of Harmony"); break;
        case about:           set ("About Harmony", "Help", "Version and credits"); break;
        case website:         set ("Open website", "Help", "harmony.yannosay.com"); break;
        default: break;
    }
}

bool MainComponent::perform (const InvocationInfo& info)
{
    using namespace cmd;
    auto& project = ctx.project;

    switch (info.commandID)
    {
        case newProject:      confirmDiscard ([this] { createNewProject(); }); break;
        case openProject:     confirmDiscard ([this] { showOpenDialog(); }); break;
        case saveProject:     save(); break;
        case saveProjectAs:   saveAs(); break;
        case exportMidi:      showMidiExportDialog(); break;
        case importFile:      showImportDialog(); break;
        case maximiseEditor:  setEditorMaximised (! editorMaximised); break;
        case exportAudio:     showPanel (createRenderPanel (ctx, [this] (engine::RenderSettings s, bool m) { startRender (std::move (s), m); }), "Render"); break;
        case projectHistory:
            showPanel (createHistoryPanel (ctx, [this] (const juce::File& f)
            {
                confirmDiscard ([this, f]
                {
                    Overlay::closeAll (*this);
                    if (ctx.project.loadFrom (f).wasOk())
                    {
                        ctx.project.setFile ({});
                        showToast ("Opened snapshot from " + f.getLastModificationTime().formatted ("%H:%M:%S") + ". Save it to keep it.");
                    }
                });
            }), "Time Machine");
            break;
        case quit:            juce::JUCEApplication::getInstance()->systemRequestedQuit(); break;

        case undo:            project.getUndo().undo(); break;
        case redo:            project.getUndo().redo(); break;
        case cut:             selectionEditor()->editCut(); break;
        case copy:            selectionEditor()->editCopy(); break;
        case paste:           activeEditor()->editPaste(); break;
        case deleteSelection: selectionEditor()->editDelete(); break;
        case selectAll:       activeEditor()->editSelectAll(); break;
        case duplicate:       activeEditor()->editDuplicate(); break;
        case quantize:        pianoRoll.editQuantize(); break;
        case humanize:        pianoRoll.editHumanize(); break;
        case toggleMuteSelection: selectionEditor()->editToggleMute(); break;

        case playPause:
            if (ctx.engine.isPlaying())
                ctx.engine.stop (prefs::returnToStartOnStop.get (ctx.settings));
            else
                ctx.engine.play();
            break;
        case stop:            ctx.engine.stop (prefs::returnToStartOnStop.get (ctx.settings)); break;
        case record:          ctx.engine.setRecording (! ctx.engine.isRecording()); showToast (ctx.engine.isRecording() ? "MIDI recording armed: play to record into the current pattern" : "Recording disarmed"); break;
        case songMode:        ctx.engine.setSongMode (! ctx.engine.isSongMode()); break;
        case metronome:       ctx.engine.setMetronomeEnabled (! ctx.engine.isMetronomeEnabled()); break;
        case returnToStart:   ctx.engine.setPositionTicks (0.0); break;
        case panic:           ctx.engine.panic(); showToast ("All notes off"); break;
        case tapTempo:        handleTapTempo(); break;

        case showPlaylist:    playlist.grabKeyboardFocus(); break;
        case showChannelRack: showDock (Dock::rack); break;
        case showPianoRoll:   showDock (Dock::pianoRoll); break;
        case showMixer:       showDock (Dock::mixer); break;
        case toggleBrowser:   browserVisible = ! browserVisible; resized(); break;
        case commandPalette:
        {
            auto palette = std::make_unique<CommandPalette> (ctx.commands);
            palette->onDone = [this] { Overlay::closeAll (*this); };
            showPanel (std::move (palette), "Command Palette");
            break;
        }
        case fullscreen:
            if (auto* peer = getPeer())
                peer->setFullScreen (! peer->isFullScreen());
            break;

        case addInstrument:   rack.showAddMenu (nullptr); break;
        case addSoundFont:    chooseFileForChannel (kind::soundfont); break;
        case addSampler:      chooseFileForChannel (kind::sampler); break;
        case newPattern:      project.beginTransaction ("New pattern"); project.setCurrentPattern (project.addPattern()[id::uid]); break;
        case clonePattern:    project.beginTransaction ("Clone pattern"); project.setCurrentPattern (project.clonePattern (project.currentPattern()[id::uid])[id::uid]); break;
        case nextPattern:     openPatternOffset (1); break;
        case previousPattern: openPatternOffset (-1); break;

        case chordGenerator:  showDock (Dock::pianoRoll); pianoRoll.showChordGenerator(); break;
        case drumGenerator:   rack.showDrumGenerator(); break;
        case scanPlugins:     startPluginScan(); break;
        case audioSettings:   showSettings (2); break;
        case preferences:     showSettings (0); break;

        case toolDraw:        activeEditor()->setTool (Tool::draw); break;
        case toolSelect:      activeEditor()->setTool (Tool::select); break;
        case toolSlice:       activeEditor()->setTool (Tool::slice); break;
        case toolMute:        activeEditor()->setTool (Tool::mute); break;

        case shortcuts:       showPanel (createShortcutsPanel (ctx.commands), "Keyboard Shortcuts"); break;
        case checkUpdates:
            showToast ("Checking for updates...");
            ctx.updater.onStateChanged = [this, previous = ctx.updater.onStateChanged]
            {
                if (ctx.updater.getState() == update::Updater::State::upToDate)
                    showToast ("You are on the latest version (" + update::currentVersion().display() + ")");
                if (previous)
                    previous();
            };
            ctx.updater.check (true);
            break;
        case about:           showPanel (createAboutPanel(), "About"); break;
        case website:         juce::URL ("https://harmony.yannosay.com").launchInDefaultBrowser(); break;
        default:              return false;
    }

    return true;
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit", "Add", "View", "Tools", "Help" };
}

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    using namespace cmd;
    juce::PopupMenu m;
    auto* cm = &ctx.commands;

    switch (index)
    {
        case 0:
        {
            m.addCommandItem (cm, newProject);
            m.addCommandItem (cm, openProject);

            juce::PopupMenu recent;
            auto files = juce::StringArray::fromLines (ctx.settings.getValue ("recentProjects"));
            files.removeEmptyStrings();
            for (auto& f : files)
                recent.addItem (juce::File (f).getFileNameWithoutExtension(), juce::File (f).existsAsFile(), false, [this, f] { openProjectFile (juce::File (f)); });
            m.addSubMenu ("Open recent", recent, ! files.isEmpty());

            m.addSeparator();
            m.addCommandItem (cm, saveProject);
            m.addCommandItem (cm, saveProjectAs);
            m.addSeparator();
            m.addCommandItem (cm, importFile);
            m.addCommandItem (cm, exportAudio);
            m.addCommandItem (cm, exportMidi);
            m.addCommandItem (cm, projectHistory);
            m.addSeparator();
            m.addCommandItem (cm, quit);
            break;
        }
        case 1:
            for (auto c : { undo, redo })
                m.addCommandItem (cm, c);
            m.addSeparator();
            for (auto c : { cut, copy, paste, deleteSelection, selectAll, duplicate })
                m.addCommandItem (cm, c);
            m.addSeparator();
            for (auto c : { quantize, humanize, toggleMuteSelection })
                m.addCommandItem (cm, c);
            break;
        case 2:
            for (auto c : { addInstrument, addSoundFont, addSampler })
                m.addCommandItem (cm, c);
            m.addSeparator();
            for (auto c : { newPattern, clonePattern, nextPattern, previousPattern })
                m.addCommandItem (cm, c);
            break;
        case 3:
            for (auto c : { showPlaylist, showChannelRack, showPianoRoll, showMixer, toggleBrowser })
                m.addCommandItem (cm, c);
            m.addSeparator();
            m.addCommandItem (cm, commandPalette);
            m.addCommandItem (cm, maximiseEditor);
            m.addCommandItem (cm, fullscreen);
            break;
        case 4:
            for (auto c : { chordGenerator, drumGenerator })
                m.addCommandItem (cm, c);
            m.addSeparator();
            for (auto c : { playPause, stop, record, songMode, metronome, tapTempo, panic })
                m.addCommandItem (cm, c);
            m.addSeparator();
            for (auto c : { scanPlugins, audioSettings, preferences })
                m.addCommandItem (cm, c);
            break;
        case 5:
            for (auto c : { shortcuts, checkUpdates, website, about })
                m.addCommandItem (cm, c);
            break;
        default:
            break;
    }
    return m;
}

void MainComponent::confirmDiscard (std::function<void()> onProceed)
{
    if (! ctx.project.isDirty())
    {
        onProceed();
        return;
    }

    auto options = juce::MessageBoxOptions::makeOptionsYesNoCancel (juce::MessageBoxIconType::QuestionIcon, "Save changes?",
                                                                    "Do you want to save the changes to \"" + ctx.project.getDisplayName() + "\"?",
                                                                    "Save", "Don't save", "Cancel", this);
    juce::AlertWindow::showAsync (options, [this, onProceed] (int result)
    {
        if (result == 1)
            save ([onProceed] (bool saved) { if (saved) onProceed(); });
        else if (result == 2)
            onProceed();
    });
}

void MainComponent::requestQuit (std::function<void()> onQuit)
{
    confirmDiscard ([this, onQuit]
    {
        if (ctx.updater.getState() == update::Updater::State::ready && prefs::autoUpdate.get (ctx.settings))
        {
            ctx.updater.installAndQuit (false);
            return;
        }
        onQuit();
    });
}

void MainComponent::createNewProject()
{
    auto& project = ctx.project;
    project.resetToDefault();
    project.getState().setProperty (id::bpm, prefs::defaultTempo.get (ctx.settings), nullptr);

    auto tracks = project.tracksTree();
    const int wanted = prefs::defaultTrackCount.get (ctx.settings);
    while (tracks.getNumChildren() > wanted)
        tracks.removeChild (tracks.getNumChildren() - 1, nullptr);
    while (tracks.getNumChildren() < wanted)
    {
        juce::ValueTree track (id::TRACK);
        track.setProperty (id::uid, project.allocateUid(), nullptr);
        track.setProperty (id::name, "Track " + juce::String (tracks.getNumChildren() + 1), nullptr);
        track.setProperty (id::mute, false, nullptr);
        tracks.appendChild (track, nullptr);
    }

    project.getUndo().clearUndoHistory();
    project.markClean();
    showToast ("New project");
}

void MainComponent::showOpenDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Open project", prefs::projectFolder (ctx.settings), "*.hmy;*.flp");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult().existsAsFile())
            openProjectFile (fc.getResult());
    });
}

void MainComponent::openProjectFile (const juce::File& file)
{
    if (file.hasFileExtension ("flp"))
    {
        importFlpProject (file);
        return;
    }

    const auto result = ctx.project.loadFrom (file);
    if (result.failed())
    {
        showToast (result.getErrorMessage(), true);
        return;
    }

    addRecent (file);
    showToast ("Opened " + file.getFileNameWithoutExtension());
}

void MainComponent::addRecent (const juce::File& file)
{
    auto files = juce::StringArray::fromLines (ctx.settings.getValue ("recentProjects"));
    files.removeEmptyStrings();
    files.removeString (file.getFullPathName());
    files.insert (0, file.getFullPathName());
    while (files.size() > 12)
        files.remove (files.size() - 1);
    ctx.settings.setValue ("recentProjects", files.joinIntoString ("\n"));
    browser.refresh();
}

void MainComponent::save (std::function<void (bool)> done)
{
    if (! ctx.project.getFile().existsAsFile())
    {
        saveAs (std::move (done));
        return;
    }

    const auto result = ctx.project.saveTo (ctx.project.getFile());
    if (result.failed())
        showToast (result.getErrorMessage(), true);
    else
    {
        ctx.history.snapshot ("Saved");
        showToast ("Saved " + ctx.project.getDisplayName());
    }

    if (done)
        done (result.wasOk());
}

void MainComponent::saveAs (std::function<void (bool)> done)
{
    const auto folder = prefs::projectFolder (ctx.settings);
    folder.createDirectory();
    chooser = std::make_unique<juce::FileChooser> ("Save project", folder.getChildFile (ctx.project.getDisplayName() + ".hmy"), "*.hmy");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this, done] (const juce::FileChooser& fc)
    {
        auto file = fc.getResult();
        if (file == juce::File())
        {
            if (done)
                done (false);
            return;
        }

        file = file.withFileExtension (".hmy");
        const auto result = ctx.project.saveTo (file);
        if (result.failed())
            showToast (result.getErrorMessage(), true);
        else
        {
            addRecent (file);
            showToast ("Saved " + file.getFileName());
        }

        if (done)
            done (result.wasOk());
    });
}

void MainComponent::startRender (engine::RenderSettings settings, bool minimise)
{
    if (renderer != nullptr)
    {
        showToast ("A render is already running", true);
        return;
    }

    Overlay::closeAll (*this);
    ctx.sync.captureStates();
    const auto output = settings.output;

    renderer = std::make_unique<engine::Renderer> (ctx.engine, std::move (settings), [this, output] (juce::Result result)
    {
        juce::MessageManager::callAsync ([this] { renderer.reset(); statusBar->setProgress (-1.0f, {}); });

        if (auto* window = dynamic_cast<juce::DocumentWindow*> (getTopLevelComponent()))
            if (window->isMinimised())
                window->setMinimised (false);

        if (result.wasOk())
        {
            showToast ("Rendered " + output.getFileName());
            output.revealToUser();
        }
        else
        {
            showToast (result.getErrorMessage(), true);
        }
    });

    renderer->start();

    if (minimise)
        if (auto* window = dynamic_cast<juce::DocumentWindow*> (getTopLevelComponent()))
            window->setMinimised (true);
}

void MainComponent::startPluginScan()
{
    if (ctx.plugins.isScanning())
    {
        showToast ("A plugin scan is already running");
        return;
    }

    ctx.settings.setValue ("scannedOnce", true);
    showToast ("Scanning for plugins. Each plugin is checked in a separate process, so a crashing plugin cannot take Harmony down.");

    ctx.plugins.startScan ([this] (float progress, juce::String current)
    {
        statusBar->setProgress (progress, "Scanning  " + current);
    },
    [this] (int added, juce::StringArray failed)
    {
        statusBar->setProgress (-1.0f, {});
        showToast ("Plugin scan finished: " + juce::String (added) + " new, " + juce::String (ctx.plugins.getKnownPlugins().getNumTypes()) + " total"
                   + (failed.isEmpty() ? juce::String() : ", " + juce::String (failed.size()) + " skipped"));
        browser.refresh();
    });
}

void MainComponent::addInstrumentChannel (const juce::String& identifier)
{
    auto desc = ctx.plugins.findByIdentifier (identifier);
    if (! desc.has_value())
    {
        showToast ("That plugin is no longer available. Rescan your plugins.", true);
        return;
    }

    if (! engine::PluginManager::isUsableInstrument (*desc))
    {
        if (engine::PluginManager::isUsableEffect (*desc))
            addEffectToInsert (effectTargetInsert(), identifier, -1);
        else
            showToast (desc->name + " cannot be used as an instrument", true);
        return;
    }

    ctx.project.beginTransaction ("Add " + desc->name);
    auto channel = ctx.project.addChannel (kind::plugin, desc->name);
    channel.setProperty (id::pluginDesc, engine::PluginManager::toXmlString (*desc), nullptr);
    ctx.project.setSelectedChannel (channel[id::uid]);
    showDock (Dock::rack);
    showToast ("Loading " + desc->name + "...");
}

void MainComponent::addFileAsChannel (const juce::File& file)
{
    if (ui::Browser::isProject (file))
    {
        confirmDiscard ([this, file] { openProjectFile (file); });
        return;
    }

    if (midi::isMidiFile (file))
    {
        importMidiAsPattern (file);
        return;
    }

    if (ui::Browser::isSoundFont (file))
    {
        ctx.project.beginTransaction ("Add SoundFont");
        auto channel = ctx.project.addChannel (kind::soundfont, file.getFileNameWithoutExtension());
        channel.setProperty (id::sfPath, file.getFullPathName(), nullptr);
        channel.setProperty (id::sfPreset, 0, nullptr);
        ctx.project.setSelectedChannel (channel[id::uid]);
        return;
    }

    if (ctx.engine.getFormats().findFormatForFileExtension (file.getFileExtension()) != nullptr)
    {
        ctx.project.beginTransaction ("Add sampler");
        auto channel = ctx.project.addChannel (kind::sampler, file.getFileNameWithoutExtension());
        channel.setProperty (id::samplePath, file.getFullPathName(), nullptr);
        ctx.project.setSelectedChannel (channel[id::uid]);
        return;
    }

    showToast ("Harmony cannot open " + file.getFileName(), true);
}

void MainComponent::importFlpProject (const juce::File& file)
{
    auto options = flp::defaultOptions();
    options.findInstrumentXml = [this] (const juce::String& name) -> juce::String
    {
        for (auto& d : ctx.plugins.getPlugins (true))
            if (d.name.equalsIgnoreCase (name))
                return engine::PluginManager::toXmlString (d);
        return {};
    };
    options.findEffectXml = [this] (const juce::String& name) -> juce::String
    {
        if (const auto builtin = flp::builtinEffectFor (name); builtin.isNotEmpty())
            if (auto* info = engine::builtin::find (builtin))
                return engine::PluginManager::toXmlString (engine::builtin::describe (*info));

        for (auto& d : ctx.plugins.getPlugins (false))
            if (d.name.equalsIgnoreCase (name))
                return engine::PluginManager::toXmlString (d);
        return {};
    };

    flp::Report report;
    juce::String error;
    const auto state = flp::convert (file, options, report, error);
    if (! state.isValid())
    {
        showToast ("Could not import " + file.getFileName() + ": " + error, true);
        return;
    }

    ctx.project.replaceState (state);
    ctx.project.setFile ({});
    ctx.engine.setSongMode (ctx.project.songLength() > 0);
    addRecent (file);
    updateTitle();
    showToast (report.summary());
    showPanel (createTextReportPanel (report.details() + "\n\nSave the project to keep it as a Harmony project."), "Imported " + file.getFileNameWithoutExtension());
}

void MainComponent::importMidiAsPattern (const juce::File& file)
{
    auto midiFile = midi::readFile (file);
    auto tracks = midiFile ? midi::importTracks (*midiFile) : std::vector<midi::ImportedTrack>();
    if (tracks.empty())
    {
        showToast (file.getFileName() + " contains no notes", true);
        return;
    }

    auto& project = ctx.project;
    project.beginTransaction ("Import MIDI");
    auto pattern = project.addPattern (file.getFileNameWithoutExtension());
    int created = 0;

    for (size_t t = 0; t < tracks.size(); ++t)
    {
        int channelUid = 0;
        if (tracks.size() == 1 && project.findChannel (project.selectedChannel()).isValid())
            channelUid = project.selectedChannel();
        else
        {
            const auto name = tracks[t].name.isNotEmpty() ? tracks[t].name : file.getFileNameWithoutExtension() + " " + juce::String ((int) t + 1);
            channelUid = project.addChannel (kind::sampler, name)[id::uid];
            ++created;
        }

        for (auto n : tracks[t].notes)
        {
            n.channel = channelUid;
            project.addNote (pattern, n);
        }
    }

    project.setCurrentPattern (pattern[id::uid]);
    showToast ("Imported " + file.getFileName() + " as pattern \"" + pattern[id::name].toString() + "\""
               + (created > 0 ? ". " + juce::String (created) + " new channels are waiting for an instrument." : juce::String()));
    if (ctx.openPianoRoll)
        ctx.openPianoRoll (project.selectedChannel());
}

void MainComponent::showImportDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Import FL Studio project or MIDI file", prefs::projectFolder (ctx.settings), "*.flp;*.mid;*.midi");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        const auto file = fc.getResult();
        if (! file.existsAsFile())
            return;

        if (file.hasFileExtension ("flp"))
            confirmDiscard ([this, file] { importFlpProject (file); });
        else
            importMidiAsPattern (file);
    });
}

void MainComponent::showMidiExportDialog()
{
    const bool song = ctx.project.songLength() > 0 && (ctx.engine.isSongMode() || currentDock != Dock::pianoRoll);
    const auto name = ctx.project.getDisplayName() + (song ? juce::String() : " - " + ctx.project.currentPattern()[id::name].toString());
    chooser = std::make_unique<juce::FileChooser> (song ? "Export song as MIDI" : "Export pattern as MIDI",
                                                   prefs::projectFolder (ctx.settings).getChildFile (juce::File::createLegalFileName (name) + ".mid"), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this, song] (const juce::FileChooser& fc)
    {
        if (fc.getResult() == juce::File())
            return;

        const auto target = fc.getResult().withFileExtension (".mid");
        const auto file = song ? midi::exportSong (ctx.project) : midi::exportPattern (ctx.project, ctx.project.currentPattern());
        if (midi::writeFile (file, target))
            showToast ("Exported " + juce::String (song ? "the song" : "the pattern") + " to " + target.getFileName());
        else
            showToast ("Could not write " + target.getFullPathName(), true);
    });
}

void MainComponent::importAudio (const juce::File& file, int trackIndex, juce::int64 tick)
{
    std::unique_ptr<juce::AudioFormatReader> reader (ctx.engine.getFormats().createReaderFor (file));
    if (reader == nullptr)
    {
        showToast ("Cannot read " + file.getFileName(), true);
        return;
    }

    const double seconds = (double) reader->lengthInSamples / reader->sampleRate;
    const auto length = (juce::int64) std::ceil (seconds * ctx.project.getBpm() / 60.0 * ppq);

    ctx.project.beginTransaction ("Import audio");
    auto channel = ctx.project.addChannel (kind::audio, file.getFileNameWithoutExtension());
    channel.setProperty (id::samplePath, file.getFullPathName(), nullptr);
    channel.setProperty (id::oneShot, false, nullptr);

    auto track = ctx.project.tracksTree().getChild (juce::jlimit (0, ctx.project.tracksTree().getNumChildren() - 1, trackIndex));
    ctx.project.addClip (track, kind::audio, channel[id::uid], tick, juce::jmax ((juce::int64) 1, length));
    ctx.engine.setSongMode (true);
}

void MainComponent::chooseFileForChannel (const juce::String& kindName)
{
    const bool sf = kindName == kind::soundfont;
    const auto soundFonts = prefs::soundFontFolder (ctx.settings);
    chooser = std::make_unique<juce::FileChooser> (sf ? "Choose a SoundFont" : "Choose a sample",
                                                   sf && soundFonts.isDirectory() ? soundFonts : juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   sf ? juce::String ("*.sf2") : ctx.engine.getFormats().getWildcardForAllFormats());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult().existsAsFile())
            addFileAsChannel (fc.getResult());
    });
}

void MainComponent::buildDemoContent()
{
    auto& project = ctx.project;
    project.resetToDefault();
    project.setBpm (140.0);
    project.getState().setProperty (id::scaleRoot, 9, nullptr);
    project.getState().setProperty (id::scaleType, "Minor", nullptr);

    const char* names[] = { "Kick", "Snare", "Clap", "Closed Hat", "Open Hat", "Keys", "Bass", "Lead" };
    std::vector<int> uids;
    for (auto* n : names)
    {
        auto ch = project.addChannel (kind::sampler, n);
        uids.push_back (ch[id::uid]);
    }

    auto drums = project.currentPattern();
    drums.setProperty (id::name, "Drums", nullptr);
    DrumGenSettings ds;
    ds.style = "Trap";
    ds.bars = 1;
    ds.seed = 5;
    for (auto& hit : generateDrums (ds, ticksPerStep))
    {
        const int roleIndex = (int) hit.role;
        if (roleIndex > 4)
            continue;

        NoteData n;
        n.channel = uids[(size_t) roleIndex];
        n.start = hit.start;
        n.length = ticksPerStep;
        n.velocity = hit.velocity;
        project.addNote (drums, n);
    }

    auto chords = project.addPattern ("Chords");
    ChordGenSettings cs;
    cs.root = 9;
    cs.progression = 3;
    cs.chordSize = 4;
    cs.rhythm = ChordRhythm::sustain;
    cs.channel = uids[5];
    for (auto& n : generateChords (cs, project.ticksPerBar(), project.ticksPerBeat()).notes)
    {
        auto note = n;
        if (note.key < 45)
            note.channel = uids[6];
        project.addNote (chords, note);
    }

    auto lead = project.addPattern ("Lead");
    const int melody[] = { 69, 72, 76, 74, 72, 71, 69, 67, 69, 72, 79, 77, 76, 74, 72, 74 };
    for (int i = 0; i < 16; ++i)
    {
        NoteData n;
        n.channel = uids[7];
        n.key = melody[i];
        n.start = (juce::int64) i * ppq / 2 + (i % 4 == 3 ? ppq / 4 : 0);
        n.length = ppq / 2 - 40;
        n.velocity = 0.55f + 0.4f * (float) ((i * 7) % 5) / 4.0f;
        project.addNote (lead, n);
    }

    auto tracks = project.tracksTree();
    const auto bar = project.ticksPerBar();
    tracks.getChild (0).setProperty (id::name, "Drums", nullptr);
    tracks.getChild (1).setProperty (id::name, "Chords", nullptr);
    tracks.getChild (2).setProperty (id::name, "Lead", nullptr);
    for (int b = 0; b < 16; ++b)
    {
        if (b >= 2) project.addClip (tracks.getChild (0), kind::pattern, drums[id::uid], b * bar, bar);
        if (b % 4 == 0) project.addClip (tracks.getChild (1), kind::pattern, chords[id::uid], b * bar, bar * 4);
        if (b >= 8 && b % 2 == 0) project.addClip (tracks.getChild (2), kind::pattern, lead[id::uid], b * bar, bar * 2);
    }

    const char* insertNames[] = { "Kick", "Snare", "Clap", "Hats", "Open Hat", "Keys", "Bass", "Lead" };
    for (int i = 0; i < 8; ++i)
        project.findInsert (i + 1).setProperty (id::name, insertNames[i], nullptr);

    auto builtin = [&project] (int insert, int slot, const char* effect)
    {
        auto desc = engine::builtin::describe (*engine::builtin::find (effect));
        return project.addSlot (insert, slot, engine::PluginManager::toXmlString (desc));
    };
    builtin (0, 0, "eq");
    builtin (0, 1, "compressor");
    builtin (0, 2, "limiter");
    builtin (6, 0, "reverb");
    builtin (8, 0, "delay");
    builtin (8, 1, "filter");

    tracks.getChild (3).setProperty (id::name, "Keys fade", nullptr);
    auto fade = automation::addClip (project, tracks.getChild (3), { automation::Kind::insertVolume, 6, -1 }, 0, bar * 8, 0.0f, "Keys volume");
    automation::writePoints (fade, { { 0.0, 0.05f, 0.6f }, { (double) bar * 4, 0.62f, 0.0f }, { (double) bar * 8, 0.66f, 0.0f } }, nullptr);

    project.setCurrentPattern (chords[id::uid]);
    project.setSelectedChannel (uids[5]);
    project.getUndo().clearUndoHistory();
    project.markClean();
}

void MainComponent::saveScreenshots (const juce::File& directory)
{
    directory.createDirectory();
    stopTimer();
    ctx.plugins.getKnownPlugins().clear();
    browser.refresh();
    statusBar->setInfo ("48.0 kHz   512 smp   " + update::currentVersion().display());
    setSize (1720, 1040);
    resized();

    auto capture = [this, &directory] (const juce::String& name)
    {
        auto image = createComponentSnapshot (getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto file = directory.getChildFile (name + ".png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        png.writeImageToStream (image, out);
    };

    showDock (Dock::rack);
    playlist.scrollToTick (0.0, false);
    capture ("harmony-playlist");

    pianoRoll.setChannel (ctx.project.selectedChannel());
    showDock (Dock::pianoRoll);
    pianoRoll.scrollToY ((127.0 - 82.0) * pianoRoll.getRowHeight(), false);
    capture ("harmony-pianoroll");
    setEditorMaximised (true);
    capture ("harmony-pianoroll-maximised");
    setEditorMaximised (false);

    showDock (Dock::mixer);
    capture ("harmony-mixer");

    auto settleOverlays = [this]
    {
        for (int i = 0; i < getNumChildComponents(); ++i)
            if (auto* overlay = dynamic_cast<Overlay*> (getChildComponent (i)))
            {
                juce::Desktop::getInstance().getAnimator().cancelAnimation (overlay, false);
                overlay->setAlpha (1.0f);
            }
    };

    showDock (Dock::pianoRoll);
    pianoRoll.showChordGenerator();
    settleOverlays();
    capture ("harmony-chords");
    Overlay::closeAll (*this);

    showDock (Dock::rack);
    showPanel (std::make_unique<CommandPalette> (ctx.commands), "Command Palette");
    settleOverlays();
    capture ("harmony-palette");
    Overlay::closeAll (*this);

    showSettings (1);
    settleOverlays();
    capture ("harmony-settings");
    Overlay::closeAll (*this);

    showDock (Dock::pianoRoll);
    pianoRoll.showFollowGenerator();
    settleOverlays();
    capture ("harmony-follow");
    Overlay::closeAll (*this);

    if (auto eq = engine::builtin::create ("eq"))
    {
        auto& effect = dynamic_cast<engine::builtin::Effect&> (*eq);
        auto set = [&effect] (const char* name, float value)
        {
            if (auto* p = effect.state.getParameter (name))
                p->setValueNotifyingHost (p->convertTo0to1 (value));
        };
        set ("lowCut", 35.0f);
        set ("lowGain", 3.0f);
        set ("midFreq", 420.0f);
        set ("midGain", -4.0f);
        set ("highGain", 2.5f);
        eq->prepareToPlay (48000.0, 512);

        std::unique_ptr<juce::AudioProcessorEditor> editor (eq->createEditorAndMakeActive());
        editor->setVisible (true);
        auto image = editor->createComponentSnapshot (editor->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png;
        auto file = directory.getChildFile ("harmony-effects.png");
        file.deleteFile();
        juce::FileOutputStream out (file);
        png.writeImageToStream (image, out);
        editor.reset();
    }
}
}
