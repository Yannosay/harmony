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

    static std::optional<Version> parse (juce::String text)
    {
        text = text.trim().trimCharactersAtStart ("vV");
        const auto core = text.upToFirstOccurrenceOf ("-", false, false).upToFirstOccurrenceOf ("+", false, false);
        auto parts = juce::StringArray::fromTokens (core, ".", {});

        if (parts.size() < 1 || parts.size() > 3)
            return std::nullopt;

        for (auto& p : parts)
            if (p.isEmpty() || ! p.containsOnly ("0123456789") || p.length() > 6)
                return std::nullopt;

        return Version { parts[0].getIntValue(), parts[1].getIntValue(), parts[2].getIntValue() };
    }

    juce::String toString() const { return juce::String (major) + "." + juce::String (minor) + "." + juce::String (patch); }

    friend bool operator< (const Version& a, const Version& b) noexcept
    {
        if (a.major != b.major) return a.major < b.major;
        if (a.minor != b.minor) return a.minor < b.minor;
        return a.patch < b.patch;
    }

    friend bool operator== (const Version& a, const Version& b) noexcept
    {
        return a.major == b.major && a.minor == b.minor && a.patch == b.patch;
    }
};

inline Version currentVersion()
{
    return Version::parse (HARMONY_VERSION_STRING).value_or (Version {});
}
}
