#pragma once

#include "ui/Theme.h"

namespace harmony::ui
{
class IconButton : public juce::Button
{
public:
    IconButton (const juce::String& name, juce::Path icon, bool filledIcon = false);

    void setIcon (juce::Path newIcon, bool filledIcon);
    void setActiveColour (juce::Colour c) { activeColour = c; repaint(); }
    void setFramed (bool shouldDrawFrame) { framed = shouldDrawFrame; repaint(); }
    void paintButton (juce::Graphics&, bool over, bool down) override;

private:
    juce::Path icon;
    bool filled;
    bool framed = false;
    juce::Colour activeColour = colours::accent;
};

class PropertyBinding final : private juce::ValueTree::Listener
{
public:
    PropertyBinding() = default;
    ~PropertyBinding() override;

    void bind (juce::ValueTree tree, juce::Identifier property, juce::UndoManager* undo, std::function<void (const juce::var&)> onExternalChange);
    void set (const juce::var& value, bool undoable = true);
    juce::var get() const;
    bool isBound() const noexcept { return tree.isValid(); }
    void beginGesture (const juce::String& name);

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;

    juce::ValueTree tree;
    juce::Identifier property;
    juce::UndoManager* undo = nullptr;
    std::function<void (const juce::var&)> onChange;
};

class Knob final : public juce::Slider
{
public:
    Knob (double minimum, double maximum, double defaultValue, const juce::String& tooltip = {});

    void bind (juce::ValueTree tree, const juce::Identifier& property, juce::UndoManager* undo);
    std::function<juce::String (double)> describe;

    void mouseDoubleClick (const juce::MouseEvent&) override;
    void startedDragging() override;
    void valueChanged() override;
    juce::String getTextFromValue (double value) override;

private:
    double defaultValue;
    PropertyBinding binding;
    bool updatingFromModel = false;
};

class Fader final : public juce::Slider
{
public:
    Fader();

    void bind (juce::ValueTree tree, const juce::Identifier& property, juce::UndoManager* undo);
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void startedDragging() override;
    void valueChanged() override;
    juce::String getTextFromValue (double value) override;
    void paint (juce::Graphics&) override;

private:
    PropertyBinding binding;
    bool updatingFromModel = false;
};

class NumberBox final : public juce::Component, public juce::SettableTooltipClient
{
public:
    NumberBox (double minimum, double maximum, double step, int decimals);

    void setValue (double v, juce::NotificationType notification = juce::sendNotification);
    double getValue() const noexcept { return value; }
    void setLabel (const juce::String& text) { label = text; repaint(); }

    std::function<void (double)> onChange;
    std::function<void()> onGestureStart;

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    double minimum, maximum, step, value = 0.0, dragStartValue = 0.0;
    int decimals;
    juce::String label;
    std::unique_ptr<juce::TextEditor> editor;
};

class LevelMeter final : public juce::Component
{
public:
    explicit LevelMeter (bool vertical = true);
    void push (float left, float right);
    void paint (juce::Graphics&) override;

private:
    bool vertical;
    float levelL = 0.0f, levelR = 0.0f, peakL = 0.0f, peakR = 0.0f;
    int peakHoldL = 0, peakHoldR = 0;
};

class Overlay final : public juce::Component
{
public:
    static Overlay* show (juce::Component& host, std::unique_ptr<juce::Component> content, const juce::String& title);
    static void closeAll (juce::Component& host);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void dismiss();

    std::function<void()> onDismiss;

private:
    Overlay (std::unique_ptr<juce::Component> content, const juce::String& title);

    std::unique_ptr<juce::Component> content;
    juce::String title;
    juce::Point<int> preferredSize;
    IconButton closeButton { "Close", icons::close() };
    juce::Rectangle<int> card;
};

class Toasts final : public juce::Component, private juce::Timer
{
public:
    Toasts();
    void show (const juce::String& message, bool isError = false);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;

    struct Item
    {
        juce::String text;
        bool error;
        juce::uint32 born;
    };
    std::vector<Item> items;
};

juce::String formatDecibels (float fader);
void keepFocusOnEditors (std::initializer_list<juce::Component*> components);
}
