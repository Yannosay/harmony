#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace harmony
{
enum class DrumRole
{
    kick,
    snare,
    clap,
    closedHat,
    openHat,
    rim,
    perc,
    tom,
    count
};

struct DrumHit
{
    DrumRole role;
    juce::int64 start;
    juce::int64 length;
    float velocity;
};

struct DrumGenSettings
{
    juce::String style { "Trap" };
    int bars = 2;
    float density = 0.5f;
    float humanize = 0.15f;
    float variation = 0.35f;
    juce::uint32 seed = 1;
};

juce::StringArray drumStyleNames();
juce::String drumRoleName (DrumRole role);
juce::StringArray drumRoleKeywords (DrumRole role);
DrumRole guessDrumRole (const juce::String& channelName, bool& matched);

std::vector<DrumHit> generateDrums (const DrumGenSettings& settings, juce::int64 ticksPerStep);
}
