#include "ui/AutomationEditor.h"
#include "model/Automation.h"
#include "engine/AudioEngine.h"

namespace harmony::ui
{
juce::String automationTargetName (Context& ctx, const juce::ValueTree& clip)
{
    const auto target = automation::parse (clip[id::target].toString());
    juce::String parameterName;

    if (target.kind == automation::Kind::slotParameter || target.kind == automation::Kind::channelParameter)
    {
        auto* plugin = target.kind == automation::Kind::slotParameter ? ctx.sync.pluginForSlot (target.id) : ctx.sync.pluginForChannel (target.id);
        if (plugin != nullptr && juce::isPositiveAndBelow (target.parameter, plugin->getParameters().size()))
            parameterName = plugin->getName() + " " + plugin->getParameters()[target.parameter]->getName (40);
    }

    return automation::describeTarget (ctx.project, target, parameterName);
}

namespace
{
    class AutomationEditor final : public juce::Component, private juce::ValueTree::Listener
    {
    public:
        AutomationEditor (Context& c, juce::ValueTree t)
            : ctx (c), clip (std::move (t))
        {
            setSize (860, 380);
            setWantsKeyboardFocus (true);
            clip.addListener (this);
            points = automation::readPoints (clip);

            for (auto* b : { &flatButton, &rampUpButton, &rampDownButton })
            {
                addAndMakeVisible (b);
                keepFocusOnEditors ({ b });
            }
            flatButton.onClick = [this] { applyPreset ({ { 0.0, 0.5f, 0.0f }, { 1.0, 0.5f, 0.0f } }); };
            rampUpButton.onClick = [this] { applyPreset ({ { 0.0, 0.0f, 0.0f }, { 1.0, 1.0f, 0.0f } }); };
            rampDownButton.onClick = [this] { applyPreset ({ { 0.0, 1.0f, 0.0f }, { 1.0, 0.0f, 0.0f } }); };
        }

        ~AutomationEditor() override
        {
            clip.removeListener (this);
        }

        void paint (juce::Graphics& g) override
        {
            const auto area = graphArea().toFloat();
            g.setColour (colours::text);
            g.setFont (font (13.0f, Weight::semibold));
            g.drawText (clip[id::name].toString(), getLocalBounds().reduced (20, 0).removeFromTop (34), juce::Justification::centredLeft, true);
            g.setColour (colours::textDim);
            g.setFont (font (10.0f));
            g.drawText (automationTargetName (ctx, clip), getLocalBounds().reduced (20, 0).withTrimmedTop (30).removeFromTop (18),
                        juce::Justification::centredLeft, true);

            g.setColour (colours::background);
            g.fillRoundedRectangle (area, 8.0f);

            const auto bar = (double) ctx.project.ticksPerBar();
            const auto beat = (double) ctx.project.ticksPerBeat();
            const double length = clipLength();
            for (double t = 0.0; t <= length; t += beat)
            {
                const bool isBar = std::fmod (t, bar) < 1.0;
                g.setColour (isBar ? colours::gridBar : colours::gridStep);
                g.fillRect (timeToX (t), area.getY(), 1.0f, area.getHeight());
                if (isBar)
                {
                    g.setColour (colours::textFaint);
                    g.setFont (font (9.0f));
                    g.drawText (juce::String ((int) (t / bar) + 1), juce::Rectangle<float> (timeToX (t) + 4.0f, area.getY() + 4.0f, 30.0f, 12.0f),
                                juce::Justification::centredLeft, false);
                }
            }
            g.setColour (colours::gridBeat);
            g.fillRect (area.getX(), valueToY (0.5f), area.getWidth(), 1.0f);

            juce::Path curve;
            const int steps = juce::jmax (2, (int) area.getWidth());
            for (int i = 0; i <= steps; ++i)
            {
                const double t = length * i / steps;
                const float y = valueToY (automation::valueAt (points, t));
                if (i == 0) curve.startNewSubPath (timeToX (t), y);
                else        curve.lineTo (timeToX (t), y);
            }

            juce::Path fill (curve);
            fill.lineTo (area.getRight(), area.getBottom());
            fill.lineTo (area.getX(), area.getBottom());
            fill.closeSubPath();
            g.setColour (colours::lilac.withAlpha (0.12f));
            g.fillPath (fill);
            g.setColour (colours::lilac);
            g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));

