#pragma once

#include "ui/Widgets.h"
#include "app/Context.h"

namespace harmony::ui
{
class TopBar final : public juce::Component, private juce::Timer, private juce::ValueTree::Listener
{
public:
    TopBar (Context& context, juce::MenuBarModel& menuModel);
    ~TopBar() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void setUpdateReady (const juce::String& version);

    std::function<void()> onUpdateClicked;

private:
    void timerCallback() override;
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void refreshTransport();
    juce::String positionText() const;

    Context& ctx;
    juce::MenuBarComponent menuBar;
    juce::TextButton patternModeButton { "PAT" }, songModeButton { "SONG" };
    IconButton playButton { "Play / pause (Space)", icons::play(), true };
    IconButton stopButton { "Stop (Ctrl+Space)", icons::stop(), true };
    IconButton recordButton { "Record (R)", icons::record(), true };
    IconButton metronomeButton { "Metronome (Ctrl+M)", icons::metronome() };
    IconButton paletteButton { "Command palette (Ctrl+K)", icons::search() };
    IconButton settingsButton { "Settings (Ctrl+,)", icons::gear() };
    IconButton updateButton { "Update ready: restart to install", icons::download() };
    NumberBox bpmBox { 10.0, 999.0, 0.01, 2 };
    LevelMeter masterMeter { false };

    juce::Rectangle<int> brandArea, timeArea, cpuArea;
    juce::String timeText, cpuText;
    bool showSeconds = false;
};
}
