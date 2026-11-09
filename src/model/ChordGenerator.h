#pragma once

#include "model/Project.h"

namespace harmony
{
enum class ChordRhythm
{
    sustain,
    quarters,
    eighths,
    offbeats,
    arpUp,
    arpDown,
    arpUpDown,
    strum
};

struct ChordProgression
{
    juce::String name;
    std::vector<int> degrees;
};

struct ChordGenSettings
{
    int root = 0;
    juce::String scale { "Minor" };
    int progression = 0;
    int randomLength = 4;
    int chordSize = 3;
    int octave = 4;
    int barsPerChord = 1;
    bool voiceLeading = true;
    bool addBass = true;
    ChordRhythm rhythm = ChordRhythm::sustain;
    float velocity = 0.8f;
    juce::uint32 seed = 1;
    int channel = 0;
};

struct ChordGenResult
{
    std::vector<NoteData> notes;
    juce::StringArray chordNames;
    juce::StringArray numerals;
    juce::int64 length = 0;
};

const std::vector<ChordProgression>& chordProgressions();
juce::StringArray chordRhythmNames();

ChordGenResult generateChords (const ChordGenSettings& settings, juce::int64 ticksPerBar, juce::int64 ticksPerBeat);
std::vector<int> leadVoicing (const std::vector<int>& previous, const std::vector<int>& chord, int lowLimit, int highLimit);
}
