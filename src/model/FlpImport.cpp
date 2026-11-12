#include "model/FlpImport.h"
#include <map>
#include <set>

namespace harmony::flp
{
namespace
{
    enum : int
    {
        timeSigNumerator = 17,
        timeSigBeat = 18,
        channelType = 21,
        channelRoutedTo = 22,
        newChannel = 64,
        newPattern = 65,
        channelInsert = 104,
        legacyTempo = 66,
        slotIndex = 98,
        fineTempo = 156,
        patternName = 193,
        samplePath = 196,
        version = 199,
        pluginDefaultName = 201,
        pluginName = 203,
        insertName = 204,
        pluginData = 213,
        channelLevels = 219,
        patternNotes = 224,
        insertFlags = 236,
        playlistItems = 233,
        trackData = 238,
        trackName = 239
    };

    enum : int { typeSampler = 0, typeGenerator = 2, typeLayer = 3, typeAudioClip = 4, typeAutomation = 5 };

    constexpr juce::uint16 patternBase = 20480;
    constexpr int flTracks = 500;

    juce::uint32 readLE (const juce::uint8* p, int bytes)
    {
        juce::uint32 v = 0;
        for (int i = bytes; --i >= 0;)
            v = (v << 8) | p[i];
        return v;
    }

    struct Channel
    {
        int index = -1;
        int type = typeGenerator;
        juce::String name, defaultName, sample, vstName;
        int routedTo = -1;
        float volume = -1.0f, pan = 0.0f;
        bool usedAsAudioClip = false;
        bool hasNotes = false;
    };

    struct Slot
    {
        int insert = 0;
        int index = 0;
        juce::String defaultName, name, vstName;
    };

    struct Insert
    {
        juce::String name;
    };

    juce::String decodeText (const juce::MemoryBlock& data, bool utf16)
    {
        if (data.isEmpty())
            return {};

        if (utf16)
        {
            const auto* chars = static_cast<const juce::CharPointer_UTF16::CharType*> (data.getData());
            return juce::String (juce::CharPointer_UTF16 (chars), (size_t) data.getSize() / 2).trimCharactersAtEnd (juce::String::charToString (0)).trim();
        }

        return juce::String::fromUTF8 (static_cast<const char*> (data.getData()), (int) data.getSize()).trimCharactersAtEnd (juce::String::charToString (0)).trim();
    }

    juce::String vstNameFromWrapper (const juce::MemoryBlock& data)
    {
        const auto* p = static_cast<const juce::uint8*> (data.getData());
        const size_t size = data.getSize();
        size_t pos = 4;

        while (pos + 12 <= size)
        {
            const auto id = readLE (p + pos, 4);
            const auto length = (size_t) readLE (p + pos + 4, 4) | ((size_t) readLE (p + pos + 8, 4) << 32);
            pos += 12;
            if (length > size - pos)
                break;

            if (id == 54)
                return juce::String::fromUTF8 (reinterpret_cast<const char*> (p + pos), (int) length).trimCharactersAtEnd (juce::String::charToString (0)).trim();
            pos += length;
        }
        return {};
    }

    int majorVersion (const juce::String& v)
    {
        return v.upToFirstOccurrenceOf (".", false, false).getIntValue();
    }

    int minorVersion (const juce::String& v)
    {
        return v.fromFirstOccurrenceOf (".", false, false).upToFirstOccurrenceOf (".", false, false).getIntValue();
    }

