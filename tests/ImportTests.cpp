#include "model/FlpImport.h"
#include "model/MidiIO.h"
#include "model/FollowGenerator.h"
#include "engine/AudioEngine.h"
#include "engine/SnapshotBuilder.h"

namespace harmony
{
class ImportTests final : public juce::UnitTest
{
public:
    ImportTests() : juce::UnitTest ("Import and export", "Harmony") {}

    struct FlpWriter
    {
        juce::MemoryOutputStream body;

        void byte (int id, int v)          { body.writeByte ((char) id); body.writeByte ((char) v); }
        void word (int id, int v)          { body.writeByte ((char) id); body.writeShort ((short) v); }
        void dword (int id, juce::uint32 v) { body.writeByte ((char) id); body.writeInt ((int) v); }

        void data (int id, const juce::MemoryBlock& d)
        {
            body.writeByte ((char) id);
            auto n = d.getSize();
            do
            {
                auto b = (juce::uint8) (n & 0x7f);
                n >>= 7;
                if (n > 0) b |= 0x80;
                body.writeByte ((char) b);
            } while (n > 0);
            body.write (d.getData(), d.getSize());
        }

        void text (int id, const juce::String& s)
        {
            juce::MemoryBlock d;
            for (auto c : s)
            {
                const auto u = (juce::uint16) c;
                d.append (&u, 2);
            }
            const juce::uint16 zero = 0;
            d.append (&zero, 2);
            data (id, d);
        }

        void ascii (int id, const juce::String& s)
        {
            juce::MemoryBlock d (s.toRawUTF8(), s.getNumBytesAsUTF8() + 1);
            data (id, d);
        }

        juce::MemoryBlock finish (int ppqValue)
        {
            juce::MemoryOutputStream out;
            out.write ("FLhd", 4);
            out.writeInt (6);
            out.writeShort (0);
            out.writeShort (2);
            out.writeShort ((short) ppqValue);
            out.write ("FLdt", 4);
            out.writeInt ((int) body.getDataSize());
            out << body.getMemoryBlock();
            return out.getMemoryBlock();
        }
    };

    static juce::MemoryBlock note (juce::uint32 pos, juce::uint16 channel, juce::uint32 length, juce::uint16 key, juce::uint8 velocity)
    {
        juce::MemoryOutputStream o;
        o.writeInt ((int) pos);
        o.writeShort (0);
        o.writeShort ((short) channel);
        o.writeInt ((int) length);
        o.writeShort ((short) key);
        o.writeShort (0);
        for (int i = 0; i < 5; ++i) o.writeByte (i == 4 ? 64 : 0);
        o.writeByte ((char) velocity);
        o.writeByte (0);
        o.writeByte (0);
        return o.getMemoryBlock();
    }

    static juce::MemoryBlock playlistItem (juce::uint32 pos, int item, juce::uint32 length, int track, juce::int32 offset)
    {
        juce::MemoryOutputStream o;
        o.writeInt ((int) pos);
        o.writeShort ((short) 20480);
        o.writeShort ((short) item);
        o.writeInt ((int) length);
        o.writeShort ((short) (499 - track));
        o.writeShort (0);
        o.writeShort (0);
        o.writeShort (64);
        o.writeInt (0);
        o.writeInt (offset);
        o.writeInt (-1);
        for (int i = 32; i < 80; ++i) o.writeByte (0);
        return o.getMemoryBlock();
    }

