#include "engine/AudioEngine.h"
#include "engine/SnapshotBuilder.h"
#include "engine/Renderer.h"

namespace harmony::engine
{
class EngineTests final : public juce::UnitTest
{
public:
    EngineTests() : juce::UnitTest ("Audio engine", "Harmony") {}

    static std::shared_ptr<const SampleData> makeBlip (double sampleRate, double seconds)
    {
        auto data = std::make_shared<SampleData>();
        data->sampleRate = sampleRate;
        const int n = (int) (sampleRate * seconds);
        data->audio.setSize (2, n);
        for (int i = 0; i < n; ++i)
        {
            const float v = 0.5f * (float) std::sin (juce::MathConstants<double>::twoPi * 440.0 * i / sampleRate);
            data->audio.setSample (0, i, v);
            data->audio.setSample (1, i, v);
        }
        return data;
    }

    static float rms (const std::vector<float>& data, int start, int end)
    {
        double sum = 0.0;
        start = juce::jlimit (0, (int) data.size(), start);
        end = juce::jlimit (start, (int) data.size(), end);
        for (int i = start; i < end; ++i)
            sum += data[(size_t) i] * data[(size_t) i];
        return end > start ? (float) std::sqrt (sum / (end - start)) : 0.0f;
    }

    std::vector<float> renderOffline (AudioEngine& engine, juce::int64 endTick, double bpm, double sampleRate, bool song)
    {
        engine.beginOffline (sampleRate, 512, 0, song);
        const int total = (int) std::ceil ((double) endTick * sampleRate * 60.0 / (bpm * ppq));
        std::vector<float> out;
        juce::AudioBuffer<float> block (2, 512);

        for (int done = 0; done < total; done += 512)
        {
            const int n = juce::jmin (512, total - done);
            juce::AudioBuffer<float> view (block.getArrayOfWritePointers(), 2, n);
            engine.processOffline (view, n);
            for (int i = 0; i < n; ++i)
                out.push_back (view.getSample (0, i));
        }

        engine.stopOfflineTransport();
        return out;
    }

