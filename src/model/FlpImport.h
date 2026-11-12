#pragma once

#include "model/Project.h"

namespace harmony::flp
{
struct Event
{
    int id = 0;
    juce::uint32 value = 0;
    juce::MemoryBlock data;
};

struct Options
{
    std::function<juce::String (const juce::String& pluginName)> findInstrumentXml;
    std::function<juce::String (const juce::String& pluginName)> findEffectXml;
    juce::Array<juce::File> factoryDataFolders;
    juce::File userDataFolder;
};

struct Report
{
    juce::String flVersion;
    int channels = 0, patterns = 0, notes = 0, patternClips = 0, audioClips = 0, effects = 0;
    int automationClipsSkipped = 0, layerChannelsSkipped = 0, insertsBeyondRange = 0;
    juce::StringArray placeholderInstruments, missingSamples, skippedEffects;

    juce::String summary() const;
    juce::String details() const;
};

bool parseEvents (const juce::MemoryBlock& file, int& fileTicksPerQuarter, std::vector<Event>& events, juce::String& error);
juce::ValueTree convert (const juce::File& file, const Options& options, Report& report, juce::String& error);
juce::String builtinEffectFor (const juce::String& flPluginName);
Options defaultOptions();
}
