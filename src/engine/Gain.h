#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace harmony::engine
{
inline constexpr float unityFader = 0.8f;

inline float faderToGain (float fader) noexcept
{
    const float x = juce::jlimit (0.0f, 1.25f, fader) / unityFader;
    return x * x;
}

inline float faderToDecibels (float fader) noexcept
{
    return juce::Decibels::gainToDecibels (faderToGain (fader), -96.0f);
}

inline void panGains (float pan, float& left, float& right) noexcept
{
    pan = juce::jlimit (-1.0f, 1.0f, pan);
    left = pan > 0.0f ? 1.0f - pan : 1.0f;
    right = pan < 0.0f ? 1.0f + pan : 1.0f;
}

struct StereoMeter
{
    std::atomic<float> left { 0.0f }, right { 0.0f };

    void push (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        update (left, buffer.getMagnitude (0, 0, numSamples));
        update (right, buffer.getMagnitude (juce::jmin (1, buffer.getNumChannels() - 1), 0, numSamples));
    }

    float takeLeft() noexcept  { return left.exchange (0.0f); }
    float takeRight() noexcept { return right.exchange (0.0f); }

private:
    static void update (std::atomic<float>& slot, float value) noexcept
    {
        float current = slot.load (std::memory_order_relaxed);
        while (value > current && ! slot.compare_exchange_weak (current, value, std::memory_order_relaxed)) {}
    }
};

class StereoGainStage
{
public:
    void prepare (double sampleRate)
    {
        left.reset (sampleRate, 0.02);
        right.reset (sampleRate, 0.02);
        left.setCurrentAndTargetValue (target (true));
        right.setCurrentAndTargetValue (target (false));
    }

    void process (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        left.setTargetValue (target (true));
        right.setTargetValue (target (false));
        left.applyGain (buffer.getWritePointer (0), numSamples);
        right.applyGain (buffer.getWritePointer (1), numSamples);
    }

    void clearAutomation() noexcept
    {
        automatedVolume.store (-1.0f, std::memory_order_relaxed);
        automatedPan.store (-2.0f, std::memory_order_relaxed);
    }

    bool isAutomated() const noexcept
    {
        return automatedVolume.load (std::memory_order_relaxed) >= 0.0f || automatedPan.load (std::memory_order_relaxed) >= -1.0f;
    }

    std::atomic<float> volume { 0.78f };
    std::atomic<float> pan { 0.0f };
    std::atomic<float> automatedVolume { -1.0f };
    std::atomic<float> automatedPan { -2.0f };
    std::atomic<bool> muted { false };
    std::atomic<bool> silencedBySolo { false };

private:
    float target (bool isLeft) const noexcept
    {
        if (muted.load (std::memory_order_relaxed) || silencedBySolo.load (std::memory_order_relaxed))
            return 0.0f;

        const float autoVolume = automatedVolume.load (std::memory_order_relaxed);
        const float autoPan = automatedPan.load (std::memory_order_relaxed);
        float l, r;
        panGains (autoPan >= -1.0f ? autoPan : pan.load (std::memory_order_relaxed), l, r);
        return faderToGain (autoVolume >= 0.0f ? autoVolume : volume.load (std::memory_order_relaxed)) * (isLeft ? l : r);
    }

    juce::SmoothedValue<float> left, right;
};
}
