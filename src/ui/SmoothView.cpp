#include "ui/SmoothView.h"

namespace harmony::ui
{
const std::vector<SnapOption>& snapOptions()
{
    static const std::vector<SnapOption> options {
        { "Auto",     0 },
        { "Bar",      -2 },
        { "Beat",     ppq },
        { "1/2 beat", ppq / 2 },
        { "1/3 beat", ppq / 3 },
        { "Step",     ppq / 4 },
        { "1/6 beat", ppq / 6 },
        { "1/8 beat", ppq / 8 },
        { "1/16 beat", ppq / 16 },
        { "None",     -1 }
    };
    return options;
}

TimelineCanvas::TimelineCanvas (Project& p)
    : project (p), vblank (this, [this] (double t) { onVBlank (t); })
{
    setOpaque (true);
    setWantsKeyboardFocus (true);
}

TimelineCanvas::~TimelineCanvas() = default;

double TimelineCanvas::tickToX (double tick) const noexcept
{
    return (double) getGridArea().getX() + (tick - viewTick.current) * pxPerTick.current;
}

double TimelineCanvas::xToTick (double x) const noexcept
{
    return viewTick.current + (x - (double) getGridArea().getX()) / pxPerTick.current;
}

juce::int64 TimelineCanvas::gridTicksForSpacing (double minPixels) const
{
    const juce::int64 bar = project.ticksPerBar();
    const juce::int64 candidates[] = { ppq / 16, ppq / 8, ppq / 4, ppq / 2, ppq, bar, bar * 2, bar * 4, bar * 8, bar * 16, bar * 32 };

    for (auto c : candidates)
        if ((double) c * pxPerTick.target >= minPixels)
            return c;

    return bar * 64;
}

juce::int64 TimelineCanvas::snapTicks() const
{
    const int t = snapOptions()[(size_t) snapIndex].ticks;
    if (t == 0)  return juce::jmin ((juce::int64) project.ticksPerBar(), gridTicksForSpacing (14.0));
    if (t == -2) return project.ticksPerBar();
    if (t < 0)   return 1;
    return t;
}

juce::int64 TimelineCanvas::snap (double tick, bool roundToNearest) const
{
    const auto s = (double) juce::jmax ((juce::int64) 1, snapTicks());
    const double snapped = roundToNearest ? std::round (tick / s) * s : std::floor (tick / s) * s;
    return (juce::int64) juce::jmax (0.0, snapped);
}

void TimelineCanvas::clampTargets()
{
    pxPerTick.target = juce::jlimit (minPxPerTick, maxPxPerTick, pxPerTick.target);
    rowHeight.target = juce::jlimit (minRowHeight, maxRowHeight, rowHeight.target);

    const auto grid = getGridArea();
    const double visibleTicks = grid.getWidth() / pxPerTick.target;
    const double length = juce::jmax (getContentLengthTicks(), visibleTicks);
    viewTick.target = juce::jlimit (0.0, juce::jmax (0.0, length - visibleTicks * 0.5), viewTick.target);

    const double maxY = juce::jmax (0.0, getContentHeight() - grid.getHeight());
    scrollY.target = juce::jlimit (0.0, maxY, scrollY.target);
}

void TimelineCanvas::scrollToTick (double tick, bool animate)
{
    viewTick.target = tick;
    clampTargets();
    if (! animate)
        viewTick.current = viewTick.target;
    repaint();
}

void TimelineCanvas::scrollToY (double y, bool animate)
{
    scrollY.target = y;
    clampTargets();
    if (! animate)
        scrollY.current = scrollY.target;
    repaint();
}

void TimelineCanvas::zoomHorizontally (double factor, double anchorX)
{
    const double anchorTick = viewTick.target + (anchorX - getGridArea().getX()) / pxPerTick.target;
    pxPerTick.target *= factor;
    clampTargets();
    viewTick.target = anchorTick - (anchorX - getGridArea().getX()) / pxPerTick.target;
    clampTargets();
}

void TimelineCanvas::zoomVertically (double factor, double anchorY)
{
    const auto grid = getGridArea();
    const double anchorContent = (scrollY.target + anchorY - grid.getY()) / rowHeight.target;
    rowHeight.target *= factor;
    clampTargets();
    scrollY.target = anchorContent * rowHeight.target - (anchorY - grid.getY());
    clampTargets();
}

void TimelineCanvas::followPlayhead (double tick)
{
    const auto grid = getGridArea();
    const double x = tickToX (tick);
    if (x > grid.getRight() - grid.getWidth() * 0.08 || x < grid.getX())
        scrollToTick (tick - (grid.getWidth() * 0.1) / pxPerTick.target);
}

void TimelineCanvas::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const double dy = wheel.deltaY;
    const double dx = wheel.deltaX;
    flingVelocity = {};

