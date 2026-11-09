#include "model/DrumGenerator.h"
#include <algorithm>
#include <array>

namespace harmony
{
namespace
{
    constexpr int roleCount = (int) DrumRole::count;

    struct StyleDefinition
    {
        const char* name;
        std::array<const char*, roleCount> rows;
        bool hatRolls;
        bool tomFills;
    };

    // Step grammar: 'x' always, 'o' likely (density-scaled), '-' occasional (density-scaled), '.' rest.
    const StyleDefinition styles[] = {
        { "Trap",        { "x.....x...x....-", "........x.......", "........o.......", "x.x.x.x.x.x.x.x.", "..............-.", ".......-........", "...-.......-....", "................" }, true,  false },
        { "Drill",       { "x.....x.....-...", "........x......-", "........o.......", "x..x..x.x..x..x.", "...........-....", "......-.........", "..-.....-.......", "................" }, true,  false },
        { "Boom Bap",    { "x.....-.-.x.....", "....x.......x...", "....-.......-...", "x.o.x.o.x.o.x.o.", "..........-.....", "......-........-", "................", "................" }, false, false },
        { "Lo-Fi",       { "x......-.-x.....", "....x.......x...", "................", "x.o.x.o.x.o.x.o-", "..............-.", "...-.......-....", "................", "................" }, false, false },
        { "House",       { "x...x...x...x...", "................", "....x.......x...", "-.-.-.-.-.-.-.-.", "..x...x...x...x.", "................", "...-..-....-..-.", "................" }, false, false },
        { "Techno",      { "x...x...x...x...", "................", "....o.......o...", "--x---x---x---x-", "..-.......-.....", "...-...-..-....-", "-.....-.....-...", "................" }, false, false },
        { "Drum & Bass", { "x.........x.....", "....x..-.-..x...", "....-.......-...", "x.x.x.x.x.x.x.x.", "......-.......-.", "................", "..-.......-.....", "................" }, false, true  },
        { "Reggaeton",   { "x...x...x...x...", "...x..x....x..x.", "................", "x.x.x.x.x.x.x.x.", "..............-.", "...o..o....o..o.", "................", "................" }, false, false },
        { "Afrobeats",   { "x..-..x...x..-..", "................", "....x.......x...", "x.oxx.ox x.oxx.ox", "......-.........", "..x..x....x..x..", "-...-...-...-...", "................" }, false, false },
        { "Pop",         { "x.......x.o.....", "....x.......x...", "....o.......o...", "x.x.x.x.x.x.x.x.", "..............-.", "................", "................", "................" }, false, true  },
        { "Rock",        { "x.....x.x.......", "....x.......x...", "................", "x.x.x.x.x.x.x.x.", "...............-", "................", "................", "................" }, false, true  }
    };

    const StyleDefinition& findStyle (const juce::String& name)
    {
        for (auto& s : styles)
            if (name.equalsIgnoreCase (s.name))
                return s;

        return styles[0];
    }