    void runTest() override
    {
        beginTest ("Fader and pan laws");
        {
            expectWithinAbsoluteError (faderToGain (unityFader), 1.0f, 1.0e-6f);
            expectEquals (faderToGain (0.0f), 0.0f);
            float l, r;
            panGains (0.0f, l, r);
            expect (l == 1.0f && r == 1.0f);
            panGains (-1.0f, l, r);
            expect (l == 1.0f && r == 0.0f);
        }

        beginTest ("Event sorting puts note-offs before note-ons");
        {
            std::vector<MidiEvent> events { { 100, 60, 100 }, { 100, 60, 0 }, { 50, 62, 90 } };
            sortEvents (events);
            expectEquals ((int) events[0].tick, 50);
            expect (events[1].isNoteOff());
            expect (! events[2].isNoteOff());
        }

        beginTest ("Pattern expansion respects clips, offsets and looping");
        {
            Project p;
            auto ch = p.addChannel (kind::sampler, "S");
            auto pattern = p.currentPattern();
            for (int i = 0; i < 4; ++i)
            {
                NoteData n;
                n.channel = ch[id::uid];
                n.start = (juce::int64) i * ppq;
                n.length = ppq / 2;
                p.addNote (pattern, n);
            }

            const auto length = p.patternLength (pattern);
            auto looped = expandPatternNotes (pattern, ch[id::uid], length, 0.0f, 1000, length * 2, 0, true);
            expectEquals ((int) looped.size(), 16);

            auto trimmed = expandPatternNotes (pattern, ch[id::uid], length, 0.0f, 0, ppq * 2, ppq, true);
            sortEvents (trimmed);
            expectEquals ((int) trimmed.size(), 4);
            expectEquals ((int) trimmed.front().tick, 0);

            auto other = expandPatternNotes (pattern, (int) ch[id::uid] + 999, length, 0.0f, 0, length, 0, false);
            expect (other.empty());

            auto swung = expandPatternNotes (pattern, ch[id::uid], length, 1.0f, 0, length, 0, false);
            expectEquals ((int) swung.size(), 8);
        }

        beginTest ("Delay compensation shifts audio by the requested amount");
        {
            DelayCompensator pdc;
            pdc.prepare (4096, 256);
            juce::AudioBuffer<float> buffer (2, 256);
            buffer.clear();
            buffer.setSample (0, 0, 1.0f);
            pdc.process (buffer, 256, 100);
            expectEquals (buffer.getSample (0, 100), 1.0f);
            expectEquals (buffer.getSample (0, 0), 0.0f);
        }

        beginTest ("Offline render plays sampler notes at the right time and honours mute");
        {
            Project project;
            project.setBpm (120.0);
            auto ch = project.addChannel (kind::sampler, "Blip");
            const int uid = ch[id::uid];

            NoteData n;
            n.channel = uid;
            n.start = 0;
            n.length = ppq / 4;
            project.addNote (project.currentPattern(), n);
            n.start = ppq * 2;
            project.addNote (project.currentPattern(), n);

            AudioEngine engine;
            const double sr = 48000.0;
            engine.prepareStandalone (sr, 512);
            auto node = std::make_unique<SamplerChannelNode> (uid, "sample|test", makeBlip (sr, 0.1));
            engine.addChannelNode (std::move (node));
            engine.findChannelNode (uid)->gain.volume = unityFader;
            engine.publish (buildSnapshot (project, engine));

            const auto out = renderOffline (engine, ppq * 4, 120.0, sr, false);
            const int beat = (int) (sr * 0.5);
            expect (rms (out, 0, 4000) > 0.1f, "first note should sound");
            expect (rms (out, 6000, beat * 2 - 100) < 1.0e-4f, "silence between notes");
            expect (rms (out, beat * 2, beat * 2 + 4000) > 0.1f, "second note at beat 3");

            engine.findChannelNode (uid)->gain.muted = true;
            const auto muted = renderOffline (engine, ppq * 4, 120.0, sr, false);
            expect (rms (muted, 4000, (int) muted.size()) < 1.0e-4f, "muted channel is silent");
            engine.endOffline();
        }

        beginTest ("Song mode renders audio clips from their offset");
        {
            Project project;
            project.setBpm (120.0);
            auto ch = project.addChannel (kind::audio, "Loop");
            const int uid = ch[id::uid];
            project.addClip (project.tracksTree().getChild (0), kind::audio, uid, ppq * 4, ppq);

            AudioEngine engine;
            const double sr = 44100.0;
            engine.prepareStandalone (sr, 512);
            engine.addChannelNode (std::make_unique<SamplerChannelNode> (uid, "sample|clip", makeBlip (sr, 2.0)));
            engine.publish (buildSnapshot (project, engine));

            const auto out = renderOffline (engine, ppq * 6, 120.0, sr, true);
            const int barSamples = (int) (sr * 2.0);
            expect (rms (out, 0, barSamples - 10) < 1.0e-4f, "nothing before the clip");
            expect (rms (out, barSamples + 100, barSamples + (int) (sr * 0.45)) > 0.1f, "clip audible");
            expect (rms (out, barSamples + (int) (sr * 0.55), (int) out.size()) < 1.0e-4f, "clip ends after its length");
            engine.endOffline();
        }

        beginTest ("Renderer writes a valid WAV file");
        {
            Project project;
            project.setBpm (120.0);
            auto ch = project.addChannel (kind::sampler, "Blip");
            const int uid = ch[id::uid];
            NoteData n;
            n.channel = uid;
            project.addNote (project.currentPattern(), n);

            AudioEngine engine;
            engine.prepareStandalone (48000.0, 512);
            engine.addChannelNode (std::make_unique<SamplerChannelNode> (uid, "sample|r", makeBlip (48000.0, 0.2)));
            engine.publish (buildSnapshot (project, engine));

            juce::TemporaryFile out (".wav");
            RenderSettings s;
            s.output = out.getFile();
            s.format = "WAV";
            s.bitDepth = 16;
            s.sampleRate = 44100.0;
            s.songMode = false;
            s.endTick = project.patternLength (project.currentPattern());
            s.bpm = 120.0;
            s.tailSeconds = 0.5;
            s.normalize = true;

            juce::WaitableEvent finished;
            Renderer renderer (engine, s, [] (juce::Result) {});
            renderer.start();
            for (int i = 0; i < 400 && renderer.getProgress() < 1.0f; ++i)
                juce::Thread::sleep (10);
            juce::Thread::sleep (50);

            juce::WavAudioFormat wav;
            std::unique_ptr<juce::AudioFormatReader> reader (wav.createReaderFor (new juce::FileInputStream (out.getFile()), true));
            expect (reader != nullptr, "render produced a readable WAV");
            if (reader != nullptr)
            {
                expectEquals ((int) reader->bitsPerSample, 16);
                expectWithinAbsoluteError (reader->sampleRate, 44100.0, 0.1);
                expectWithinAbsoluteError ((double) reader->lengthInSamples, 44100.0 * 2.5, 2.0);
                juce::AudioBuffer<float> audio (2, (int) reader->lengthInSamples);
                reader->read (&audio, 0, audio.getNumSamples(), 0, true, true);
                expectWithinAbsoluteError (audio.getMagnitude (0, audio.getNumSamples()), juce::Decibels::decibelsToGain (-0.3f), 0.01f);
            }
            engine.endOffline();
        }
    }
};

static EngineTests engineTests;
}