    void runTest() override
    {
        beginTest ("A synthetic FL Studio project converts channels, notes, clips, tempo and mixer effects");
        {
            FlpWriter w;
            w.ascii (199, "25.2.0.5000");
            w.dword (156, 140000);
            w.byte (17, 3);
            w.byte (18, 4);
            w.word (65, 1);
            juce::MemoryBlock notes = note (0, 0, 96, 60, 100);
            notes.append (note (96, 0, 48, 64, 128).getData(), 24);
            notes.append (note (0, 1, 24, 36, 64).getData(), 24);
            w.data (224, notes);
            w.word (64, 0);
            w.byte (21, 2);
            w.text (201, "Sytrus");
            w.text (203, "Lead synth");
            w.byte (22, 3);
            w.word (64, 1);
            w.byte (21, 0);
            w.text (203, "Kick");
            w.text (196, "C:\\does\\not\\exist\\kick.wav");
            w.word (65, 1);
            w.text (193, "Intro");

            juce::MemoryBlock items = playlistItem (0, 20481, 384, 2, -1);
            items.append (playlistItem (384, 20481, 192, 2, 96).getData(), 80);
            w.data (233, items);

            w.text (239, "unused");
            w.data (236, juce::MemoryBlock (12, true));
            w.text (201, "Fruity Limiter");
            w.word (98, 9);
            w.text (204, "Synth bus");
            w.data (236, juce::MemoryBlock (12, true));
            w.text (201, "Fruity Parametric EQ 2");
            w.word (98, 0);
            w.text (201, "Some Unknown FX");
            w.word (98, 1);

            juce::TemporaryFile temp (".flp");
            temp.getFile().replaceWithData (w.finish (96).getData(), w.finish (96).getSize());

            flp::Options options;
            options.findEffectXml = [] (const juce::String& plugin) { return flp::builtinEffectFor (plugin).isNotEmpty() ? "<PLUGIN name=\"" + plugin + "\"/>" : juce::String(); };
            flp::Report report;
            juce::String error;
            auto state = flp::convert (temp.getFile(), options, report, error);
            expect (state.isValid(), error);

            Project p;
            p.replaceState (state);
            expectWithinAbsoluteError (p.getBpm(), 140.0, 1.0e-9);
            expectEquals (p.getNumerator(), 3);
            expectEquals (p.channelsTree().getNumChildren(), 2);
            expectEquals (report.notes, 3);
            expectEquals (report.patternClips, 2);
            expectEquals (report.effects, 2);
            expect (report.skippedEffects.contains ("Some Unknown FX"));
            expect (report.placeholderInstruments.size() == 1 && report.placeholderInstruments[0].contains ("Sytrus"));
            expect (report.missingSamples.size() == 1);

            auto lead = p.channelsTree().getChild (0);
            expectEquals (lead[id::name].toString(), juce::String ("Lead synth"));
            expectEquals ((int) lead[id::insert], 2);
            expectEquals (p.findInsert (2)[id::name].toString(), juce::String ("Insert 2"));
            expectEquals (p.findInsert (1)[id::name].toString(), juce::String ("Synth bus"));
            expectEquals (p.findInsert (0).getNumChildren(), 1);

            auto pattern = p.currentPattern();
            expectEquals (pattern[id::name].toString(), juce::String ("Intro"));
            auto first = Project::readNote (pattern.getChild (0));
            expectEquals ((int) first.length, ppq);
            expectEquals (first.key, 60);
            auto second = Project::readNote (pattern.getChild (1));
            expectEquals ((int) second.start, ppq);
            expectWithinAbsoluteError (second.velocity, 1.0f, 1.0e-6f);

            auto clip = p.tracksTree().getChild (2).getChild (1);
            expectEquals ((int) (juce::int64) clip[id::start], ppq * 4);
            expectEquals ((int) (juce::int64) clip[id::offset], ppq);
        }

        beginTest ("Garbage and truncated files are rejected without crashing");
        {
            juce::TemporaryFile temp (".flp");
            temp.getFile().replaceWithText ("not an fl project");
            flp::Report report;
            juce::String error;
            expect (! flp::convert (temp.getFile(), {}, report, error).isValid());
            expect (error.isNotEmpty());

            FlpWriter w;
            w.ascii (199, "21.0.0.1");
            auto data = w.finish (96);
            data.setSize (data.getSize() - 4);
            temp.getFile().replaceWithData (data.getData(), data.getSize());
            error = {};
            juce::ignoreUnused (flp::convert (temp.getFile(), {}, report, error));
        }

        beginTest ("Every FL Studio demo project installed on this machine imports");
        {
            int imported = 0;
            const auto options = flp::defaultOptions();
            for (auto& install : options.factoryDataFolders)
            {
                for (const auto& entry : juce::RangedDirectoryIterator (install.getChildFile ("Data").getChildFile ("Demo projects"), true, "*.flp"))
                {
                    flp::Report report;
                    juce::String error;
                    const auto state = flp::convert (entry.getFile(), options, report, error);
                    expect (state.isValid(), entry.getFile().getFileName() + ": " + error);
                    expect (report.notes > 0 && report.patterns > 0, entry.getFile().getFileName() + " has notes");
                    expect (report.patternClips + report.audioClips > 0, entry.getFile().getFileName() + " has an arrangement");
                    if (imported < 4)
                        logMessage ("  " + entry.getFile().getFileNameWithoutExtension() + ": " + report.summary()
                                    + " Placeholders: " + juce::String (report.placeholderInstruments.size())
                                    + ", missing samples: " + juce::String (report.missingSamples.size()));
                    ++imported;
                }
                if (imported > 0)
                    break;
            }
            logMessage ("FL Studio demo projects imported: " + juce::String (imported));
        }

        beginTest ("MIDI export and import round trip");
        {
            Project p;
            auto ch = p.addChannel (kind::sampler, "Keys");
            NoteData a;
            a.channel = ch[id::uid];
            a.key = 62;
            a.start = ppq / 2;
            a.length = ppq * 3;
            a.velocity = 0.5f;
            p.addNote (p.currentPattern(), a);

            auto midi = midi::exportPattern (p, p.currentPattern());
            juce::TemporaryFile temp (".mid");
            expect (midi::writeFile (midi, temp.getFile()));
            auto back = midi::readFile (temp.getFile());
            expect (back.has_value());

            auto tracks = midi::importTracks (*back);
            expectEquals ((int) tracks.size(), 1);
            expectEquals (tracks[0].name, juce::String ("Keys"));
            const auto& n = tracks[0].notes.front();
            expectEquals (n.key, 62);
            expectEquals ((int) n.start, ppq / 2);
            expectEquals ((int) n.length, ppq * 3);
            expectWithinAbsoluteError (n.velocity, 0.5f, 0.01f);

            juce::MidiFile coarse;
            coarse.setTicksPerQuarterNote (96);
            juce::MidiMessageSequence seq;
            seq.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 100), 48.0);
            seq.addEvent (juce::MidiMessage::noteOff (1, 40), 144.0);
            coarse.addTrack (seq);
            auto scaled = midi::flatten (midi::importTracks (coarse));
            expectEquals ((int) scaled.front().start, ppq / 2);
            expectEquals ((int) scaled.front().length, ppq);
        }

