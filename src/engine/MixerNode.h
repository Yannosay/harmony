#pragma once

#include "engine/Gain.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace harmony::engine
{
class FxSlotNode
{
public:
    FxSlotNode (int uid, juce::String signature, std::unique_ptr<juce::AudioPluginInstance> instance);
    ~FxSlotNode();

    void prepare (double sampleRate, int maxBlockSize);
    void process (juce::AudioBuffer<float>& io, int numSamples);
    int getLatencySamples() const noexcept;

    juce::AudioPluginInstance* getPlugin() noexcept { return plugin.get(); }

    const int uid;
    const juce::String signature;
    std::atomic<bool> bypassed { false };

private:
    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::AudioBuffer<float> work;
    juce::MidiBuffer midi;
};

class DelayCompensator
{
public:
    void prepare (int maxDelaySamples, int maxBlockSize);
    void process (juce::AudioBuffer<float>& io, int numSamples, int delaySamples) noexcept;

private:
    juce::AudioBuffer<float> ring;
    int writePosition = 0;
    int currentDelay = 0;
};

class InsertNode
{
public:
    explicit InsertNode (int index);

    void prepare (double sampleRate, int maxBlockSize);
    juce::AudioBuffer<float>& getBuffer() noexcept { return buffer; }
    void clear (int numSamples) noexcept { buffer.clear (0, numSamples); }

    const int index;
    StereoGainStage gain;
    StereoMeter meter;
    std::atomic<bool> solo { false };
    DelayCompensator compensator;

private:
    juce::AudioBuffer<float> buffer;
};
}
