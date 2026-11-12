#include "ui/Theme.h"
#include "HarmonyAssets.h"

namespace harmony::ui
{
juce::Typeface::Ptr typeface (Weight weight)
{
    static const auto load = [] (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    };

    static const juce::Typeface::Ptr light    = load (HarmonyAssets::UnboundedLight_ttf,    HarmonyAssets::UnboundedLight_ttfSize);
    static const juce::Typeface::Ptr regular  = load (HarmonyAssets::UnboundedRegular_ttf,  HarmonyAssets::UnboundedRegular_ttfSize);
    static const juce::Typeface::Ptr medium   = load (HarmonyAssets::UnboundedMedium_ttf,   HarmonyAssets::UnboundedMedium_ttfSize);
    static const juce::Typeface::Ptr semibold = load (HarmonyAssets::UnboundedSemiBold_ttf, HarmonyAssets::UnboundedSemiBold_ttfSize);
    static const juce::Typeface::Ptr bold     = load (HarmonyAssets::UnboundedBold_ttf,     HarmonyAssets::UnboundedBold_ttfSize);

    switch (weight)
    {
        case Weight::light:    return light;
        case Weight::medium:   return medium;
        case Weight::semibold: return semibold;
        case Weight::bold:     return bold;
        case Weight::regular:  break;
    }
    return regular;
}

juce::Font font (float height, Weight weight)
{
    return juce::Font (juce::FontOptions (typeface (weight)).withHeight (height));
}

juce::Drawable* brandMark()
{
    static std::unique_ptr<juce::Drawable> mark = juce::Drawable::createFromImageData (HarmonyAssets::harmonymark_svg,
                                                                                     (size_t) HarmonyAssets::harmonymark_svgSize);
    return mark.get();
}

void drawIcon (juce::Graphics& g, const juce::Path& icon, juce::Rectangle<float> area, juce::Colour colour, bool filled)
{
    const float side = juce::jmin (area.getWidth(), area.getHeight());
    const auto box = area.withSizeKeepingCentre (side, side);
    const auto transform = juce::AffineTransform::scale (side / 24.0f).translated (box.getX(), box.getY());

    g.setColour (colour);
    if (filled)
        g.fillPath (icon, transform);
    else
        g.strokePath (icon, juce::PathStrokeType (juce::jmax (1.2f, side / 24.0f * 1.75f), juce::PathStrokeType::curved, juce::PathStrokeType::rounded), transform);
}

namespace icons
{
    juce::Path play()
    {
        juce::Path p;
        p.startNewSubPath (7.0f, 4.5f);
        p.lineTo (19.5f, 12.0f);
        p.lineTo (7.0f, 19.5f);
        p.closeSubPath();
        return p.createPathWithRoundedCorners (2.0f);
    }

    juce::Path pause()
    {
        juce::Path p;
        p.addRoundedRectangle (6.0f, 5.0f, 4.0f, 14.0f, 1.2f);
        p.addRoundedRectangle (14.0f, 5.0f, 4.0f, 14.0f, 1.2f);
        return p;
    }

    juce::Path stop()
    {
        juce::Path p;
        p.addRoundedRectangle (6.0f, 6.0f, 12.0f, 12.0f, 2.0f);
        return p;
    }

    juce::Path record()
    {
        juce::Path p;
        p.addEllipse (6.0f, 6.0f, 12.0f, 12.0f);
        return p;
    }

    juce::Path metronome()
    {
        juce::Path p;
        p.startNewSubPath (8.5f, 4.0f);
        p.lineTo (15.5f, 4.0f);
        p.lineTo (19.0f, 20.0f);
        p.lineTo (5.0f, 20.0f);
        p.closeSubPath();
        p.startNewSubPath (12.0f, 15.0f);
        p.lineTo (17.5f, 7.0f);
        p.startNewSubPath (7.0f, 15.5f);
        p.lineTo (17.0f, 15.5f);
        return p;
    }

    juce::Path plus()
    {
        juce::Path p;
        p.startNewSubPath (12.0f, 5.0f); p.lineTo (12.0f, 19.0f);
        p.startNewSubPath (5.0f, 12.0f); p.lineTo (19.0f, 12.0f);
        return p;
    }

    juce::Path close()
    {
        juce::Path p;
        p.startNewSubPath (6.5f, 6.5f); p.lineTo (17.5f, 17.5f);
        p.startNewSubPath (17.5f, 6.5f); p.lineTo (6.5f, 17.5f);
        return p;
    }

    juce::Path search()
    {
        juce::Path p;
        p.addEllipse (4.5f, 4.5f, 11.0f, 11.0f);
        p.startNewSubPath (14.0f, 14.0f); p.lineTo (19.5f, 19.5f);
        return p;
    }