            for (size_t i = 0; i < points.size(); ++i)
            {
                const auto c = pointPosition (i);
                const bool active = (int) i == dragging || (int) i == hovered;
                g.setColour (active ? colours::text : colours::lilac);
                g.fillEllipse (c.x - 5.0f, c.y - 5.0f, 10.0f, 10.0f);
                g.setColour (colours::background);
                g.fillEllipse (c.x - 2.0f, c.y - 2.0f, 4.0f, 4.0f);
            }

            if (hovered >= 0 || dragging >= 0)
            {
                const int i = dragging >= 0 ? dragging : hovered;
                const auto c = pointPosition ((size_t) i);
                g.setColour (colours::text);
                g.setFont (font (10.0f, Weight::medium));
                g.drawText (describeValue (points[(size_t) i].value), juce::Rectangle<float> (c.x + 8.0f, c.y - 18.0f, 90.0f, 14.0f),
                            juce::Justification::centredLeft, false);
            }

            g.setColour (colours::textFaint);
            g.setFont (font (9.5f));
            g.drawText ("Click to add a point, drag to move, right-click to delete, Alt+drag a curve to bend it",
                        getLocalBounds().reduced (20, 0).removeFromBottom (30), juce::Justification::centredLeft, true);
        }

        void resized() override
        {
            auto bar = getLocalBounds().reduced (20, 0).removeFromTop (48).withTrimmedTop (10);
            for (auto* b : { &rampDownButton, &rampUpButton, &flatButton })
            {
                b->setBounds (bar.removeFromRight (88).withHeight (26));
                bar.removeFromRight (6);
            }
        }

        void mouseMove (const juce::MouseEvent& e) override
        {
            const int hit = pointAt (e.position);
            if (hit != hovered)
            {
                hovered = hit;
                repaint();
            }
            setMouseCursor (hit >= 0 ? juce::MouseCursor::DraggingHandCursor
                            : graphArea().contains (e.getPosition()) ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor);
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! graphArea().expanded (6).contains (e.getPosition()))
                return;

            ctx.project.beginTransaction ("Edit automation");
            int hit = pointAt (e.position);

            if (e.mods.isPopupMenu())
            {
                if (hit >= 0 && points.size() > 1)
                {
                    points.erase (points.begin() + hit);
                    commit();
                }
                return;
            }

            if (e.mods.isAltDown() && hit < 0)
            {
                bendSegment = segmentAt (xToTime (e.position.x));
                bendStartY = e.position.y;
                bendStartTension = bendSegment >= 0 ? points[(size_t) bendSegment].tension : 0.0f;
                return;
            }

            if (hit < 0)
            {
                automation::Point p { snapTime (xToTime (e.position.x), e.mods.isShiftDown()), yToValue (e.position.y), 0.0f };
                points.push_back (p);
                std::stable_sort (points.begin(), points.end(), [] (auto& a, auto& b) { return a.time < b.time; });
                for (size_t i = 0; i < points.size(); ++i)
                    if (points[i].time == p.time && points[i].value == p.value)
                        hit = (int) i;
                commit();
            }

