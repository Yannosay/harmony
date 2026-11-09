#pragma once

#include "ui/Widgets.h"
#include "app/Context.h"

namespace harmony::ui
{
class ChannelRow;

class ChannelRack final : public juce::Component,
                          public juce::DragAndDropTarget,
                          public juce::FileDragAndDropTarget,
                          private juce::ValueTree::Listener,
                          private juce::Timer
{
public:
    explicit ChannelRack (Context& context);
    ~ChannelRack() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;
    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

    void showAddMenu (juce::Component* target);
    void showDrumGenerator();
    void showPatternMenu();

    Context& ctx;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;
    void timerCallback() override;
    void rebuildRows();
    void refreshHeader();

    juce::Viewport viewport;
    juce::Component rowsHolder;
    std::vector<std::unique_ptr<ChannelRow>> rows;

    juce::TextButton patternButton;
    IconButton newPatternButton { "New pattern (F4)", icons::plus() };
    NumberBox stepsBox { 4.0, 64.0, 1.0, 0 };
    Knob swingKnob { 0.0, 1.0, 0.0, "Swing" };
    IconButton addButton { "Add channel", icons::plus() };
    IconButton drumButton { "Drum generator (Ctrl+Shift+G)", icons::drum() };
};
}
