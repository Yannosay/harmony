#include "ui/PatternBar.h"
#include "ui/Dialogs.h"

namespace harmony::ui
{
namespace
{
    constexpr float chipHeight = 24.0f;
    constexpr float chipGap = 6.0f;
    constexpr float addWidth = 28.0f;
}

PatternBar::PatternBar (Context& c)
    : ctx (c)
{
    setTooltip ("Patterns: click to edit, drag into the playlist, double-click for the piano roll, right-click for options");
    ctx.project.getState().addListener (this);
    layoutChips();
}

PatternBar::~PatternBar()
{
    ctx.project.getState().removeListener (this);
}

void PatternBar::layoutChips()
{
    chips.clear();
    const auto chipFont = font (10.0f, Weight::medium);
    float x = 0.0f;

    for (auto p : ctx.project.patternsTree())
    {
        const float w = juce::jlimit (64.0f, 170.0f, juce::GlyphArrangement::getStringWidth (chipFont, p[id::name].toString()) + 34.0f);
        chips.push_back ({ (int) p[id::uid], { x, 0.0f, w, chipHeight } });
        x += w + chipGap;
    }

    contentWidth = x + addWidth;
    scroll = juce::jlimit (0.0f, juce::jmax (0.0f, contentWidth - (float) getWidth()), scroll);
    repaint();
}

void PatternBar::scrollToCurrent()
{
    const int current = ctx.project.currentPattern()[id::uid];
    for (auto& chip : chips)
    {
        if (chip.uid != current)
            continue;

        if (chip.bounds.getX() < scroll)
            scroll = chip.bounds.getX();
        else if (chip.bounds.getRight() > scroll + (float) getWidth() - addWidth)
            scroll = chip.bounds.getRight() - (float) getWidth() + addWidth + chipGap;
    }
    scroll = juce::jlimit (0.0f, juce::jmax (0.0f, contentWidth - (float) getWidth()), scroll);
}

void PatternBar::resized()
{
    layoutChips();
    scrollToCurrent();
}

void PatternBar::paint (juce::Graphics& g)
{
    const float y = ((float) getHeight() - chipHeight) * 0.5f;
    const int current = ctx.project.currentPattern()[id::uid];

    {
        juce::Graphics::ScopedSaveState state (g);
        g.reduceClipRegion (getLocalBounds().withTrimmedRight ((int) addWidth + 4));

        for (size_t i = 0; i < chips.size(); ++i)
        {
            const auto& chip = chips[i];
            const auto pattern = ctx.project.findPattern (chip.uid);
            const auto colour = juce::Colour::fromString (pattern[id::colour].toString());
            const auto r = chip.bounds.translated (-scroll, y);
            const bool isCurrent = chip.uid == current;
            const bool isHover = (int) i == hovered;

            g.setColour (isCurrent ? colour.withAlpha (0.24f) : (isHover ? colours::controlHover : colours::control));
            g.fillRoundedRectangle (r, 6.0f);
            g.setColour (isCurrent ? colour : colours::line);
            g.drawRoundedRectangle (r.reduced (0.5f), 6.0f, 1.0f);

            g.setColour (colour);
            g.fillEllipse (r.getX() + 9.0f, r.getCentreY() - 3.5f, 7.0f, 7.0f);

            g.setColour (isCurrent ? colours::text : colours::textDim);
            g.setFont (font (10.0f, isCurrent ? Weight::semibold : Weight::medium));
            g.drawText (pattern[id::name].toString(), r.withTrimmedLeft (22.0f).withTrimmedRight (8.0f), juce::Justification::centredLeft, true);
        }
    }

    addButton = { (float) getWidth() - addWidth, y, addWidth, chipHeight };
    g.setColour (hoverAdd ? colours::controlHover : colours::control);
    g.fillRoundedRectangle (addButton, 6.0f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (addButton.reduced (0.5f), 6.0f, 1.0f);
    drawIcon (g, icons::plus(), addButton.reduced (6.0f), hoverAdd ? colours::text : colours::textDim);

    if (scroll > 0.5f)
    {
        juce::ColourGradient fade (colours::panel, 0.0f, 0.0f, colours::panel.withAlpha (0.0f), 18.0f, 0.0f, false);
        g.setGradientFill (fade);
        g.fillRect (0, 0, 18, getHeight());
    }
}

int PatternBar::chipAt (juce::Point<float> p) const
{
    if (p.x >= (float) getWidth() - addWidth - 4.0f)
        return -1;

    const float y = ((float) getHeight() - chipHeight) * 0.5f;
    for (size_t i = 0; i < chips.size(); ++i)
        if (chips[i].bounds.translated (-scroll, y).contains (p))
            return (int) i;
    return -1;
}

void PatternBar::mouseMove (const juce::MouseEvent& e)
{
    const int hit = chipAt (e.position);
    const bool overAdd = addButton.contains (e.position);
    if (hit != hovered || overAdd != hoverAdd)
    {
        hovered = hit;
        hoverAdd = overAdd;
        setMouseCursor (hit >= 0 || overAdd ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
        repaint();
    }
}

void PatternBar::mouseExit (const juce::MouseEvent&)
{
    hovered = -1;
    hoverAdd = false;
    repaint();
}

void PatternBar::mouseDown (const juce::MouseEvent& e)
{
    dragStarted = false;
    pressed = chipAt (e.position);

    if (addButton.contains (e.position))
    {
        ctx.project.beginTransaction ("New pattern");
        ctx.project.setCurrentPattern (ctx.project.addPattern()[id::uid]);
        return;
    }

    if (pressed < 0)
        return;

    const int uid = chips[(size_t) pressed].uid;
    if (e.mods.isPopupMenu())
    {
        showMenu (uid);
        pressed = -1;
        return;
    }

    ctx.project.setCurrentPattern (uid);
}

void PatternBar::mouseDrag (const juce::MouseEvent& e)
{
    if (pressed < 0 || dragStarted || e.getDistanceFromDragStart() < 6)
        return;

    if (auto* container = juce::DragAndDropContainer::findParentDragContainerFor (this))
    {
        dragStarted = true;
        const auto& chip = chips[(size_t) pressed];
        const auto image = createComponentSnapshot (chip.bounds.translated (-scroll, ((float) getHeight() - chipHeight) * 0.5f).toNearestInt());
        container->startDragging ("pattern:" + juce::String (chip.uid), this, juce::ScaledImage (image));
    }
}

void PatternBar::mouseUp (const juce::MouseEvent&)
{
    pressed = -1;
    dragStarted = false;
}

void PatternBar::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = chipAt (e.position);
    if (hit < 0)
        return;

    ctx.project.setCurrentPattern (chips[(size_t) hit].uid);
    if (ctx.openPianoRoll)
        ctx.openPianoRoll (ctx.project.selectedChannel());
}

void PatternBar::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    const float delta = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
    scroll = juce::jlimit (0.0f, juce::jmax (0.0f, contentWidth - (float) getWidth()), scroll - delta * 240.0f);
    repaint();
}

void PatternBar::showMenu (int uid)
{
    auto& project = ctx.project;
    juce::PopupMenu menu;
    menu.addItem ("Open in piano roll", [this, uid] { ctx.project.setCurrentPattern (uid); if (ctx.openPianoRoll) ctx.openPianoRoll (ctx.project.selectedChannel()); });
    menu.addItem ("Rename", [&project, uid]
    {
        askForText ("Rename pattern", "Name", project.findPattern (uid)[id::name].toString(), [&project, uid] (const juce::String& text)
        {
            if (text.trim().isNotEmpty())
            {
                project.beginTransaction ("Rename pattern");
                project.findPattern (uid).setProperty (id::name, text.trim(), project.um());
            }
        });
    });
    menu.addItem ("Clone", [&project, uid] { project.beginTransaction ("Clone pattern"); project.setCurrentPattern (project.clonePattern (uid)[id::uid]); });

    juce::PopupMenu colourMenu;
    for (int i = 0; i < 10; ++i)
        colourMenu.addColouredItem (100 + i, "Colour " + juce::String (i + 1), palette::channelColour (i), true, false, nullptr);
    menu.addSubMenu ("Colour", colourMenu);

    menu.addItem ("Add to playlist at playhead", [this, uid]
    {
        auto& p = ctx.project;
        auto pattern = p.findPattern (uid);
        const auto bar = p.ticksPerBar();
        const auto at = ((juce::int64) ctx.engine.getDisplayPositionTicks() / bar) * bar;

        for (auto track : p.tracksTree())
        {
            bool free = true;
            for (auto clip : track)
                free = free && ((juce::int64) clip[id::start] >= at + p.patternLength (pattern)
                                || (juce::int64) clip[id::start] + (juce::int64) clip[id::length] <= at);
            if (free)
            {
                p.beginTransaction ("Add clip");
                p.addClip (track, kind::pattern, uid, at, p.patternLength (pattern));
                return;
            }
        }
    });
    menu.addSeparator();
    menu.addItem ("Delete", project.patternsTree().getNumChildren() > 1, false, [&project, uid]
    {
        project.beginTransaction ("Delete pattern");
        project.removePattern (uid);
    });

    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition(), [&project, uid] (int result)
    {
        if (result >= 100 && result < 110)
        {
            project.beginTransaction ("Pattern colour");
            project.findPattern (uid).setProperty (id::colour, palette::channelColour (result - 100).toString(), project.um());
        }
    });
}

void PatternBar::valueTreePropertyChanged (juce::ValueTree& t, const juce::Identifier& p)
{
    if (t.hasType (id::PATTERN) && (p == id::name || p == id::colour))
        layoutChips();
    else if (t.hasType (id::PROJECT) && p == id::currentPattern)
    {
        scrollToCurrent();
        repaint();
    }
}

void PatternBar::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent.hasType (id::PATTERNS) || parent.hasType (id::PROJECT))
    {
        layoutChips();
        scrollToCurrent();
    }
}

void PatternBar::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent.hasType (id::PATTERNS) || parent.hasType (id::PROJECT))
        layoutChips();
}

void PatternBar::valueTreeChildOrderChanged (juce::ValueTree& parent, int, int)
{
    if (parent.hasType (id::PATTERNS))
        layoutChips();
}
}
