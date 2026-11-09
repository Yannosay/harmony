#pragma once

#include "ui/Theme.h"
#include "model/Project.h"

namespace harmony::ui
{
struct SnapOption
{
    juce::String name;
    int ticks;
};

const std::vector<SnapOption>& snapOptions();

enum class Tool { draw, select, slice, mute };

struct EditTarget
{
    virtual ~EditTarget() = default;
    virtual void editCut() = 0;
    virtual void editCopy() = 0;
    virtual void editPaste() = 0;
    virtual void editDelete() = 0;
    virtual void editSelectAll() = 0;
    virtual void editDuplicate() = 0;
    virtual void editQuantize() {}
    virtual void editHumanize() {}
    virtual void editToggleMute() = 0;
    virtual void setTool (Tool) = 0;
};

class TimelineCanvas : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit TimelineCanvas (Project& project);
    ~TimelineCanvas() override;

    double tickToX (double tick) const noexcept;
    double xToTick (double x) const noexcept;
    double getPixelsPerTick() const noexcept { return pxPerTick.current; }
    double getScrollY() const noexcept { return scrollY.current; }
    double getRowHeight() const noexcept { return rowHeight.current; }

    void scrollToTick (double tick, bool animate = true);
    void scrollToY (double y, bool animate = true);
    void zoomHorizontally (double factor, double anchorX);
    void zoomVertically (double factor, double anchorY);
    void followPlayhead (double tick);
    void setSnapIndex (int index) { snapIndex = juce::jlimit (0, (int) snapOptions().size() - 1, index); repaint(); }
    int getSnapIndex() const noexcept { return snapIndex; }

    juce::int64 snapTicks() const;
    juce::int64 snap (double tick, bool roundToNearest = false) const;

    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseMagnify (const juce::MouseEvent&, float scaleFactor) override;

protected:
    struct Animated
    {
        double current, target;
        bool step (double alpha) noexcept
        {
            const double diff = target - current;
            if (std::abs (diff) < 1.0e-4 * juce::jmax (1.0, std::abs (target)))
            {
                const bool changed = current != target;
                current = target;
                return changed;
            }
            current += diff * alpha;
            return true;
        }
    };

    virtual juce::Rectangle<int> getGridArea() const = 0;
    virtual double getContentHeight() const = 0;
    virtual double getContentLengthTicks() const = 0;
    virtual void viewChanged() {}
    virtual void animationTick() {}

    bool beginPan (const juce::MouseEvent&);
    bool dragPan (const juce::MouseEvent&);
    bool endPan (const juce::MouseEvent&);

    void drawTimeGrid (juce::Graphics&, juce::Rectangle<int> area) const;
    void drawRuler (juce::Graphics&, juce::Rectangle<int> area, juce::int64 loopStart, juce::int64 loopEnd) const;
    void drawPlayhead (juce::Graphics&, juce::Rectangle<int> area, double tick) const;
    juce::int64 gridTicksForSpacing (double minPixels) const;

    Project& project;
    Animated viewTick { 0.0, 0.0 };
    Animated pxPerTick { 0.06, 0.06 };
    Animated scrollY { 0.0, 0.0 };
    Animated rowHeight { 16.0, 16.0 };
    double minPxPerTick = 0.0025, maxPxPerTick = 1.2;
    double minRowHeight = 8.0, maxRowHeight = 48.0;
    int snapIndex = 0;

private:
    void onVBlank (double timestampSec);
    void clampTargets();

    juce::VBlankAttachment vblank;
    double lastFrame = 0.0;
    bool panning = false;
    juce::Point<float> panLast;
    juce::Point<double> flingVelocity;
    double lastPanTime = 0.0;
};
}
