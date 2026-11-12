#include "model/FollowGenerator.h"
#include "model/MusicTheory.h"
#include <set>

namespace harmony::follow
{
namespace
{
    int chooseRoot (const std::vector<int>& keys)
    {
        std::set<int> classes;
        for (auto k : keys)
            classes.insert (k % 12);

        int best = keys.empty() ? 0 : *std::min_element (keys.begin(), keys.end()) % 12;
        int bestScore = -1;
        const int lowest = best;

        for (int candidate : classes)
        {
            const bool third = classes.count ((candidate + 3) % 12) > 0 || classes.count ((candidate + 4) % 12) > 0;
            const bool fifth = classes.count ((candidate + 7) % 12) > 0;
            const int score = (third ? 2 : 0) + (fifth ? 2 : 0) + (candidate == lowest ? 1 : 0);
            if (score > bestScore)
            {
                bestScore = score;
                best = candidate;
            }
        }
        return best;
    }

    std::vector<int> chordTonesNear (const ChordSegment& chord, int low, int high)
    {
        std::set<int> classes;
        for (auto k : chord.keys)
            classes.insert (k % 12);

        std::vector<int> tones;
        for (int k = low; k <= high; ++k)
            if (classes.count (k % 12) > 0)
                tones.push_back (k);
        return tones;
    }

    int nearest (const std::vector<int>& candidates, int target)
    {
        int best = candidates.front();
        for (auto c : candidates)
            if (std::abs (c - target) < std::abs (best - target))
                best = c;
        return best;
    }

    void add (std::vector<NoteData>& out, const Settings& s, int key, juce::int64 start, juce::int64 length, float accent)
    {
        NoteData n;
        n.channel = s.channel;
        n.key = juce::jlimit (0, 127, key);
        n.start = start;
        n.length = juce::jmax ((juce::int64) 1, length);
        n.velocity = juce::jlimit (0.05f, 1.0f, s.velocity * accent);
        out.push_back (n);
    }
}

juce::StringArray styleNames()
{
    return { "Bass: sustained", "Bass: pulsing", "Arpeggio", "Melody" };
}

std::vector<ChordSegment> analyse (const std::vector<NoteData>& source, juce::int64 length, juce::int64 ticksPerBeat)
{
    std::vector<ChordSegment> segments;
    if (source.empty() || length <= 0)
        return segments;

    const auto beat = juce::jmax ((juce::int64) 1, ticksPerBeat);
    for (juce::int64 t = 0; t < length; t += beat)
    {
        std::vector<int> keys;
        for (auto& n : source)
            if (! n.muted && n.start < t + beat && n.start + n.length > t)
                keys.push_back (n.key);

        std::sort (keys.begin(), keys.end());
        keys.erase (std::unique (keys.begin(), keys.end()), keys.end());

        std::set<int> classes;
        for (auto k : keys)
            classes.insert (k % 12);

        if (! segments.empty())
        {
            std::set<int> previous;
            for (auto k : segments.back().keys)
                previous.insert (k % 12);

            if (previous == classes || classes.empty())
            {
                segments.back().end = t + beat;
                continue;
            }
        }

        if (classes.empty())
            continue;

        ChordSegment s;
        s.start = t;
        s.end = t + beat;
        s.keys = keys;
        s.root = chooseRoot (keys);
        s.name = keys.size() >= 2 ? theory::detectChord (keys) : theory::pitchClassName (s.root);
        if (s.name.isEmpty())
            s.name = theory::pitchClassName (s.root);
        segments.push_back (s);
    }

    return segments;
}

std::vector<NoteData> generate (const std::vector<ChordSegment>& chords, const Settings& s, juce::int64 ticksPerBeat)
{
    std::vector<NoteData> out;
    if (chords.empty())
        return out;

    juce::Random rng ((juce::int64) s.seed);
    const auto beat = juce::jmax ((juce::int64) 1, ticksPerBeat);
    const auto eighth = beat / 2;
    const auto sixteenth = beat / 4;
    const auto& scale = theory::findScale (s.scale);
    const int base = 12 * (juce::jlimit (0, 8, s.octave) + 1);

    switch (s.style)
    {
        case Style::sustainedBass:
            for (auto& c : chords)
                add (out, s, base + c.root, c.start, c.end - c.start, 1.0f);
            break;

        case Style::pulsingBass:
        {
            const auto step = s.density < 0.34f ? beat : (s.density < 0.75f ? eighth : sixteenth);
            for (auto& c : chords)
            {
                int index = 0;
                for (auto t = c.start; t < c.end; t += step, ++index)
                {
                    int key = base + c.root;
                    if (step <= eighth && index % 4 == 2 && s.density > 0.5f)
                        key += 12;
                    else if (index > 0 && index % 8 == 7 && rng.nextFloat() < s.density)
                        key += 7;
                    const bool onBeat = (t % beat) == 0;
                    add (out, s, key, t, juce::jmin (step - step / 8, c.end - t), onBeat ? 1.0f : 0.82f);
                }
            }
            break;
        }

        case Style::arpeggio:
        {
            const auto step = s.density < 0.5f ? eighth : sixteenth;
            for (auto& c : chords)
            {
                const auto tones = chordTonesNear (c, base, base + 24);
                if (tones.empty())
                    continue;

                int index = 0;
                const int span = (int) tones.size();
                for (auto t = c.start; t < c.end; t += step, ++index)
                {
                    const int cycle = juce::jmax (1, span * 2 - 2);
                    const int pos = index % cycle;
                    const int toneIndex = pos < span ? pos : cycle - pos;
                    add (out, s, tones[(size_t) juce::jlimit (0, span - 1, toneIndex)], t, juce::jmin (step, c.end - t), (t % beat) == 0 ? 1.0f : 0.8f);
                }
            }
            break;
        }

        case Style::melody:
        {
            int previous = base + 7;
            for (auto& c : chords)
            {
                const auto tones = chordTonesNear (c, base, base + 19);
                if (tones.empty())
                    continue;

                for (auto t = c.start; t < c.end; t += beat)
                {
                    const bool rest = rng.nextFloat() > 0.55f + s.density * 0.45f && t != c.start;
                    if (rest)
                        continue;

                    const int target = previous + rng.nextInt ({ -4, 5 });
                    const int chordTone = nearest (tones, target);
                    const bool split = rng.nextFloat() < s.density * 0.8f && t + beat <= c.end;

                    if (split)
                    {
                        add (out, s, chordTone, t, eighth, 1.0f);
                        int passing = chordTone + (rng.nextBool() ? 2 : -2);
                        passing = theory::snapToScale (passing, s.scaleRoot, scale);
                        add (out, s, juce::jlimit (base, base + 21, passing), t + eighth, eighth, 0.78f);
                        previous = passing;
                    }
                    else
                    {
                        const auto length = (t + beat * 2 <= c.end && rng.nextFloat() > s.density) ? beat * 2 : beat;
                        add (out, s, chordTone, t, length - sixteenth / 2, 1.0f);
                        previous = chordTone;
                        if (length > beat)
                            t += beat;
                    }
                }
            }
            break;
        }
    }

    return out;
}
}