    juce::File resolveSample (const juce::String& raw, const juce::File& projectFolder, const Options& options)
    {
        if (raw.isEmpty())
            return {};

        juce::StringArray candidates;
        if (raw.containsIgnoreCase ("%FLStudioFactoryData%"))
            for (auto& folder : options.factoryDataFolders)
                candidates.add (raw.replace ("%FLStudioFactoryData%", folder.getFullPathName(), true));
        else if (raw.containsIgnoreCase ("%FLStudioUserData%"))
            candidates.add (raw.replace ("%FLStudioUserData%", options.userDataFolder.getFullPathName(), true));
        else
            candidates.add (raw);

        for (auto& c : candidates)
        {
            const auto path = c.replaceCharacter ('/', '\\');
            if (juce::File::isAbsolutePath (path) && juce::File (path).existsAsFile())
                return juce::File (path);
        }

        const auto fileName = raw.replaceCharacter ('\\', '/').fromLastOccurrenceOf ("/", false, false);
        if (fileName.isNotEmpty())
        {
            if (auto beside = projectFolder.getChildFile (fileName); beside.existsAsFile())
                return beside;

            int visited = 0;
            for (const auto& entry : juce::RangedDirectoryIterator (projectFolder, true, fileName, juce::File::findFiles))
            {
                if (++visited > 2000)
                    break;
                return entry.getFile();
            }
        }

        return {};
    }

