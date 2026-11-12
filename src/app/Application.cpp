#include "app/Application.h"
#include "HarmonyAssets.h"

namespace harmony
{
class Application::MainWindow final : public juce::DocumentWindow
{
public:
    MainWindow (MainComponent& content)
        : juce::DocumentWindow ("Yannosay Harmony", ui::colours::background, juce::DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (false);
        setTitleBarHeight (34);
        setContentNonOwned (&content, true);
        setResizable (true, false);
        setResizeLimits (1100, 700, 10000, 10000);

        auto icon = juce::ImageCache::getFromMemory (HarmonyAssets::harmonyicon256_png, HarmonyAssets::harmonyicon256_pngSize);
        setIcon (icon);
        if (auto* peer = getPeer())
            peer->setIcon (icon);

        if (auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
        {
            const auto area = display->userBounds.toNearestInt();
            setBounds (area.withSizeKeepingCentre (juce::jmin (area.getWidth() - 40, 1680), juce::jmin (area.getHeight() - 40, 1020)));
        }

        content.onTitleChanged = [this] (const juce::String& title) { setName (title); };
        setVisible (true);
    }

    void closeButtonPressed() override
    {
        juce::JUCEApplication::getInstance()->systemRequestedQuit();
    }
};

class Application::HeadlessRender final : private juce::Timer
{
public:
    HeadlessRender (engine::AudioEngine& e, engine::ProjectSync& s, Project& p, engine::RenderSettings settings)
        : engine (e), sync (s), project (p), pending (std::move (settings))
    {
        startTimer (100);
    }

private:
    void timerCallback() override
    {
        if ((sync.isLoading() && ++waits < 600) || renderer != nullptr)
            return;

        stopTimer();
        pending.bpm = project.getBpm();
        renderer = std::make_unique<engine::Renderer> (engine, pending, [] (juce::Result result)
        {
            juce::JUCEApplication::getInstance()->setApplicationReturnValue (result.wasOk() ? 0 : 1);
            if (result.failed())
                std::fprintf (stderr, "%s\n", result.getErrorMessage().toRawUTF8());
            juce::JUCEApplication::quit();
        });
        renderer->start();
    }

