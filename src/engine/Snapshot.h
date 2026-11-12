#pragma once

#include "model/Automation.h"
#include <vector>

namespace juce { class AudioProcessorParameter; }

namespace harmony::engine
{
class ChannelNode;
class InsertNode;
class FxSlotNode;
class StereoGainStage;

struct MidiEvent
{
    juce::int64 tick;
    juce::uint8 key;
    juce::uint8 velocity;

    bool isNoteOff() const noexcept { return velocity == 0; }
};

struct ClipSpan
{
    juce::int64 start;
    juce::int64 end;
    juce::int64 sourceOffset;
    float gain;
};

struct PatternInfo
{
    int uid;
    juce::int64 length;
};

struct ChannelPlan
{
    int uid = 0;
    int insert = 0;
    ChannelNode* node = nullptr;
    std::vector<MidiEvent> song;
    std::vector<ClipSpan> audioClips;
    std::vector<std::vector<MidiEvent>> patterns;
};

struct InsertPlan
{
    int index = 0;
    InsertNode* node = nullptr;
    std::vector<FxSlotNode*> slots;
};

struct AutomationSegment
{
    juce::int64 start;
    juce::int64 end;
    juce::int64 offset;
    std::vector<automation::Point> points;
};

struct AutomationLane
{
    enum class Mode { volume, pan, parameter };

    Mode mode = Mode::parameter;
    StereoGainStage* gain = nullptr;
    juce::AudioProcessorParameter* parameter = nullptr;
    std::vector<AutomationSegment> segments;

    bool valueAt (double tick, float& value) const noexcept;
};

struct Snapshot
{
    double bpm = 128.0;
    juce::int64 ticksPerBeat = 960;
    juce::int64 ticksPerBar = 3840;
    juce::int64 songLength = 0;
    juce::int64 loopStart = 0;
    juce::int64 loopEnd = 0;
    std::vector<PatternInfo> patterns;
    std::vector<ChannelPlan> channels;
    std::vector<InsertPlan> inserts;
    std::vector<AutomationLane> automation;
    int numerator = 4;
    int denominator = 4;

    int patternIndex (int uid) const noexcept
    {
        for (size_t i = 0; i < patterns.size(); ++i)
            if (patterns[i].uid == uid)
                return (int) i;

        return patterns.empty() ? -1 : 0;
    }

    juce::int64 songLoopEnd() const noexcept
    {
        const auto bar = juce::jmax ((juce::int64) 1, ticksPerBar);
        return juce::jmax (bar, ((songLength + bar - 1) / bar) * bar);
    }
};

void sortEvents (std::vector<MidiEvent>& events);
}
