#include "model/MusicTheory.h"
#include <algorithm>
#include <set>

namespace harmony::theory
{
const std::vector<Scale>& allScales()
{
    static const std::vector<Scale> scales {
        { "Major",            { 0, 2, 4, 5, 7, 9, 11 } },
        { "Minor",            { 0, 2, 3, 5, 7, 8, 10 } },
        { "Harmonic Minor",   { 0, 2, 3, 5, 7, 8, 11 } },
        { "Melodic Minor",    { 0, 2, 3, 5, 7, 9, 11 } },
        { "Dorian",           { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygian",         { 0, 1, 3, 5, 7, 8, 10 } },
        { "Lydian",           { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydian",       { 0, 2, 4, 5, 7, 9, 10 } },
        { "Locrian",          { 0, 1, 3, 5, 6, 8, 10 } },
        { "Phrygian Dominant",{ 0, 1, 4, 5, 7, 8, 10 } },
        { "Major Pentatonic", { 0, 2, 4, 7, 9 } },
        { "Minor Pentatonic", { 0, 3, 5, 7, 10 } },
        { "Blues",            { 0, 3, 5, 6, 7, 10 } },
        { "Chromatic",        { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } }
    };
    return scales;
}

const Scale& findScale (const juce::String& name)
{
    for (auto& s : allScales())
        if (s.name.equalsIgnoreCase (name))
            return s;

    return allScales()[1];
}

juce::String pitchClassName (int pitchClass)
{
    static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    return names[((pitchClass % 12) + 12) % 12];
}

juce::String noteName (int key)
{
    return pitchClassName (key) + juce::String (key / 12 - 1);
}

bool isInScale (int key, int root, const Scale& scale)
{
    const int pc = ((key - root) % 12 + 12) % 12;
    return std::find (scale.intervals.begin(), scale.intervals.end(), pc) != scale.intervals.end();
}

int snapToScale (int key, int root, const Scale& scale)
{
    for (int distance = 0; distance < 12; ++distance)
    {
        if (isInScale (key - distance, root, scale)) return juce::jlimit (0, 127, key - distance);
        if (isInScale (key + distance, root, scale)) return juce::jlimit (0, 127, key + distance);
    }
    return key;
}

std::vector<int> scaleDegreePitches (int root, const Scale& scale, int degree, int chordSize, int baseKey)
{
    std::vector<int> pitches;
    const int n = (int) scale.intervals.size();
    const int startIndex = ((degree - 1) % n + n) % n;

    for (int i = 0; i < chordSize; ++i)
    {
        const int idx = startIndex + i * 2;
        const int octave = idx / n;
        pitches.push_back (baseKey + root + scale.intervals[(size_t) (idx % n)] + 12 * octave);
    }
    return pitches;
}

const std::vector<ChordQuality>& chordQualities()
{
    static const std::vector<ChordQuality> qualities {
        { "major",          "",      { 0, 4, 7 } },
        { "minor",          "m",     { 0, 3, 7 } },
        { "diminished",     "dim",   { 0, 3, 6 } },
        { "augmented",      "aug",   { 0, 4, 8 } },
        { "suspended 2",    "sus2",  { 0, 2, 7 } },
        { "suspended 4",    "sus4",  { 0, 5, 7 } },
        { "major 7",        "maj7",  { 0, 4, 7, 11 } },
        { "minor 7",        "m7",    { 0, 3, 7, 10 } },
        { "dominant 7",     "7",     { 0, 4, 7, 10 } },
        { "half-diminished","m7b5",  { 0, 3, 6, 10 } },
        { "diminished 7",   "dim7",  { 0, 3, 6, 9 } },
        { "minor-major 7",  "mMaj7", { 0, 3, 7, 11 } },
        { "augmented 7",    "aug7",  { 0, 4, 8, 10 } },
        { "major 6",        "6",     { 0, 4, 7, 9 } },
        { "minor 6",        "m6",    { 0, 3, 7, 9 } },
        { "add 9",          "add9",  { 0, 2, 4, 7 } },
        { "minor add 9",    "madd9", { 0, 2, 3, 7 } },
        { "major 9",        "maj9",  { 0, 2, 4, 7, 11 } },
        { "minor 9",        "m9",    { 0, 2, 3, 7, 10 } },
        { "dominant 9",     "9",     { 0, 2, 4, 7, 10 } },
        { "power",          "5",     { 0, 7 } }
    };
    return qualities;
}

juce::String detectChord (std::vector<int> keys)
{
    if (keys.size() < 2)
        return keys.empty() ? juce::String() : noteName (keys.front());

    std::sort (keys.begin(), keys.end());
    std::set<int> classes;
    for (auto k : keys)
        classes.insert (((k % 12) + 12) % 12);

    const int bass = ((keys.front() % 12) + 12) % 12;

    auto matches = [&classes] (int root, const ChordQuality& q)
    {
        std::set<int> shape;
        for (auto i : q.intervals)
            shape.insert ((root + i) % 12);
        return shape == classes;
    };

    for (auto& q : chordQualities())
        if (matches (bass, q))
            return pitchClassName (bass) + q.suffix;

    for (int root = 0; root < 12; ++root)
        if (root != bass)
            for (auto& q : chordQualities())
                if (matches (root, q))
                    return pitchClassName (root) + q.suffix + "/" + pitchClassName (bass);

    return {};
}

juce::String romanNumeral (int degree, const std::vector<int>& chordIntervals)
{
    static const char* numerals[] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    juce::String base = numerals[((degree - 1) % 7 + 7) % 7];

    if (chordIntervals.size() >= 3)
    {
        const int third = chordIntervals[1] - chordIntervals[0];
        const int fifth = chordIntervals[2] - chordIntervals[0];

        if (third == 3)
            base = base.toLowerCase();
        if (third == 3 && fifth == 6)
            base << juce::String::fromUTF8 ("\xc2\xb0");
        if (third == 4 && fifth == 8)
            base << "+";
    }
    return base;
}
}
