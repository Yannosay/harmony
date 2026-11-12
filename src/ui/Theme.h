#pragma once

#include <juce_gui_extra/juce_gui_extra.h>

namespace harmony::ui
{
namespace colours
{
    inline const juce::Colour background   { 0xff0a0a0b };
    inline const juce::Colour panel        { 0xff111113 };
    inline const juce::Colour raised       { 0xff17171a };
    inline const juce::Colour control      { 0xff1e1e23 };
    inline const juce::Colour controlHover { 0xff26262c };
    inline const juce::Colour line         { 0xff26262c };
    inline const juce::Colour lineSoft     { 0xff1a1a1e };
    inline const juce::Colour text         { 0xfff5f5f6 };
    inline const juce::Colour textDim      { 0xffa1a1aa };
    inline const juce::Colour textFaint    { 0xff63636b };
    inline const juce::Colour accent       { 0xfff7b2ca };
    inline const juce::Colour accentDeep   { 0xffe98fb0 };
    inline const juce::Colour lilac        { 0xffc7b2fa };
    inline const juce::Colour danger       { 0xffff6b81 };
    inline const juce::Colour success      { 0xff8fe3b7 };
    inline const juce::Colour warning      { 0xfff5d49a };
    inline const juce::Colour gridBar      { 0xff2c2c33 };
    inline const juce::Colour gridBeat     { 0xff1f1f24 };
    inline const juce::Colour gridStep     { 0xff161619 };
    inline const juce::Colour playhead     { 0xfff5f5f6 };
}

enum class Weight { light, regular, medium, semibold, bold };

juce::Font font (float height, Weight weight = Weight::regular);
juce::Typeface::Ptr typeface (Weight weight);

namespace icons
{
    juce::Path play();
    juce::Path pause();
    juce::Path stop();
    juce::Path record();
    juce::Path metronome();
    juce::Path plus();
    juce::Path close();
    juce::Path search();
    juce::Path pencil();
    juce::Path select();
    juce::Path slice();
    juce::Path mute();
    juce::Path magnet();
    juce::Path wand();
    juce::Path drum();
    juce::Path piano();
    juce::Path mixer();
    juce::Path playlist();
    juce::Path rack();
    juce::Path folder();
    juce::Path plug();
    juce::Path wave();
    juce::Path chevronDown();
    juce::Path chevronRight();
    juce::Path loop();
    juce::Path ghost();
    juce::Path download();
    juce::Path history();
    juce::Path cpu();
    juce::Path scale();
    juce::Path gear();
    juce::Path automation();
    juce::Path maximise();
    juce::Path follow();
    juce::Path dragOut();
}

juce::Drawable* brandMark();

void drawIcon (juce::Graphics& g, const juce::Path& icon, juce::Rectangle<float> area, juce::Colour colour, bool filled = false);

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel();

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawTickBox (juce::Graphics&, juce::Component&, float x, float y, float w, float h, bool ticked, bool enabled, bool highlighted, bool down) override;

    void drawComboBox (juce::Graphics&, int width, int height, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getLabelFont (juce::Label&) override;

    void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
    void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area, bool separator, bool active, bool highlighted,
                            bool ticked, bool hasSubMenu, const juce::String& text, const juce::String& shortcut,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;
    void drawPopupMenuSectionHeader (juce::Graphics&, const juce::Rectangle<int>&, const juce::String&) override;
    juce::Font getPopupMenuFont() override;
    int getPopupMenuBorderSize() override { return 6; }
    void getIdealPopupMenuItemSize (const juce::String& text, bool separator, int standardHeight, int& w, int& h) override;

    void drawMenuBarBackground (juce::Graphics&, int width, int height, bool, juce::MenuBarComponent&) override;
    void drawMenuBarItem (juce::Graphics&, int width, int height, int index, const juce::String& text, bool over, bool open, bool, juce::MenuBarComponent&) override;
    juce::Font getMenuBarFont (juce::MenuBarComponent&, int, const juce::String&) override;
    int getMenuBarItemWidth (juce::MenuBarComponent&, int index, const juce::String& text) override;

    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical, int thumbStart, int thumbSize, bool over, bool down) override;
    int getDefaultScrollbarWidth() override { return 10; }

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos, float start, float end, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float min, float max, juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    void fillTextEditorBackground (juce::Graphics&, int width, int height, juce::TextEditor&) override;
    void drawTextEditorOutline (juce::Graphics&, int width, int height, juce::TextEditor&) override;

    void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& tipText, juce::Point<int> screenPos, juce::Rectangle<int> parentArea) override;

    void drawProgressBar (juce::Graphics&, juce::ProgressBar&, int width, int height, double progress, const juce::String& text) override;
    void drawStretchableLayoutResizerBar (juce::Graphics&, int w, int h, bool vertical, bool over, bool dragging) override;
    void drawTreeviewPlusMinusBox (juce::Graphics&, const juce::Rectangle<float>& area, juce::Colour, bool isOpen, bool over) override;
    void drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics&, const juce::Path&, juce::Image&) override;

    void drawDocumentWindowTitleBar (juce::DocumentWindow&, juce::Graphics&, int w, int h, int titleSpaceX, int titleSpaceW, const juce::Image* icon, bool drawTitleTextOnLeft) override;
    juce::Button* createDocumentWindowButton (int buttonType) override;
    void positionDocumentWindowButtons (juce::DocumentWindow&, int titleBarX, int titleBarY, int titleBarW, int titleBarH,
                                        juce::Button* minimise, juce::Button* maximise, juce::Button* close, bool positionTitleBarButtonsOnLeft) override;

    void drawAlertBox (juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>& textArea, juce::TextLayout&) override;
    juce::Font getAlertWindowTitleFont() override;
    juce::Font getAlertWindowMessageFont() override;
    juce::Font getAlertWindowFont() override;
};
}
