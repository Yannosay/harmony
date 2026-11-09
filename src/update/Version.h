#pragma once

#include <juce_core/juce_core.h>
#include <optional>

namespace harmony::update
{
struct Version
{
    int major = 0;
    int minor = 0;
    int patch = 0;
    juce::String prerelease;

    static std::optional<Version> parse (juce::String text)
    {
        text = text.trim().trimCharactersAtStart ("vV");
        text = text.upToFirstOccurrenceOf ("+", false, false);
        const auto core = text.upToFirstOccurrenceOf ("-", false, false);
        const auto pre = text.fromFirstOccurrenceOf ("-", false, false).toLowerCase();
        auto parts = juce::StringArray::fromTokens (core, ".", {});

        if (parts.size() < 1 || parts.size() > 3)
            return std::nullopt;

        for (auto& p : parts)
            if (p.isEmpty() || ! p.containsOnly ("0123456789") || p.length() > 6)
                return std::nullopt;

        if (pre.isNotEmpty() && ! pre.containsOnly ("abcdefghijklmnopqrstuvwxyz0123456789."))
            return std::nullopt;

        return Version { parts[0].getIntValue(), parts[1].getIntValue(), parts[2].getIntValue(), pre };
    }

    juce::String numbers() const
    {
        return juce::String (major) + "." + juce::String (minor) + "." + juce::String (patch);
    }

    juce::String toString() const
    {
        return prerelease.isEmpty() ? numbers() : numbers() + "-" + prerelease;
    }

    juce::String display() const
    {
        if (prerelease.isEmpty())
            return numbers();

        const auto stage = prerelease.upToFirstOccurrenceOf (".", false, false);
        const auto rest = prerelease.fromFirstOccurrenceOf (".", false, false);
        return stage.substring (0, 1).toUpperCase() + stage.substring (1) + " " + numbers() + (rest.isNotEmpty() ? "." + rest : juce::String());
    }

    friend bool operator< (const Version& a, const Version& b) noexcept
    {
        if (a.major != b.major) return a.major < b.major;
        if (a.minor != b.minor) return a.minor < b.minor;
        if (a.patch != b.patch) return a.patch < b.patch;
        if (a.prerelease == b.prerelease) return false;
        if (a.prerelease.isEmpty()) return false;
        if (b.prerelease.isEmpty()) return true;
        return a.prerelease.compareNatural (b.prerelease) < 0;
    }

    friend bool operator== (const Version& a, const Version& b) noexcept
    {
        return a.major == b.major && a.minor == b.minor && a.patch == b.patch && a.prerelease == b.prerelease;
    }
};

inline Version currentVersion()
{
    return Version::parse (HARMONY_VERSION_STRING).value_or (Version {});
}
}