    if (e.mods.isCtrlDown() || e.mods.isCommandDown())
    {
        zoomHorizontally (std::pow (1.0015, (wheel.isSmooth ? dy * 160.0 : dy * 220.0) * 1.1), e.position.x);
    }
    else if (e.mods.isAltDown())
    {
        zoomVertically (std::pow (1.0015, dy * 200.0), e.position.y);
    }
    else if (e.mods.isShiftDown() || std::abs (dx) > std::abs (dy))
    {
        const double delta = (std::abs (dx) > std::abs (dy) ? dx : dy) * 420.0;
        viewTick.target -= delta / pxPerTick.target;
        clampTargets();
    }
    else
    {
        scrollY.target -= dy * 320.0;
        clampTargets();
    }
}

void TimelineCanvas::mouseMagnify (const juce::MouseEvent& e, float scaleFactor)
{
    zoomHorizontally (scaleFactor, e.position.x);
}

bool TimelineCanvas::beginPan (const juce::MouseEvent& e)
{
    if (! e.mods.isMiddleButtonDown())
        return false;

    panning = true;
    panLast = e.position;
    flingVelocity = {};
    lastPanTime = juce::Time::getMillisecondCounterHiRes();
    setMouseCursor (juce::MouseCursor::DraggingHandCursor);
    return true;
}

bool TimelineCanvas::dragPan (const juce::MouseEvent& e)
{
    if (! panning)
        return false;

    const auto delta = e.position - panLast;
    panLast = e.position;
    const double now = juce::Time::getMillisecondCounterHiRes();
    const double dt = juce::jmax (1.0, now - lastPanTime);
    lastPanTime = now;

    viewTick.target -= delta.x / pxPerTick.target;
    scrollY.target -= delta.y;
    clampTargets();
    viewTick.current = viewTick.target;
    scrollY.current = scrollY.target;
    flingVelocity = { delta.x / dt, delta.y / dt };
    repaint();
    return true;
}

bool TimelineCanvas::endPan (const juce::MouseEvent&)
{
    if (! panning)
        return false;

    panning = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    if (juce::Time::getMillisecondCounterHiRes() - lastPanTime > 60.0)
        flingVelocity = {};
    return true;
}

void TimelineCanvas::onVBlank (double timestampSec)
{
    const double dt = lastFrame > 0.0 ? juce::jlimit (0.001, 0.1, timestampSec - lastFrame) : 1.0 / 60.0;
    lastFrame = timestampSec;

    if (! panning && (std::abs (flingVelocity.x) > 0.01 || std::abs (flingVelocity.y) > 0.01))
    {
        viewTick.target -= flingVelocity.x * dt * 1000.0 / pxPerTick.target;
        scrollY.target -= flingVelocity.y * dt * 1000.0;
        const double decay = std::exp (-dt * 5.5);
        flingVelocity *= decay;
        clampTargets();
    }

    const double alpha = 1.0 - std::exp (-dt * 20.0);
    bool changed = false;
    changed |= viewTick.step (alpha);
    changed |= pxPerTick.step (alpha);
    changed |= scrollY.step (alpha);
    changed |= rowHeight.step (alpha);

    animationTick();

    if (changed)
    {
        viewChanged();
        repaint();
    }
}