    juce::Path pencil()
    {
        juce::Path p;
        p.startNewSubPath (4.5f, 19.5f);
        p.lineTo (5.5f, 15.0f);
        p.lineTo (15.5f, 5.0f);
        p.lineTo (19.0f, 8.5f);
        p.lineTo (9.0f, 18.5f);
        p.closeSubPath();
        p.startNewSubPath (13.0f, 7.5f); p.lineTo (16.5f, 11.0f);
        return p;
    }

    juce::Path select()
    {
        juce::Path p;
        p.startNewSubPath (6.0f, 4.0f);
        p.lineTo (18.0f, 13.0f);
        p.lineTo (12.5f, 14.0f);
        p.lineTo (15.5f, 20.0f);
        p.lineTo (13.0f, 21.0f);
        p.lineTo (10.2f, 15.2f);
        p.lineTo (6.0f, 18.5f);
        p.closeSubPath();
        return p;
    }

    juce::Path slice()
    {
        juce::Path p;
        p.startNewSubPath (12.0f, 3.5f); p.lineTo (12.0f, 20.5f);
        p.startNewSubPath (4.0f, 9.0f); p.lineTo (9.0f, 9.0f);
        p.startNewSubPath (15.0f, 15.0f); p.lineTo (20.0f, 15.0f);
        return p;
    }

    juce::Path mute()
    {
        juce::Path p;
        p.startNewSubPath (4.0f, 9.5f);
        p.lineTo (7.5f, 9.5f);
        p.lineTo (12.0f, 5.5f);
        p.lineTo (12.0f, 18.5f);
        p.lineTo (7.5f, 14.5f);
        p.lineTo (4.0f, 14.5f);
        p.closeSubPath();
        p.startNewSubPath (15.5f, 9.5f); p.lineTo (20.5f, 14.5f);
        p.startNewSubPath (20.5f, 9.5f); p.lineTo (15.5f, 14.5f);
        return p;
    }

    juce::Path magnet()
    {
        juce::Path p;
        p.startNewSubPath (6.0f, 4.5f);
        p.lineTo (6.0f, 12.0f);
        p.cubicTo (6.0f, 20.5f, 18.0f, 20.5f, 18.0f, 12.0f);
        p.lineTo (18.0f, 4.5f);
        p.startNewSubPath (6.0f, 8.5f); p.lineTo (9.5f, 8.5f);
        p.startNewSubPath (14.5f, 8.5f); p.lineTo (18.0f, 8.5f);
        return p;
    }

    juce::Path wand()
    {
        juce::Path p;
        p.startNewSubPath (4.5f, 19.5f); p.lineTo (14.5f, 9.5f);
        p.startNewSubPath (17.0f, 3.5f); p.lineTo (17.0f, 7.5f);
        p.startNewSubPath (15.0f, 5.5f); p.lineTo (19.0f, 5.5f);
        p.startNewSubPath (20.0f, 10.5f); p.lineTo (20.0f, 13.5f);
        p.startNewSubPath (18.5f, 12.0f); p.lineTo (21.5f, 12.0f);
        p.startNewSubPath (9.0f, 3.5f); p.lineTo (9.0f, 6.5f);
        p.startNewSubPath (7.5f, 5.0f); p.lineTo (10.5f, 5.0f);
        return p;
    }

    juce::Path drum()
    {
        juce::Path p;
        p.addEllipse (4.0f, 6.0f, 16.0f, 6.0f);
        p.startNewSubPath (4.0f, 9.0f); p.lineTo (4.0f, 16.0f);
        p.cubicTo (4.0f, 19.5f, 20.0f, 19.5f, 20.0f, 16.0f);
        p.lineTo (20.0f, 9.0f);
        p.startNewSubPath (7.0f, 3.0f); p.lineTo (11.0f, 8.0f);
        p.startNewSubPath (17.0f, 3.0f); p.lineTo (13.0f, 8.0f);
        return p;
    }

    juce::Path piano()
    {
        juce::Path p;
        p.addRoundedRectangle (3.5f, 5.0f, 17.0f, 14.0f, 2.0f);
        for (float x : { 7.75f, 12.0f, 16.25f })
        {
            p.startNewSubPath (x, 5.0f);
            p.lineTo (x, 19.0f);
        }
        for (float x : { 7.75f, 16.25f })
        {
            p.addRectangle (x - 1.3f, 5.0f, 2.6f, 7.5f);
        }
        return p;
    }

    juce::Path mixer()
    {
        juce::Path p;
        for (auto [x, y] : { std::pair { 6.0f, 9.0f }, std::pair { 12.0f, 15.0f }, std::pair { 18.0f, 7.0f } })
        {
            p.startNewSubPath (x, 4.0f);
            p.lineTo (x, 20.0f);
            p.addRoundedRectangle (x - 2.2f, y - 1.5f, 4.4f, 3.0f, 1.0f);
        }
        return p;
    }

