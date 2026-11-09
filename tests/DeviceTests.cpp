#include "engine/AudioEngine.h"
#include "engine/SnapshotBuilder.h"

namespace harmony::engine
{
class DeviceTests final : public juce::UnitTest
{
public:
    DeviceTests() : juce::UnitTest ("Realtime audio device", "Device") {}

    void runTest() override
    {
        beginTest ("Preview notes reach the master output through the real device");

        Project project;
        auto ch = project.addChannel (kind::sampler, "Blip");
        const int uid = ch[id::uid];

        AudioEngine engine;
        const auto error = engine.openDevice (nullptr);
        logMessage ("Device: " + (engine.getDeviceManager().getCurrentAudioDevice() != nullptr
                                     ? engine.getDeviceManager().getCurrentAudioDevice()->getName() : juce::String ("none"))
                    + " error='" + error + "' sr=" + juce::String (engine.getSampleRate()) + " bs=" + juce::String (engine.getBlockSize()));

        auto data = std::make_shared<SampleData>();
        data->sampleRate = 48000.0;
        data->audio.setSize (2, 48000);
        for (int i = 0; i < 48000; ++i)
            for (int c = 0; c < 2; ++c)
                data->audio.setSample (c, i, 0.3f * (float) std::sin (i * 0.05));

        engine.addChannelNode (std::make_unique<SamplerChannelNode> (uid, "sample|t", data));
        engine.publish (buildSnapshot (project, engine));
        juce::Thread::sleep (300);

        engine.getInsertNode (0)->meter.takeLeft();
        engine.sendPreviewNote (uid, 60, 0.9f, true);
        juce::Thread::sleep (300);
        const float channelPeak = engine.findChannelNode (uid)->meter.takeLeft();
        const float masterPeak = engine.getInsertNode (0)->meter.takeLeft();
        logMessage ("channel peak " + juce::String (channelPeak) + " master peak " + juce::String (masterPeak)
                    + " cpu " + juce::String (engine.getCpuLoad()));
        expect (channelPeak > 0.01f, "channel produced audio");
        expect (masterPeak > 0.01f, "master produced audio");
        engine.closeDevice();
    }
};

static DeviceTests deviceTests;
}
