#include "engine/AudioEngine.h"
#include "engine/SnapshotBuilder.h"
#include "engine/PluginManager.h"
#include "engine/BuiltinEffects.h"
#include "model/Automation.h"

namespace harmony::engine
{
class EffectTests final : public juce::UnitTest
{
public:
    EffectTests() : juce::UnitTest ("Effects and automation", "Harmony") {}

    static std::shared_ptr<const SampleData> makeTone (double sampleRate, double seconds, float level)
    {
        auto data = std::make_shared<SampleData>();
        data->sampleRate = sampleRate;
        const int n = (int) (sampleRate * seconds);
        data->audio.setSize (2, n);
        for (int i = 0; i < n; ++i)
        {
            const float v = level * (float) std::sin (juce::MathConstants<double>::twoPi * 220.0 * i / sampleRate);
            data->audio.setSample (0, i, v);
            data->audio.setSample (1, i, v);
        }
        return data;
    }

    static std::vector<float> render (AudioEngine& engine, int blocks, bool song)
    {
        std::vector<float> out;
        engine.beginOffline (48000.0, 512, 0, song);
        juce::AudioBuffer<float> block (2, 512);
        for (int b = 0; b < blocks; ++b)
        {
            engine.processOffline (block, 512);
            for (int i = 0; i < 512; ++i)
                out.push_back (block.getSample (0, i));
        }
        engine.stopOfflineTransport();
        engine.endOffline();
        return out;
    }

    static float rms (const std::vector<float>& data, size_t start, size_t end)
    {
        end = juce::jmin (end, data.size());
        double sum = 0.0;
        for (size_t i = start; i < end; ++i)
            sum += (double) data[i] * data[i];
        return end > start ? (float) std::sqrt (sum / (double) (end - start)) : 0.0f;
    }

    static float peak (const std::vector<float>& data, size_t start)
    {
        float p = 0.0f;
        for (size_t i = start; i < data.size(); ++i)
            p = juce::jmax (p, std::abs (data[i]));
        return p;
    }

    struct Rig
    {
        Project project;
        AudioEngine engine;
        int channel = 0;

        explicit Rig (int insert, float level = 0.5f, juce::int64 noteLength = ppq * 16)
        {
            project.setBpm (120.0);
            auto ch = project.addChannel (kind::sampler, "Tone");
            ch.setProperty (id::insert, insert, nullptr);
            channel = ch[id::uid];

            NoteData n;
            n.channel = channel;
            n.length = noteLength;
            project.currentPattern().setProperty (id::steps, 64, nullptr);
            project.addNote (project.currentPattern(), n);
            project.addClip (project.tracksTree().getChild (0), kind::pattern, project.currentPattern()[id::uid], 0, noteLength);

            engine.prepareStandalone (48000.0, 512);
            engine.addChannelNode (std::make_unique<SamplerChannelNode> (channel, "sample|tone", makeTone (48000.0, 10.0, level)));
            engine.findChannelNode (channel)->gain.volume = unityFader;
            engine.getInsertNode (insert)->gain.volume = unityFader;
            engine.getInsertNode (0)->gain.volume = unityFader;
        }

        FxSlotNode* addEffect (int insert, const juce::String& effectId, std::function<void (builtin::Effect&)> configure = {})
        {
            auto desc = builtin::describe (*builtin::find (effectId));
            auto slot = project.addSlot (insert, 0, PluginManager::toXmlString (desc));
            auto instance = builtin::create (effectId);
            if (configure)
                configure (*dynamic_cast<builtin::Effect*> (instance.get()));
            engine.addSlotNode (std::make_unique<FxSlotNode> ((int) slot[id::uid], "fx|" + desc.createIdentifierString(), std::move (instance)));
            return engine.findSlotNode (slot[id::uid]);
        }

        void publish() { engine.publish (buildSnapshot (project, engine)); }
    };

    static void setParam (builtin::Effect& e, const juce::String& id, float value)
    {
        if (auto* p = e.state.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (value));
    }