void TimelineCanvas::drawTimeGrid (juce::Graphics& g, juce::Rectangle<int> area) const
{
    const juce::int64 bar = project.ticksPerBar();
    const juce::int64 beat = project.ticksPerBeat();
    const juce::int64 fine = gridTicksForSpacing (10.0);
    const double startTick = juce::jmax (0.0, xToTick (area.getX()));
    const double endTick = xToTick (area.getRight());

    for (juce::int64 t = (juce::int64) (startTick / (double) fine) * fine; (double) t <= endTick; t += fine)
    {
        const float x = (float) tickToX ((double) t);
        if (t % bar == 0)
        {
            g.setColour (colours::gridBar);
            g.fillRect (x, (float) area.getY(), 1.0f, (float) area.getHeight());
        }
        else if (t % beat == 0)
        {
            g.setColour (colours::gridBeat);
            g.fillRect (x, (float) area.getY(), 1.0f, (float) area.getHeight());
        }
        else
        {
            g.setColour (colours::gridStep);
            g.fillRect (x, (float) area.getY(), 1.0f, (float) area.getHeight());
        }
    }
}

void TimelineCanvas::drawRuler (juce::Graphics& g, juce::Rectangle<int> area, juce::int64 loopStart, juce::int64 loopEnd) const
{
    g.setColour (colours::panel);
    g.fillRect (area);

    if (loopEnd > loopStart)
    {
        const float x1 = (float) tickToX ((double) loopStart);
        const float x2 = (float) tickToX ((double) loopEnd);
        g.setColour (colours::accent.withAlpha (0.16f));
        g.fillRect (x1, (float) area.getY(), x2 - x1, (float) area.getHeight());
        g.setColour (colours::accent);
        g.fillRect (x1, (float) area.getBottom() - 3.0f, x2 - x1, 3.0f);
    }

    const juce::int64 bar = project.ticksPerBar();
    const juce::int64 labelStep = juce::jmax (bar, gridTicksForSpacing (56.0));
    const juce::int64 tickStep = gridTicksForSpacing (8.0);
    const double startTick = juce::jmax (0.0, xToTick (area.getX()));
    const double endTick = xToTick (area.getRight());

    g.setFont (font (10.0f, Weight::medium));
    for (juce::int64 t = (juce::int64) (startTick / (double) tickStep) * tickStep; (double) t <= endTick; t += tickStep)
    {
        const float x = (float) tickToX ((double) t);
        const bool isBar = t % bar == 0;
        g.setColour (isBar ? colours::textFaint : colours::line);
        const float h = isBar ? 8.0f : 4.0f;
        g.fillRect (x, (float) area.getBottom() - h, 1.0f, h);

        if (t % labelStep == 0)
        {
            g.setColour (colours::textDim);
            g.drawText (juce::String (t / bar + 1), juce::Rectangle<float> (x + 4.0f, (float) area.getY(), 60.0f, (float) area.getHeight() - 6.0f),
                        juce::Justification::centredLeft, false);
        }
    }

    g.setColour (colours::line);
    g.fillRect (area.getX(), area.getBottom() - 1, area.getWidth(), 1);
}

void TimelineCanvas::drawPlayhead (juce::Graphics& g, juce::Rectangle<int> area, double tick) const
{
    const float x = (float) tickToX (tick);
    if (x < (float) area.getX() - 1.0f || x > (float) area.getRight())
        return;

    g.setColour (colours::playhead.withAlpha (0.9f));
    g.fillRect (x, (float) area.getY(), 1.25f, (float) area.getHeight());

    juce::Path head;
    head.addTriangle (x - 5.0f, (float) area.getY(), x + 6.0f, (float) area.getY(), x + 0.5f, (float) area.getY() + 7.0f);
    g.fillPath (head);
}
}
