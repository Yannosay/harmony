#pragma once

#include "update/Version.h"
#include <juce_events/juce_events.h>

namespace harmony::update
{
inline constexpr const char* repository = "Yannosay/harmony";
inline constexpr const char* manifestUrl = "https://harmony.yannosay.com/latest.json";

struct ReleaseInfo
{
    Version version;
    juce::String tag;
    juce::String notes;
    juce::String installerName;
    juce::URL installerUrl;
    juce::URL checksumsUrl;
    juce::String sha256;
};

juce::String platformInstallerSuffix();
std::optional<ReleaseInfo> parseRelease (const juce::var& json, const juce::String& installerSuffix);
std::optional<ReleaseInfo> parseManifest (const juce::var& json, const juce::String& installerSuffix);
juce::String findChecksum (const juce::String& checksumsText, const juce::String& fileName);
bool verifySha256 (const juce::File& file, const juce::String& expectedHex);

class Updater final : private juce::Thread
{
public:
    enum class State { idle, checking, upToDate, downloading, ready, failed };

    explicit Updater (const juce::File& downloadDirectory);
    ~Updater() override;

    void check (bool downloadIfAvailable);
    State getState() const noexcept { return state.load(); }
    juce::String getAvailableVersion() const;
    juce::String getReleaseNotes() const;
    juce::String getError() const;
    float getDownloadProgress() const noexcept { return downloadProgress.load(); }
    bool installAndQuit (bool relaunch);

    std::function<void()> onStateChanged;

private:
    void run() override;
    void setState (State newState);
    juce::Result download (const ReleaseInfo& release);
    std::optional<ReleaseInfo> fetchLatest() const;

    juce::File directory;
    std::atomic<State> state { State::idle };
    std::atomic<float> downloadProgress { 0.0f };
    std::atomic<bool> autoDownload { true };

    mutable juce::CriticalSection lock;
    std::optional<ReleaseInfo> available;
    juce::File installer;
    juce::String error;
};
}