    juce::Path playlist()
    {
        juce::Path p;
        p.addRoundedRectangle (3.5f, 5.0f, 9.0f, 4.0f, 1.0f);
        p.addRoundedRectangle (9.5f, 10.0f, 11.0f, 4.0f, 1.0f);
        p.addRoundedRectangle (5.5f, 15.0f, 7.0f, 4.0f, 1.0f);
        return p;
    }

    juce::Path rack()
    {
        juce::Path p;
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 3; ++c)
                p.addRoundedRectangle (4.0f + (float) c * 6.0f, 4.0f + (float) r * 6.0f, 4.0f, 4.0f, 1.0f);
        return p;
    }

    juce::Path folder()
    {
        juce::Path p;
        p.startNewSubPath (3.5f, 7.0f);
        p.lineTo (3.5f, 18.0f);
        p.lineTo (20.5f, 18.0f);
        p.lineTo (20.5f, 8.5f);
        p.lineTo (11.5f, 8.5f);
        p.lineTo (9.5f, 6.0f);
        p.lineTo (3.5f, 6.0f);
        p.closeSubPath();
        return p;
    }

    juce::Path plug()
    {
        juce::Path p;
        p.startNewSubPath (9.0f, 3.5f); p.lineTo (9.0f, 8.0f);
        p.startNewSubPath (15.0f, 3.5f); p.lineTo (15.0f, 8.0f);
        p.startNewSubPath (6.0f, 8.0f);
        p.lineTo (18.0f, 8.0f);
        p.lineTo (18.0f, 12.0f);
        p.cubicTo (18.0f, 15.5f, 15.5f, 17.0f, 12.0f, 17.0f);
        p.cubicTo (8.5f, 17.0f, 6.0f, 15.5f, 6.0f, 12.0f);
        p.closeSubPath();
        p.startNewSubPath (12.0f, 17.0f); p.lineTo (12.0f, 21.0f);
        return p;
    }

    juce::Path wave()
    {
        juce::Path p;
        const float xs[] = { 3.0f, 6.0f, 9.0f, 12.0f, 15.0f, 18.0f, 21.0f };
        const float hs[] = { 2.0f, 7.0f, 12.0f, 6.0f, 10.0f, 4.0f, 2.0f };
        for (int i = 0; i < 7; ++i)
        {
            p.startNewSubPath (xs[i], 12.0f - hs[i] * 0.6f);
            p.lineTo (xs[i], 12.0f + hs[i] * 0.6f);
        }
        return p;
    }

    juce::Path chevronDown()
    {
        juce::Path p;
        p.startNewSubPath (7.0f, 10.0f); p.lineTo (12.0f, 15.0f); p.lineTo (17.0f, 10.0f);
        return p;
    }

    juce::Path chevronRight()
    {
        juce::Path p;
        p.startNewSubPath (10.0f, 7.0f); p.lineTo (15.0f, 12.0f); p.lineTo (10.0f, 17.0f);
        return p;
    }

    juce::Path loop()
    {
        juce::Path p;
        p.startNewSubPath (5.0f, 13.0f);
        p.lineTo (5.0f, 10.0f);
        p.cubicTo (5.0f, 8.0f, 6.5f, 7.0f, 8.5f, 7.0f);
        p.lineTo (18.5f, 7.0f);
        p.startNewSubPath (16.0f, 4.5f); p.lineTo (18.5f, 7.0f); p.lineTo (16.0f, 9.5f);
        p.startNewSubPath (19.0f, 11.0f);
        p.lineTo (19.0f, 14.0f);
        p.cubicTo (19.0f, 16.0f, 17.5f, 17.0f, 15.5f, 17.0f);
        p.lineTo (5.5f, 17.0f);
        p.startNewSubPath (8.0f, 14.5f); p.lineTo (5.5f, 17.0f); p.lineTo (8.0f, 19.5f);
        return p;
    }

    juce::Path ghost()
    {
        juce::Path p;
        p.startNewSubPath (5.0f, 20.0f);
        p.lineTo (5.0f, 11.0f);
        p.cubicTo (5.0f, 2.5f, 19.0f, 2.5f, 19.0f, 11.0f);
        p.lineTo (19.0f, 20.0f);
        p.lineTo (16.5f, 18.0f);
        p.lineTo (14.0f, 20.0f);
        p.lineTo (12.0f, 18.0f);
        p.lineTo (10.0f, 20.0f);
        p.lineTo (7.5f, 18.0f);
        p.closeSubPath();
        p.addEllipse (8.5f, 9.0f, 2.0f, 2.5f);
        p.addEllipse (13.5f, 9.0f, 2.0f, 2.5f);
        return p;
    }

    juce::Path download()
    {
        juce::Path p;
        p.startNewSubPath (12.0f, 4.0f); p.lineTo (12.0f, 15.0f);
        p.startNewSubPath (7.5f, 10.5f); p.lineTo (12.0f, 15.0f); p.lineTo (16.5f, 10.5f);
        p.startNewSubPath (5.0f, 19.5f); p.lineTo (19.0f, 19.5f);
        return p;
    }

