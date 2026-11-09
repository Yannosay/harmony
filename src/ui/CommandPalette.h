#pragma once

#include "ui/Widgets.h"

namespace harmony::ui
{
class CommandPalette final : public juce::Component, private juce::ListBoxModel, private juce::KeyListener
{
public:
    explicit CommandPalette (juce::ApplicationCommandManager& commands);

    void paint (juce::Graphics&) override;
    void resized() override;
    void visibilityChanged() override;

    std::function<void()> onDone;

    static int fuzzyScore (const juce::String& query, const juce::String& text);

private:
    struct Item
    {
        juce::CommandID command;
        juce::String name, category, keys;
        int score;
    };

    bool keyPressed (const juce::KeyPress&, juce::Component*) override;
    int getNumRows() override { return (int) visible.size(); }
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;
    void returnKeyPressed (int row) override;
    void filter();
    void run (int row);

    juce::ApplicationCommandManager& commands;
    juce::TextEditor input;
    juce::ListBox list;
    std::vector<Item> all, visible;
};
}