    void runTest() override
    {
        beginTest ("Every built-in effect loads through the plugin manager, processes cleanly and restores state");
        {
            PluginManager manager (juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("HarmonyEffectTests"));
            expectEquals (PluginManager::getBuiltinEffects().size(), (int) builtin::all().size());

            for (auto& desc : PluginManager::getBuiltinEffects())
            {
                expect (PluginManager::isUsableEffect (desc));
                expect (manager.findByIdentifier (desc.createIdentifierString()).has_value(), desc.name + " is findable");

                juce::String error;
                auto instance = manager.createInstanceSync (desc, 48000.0, 256, error);
                expect (instance != nullptr, desc.name + ": " + error);
                if (instance == nullptr)
                    continue;

                instance->prepareToPlay (48000.0, 256);
                juce::Random rng (42);
                juce::AudioBuffer<float> buffer (2, 256);
                juce::MidiBuffer midi;
                bool finite = true;
                for (int b = 0; b < 200; ++b)
                {
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 256; ++i)
                            buffer.setSample (ch, i, (rng.nextFloat() * 2.0f - 1.0f) * 0.5f);
                    instance->processBlock (buffer, midi);
                    for (int ch = 0; ch < 2; ++ch)
                        for (int i = 0; i < 256; ++i)
                            finite = finite && std::isfinite (buffer.getSample (ch, i));
                }
                expect (finite, desc.name + " output stays finite");

                auto& params = instance->getParameters();
                expect (! params.isEmpty(), desc.name + " exposes automatable parameters");
                int floatIndex = 0;
                while (floatIndex < params.size() && dynamic_cast<juce::AudioParameterFloat*> (params[floatIndex]) == nullptr)
                    ++floatIndex;
                params[floatIndex]->setValueNotifyingHost (0.123f);
                juce::MemoryBlock saved;
                instance->getStateInformation (saved);

                auto restored = builtin::create (desc.fileOrIdentifier.fromFirstOccurrenceOf ("harmony:", false, false));
                restored->setStateInformation (saved.getData(), (int) saved.getSize());
                expectWithinAbsoluteError (restored->getParameters()[floatIndex]->getValue(), 0.123f, 0.01f, desc.name + " state round trip");
            }
        }

        beginTest ("Plugin lists hide ARA-only plugins and generators from the effect slots");
        {
            juce::PluginDescription ara;
            ara.category = "OnlyARA";
            ara.numInputChannels = ara.numOutputChannels = 2;
            expect (! PluginManager::isUsableEffect (ara));
            expect (! PluginManager::isUsableInstrument (ara));

            juce::PluginDescription generator;
            generator.numInputChannels = 0;
            generator.numOutputChannels = 2;
            expect (! PluginManager::isUsableEffect (generator));
            expect (PluginManager::isUsableInstrument (generator));
        }

        beginTest ("Effects on the master insert shape the final mix");
        {
            Rig dry (0);
            dry.publish();
            const auto reference = render (dry.engine, 100, true);

            Rig wet (0);
            wet.addEffect (0, "utility", [] (builtin::Effect& e) { setParam (e, "gain", -24.0f); });
            wet.publish();
            const auto quiet = render (wet.engine, 100, true);

            const float ratio = rms (quiet, 10000, quiet.size()) / juce::jmax (1.0e-6f, rms (reference, 10000, reference.size()));
            expectWithinAbsoluteError (ratio, juce::Decibels::decibelsToGain (-24.0f), 0.01f);

            Rig routed (3);
            routed.addEffect (0, "utility", [] (builtin::Effect& e) { setParam (e, "gain", -24.0f); });
            routed.publish();
            const auto viaInsert = render (routed.engine, 100, true);
            expect (rms (viaInsert, 10000, viaInsert.size()) < rms (reference, 10000, reference.size()) * 0.1f,
                    "master effect also processes audio arriving through mixer inserts");
        }

