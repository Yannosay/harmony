#pragma once

#include "model/Ids.h"
#include <juce_events/juce_events.h>
#include <juce_graphics/juce_graphics.h>
#include <optional>

namespace harmony
{
namespace palette
{
    inline const juce::Colour notePink { 0xfff7b2ca };
    juce::Colour channelColour (int index);
    juce::Colour patternColour (int index);
}

struct NoteData
{
    int channel = 0;
    int key = 60;
    juce::int64 start = 0;
    juce::int64 length = ticksPerStep;
    float velocity = 0.78f;
    bool muted = false;
};

class Project final : private juce::ValueTree::Listener
{
public:
    Project();
    ~Project() override;

    static juce::ValueTree createDefaultState();

    juce::ValueTree& getState() noexcept { return state; }
    const juce::ValueTree& getState() const noexcept { return state; }
    juce::UndoManager& getUndo() noexcept { return undo; }
    juce::UndoManager* um() noexcept { return &undo; }

    void beginTransaction (const juce::String& name = {});

    int allocateUid();

    double getBpm() const;
    void setBpm (double bpm);
    int getNumerator() const;
    int getDenominator() const;
    void setTimeSignature (int numerator, int denominator);
    juce::int64 ticksPerBar() const;
    juce::int64 ticksPerBeat() const;
    float getSwing() const;

    juce::ValueTree channelsTree() const;
    juce::ValueTree patternsTree() const;
    juce::ValueTree tracksTree() const;
    juce::ValueTree mixerTree() const;

    juce::ValueTree findChannel (int uid) const;
    juce::ValueTree findPattern (int uid) const;
    juce::ValueTree findTrack (int uid) const;
    juce::ValueTree findInsert (int index) const;

    juce::ValueTree addChannel (const juce::String& kind, const juce::String& name);
    void removeChannel (int uid);
    juce::ValueTree cloneChannel (int uid);
    int nextFreeInsert() const;

    juce::ValueTree addPattern (const juce::String& name = {});
    void removePattern (int uid);
    juce::ValueTree clonePattern (int uid);
    juce::ValueTree currentPattern() const;
    void setCurrentPattern (int uid);
    int selectedChannel() const;
    void setSelectedChannel (int uid);

    juce::ValueTree addNote (juce::ValueTree pattern, const NoteData& note);
    static NoteData readNote (const juce::ValueTree& note);
    static void writeNote (juce::ValueTree& note, const NoteData& data, juce::UndoManager* undo);

    juce::ValueTree addClip (juce::ValueTree track, const juce::String& kind, int sourceUid,
                             juce::int64 start, juce::int64 length);

    juce::int64 patternLength (const juce::ValueTree& pattern) const;
    juce::int64 songLength() const;
    std::optional<double> patternPositionAt (int patternUid, double songTick) const;
    juce::ValueTree findPatternClipAt (int patternUid, double songTick) const;

    juce::ValueTree addSlot (int insertIndex, int slotIndex, const juce::String& pluginDescXml);

    juce::Result saveTo (const juce::File& file);
    juce::Result loadFrom (const juce::File& file);
    void resetToDefault();
    void replaceState (const juce::ValueTree& newState);
    static juce::ValueTree readStateFromFile (const juce::File& file, juce::String& error);
    static bool writeStateToFile (const juce::ValueTree& state, const juce::File& file);

    juce::File getFile() const { return file; }
    void setFile (const juce::File& f) { file = f; }
    juce::String getDisplayName() const;
    bool isDirty() const noexcept { return dirty; }
    void markClean() noexcept { dirty = false; changeCounter = 0; }
    juce::uint64 getChangeCounter() const noexcept { return changeCounter; }

    std::function<void()> onStateReplaced;
    std::function<void()> onBeforeSave;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;
    void touch() noexcept;
    static void upgrade (juce::ValueTree& state);

    juce::ValueTree state;
    juce::UndoManager undo { 0, 400 };
    juce::File file;
    bool dirty = false;
    juce::uint64 changeCounter = 0;
};

juce::ValueTree findChildWithUid (const juce::ValueTree& parent, int uid);
}
