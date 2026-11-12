#include "engine/SnapshotBuilder.h"
#include "engine/AudioEngine.h"
#include <map>

namespace harmony::engine
{
namespace
{
    juce::int64 swungStart (juce::int64 start, float swing) noexcept
    {
        if (swing <= 0.0f)
            return start;

        const juce::int64 pair = ticksPerStep * 2;
        if (start % pair == ticksPerStep)
            return start + (juce::int64) (swing * (float) ticksPerStep * 0.66f);

        return start;
    }

    juce::uint8 toMidiVelocity (float v) noexcept
    {
        return (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (v * 127.0f));
    }

    juce::AudioProcessorParameter* parameterAt (juce::AudioPluginInstance* plugin, int index)
    {
        if (plugin == nullptr)
            return nullptr;

        const auto& params = plugin->getParameters();
        return juce::isPositiveAndBelow (index, params.size()) ? params[index] : nullptr;
    }

    bool resolveLane (const automation::Target& target, const AudioEngine& engine, AutomationLane& lane)
    {
        using automation::Kind;

        switch (target.kind)
        {
            case Kind::channelVolume:
            case Kind::channelPan:
                if (auto* node = engine.findChannelNode (target.id))
                {
                    lane.gain = &node->gain;
                    lane.mode = target.kind == Kind::channelVolume ? AutomationLane::Mode::volume : AutomationLane::Mode::pan;
                    return true;
                }
                return false;

            case Kind::insertVolume:
            case Kind::insertPan:
                if (auto* insert = engine.getInsertNode (target.id))
                {
                    lane.gain = &insert->gain;
                    lane.mode = target.kind == Kind::insertVolume ? AutomationLane::Mode::volume : AutomationLane::Mode::pan;
                    return true;
                }
                return false;

            case Kind::slotParameter:
                if (auto* slot = engine.findSlotNode (target.id))
                    lane.parameter = parameterAt (slot->getPlugin(), target.parameter);
                lane.mode = AutomationLane::Mode::parameter;
                return lane.parameter != nullptr;

            case Kind::channelParameter:
                if (auto* node = engine.findChannelNode (target.id))
                    lane.parameter = parameterAt (node->getPlugin(), target.parameter);
                lane.mode = AutomationLane::Mode::parameter;
                return lane.parameter != nullptr;

            case Kind::none:
                break;
        }
        return false;
    }
}

std::vector<MidiEvent> expandPatternNotes (const juce::ValueTree& pattern, int channelUid, juce::int64 patternLength,
                                           float swing, juce::int64 clipStart, juce::int64 clipLength,
                                           juce::int64 clipOffset, bool loopContent)
{
    std::vector<MidiEvent> events;
    const juce::int64 clipEnd = clipStart + clipLength;
    patternLength = juce::jmax ((juce::int64) 1, patternLength);

    for (auto noteTree : pattern)
    {
        const auto note = Project::readNote (noteTree);
        if (note.channel != channelUid || note.muted)
            continue;

        const juce::int64 noteStart = swungStart (note.start, swing);
        const juce::int64 firstRepeat = loopContent ? juce::jmax ((juce::int64) 0, clipOffset / patternLength) : 0;
        const juce::int64 lastRepeat = loopContent ? (clipOffset + clipLength) / patternLength : 0;

        for (juce::int64 r = firstRepeat; r <= lastRepeat; ++r)
        {
            const juce::int64 start = clipStart + r * patternLength + noteStart - clipOffset;
            if (start < clipStart || start >= clipEnd)
                continue;

            const juce::int64 end = juce::jmin (clipEnd, start + note.length);
            events.push_back ({ start, (juce::uint8) note.key, toMidiVelocity (note.velocity) });
            events.push_back ({ juce::jmax (start + 1, end), (juce::uint8) note.key, 0 });
        }
    }
    return events;
}

std::unique_ptr<Snapshot> buildSnapshot (const Project& project, const AudioEngine& engine)
{
    auto snap = std::make_unique<Snapshot>();
    const auto& state = project.getState();

    snap->bpm = project.getBpm();
    snap->numerator = project.getNumerator();
    snap->denominator = project.getDenominator();
    snap->ticksPerBeat = project.ticksPerBeat();
    snap->ticksPerBar = project.ticksPerBar();
    snap->songLength = project.songLength();
    snap->loopStart = juce::jmax ((juce::int64) 0, (juce::int64) state[id::loopStart]);
    snap->loopEnd = juce::jmax ((juce::int64) 0, (juce::int64) state[id::loopEnd]);

    const float swing = project.getSwing();
    const auto patterns = project.patternsTree();

    for (auto pattern : patterns)
        snap->patterns.push_back ({ (int) pattern[id::uid], project.patternLength (pattern) });

    for (auto channel : project.channelsTree())
    {
        ChannelPlan plan;
        plan.uid = channel[id::uid];
        plan.insert = juce::jlimit (0, numMixerInserts, (int) channel[id::insert]);
        plan.node = engine.findChannelNode (plan.uid);

        for (size_t p = 0; p < snap->patterns.size(); ++p)
        {
            const auto length = snap->patterns[p].length;
            auto events = expandPatternNotes (patterns.getChild ((int) p), plan.uid, length, swing, 0, length, 0, false);
            sortEvents (events);
            plan.patterns.push_back (std::move (events));
        }

        snap->channels.push_back (std::move (plan));
    }

    std::map<juce::String, size_t> laneIndex;

    for (auto track : project.tracksTree())
    {
        if ((bool) track[id::mute])
            continue;

        for (auto clip : track)
        {
            if ((bool) clip[id::muted])
                continue;

            const juce::int64 start = clip[id::start];
            const juce::int64 length = juce::jmax ((juce::int64) 1, (juce::int64) clip[id::length]);
            const juce::int64 offset = juce::jmax ((juce::int64) 0, (juce::int64) clip[id::offset]);

            if (clip[id::kind].toString() == kind::automation)
            {
                const auto key = clip[id::target].toString();
                auto existing = laneIndex.find (key);
                if (existing == laneIndex.end())
                {
                    AutomationLane lane;
                    if (! resolveLane (automation::parse (key), engine, lane))
                        continue;

                    existing = laneIndex.emplace (key, snap->automation.size()).first;
                    snap->automation.push_back (std::move (lane));
                }

                snap->automation[existing->second].segments.push_back ({ start, start + length, offset, automation::readPoints (clip) });
            }
            else if (clip[id::kind].toString() == kind::pattern)
            {
                const int patternUid = clip[id::pattern];
                const int patternIndex = snap->patternIndex (patternUid);
                const auto pattern = project.findPattern (patternUid);
                if (! pattern.isValid() || patternIndex < 0)
                    continue;

                const auto patternLength = snap->patterns[(size_t) patternIndex].length;
                for (auto& plan : snap->channels)
                {
                    auto events = expandPatternNotes (pattern, plan.uid, patternLength, swing, start, length, offset, true);
                    plan.song.insert (plan.song.end(), events.begin(), events.end());
                }
            }
            else
            {
                const int channelUid = clip[id::channel];
                for (auto& plan : snap->channels)
                    if (plan.uid == channelUid)
                        plan.audioClips.push_back ({ start, start + length, offset,
                                                     (float) clip.getProperty (id::gain, 1.0f) });
            }
        }
    }

    for (auto& plan : snap->channels)
        sortEvents (plan.song);

    for (auto& lane : snap->automation)
        std::stable_sort (lane.segments.begin(), lane.segments.end(),
                          [] (const AutomationSegment& a, const AutomationSegment& b) { return a.start < b.start; });

    for (auto insert : project.mixerTree())
    {
        InsertPlan plan;
        plan.index = insert[id::index];
        plan.node = engine.getInsertNode (plan.index);
        if (plan.node == nullptr)
            continue;

        std::vector<std::pair<int, FxSlotNode*>> ordered;
        for (auto slot : insert)
            if (auto* node = engine.findSlotNode (slot[id::uid]))
                ordered.push_back ({ (int) slot[id::index], node });

        std::sort (ordered.begin(), ordered.end(), [] (auto& a, auto& b) { return a.first < b.first; });
        for (auto& [index, node] : ordered)
            plan.slots.push_back (node);

        snap->inserts.push_back (std::move (plan));
    }

    return snap;
}
}
