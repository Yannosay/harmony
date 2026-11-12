#pragma once

#include "engine/Snapshot.h"
#include "engine/ChannelNodes.h"
#include "engine/MixerNode.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <map>

namespace harmony::engine
{
struct RecordedEvent
{
    double tick;
    int channel;
    int key;
    float velocity;
    bool on;
};

class HostPlayHead final : public juce::AudioPlayHead
{
public:
    juce::Optional<PositionInfo> getPosition() const override { return info; }
    PositionInfo info;
};

class AudioEngine final : public juce::AudioIODeviceCallback,
                          private juce::MidiInputCallback,
                          private juce::Timer
{
public:
    AudioEngine();
    ~AudioEngine() override;

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }
    juce::AudioFormatManager& getFormats() noexcept { return formats; }
    juce::String openDevice (const juce::XmlElement* savedState);
    void closeDevice();
    void enableMidiInputs();

    void play();
    void stop (bool returnToStart = true);
    void togglePlay();
    bool isPlaying() const noexcept { return playing.load(); }
    void setPositionTicks (double ticks);
    void jumpTo (double ticks);
    double getPositionTicks() const noexcept { return position.load(); }
    double getDisplayPositionTicks() const noexcept;
    void setSongMode (bool shouldUseSong);
    bool isSongMode() const noexcept { return songMode.load(); }
    void setRecording (bool shouldRecord) noexcept { recording = shouldRecord; }
    bool isRecording() const noexcept { return recording.load(); }
    void setMetronomeEnabled (bool enabled) noexcept { metronome = enabled; }
    void setMetronomeLevel (float level) noexcept { metronomeLevel = juce::jlimit (0.0f, 1.0f, level); }
    bool isMetronomeEnabled() const noexcept { return metronome.load(); }
    void setCurrentPattern (int uid) noexcept { currentPattern = uid; }
    void setLiveChannel (int uid) noexcept { liveChannel = uid; }
    void panic();
    void clearAutomation();

    void publish (std::unique_ptr<Snapshot> snapshot);
    const Snapshot* getPublishedSnapshot() const noexcept { return published.get(); }

    void addChannelNode (std::unique_ptr<ChannelNode> node);
    void retireChannelNode (int uid);
    ChannelNode* findChannelNode (int uid) const;
    std::vector<int> getChannelNodeIds() const;

    void addSlotNode (std::unique_ptr<FxSlotNode> node);
    void retireSlotNode (int uid);
    FxSlotNode* findSlotNode (int uid) const;
    std::vector<int> getSlotNodeIds() const;

    InsertNode* getInsertNode (int index) const noexcept;
    int getNumInserts() const noexcept { return (int) inserts.size(); }

    void sendPreviewNote (int channelUid, int key, float velocity, bool on);
    void previewFile (const juce::File& file);
    void stopPreview();

    void drainRecorded (std::vector<RecordedEvent>& destination);

    void beginOffline (double sampleRate, int blockSize, juce::int64 startTick, bool songMode);
    void processOffline (juce::AudioBuffer<float>& output, int numSamples);
    void stopOfflineTransport() noexcept { playing = false; stopPending = true; }
    void endOffline();
    bool isOffline() const noexcept { return offline.load(); }

    double getSampleRate() const noexcept { return sampleRate; }
    int getBlockSize() const noexcept { return blockSize; }
    double getCpuLoad() const { return deviceManager.getCpuUsage(); }
    void prepareStandalone (double newSampleRate, int newBlockSize);

    std::function<void()> onDeviceStarted;

    void audioDeviceIOCallbackWithContext (const float* const* inputs, int numInputs,
                                           float* const* outputs, int numOutputs, int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;

private:
    struct ReaderGuard
    {
        std::atomic<bool> busy { false };
        std::atomic<juce::uint64> counter { 0 };
    };

    struct Retired
    {
        std::function<void()> destroy;
        juce::uint64 realtimeCounter;
        juce::uint64 offlineCounter;
        bool realtimeBusy;
        bool offlineBusy;
    };

    struct PreviewEvent
    {
        int channel;
        int key;
        float velocity;
        bool on;
    };

    void handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage&) override;
    void timerCallback() override;

    void processChunk (const Snapshot& snap, juce::AudioBuffer<float>& out, int numSamples, bool realtime);
    void schedule (const Snapshot& snap, bool song, int patternIndex, double from, double to,
                   int sampleOffset, double samplesPerTick, int numSamples);
    void releaseAll (const Snapshot& snap, int sampleOffset);
    void chaseNotes (const Snapshot& snap, bool song, int patternIndex, double tick, int sampleOffset);
    void mixDown (const Snapshot& snap, int numSamples);
    void applyAutomation (const Snapshot& snap, double tick) noexcept;
    void updatePlayHead (const Snapshot& snap, double tick, bool isPlaying, double samplesPerTick, bool looping);
    void clearUntargetedAutomation();
    void addMetronome (juce::AudioBuffer<float>& out, int numSamples);
    void prepareAll (double newSampleRate, int newBlockSize);
    void retire (std::function<void()> destroy);
    void collectGarbage (bool force);
    ChannelNode* liveTarget (const Snapshot& snap) const noexcept;

    juce::AudioDeviceManager deviceManager;
    juce::AudioFormatManager formats;
    HostPlayHead playHead;

    std::map<int, std::unique_ptr<ChannelNode>> channelNodes;
    std::map<int, std::unique_ptr<FxSlotNode>> slotNodes;
    std::vector<std::unique_ptr<InsertNode>> inserts;

    std::unique_ptr<Snapshot> published;
    std::atomic<const Snapshot*> live { nullptr };
    ReaderGuard realtimeGuard, offlineGuard;
    std::vector<Retired> graveyard;
    std::vector<std::function<void()>> retireAfterPublish;

    std::atomic<bool> playing { false }, songMode { false }, recording { false }, metronome { false };
    std::atomic<bool> stopPending { false }, seekPending { false }, panicPending { false }, offline { false }, chasePending { false };
    std::atomic<double> position { 0.0 }, seekTarget { 0.0 };
    std::atomic<double> positionStampMs { 0.0 }, ticksPerMs { 0.0 }, maxExtrapolationMs { 0.0 };
    std::atomic<float> metronomeLevel { 0.7f };
    std::atomic<int> currentPattern { 0 }, liveChannel { 0 };
    double playStartTick = 0.0;
    bool offlineSongMode = true;

    double sampleRate = 48000.0;
    int blockSize = 512;
    juce::AudioBuffer<float> chunkBuffer, directBuffer;
    DelayCompensator directCompensator;

    juce::MidiMessageCollector liveCollector;
    juce::MidiBuffer liveBuffer;

    juce::AbstractFifo previewFifo { 1024 };
    std::array<PreviewEvent, 1024> previewEvents;

    juce::AbstractFifo recordFifo { 4096 };
    std::array<RecordedEvent, 4096> recordEvents;

    juce::AudioBuffer<float> clickHigh, clickLow;
    std::array<std::pair<int, bool>, 16> pendingClicks;
    int numPendingClicks = 0;
    int clickPosition = -1;
    bool clickIsHigh = false;

    juce::TimeSliceThread previewThread { "Harmony file preview" };
    juce::AudioTransportSource previewTransport;
    std::unique_ptr<juce::AudioFormatReaderSource> previewSource;
    juce::AudioBuffer<float> previewBuffer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
}
