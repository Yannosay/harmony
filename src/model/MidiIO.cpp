#include "model/MidiIO.h"
#include "engine/SnapshotBuilder.h"
#include <map>

namespace harmony::midi
{
namespace
{
    juce::MidiMessageSequence conductor (const Project& project)
    {
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::round (60000000.0 / project.getBpm())), 0.0);
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (project.getNumerator(), project.getDenominator()), 0.0);
        return seq;
    }

    juce::MidiMessageSequence trackFor (const std::vector<NoteData>& notes, const juce::String& name, int midiChannel)
    {
        juce::MidiMessageSequence seq;
        if (name.isNotEmpty())
            seq.addEvent (juce::MidiMessage::textMetaEvent (3, name), 0.0);

        for (auto& n : notes)
        {
            if (n.muted)
                continue;

            const auto velocity = (juce::uint8) juce::jlimit (1, 127, juce::roundToInt (n.velocity * 127.0f));
            seq.addEvent (juce::MidiMessage::noteOn (midiChannel, n.key, velocity), (double) n.start);
            seq.addEvent (juce::MidiMessage::noteOff (midiChannel, n.key), (double) (n.start + n.length));
        }

        seq.sort();
        seq.updateMatchedPairs();
        return seq;
    }

    juce::MidiFile assemble (const Project& project, const std::vector<std::pair<juce::String, std::vector<NoteData>>>& tracks)
    {
        juce::MidiFile file;
        file.setTicksPerQuarterNote (ppq);
        file.addTrack (conductor (project));

        int channel = 1;
        for (auto& [name, notes] : tracks)
        {
            if (notes.empty())
                continue;
            file.addTrack (trackFor (notes, name, channel));
            channel = channel % 16 + 1;
        }
        return file;
    }
}

juce::MidiFile exportNotes (const Project& project, const std::vector<NoteData>& notes, const juce::String& trackName)
{
    return assemble (project, { { trackName, notes } });
}

juce::MidiFile exportPattern (const Project& project, const juce::ValueTree& pattern, int onlyChannel)
{
    std::vector<std::pair<juce::String, std::vector<NoteData>>> tracks;
    for (auto channel : project.channelsTree())
    {
        const int uid = channel[id::uid];
        if (onlyChannel > 0 && uid != onlyChannel)
            continue;

        std::vector<NoteData> notes;
        for (auto n : pattern)
            if ((int) n[id::channel] == uid)
                notes.push_back (Project::readNote (n));

        tracks.push_back ({ channel[id::name].toString(), std::move (notes) });
    }
    return assemble (project, tracks);
}

juce::MidiFile exportSong (const Project& project)
{
    std::map<int, std::vector<NoteData>> byChannel;

    for (auto track : project.tracksTree())
    {
        if ((bool) track[id::mute])
            continue;

        for (auto clip : track)
        {
            if ((bool) clip[id::muted] || clip[id::kind].toString() != kind::pattern)
                continue;

            const auto pattern = project.findPattern (clip[id::pattern]);
            if (! pattern.isValid())
                continue;

            const auto length = project.patternLength (pattern);
            for (auto channel : project.channelsTree())
            {
                const int uid = channel[id::uid];
                auto events = engine::expandPatternNotes (pattern, uid, length, project.getSwing(), clip[id::start],
                                                          juce::jmax ((juce::int64) 1, (juce::int64) clip[id::length]), clip[id::offset], true);
                engine::sortEvents (events);

                std::map<int, std::vector<engine::MidiEvent>> open;
                for (auto& e : events)
                {
                    if (! e.isNoteOff())
                    {
                        open[e.key].push_back (e);
                        continue;
                    }

                    auto& stack = open[e.key];
                    if (stack.empty())
                        continue;

                    const auto on = stack.front();
                    stack.erase (stack.begin());
                    NoteData n;
                    n.channel = uid;
                    n.key = on.key;
                    n.start = on.tick;
                    n.length = juce::jmax ((juce::int64) 1, e.tick - on.tick);
                    n.velocity = on.velocity / 127.0f;
                    byChannel[uid].push_back (n);
                }
            }
        }
    }

    std::vector<std::pair<juce::String, std::vector<NoteData>>> tracks;
    for (auto channel : project.channelsTree())
        tracks.push_back ({ channel[id::name].toString(), byChannel[(int) channel[id::uid]] });
    return assemble (project, tracks);
}