    juce::Path history()
    {
        juce::Path p;
        p.addCentredArc (12.0f, 12.0f, 8.0f, 8.0f, 0.0f, -2.2f, 3.1416f, true);
        p.startNewSubPath (2.8f, 4.8f); p.lineTo (4.6f, 7.8f); p.lineTo (7.6f, 6.6f);
        p.startNewSubPath (12.0f, 8.0f); p.lineTo (12.0f, 12.5f); p.lineTo (15.0f, 14.5f);
        return p;
    }

    juce::Path cpu()
    {
        juce::Path p;
        p.addRoundedRectangle (6.0f, 6.0f, 12.0f, 12.0f, 2.0f);
        for (float v : { 9.0f, 12.0f, 15.0f })
        {
            p.startNewSubPath (v, 3.0f);  p.lineTo (v, 6.0f);
            p.startNewSubPath (v, 18.0f); p.lineTo (v, 21.0f);
            p.startNewSubPath (3.0f, v);  p.lineTo (6.0f, v);
            p.startNewSubPath (18.0f, v); p.lineTo (21.0f, v);
        }
        return p;
    }

    juce::Path scale()
    {
        juce::Path p;
        for (int i = 0; i < 4; ++i)
        {
            const float y = 18.0f - (float) i * 4.0f;
            p.startNewSubPath (4.0f + (float) i * 3.0f, y);
            p.lineTo (11.0f + (float) i * 3.0f, y);
        }
        return p;
    }

    juce::Path gear()
    {
        juce::Path p;
        p.addEllipse (8.5f, 8.5f, 7.0f, 7.0f);
        for (int i = 0; i < 8; ++i)
        {
            const float a = (float) i * juce::MathConstants<float>::twoPi / 8.0f;
            p.startNewSubPath (12.0f + std::cos (a) * 6.2f, 12.0f + std::sin (a) * 6.2f);
            p.lineTo (12.0f + std::cos (a) * 9.0f, 12.0f + std::sin (a) * 9.0f);
        }
        return p;
    }

    juce::Path automation()
    {
        juce::Path p;
        p.startNewSubPath (3.5f, 17.0f);
        p.lineTo (8.5f, 17.0f);
        p.quadraticTo (12.0f, 6.0f, 15.5f, 7.0f);
        p.lineTo (20.5f, 7.0f);
        p.addEllipse (7.0f, 15.5f, 3.0f, 3.0f);
        p.addEllipse (14.0f, 5.5f, 3.0f, 3.0f);
        return p;
    }
}

LookAndFeel::LookAndFeel()
{
    using namespace colours;
    setColourScheme ({ panel, background, raised, line, text, accent, background, accent, text });

    setColour (juce::ResizableWindow::backgroundColourId, background);
    setColour (juce::DocumentWindow::textColourId, text);
    setColour (juce::TextButton::buttonColourId, control);
    setColour (juce::TextButton::buttonOnColourId, accent);
    setColour (juce::TextButton::textColourOffId, text);
    setColour (juce::TextButton::textColourOnId, background);
    setColour (juce::ComboBox::backgroundColourId, control);
    setColour (juce::ComboBox::textColourId, text);
    setColour (juce::ComboBox::outlineColourId, line);
    setColour (juce::ComboBox::arrowColourId, textDim);
    setColour (juce::PopupMenu::backgroundColourId, raised);
    setColour (juce::PopupMenu::textColourId, text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, controlHover);
    setColour (juce::PopupMenu::highlightedTextColourId, text);
    setColour (juce::PopupMenu::headerTextColourId, textFaint);
    setColour (juce::Label::textColourId, text);
    setColour (juce::TextEditor::backgroundColourId, control);
    setColour (juce::TextEditor::textColourId, text);
    setColour (juce::TextEditor::highlightColourId, accent.withAlpha (0.35f));
    setColour (juce::TextEditor::highlightedTextColourId, text);
    setColour (juce::TextEditor::outlineColourId, line);
    setColour (juce::TextEditor::focusedOutlineColourId, accent.withAlpha (0.7f));
    setColour (juce::CaretComponent::caretColourId, accent);
    setColour (juce::ScrollBar::thumbColourId, textFaint);
    setColour (juce::Slider::thumbColourId, accent);
    setColour (juce::Slider::trackColourId, accent);
    setColour (juce::Slider::backgroundColourId, control);
    setColour (juce::Slider::rotarySliderFillColourId, accent);
    setColour (juce::Slider::rotarySliderOutlineColourId, control);
    setColour (juce::Slider::textBoxTextColourId, text);
    setColour (juce::Slider::textBoxBackgroundColourId, control);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::ToggleButton::textColourId, text);
    setColour (juce::ToggleButton::tickColourId, background);
    setColour (juce::ToggleButton::tickDisabledColourId, textFaint);
    setColour (juce::TreeView::backgroundColourId, panel);
    setColour (juce::TreeView::linesColourId, line);
    setColour (juce::TreeView::selectedItemBackgroundColourId, controlHover);
    setColour (juce::ListBox::backgroundColourId, panel);
    setColour (juce::ListBox::textColourId, text);
    setColour (juce::ListBox::outlineColourId, line);
    setColour (juce::AlertWindow::backgroundColourId, raised);
    setColour (juce::AlertWindow::textColourId, text);
    setColour (juce::AlertWindow::outlineColourId, line);
    setColour (juce::TooltipWindow::backgroundColourId, raised);
    setColour (juce::TooltipWindow::textColourId, text);
    setColour (juce::TooltipWindow::outlineColourId, line);
    setColour (juce::ProgressBar::backgroundColourId, control);
    setColour (juce::ProgressBar::foregroundColourId, accent);
    setColour (juce::GroupComponent::outlineColourId, line);
    setColour (juce::GroupComponent::textColourId, textDim);
    setColour (juce::HyperlinkButton::textColourId, accent);
    setColour (juce::DirectoryContentsDisplayComponent::highlightColourId, controlHover);
    setColour (juce::DirectoryContentsDisplayComponent::textColourId, text);
    setColour (juce::FileChooserDialogBox::titleTextColourId, text);

    setDefaultSansSerifTypeface (typeface (Weight::regular));
}

