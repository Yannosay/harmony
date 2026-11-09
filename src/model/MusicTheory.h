#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace harmony::theory
{
struct Scale
{
    juce::String name;
    std::vector<int> intervals;
    bool isHeptatonic() const noexcept { return intervals.size() == 7; }
};

struct ChordQuality
{
    juce::String name;
    juce::String suffix;
    std::vector<int> intervals;
};

const std::vector<Scale>& allScales();
const Scale& findScale (const juce::String& name);

juce::String pitchClassName (int pitchClass);
juce::String noteName (int key);

bool isInScale (int key, int root, const Scale& scale);
int snapToScale (int key, int root, const Scale& scale);

std::vector<int> scaleDegreePitches (int root, const Scale& scale, int degree, int chordSize, int baseKey);

const std::vector<ChordQuality>& chordQualities();
juce::String detectChord (std::vector<int> keys);
juce::String romanNumeral (int degree, const std::vector<int>& chordIntervals);
}
