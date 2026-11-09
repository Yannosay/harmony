#pragma once

#include "engine/Snapshot.h"
#include "model/Project.h"

namespace harmony::engine
{
class AudioEngine;

std::vector<MidiEvent> expandPatternNotes (const juce::ValueTree& pattern, int channelUid, juce::int64 patternLength,
                                           float swing, juce::int64 clipStart, juce::int64 clipLength,
                                           juce::int64 clipOffset, bool loopContent);

std::unique_ptr<Snapshot> buildSnapshot (const Project& project, const AudioEngine& engine);
}
