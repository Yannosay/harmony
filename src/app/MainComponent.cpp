#include "app/MainComponent.h"
#include "ui/Dialogs.h"
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
    explicit DockBar (MainComponent& o) : owner (o)
    {
        for (auto* b : { &rackButton, &pianoButton, &mixerButton })
        {
            b->setClickingTogglesState (false);
            b->setColour (juce::TextButton::buttonOnColourId, colours::accent);
            addAndMakeVisible (b);
        }
        keepFocusOnEditors ({ &rackButton, &pianoButton, &mixerButton });
        rackButton.setTooltip ("Channel Rack (F6)");
        pianoButton.setTooltip ("Piano Roll (F7)");
        mixerButton.setTooltip ("Mixer (F9)");
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
            b->setBounds (r.removeFromLeft (130));
            r.removeFromLeft (6);
        }
    }

private:
    MainComponent& owner;
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

    ctx.sync.onError = [this] (const juce::String& message) { showToast (message, true); };
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
        updateTitle();
    };

    showDock (Dock::rack);
    setSize (1500, 920);
    startTimerHz (20);
}

MainComponent::~MainComponent()
{
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
}

void MainComponent::startup()
{
    ctx.plugins.extraFolders = juce::StringArray::fromLines (ctx.settings.getValue ("pluginFolders"));
    ctx.plugins.extraFolders.removeEmptyStrings();

    static const int minutes[] = { 0, 1, 3, 5, 10 };
    ctx.history.setIntervalMinutes (minutes[juce::jlimit (0, 4, ctx.settings.getIntValue ("autosaveChoice", 2))]);

    if (ctx.plugins.getKnownPlugins().getNumTypes() == 0 && ! ctx.settings.getBoolValue ("scannedOnce", false))
        juce::Timer::callAfterDelay (1200, [safe = juce::Component::SafePointer<MainComponent> (this)] { if (safe) safe->startPluginScan(); });

    if (ctx.settings.getBoolValue ("autoUpdate", true))
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
        }
    }

    updateTitle();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void MainComponent::layoutMiddle()
{
    auto r = getLocalBounds();
    r.removeFromTop (topBarHeight);
    r.removeFromBottom (statusHeight);

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

ui::EditTarget* MainComponent::activeEditor()
{
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
    c.addArray ({ newProject, openProject, saveProject, saveProjectAs, exportAudio, projectHistory, quit,
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
        case audioSettings:   set ("Audio settings...", "Tools", "Choose audio and MIDI devices"); info.addDefaultKeypress (K::F10Key, 0); break;
        case preferences:     set ("Preferences...", "Tools", "Plugin folders, autosave and updates"); info.addDefaultKeypress (',', ctrl); break;

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
        case cut:             activeEditor()->editCut(); break;
        case copy:            activeEditor()->editCopy(); break;
        case paste:           activeEditor()->editPaste(); break;
        case deleteSelection: activeEditor()->editDelete(); break;
        case selectAll:       activeEditor()->editSelectAll(); break;
        case duplicate:       activeEditor()->editDuplicate(); break;
        case quantize:        pianoRoll.editQuantize(); break;
        case humanize:        pianoRoll.editHumanize(); break;
        case toggleMuteSelection: activeEditor()->editToggleMute(); break;

        case playPause:       ctx.engine.togglePlay(); break;
        case stop:            ctx.engine.stop(); break;
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
        case audioSettings:   showPanel (createAudioSettingsPanel (ctx), "Audio & MIDI"); break;
        case preferences:     showPanel (createPreferencesPanel (ctx, [this] { startPluginScan(); }), "Preferences"); break;

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
            m.addCommandItem (cm, exportAudio);
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
        if (ctx.updater.getState() == update::Updater::State::ready && ctx.settings.getBoolValue ("autoUpdate", true))
        {
            ctx.updater.installAndQuit (false);
            return;
        }
        onQuit();
    });
}

void MainComponent::createNewProject()
{
    ctx.project.resetToDefault();
    showToast ("New project");
}

void MainComponent::showOpenDialog()
{
    chooser = std::make_unique<juce::FileChooser> ("Open project", juce::File::getSpecialLocation (juce::File::userDocumentsDirectory), "*.hmy");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles, [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult().existsAsFile())
            openProjectFile (fc.getResult());
    });
}

void MainComponent::openProjectFile (const juce::File& file)
{
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
    const auto folder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Harmony Projects");
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
    chooser = std::make_unique<juce::FileChooser> (sf ? "Choose a SoundFont" : "Choose a sample",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
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
}
}