    float flVolumeToFader (juce::uint32 volume)
    {
        return juce::jlimit (0.0f, 1.25f, 0.78f * (float) volume / 10000.0f);
    }
}

juce::String builtinEffectFor (const juce::String& name)
{
    static const std::pair<const char*, const char*> table[] = {
        { "Fruity Parametric EQ", "eq" }, { "Fruity 7 Band EQ", "eq" }, { "EQUO", "eq" },
        { "Fruity Filter", "filter" }, { "Fruity Fast LP", "filter" }, { "Fruity Love Philter", "filter" },
        { "Fruity Compressor", "compressor" }, { "Fruity Multiband Compressor", "compressor" }, { "Transient Processor", "compressor" },
        { "Fruity Limiter", "limiter" }, { "Maximus", "limiter" }, { "Soundgoodizer", "limiter" },
        { "Fruity Reeverb", "reverb" }, { "LuxeVerb", "reverb" }, { "Fruity Convolver", "reverb" },
        { "Fruity Delay", "delay" }, { "Fruity Delay Bank", "delay" },
        { "Fruity Chorus", "chorus" }, { "Hyper Chorus", "chorus" }, { "Vintage Chorus", "chorus" }, { "Fruity Flangus", "chorus" },
        { "Fruity Flanger", "chorus" },
        { "Fruity Phaser", "phaser" }, { "Vintage Phaser", "phaser" },
        { "Fruity Fast Dist", "saturator" }, { "Fruity Blood Overdrive", "saturator" }, { "Fruity Soft Clipper", "saturator" },
        { "Fruity WaveShaper", "saturator" }, { "Fruity Squeeze", "saturator" }, { "Distructor", "saturator" },
        { "Fruity Balance", "utility" }, { "Fruity Stereo Shaper", "utility" }, { "Fruity Stereo Enhancer", "utility" },
        { "Fruity PanOMatic", "utility" }, { "Spreader", "utility" }
    };

    for (auto& [prefix, id] : table)
        if (name.startsWithIgnoreCase (prefix))
            return id;
    return {};
}

Options defaultOptions()
{
    Options o;
    for (auto location : { juce::File::globalApplicationsDirectory, juce::File::globalApplicationsDirectoryX86 })
    {
        const auto imageLine = juce::File::getSpecialLocation (location).getChildFile ("Image-Line");
        if (! imageLine.isDirectory())
            continue;

        juce::Array<juce::File> installs;
        for (const auto& entry : juce::RangedDirectoryIterator (imageLine, false, "FL Studio*", juce::File::findDirectories))
            installs.add (entry.getFile());

        juce::File::NaturalFileComparator comparator { false };
        installs.sort (comparator);
        for (int i = installs.size(); --i >= 0;)
            o.factoryDataFolders.add (installs[i]);
    }

    o.userDataFolder = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("Image-Line").getChildFile ("FL Studio");
    return o;
}

juce::String Report::summary() const
{
    juce::String s;
    s << "Imported " << channels << " channels, " << patterns << " patterns, " << notes << " notes and "
      << (patternClips + audioClips) << " playlist clips from FL Studio " << flVersion << ".";

    if (! placeholderInstruments.isEmpty())
        s << " " << placeholderInstruments.size() << " FL instruments need a replacement.";
    if (! missingSamples.isEmpty())
        s << " " << missingSamples.size() << " samples are missing.";
    return s;
}

juce::String Report::details() const
{
    juce::String s = summary() + "\n\n";

    auto list = [&s] (const juce::String& title, const juce::StringArray& items)
    {
        if (items.isEmpty())
            return;
        s << title << "\n";
        for (auto& i : items)
            s << "  - " << i << "\n";
        s << "\n";
    };

    if (effects > 0)
        s << effects << " FL Studio effects were replaced with the matching Harmony effect at default settings.\n\n";

    list ("FL Studio instruments without a Harmony equivalent (notes are kept, pick an instrument for these channels):", placeholderInstruments);
    list ("Samples that could not be found:", missingSamples);
    list ("Effects that were not imported:", skippedEffects);

    if (automationClipsSkipped > 0)
        s << automationClipsSkipped << " FL Studio automation clips were not imported.\n";
    if (layerChannelsSkipped > 0)
        s << layerChannelsSkipped << " layer channels were not imported.\n";
    if (insertsBeyondRange > 0)
        s << insertsBeyondRange << " more mixer inserts than Harmony's " << numMixerInserts << " were routed to the master.\n";
    return s.trimEnd();
}

bool parseEvents (const juce::MemoryBlock& file, int& fileTicksPerQuarter, std::vector<Event>& events, juce::String& error)
{
    const auto* d = static_cast<const juce::uint8*> (file.getData());
    const size_t size = file.getSize();

    if (size < 22 || std::memcmp (d, "FLhd", 4) != 0)
    {
        error = "This is not an FL Studio project.";
        return false;
    }

    const auto headerLength = (size_t) readLE (d + 4, 4);
    if (headerLength < 6 || 8 + headerLength + 8 > size)
    {
        error = "The FL Studio project header is damaged.";
        return false;
    }

    fileTicksPerQuarter = (int) readLE (d + 12, 2);
    size_t pos = 8 + headerLength;
    if (std::memcmp (d + pos, "FLdt", 4) != 0 || fileTicksPerQuarter <= 0)
    {
        error = "The FL Studio project data is damaged.";
        return false;
    }

    const size_t end = juce::jmin (size, pos + 8 + (size_t) readLE (d + pos + 4, 4));
    pos += 8;

    while (pos < end)
    {
        Event e;
        e.id = d[pos++];

        if (e.id < 64)
        {
            if (pos + 1 > end) break;
            e.value = d[pos];
            pos += 1;
        }
        else if (e.id < 128)
        {
            if (pos + 2 > end) break;
            e.value = readLE (d + pos, 2);
            pos += 2;
        }
        else if (e.id < 192)
        {
            const bool shortForm = e.id == 172 && pos + 3 < end && d[pos + 3] == 0xC0;
            const size_t bytes = shortForm ? 3 : 4;
            if (pos + bytes > end) break;
            e.value = readLE (d + pos, (int) bytes);
            pos += bytes;
        }
        else
        {
            size_t length = 0;
            int shift = 0;
            while (pos < end)
            {
                const auto b = d[pos++];
                length |= (size_t) (b & 0x7f) << shift;
                shift += 7;
                if ((b & 0x80) == 0 || shift > 28)
                    break;
            }

            if (length > end - pos)
            {
                error = "The FL Studio project is truncated.";
                return false;
            }

            e.data.append (d + pos, length);
            pos += length;
        }

        events.push_back (std::move (e));
    }

    return true;
}

juce::ValueTree convert (const juce::File& file, const Options& options, Report& report, juce::String& error)
{
    juce::MemoryBlock raw;
    if (! file.loadFileAsData (raw))
    {
        error = "The file could not be read.";
        return {};
    }

    int flPpq = 96;
    std::vector<Event> events;
    if (! parseEvents (raw, flPpq, events, error))
        return {};

    for (auto& e : events)
        if (e.id == version)
        {
            report.flVersion = juce::String::fromUTF8 (static_cast<const char*> (e.data.getData()), (int) e.data.getSize()).trimCharactersAtEnd (juce::String::charToString (0));
            break;
        }

    const bool utf16 = majorVersion (report.flVersion) > 11 || (majorVersion (report.flVersion) == 11 && minorVersion (report.flVersion) >= 5);
    const double scale = (double) ppq / (double) flPpq;
    const auto toTicks = [scale] (double flTicks) { return (juce::int64) std::llround (flTicks * scale); };

    double tempo = 0.0;
    int numerator = 4, denominator = 4;
    std::map<int, Channel> channels;
    std::map<int, juce::String> patternNames;
    std::map<int, std::vector<NoteData>> patternNotesById;
    std::map<int, juce::String> trackNames;
    std::vector<Slot> slots;
    std::map<int, Insert> inserts;
    const juce::MemoryBlock* playlist = nullptr;

    int currentChannel = -1, currentPattern = -1, currentTrack = -1, currentInsert = -1;
    bool inMixer = false;
    Slot pendingSlot;
    juce::String pendingInsertName;

    for (auto& e : events)
    {
        switch (e.id)
        {
            case timeSigNumerator: numerator = juce::jlimit (1, 32, (int) e.value); break;
            case timeSigBeat:      denominator = juce::jlimit (1, 32, (int) e.value); break;
            case legacyTempo:      if (tempo <= 0.0) tempo = e.value; break;
            case fineTempo:        tempo = e.value / 1000.0; break;

            case newChannel:
                currentChannel = (int) e.value;
                channels[currentChannel].index = currentChannel;
                break;

            case channelType:
                if (currentChannel >= 0 && ! inMixer)
                    channels[currentChannel].type = (int) e.value;
                break;

            case channelRoutedTo:
                if (currentChannel >= 0 && ! inMixer && channels[currentChannel].routedTo < 0)
                    channels[currentChannel].routedTo = (int) (juce::int8) e.value;
                break;

            case channelInsert:
                if (currentChannel >= 0 && ! inMixer)
                    channels[currentChannel].routedTo = (int) e.value;
                break;

            case samplePath:
                if (currentChannel >= 0 && ! inMixer)
                    channels[currentChannel].sample = decodeText (e.data, utf16);
                break;

            case channelLevels:
                if (currentChannel >= 0 && ! inMixer && e.data.getSize() >= 8)
                {
                    const auto* p = static_cast<const juce::uint8*> (e.data.getData());
                    channels[currentChannel].pan = juce::jlimit (-1.0f, 1.0f, (float) (juce::int32) readLE (p, 4) / 6400.0f);
                    channels[currentChannel].volume = flVolumeToFader (readLE (p + 4, 4));
                }
                break;

            case pluginDefaultName:
                if (inMixer) pendingSlot.defaultName = decodeText (e.data, utf16);
                else if (currentChannel >= 0) channels[currentChannel].defaultName = decodeText (e.data, utf16);
                break;

            case pluginName:
                if (inMixer) pendingSlot.name = decodeText (e.data, utf16);
                else if (currentChannel >= 0) channels[currentChannel].name = decodeText (e.data, utf16);
                break;

            case pluginData:
                if (inMixer && pendingSlot.defaultName == "Fruity Wrapper")
                    pendingSlot.vstName = vstNameFromWrapper (e.data);
                else if (! inMixer && currentChannel >= 0 && channels[currentChannel].defaultName == "Fruity Wrapper")
                    channels[currentChannel].vstName = vstNameFromWrapper (e.data);
                break;

            case newPattern:
                currentPattern = (int) e.value;
                break;

            case patternName:
                if (currentPattern >= 0)
                    patternNames[currentPattern] = decodeText (e.data, utf16);
                break;

            case patternNotes:
                if (currentPattern >= 0)
                {
                    const auto* p = static_cast<const juce::uint8*> (e.data.getData());
                    auto& list = patternNotesById[currentPattern];
                    for (size_t i = 0; i + 24 <= e.data.getSize(); i += 24)
                    {
                        NoteData n;
                        n.start = toTicks (readLE (p + i, 4));
                        n.channel = (int) readLE (p + i + 6, 2);
                        n.length = juce::jmax ((juce::int64) 1, toTicks (readLE (p + i + 8, 4)));
                        n.key = juce::jlimit (0, 127, (int) readLE (p + i + 12, 2));
                        n.velocity = juce::jlimit (0.0f, 1.0f, p[i + 21] / 128.0f);
                        list.push_back (n);
                    }
                }
                break;

            case playlistItems:
                if (playlist == nullptr)
                    playlist = &e.data;
                break;

            case trackData:
                currentTrack = e.data.getSize() >= 4 ? (int) readLE (static_cast<const juce::uint8*> (e.data.getData()), 4) - 1 : currentTrack + 1;
                break;

            case trackName:
                if (currentTrack >= 0)
                    trackNames[currentTrack] = decodeText (e.data, utf16);
                break;

            case insertName:
                pendingInsertName = decodeText (e.data, utf16);
                break;

            case insertFlags:
                inMixer = true;
                ++currentInsert;
                inserts[currentInsert].name = pendingInsertName;
                pendingInsertName = {};
                pendingSlot = {};
                break;

            case slotIndex:
                if (inMixer)
                {
                    if (pendingSlot.defaultName.isNotEmpty())
                    {
                        pendingSlot.insert = currentInsert;
                        pendingSlot.index = (int) e.value;
                        slots.push_back (pendingSlot);
                    }
                    pendingSlot = {};
                }
                break;

            default:
                break;
        }
    }

    Project project;
    auto& state = project.getState();
    if (tempo > 0.0)
        project.setBpm (tempo);
    project.setTimeSignature (numerator, denominator);
    state.setProperty (id::name, file.getFileNameWithoutExtension(), nullptr);

    struct Clip
    {
        int track;
        bool pattern;
        int source;
        juce::int64 start, length, offset;
    };
    std::vector<Clip> clips;
    int highestTrack = 0;

    if (playlist != nullptr)
    {
        const auto* p = static_cast<const juce::uint8*> (playlist->getData());
        const size_t size = playlist->getSize();
        size_t stride = 0;

        for (size_t candidate : { (size_t) 80, (size_t) 60, (size_t) 32 })
        {
            if (size == 0 || size % candidate != 0)
                continue;

            bool valid = true;
            for (size_t i = 0; i < size && valid; i += candidate)
                valid = readLE (p + i + 4, 2) == patternBase;

            if (valid)
            {
                stride = candidate;
                break;
            }
        }

        for (size_t i = 0; stride > 0 && i + stride <= size; i += stride)
        {
            const auto item = (int) readLE (p + i + 6, 2);
            const int track = flTracks - 1 - (int) readLE (p + i + 12, 2);
            if (! juce::isPositiveAndBelow (track, flTracks))
                continue;

            Clip c;
            c.track = track;
            c.pattern = item > patternBase;
            c.source = c.pattern ? item - patternBase : item;
            c.start = toTicks (readLE (p + i, 4));
            c.length = juce::jmax ((juce::int64) 1, toTicks (readLE (p + i + 8, 4)));

            if (c.pattern)
            {
                const auto startOffset = (juce::int32) readLE (p + i + 24, 4);
                c.offset = startOffset > 0 ? toTicks (startOffset) : 0;
            }
            else
            {
                float ms = 0.0f;
                const auto bits = readLE (p + i + 24, 4);
                std::memcpy (&ms, &bits, sizeof (float));
                const double bpm = tempo > 0.0 ? tempo : 120.0;
                c.offset = std::isfinite (ms) && ms > 0.0f ? (juce::int64) std::llround (ms / 1000.0 * bpm / 60.0 * ppq) : 0;

                auto found = channels.find (c.source);
                if (found == channels.end() || found->second.type == typeAutomation)
                {
                    ++report.automationClipsSkipped;
                    continue;
                }
                found->second.usedAsAudioClip = true;
            }

            highestTrack = juce::jmax (highestTrack, track);
            clips.push_back (c);
        }
    }

    for (auto& [patternId, notes] : patternNotesById)
        for (auto& n : notes)
            if (auto found = channels.find (n.channel); found != channels.end())
                found->second.hasNotes = true;

    std::set<int> usedInserts;
    for (auto& [index, ch] : channels)
        if (ch.routedTo > 0 && ch.type != typeAutomation && ch.type != typeLayer)
            usedInserts.insert (ch.routedTo);
    for (auto& slot : slots)
        if (slot.insert > 0)
            usedInserts.insert (slot.insert);

    std::map<int, int> insertMap { { 0, 0 } };
    for (int flInsert : usedInserts)
    {
        const int next = (int) insertMap.size();
        if (next <= numMixerInserts)
            insertMap[flInsert] = next;
        else
            ++report.insertsBeyondRange;
    }

    const auto mapInsert = [&insertMap] (int flInsert)
    {
        auto it = insertMap.find (flInsert);
        return it != insertMap.end() ? it->second : -1;
    };

    std::map<int, int> channelUids;
    for (auto& [index, ch] : channels)
    {
        if (ch.type == typeAutomation)
            continue;

        if (ch.type == typeLayer)
        {
            ++report.layerChannelsSkipped;
            continue;
        }

        if (! ch.hasNotes && ! ch.usedAsAudioClip && ch.sample.isEmpty() && ch.defaultName.isEmpty())
            continue;

        const auto displayName = ch.name.isNotEmpty() ? ch.name
                               : ch.vstName.isNotEmpty() ? ch.vstName
                               : ch.defaultName.isNotEmpty() ? ch.defaultName
                               : juce::File::createFileWithoutCheckingPath (ch.sample.replaceCharacter ('\\', '/')).getFileNameWithoutExtension();

        juce::ValueTree channel;
        if (ch.sample.isNotEmpty())
        {
            const auto resolved = resolveSample (ch.sample, file.getParentDirectory(), options);
            channel = project.addChannel (ch.usedAsAudioClip ? kind::audio : kind::sampler, displayName);
            channel.setProperty (id::samplePath, resolved.existsAsFile() ? resolved.getFullPathName() : ch.sample, nullptr);
            channel.setProperty (id::oneShot, ! ch.usedAsAudioClip, nullptr);
            if (! resolved.existsAsFile())
                report.missingSamples.addIfNotAlreadyThere (ch.sample);
        }
        else
        {
            const auto pluginToFind = ch.vstName.isNotEmpty() ? ch.vstName : ch.defaultName;
            const auto xml = options.findInstrumentXml && pluginToFind.isNotEmpty() ? options.findInstrumentXml (pluginToFind) : juce::String();

            if (xml.isNotEmpty())
            {
                channel = project.addChannel (kind::plugin, displayName);
                channel.setProperty (id::pluginDesc, xml, nullptr);
            }
            else
            {
                channel = project.addChannel (kind::sampler, displayName);
                report.placeholderInstruments.add (displayName + (pluginToFind.isNotEmpty() && pluginToFind != displayName ? " (" + pluginToFind + ")" : juce::String()));
            }
        }

        if (ch.volume >= 0.0f)
            channel.setProperty (id::volume, ch.volume, nullptr);
        channel.setProperty (id::pan, ch.pan, nullptr);

        channel.setProperty (id::insert, juce::jmax (0, mapInsert (ch.routedTo)), nullptr);

        channelUids[index] = channel[id::uid];
        ++report.channels;
    }

    std::set<int> usedPatterns;
    for (auto& [patternId, notes] : patternNotesById)
        if (! notes.empty())
            usedPatterns.insert (patternId);
    for (auto& c : clips)
        if (c.pattern)
            usedPatterns.insert (c.source);

    std::map<int, int> patternUids;
    auto firstPattern = project.currentPattern();
    bool reusedFirst = false;

    for (int patternId : usedPatterns)
    {
        const auto name = patternNames.count (patternId) > 0 && patternNames[patternId].isNotEmpty() ? patternNames[patternId]
                                                                                                     : "Pattern " + juce::String (patternId);
        juce::ValueTree pattern;
        if (! reusedFirst)
        {
            pattern = firstPattern;
            pattern.setProperty (id::name, name, nullptr);
            reusedFirst = true;
        }
        else
        {
            pattern = project.addPattern (name);
        }

        patternUids[patternId] = pattern[id::uid];
        ++report.patterns;

        for (auto n : patternNotesById[patternId])
        {
            auto uid = channelUids.find (n.channel);
            if (uid == channelUids.end())
                continue;

            n.channel = uid->second;
            project.addNote (pattern, n);
            ++report.notes;
        }
    }

    auto tracks = project.tracksTree();
    while (tracks.getNumChildren() <= highestTrack)
    {
        juce::ValueTree track (id::TRACK);
        track.setProperty (id::uid, project.allocateUid(), nullptr);
        track.setProperty (id::name, "Track " + juce::String (tracks.getNumChildren() + 1), nullptr);
        track.setProperty (id::mute, false, nullptr);
        tracks.appendChild (track, nullptr);
    }

    for (auto& [index, name] : trackNames)
        if (juce::isPositiveAndBelow (index, tracks.getNumChildren()) && name.isNotEmpty())
            tracks.getChild (index).setProperty (id::name, name, nullptr);

    for (auto& c : clips)
    {
        auto track = tracks.getChild (c.track);
        if (c.pattern)
        {
            auto uid = patternUids.find (c.source);
            if (uid == patternUids.end())
                continue;
            auto clip = project.addClip (track, kind::pattern, uid->second, c.start, c.length);
            clip.setProperty (id::offset, c.offset, nullptr);
            ++report.patternClips;
        }
        else
        {
            auto uid = channelUids.find (c.source);
            if (uid == channelUids.end())
                continue;
            auto clip = project.addClip (track, kind::audio, uid->second, c.start, c.length);
            clip.setProperty (id::offset, c.offset, nullptr);
            ++report.audioClips;
        }
    }

    for (int i = 1; i <= numMixerInserts; ++i)
        project.findInsert (i).setProperty (id::name, "Insert " + juce::String (i), nullptr);

    for (auto& [index, insert] : inserts)
    {
        const int target = mapInsert (index);
        if (target > 0 && insert.name.isNotEmpty())
            project.findInsert (target).setProperty (id::name, insert.name, nullptr);
    }

    for (auto& slot : slots)
    {
        const int target = mapInsert (slot.insert);
        if (target < 0 || ! juce::isPositiveAndBelow (slot.index, numFxSlots))
            continue;

        const auto effectName = slot.vstName.isNotEmpty() ? slot.vstName : slot.defaultName;
        const auto xml = options.findEffectXml ? options.findEffectXml (effectName) : juce::String();
        if (xml.isEmpty())
        {
            report.skippedEffects.addIfNotAlreadyThere (effectName);
            continue;
        }

        project.addSlot (target, slot.index, xml);
        ++report.effects;
    }

    if (project.patternsTree().getNumChildren() == 0)
        project.addPattern();

    project.setCurrentPattern (project.patternsTree().getChild (0)[id::uid]);
    if (project.channelsTree().getNumChildren() > 0)
        project.setSelectedChannel (project.channelsTree().getChild (0)[id::uid]);

    return state.createCopy();
}
}
