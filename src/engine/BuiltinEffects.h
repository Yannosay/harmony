#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

namespace harmony::engine::builtin
{
inline const juce::String formatName { "Harmony" };
inline const juce::String vendor { "Yannosay" };

struct Info
{
    juce::String id;
    juce::String name;
    juce::String group;
    juce::String description;
};

const std::vector<Info>& all();
const Info* find (const juce::String& id);
juce::PluginDescription describe (const Info& info);
bool isBuiltin (const juce::PluginDescription& description);
std::unique_ptr<juce::AudioPluginInstance> create (const juce::String& id);

class Effect : public juce::AudioPluginInstance
{
public:
    Effect (const Info& info, juce::AudioProcessorValueTreeState::ParameterLayout layout);

    static inline std::function<juce::AudioProcessorEditor* (Effect&)> editorFactory;

    const Info& info;
    juce::AudioProcessorValueTreeState state;

    virtual bool hasResponseCurve() const { return false; }
    virtual void computeResponse (const std::vector<double>& frequencies, std::vector<float>& decibels) const;
    float getReductionDecibels() const noexcept { return reduction.load (std::memory_order_relaxed); }

    void fillInPluginDescription (juce::PluginDescription& d) const override { d = describe (info); }
    const juce::String getName() const override { return info.name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool hasEditor() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destination) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override;
    using juce::AudioPluginInstance::processBlock;

protected:
    virtual void prepareEffect (const juce::dsp::ProcessSpec& spec) = 0;
    virtual void processEffect (juce::AudioBuffer<float>& buffer, int numSamples) = 0;

    float param (juce::StringRef id) const;
    double getHostBpm() const;

    double currentSampleRate = 48000.0;
    int maxBlock = 512;
    std::atomic<float> reduction { 0.0f };
};
}
