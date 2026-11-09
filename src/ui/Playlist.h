#pragma once

#include "ui/SmoothView.h"
#include "ui/Widgets.h"
#include "app/Context.h"
#include <map>

namespace harmony::ui
{
class Playlist final : public TimelineCanvas,
                       public EditTarget,
                       public juce::DragAndDropTarget,
                       public juce::FileDragAndDropTarget,
                       private juce::ValueTree::Listener
{
public:
    explicit Playlist (Context& context);
    ~Playlist() override;

    void editCut() override;
    void editCopy() override;
    void editPaste() override;
    void editDelete() override;
    void editSelectAll() override;
    void editDuplicate() override;
    void editToggleMute() override;
    void setTool (Tool) override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int x, int y) override;

private:
    enum class Drag { none, move, resizeRight, resizeLeft, marquee, erase, ruler, loop };

    struct Original
    {
        juce::ValueTree clip;
        int track;
        juce::int64 start, length, offset;
    };

    juce::Rectangle<int> getGridArea() const override { return gridArea; }
    double getContentHeight() const override;
    double getContentLengthTicks() const override;
    void animationTick() override;

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    int trackAtY (float y) const;
    float trackY (int index) const;
    juce::Rectangle<float> clipRect (const juce::ValueTree& clip) const;
    juce::ValueTree hitTest (juce::Point<float> p, bool& rightEdge, bool& leftEdge) const;
    juce::int64 sourceLength (const juce::ValueTree& clip) const;
    void pruneSelection();
    void captureOriginals();
    void moveClipToTrack (juce::ValueTree& clip, int trackIndex);
    void sliceAt (const juce::ValueTree& clip, juce::int64 tick);
    void showTrackMenu (int trackIndex);
    void showClipMenu (juce::ValueTree clip);
    void refreshToolbar();
    const std::vector<std::pair<float, float>>& peaksFor (const engine::SampleData& sample);

    void drawTracks (juce::Graphics&) const;
    void drawHeaders (juce::Graphics&) const;
    void drawClip (juce::Graphics&, const juce::ValueTree& clip, juce::Rectangle<float> r, bool selected);

    Context& ctx;
    Tool tool = Tool::draw;
    juce::Array<juce::ValueTree> selection;
    Drag drag = Drag::none;
    std::vector<Original> originals;
    juce::ValueTree anchor;
    juce::Point<float> dragStart;
    double dragStartTick = 0.0;
    int dragStartTrack = 0;
    juce::Rectangle<float> marquee;
    juce::int64 loopAnchor = 0;
    float lastPlayheadX = -1.0f;

    juce::Rectangle<int> toolbarArea, rulerArea, headerArea, gridArea;

    IconButton drawButton { "Draw (P)", icons::pencil() };
    IconButton selectButton { "Select (E)", icons::select() };
    IconButton sliceButton { "Slice (C)", icons::slice() };
    IconButton muteButton { "Mute (T)", icons::mute() };
    IconButton followButton { "Follow playhead", icons::playlist() };
    juce::ComboBox snapBox;
    juce::TextButton patternButton;
    bool follow = true;

    std::map<const engine::SampleData*, std::vector<std::pair<float, float>>> peakCache;
    static std::vector<juce::ValueTree> clipboard;
};
}