    char stepAt (const char* row, int step)
    {
        int index = 0;
        for (const char* c = row; *c != 0; ++c)
        {
            if (*c == ' ')
                continue;
            if (index == step)
                return *c;
            ++index;
        }
        return '.';
    }
}

juce::StringArray drumStyleNames()
{
    juce::StringArray names;
    for (auto& s : styles)
        names.add (s.name);
    return names;
}

juce::String drumRoleName (DrumRole role)
{
    static const char* names[] = { "Kick", "Snare", "Clap", "Closed Hat", "Open Hat", "Rim", "Percussion", "Tom" };
    return names[juce::jlimit (0, roleCount - 1, (int) role)];
}

juce::StringArray drumRoleKeywords (DrumRole role)
{
    switch (role)
    {
        case DrumRole::kick:      return { "kick", "kik", "bd", "bass drum", "808" };
        case DrumRole::snare:     return { "snare", "snr", "sd" };
        case DrumRole::clap:      return { "clap", "clp" };
        case DrumRole::closedHat: return { "closed", "hat", "hh", "chh", "hihat", "hi-hat" };
        case DrumRole::openHat:   return { "open", "ohh", "oh" };
        case DrumRole::rim:       return { "rim", "stick", "click" };
        case DrumRole::perc:      return { "perc", "shaker", "conga", "bongo", "tamb", "cowbell", "fx" };
        case DrumRole::tom:       return { "tom" };
        case DrumRole::count:     break;
    }
    return {};
}

DrumRole guessDrumRole (const juce::String& channelName, bool& matched)
{
    const auto lower = channelName.toLowerCase();
    matched = true;

    for (auto role : { DrumRole::openHat, DrumRole::closedHat, DrumRole::kick, DrumRole::snare,
                       DrumRole::clap, DrumRole::rim, DrumRole::tom, DrumRole::perc })
        for (auto& keyword : drumRoleKeywords (role))
            if (lower.contains (keyword))
                return role;

    matched = false;
    return DrumRole::perc;
}

std::vector<DrumHit> generateDrums (const DrumGenSettings& s, juce::int64 stepTicks)
{
    std::vector<DrumHit> hits;
    const auto& style = findStyle (s.style);
    juce::Random rng ((juce::int64) s.seed);

    const float density = juce::jlimit (0.0f, 1.0f, s.density);
    const float variation = juce::jlimit (0.0f, 1.0f, s.variation);
    const float humanize = juce::jlimit (0.0f, 1.0f, s.humanize);
    const int bars = juce::jlimit (1, 16, s.bars);

    auto addHit = [&] (DrumRole role, juce::int64 start, juce::int64 length, float velocity)
    {
        const bool anchored = role == DrumRole::kick || role == DrumRole::snare || role == DrumRole::clap;
        const auto jitterRange = (juce::int64) (stepTicks * 0.12f * humanize * (anchored ? 0.3f : 1.0f));
        const auto jitter = jitterRange > 0 ? (juce::int64) rng.nextInt ((int) (jitterRange * 2 + 1)) - jitterRange : 0;
        const float vel = juce::jlimit (0.08f, 1.0f, velocity + (rng.nextFloat() - 0.5f) * 0.3f * humanize);
        hits.push_back ({ role, juce::jmax ((juce::int64) 0, start + jitter), juce::jmax ((juce::int64) 1, length), vel });
    };

    for (int bar = 0; bar < bars; ++bar)
    {
        const bool phraseEnd = (bar % 2) == 1 || bars == 1;
        const juce::int64 barStart = (juce::int64) bar * 16 * stepTicks;

        for (int r = 0; r < roleCount; ++r)
        {
            const auto role = (DrumRole) r;

            for (int step = 0; step < 16; ++step)
            {
                const char c = stepAt (style.rows[(size_t) r], step);
                float probability = 0.0f, velocity = 0.0f;

                switch (c)
                {
                    case 'x': probability = 1.0f;                     velocity = 0.9f;  break;
                    case 'o': probability = 0.3f + 0.65f * density;   velocity = 0.72f; break;
                    case '-': probability = 0.08f + 0.5f * density;   velocity = 0.55f; break;
                    default: break;
                }

                if (c == 'x' && role != DrumRole::kick && role != DrumRole::snare && rng.nextFloat() < variation * 0.08f)
                    probability = 0.0f;

                if (c == '.' && role == DrumRole::kick && step % 2 == 1 && rng.nextFloat() < variation * density * 0.06f)
                {
                    probability = 1.0f;
                    velocity = 0.6f;
                }

                if (probability <= 0.0f || rng.nextFloat() > probability)
                    continue;

                const juce::int64 start = barStart + step * stepTicks;

                if (role == DrumRole::closedHat && style.hatRolls && step >= 4 && rng.nextFloat() < variation * 0.22f)
                {
                    const int divisions = rng.nextBool() ? 2 : 3;
                    const int span = rng.nextFloat() < 0.3f ? 2 : 1;
                    const juce::int64 total = stepTicks * span;
                    const int count = divisions * span;

                    for (int i = 0; i < count; ++i)
                        addHit (role, start + total * i / count, total / count, velocity * (0.75f + 0.25f * (float) i / (float) count));

                    step += span - 1;
                    continue;
                }

                addHit (role, start, stepTicks, velocity);
            }
        }

        if (phraseEnd && style.tomFills && rng.nextFloat() < variation)
        {
            for (int step = 12; step < 16; ++step)
                if (rng.nextFloat() < 0.5f + 0.4f * density)
                    addHit (rng.nextBool() ? DrumRole::tom : DrumRole::snare, barStart + step * stepTicks, stepTicks, 0.7f + 0.05f * (float) (step - 12));
        }
        else if (phraseEnd && rng.nextFloat() < variation * 0.6f)
        {
            const juce::int64 rollStart = barStart + 14 * stepTicks;
            for (int i = 0; i < 4; ++i)
                addHit (DrumRole::snare, rollStart + stepTicks * i / 2, stepTicks / 2, 0.45f + 0.12f * (float) i);
        }
    }

    std::sort (hits.begin(), hits.end(), [] (const DrumHit& a, const DrumHit& b) { return a.start < b.start; });
    return hits;
}
}
