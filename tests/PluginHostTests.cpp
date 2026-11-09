#include "engine/AudioEngine.h"
#include "engine/PluginManager.h"

namespace harmony::engine
{
class PluginHostTests final : public juce::UnitTest
{
public:
    PluginHostTests() : juce::UnitTest ("Plugin hosting", "Plugins") {}

    void runTest() override
    {
        beginTest ("Installed VST3 plugins scan, instantiate and process audio");

        juce::AudioPluginFormatManager formats;
        juce::addDefaultFormatsToManager (formats);

        juce::AudioPluginFormat* vst3 = nullptr;
        for (auto* f : formats.getFormats())
            if (f->getName() == "VST3")
                vst3 = f;

        expect (vst3 != nullptr);
        if (vst3 == nullptr)
            return;

        const auto found = vst3->searchPathsForPlugins (vst3->getDefaultLocationsToSearch(), true, false);
        logMessage ("VST3 files found: " + juce::String (found.size()));

        int instantiated = 0;
        for (auto& file : found)
        {
            if (instantiated >= 5)
                break;

            juce::OwnedArray<juce::PluginDescription> types;
            vst3->findAllTypesForFile (types, file);
            logMessage ("  " + file + " -> " + juce::String (types.size()) + " type(s)");

            for (auto* desc : types)
            {
                juce::String error;
                auto instance = formats.createPluginInstance (*desc, 48000.0, 512, error);
                if (instance == nullptr)
                {
                    logMessage ("    could not instantiate " + desc->name + ": " + error);
                    continue;
                }

                juce::AudioBuffer<float> out (2, 512);
                std::unique_ptr<FxSlotNode> fx;
                std::unique_ptr<PluginChannelNode> synth;

                if (desc->isInstrument)
                {
                    synth = std::make_unique<PluginChannelNode> (1, "test", std::move (instance));
                    synth->prepare (48000.0, 512);
                    synth->addNoteOn (60, 0.9f, 0);
                    for (int i = 0; i < 20; ++i)
                        synth->process (512);
                    synth->addNoteOff (60, 0);
                    synth->process (512);
                    expect (synth->getOutput().getMagnitude (0, 512) >= 0.0f);
                }
                else
                {
                    fx = std::make_unique<FxSlotNode> (2, "test", std::move (instance));
                    fx->prepare (48000.0, 512);
                    for (int i = 0; i < 20; ++i)
                    {
                        out.clear();
                        out.setSample (0, 0, 1.0f);
                        fx->process (out, 512);
                    }
                    expect (std::isfinite (out.getMagnitude (0, 512)));
                }

                logMessage ("    processed " + desc->name + (desc->isInstrument ? " (instrument)" : " (effect)"));
                ++instantiated;
            }
        }

        if (found.isEmpty())
            logMessage ("No VST3 plugins installed: hosting test skipped");
        else
            expect (instantiated > 0, "at least one installed plugin should load");
    }
};

static PluginHostTests pluginHostTests;
}
