#include "engine/PluginManager.h"

namespace harmony::engine
{
namespace
{
    class OutOfProcessScanner final : public juce::KnownPluginList::CustomScanner
    {
    public:
        bool findPluginTypesFor (juce::AudioPluginFormat& format,
                                 juce::OwnedArray<juce::PluginDescription>& result,
                                 const juce::String& fileOrIdentifier) override
        {
            const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
            const juce::TemporaryFile output (".xml");

            juce::ChildProcess child;
            const juce::StringArray args { exe.getFullPathName(), "--scan-plugin", format.getName(),
                                           fileOrIdentifier, output.getFile().getFullPathName() };

            if (! exe.getFileName().containsIgnoreCase ("Harmony") || exe.getFileName().containsIgnoreCase ("Tests")
                || ! child.start (args, 0))
            {
                format.findAllTypesForFile (result, fileOrIdentifier);
                return true;
            }

            const auto deadline = juce::Time::getMillisecondCounter() + 180000u;
            while (child.isRunning())
            {
                if (shouldExit() || juce::Time::getMillisecondCounter() > deadline)
                {
                    child.kill();
                    return shouldExit();
                }
                juce::Thread::sleep (20);
            }

            if (child.getExitCode() != 0)
                return false;

            if (auto xml = juce::parseXML (output.getFile()))
                for (auto* e : xml->getChildIterator())
                {
                    auto desc = std::make_unique<juce::PluginDescription>();
                    if (desc->loadFromXml (*e))
                        result.add (desc.release());
                }

            return true;
        }
    };

    void addIfExists (juce::FileSearchPath& path, const juce::File& dir)
    {
        if (dir.isDirectory())
            path.addIfNotAlreadyThere (dir);
    }
}

class PluginManager::ScanThread final : public juce::Thread
{
public:
    ScanThread (PluginManager& o, std::function<void (float, juce::String)> progress,
                std::function<void (int, juce::StringArray)> finished)
        : juce::Thread ("Harmony plugin scan"), owner (o), onProgress (std::move (progress)), onFinished (std::move (finished))
    {
    }

    ~ScanThread() override { stopThread (10000); }

    void run() override
    {
        const int before = owner.known.getNumTypes();
        juce::StringArray failed;
        const int numFormats = owner.formats.getNumFormats();

        for (int f = 0; f < numFormats && ! threadShouldExit(); ++f)
        {
            auto* format = owner.formats.getFormat (f);
            juce::PluginDirectoryScanner directoryScanner (owner.known, *format, owner.getSearchPath (*format),
                                                           true, owner.deadMansPedal, true);
            juce::String name;

            for (;;)
            {
                if (threadShouldExit())
                    break;

                const auto next = directoryScanner.getNextPluginFileThatWillBeScanned();
                report (((float) f + directoryScanner.getProgress()) / (float) numFormats,
                        format->getName() + "  " + juce::File::createFileWithoutCheckingPath (next).getFileNameWithoutExtension());

                if (! directoryScanner.scanNextFile (true, name))
                    break;
            }

            failed.addArray (directoryScanner.getFailedFiles());
        }

        const int added = owner.known.getNumTypes() - before;
        juce::MessageManager::callAsync ([this, added, failed]
        {
            owner.saveList();
            auto done = onFinished;
            owner.scanner.reset();
            if (done)
                done (added, failed);
        });
    }

private:
    void report (float progress, const juce::String& text)
    {
        if (onProgress)
            juce::MessageManager::callAsync ([cb = onProgress, progress, text] { cb (progress, text); });
    }

