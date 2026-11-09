#pragma once

#include "model/Project.h"

namespace harmony
{
class History final : private juce::Timer
{
public:
    struct Entry
    {
        juce::File file;
        juce::Time time;
        juce::String project;
    };

    History (Project& project, const juce::File& dataDirectory, std::function<void()> captureStates);
    ~History() override;

    void setIntervalMinutes (int minutes);
    bool snapshot (const juce::String& reason);
    juce::Array<Entry> list() const;

    bool previousSessionCrashed() const noexcept { return crashed; }
    juce::File latestSnapshot() const;
    void markCleanExit();

private:
    void timerCallback() override;
    void prune (const juce::File& folder) const;

    Project& project;
    juce::File root, marker;
    std::function<void()> capture;
    juce::uint64 lastSavedCounter = 0;
    bool crashed = false;
};
}
