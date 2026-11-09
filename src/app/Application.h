#pragma once

#include "app/MainComponent.h"
#include "ui/Theme.h"

namespace harmony
{
class Application final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Yannosay Harmony"; }
    const juce::String getApplicationVersion() override { return HARMONY_VERSION_STRING; }
    Application();
    ~Application() override;
    bool moreThanOneInstanceAllowed() override;

    void initialise (const juce::String& commandLine) override;
    void shutdown() override;
    void systemRequestedQuit() override;
    void anotherInstanceStarted (const juce::String& commandLine) override;

    static juce::File dataDirectory();

private:
    class MainWindow;
    class HeadlessRender;

    void createCore();
    static juce::File projectArgument (const juce::StringArray& args);

    std::unique_ptr<ui::LookAndFeel> lookAndFeel;
    juce::ApplicationProperties properties;
    juce::ApplicationCommandManager commands;
    std::unique_ptr<Project> project;
    std::unique_ptr<engine::AudioEngine> engine;
    std::unique_ptr<engine::PluginManager> plugins;
    std::unique_ptr<engine::ProjectSync> sync;
    std::unique_ptr<History> history;
    std::unique_ptr<update::Updater> updater;
    std::unique_ptr<Context> context;
    std::unique_ptr<MainComponent> mainComponent;
    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<HeadlessRender> headless;
};
}