    PluginManager& owner;
    std::function<void (float, juce::String)> onProgress;
    std::function<void (int, juce::StringArray)> onFinished;
};

PluginManager::PluginManager (const juce::File& dataDirectory)
    : listFile (dataDirectory.getChildFile ("Plugins.xml")),
      deadMansPedal (dataDirectory.getChildFile ("PluginScan.pending"))
{
    juce::addDefaultFormatsToManager (formats);
    known.setCustomScanner (std::make_unique<OutOfProcessScanner>());
    loadList();
}

PluginManager::~PluginManager()
{
    scanner.reset();
}

void PluginManager::loadList()
{
    if (auto xml = juce::parseXML (listFile))
        known.recreateFromXml (*xml);
}

void PluginManager::saveList() const
{
    if (auto xml = known.createXml())
    {
        listFile.getParentDirectory().createDirectory();
        xml->writeTo (listFile);
    }
}

juce::Array<juce::PluginDescription> PluginManager::getPlugins (bool instruments) const
{
    juce::Array<juce::PluginDescription> result;
    for (auto& d : known.getTypes())
        if (d.isInstrument == instruments)
            result.add (d);

    std::sort (result.begin(), result.end(), [] (const juce::PluginDescription& a, const juce::PluginDescription& b)
    {
        return a.name.compareIgnoreCase (b.name) < 0;
    });
    return result;
}

std::optional<juce::PluginDescription> PluginManager::findByIdentifier (const juce::String& identifier) const
{
    for (auto& d : known.getTypes())
        if (d.createIdentifierString() == identifier)
            return d;
    return std::nullopt;
}

juce::String PluginManager::toXmlString (const juce::PluginDescription& description)
{
    return description.createXml()->toString (juce::XmlElement::TextFormat().singleLine().withoutHeader());
}

std::optional<juce::PluginDescription> PluginManager::fromXmlString (const juce::String& xml)
{
    if (auto e = juce::parseXML (xml))
    {
        juce::PluginDescription d;
        if (d.loadFromXml (*e))
            return d;
    }
    return std::nullopt;
}

void PluginManager::createInstance (const juce::PluginDescription& description, double sampleRate, int blockSize,
                                    juce::AudioPluginFormat::PluginCreationCallback callback)
{
    formats.createPluginInstanceAsync (description, sampleRate, blockSize, std::move (callback));
}

std::unique_ptr<juce::AudioPluginInstance> PluginManager::createInstanceSync (const juce::PluginDescription& description,
                                                                              double sampleRate, int blockSize, juce::String& error)
{
    return formats.createPluginInstance (description, sampleRate, blockSize, error);
}

juce::FileSearchPath PluginManager::getSearchPath (juce::AudioPluginFormat& format) const
{
    auto path = format.getDefaultLocationsToSearch();

   #if JUCE_WINDOWS
    if (format.getName() == "VST")
    {
        const auto programFiles = juce::File::getSpecialLocation (juce::File::globalApplicationsDirectory);
        addIfExists (path, programFiles.getChildFile ("VSTPlugins"));
        addIfExists (path, programFiles.getChildFile ("Steinberg/VstPlugins"));
        addIfExists (path, programFiles.getChildFile ("Common Files/VST2"));
        addIfExists (path, programFiles.getChildFile ("Common Files/Steinberg/VST2"));
    }
   #endif

    if (format.getName() == "VST" || format.getName() == "VST3")
        for (auto& folder : extraFolders)
            addIfExists (path, juce::File (folder));

    path.removeRedundantPaths();
    return path;
}

void PluginManager::startScan (std::function<void (float, juce::String)> onProgress,
                               std::function<void (int, juce::StringArray)> onFinished)
{
    if (scanner != nullptr)
        return;

    scanner = std::make_unique<ScanThread> (*this, std::move (onProgress), std::move (onFinished));
    scanner->startThread (juce::Thread::Priority::low);
}

void PluginManager::cancelScan()
{
    if (scanner != nullptr)
        scanner->signalThreadShouldExit();
}

int PluginManager::runScanWorker (const juce::StringArray& args)
{
    const int index = args.indexOf ("--scan-plugin");
    if (index < 0 || args.size() < index + 4)
        return 2;

    const auto formatName = args[index + 1].unquoted();
    const auto identifier = args[index + 2].unquoted();
    const juce::File output (args[index + 3].unquoted());

    juce::AudioPluginFormatManager manager;
    juce::addDefaultFormatsToManager (manager);

    for (auto* format : manager.getFormats())
    {
        if (format->getName() != formatName)
            continue;

        juce::OwnedArray<juce::PluginDescription> found;
        format->findAllTypesForFile (found, identifier);

        juce::XmlElement root ("PLUGINS");
        for (auto* d : found)
            root.addChildElement (d->createXml().release());

        return root.writeTo (output) ? 0 : 3;
    }
    return 4;
}
}
