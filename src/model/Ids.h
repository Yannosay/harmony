#pragma once

#include <juce_data_structures/juce_data_structures.h>

namespace harmony::id
{
#define HARMONY_ID(name) inline const juce::Identifier name { #name };

HARMONY_ID (PROJECT)
HARMONY_ID (CHANNELS)
HARMONY_ID (CHANNEL)
HARMONY_ID (PATTERNS)
HARMONY_ID (PATTERN)
HARMONY_ID (NOTE)
HARMONY_ID (TRACKS)
HARMONY_ID (TRACK)
HARMONY_ID (CLIP)
HARMONY_ID (MIXER)
HARMONY_ID (INSERT)
HARMONY_ID (SLOT)
HARMONY_ID (POINT)

HARMONY_ID (formatVersion)
HARMONY_ID (appVersion)
HARMONY_ID (nextUid)
HARMONY_ID (uid)
HARMONY_ID (name)
HARMONY_ID (colour)
HARMONY_ID (bpm)
HARMONY_ID (numerator)
HARMONY_ID (denominator)
HARMONY_ID (swing)
HARMONY_ID (loopStart)
HARMONY_ID (loopEnd)
HARMONY_ID (scaleRoot)
HARMONY_ID (scaleType)
HARMONY_ID (currentPattern)
HARMONY_ID (selectedChannel)

HARMONY_ID (kind)
HARMONY_ID (volume)
HARMONY_ID (pan)
HARMONY_ID (mute)
HARMONY_ID (solo)
HARMONY_ID (insert)
HARMONY_ID (pluginDesc)
HARMONY_ID (pluginState)
HARMONY_ID (bypass)
HARMONY_ID (sfPath)
HARMONY_ID (sfPreset)
HARMONY_ID (samplePath)
HARMONY_ID (rootNote)
HARMONY_ID (oneShot)
HARMONY_ID (attack)
HARMONY_ID (release)
HARMONY_ID (index)

HARMONY_ID (steps)
HARMONY_ID (channel)
HARMONY_ID (key)
HARMONY_ID (start)
HARMONY_ID (length)
HARMONY_ID (velocity)
HARMONY_ID (muted)

HARMONY_ID (pattern)
HARMONY_ID (offset)
HARMONY_ID (gain)
HARMONY_ID (target)
HARMONY_ID (time)
HARMONY_ID (value)
HARMONY_ID (tension)

#undef HARMONY_ID
}

namespace harmony
{
inline constexpr int ppq = 960;
inline constexpr int ticksPerStep = ppq / 4;
inline constexpr int numMixerInserts = 24;
inline constexpr int numFxSlots = 10;
inline constexpr int defaultTrackCount = 24;
inline constexpr int projectFormatVersion = 1;

namespace kind
{
    inline const juce::String plugin { "plugin" };
    inline const juce::String soundfont { "soundfont" };
    inline const juce::String sampler { "sampler" };
    inline const juce::String audio { "audio" };
    inline const juce::String pattern { "pattern" };
    inline const juce::String automation { "automation" };
}
}
