#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>

namespace harmony::engine
{
class PluginManager final
{
public:
    explicit PluginManager (const juce::File& dataDirectory);
    ~PluginManager();

    juce::AudioPluginFormatManager& getFormats() noexcept { return formats; }
    juce::KnownPluginList& getKnownPlugins() noexcept { return known; }

    void loadList();
    void saveList() const;

    juce::Array<juce::PluginDescription> getPlugins (bool instruments) const;
    std::optional<juce::PluginDescription> findByIdentifier (const juce::String& identifier) const;

    static juce::String toXmlString (const juce::PluginDescription& description);
    static std::optional<juce::PluginDescription> fromXmlString (const juce::String& xml);

    void createInstance (const juce::PluginDescription& description, double sampleRate, int blockSize,
                         juce::AudioPluginFormat::PluginCreationCallback callback);
    std::unique_ptr<juce::AudioPluginInstance> createInstanceSync (const juce::PluginDescription& description,
                                                                   double sampleRate, int blockSize, juce::String& error);

    juce::StringArray extraFolders;
    juce::FileSearchPath getSearchPath (juce::AudioPluginFormat& format) const;

    void startScan (std::function<void (float, juce::String)> onProgress,
                    std::function<void (int, juce::StringArray)> onFinished);
    void cancelScan();
    bool isScanning() const noexcept { return scanner != nullptr; }

    static int runScanWorker (const juce::StringArray& args);

private:
    class ScanThread;

    juce::AudioPluginFormatManager formats;
    juce::KnownPluginList known;
    juce::File listFile, deadMansPedal;
    std::unique_ptr<ScanThread> scanner;
};
}
