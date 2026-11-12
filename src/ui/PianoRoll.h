#pragma once

#include "ui/SmoothView.h"
#include "ui/Widgets.h"
#include "app/Context.h"

namespace harmony::ui
{
class PianoRoll final : public TimelineCanvas,
                        public EditTarget,
                        private juce::ValueTree::Listener
{
public:
    explicit PianoRoll (Context& context);
    ~PianoRoll() override;

    void setChannel (int uid);
    void setGhostsVisible (bool shouldShow);
    int getChannel() const noexcept { return channelUid; }
    void insertNotes (const std::vector<NoteData>& notes, bool replaceExisting);
    void showChordGenerator();

    void editCut() override;
    void editCopy() override;
    void editPaste() override;
    void editDelete() override;
    void editSelectAll() override;
    void editDuplicate() override;
    void editQuantize() override;
    void editHumanize() override;
    void editToggleMute() override;
    void setTool (Tool) override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    enum class Drag { none, move, resizeRight, resizeLeft, marquee, erase, velocity, keyboard, ruler };

    struct Original
    {
        juce::ValueTree tree;
        NoteData data;
    };

    juce::Rectangle<int> getGridArea() const override { return gridArea; }
    double getContentHeight() const override { return 128.0 * rowHeight.target; }
    double getContentLengthTicks() const override;
    void animationTick() override;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    juce::ValueTree pattern() const;
    std::vector<juce::ValueTree> channelNotes() const;
    juce::ValueTree hitTest (juce::Point<float> p, bool& rightEdge, bool& leftEdge) const;
    juce::Rectangle<float> noteRect (const NoteData& n) const;
    float keyToY (int key) const;
    int yToKey (float y) const;
    int constrainKey (int key) const;
    void pruneSelection();
    void captureOriginals();
    void preview (int key);
    void stopPreview();
    void applyVelocity (juce::Point<float> p, bool paint);
    std::vector<juce::ValueTree> velocityTargetsAt (float x) const;
    void nudgeVelocity (const std::vector<juce::ValueTree>& notes, float delta);
    juce::ValueTree ghostAt (juce::Point<float> p) const;
    double playheadTick() const;
    double rulerTickToTransport (double patternTick) const;
    void eraseAt (juce::Point<float> p);
    void sliceAt (double tick, const juce::ValueTree& hit);
    void updateChordLabel();
    void refreshToolbar();
    void showChannelMenu();

    void drawKeyboard (juce::Graphics&) const;
    void drawRows (juce::Graphics&) const;
    void drawNotes (juce::Graphics&) const;
    void drawVelocityLane (juce::Graphics&) const;

    Context& ctx;
    int channelUid = 0;
    Tool tool = Tool::draw;
    bool ghosts = true, scaleLock = false;
    juce::Array<juce::ValueTree> selection;

    Drag drag = Drag::none;
    std::vector<Original> originals;
    juce::ValueTree anchor;
    juce::Point<float> dragStart;
    double dragStartTick = 0.0;
    int dragStartKey = 60;
    juce::Rectangle<float> marquee;
    juce::Array<juce::ValueTree> marqueeBase;
    juce::int64 lastLength = ticksPerStep;
    float lastVelocity = 0.78f;
    int previewKey = -1, hoverKey = -1;
    float lastPlayheadX = -1.0f;
    std::vector<juce::ValueTree> velocityTargets;
    std::vector<float> velocityOriginals;
    float velocityGrabValue = 0.0f;
    juce::ValueTree hoverStem, lastAddedNote;
    double lastWheelEdit = 0.0;

    juce::Rectangle<int> toolbarArea, rulerArea, keysArea, gridArea, velocityArea;

    IconButton drawButton { "Draw (P)", icons::pencil() };
    IconButton selectButton { "Select (E)", icons::select() };
    IconButton sliceButton { "Slice (C)", icons::slice() };
    IconButton muteButton { "Mute (T)", icons::mute() };
    IconButton ghostButton { "Ghost notes", icons::ghost() };
    IconButton scaleLockButton { "Scale lock: new and moved notes stay in key", icons::scale() };
    IconButton quantizeButton { "Quantize (Ctrl+Q)", icons::magnet() };
    IconButton humanizeButton { "Humanize (Alt+H)", icons::wave() };
    IconButton chordButton { "Chord generator (Ctrl+G)", icons::wand() };
    juce::TextButton channelButton;
    juce::ComboBox snapBox, rootBox, scaleBox;
    juce::String chordText;

    static std::vector<NoteData> clipboard;
};
}