juce::Typeface::Ptr LookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName() || f.getTypefaceName() == "<Sans-Serif>")
        return typeface (f.isBold() ? Weight::semibold : Weight::regular);

    return juce::LookAndFeel_V4::getTypefaceForFont (f);
}

void LookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& base, bool highlighted, bool down)
{
    auto r = button.getLocalBounds().toFloat().reduced (0.5f);
    const bool on = button.getToggleState();
    auto fill = on ? button.findColour (juce::TextButton::buttonOnColourId) : base;

    if (! button.isEnabled())
        fill = fill.withMultipliedAlpha (0.4f);
    else if (down)
        fill = fill.brighter (0.06f);
    else if (highlighted)
        fill = on ? fill.brighter (0.08f) : colours::controlHover;

    g.setColour (fill);
    g.fillRoundedRectangle (r, 6.0f);

    if (! on)
    {
        g.setColour (colours::line.withAlpha (highlighted ? 1.0f : 0.7f));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }
}

juce::Font LookAndFeel::getTextButtonFont (juce::TextButton&, int buttonHeight)
{
    return font (juce::jlimit (10.0f, 13.0f, (float) buttonHeight * 0.4f), Weight::medium);
}

void LookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setFont (getTextButtonFont (button, button.getHeight()));
    g.setColour (button.findColour (button.getToggleState() ? juce::TextButton::textColourOnId : juce::TextButton::textColourOffId)
                     .withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.45f));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().reduced (8, 2), juce::Justification::centred, 1);
}

void LookAndFeel::drawTickBox (juce::Graphics& g, juce::Component&, float x, float y, float w, float h,
                               bool ticked, bool enabled, bool highlighted, bool)
{
    juce::Rectangle<float> box (x, y, w, h);
    box = box.withSizeKeepingCentre (14.0f, 14.0f);

    g.setColour (ticked ? colours::accent : (highlighted ? colours::controlHover : colours::control));
    g.fillRoundedRectangle (box, 4.0f);
    g.setColour (ticked ? colours::accent : colours::line);
    g.drawRoundedRectangle (box, 4.0f, 1.0f);

    if (ticked)
    {
        juce::Path tick;
        tick.startNewSubPath (box.getX() + 3.5f, box.getCentreY());
        tick.lineTo (box.getX() + 6.0f, box.getBottom() - 4.0f);
        tick.lineTo (box.getRight() - 3.5f, box.getY() + 4.0f);
        g.setColour (colours::background.withMultipliedAlpha (enabled ? 1.0f : 0.5f));
        g.strokePath (tick, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
}

void LookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down)
{
    drawTickBox (g, button, 2.0f, 0.0f, 16.0f, (float) button.getHeight(), button.getToggleState(), button.isEnabled(), highlighted, down);
    g.setFont (font (11.5f));
    g.setColour (colours::text.withMultipliedAlpha (button.isEnabled() ? 1.0f : 0.5f));
    g.drawFittedText (button.getButtonText(), button.getLocalBounds().withTrimmedLeft (26), juce::Justification::centredLeft, 1);
}

void LookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box)
{
    juce::Rectangle<float> r (0.0f, 0.0f, (float) width, (float) height);
    r = r.reduced (0.5f);
    g.setColour (box.isMouseOver (true) ? colours::controlHover : colours::control);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (box.hasKeyboardFocus (true) ? colours::accent.withAlpha (0.6f) : colours::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);
    drawIcon (g, icons::chevronDown(), juce::Rectangle<float> ((float) width - 20.0f, 0.0f, 14.0f, (float) height), colours::textDim);
}