            dragging = hit;
            repaint();
        }

        void mouseDrag (const juce::MouseEvent& e) override
        {
            if (bendSegment >= 0)
            {
                const float delta = (bendStartY - e.position.y) / 120.0f;
                const bool rising = points[(size_t) bendSegment + 1].value >= points[(size_t) bendSegment].value;
                points[(size_t) bendSegment].tension = juce::jlimit (-1.0f, 1.0f, bendStartTension + (rising ? delta : -delta));
                commit();
                return;
            }

            if (dragging < 0)
                return;

            auto& p = points[(size_t) dragging];
            const double minTime = dragging > 0 ? points[(size_t) dragging - 1].time : 0.0;
            const double maxTime = dragging + 1 < (int) points.size() ? points[(size_t) dragging + 1].time : clipLength();
            p.time = juce::jlimit (minTime, maxTime, snapTime (xToTime (e.position.x), e.mods.isShiftDown()));
            p.value = yToValue (e.position.y);
            commit();
        }

        void mouseUp (const juce::MouseEvent&) override
        {
            dragging = -1;
            bendSegment = -1;
            repaint();
        }

        void mouseDoubleClick (const juce::MouseEvent& e) override
        {
            const int segment = segmentAt (xToTime (e.position.x));
            if (segment >= 0 && pointAt (e.position) < 0)
            {
                ctx.project.beginTransaction ("Straighten automation");
                points[(size_t) segment].tension = 0.0f;
                commit();
            }
        }

    private:
        juce::Rectangle<int> graphArea() const { return getLocalBounds().reduced (20, 0).withTrimmedTop (56).withTrimmedBottom (34); }
        double clipLength() const { return (double) juce::jmax ((juce::int64) 1, (juce::int64) clip[id::length]) + (double) (juce::int64) clip[id::offset]; }

        float timeToX (double t) const
        {
            const auto a = graphArea().toFloat();
            return a.getX() + a.getWidth() * (float) (t / clipLength());
        }

        double xToTime (float x) const
        {
            const auto a = graphArea().toFloat();
            return juce::jlimit (0.0, clipLength(), (double) ((x - a.getX()) / a.getWidth()) * clipLength());
        }

        float valueToY (float v) const
        {
            const auto a = graphArea().toFloat().reduced (0.0f, 8.0f);
            return a.getBottom() - v * a.getHeight();
        }

        float yToValue (float y) const
        {
            const auto a = graphArea().toFloat().reduced (0.0f, 8.0f);
            return juce::jlimit (0.0f, 1.0f, (a.getBottom() - y) / a.getHeight());
        }

        double snapTime (double t, bool free) const
        {
            if (free)
                return t;
            const double step = (double) ticksPerStep;
            return juce::jlimit (0.0, clipLength(), std::round (t / step) * step);
        }

        juce::Point<float> pointPosition (size_t i) const
        {
            return { timeToX (points[i].time), valueToY (points[i].value) };
        }

        int pointAt (juce::Point<float> p) const
        {
            for (size_t i = 0; i < points.size(); ++i)
                if (pointPosition (i).getDistanceFrom (p) <= 8.0f)
                    return (int) i;
            return -1;
        }

        int segmentAt (double t) const
        {
            for (size_t i = 0; i + 1 < points.size(); ++i)
                if (t >= points[i].time && t <= points[i + 1].time)
                    return (int) i;
            return -1;
        }

        juce::String describeValue (float v) const
        {
            const auto target = automation::parse (clip[id::target].toString());
            switch (target.kind)
            {
                case automation::Kind::channelVolume:
                case automation::Kind::insertVolume:
                    return formatDecibels (automation::faderFromNormalised (v));
                case automation::Kind::channelPan:
                case automation::Kind::insertPan:
                {
                    const float pan = automation::panFromNormalised (v);
                    return std::abs (pan) < 0.005f ? juce::String ("Centre") : juce::String (std::abs (pan) * 100.0f, 0) + (pan < 0.0f ? "% L" : "% R");
                }
                case automation::Kind::slotParameter:
                case automation::Kind::channelParameter:
                {
                    auto* plugin = target.kind == automation::Kind::slotParameter ? ctx.sync.pluginForSlot (target.id) : ctx.sync.pluginForChannel (target.id);
                    if (plugin != nullptr && juce::isPositiveAndBelow (target.parameter, plugin->getParameters().size()))
                    {
                        auto* param = plugin->getParameters()[target.parameter];
                        return param->getText (v, 24) + " " + param->getLabel();
                    }
                    break;
                }
                case automation::Kind::none:
                    break;
            }
            return juce::String (juce::roundToInt (v * 100.0f)) + "%";
        }

        void applyPreset (std::vector<automation::Point> preset)
        {
            for (auto& p : preset)
                p.time *= clipLength();
            ctx.project.beginTransaction ("Reset automation");
            points = std::move (preset);
            commit();
        }

        void commit()
        {
            const juce::ScopedValueSetter<bool> svs (writing, true);
            automation::writePoints (clip, points, ctx.project.um());
            repaint();
        }

        void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override        { reload(); }
        void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override { reload(); }
        void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override { reload(); }

        void reload()
        {
            if (writing)
                return;
            points = automation::readPoints (clip);
            dragging = hovered = bendSegment = -1;
            repaint();
        }

        Context& ctx;
        juce::ValueTree clip;
        std::vector<automation::Point> points;
        int dragging = -1, hovered = -1, bendSegment = -1;
        float bendStartY = 0.0f, bendStartTension = 0.0f;
        bool writing = false;
        juce::TextButton flatButton { "Flat" }, rampUpButton { "Ramp up" }, rampDownButton { "Ramp down" };
    };
}

std::unique_ptr<juce::Component> createAutomationEditor (Context& ctx, juce::ValueTree clip)
{
    return std::make_unique<AutomationEditor> (ctx, std::move (clip));
}
}
