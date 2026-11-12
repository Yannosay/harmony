#pragma once

#include "engine/ProjectSync.h"
#include <juce_gui_extra/juce_gui_extra.h>

namespace harmony
{
class History;
namespace update { class Updater; }

struct Context
{
    Project& project;
    engine::AudioEngine& engine;
    engine::PluginManager& plugins;
    engine::ProjectSync& sync;
    juce::ApplicationCommandManager& commands;
    juce::PropertiesFile& settings;
    History& history;
    update::Updater& updater;

    std::function<void (const juce::String&, bool)> notify;
    std::function<void (const juce::String&)> setHint;
    std::function<void (int)> openPianoRoll;
    std::function<void (int)> openChannelEditor;
    std::function<void (int)> openSlotEditor;
    std::function<void (int)> focusMixerInsert;
    std::function<void (std::unique_ptr<juce::Component>, const juce::String&)> showPanel;
    std::function<void()> closePanel;
    std::function<void (const juce::File&, int trackIndex, juce::int64 tick)> importAudio;
    std::function<void (const juce::String& pluginIdentifier)> addInstrument;
    std::function<void (const juce::File&)> addFileAsChannel;
    std::function<void (const automation::Target&, const juce::String& name)> createAutomation;
    std::function<void (juce::AudioProcessor&, int parameterIndex)> automateParameter;
    std::function<int()> effectTargetInsert;
    std::function<void()> toggleEditorMaximised;
    std::function<bool()> isEditorMaximised;
    std::function<bool (int insertIndex, const juce::String& pluginIdentifier, int slotIndex)> addEffect;
};
}
