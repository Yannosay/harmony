#include "model/MusicTheory.h"
#include "model/ChordGenerator.h"
#include "model/DrumGenerator.h"

namespace harmony
{
class TheoryTests final : public juce::UnitTest
{
public:
    TheoryTests() : juce::UnitTest ("Music theory and generators", "Harmony") {}

    void runTest() override
    {
        beginTest ("Note names and scales");
        {
            expect (theory::noteName (60) == "C4");
            expect (theory::noteName (69) == "A4");
            const auto& minor = theory::findScale ("Minor");
            expect (theory::isInScale (69, 9, minor));
            expect (theory::isInScale (72, 9, minor));
            expect (! theory::isInScale (73, 9, minor));
            expectEquals (theory::snapToScale (61, 0, theory::findScale ("Major")), 60);
            expect (theory::findScale ("does not exist").name == "Minor");
        }

        beginTest ("Chord detection");
        {
            expect (theory::detectChord ({ 57, 60, 64 }) == "Am");
            expect (theory::detectChord ({ 60, 64, 67, 71 }) == "Cmaj7");
            expect (theory::detectChord ({ 64, 67, 72 }) == "C/E");
            expect (theory::detectChord ({ 55, 59, 62, 65 }) == "G7");
            expect (theory::detectChord ({ 60, 61, 62 }).isEmpty());
        }

        beginTest ("Pop progression in C major");
        {
            ChordGenSettings s;
            s.root = 0;
            s.scale = "Major";
            s.progression = 1;
            s.voiceLeading = false;
            s.addBass = false;
            const auto r = generateChords (s, ppq * 4, ppq);
            expectEquals (r.chordNames.size(), 4);
            expect (r.chordNames[0] == "C");
            expect (r.chordNames[1] == "G");
            expect (r.chordNames[2] == "Am");
            expect (r.chordNames[3] == "F");
            expect (r.numerals[2] == "vi");
            expectEquals ((int) r.notes.size(), 12);
            expectEquals ((juce::int64) r.length, (juce::int64) ppq * 16);
        }

        beginTest ("Voice leading keeps chords close and in range");
        {
            const std::vector<int> previous { 60, 64, 67 };
            const auto led = leadVoicing (previous, { 65, 69, 72 }, 50, 84);
            int movement = 0;
            for (size_t i = 0; i < led.size(); ++i)
                movement += std::abs (led[i] - previous[i]);
            expect (movement <= 5);
            expect (led.front() >= 50 && led.back() <= 84);
        }

        beginTest ("Chord generator is deterministic and valid for every option");
        {
            for (int prog = 0; prog < (int) chordProgressions().size(); ++prog)
                for (int rhythm = 0; rhythm < chordRhythmNames().size(); ++rhythm)
                    for (int size = 3; size <= 5; ++size)
                    {
                        ChordGenSettings s;
                        s.progression = prog;
                        s.rhythm = (ChordRhythm) rhythm;
                        s.chordSize = size;
                        s.scale = "Blues";
                        s.seed = 42;
                        const auto a = generateChords (s, ppq * 4, ppq);
                        const auto b = generateChords (s, ppq * 4, ppq);
                        expectEquals ((int) a.notes.size(), (int) b.notes.size());
                        for (auto& n : a.notes)
                        {
                            expect (n.key >= 0 && n.key <= 127);
                            expect (n.start >= 0 && n.length > 0);
                            expect (n.start < a.length);
                            expect (n.velocity > 0.0f && n.velocity <= 1.0f);
                        }
                    }
        }

        beginTest ("Drum generator");
        {
            for (auto& style : drumStyleNames())
            {
                DrumGenSettings s;
                s.style = style;
                s.bars = 2;
                s.seed = 3;
                s.humanize = 0.0f;
                const auto hits = generateDrums (s, ticksPerStep);
                expect (! hits.empty(), style);

                bool kickOnOne = false;
                for (auto& h : hits)
                {
                    kickOnOne = kickOnOne || (h.role == DrumRole::kick && h.start == 0);
                    expect (h.start >= 0 && h.start < 32 * ticksPerStep + ticksPerStep);
                    expect (h.velocity > 0.0f && h.velocity <= 1.0f);
                }
                expect (kickOnOne, style + " should start with a kick");
                expectEquals ((int) generateDrums (s, ticksPerStep).size(), (int) hits.size());
            }

            DrumGenSettings sparse, dense;
            sparse.density = 0.0f;
            dense.density = 1.0f;
            sparse.variation = dense.variation = 0.0f;
            expect (generateDrums (dense, ticksPerStep).size() > generateDrums (sparse, ticksPerStep).size());
        }

        beginTest ("Drum role guessing from channel names");
        {
            bool matched = false;
            expect (guessDrumRole ("808 Kick Hard", matched) == DrumRole::kick && matched);
            expect (guessDrumRole ("Open HH 2", matched) == DrumRole::openHat && matched);
            expect (guessDrumRole ("Hihat closed", matched) == DrumRole::closedHat && matched);
            guessDrumRole ("Strings", matched);
            expect (! matched);
        }
    }
};

static TheoryTests theoryTests;
}
