#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace harmony::prefs
{
struct BoolSetting
{
    const char* key;
    bool fallback;

    bool get (const juce::PropertySet& s) const { return s.getBoolValue (key, fallback); }
    void set (juce::PropertySet& s, bool v) const { s.setValue (key, v); }
};

struct IntSetting
{
    const char* key;
    int fallback;
    int minimum;
    int maximum;

    int get (const juce::PropertySet& s) const { return juce::jlimit (minimum, maximum, s.getIntValue (key, fallback)); }
    void set (juce::PropertySet& s, int v) const { s.setValue (key, juce::jlimit (minimum, maximum, v)); }
};

struct DoubleSetting
{
    const char* key;
    double fallback;
    double minimum;
    double maximum;

    double get (const juce::PropertySet& s) const { return juce::jlimit (minimum, maximum, s.getDoubleValue (key, fallback)); }
    void set (juce::PropertySet& s, double v) const { s.setValue (key, juce::jlimit (minimum, maximum, v)); }
};

inline constexpr BoolSetting previewNotes          { "previewNotes", true };
inline constexpr BoolSetting previewWhilePlaying   { "previewWhilePlaying", false };
inline constexpr BoolSetting ghostNotes            { "ghostNotes", true };
inline constexpr BoolSetting noteNames             { "noteNames", true };
inline constexpr BoolSetting snapPlayhead          { "snapPlayhead", true };
inline constexpr BoolSetting followPlayhead        { "followPlayhead", true };
inline constexpr BoolSetting returnToStartOnStop   { "returnToStartOnStop", true };
inline constexpr BoolSetting autoUpdate            { "autoUpdate", true };
inline constexpr BoolSetting openLastProject       { "openLastProject", false };

inline constexpr IntSetting autosaveChoice         { "autosaveChoice", 2, 0, 4 };
inline constexpr IntSetting uiScalePercent         { "uiScalePercent", 100, 75, 200 };
inline constexpr IntSetting pianoRollSnap          { "pianoRollSnap", 0, 0, 9 };
inline constexpr IntSetting playlistSnap           { "playlistSnap", 1, 0, 9 };
inline constexpr IntSetting metronomePercent       { "metronomePercent", 70, 0, 100 };
inline constexpr IntSetting defaultTrackCount      { "defaultTrackCount", 24, 4, 200 };

inline constexpr DoubleSetting defaultTempo        { "defaultTempo", 128.0, 10.0, 999.0 };

inline const int autosaveMinutes[] = { 0, 1, 3, 5, 10 };
inline const int uiScaleChoices[] = { 75, 90, 100, 110, 125, 150, 175, 200 };

inline juce::File projectFolder (const juce::PropertySet& s)
{
    const juce::File chosen (s.getValue ("projectFolder"));
    if (chosen.isDirectory())
        return chosen;
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Harmony Projects");
}
}
