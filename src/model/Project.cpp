#include "model/Project.h"

namespace harmony
{
namespace palette
{
    static const juce::uint32 channelColours[] = {
        0xfff7b2ca, 0xffc7b2fa, 0xffa8d8f0, 0xffa6e3c4, 0xfff5d49a,
        0xfff0a8a0, 0xffb8c4f5, 0xffe6b3e8, 0xff9fe0dc, 0xffd8e39f
    };

    juce::Colour channelColour (int index)
    {
        const auto n = (int) std::size (channelColours);
        return juce::Colour (channelColours[((index % n) + n) % n]);
    }

    juce::Colour patternColour (int index)
    {
        return channelColour (index * 3 + 1);
    }
}

juce::ValueTree findChildWithUid (const juce::ValueTree& parent, int uid)
{
    for (auto child : parent)
        if ((int) child[id::uid] == uid)
            return child;

    return {};
}

Project::Project()
    : state (createDefaultState())
{
    state.addListener (this);
}

Project::~Project()
{
    state.removeListener (this);
}

juce::ValueTree Project::createDefaultState()
{
    juce::ValueTree root (id::PROJECT);
    root.setProperty (id::formatVersion, projectFormatVersion, nullptr);
    root.setProperty (id::appVersion, HARMONY_VERSION_STRING, nullptr);
    root.setProperty (id::name, "Untitled", nullptr);
    root.setProperty (id::bpm, 128.0, nullptr);
    root.setProperty (id::numerator, 4, nullptr);
    root.setProperty (id::denominator, 4, nullptr);
    root.setProperty (id::swing, 0.0f, nullptr);
    root.setProperty (id::loopStart, 0, nullptr);
    root.setProperty (id::loopEnd, 0, nullptr);
    root.setProperty (id::scaleRoot, 0, nullptr);
    root.setProperty (id::scaleType, "Minor", nullptr);
    root.setProperty (id::selectedChannel, 0, nullptr);

    int uid = 1;

    juce::ValueTree channels (id::CHANNELS);
    juce::ValueTree patterns (id::PATTERNS);
    juce::ValueTree pattern (id::PATTERN);
    pattern.setProperty (id::uid, uid, nullptr);
    pattern.setProperty (id::name, "Pattern 1", nullptr);
    pattern.setProperty (id::colour, palette::patternColour (0).toString(), nullptr);
    pattern.setProperty (id::steps, 16, nullptr);
    patterns.appendChild (pattern, nullptr);
    root.setProperty (id::currentPattern, uid, nullptr);
    ++uid;

    juce::ValueTree tracks (id::TRACKS);
    for (int i = 0; i < defaultTrackCount; ++i)
    {
        juce::ValueTree track (id::TRACK);
        track.setProperty (id::uid, uid++, nullptr);
        track.setProperty (id::name, "Track " + juce::String (i + 1), nullptr);
        track.setProperty (id::mute, false, nullptr);
        tracks.appendChild (track, nullptr);
    }

    juce::ValueTree mixer (id::MIXER);
    for (int i = 0; i <= numMixerInserts; ++i)
    {
        juce::ValueTree insert (id::INSERT);
        insert.setProperty (id::index, i, nullptr);
        insert.setProperty (id::name, i == 0 ? juce::String ("Master") : "Insert " + juce::String (i), nullptr);
        insert.setProperty (id::volume, 0.8f, nullptr);
        insert.setProperty (id::pan, 0.0f, nullptr);
        insert.setProperty (id::mute, false, nullptr);
        insert.setProperty (id::solo, false, nullptr);
        mixer.appendChild (insert, nullptr);
    }

    root.appendChild (channels, nullptr);
    root.appendChild (patterns, nullptr);
    root.appendChild (tracks, nullptr);
    root.appendChild (mixer, nullptr);
    root.setProperty (id::nextUid, uid, nullptr);
    return root;
}

void Project::beginTransaction (const juce::String& name)
{
    undo.beginNewTransaction (name);
}

int Project::allocateUid()
{
    const int uid = juce::jmax (1, (int) state[id::nextUid]);
    state.setProperty (id::nextUid, uid + 1, nullptr);
    return uid;
}

double Project::getBpm() const            { return juce::jlimit (10.0, 999.0, (double) state.getProperty (id::bpm, 128.0)); }
void Project::setBpm (double bpm)         { state.setProperty (id::bpm, juce::jlimit (10.0, 999.0, bpm), &undo); }
int Project::getNumerator() const         { return juce::jlimit (1, 32, (int) state.getProperty (id::numerator, 4)); }
int Project::getDenominator() const       { return juce::jlimit (1, 32, (int) state.getProperty (id::denominator, 4)); }
float Project::getSwing() const           { return juce::jlimit (0.0f, 1.0f, (float) state.getProperty (id::swing, 0.0f)); }

void Project::setTimeSignature (int numerator, int denominator)
{
    state.setProperty (id::numerator, juce::jlimit (1, 32, numerator), &undo);
    state.setProperty (id::denominator, juce::jlimit (1, 32, denominator), &undo);
}

juce::int64 Project::ticksPerBeat() const { return (juce::int64) ppq * 4 / getDenominator(); }
juce::int64 Project::ticksPerBar() const  { return ticksPerBeat() * getNumerator(); }

juce::ValueTree Project::channelsTree() const { return state.getChildWithName (id::CHANNELS); }
juce::ValueTree Project::patternsTree() const { return state.getChildWithName (id::PATTERNS); }
juce::ValueTree Project::tracksTree() const   { return state.getChildWithName (id::TRACKS); }
juce::ValueTree Project::mixerTree() const    { return state.getChildWithName (id::MIXER); }

juce::ValueTree Project::findChannel (int uid) const { return findChildWithUid (channelsTree(), uid); }
juce::ValueTree Project::findPattern (int uid) const { return findChildWithUid (patternsTree(), uid); }
juce::ValueTree Project::findTrack (int uid) const   { return findChildWithUid (tracksTree(), uid); }

juce::ValueTree Project::findInsert (int index) const
{
    for (auto insert : mixerTree())
        if ((int) insert[id::index] == index)
            return insert;

    return {};
}

int Project::nextFreeInsert() const
{
    std::vector<bool> used ((size_t) numMixerInserts + 1, false);

    for (auto ch : channelsTree())
    {
        const int idx = ch[id::insert];
        if (idx > 0 && idx <= numMixerInserts)
            used[(size_t) idx] = true;
    }

    for (int i = 1; i <= numMixerInserts; ++i)
        if (! used[(size_t) i])
            return i;

    return 0;
}

juce::ValueTree Project::addChannel (const juce::String& kindName, const juce::String& name)
{
    auto channels = channelsTree();
    const int insertIndex = nextFreeInsert();

    juce::ValueTree ch (id::CHANNEL);
    ch.setProperty (id::uid, allocateUid(), nullptr);
    ch.setProperty (id::kind, kindName, nullptr);
    ch.setProperty (id::name, name, nullptr);
    ch.setProperty (id::colour, palette::channelColour (channels.getNumChildren()).toString(), nullptr);
    ch.setProperty (id::volume, 0.78f, nullptr);
    ch.setProperty (id::pan, 0.0f, nullptr);
    ch.setProperty (id::mute, false, nullptr);
    ch.setProperty (id::solo, false, nullptr);
    ch.setProperty (id::insert, insertIndex, nullptr);
    ch.setProperty (id::rootNote, 60, nullptr);
    ch.setProperty (id::oneShot, kindName == kind::sampler || kindName == kind::audio, nullptr);
    ch.setProperty (id::attack, 0.001f, nullptr);
    ch.setProperty (id::release, 0.08f, nullptr);
    channels.appendChild (ch, &undo);

    if (insertIndex > 0)
    {
        auto insert = findInsert (insertIndex);
        if (insert.isValid() && insert[id::name].toString().startsWith ("Insert "))
            insert.setProperty (id::name, name, &undo);
    }

    if (selectedChannel() == 0)
        setSelectedChannel (ch[id::uid]);

    return ch;
}

void Project::removeChannel (int uid)
{
    auto ch = findChannel (uid);
    if (! ch.isValid())
        return;

    for (auto pattern : patternsTree())
        for (int i = pattern.getNumChildren(); --i >= 0;)
            if ((int) pattern.getChild (i)[id::channel] == uid)
                pattern.removeChild (i, &undo);

    for (auto track : tracksTree())
        for (int i = track.getNumChildren(); --i >= 0;)
        {
            auto clip = track.getChild (i);
            if (clip[id::kind].toString() == kind::audio && (int) clip[id::channel] == uid)
                track.removeChild (i, &undo);
        }

    channelsTree().removeChild (ch, &undo);

    if (selectedChannel() == uid)
        setSelectedChannel (channelsTree().getNumChildren() > 0 ? (int) channelsTree().getChild (0)[id::uid] : 0);
}

juce::ValueTree Project::cloneChannel (int uid)
{
    auto source = findChannel (uid);
    if (! source.isValid())
        return {};

    auto copy = source.createCopy();
    copy.setProperty (id::uid, allocateUid(), nullptr);
    copy.setProperty (id::name, source[id::name].toString() + " #2", nullptr);
    channelsTree().addChild (copy, channelsTree().indexOf (source) + 1, &undo);
    return copy;
}

juce::ValueTree Project::addPattern (const juce::String& name)
{
    auto patterns = patternsTree();
    const int count = patterns.getNumChildren();

    juce::ValueTree pattern (id::PATTERN);
    pattern.setProperty (id::uid, allocateUid(), nullptr);
    pattern.setProperty (id::name, name.isNotEmpty() ? name : "Pattern " + juce::String (count + 1), nullptr);
    pattern.setProperty (id::colour, palette::patternColour (count).toString(), nullptr);
    pattern.setProperty (id::steps, 16, nullptr);
    patterns.appendChild (pattern, &undo);
    return pattern;
}

void Project::removePattern (int uid)
{
    auto pattern = findPattern (uid);
    if (! pattern.isValid() || patternsTree().getNumChildren() <= 1)
        return;

    for (auto track : tracksTree())
        for (int i = track.getNumChildren(); --i >= 0;)
        {
            auto clip = track.getChild (i);
            if (clip[id::kind].toString() == kind::pattern && (int) clip[id::pattern] == uid)
                track.removeChild (i, &undo);
        }

    const int index = patternsTree().indexOf (pattern);
    patternsTree().removeChild (pattern, &undo);

    if ((int) state[id::currentPattern] == uid)
        setCurrentPattern (patternsTree().getChild (juce::jmax (0, index - 1))[id::uid]);
}

juce::ValueTree Project::clonePattern (int uid)
{
    auto source = findPattern (uid);
    if (! source.isValid())
        return {};

    auto copy = source.createCopy();
    copy.setProperty (id::uid, allocateUid(), nullptr);
    copy.setProperty (id::name, source[id::name].toString() + " (copy)", nullptr);
    copy.setProperty (id::colour, palette::patternColour (patternsTree().getNumChildren()).toString(), nullptr);
    patternsTree().addChild (copy, patternsTree().indexOf (source) + 1, &undo);
    return copy;
}

juce::ValueTree Project::currentPattern() const
{
    auto p = findPattern (state[id::currentPattern]);
    return p.isValid() ? p : patternsTree().getChild (0);
}

void Project::setCurrentPattern (int uid)       { state.setProperty (id::currentPattern, uid, nullptr); }
int Project::selectedChannel() const            { return state[id::selectedChannel]; }
void Project::setSelectedChannel (int uid)      { state.setProperty (id::selectedChannel, uid, nullptr); }

juce::ValueTree Project::addNote (juce::ValueTree pattern, const NoteData& note)
{
    juce::ValueTree n (id::NOTE);
    writeNote (n, note, nullptr);
    pattern.appendChild (n, &undo);
    return n;
}

NoteData Project::readNote (const juce::ValueTree& note)
{
    NoteData d;
    d.channel = note[id::channel];
    d.key = juce::jlimit (0, 127, (int) note[id::key]);
    d.start = juce::jmax ((juce::int64) 0, (juce::int64) note[id::start]);
    d.length = juce::jmax ((juce::int64) 1, (juce::int64) note[id::length]);
    d.velocity = juce::jlimit (0.0f, 1.0f, (float) note.getProperty (id::velocity, 0.78f));
    d.muted = note[id::muted];
    return d;
}

void Project::writeNote (juce::ValueTree& note, const NoteData& d, juce::UndoManager* um)
{
    note.setProperty (id::channel, d.channel, um);
    note.setProperty (id::key, juce::jlimit (0, 127, d.key), um);
    note.setProperty (id::start, juce::jmax ((juce::int64) 0, d.start), um);
    note.setProperty (id::length, juce::jmax ((juce::int64) 1, d.length), um);
    note.setProperty (id::velocity, juce::jlimit (0.0f, 1.0f, d.velocity), um);
    note.setProperty (id::muted, d.muted, um);
}

juce::ValueTree Project::addClip (juce::ValueTree track, const juce::String& kindName, int sourceUid,
                                  juce::int64 start, juce::int64 length)
{
    juce::ValueTree clip (id::CLIP);
    clip.setProperty (id::uid, allocateUid(), nullptr);
    clip.setProperty (id::kind, kindName, nullptr);
    clip.setProperty (kindName == kind::pattern ? id::pattern : id::channel, sourceUid, nullptr);
    clip.setProperty (id::start, juce::jmax ((juce::int64) 0, start), nullptr);
    clip.setProperty (id::length, juce::jmax ((juce::int64) 1, length), nullptr);
    clip.setProperty (id::offset, 0, nullptr);
    clip.setProperty (id::muted, false, nullptr);
    track.appendChild (clip, &undo);
    return clip;
}

juce::int64 Project::patternLength (const juce::ValueTree& pattern) const
{
    const juce::int64 bar = ticksPerBar();
    juce::int64 end = (juce::int64) juce::jmax (1, (int) pattern.getProperty (id::steps, 16)) * ticksPerStep;

    for (auto note : pattern)
        end = juce::jmax (end, (juce::int64) note[id::start] + (juce::int64) note[id::length]);

    return ((end + bar - 1) / bar) * bar;
}

juce::int64 Project::songLength() const
{
    juce::int64 end = 0;

    for (auto track : tracksTree())
        for (auto clip : track)
            end = juce::jmax (end, (juce::int64) clip[id::start] + (juce::int64) clip[id::length]);

    return end;
}

juce::ValueTree Project::addSlot (int insertIndex, int slotIndex, const juce::String& pluginDescXml)
{
    auto insert = findInsert (insertIndex);
    if (! insert.isValid())
        return {};

    for (int i = insert.getNumChildren(); --i >= 0;)
        if ((int) insert.getChild (i)[id::index] == slotIndex)
            insert.removeChild (i, &undo);

    juce::ValueTree slot (id::SLOT);
    slot.setProperty (id::uid, allocateUid(), nullptr);
    slot.setProperty (id::index, slotIndex, nullptr);
    slot.setProperty (id::pluginDesc, pluginDescXml, nullptr);
    slot.setProperty (id::bypass, false, nullptr);
    insert.appendChild (slot, &undo);
    return slot;
}

bool Project::writeStateToFile (const juce::ValueTree& s, const juce::File& target)
{
    const auto temp = target.getSiblingFile (target.getFileName() + ".saving");
    temp.deleteFile();

    {
        juce::FileOutputStream out (temp);
        if (! out.openedOk())
            return false;

        juce::GZIPCompressorOutputStream gz (out, 6, juce::GZIPCompressorOutputStream::windowBitsGZIP);
        if (auto xml = s.createXml())
            gz.writeText (xml->toString(), false, false, nullptr);
        gz.flush();
    }

    return temp.existsAsFile() && temp.moveFileTo (target);
}

juce::ValueTree Project::readStateFromFile (const juce::File& source, juce::String& error)
{
    juce::MemoryBlock data;
    if (! source.loadFileAsData (data) || data.getSize() < 4)
    {
        error = "The file could not be read.";
        return {};
    }

    juce::String text;
    const auto* bytes = static_cast<const juce::uint8*> (data.getData());

    if (bytes[0] == 0x1f && bytes[1] == 0x8b)
    {
        juce::MemoryInputStream raw (data, false);
        juce::GZIPDecompressorInputStream gz (&raw, false, juce::GZIPDecompressorInputStream::gzipFormat);
        text = gz.readEntireStreamAsString();
    }
    else
    {
        text = data.toString();
    }

    auto xml = juce::parseXML (text);
    if (xml == nullptr || ! xml->hasTagName (id::PROJECT.toString()))
    {
        error = "This is not a Harmony project.";
        return {};
    }

    auto loaded = juce::ValueTree::fromXml (*xml);
    if ((int) loaded[id::formatVersion] > projectFormatVersion)
    {
        error = "This project was made with a newer version of Harmony. Please update.";
        return {};
    }

    upgrade (loaded);
    return loaded;
}

void Project::upgrade (juce::ValueTree& s)
{
    auto defaults = createDefaultState();

    for (int i = 0; i < defaults.getNumProperties(); ++i)
    {
        const auto name = defaults.getPropertyName (i);
        if (! s.hasProperty (name))
            s.setProperty (name, defaults[name], nullptr);
    }

    for (auto childType : { id::CHANNELS, id::PATTERNS, id::TRACKS, id::MIXER })
        if (! s.getChildWithName (childType).isValid())
            s.appendChild (defaults.getChildWithName (childType).createCopy(), nullptr);

    auto mixer = s.getChildWithName (id::MIXER);
    for (int i = 0; i <= numMixerInserts; ++i)
    {
        bool found = false;
        for (auto insert : mixer)
            found = found || (int) insert[id::index] == i;

        if (! found)
            mixer.appendChild (defaults.getChildWithName (id::MIXER).getChild (i).createCopy(), nullptr);
    }

    if (s.getChildWithName (id::PATTERNS).getNumChildren() == 0)
        s.getChildWithName (id::PATTERNS).appendChild (defaults.getChildWithName (id::PATTERNS).getChild (0).createCopy(), nullptr);

    int maxUid = 0;
    std::function<void (const juce::ValueTree&)> scan = [&] (const juce::ValueTree& t)
    {
        maxUid = juce::jmax (maxUid, (int) t[id::uid]);
        for (auto c : t)
            scan (c);
    };
    scan (s);
    s.setProperty (id::nextUid, juce::jmax ((int) s[id::nextUid], maxUid + 1), nullptr);
    s.setProperty (id::formatVersion, projectFormatVersion, nullptr);
}

juce::Result Project::saveTo (const juce::File& target)
{
    if (onBeforeSave)
        onBeforeSave();

    state.setProperty (id::appVersion, HARMONY_VERSION_STRING, nullptr);
    state.setProperty (id::name, target.getFileNameWithoutExtension(), nullptr);

    if (! writeStateToFile (state, target))
        return juce::Result::fail ("Could not write " + target.getFullPathName());

    file = target;
    markClean();
    return juce::Result::ok();
}

juce::Result Project::loadFrom (const juce::File& source)
{
    juce::String error;
    auto loaded = readStateFromFile (source, error);
    if (! loaded.isValid())
        return juce::Result::fail (error);

    replaceState (loaded);
    file = source;
    markClean();
    return juce::Result::ok();
}

void Project::resetToDefault()
{
    replaceState (createDefaultState());
    file = juce::File();
    markClean();
}

void Project::replaceState (const juce::ValueTree& newState)
{
    state.copyPropertiesAndChildrenFrom (newState, nullptr);
    undo.clearUndoHistory();

    if (onStateReplaced)
        onStateReplaced();
}

juce::String Project::getDisplayName() const
{
    return file.existsAsFile() ? file.getFileNameWithoutExtension() : juce::String ("Untitled");
}

void Project::touch() noexcept
{
    dirty = true;
    ++changeCounter;
}

void Project::valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier& property)
{
    if (property != id::selectedChannel && property != id::currentPattern)
        touch();
}

void Project::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&)          { touch(); }
void Project::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)   { touch(); }
void Project::valueTreeChildOrderChanged (juce::ValueTree&, int, int)           { touch(); }
}