    engine::AudioEngine& engine;
    engine::ProjectSync& sync;
    Project& project;
    engine::RenderSettings pending;
    std::unique_ptr<engine::Renderer> renderer;
    int waits = 0;
};

Application::Application() = default;
Application::~Application() = default;

bool Application::moreThanOneInstanceAllowed()
{
    const auto args = getCommandLineParameterArray();
    return args.contains ("--scan-plugin") || args.contains ("--render") || args.contains ("--screenshot");
}

juce::File Application::dataDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("Yannosay Harmony");
}

juce::File Application::projectArgument (const juce::StringArray& args)
{
    for (auto a : args)
    {
        a = a.unquoted();
        if ((a.endsWithIgnoreCase (".hmy") || a.endsWithIgnoreCase (".flp")) && juce::File::isAbsolutePath (a) && juce::File (a).existsAsFile())
            return juce::File (a);
    }
    return {};
}

void Application::createCore()
{
    juce::PropertiesFile::Options options;
    options.applicationName = "Harmony";
    options.filenameSuffix = ".settings";
    options.folderName = "Yannosay Harmony";
    options.osxLibrarySubFolder = "Application Support";
    options.storageFormat = juce::PropertiesFile::storeAsXML;
    properties.setStorageParameters (options);

    dataDirectory().createDirectory();
    project = std::make_unique<Project>();
    engine = std::make_unique<engine::AudioEngine>();
    plugins = std::make_unique<engine::PluginManager> (dataDirectory());
    sync = std::make_unique<engine::ProjectSync> (*project, *engine, *plugins);
    history = std::make_unique<History> (*project, dataDirectory(), [this] { sync->captureStates(); });
    updater = std::make_unique<update::Updater> (dataDirectory().getChildFile ("Updates"));
    project->onBeforeSave = [this] { sync->captureStates(); };

    context = std::unique_ptr<Context> (new Context { *project, *engine, *plugins, *sync, commands, *properties.getUserSettings(),
                                                      *history, *updater, {}, {}, {}, {}, {}, {}, {}, {}, {}, {}, {} });
}

void Application::initialise (const juce::String&)
{
    const auto args = getCommandLineParameterArray();

    if (args.contains ("--scan-plugin"))
    {
        setApplicationReturnValue (engine::PluginManager::runScanWorker (args));
        quit();
        return;
    }

    lookAndFeel = std::make_unique<ui::LookAndFeel>();
    juce::LookAndFeel::setDefaultLookAndFeel (lookAndFeel.get());
    createCore();

    if (const int renderIndex = args.indexOf ("--render"); renderIndex >= 0)
    {
        const juce::File source (args[renderIndex + 1].unquoted());
        const juce::File output (args[renderIndex + 2].unquoted());
        const auto loaded = project->loadFrom (source);

        if (loaded.failed() || output == juce::File())
        {
            std::fprintf (stderr, "Usage: Harmony --render <project.hmy> <output.wav|flac|ogg|aiff>\n%s\n", loaded.getErrorMessage().toRawUTF8());
            setApplicationReturnValue (2);
            quit();
            return;
        }

        engine->prepareStandalone (48000.0, 1024);
        sync->syncNow();

        engine::RenderSettings settings;
        settings.output = output;
        const auto ext = output.getFileExtension().toLowerCase();
        settings.format = ext == ".flac" ? "FLAC" : ext == ".ogg" ? "OGG Vorbis" : (ext == ".aif" || ext == ".aiff") ? "AIFF" : "WAV";
        settings.songMode = project->songLength() > 0;
        settings.endTick = settings.songMode ? project->songLength() : project->patternLength (project->currentPattern());
        headless = std::make_unique<HeadlessRender> (*engine, *sync, *project, settings);
        return;
    }

    auto deviceState = properties.getUserSettings()->getXmlValue ("audioDevice");
    const auto error = engine->openDevice (deviceState.get());

    mainComponent = std::make_unique<MainComponent> (*context);
    commands.registerAllCommandsForTarget (mainComponent.get());
    commands.setFirstCommandTarget (mainComponent.get());
    mainWindow = std::make_unique<MainWindow> (*mainComponent);
    mainWindow->addKeyListener (commands.getKeyMappings());

    if (const int shotIndex = args.indexOf ("--screenshot"); shotIndex >= 0)
    {
        const juce::File dir (args[shotIndex + 1].unquoted());
        if (auto file = projectArgument (args); file.existsAsFile())
            mainComponent->openProjectFile (file);
        else
            mainComponent->buildDemoContent();
        juce::Timer::callAfterDelay (1500, [this, dir]
        {
            mainComponent->saveScreenshots (dir);
            quit();
        });
        return;
    }

    mainComponent->startup();

    if (error.isNotEmpty())
        mainComponent->showToast ("Audio device: " + error, true);

    if (auto file = projectArgument (args); file.existsAsFile())
        mainComponent->openProjectFile (file);
}

void Application::shutdown()
{
    if (engine != nullptr && mainComponent != nullptr)
        if (auto xml = engine->getDeviceManager().createStateXml())
            properties.getUserSettings()->setValue ("audioDevice", xml.get());

    mainWindow.reset();
    mainComponent.reset();
    headless.reset();

    if (history != nullptr)
        history->markCleanExit();

    if (engine != nullptr)
        engine->closeDevice();

    context.reset();
    updater.reset();
    history.reset();
    sync.reset();
    plugins.reset();
    engine.reset();
    project.reset();
    properties.saveIfNeeded();
    juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    lookAndFeel.reset();
}

void Application::systemRequestedQuit()
{
    if (mainComponent != nullptr)
        mainComponent->requestQuit ([] { juce::JUCEApplication::quit(); });
    else
        quit();
}

void Application::anotherInstanceStarted (const juce::String& commandLine)
{
    if (mainComponent == nullptr)
        return;

    if (auto file = projectArgument (juce::StringArray::fromTokens (commandLine, true)); file.existsAsFile())
        mainComponent->confirmDiscard ([this, file] { mainComponent->openProjectFile (file); });

    if (mainWindow != nullptr)
        mainWindow->toFront (true);
}
}