        beginTest ("Song export expands clips into absolute notes");
        {
            Project p;
            auto ch = p.addChannel (kind::sampler, "Bass");
            NoteData n;
            n.channel = ch[id::uid];
            p.addNote (p.currentPattern(), n);
            const auto bar = p.ticksPerBar();
            p.addClip (p.tracksTree().getChild (0), kind::pattern, p.currentPattern()[id::uid], bar * 2, bar * 2);

            auto tracks = midi::importTracks (midi::exportSong (p));
            expectEquals ((int) tracks.size(), 1);
            expectEquals ((int) tracks[0].notes.size(), 2);
            expectEquals ((int) tracks[0].notes[0].start, (int) bar * 2);
            expectEquals ((int) tracks[0].notes[1].start, (int) bar * 3);
        }

        beginTest ("Follow chords finds the progression and writes parts that fit it");
        {
            std::vector<NoteData> chords;
            auto chord = [&chords] (juce::int64 start, std::initializer_list<int> keys)
            {
                for (auto k : keys)
                {
                    NoteData n;
                    n.key = k;
                    n.start = start;
                    n.length = ppq * 4;
                    chords.push_back (n);
                }
            };
            chord (0, { 57, 60, 64 });
            chord (ppq * 4, { 53, 57, 60 });
            chord (ppq * 8, { 48, 52, 55 });
            chord (ppq * 12, { 55, 59, 62 });

            const auto segments = follow::analyse (chords, ppq * 16, ppq);
            expectEquals ((int) segments.size(), 4);
            const int roots[] = { 9, 5, 0, 7 };
            for (size_t i = 0; i < segments.size(); ++i)
            {
                expectEquals (segments[i].root, roots[i]);
                expectEquals ((int) segments[i].start, (int) i * ppq * 4);
            }

            for (int style = 0; style < 4; ++style)
            {
                follow::Settings s;
                s.style = (follow::Style) style;
                s.octave = style <= 1 ? 2 : 4;
                s.scaleRoot = 9;
                s.scale = "Minor";
                s.channel = 42;
                const auto notes = follow::generate (segments, s, ppq);
                expect (! notes.empty());

                for (auto& n : notes)
                {
                    expectEquals (n.channel, 42);
                    expect (n.start >= 0 && n.start + n.length <= ppq * 16, "notes stay inside the progression");

                    auto segment = std::find_if (segments.begin(), segments.end(), [&n] (auto& seg) { return n.start >= seg.start && n.start < seg.end; });
                    expect (segment != segments.end());
                    if (style <= 1 && segment != segments.end())
                        expect (n.key % 12 == segment->root || n.key % 12 == (segment->root + 7) % 12, "bass plays roots and fifths");
                }
            }
        }

        beginTest ("Playback that starts inside a long note plays that note");
        {
            Project project;
            project.setBpm (120.0);
            auto ch = project.addChannel (kind::sampler, "Pad");
            const int uid = ch[id::uid];
            NoteData n;
            n.channel = uid;
            n.length = ppq * 4;
            project.addNote (project.currentPattern(), n);

            engine::AudioEngine engine;
            engine.prepareStandalone (48000.0, 512);
            auto sample = std::make_shared<engine::SampleData>();
            sample->sampleRate = 48000.0;
            sample->audio.setSize (2, 96000);
            for (int i = 0; i < 96000; ++i)
                for (int c = 0; c < 2; ++c)
                    sample->audio.setSample (c, i, 0.5f * std::sin ((float) i * 0.05f));
            engine.addChannelNode (std::make_unique<engine::SamplerChannelNode> (uid, "sample|pad", sample));
            engine.findChannelNode (uid)->gain.volume = engine::unityFader;
            engine.publish (engine::buildSnapshot (project, engine));

            engine.beginOffline (48000.0, 512, ppq * 2, false);
            juce::AudioBuffer<float> block (2, 512);
            float level = 0.0f;
            for (int b = 0; b < 8; ++b)
            {
                engine.processOffline (block, 512);
                level = juce::jmax (level, block.getMagnitude (0, 0, 512));
            }
            engine.stopOfflineTransport();
            engine.endOffline();
            expect (level > 0.1f, "the note that started before the playhead sounds");
        }
    }
};

static ImportTests importTests;
}
