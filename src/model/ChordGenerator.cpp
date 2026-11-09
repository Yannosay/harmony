#include "model/ChordGenerator.h"
#include "model/MusicTheory.h"
#include <algorithm>

namespace harmony
{
const std::vector<ChordProgression>& chordProgressions()
{
    static const std::vector<ChordProgression> list {
        { "Random (smart)",            {} },
        { "Pop  I V vi IV",            { 1, 5, 6, 4 } },
        { "Emotional  vi IV I V",      { 6, 4, 1, 5 } },
        { "Epic  i VI III VII",        { 1, 6, 3, 7 } },
        { "Dark  i iv v i",            { 1, 4, 5, 1 } },
        { "Andalusian  i VII VI V",    { 1, 7, 6, 5 } },
        { "Jazz  ii V I I",            { 2, 5, 1, 1 } },
        { "Royal Road  IV V iii vi",   { 4, 5, 3, 6 } },
        { "Trap  i VI iv V",           { 1, 6, 4, 5 } },
        { "Lo-Fi  ii V iii vi",        { 2, 5, 3, 6 } },
        { "Anthem  I IV vi V",         { 1, 4, 6, 5 } },
        { "Doo-Wop  I vi IV V",        { 1, 6, 4, 5 } },
        { "Cinematic  i VI VII i",     { 1, 6, 7, 1 } },
        { "Neo Soul  IV iii ii I",     { 4, 3, 2, 1 } },
        { "Canon  I V vi iii IV I IV V", { 1, 5, 6, 3, 4, 1, 4, 5 } }
    };
    return list;
}

juce::StringArray chordRhythmNames()
{
    return { "Sustain", "Quarter pulse", "Eighth pulse", "Offbeat stabs", "Arp up", "Arp down", "Arp up/down", "Strum" };
}

static std::vector<int> randomDegrees (int count, juce::Random& rng)
{
    static const std::vector<std::vector<int>> moves {
        {},
        { 4, 5, 6, 2, 3 },
        { 5, 4, 7 },
        { 6, 4, 2 },
        { 1, 5, 2, 6 },
        { 1, 6, 4 },
        { 4, 2, 5, 3 },
        { 1, 3, 6 }
    };

    std::vector<int> degrees { rng.nextInt (3) == 0 ? 6 : 1 };
    while ((int) degrees.size() < count)
    {
        const auto& options = moves[(size_t) degrees.back()];
        degrees.push_back (options[(size_t) rng.nextInt ((int) options.size())]);
    }
    return degrees;
}

std::vector<int> leadVoicing (const std::vector<int>& previous, const std::vector<int>& chord, int lowLimit, int highLimit)
{
    if (previous.size() != chord.size() || chord.empty())
        return chord;

    std::vector<int> best = chord;
    int bestCost = std::numeric_limits<int>::max();
    const int n = (int) chord.size();

    for (int inversion = 0; inversion < n; ++inversion)
    {
        std::vector<int> voicing;
        for (int i = 0; i < n; ++i)
        {
            int pitch = chord[(size_t) ((inversion + i) % n)];
            if (! voicing.empty())
                while (pitch <= voicing.back())
                    pitch += 12;
            voicing.push_back (pitch);
        }

        for (int shift = -3; shift <= 3; ++shift)
        {
            auto candidate = voicing;
            for (auto& p : candidate)
                p += 12 * shift;

            if (candidate.front() < lowLimit || candidate.back() > highLimit)
                continue;

            int cost = 0;
            for (size_t i = 0; i < candidate.size(); ++i)
                cost += std::abs (candidate[i] - previous[i]);

            if (cost < bestCost)
            {
                bestCost = cost;
                best = candidate;
            }
        }
    }
    return best;
}

static void emitRhythm (std::vector<NoteData>& out, const std::vector<int>& voicing, juce::int64 start, juce::int64 span,
                        juce::int64 beat, const ChordGenSettings& s, juce::Random& rng)
{
    auto vel = [&] { return juce::jlimit (0.05f, 1.0f, s.velocity + (rng.nextFloat() - 0.5f) * 0.08f); };
    auto push = [&] (int key, juce::int64 t, juce::int64 len)
    {
        NoteData n;
        n.channel = s.channel;
        n.key = juce::jlimit (0, 127, key);
        n.start = t;
        n.length = juce::jmax ((juce::int64) 1, len);
        n.velocity = vel();
        out.push_back (n);
    };

    const juce::int64 eighth = beat / 2;
    const juce::int64 sixteenth = beat / 4;

    switch (s.rhythm)
    {
        case ChordRhythm::sustain:
            for (auto k : voicing) push (k, start, span);
            break;

        case ChordRhythm::quarters:
            for (juce::int64 t = start; t < start + span; t += beat)
                for (auto k : voicing) push (k, t, beat * 9 / 10);
            break;

        case ChordRhythm::eighths:
            for (juce::int64 t = start; t < start + span; t += eighth)
                for (auto k : voicing) push (k, t, eighth * 8 / 10);
            break;

        case ChordRhythm::offbeats:
            for (juce::int64 t = start + eighth; t < start + span; t += beat)
                for (auto k : voicing) push (k, t, eighth * 8 / 10);
            break;

        case ChordRhythm::arpUp:
        case ChordRhythm::arpDown:
        case ChordRhythm::arpUpDown:
        {
            std::vector<int> order = voicing;
            if (s.rhythm == ChordRhythm::arpDown)
                std::reverse (order.begin(), order.end());
            else if (s.rhythm == ChordRhythm::arpUpDown && voicing.size() > 2)
                for (int i = (int) voicing.size() - 2; i > 0; --i)
                    order.push_back (voicing[(size_t) i]);

            size_t step = 0;
            for (juce::int64 t = start; t < start + span; t += sixteenth, ++step)
                push (order[step % order.size()], t, sixteenth * 9 / 10);
            break;
        }

        case ChordRhythm::strum:
        {
            const juce::int64 gap = juce::jmax ((juce::int64) 1, beat / 24);
            for (size_t i = 0; i < voicing.size(); ++i)
                push (voicing[i], start + (juce::int64) i * gap, span - (juce::int64) i * gap);
            break;
        }
    }
}

ChordGenResult generateChords (const ChordGenSettings& s, juce::int64 ticksPerBar, juce::int64 ticksPerBeat)
{
    ChordGenResult result;
    juce::Random rng ((juce::int64) s.seed);

    const auto& requested = theory::findScale (s.scale);
    const auto& scale = requested.isHeptatonic()
                            ? requested
                            : theory::findScale (std::find (requested.intervals.begin(), requested.intervals.end(), 4) != requested.intervals.end() ? "Major" : "Minor");

    const auto& presets = chordProgressions();
    const int presetIndex = juce::jlimit (0, (int) presets.size() - 1, s.progression);
    const auto degrees = presets[(size_t) presetIndex].degrees.empty()
                             ? randomDegrees (juce::jlimit (2, 16, s.randomLength), rng)
                             : presets[(size_t) presetIndex].degrees;

    const int chordSize = juce::jlimit (2, 5, s.chordSize);
    const int baseKey = (juce::jlimit (1, 7, s.octave) + 1) * 12;
    const juce::int64 span = ticksPerBar * juce::jlimit (1, 8, s.barsPerChord);

    std::vector<int> previous;
    juce::int64 t = 0;

    for (auto degree : degrees)
    {
        auto chord = theory::scaleDegreePitches (s.root, scale, degree, chordSize, baseKey);

        std::vector<int> intervals;
        for (auto p : chord)
            intervals.push_back (p - chord.front());

        result.chordNames.add (theory::detectChord (chord));
        result.numerals.add (theory::romanNumeral (degree, intervals));

        auto voicing = (s.voiceLeading && ! previous.empty())
                           ? leadVoicing (previous, chord, baseKey - 7, baseKey + 24)
                           : chord;
        previous = voicing;

        emitRhythm (result.notes, voicing, t, span, ticksPerBeat, s, rng);

        if (s.addBass)
        {
            int bass = chord.front() - 12;
            while (bass > 52) bass -= 12;
            while (bass < 28) bass += 12;

            NoteData n;
            n.channel = s.channel;
            n.key = bass;
            n.start = t;
            n.length = span;
            n.velocity = juce::jlimit (0.05f, 1.0f, s.velocity * 0.95f);
            result.notes.push_back (n);
        }

        t += span;
    }

    result.length = t;
    return result;
}
}
