#pragma once

#include "ui/Widgets.h"
#include "app/Context.h"

namespace harmony::ui
{
class MixerStrip;

class MixerView final : public juce::Component,
                        public juce::DragAndDropTarget,
                        private juce::Timer,
                        private juce::ValueTree::Listener
{
public:
    explicit MixerView (Context& context);
    ~MixerView() override;

    bool isInterestedInDragSource (const SourceDetails&) override;
    void itemDragMove (const SourceDetails&) override;
    void itemDragExit (const SourceDetails&) override;
    void itemDropped (const SourceDetails&) override;
    int getDropHighlight() const noexcept { return dropTarget; }

    static juce::PopupMenu buildEffectMenu (Context& ctx, std::function<void (const juce::String& identifier)> onChosen);

    void focusInsert (int index);
    int getSelectedInsert() const noexcept { return selected; }
    void selectInsert (int index);

    void paint (juce::Graphics&) override;
    void resized() override;

    Context& ctx;

private:
    void timerCallback() override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeRedirected (juce::ValueTree&) override;
    void rebuild();
    MixerStrip* stripAt (juce::Point<int> positionInView) const;

    juce::Viewport viewport;
    juce::Component holder;
    std::vector<std::unique_ptr<MixerStrip>> strips;
    int selected = 0;
    int dropTarget = -1;
};
}
