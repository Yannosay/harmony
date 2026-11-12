#pragma once

#include "model/Project.h"

namespace harmony::follow
{
enum class Style { sustainedBass, pulsingBass, arpeggio, melody };

struct ChordSegment
{
    juce::int64 start = 0;
    juce::int64 end = 0;
    std::vector<int> keys;
    int root = 0;
    juce::String name;
};

struct Settings
{
    Style style = Style::pulsingBass;
    int octave = 2;
    float density = 0.5f;
    float velocity = 0.8f;
    int scaleRoot = 0;
    juce::String scale { "Minor" };
    juce::uint32 seed = 1;
    int channel = 0;
};

juce::StringArray styleNames();
std::vector<ChordSegment> analyse (const std::vector<NoteData>& source, juce::int64 length, juce::int64 ticksPerBeat);
std::vector<NoteData> generate (const std::vector<ChordSegment>& chords, const Settings& settings, juce::int64 ticksPerBeat);
}
