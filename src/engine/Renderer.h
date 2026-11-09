#pragma once

#include "engine/AudioEngine.h"

namespace harmony::engine
{
struct RenderSettings
{
    juce::File output;
    juce::String format { "WAV" };
    int bitDepth = 24;
    double sampleRate = 48000.0;
    int oggQuality = 8;
    bool songMode = true;
    juce::int64 startTick = 0;
    juce::int64 endTick = 0;
    double bpm = 128.0;
    double tailSeconds = 2.0;
    bool normalize = false;
    bool dither = true;
    std::vector<std::pair<int, juce::String>> stemInserts;
};

class Renderer final : private juce::Thread
{
public:
    Renderer (AudioEngine& engine, RenderSettings settings, std::function<void (juce::Result)> onFinished);
    ~Renderer() override;

    void start();
    void cancel() { signalThreadShouldExit(); }
    float getProgress() const noexcept { return progress.load(); }
    const RenderSettings& getSettings() const noexcept { return settings; }

    static juce::StringArray formatNames();
    static juce::String extensionFor (const juce::String& formatName);
    static std::unique_ptr<juce::AudioFormat> createFormat (const juce::String& formatName);
    static juce::Array<int> bitDepthsFor (const juce::String& formatName);

private:
    void run() override;
    juce::Result renderAll();
    std::unique_ptr<juce::AudioFormatWriter> openWriter (const juce::File& file, juce::AudioFormat& format, bool floatingPoint, int bits) const;
    juce::Result convertFinal (const juce::File& temp, float peak);

    AudioEngine& engine;
    RenderSettings settings;
    std::function<void (juce::Result)> onFinished;
    std::atomic<float> progress { 0.0f };
};
}