juce::Font LookAndFeel::getComboBoxFont (juce::ComboBox& box)
{
    return font (juce::jlimit (10.0f, 12.0f, (float) box.getHeight() * 0.42f));
}

void LookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (6, 1, box.getWidth() - 26, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font LookAndFeel::getLabelFont (juce::Label& label)
{
    return font (juce::jmin (12.0f, label.getFont().getHeight()));
}

void LookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
{
    g.fillAll (colours::raised);
    g.setColour (colours::line);
    g.drawRect (0, 0, width, height, 1);
}

juce::Font LookAndFeel::getPopupMenuFont()
{
    return font (12.0f);
}

void LookAndFeel::getIdealPopupMenuItemSize (const juce::String& text, bool separator, int, int& w, int& h)
{
    if (separator)
    {
        w = 50;
        h = 9;
        return;
    }

    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (getPopupMenuFont(), text, 0.0f, 0.0f);
    w = (int) glyphs.getBoundingBox (0, -1, true).getWidth() + 64;
    h = 28;
}

void LookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area, bool separator, bool active,
                                     bool highlighted, bool ticked, bool hasSubMenu, const juce::String& text,
                                     const juce::String& shortcut, const juce::Drawable* icon, const juce::Colour* textColour)
{
    if (separator)
    {
        g.setColour (colours::line);
        g.fillRect (area.reduced (10, 0).withSizeKeepingCentre (area.getWidth() - 20, 1));
        return;
    }

    auto r = area.reduced (4, 1);
    if (highlighted && active)
    {
        g.setColour (colours::controlHover);
        g.fillRoundedRectangle (r.toFloat(), 5.0f);
    }

    const auto colour = (textColour != nullptr ? *textColour : colours::text).withMultipliedAlpha (active ? 1.0f : 0.4f);
    auto content = r.reduced (10, 0);
    auto iconArea = content.removeFromLeft (18);

    if (ticked)
    {
        g.setColour (colours::accent);
        g.fillEllipse (iconArea.toFloat().withSizeKeepingCentre (6.0f, 6.0f));
    }
    else if (icon != nullptr)
    {
        icon->drawWithin (g, iconArea.toFloat().reduced (1.0f), juce::RectanglePlacement::centred, 1.0f);
    }

    content.removeFromLeft (4);
    g.setColour (colour);
    g.setFont (getPopupMenuFont());

    if (hasSubMenu)
        drawIcon (g, icons::chevronRight(), content.removeFromRight (14).toFloat(), colours::textDim);

    if (shortcut.isNotEmpty())
    {
        g.setFont (font (10.5f, Weight::light));
        g.setColour (colours::textFaint);
        g.drawText (shortcut, content, juce::Justification::centredRight, false);
        g.setFont (getPopupMenuFont());
        g.setColour (colour);
    }

    g.drawFittedText (text, content, juce::Justification::centredLeft, 1);
}

void LookAndFeel::drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area, const juce::String& name)
{
    g.setFont (font (10.0f, Weight::medium));
    g.setColour (colours::textFaint);
    g.drawText (name.toUpperCase(), area.reduced (14, 0), juce::Justification::bottomLeft, true);
}

void LookAndFeel::drawMenuBarBackground (juce::Graphics& g, int, int, bool, juce::MenuBarComponent&)
{
    g.fillAll (juce::Colours::transparentBlack);
}

juce::Font LookAndFeel::getMenuBarFont (juce::MenuBarComponent&, int, const juce::String&)
{
    return font (11.5f);
}

int LookAndFeel::getMenuBarItemWidth (juce::MenuBarComponent& bar, int index, const juce::String& text)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (getMenuBarFont (bar, index, text), text, 0.0f, 0.0f);
    return (int) glyphs.getBoundingBox (0, -1, true).getWidth() + 22;
}

void LookAndFeel::drawMenuBarItem (juce::Graphics& g, int width, int height, int index, const juce::String& text,
                                   bool over, bool open, bool, juce::MenuBarComponent& bar)
{
    if (over || open)
    {
        g.setColour (colours::control);
        g.fillRoundedRectangle (juce::Rectangle<float> (2.0f, 4.0f, (float) width - 4.0f, (float) height - 8.0f), 5.0f);
    }

    g.setColour (open || over ? colours::text : colours::textDim);
    g.setFont (getMenuBarFont (bar, index, text));
    g.drawFittedText (text, 0, 0, width, height, juce::Justification::centred, 1);
}

void LookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                 int thumbStart, int thumbSize, bool over, bool down)
{
    juce::Rectangle<int> thumb = vertical ? juce::Rectangle<int> (x, thumbStart, w, thumbSize)
                                          : juce::Rectangle<int> (thumbStart, y, thumbSize, h);
    const auto t = thumb.toFloat().reduced (vertical ? 2.5f : 1.0f, vertical ? 1.0f : 2.5f);
    g.setColour (colours::textFaint.withAlpha (down ? 0.9f : (over ? 0.7f : 0.4f)));
    g.fillRoundedRectangle (t, juce::jmin (t.getWidth(), t.getHeight()) * 0.5f);
}

void LookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float start, float end, juce::Slider& slider)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (2.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = start + pos * (end - start);
    const float track = juce::jmax (1.5f, radius * 0.16f);
    const bool bipolar = slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0;
    const float origin = bipolar ? (start + end) * 0.5f : start;

    juce::Path background;
    background.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, start, end, true);
    g.setColour (colours::control.brighter (0.15f));
    g.strokePath (background, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (std::abs (angle - origin) > 0.01f)
    {
        juce::Path value;
        value.addCentredArc (centre.x, centre.y, radius - track, radius - track, 0.0f, juce::jmin (origin, angle), juce::jmax (origin, angle), true);
        g.setColour (slider.findColour (juce::Slider::rotarySliderFillColourId).withMultipliedAlpha (slider.isEnabled() ? 1.0f : 0.4f));
        g.strokePath (value, juce::PathStrokeType (track, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    const float knob = radius - track * 2.2f;
    g.setColour (slider.isMouseOverOrDragging() ? colours::controlHover.brighter (0.1f) : colours::controlHover);
    g.fillEllipse (centre.x - knob, centre.y - knob, knob * 2.0f, knob * 2.0f);

    const auto tip = centre.getPointOnCircumference (knob * 0.75f, angle);
    g.setColour (colours::text);
    g.drawLine ({ centre.getPointOnCircumference (knob * 0.2f, angle), tip }, juce::jmax (1.2f, radius * 0.12f));
}

void LookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                    juce::Slider::SliderStyle style, juce::Slider& slider)
{
    const bool vertical = style == juce::Slider::LinearVertical || style == juce::Slider::LinearBarVertical;
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat();
    const auto fill = slider.findColour (juce::Slider::trackColourId).withMultipliedAlpha (slider.isEnabled() ? 1.0f : 0.4f);

    if (style == juce::Slider::LinearBar || style == juce::Slider::LinearBarVertical)
    {
        g.setColour (colours::control);
        g.fillRoundedRectangle (bounds, 4.0f);
        auto filled = vertical ? bounds.withTop (pos) : bounds.withRight (pos);
        g.setColour (fill.withAlpha (0.55f));
        g.fillRoundedRectangle (filled, 4.0f);
        return;
    }

    const float thickness = 4.0f;
    auto trackRect = vertical ? bounds.withSizeKeepingCentre (thickness, bounds.getHeight())
                              : bounds.withSizeKeepingCentre (bounds.getWidth(), thickness);
    g.setColour (colours::control.brighter (0.12f));
    g.fillRoundedRectangle (trackRect, thickness * 0.5f);

    auto filled = vertical ? trackRect.withTop (pos) : trackRect.withRight (pos);
    g.setColour (fill);
    g.fillRoundedRectangle (filled, thickness * 0.5f);

    const auto thumb = vertical ? juce::Rectangle<float> (bounds.getCentreX() - 9.0f, pos - 4.0f, 18.0f, 8.0f)
                                : juce::Rectangle<float> (pos - 4.0f, bounds.getCentreY() - 9.0f, 8.0f, 18.0f);
    g.setColour (colours::text);
    g.fillRoundedRectangle (thumb, 2.5f);
}

juce::Label* LookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (font (10.5f));
    label->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return label;
}

void LookAndFeel::fillTextEditorBackground (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.findColour (juce::TextEditor::backgroundColourId));
    g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height), 6.0f);
}

void LookAndFeel::drawTextEditorOutline (juce::Graphics& g, int width, int height, juce::TextEditor& editor)
{
    g.setColour (editor.hasKeyboardFocus (true) ? colours::accent.withAlpha (0.6f) : colours::line);
    g.drawRoundedRectangle (juce::Rectangle<float> (0.5f, 0.5f, (float) width - 1.0f, (float) height - 1.0f), 6.0f, 1.0f);
}

void LookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
{
    const juce::Rectangle<float> r (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (colours::raised);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (colours::text);
    g.setFont (font (11.0f));
    g.drawFittedText (text, juce::Rectangle<int> (width, height).reduced (10, 4), juce::Justification::centredLeft, 4);
}

juce::Rectangle<int> LookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::GlyphArrangement glyphs;
    glyphs.addLineOfText (font (11.0f), text, 0.0f, 0.0f);
    const int w = juce::jmin (420, (int) glyphs.getBoundingBox (0, -1, true).getWidth() + 24);
    const int h = 28;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 18,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 8) : pos.y + 18, w, h)
        .constrainedWithin (parent);
}