        beginTest ("Limiter on the master never exceeds its ceiling");
        {
            Rig rig (0, 1.0f);
            rig.engine.findChannelNode (rig.channel)->gain.volume = 1.25f;
            rig.addEffect (0, "limiter", [] (builtin::Effect& e) { setParam (e, "gain", 12.0f); setParam (e, "ceiling", -1.0f); });
            rig.publish();
            const auto out = render (rig.engine, 120, true);
            expect (peak (out, 0) <= juce::Decibels::decibelsToGain (-1.0f) + 1.0e-4f, "peak " + juce::String (peak (out, 0)));
            expect (rms (out, 20000, out.size()) > 0.3f, "limited signal still loud");
        }

        beginTest ("Automation targets, curves and evaluation");
        {
            for (auto text : { "ch:5:vol", "ch:5:pan", "mix:3:vol", "mix:0:pan", "fx:12:param:4", "ch:7:param:0" })
                expectEquals (automation::toString (automation::parse (text)), juce::String (text));
            expect (! automation::parse ("fx:12:param").isValid());
            expect (! automation::parse ("nonsense").isValid());

            std::vector<automation::Point> points { { 0.0, 0.0f, 0.0f }, { 100.0, 1.0f, 0.8f }, { 200.0, 0.5f, 0.0f } };
            expectWithinAbsoluteError (automation::valueAt (points, -10.0), 0.0f, 1.0e-6f);
            expectWithinAbsoluteError (automation::valueAt (points, 50.0), 0.5f, 1.0e-6f);
            expectWithinAbsoluteError (automation::valueAt (points, 500.0), 0.5f, 1.0e-6f);
            expect (automation::valueAt (points, 150.0) < 0.7f, "positive tension moves towards the next point sooner");
        }

        beginTest ("Automation clips drive channel volume and plugin parameters in song mode");
        {
            Rig rig (0);
            const auto bar = rig.project.ticksPerBar();
            auto track = rig.project.tracksTree().getChild (1);
            auto clip = automation::addClip (rig.project, track, { automation::Kind::channelVolume, rig.channel, -1 }, 0, bar * 2, 0.0f, "Fade in");
            automation::writePoints (clip, { { 0.0, 0.0f, 0.0f }, { (double) bar * 2, automation::normalisedFromFader (unityFader), 0.0f } }, nullptr);

            auto* fx = rig.addEffect (0, "filter");
            const int cutoffIndex = 1;
            const juce::ValueTree slot = rig.project.findInsert (0).getChild (0);
            auto paramClip = automation::addClip (rig.project, rig.project.tracksTree().getChild (2),
                                                  { automation::Kind::slotParameter, (int) slot[id::uid], cutoffIndex }, 0, bar, 0.25f, "Cutoff");
            rig.publish();

            expectEquals ((int) rig.engine.getPublishedSnapshot()->automation.size(), 2);
            const auto out = render (rig.engine, 360, true);
            const size_t samplesPerBar = (size_t) (48000.0 * 2.0);
            expect (rms (out, 0, samplesPerBar / 4) < rms (out, samplesPerBar * 3 / 2, samplesPerBar * 2) * 0.5f, "volume ramps up");
            expectWithinAbsoluteError (fx->getPlugin()->getParameters()[cutoffIndex]->getValue(), 0.25f, 1.0e-3f);
            juce::ignoreUnused (paramClip);

            rig.engine.clearAutomation();
            expect (! rig.engine.findChannelNode (rig.channel)->gain.isAutomated());
        }

        beginTest ("Removing a channel removes its automation clips");
        {
            Project p;
            auto ch = p.addChannel (kind::sampler, "X");
            automation::addClip (p, p.tracksTree().getChild (0), { automation::Kind::channelPan, (int) ch[id::uid], -1 }, 0, 960, 0.5f, "Pan");
            expectEquals (p.tracksTree().getChild (0).getNumChildren(), 1);
            p.removeChannel (ch[id::uid]);
            expectEquals (p.tracksTree().getChild (0).getNumChildren(), 0);
        }
    }
};

static EffectTests effectTests;
}