bool writeFile (const juce::MidiFile& midi, const juce::File& target)
{
    target.deleteFile();
    juce::FileOutputStream out (target);
    return out.openedOk() && midi.writeTo (out, 1);
}

std::optional<juce::MidiFile> readData (const void* data, size_t size)
{
    juce::MemoryInputStream in (data, size, false);
    juce::MidiFile midi;
    if (! midi.readFrom (in, true))
        return std::nullopt;
    return midi;
}

std::optional<juce::MidiFile> readFile (const juce::File& source)
{
    juce::MemoryBlock data;
    if (! source.loadFileAsData (data))
        return std::nullopt;
    return readData (data.getData(), data.getSize());
}

std::vector<ImportedTrack> importTracks (const juce::MidiFile& source)
{
    std::vector<ImportedTrack> result;
    const int format = source.getTimeFormat();
    if (format <= 0)
    {
        juce::MidiFile copy (source);
        copy.convertTimestampTicksToSeconds();
        const double bpm = 120.0;

        for (int t = 0; t < copy.getNumTracks(); ++t)
        {
            ImportedTrack track;
            auto seq = *copy.getTrack (t);
            seq.updateMatchedPairs();
            for (auto* e : seq)
                if (e->message.isNoteOn() && e->noteOffObject != nullptr)
                {
                    NoteData n;
                    n.key = e->message.getNoteNumber();
                    n.start = (juce::int64) std::llround (e->message.getTimeStamp() * bpm / 60.0 * ppq);
                    n.length = juce::jmax ((juce::int64) 1, (juce::int64) std::llround ((e->noteOffObject->message.getTimeStamp() - e->message.getTimeStamp()) * bpm / 60.0 * ppq));
                    n.velocity = e->message.getFloatVelocity();
                    track.notes.push_back (n);
                }
            if (! track.notes.empty())
                result.push_back (std::move (track));
        }
        return result;
    }

    const double scale = (double) ppq / (double) format;

    for (int t = 0; t < source.getNumTracks(); ++t)
    {
        ImportedTrack track;
        auto seq = *source.getTrack (t);
        seq.updateMatchedPairs();

        for (auto* e : seq)
        {
            const auto& m = e->message;
            if (m.isTrackNameEvent() && track.name.isEmpty())
                track.name = m.getTextFromTextMetaEvent();

            if (! m.isNoteOn() || e->noteOffObject == nullptr)
                continue;

            NoteData n;
            n.channel = t;
            n.key = m.getNoteNumber();
            n.start = (juce::int64) std::llround (m.getTimeStamp() * scale);
            n.length = juce::jmax ((juce::int64) 1, (juce::int64) std::llround ((e->noteOffObject->message.getTimeStamp() - m.getTimeStamp()) * scale));
            n.velocity = juce::jlimit (0.0f, 1.0f, m.getFloatVelocity());
            track.notes.push_back (n);
        }

        if (! track.notes.empty())
            result.push_back (std::move (track));
    }
    return result;
}

std::vector<NoteData> flatten (const std::vector<ImportedTrack>& tracks)
{
    std::vector<NoteData> notes;
    for (auto& t : tracks)
        notes.insert (notes.end(), t.notes.begin(), t.notes.end());

    std::stable_sort (notes.begin(), notes.end(), [] (const NoteData& a, const NoteData& b) { return a.start < b.start; });
    return notes;
}

bool isMidiFile (const juce::File& file)
{
    return file.hasFileExtension ("mid;midi;smf");
}
}