void LookAndFeel::drawProgressBar (juce::Graphics& g, juce::ProgressBar&, int width, int height, double progress, const juce::String& text)
{
    const juce::Rectangle<float> r (0.0f, 0.0f, (float) width, (float) height);
    g.setColour (colours::control);
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);

    if (progress >= 0.0 && progress <= 1.0)
    {
        juce::ColourGradient grad (colours::accent, 0.0f, 0.0f, colours::lilac, (float) width, 0.0f, false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r.withWidth (juce::jmax (r.getHeight(), r.getWidth() * (float) progress)), r.getHeight() * 0.5f);
    }

    if (text.isNotEmpty())
    {
        g.setColour (colours::text);
        g.setFont (font (10.5f, Weight::medium));
        g.drawText (text, r, juce::Justification::centred, false);
    }
}

void LookAndFeel::drawStretchableLayoutResizerBar (juce::Graphics& g, int w, int h, bool vertical, bool over, bool dragging)
{
    g.setColour (dragging ? colours::accent.withAlpha (0.6f) : (over ? colours::line.brighter (0.2f) : colours::lineSoft));
    if (vertical)
        g.fillRect (w / 2, 0, 1, h);
    else
        g.fillRect (0, h / 2, w, 1);
}

void LookAndFeel::drawTreeviewPlusMinusBox (juce::Graphics& g, const juce::Rectangle<float>& area, juce::Colour, bool isOpen, bool over)
{
    drawIcon (g, isOpen ? icons::chevronDown() : icons::chevronRight(), area.reduced (1.0f), over ? colours::text : colours::textDim);
}

void LookAndFeel::drawCallOutBoxBackground (juce::CallOutBox&, juce::Graphics& g, const juce::Path& path, juce::Image&)
{
    g.setColour (colours::raised);
    g.fillPath (path);
    g.setColour (colours::line);
    g.strokePath (path, juce::PathStrokeType (1.0f));
}

void LookAndFeel::drawDocumentWindowTitleBar (juce::DocumentWindow& window, juce::Graphics& g, int w, int h,
                                              int titleSpaceX, int titleSpaceW, const juce::Image*, bool)
{
    g.setColour (colours::panel);
    g.fillRect (0, 0, w, h);
    g.setColour (colours::line);
    g.fillRect (0, h - 1, w, 1);

    if (auto* mark = brandMark())
        mark->drawWithin (g, juce::Rectangle<float> (10.0f, 0.0f, 16.0f, (float) h).reduced (0.0f, 6.0f), juce::RectanglePlacement::centred, 1.0f);

    g.setColour (window.isActiveWindow() ? colours::text : colours::textDim);
    g.setFont (font (11.5f, Weight::medium));
    g.drawText (window.getName(), juce::jmax (32, titleSpaceX), 0, titleSpaceW, h, juce::Justification::centredLeft, true);
}

namespace
{
    class WindowButton final : public juce::Button
    {
    public:
        WindowButton (const juce::String& name, juce::Path icon, bool isClose)
            : juce::Button (name), shape (std::move (icon)), closeStyle (isClose) {}

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            if (over || down)
            {
                g.setColour (closeStyle ? colours::danger.withAlpha (0.85f) : colours::controlHover);
                g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (3.0f), 5.0f);
            }
            drawIcon (g, shape, getLocalBounds().toFloat().reduced (8.0f), over && closeStyle ? colours::text : colours::textDim);
        }

    private:
        juce::Path shape;
        bool closeStyle;
    };
}

juce::Button* LookAndFeel::createDocumentWindowButton (int type)
{
    if (type == juce::DocumentWindow::closeButton)
        return new WindowButton ("close", icons::close(), true);

    if (type == juce::DocumentWindow::minimiseButton)
    {
        juce::Path p;
        p.startNewSubPath (6.0f, 12.0f);
        p.lineTo (18.0f, 12.0f);
        return new WindowButton ("minimise", p, false);
    }

    juce::Path p;
    p.addRoundedRectangle (6.0f, 6.0f, 12.0f, 12.0f, 1.5f);
    return new WindowButton ("maximise", p, false);
}

void LookAndFeel::positionDocumentWindowButtons (juce::DocumentWindow&, int x, int y, int w, int h,
                                                 juce::Button* minimise, juce::Button* maximise, juce::Button* close, bool)
{
    int right = x + w - 4;
    for (auto* b : { close, maximise, minimise })
    {
        if (b == nullptr)
            continue;

        right -= h;
        b->setBounds (right, y, h, h);
    }
}

void LookAndFeel::drawAlertBox (juce::Graphics& g, juce::AlertWindow& alert, const juce::Rectangle<int>& textArea, juce::TextLayout& layout)
{
    const auto r = alert.getLocalBounds().toFloat();
    g.setColour (colours::raised);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    layout.draw (g, textArea.toFloat().withTrimmedLeft (4.0f));
}

juce::Font LookAndFeel::getAlertWindowTitleFont()   { return font (15.0f, Weight::semibold); }
juce::Font LookAndFeel::getAlertWindowMessageFont() { return font (12.0f); }
juce::Font LookAndFeel::getAlertWindowFont()        { return font (12.0f); }
}
