#pragma once

#include "ui/Widgets.h"
#include "app/Context.h"

namespace harmony::ui
{
class PatternBar final : public juce::Component, public juce::SettableTooltipClient, private juce::ValueTree::Listener
{
public:
    explicit PatternBar (Context& context);
    ~PatternBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    struct Chip
    {
        int uid;
        juce::Rectangle<float> bounds;
    };

    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;

    void layoutChips();
    int chipAt (juce::Point<float> p) const;
    void showMenu (int uid);
    void scrollToCurrent();

    Context& ctx;
    std::vector<Chip> chips;
    juce::Rectangle<float> addButton;
    float scroll = 0.0f, contentWidth = 0.0f;
    int hovered = -1, pressed = -1;
    bool hoverAdd = false, dragStarted = false;
};
}
