#include "model/Project.h"

namespace harmony
{
class ModelTests final : public juce::UnitTest
{
public:
    ModelTests() : juce::UnitTest ("Project model", "Harmony") {}

    void runTest() override
    {
        beginTest ("Default project layout");
        {
            Project p;
            expectEquals (p.patternsTree().getNumChildren(), 1);
            expectEquals (p.tracksTree().getNumChildren(), defaultTrackCount);
            expectEquals (p.mixerTree().getNumChildren(), numMixerInserts + 1);
            expect (p.findInsert (0)[id::name].toString() == "Master");
            expectEquals ((int) p.ticksPerBar(), ppq * 4);
            expect (! p.isDirty());
        }

        beginTest ("Channels get their own mixer insert and are cleaned up");
        {
            Project p;
            auto a = p.addChannel (kind::sampler, "Kick");
            auto b = p.addChannel (kind::sampler, "Snare");
            expectEquals ((int) a[id::insert], 1);
            expectEquals ((int) b[id::insert], 2);
            expect (p.findInsert (1)[id::name].toString() == "Kick");
            expectEquals (p.selectedChannel(), (int) a[id::uid]);

            NoteData n;
            n.channel = b[id::uid];
            p.addNote (p.currentPattern(), n);
            auto audio = p.addChannel (kind::audio, "Vocal");
            p.addClip (p.tracksTree().getChild (0), kind::audio, audio[id::uid], 0, ppq * 4);

            p.removeChannel (b[id::uid]);
            p.removeChannel (audio[id::uid]);
            expectEquals (p.currentPattern().getNumChildren(), 0);
            expectEquals (p.tracksTree().getChild (0).getNumChildren(), 0);
            expectEquals (p.channelsTree().getNumChildren(), 1);
        }

        beginTest ("Undo and redo note edits");
        {
            Project p;
            auto ch = p.addChannel (kind::sampler, "Keys");
            p.getUndo().clearUndoHistory();
            p.beginTransaction ("add");
            NoteData n;
            n.channel = ch[id::uid];
            n.key = 64;
            auto note = p.addNote (p.currentPattern(), n);
            p.beginTransaction ("move");
            note.setProperty (id::key, 67, p.um());
            expectEquals ((int) note[id::key], 67);
            p.getUndo().undo();
            expectEquals ((int) note[id::key], 64);
            p.getUndo().undo();
            expectEquals (p.currentPattern().getNumChildren(), 0);
            p.getUndo().redo();
            expectEquals (p.currentPattern().getNumChildren(), 1);
        }

        beginTest ("Pattern and song length");
        {
            Project p;
            auto pattern = p.currentPattern();
            expectEquals ((juce::int64) p.patternLength (pattern), (juce::int64) ppq * 4);
            NoteData n;
            n.start = ppq * 5;
            n.length = ppq;
            p.addNote (pattern, n);
            expectEquals ((juce::int64) p.patternLength (pattern), (juce::int64) ppq * 8);
            p.addClip (p.tracksTree().getChild (3), kind::pattern, pattern[id::uid], ppq * 16, ppq * 8);
            expectEquals ((juce::int64) p.songLength(), (juce::int64) ppq * 24);
        }

        beginTest ("Song positions map back into pattern positions through clips");
        {
            Project p;
            auto pattern = p.currentPattern();
            const auto bar = p.ticksPerBar();
            auto clip = p.addClip (p.tracksTree().getChild (2), kind::pattern, pattern[id::uid], bar * 4, bar * 3);
            clip.setProperty (id::offset, ppq, nullptr);

            expect (! p.patternPositionAt (pattern[id::uid], (double) bar * 2).has_value(), "no clip before bar 5");
            expectWithinAbsoluteError (*p.patternPositionAt (pattern[id::uid], (double) bar * 4), (double) ppq, 1.0e-9);
            expectWithinAbsoluteError (*p.patternPositionAt (pattern[id::uid], (double) bar * 5 + ppq), (double) ppq * 2, 1.0e-9);
            expect (! p.patternPositionAt (pattern[id::uid], (double) bar * 7).has_value(), "clip ends after three bars");

            clip.setProperty (id::muted, true, nullptr);
            expect (! p.patternPositionAt (pattern[id::uid], (double) bar * 4).has_value(), "muted clips do not play");
        }

        beginTest ("Removing a pattern removes its clips but keeps the last pattern");
        {
            Project p;
            auto second = p.addPattern();
            p.addClip (p.tracksTree().getChild (0), kind::pattern, second[id::uid], 0, ppq * 4);
            p.setCurrentPattern (second[id::uid]);
            p.removePattern (second[id::uid]);
            expectEquals (p.patternsTree().getNumChildren(), 1);
            expectEquals (p.tracksTree().getChild (0).getNumChildren(), 0);
            p.removePattern (p.currentPattern()[id::uid]);
            expectEquals (p.patternsTree().getNumChildren(), 1);
        }

        beginTest ("Save and load round trip");
        {
            Project p;
            auto ch = p.addChannel (kind::soundfont, "Piano");
            ch.setProperty (id::sfPath, "C:/Sounds/Piano.sf2", nullptr);
            for (int i = 0; i < 50; ++i)
            {
                NoteData n;
                n.channel = ch[id::uid];
                n.key = 40 + i % 30;
                n.start = (juce::int64) i * 120;
                n.velocity = (float) (i % 10) / 10.0f;
                p.addNote (p.currentPattern(), n);
            }
            p.setBpm (93.5);

            juce::TemporaryFile temp (".hmy");
            expect (p.saveTo (temp.getFile()).wasOk());
            expect (! p.isDirty());

            Project q;
            expect (q.loadFrom (temp.getFile()).wasOk());
            expectWithinAbsoluteError (q.getBpm(), 93.5, 1.0e-9);
            expectEquals (q.currentPattern().getNumChildren(), 50);
            expect (q.findChannel (ch[id::uid])[id::sfPath].toString() == "C:/Sounds/Piano.sf2");
            expect (q.getState().isEquivalentTo (p.getState()));
            expect (q.allocateUid() > (int) ch[id::uid]);
        }

        beginTest ("Invalid and future files are rejected");
        {
            juce::TemporaryFile junk (".hmy");
            junk.getFile().replaceWithText ("not a project");
            Project p;
            expect (p.loadFrom (junk.getFile()).failed());

            auto future = Project::createDefaultState();
            future.setProperty (id::formatVersion, projectFormatVersion + 1, nullptr);
            juce::TemporaryFile newer (".hmy");
            expect (Project::writeStateToFile (future, newer.getFile()));
            expect (p.loadFrom (newer.getFile()).failed());
        }

        beginTest ("Older files missing sections are upgraded");
        {
            juce::ValueTree minimal (id::PROJECT);
            minimal.setProperty (id::formatVersion, 1, nullptr);
            juce::TemporaryFile file (".hmy");
            file.getFile().replaceWithText (minimal.toXmlString());

            Project p;
            expect (p.loadFrom (file.getFile()).wasOk());
            expectEquals (p.patternsTree().getNumChildren(), 1);
            expectEquals (p.mixerTree().getNumChildren(), numMixerInserts + 1);
            expectWithinAbsoluteError (p.getBpm(), 128.0, 1.0e-9);
        }
    }
};

static ModelTests modelTests;
}
