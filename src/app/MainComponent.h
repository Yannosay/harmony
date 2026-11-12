#pragma once

#include "app/Context.h"
#include "app/CommandIds.h"
#include "app/History.h"
#include "update/Updater.h"
#include "ui/TopBar.h"
#include "ui/Browser.h"
#include "ui/Playlist.h"
#include "ui/ChannelRack.h"
#include "ui/PianoRoll.h"
#include "ui/MixerView.h"
#include "ui/PluginWindows.h"
#include "ui/CommandPalette.h"
#include "ui/PatternBar.h"
#include "engine/Renderer.h"

namespace harmony
{
class MainComponent final : public juce::Component,
                            public juce::ApplicationCommandTarget,
                            public juce::MenuBarModel,
                            public juce::DragAndDropContainer,
                            private juce::ChangeListener,
                            private juce::Timer
{
public:
    enum class Dock { rack, pianoRoll, mixer };

    MainComponent (Context& context);
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    juce::ApplicationCommandTarget* getNextCommandTarget() override { return nullptr; }
    void getAllCommands (juce::Array<juce::CommandID>&) override;
    void getCommandInfo (juce::CommandID, juce::ApplicationCommandInfo&) override;
    bool perform (const InvocationInfo&) override;

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int index, const juce::String& name) override;
    void menuItemSelected (int, int) override {}

    void showDock (Dock dock);
    void openProjectFile (const juce::File& file);
    void confirmDiscard (std::function<void()> onProceed);
    void requestQuit (std::function<void()> onQuit);
    void showToast (const juce::String& message, bool error = false);
    void saveScreenshots (const juce::File& directory);
    void buildDemoContent();
    void startup();

    std::function<void (const juce::String&)> onTitleChanged;

private:
    class DockBar;
    class StatusBar;

    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void applySettings();
    void bindContext();
    void createAutomationClip (const automation::Target& target, const juce::String& name, float initialValue);
    void automatePluginParameter (juce::AudioProcessor& processor, int parameterIndex);
    bool addEffectToInsert (int insertIndex, const juce::String& identifier, int slotIndex);
    int effectTargetInsert() const;
    void layoutMiddle();
    void createNewProject();
    void showOpenDialog();
    void save (std::function<void (bool)> done = {});
    void saveAs (std::function<void (bool)> done = {});
    void addRecent (const juce::File& file);
    void showSettings (int page);
    void queueError (const juce::String& message);
    void importFlpProject (const juce::File& file);
    void importMidiAsPattern (const juce::File& file);
    void showImportDialog();
    void showMidiExportDialog();
    void setEditorMaximised (bool shouldMaximise);
    void showPanel (std::unique_ptr<juce::Component> panel, const juce::String& title);
    void startRender (engine::RenderSettings settings, bool minimise);
    void startPluginScan();
    void addInstrumentChannel (const juce::String& identifier);
    void addFileAsChannel (const juce::File& file);
    void importAudio (const juce::File& file, int trackIndex, juce::int64 tick);
    void chooseFileForChannel (const juce::String& kindName);
    void drainRecording();
    void handleTapTempo();
    void updateTitle();
    ui::EditTarget* activeEditor();
    ui::EditTarget* selectionEditor();
    void openPatternOffset (int delta);

    Context& ctx;
    ui::TopBar topBar;
    ui::Browser browser;
    ui::Playlist playlist;
    ui::ChannelRack rack;
    ui::PianoRoll pianoRoll;
    ui::MixerView mixer;
    std::unique_ptr<DockBar> dockBar;
    std::unique_ptr<StatusBar> statusBar;
    ui::Toasts toasts;
    ui::WindowManager windows;

    juce::StretchableLayoutManager horizontalLayout, verticalLayout;
    juce::Component editorArea, dockArea;
    std::unique_ptr<juce::StretchableLayoutResizerBar> browserResizer, dockResizer;
    Dock currentDock = Dock::rack;
    bool browserVisible = true;
    bool editorMaximised = false;

    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<engine::Renderer> renderer;
    std::vector<engine::RecordedEvent> recorded;
    std::map<int, std::pair<double, float>> heldRecording;
    juce::Array<double> taps;
    juce::String lastTitle;
    juce::StringArray pendingErrors;
};
}
