#pragma once

#include "engine/Gain.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <array>
#include <bitset>

struct tsf;

namespace harmony::engine
{
struct ClipSegment
{
    int destStart = 0;
    int numSamples = 0;
    double sourcePosition = 0.0;
    float gain = 1.0f;
};

class ChannelNode
{
public:
    ChannelNode (int uid, juce::String signature);
    virtual ~ChannelNode() = default;

    void prepare (double sampleRate, int maxBlockSize);
    void process (int numSamples);

    void addNoteOn (int key, float velocity, int sampleOffset);
    void addNoteOff (int key, int sampleOffset);
    void addMessage (const juce::MidiMessage& message, int sampleOffset);
    void releaseHeldNotes (int sampleOffset);
    void addClipSegment (const ClipSegment& segment) noexcept;

    juce::AudioBuffer<float>& getOutput() noexcept { return output; }

    virtual juce::AudioPluginInstance* getPlugin() noexcept { return nullptr; }
    virtual void setNonRealtime (bool) {}
    virtual juce::String getTypeName() const = 0;

    const int uid;
    const juce::String signature;
    StereoGainStage gain;
    StereoMeter meter;
    std::atomic<bool> solo { false };

protected:
    virtual void prepareNode (double sampleRate, int maxBlockSize) = 0;
    virtual void renderNode (juce::AudioBuffer<float>& out, int numSamples, const juce::MidiBuffer& midi) = 0;
    virtual void renderClip (juce::AudioBuffer<float>&, const ClipSegment&) {}

    double sampleRate = 44100.0;

private:
    juce::MidiBuffer pending;
    juce::AudioBuffer<float> output;
    std::bitset<128> held;
    std::array<ClipSegment, 16> clips;
    int numClips = 0;
};

class PluginChannelNode final : public ChannelNode
{
public:
    PluginChannelNode (int uid, juce::String signature, std::unique_ptr<juce::AudioPluginInstance> instance);
    ~PluginChannelNode() override;

    juce::AudioPluginInstance* getPlugin() noexcept override { return plugin.get(); }
    void setNonRealtime (bool nonRealtime) override { plugin->setNonRealtime (nonRealtime); }
    juce::String getTypeName() const override { return plugin->getPluginDescription().pluginFormatName; }

private:
    void prepareNode (double sampleRate, int maxBlockSize) override;
    void renderNode (juce::AudioBuffer<float>& out, int numSamples, const juce::MidiBuffer& midi) override;

    std::unique_ptr<juce::AudioPluginInstance> plugin;
    juce::AudioBuffer<float> work;
    juce::MidiBuffer midiCopy;
};

class SoundFontChannelNode final : public ChannelNode
{
public:
    SoundFontChannelNode (int uid, juce::String signature, tsf* font);
    ~SoundFontChannelNode() override;

    static tsf* loadFont (const juce::File& file, juce::String& error);
    juce::StringArray getPresetNames() const;
    juce::String getTypeName() const override { return "SoundFont"; }

    std::atomic<int> preset { 0 };

private:
    void prepareNode (double sampleRate, int maxBlockSize) override;
    void renderNode (juce::AudioBuffer<float>& out, int numSamples, const juce::MidiBuffer& midi) override;
    void renderSpan (juce::AudioBuffer<float>& out, int start, int count);

    tsf* font = nullptr;
    int appliedPreset = -1;
    juce::HeapBlock<float> interleaved;
    int interleavedCapacity = 0;
};

struct SampleData
{
    juce::AudioBuffer<float> audio;
    double sampleRate = 44100.0;
};

class SamplerChannelNode final : public ChannelNode
{
public:
    SamplerChannelNode (int uid, juce::String signature, std::shared_ptr<const SampleData> sample);

    static std::shared_ptr<const SampleData> loadSample (const juce::File& file, juce::AudioFormatManager& formats, juce::String& error);
    std::shared_ptr<const SampleData> getSample() const { return sample; }
    juce::String getTypeName() const override { return "Sampler"; }

    std::atomic<int> rootNote { 60 };
    std::atomic<bool> oneShot { true };
    std::atomic<float> attackSeconds { 0.001f };
    std::atomic<float> releaseSeconds { 0.08f };

private:
    struct Voice
    {
        bool active = false;
        int key = 0;
        double position = 0.0;
        double increment = 1.0;
        float velocityGain = 1.0f;
        float envelope = 0.0f;
        float attackStep = 1.0f;
        float releaseStep = 0.0f;
        bool releasing = false;
        juce::uint32 age = 0;
    };

    void prepareNode (double sampleRate, int maxBlockSize) override;
    void renderNode (juce::AudioBuffer<float>& out, int numSamples, const juce::MidiBuffer& midi) override;
    void renderClip (juce::AudioBuffer<float>& out, const ClipSegment& segment) override;
    void startVoice (int key, float velocity);
    void stopVoices (int key, bool fast);
    void renderVoices (juce::AudioBuffer<float>& out, int start, int count);

    std::shared_ptr<const SampleData> sample;
    std::array<Voice, 48> voices;
    juce::uint32 voiceCounter = 0;
};
}
