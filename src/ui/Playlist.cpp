#include "ui/Playlist.h"
#include "ui/Dialogs.h"
#include "engine/AudioEngine.h"

namespace harmony::ui
{
std::vector<juce::ValueTree> Playlist::clipboard;

namespace
{
    constexpr int toolbarHeight = 40;
    constexpr int rulerHeight = 26;
    constexpr int headerWidth = 168;
    constexpr float edgeGrab = 7.0f;
}

Playlist::Playlist (Context& c)
    : TimelineCanvas (c.project), ctx (c)
{
    rowHeight = { 46.0, 46.0 };
    pxPerTick = { 0.022, 0.022 };
    minRowHeight = 24.0;
    maxRowHeight = 140.0;

    for (auto* b : { &drawButton, &selectButton, &sliceButton, &muteButton, &followButton })
        addAndMakeVisible (b);

    drawButton.onClick = [this] { setTool (Tool::draw); };
    selectButton.onClick = [this] { setTool (Tool::select); };
    sliceButton.onClick = [this] { setTool (Tool::slice); };
    muteButton.onClick = [this] { setTool (Tool::mute); };
    followButton.onClick = [this] { follow = ! follow; refreshToolbar(); };

    for (auto& s : snapOptions())
        snapBox.addItem (s.name, snapBox.getNumItems() + 1);
    snapBox.setSelectedItemIndex (1, juce::dontSendNotification);
    snapIndex = 1;
    snapBox.setTooltip ("Snap");
    snapBox.onChange = [this] { setSnapIndex (snapBox.getSelectedItemIndex()); };
    addAndMakeVisible (snapBox);

    patternButton.setTooltip ("Pattern that the draw tool places");
    patternButton.onClick = [this]
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("Paint with pattern");
        const int current = project.currentPattern()[id::uid];
        for (auto p : project.patternsTree())
        {
            const int uid = p[id::uid];
            menu.addItem (p[id::name].toString(), true, uid == current, [this, uid] { project.setCurrentPattern (uid); });
        }
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (patternButton));
    };
    addAndMakeVisible (patternButton);

    keepFocusOnEditors ({ &snapBox, &patternButton });
    project.getState().addListener (this);
    refreshToolbar();
}

Playlist::~Playlist()
{
    project.getState().removeListener (this);
}

void Playlist::setTool (Tool t)
{
    tool = t;
    refreshToolbar();
}

void Playlist::refreshToolbar()
{
    drawButton.setToggleState (tool == Tool::draw, juce::dontSendNotification);
    selectButton.setToggleState (tool == Tool::select, juce::dontSendNotification);
    sliceButton.setToggleState (tool == Tool::slice, juce::dontSendNotification);
    muteButton.setToggleState (tool == Tool::mute, juce::dontSendNotification);
    followButton.setToggleState (follow, juce::dontSendNotification);

    const auto pattern = project.currentPattern();
    patternButton.setButtonText (pattern[id::name].toString());
    patternButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromString (pattern[id::colour].toString()).withAlpha (0.2f));
}

void Playlist::resized()
{
    auto r = getLocalBounds();
    toolbarArea = r.removeFromTop (toolbarHeight);
    rulerArea = r.removeFromTop (rulerHeight).withTrimmedLeft (headerWidth);
    headerArea = r.removeFromLeft (headerWidth);
    gridArea = r;

    auto bar = toolbarArea.reduced (8, 6);
    bar.removeFromLeft (headerWidth - 8);
    for (auto* b : { &drawButton, &selectButton, &sliceButton, &muteButton })
    {
        b->setBounds (bar.removeFromLeft (28));
        bar.removeFromLeft (2);
    }
    bar.removeFromLeft (10);
    snapBox.setBounds (bar.removeFromLeft (104));
    bar.removeFromLeft (10);
    patternButton.setBounds (bar.removeFromLeft (150));
    bar.removeFromLeft (6);
    followButton.setBounds (bar.removeFromLeft (28));
}

double Playlist::getContentHeight() const
{
    return project.tracksTree().getNumChildren() * rowHeight.target + 40.0;
}

double Playlist::getContentLengthTicks() const
{
    return (double) juce::jmax (project.songLength(), project.ticksPerBar() * 64) + (double) project.ticksPerBar() * 32.0;
}

float Playlist::trackY (int index) const
{
    return (float) (gridArea.getY() - scrollY.current + index * rowHeight.current);
}

int Playlist::trackAtY (float y) const
{
    const int count = project.tracksTree().getNumChildren();
    return juce::jlimit (0, juce::jmax (0, count - 1), (int) std::floor (((double) y - gridArea.getY() + scrollY.current) / rowHeight.current));
}

juce::int64 Playlist::sourceLength (const juce::ValueTree& clip) const
{
    if (clip[id::kind].toString() == kind::pattern)
        return project.patternLength (project.findPattern (clip[id::pattern]));

    if (auto sample = ctx.sync.sampleForChannel (clip[id::channel]))
    {
        const double seconds = sample->audio.getNumSamples() / sample->sampleRate;
        return (juce::int64) (seconds * project.getBpm() / 60.0 * ppq);
    }
    return (juce::int64) clip[id::length];
}

juce::Rectangle<float> Playlist::clipRect (const juce::ValueTree& clip) const
{
    const int trackIndex = project.tracksTree().indexOf (clip.getParent());
    const float x1 = (float) tickToX ((double) (juce::int64) clip[id::start]);
    const float x2 = (float) tickToX ((double) ((juce::int64) clip[id::start] + (juce::int64) clip[id::length]));
    return { x1, trackY (trackIndex) + 1.0f, juce::jmax (4.0f, x2 - x1), (float) rowHeight.current - 2.0f };
}

juce::ValueTree Playlist::hitTest (juce::Point<float> p, bool& rightEdge, bool& leftEdge) const
{
    rightEdge = leftEdge = false;
    const int trackIndex = trackAtY (p.y);
    auto track = project.tracksTree().getChild (trackIndex);

    for (int i = track.getNumChildren(); --i >= 0;)
    {
        auto clip = track.getChild (i);
        const auto r = clipRect (clip);
        if (r.contains (p))
        {
            const float grab = juce::jmin (edgeGrab, r.getWidth() * 0.25f);
            rightEdge = p.x >= r.getRight() - grab;
            leftEdge = ! rightEdge && p.x <= r.getX() + grab;
            return clip;
        }
    }
    return {};
}

void Playlist::pruneSelection()
{
    for (int i = selection.size(); --i >= 0;)
        if (! selection.getReference (i).getParent().isValid())
            selection.remove (i);
}

void Playlist::captureOriginals()
{
    originals.clear();
    for (auto& c : selection)
        originals.push_back ({ c, project.tracksTree().indexOf (c.getParent()), (juce::int64) c[id::start], (juce::int64) c[id::length], (juce::int64) c[id::offset] });
}

void Playlist::moveClipToTrack (juce::ValueTree& clip, int trackIndex)
{
    auto target = project.tracksTree().getChild (trackIndex);
    auto current = clip.getParent();
    if (! target.isValid() || current == target)
        return;

    current.removeChild (clip, project.um());
    target.appendChild (clip, project.um());
}

const std::vector<std::pair<float, float>>& Playlist::peaksFor (const engine::SampleData& sample)
{
    auto& peaks = peakCache[&sample];
    if (! peaks.empty())
        return peaks;

    constexpr int block = 256;
    const int n = sample.audio.getNumSamples();
    peaks.reserve ((size_t) (n / block + 1));

    for (int start = 0; start < n; start += block)
    {
        const int len = juce::jmin (block, n - start);
        float lo = 0.0f, hi = 0.0f;
        for (int ch = 0; ch < sample.audio.getNumChannels(); ++ch)
        {
            auto range = juce::FloatVectorOperations::findMinAndMax (sample.audio.getReadPointer (ch, start), len);
            lo = juce::jmin (lo, range.getStart());
            hi = juce::jmax (hi, range.getEnd());
        }
        peaks.push_back ({ lo, hi });
    }
    return peaks;
}

void Playlist::drawTracks (juce::Graphics& g) const
{
    const int count = project.tracksTree().getNumChildren();
    for (int i = 0; i < count; ++i)
    {
        const float y = trackY (i);
        if (y > (float) gridArea.getBottom() || y + (float) rowHeight.current < (float) gridArea.getY())
            continue;

        g.setColour (i % 2 == 0 ? juce::Colour (0xff0e0e10) : juce::Colour (0xff111114));
        g.fillRect ((float) gridArea.getX(), y, (float) gridArea.getWidth(), (float) rowHeight.current);
        g.setColour (colours::lineSoft);
        g.fillRect ((float) gridArea.getX(), y + (float) rowHeight.current - 1.0f, (float) gridArea.getWidth(), 1.0f);
    }

    drawTimeGrid (g, gridArea);

    const auto& state = project.getState();
    const juce::int64 ls = state[id::loopStart], le = state[id::loopEnd];
    if (le > ls)
    {
        g.setColour (colours::accent.withAlpha (0.04f));
        const float x1 = (float) tickToX ((double) ls), x2 = (float) tickToX ((double) le);
        g.fillRect (x1, (float) gridArea.getY(), x2 - x1, (float) gridArea.getHeight());
    }
}

void Playlist::drawClip (juce::Graphics& g, const juce::ValueTree& clip, juce::Rectangle<float> r, bool selected)
{
    const bool isPattern = clip[id::kind].toString() == kind::pattern;
    const auto source = isPattern ? project.findPattern (clip[id::pattern]) : project.findChannel (clip[id::channel]);
    auto colour = source.isValid() ? juce::Colour::fromString (source[id::colour].toString()) : colours::textFaint;
    const bool muted = (bool) clip[id::muted] || (bool) clip.getParent()[id::mute];

    if (muted)
        colour = colour.withSaturation (0.0f).withMultipliedBrightness (0.55f);

    g.setColour (colour.withAlpha (0.22f));
    g.fillRoundedRectangle (r, 5.0f);

    const float headerH = juce::jmin (16.0f, r.getHeight() * 0.36f);
    auto header = r.withHeight (headerH);
    g.setColour (colour.withAlpha (0.85f));
    g.fillRoundedRectangle (header, 5.0f);
    g.fillRect (header.withTrimmedTop (headerH * 0.5f));

    g.setColour (selected ? colours::text : colour.withAlpha (0.9f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, selected ? 1.6f : 1.0f);

    if (r.getWidth() > 30.0f)
    {
        g.setColour (colours::background.withAlpha (0.9f));
        g.setFont (font (juce::jmin (10.0f, headerH - 4.0f), Weight::semibold));
        g.drawText (source.isValid() ? source[id::name].toString() : juce::String ("Missing"), header.reduced (6.0f, 0.0f), juce::Justification::centredLeft, true);
    }

    auto body = r.withTrimmedTop (headerH).reduced (3.0f, 3.0f);
    if (body.getHeight() < 4.0f)
        return;

    juce::Graphics::ScopedSaveState state (g);
    g.reduceClipRegion (body.toNearestInt().getIntersection (gridArea));

    const juce::int64 start = clip[id::start], length = clip[id::length], offset = clip[id::offset];

    if (isPattern && source.isValid())
    {
        int lo = 127, hi = 0;
        for (auto n : source)
        {
            lo = juce::jmin (lo, (int) n[id::key]);
            hi = juce::jmax (hi, (int) n[id::key]);
        }
        if (hi < lo)
            return;

        const auto patternLength = juce::jmax ((juce::int64) 1, project.patternLength (source));
        const float range = (float) juce::jmax (8, hi - lo + 1);
        const float noteH = juce::jmax (1.5f, body.getHeight() / range);
        g.setColour (colour.brighter (0.25f).withAlpha (muted ? 0.4f : 0.95f));

        for (juce::int64 rep = offset / patternLength; rep * patternLength - offset < length; ++rep)
        {
            for (auto n : source)
            {
                const auto d = Project::readNote (n);
                if (d.muted)
                    continue;

                const juce::int64 t = rep * patternLength + d.start - offset;
                if (t < 0 || t >= length)
                    continue;

                const float x = (float) tickToX ((double) (start + t));
                if (x > body.getRight())
                    continue;

                const float w = juce::jmax (1.5f, (float) (juce::jmin (d.length, length - t) * pxPerTick.current));
                const float y = body.getBottom() - body.getHeight() * ((float) (d.key - lo + 1) / range);
                g.fillRect (x, y, w, noteH);
            }
        }
    }
    else if (auto sample = ctx.sync.sampleForChannel (clip[id::channel]))
    {
        const auto& peaks = peaksFor (*sample);
        const double samplesPerTick = sample->sampleRate * 60.0 / (project.getBpm() * ppq);
        const float mid = body.getCentreY();
        const float amp = body.getHeight() * 0.5f;
        g.setColour (colour.brighter (0.2f).withAlpha (muted ? 0.35f : 0.9f));

        const int x0 = juce::jmax ((int) body.getX(), gridArea.getX());
        const int x1 = juce::jmin ((int) body.getRight(), gridArea.getRight());
        for (int x = x0; x < x1; ++x)
        {
            const double t0 = xToTick (x) - (double) start + (double) offset;
            const double t1 = xToTick (x + 1) - (double) start + (double) offset;
            const int b0 = juce::jmax (0, (int) (t0 * samplesPerTick / 256.0));
            const int b1 = juce::jmin ((int) peaks.size(), (int) (t1 * samplesPerTick / 256.0) + 1);
            if (b0 >= b1)
                continue;

            float lo = 0.0f, hi = 0.0f;
            for (int b = b0; b < b1; ++b)
            {
                lo = juce::jmin (lo, peaks[(size_t) b].first);
                hi = juce::jmax (hi, peaks[(size_t) b].second);
            }
            g.fillRect ((float) x, mid - hi * amp, 1.0f, juce::jmax (1.0f, (hi - lo) * amp));
        }
    }
}

void Playlist::drawHeaders (juce::Graphics& g) const
{
    g.setColour (colours::panel);
    g.fillRect (headerArea);

    const int count = project.tracksTree().getNumChildren();
    for (int i = 0; i < count; ++i)
    {
        const float y = trackY (i);
        if (y > (float) headerArea.getBottom() || y + (float) rowHeight.current < (float) headerArea.getY())
            continue;

        auto track = project.tracksTree().getChild (i);
        juce::Rectangle<float> r ((float) headerArea.getX(), y, (float) headerArea.getWidth(), (float) rowHeight.current);
        g.setColour (colours::lineSoft);
        g.fillRect (r.removeFromBottom (1.0f));

        const bool muted = track[id::mute];
        auto led = juce::Rectangle<float> (r.getX() + 12.0f, r.getCentreY() - 5.0f, 10.0f, 10.0f);
        g.setColour (muted ? colours::control.brighter (0.2f) : colours::success.withAlpha (0.8f));
        g.fillEllipse (led);

        g.setColour (muted ? colours::textFaint : colours::textDim);
        g.setFont (font (11.0f, Weight::medium));
        g.drawText (track[id::name].toString(), r.withTrimmedLeft (32.0f).withTrimmedRight (8.0f), juce::Justification::centredLeft, true);
    }

    g.setColour (colours::line);
    g.fillRect (headerArea.getRight() - 1, headerArea.getY(), 1, headerArea.getHeight());
}

void Playlist::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (gridArea);
        drawTracks (g);

        const double visibleStart = xToTick (gridArea.getX());
        const double visibleEnd = xToTick (gridArea.getRight());

        for (auto track : project.tracksTree())
            for (auto clip : track)
            {
                const juce::int64 start = clip[id::start], length = clip[id::length];
                if ((double) (start + length) < visibleStart || (double) start > visibleEnd)
                    continue;

                const auto r = clipRect (clip);
                if (r.getBottom() < (float) gridArea.getY() || r.getY() > (float) gridArea.getBottom())
                    continue;

                drawClip (g, clip, r, selection.contains (clip));
            }

        if (drag == Drag::marquee)
        {
            g.setColour (colours::accent.withAlpha (0.12f));
            g.fillRect (marquee);
            g.setColour (colours::accent.withAlpha (0.8f));
            g.drawRect (marquee, 1.0f);
        }

        if (project.songLength() == 0)
        {
            g.setColour (colours::textFaint);
            g.setFont (font (12.0f, Weight::medium));
            g.drawText ("Paint patterns here with the draw tool, or drop audio files to create clips", gridArea, juce::Justification::centred, false);
        }
    }

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (headerArea);
        drawHeaders (g);
    }

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (rulerArea);
        const auto& state = project.getState();
        drawRuler (g, rulerArea, state[id::loopStart], state[id::loopEnd]);
    }

    g.setColour (colours::panel);
    g.fillRect (toolbarArea);
    g.fillRect (0, rulerArea.getY(), headerWidth, rulerHeight);
    g.setColour (colours::line);
    g.fillRect (0, toolbarArea.getBottom() - 1, getWidth(), 1);
    g.fillRect (headerWidth - 1, rulerArea.getY(), 1, rulerHeight);

    g.setColour (colours::text);
    g.setFont (font (12.0f, Weight::semibold));
    g.drawText ("Playlist", toolbarArea.withWidth (headerWidth).reduced (14, 0), juce::Justification::centredLeft, false);

    if (ctx.engine.isSongMode())
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (gridArea.getUnion (rulerArea));
        drawPlayhead (g, rulerArea.getUnion (gridArea), ctx.engine.getDisplayPositionTicks());
    }
}

void Playlist::animationTick()
{
    if (! ctx.engine.isSongMode())
    {
        if (lastPlayheadX >= 0.0f)
        {
            lastPlayheadX = -1.0f;
            repaint();
        }
        return;
    }

    const double tick = ctx.engine.getDisplayPositionTicks();
    const float x = (float) tickToX (tick);

    if (ctx.engine.isPlaying() && follow)
        followPlayhead (tick);

    if (std::abs (x - lastPlayheadX) >= 0.5f)
    {
        const int top = rulerArea.getY();
        const int h = gridArea.getBottom() - top;
        repaint (juce::Rectangle<int> ((int) lastPlayheadX - 12, top, 25, h));
        repaint (juce::Rectangle<int> ((int) x - 12, top, 25, h));
        lastPlayheadX = x;
    }
}

void Playlist::mouseMove (const juce::MouseEvent& e)
{
    if (gridArea.contains (e.position.toInt()))
    {
        bool r, l;
        auto hit = hitTest (e.position, r, l);
        setMouseCursor (hit.isValid() && (r || l) ? juce::MouseCursor::LeftRightResizeCursor
                        : tool == Tool::slice ? juce::MouseCursor::IBeamCursor
                        : hit.isValid() ? juce::MouseCursor::DraggingHandCursor
                                        : juce::MouseCursor::NormalCursor);
        if (ctx.setHint)
            ctx.setHint ("Bar " + juce::String ((juce::int64) xToTick (e.position.x) / project.ticksPerBar() + 1)
                         + "   Click to paint the current pattern, right-click to delete, Ctrl+drag to select, Shift+drag to clone");
    }
    else if (rulerArea.contains (e.position.toInt()))
    {
        setMouseCursor (juce::MouseCursor::NormalCursor);
        if (ctx.setHint)
            ctx.setHint ("Click to set the song position, right-drag to set a loop, double-click to clear the loop");
    }
}

void Playlist::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (beginPan (e))
        return;

    const auto p = e.position;
    dragStart = p;
    dragStartTick = xToTick (p.x);
    dragStartTrack = trackAtY (p.y);
    pruneSelection();

    if (rulerArea.contains (p.toInt()))
    {
        if (e.mods.isPopupMenu() || e.mods.isShiftDown())
        {
            drag = Drag::loop;
            loopAnchor = snap (dragStartTick, true);
            project.beginTransaction ("Set loop");
        }
        else
        {
            drag = Drag::ruler;
            ctx.engine.setSongMode (true);
            ctx.engine.setPositionTicks (juce::jmax (0.0, dragStartTick));
        }
        return;
    }

    if (headerArea.contains (p.toInt()))
    {
        const int index = trackAtY (p.y);
        auto track = project.tracksTree().getChild (index);
        if (! track.isValid())
            return;

        if (e.mods.isPopupMenu())
            showTrackMenu (index);
        else if (p.x < (float) headerArea.getX() + 30.0f)
        {
            project.beginTransaction ("Mute track");
            track.setProperty (id::mute, ! (bool) track[id::mute], project.um());
        }
        return;
    }

    if (! gridArea.contains (p.toInt()))
        return;

    bool rightEdge, leftEdge;
    auto hit = hitTest (p, rightEdge, leftEdge);

    if (e.mods.isPopupMenu())
    {
        if (hit.isValid() && e.mods.isShiftDown())
        {
            showClipMenu (hit);
            return;
        }
        project.beginTransaction ("Delete clips");
        drag = Drag::erase;
        if (hit.isValid())
            hit.getParent().removeChild (hit, project.um());
        return;
    }

    if (tool == Tool::slice && hit.isValid())
    {
        project.beginTransaction ("Slice clip");
        sliceAt (hit, snap (dragStartTick, true));
        return;
    }

    if (tool == Tool::mute && hit.isValid())
    {
        project.beginTransaction ("Mute clip");
        hit.setProperty (id::muted, ! (bool) hit[id::muted], project.um());
        return;
    }

    if (! hit.isValid() && (tool == Tool::select || e.mods.isCtrlDown()))
    {
        drag = Drag::marquee;
        if (! e.mods.isShiftDown())
            selection.clear();
        marquee = { p, p };
        repaint();
        return;
    }

    project.beginTransaction (hit.isValid() ? "Edit clips" : "Add clip");

    if (hit.isValid())
    {
        if (! selection.contains (hit))
        {
            if (! e.mods.isShiftDown())
                selection.clear();
            selection.add (hit);
        }

        if (e.mods.isShiftDown() && ! rightEdge && ! leftEdge)
        {
            juce::Array<juce::ValueTree> clones;
            for (auto& c : selection)
            {
                auto copy = c.createCopy();
                copy.setProperty (id::uid, project.allocateUid(), nullptr);
                c.getParent().appendChild (copy, project.um());
                clones.add (copy);
                if (c == hit)
                    hit = copy;
            }
            selection = clones;
        }

        anchor = hit;
        drag = rightEdge ? Drag::resizeRight : leftEdge ? Drag::resizeLeft : Drag::move;

        if (hit[id::kind].toString() == kind::pattern)
            project.setCurrentPattern (hit[id::pattern]);
    }
    else
    {
        auto pattern = project.currentPattern();
        auto track = project.tracksTree().getChild (dragStartTrack);
        if (! track.isValid() || ! pattern.isValid())
            return;

        auto clip = project.addClip (track, kind::pattern, pattern[id::uid], snap (dragStartTick), project.patternLength (pattern));
        selection.clear();
        selection.add (clip);
        anchor = clip;
        drag = Drag::move;
    }

    captureOriginals();
    repaint();
}

void Playlist::mouseDrag (const juce::MouseEvent& e)
{
    if (dragPan (e))
        return;

    const auto p = e.position;
    const double tick = xToTick (p.x);
    const bool free = e.mods.isAltDown();

    switch (drag)
    {
        case Drag::ruler:
            ctx.engine.setPositionTicks (juce::jmax (0.0, tick));
            repaint();
            break;

        case Drag::loop:
        {
            const auto other = snap (juce::jmax (0.0, tick), true);
            auto& state = project.getState();
            state.setProperty (id::loopStart, juce::jmin (loopAnchor, other), project.um());
            state.setProperty (id::loopEnd, juce::jmax (loopAnchor, other), project.um());
            break;
        }

        case Drag::erase:
        {
            bool r, l;
            auto hit = hitTest (p, r, l);
            if (hit.isValid())
            {
                selection.removeAllInstancesOf (hit);
                hit.getParent().removeChild (hit, project.um());
            }
            break;
        }

        case Drag::marquee:
        {
            marquee = juce::Rectangle<float> (dragStart, p);
            for (auto track : project.tracksTree())
                for (auto clip : track)
                    if (clipRect (clip).intersects (marquee) && ! selection.contains (clip))
                        selection.add (clip);
            repaint();
            break;
        }

        case Drag::move:
        {
            auto it = std::find_if (originals.begin(), originals.end(), [this] (const Original& o) { return o.clip == anchor; });
            if (it == originals.end())
                break;

            const double raw = (double) it->start + (tick - dragStartTick);
            const juce::int64 newStart = free ? (juce::int64) raw : snap (raw, true);
            juce::int64 delta = newStart - it->start;
            int trackDelta = trackAtY (p.y) - dragStartTrack;
            const int numTracks = project.tracksTree().getNumChildren();

            for (auto& o : originals)
            {
                delta = juce::jmax (delta, -o.start);
                trackDelta = juce::jlimit (-o.track, numTracks - 1 - o.track, trackDelta);
            }

            for (auto& o : originals)
            {
                o.clip.setProperty (id::start, o.start + delta, project.um());
                moveClipToTrack (o.clip, o.track + trackDelta);
            }
            break;
        }

        case Drag::resizeRight:
        case Drag::resizeLeft:
        {
            const double delta = tick - dragStartTick;
            const auto minLen = juce::jmax ((juce::int64) 1, free ? (juce::int64) 1 : snapTicks());

            for (auto& o : originals)
            {
                if (drag == Drag::resizeRight)
                {
                    const double rawEnd = (double) (o.start + o.length) + delta;
                    juce::int64 end = free ? (juce::int64) rawEnd : snap (rawEnd, true);
                    if (o.clip[id::kind].toString() == kind::audio)
                        end = juce::jmin (end, o.start + sourceLength (o.clip) - o.offset);
                    o.clip.setProperty (id::length, juce::jmax (minLen, end - o.start), project.um());
                }
                else
                {
                    const juce::int64 end = o.start + o.length;
                    const double rawStart = (double) o.start + delta;
                    juce::int64 start = free ? (juce::int64) rawStart : snap (rawStart, true);
                    start = juce::jlimit (juce::jmax ((juce::int64) 0, o.start - o.offset), end - minLen, start);
                    o.clip.setProperty (id::start, start, project.um());
                    o.clip.setProperty (id::length, end - start, project.um());
                    o.clip.setProperty (id::offset, juce::jmax ((juce::int64) 0, o.offset + (start - o.start)), project.um());
                }
            }
            break;
        }

        case Drag::none:
            break;
    }
}

void Playlist::mouseUp (const juce::MouseEvent& e)
{
    endPan (e);
    drag = Drag::none;
    originals.clear();
    repaint();
}

void Playlist::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto p = e.position;

    if (rulerArea.contains (p.toInt()))
    {
        project.beginTransaction ("Clear loop");
        project.getState().setProperty (id::loopStart, 0, project.um());
        project.getState().setProperty (id::loopEnd, 0, project.um());
        return;
    }

    if (headerArea.contains (p.toInt()))
    {
        auto track = project.tracksTree().getChild (trackAtY (p.y));
        askForText ("Rename track", "Name", track[id::name].toString(), [this, track] (const juce::String& text) mutable
        {
            if (text.trim().isNotEmpty())
            {
                project.beginTransaction ("Rename track");
                track.setProperty (id::name, text.trim(), project.um());
            }
        });
        return;
    }

    bool r, l;
    auto hit = hitTest (p, r, l);
    if (hit.isValid() && hit[id::kind].toString() == kind::pattern)
    {
        project.setCurrentPattern (hit[id::pattern]);
        if (ctx.openPianoRoll)
            ctx.openPianoRoll (project.selectedChannel());
    }
}

void Playlist::sliceAt (const juce::ValueTree& clip, juce::int64 tick)
{
    const juce::int64 start = clip[id::start], length = clip[id::length], offset = clip[id::offset];
    if (tick <= start || tick >= start + length)
        return;

    auto tail = clip.createCopy();
    tail.setProperty (id::uid, project.allocateUid(), nullptr);
    tail.setProperty (id::start, tick, nullptr);
    tail.setProperty (id::length, start + length - tick, nullptr);
    tail.setProperty (id::offset, offset + (tick - start), nullptr);

    auto c = clip;
    c.setProperty (id::length, tick - start, project.um());
    clip.getParent().appendChild (tail, project.um());
}

void Playlist::showTrackMenu (int index)
{
    auto track = project.tracksTree().getChild (index);
    juce::PopupMenu menu;
    menu.addItem ("Rename", [this, track]
    {
        askForText ("Rename track", "Name", track[id::name].toString(), [this, track] (const juce::String& text) mutable
        {
            if (text.trim().isNotEmpty())
            {
                project.beginTransaction ("Rename track");
                track.setProperty (id::name, text.trim(), project.um());
            }
        });
    });
    menu.addItem ("Insert track", [this, index]
    {
        juce::ValueTree t (id::TRACK);
        t.setProperty (id::uid, project.allocateUid(), nullptr);
        t.setProperty (id::name, "Track " + juce::String (project.tracksTree().getNumChildren() + 1), nullptr);
        project.beginTransaction ("Insert track");
        project.tracksTree().addChild (t, index, project.um());
    });
    menu.addItem ("Clear track", [this, track]
    {
        project.beginTransaction ("Clear track");
        auto t = track;
        t.removeAllChildren (project.um());
    });
    menu.addItem ("Delete track", project.tracksTree().getNumChildren() > 1, false, [this, track]
    {
        project.beginTransaction ("Delete track");
        project.tracksTree().removeChild (track, project.um());
    });
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

void Playlist::showClipMenu (juce::ValueTree clip)
{
    juce::PopupMenu menu;
    menu.addItem ("Mute", true, (bool) clip[id::muted], [this, clip] () mutable
    {
        project.beginTransaction ("Mute clip");
        clip.setProperty (id::muted, ! (bool) clip[id::muted], project.um());
    });
    if (clip[id::kind].toString() == kind::pattern)
        menu.addItem ("Make unique", [this, clip] () mutable
        {
            project.beginTransaction ("Make unique");
            auto copy = project.clonePattern (clip[id::pattern]);
            clip.setProperty (id::pattern, copy[id::uid], project.um());
        });
    menu.addItem ("Delete", [this, clip]
    {
        project.beginTransaction ("Delete clip");
        clip.getParent().removeChild (clip, project.um());
    });
    menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
}

void Playlist::editSelectAll()
{
    selection.clear();
    for (auto track : project.tracksTree())
        for (auto clip : track)
            selection.add (clip);
    repaint();
}

void Playlist::editCopy()
{
    pruneSelection();
    clipboard.clear();
    for (auto& c : selection)
    {
        auto copy = c.createCopy();
        copy.setProperty ("track", project.tracksTree().indexOf (c.getParent()), nullptr);
        clipboard.push_back (copy);
    }
}

void Playlist::editCut()
{
    editCopy();
    editDelete();
}

void Playlist::editPaste()
{
    if (clipboard.empty())
        return;

    juce::int64 first = std::numeric_limits<juce::int64>::max();
    for (auto& c : clipboard)
        first = juce::jmin (first, (juce::int64) c[id::start]);

    const auto at = snap (ctx.engine.isSongMode() ? ctx.engine.getDisplayPositionTicks() : viewTick.current, true);
    project.beginTransaction ("Paste clips");
    selection.clear();

    for (auto& c : clipboard)
    {
        auto track = project.tracksTree().getChild ((int) c["track"]);
        if (! track.isValid())
            continue;

        auto copy = c.createCopy();
        copy.removeProperty ("track", nullptr);
        copy.setProperty (id::uid, project.allocateUid(), nullptr);
        copy.setProperty (id::start, (juce::int64) c[id::start] - first + at, nullptr);
        track.appendChild (copy, project.um());
        selection.add (copy);
    }
}

void Playlist::editDelete()
{
    pruneSelection();
    if (selection.isEmpty())
        return;

    project.beginTransaction ("Delete clips");
    for (auto& c : selection)
        c.getParent().removeChild (c, project.um());
    selection.clear();
    repaint();
}

void Playlist::editDuplicate()
{
    pruneSelection();
    if (selection.isEmpty())
        return;

    juce::int64 first = std::numeric_limits<juce::int64>::max(), last = 0;
    for (auto& c : selection)
    {
        first = juce::jmin (first, (juce::int64) c[id::start]);
        last = juce::jmax (last, (juce::int64) c[id::start] + (juce::int64) c[id::length]);
    }

    const auto bar = project.ticksPerBar();
    const auto span = juce::jmax (bar, ((last - first + bar - 1) / bar) * bar);

    project.beginTransaction ("Duplicate clips");
    juce::Array<juce::ValueTree> copies;
    for (auto& c : selection)
    {
        auto copy = c.createCopy();
        copy.setProperty (id::uid, project.allocateUid(), nullptr);
        copy.setProperty (id::start, (juce::int64) c[id::start] + span, nullptr);
        c.getParent().appendChild (copy, project.um());
        copies.add (copy);
    }
    selection = copies;
}

void Playlist::editToggleMute()
{
    pruneSelection();
    if (selection.isEmpty())
        return;

    project.beginTransaction ("Mute clips");
    const bool newState = ! (bool) selection.getFirst()[id::muted];
    for (auto& c : selection)
        c.setProperty (id::muted, newState, project.um());
}

bool Playlist::keyPressed (const juce::KeyPress& key)
{
    const int code = key.getKeyCode();
    if (code == juce::KeyPress::escapeKey)
    {
        selection.clear();
        repaint();
        return true;
    }

    if (! key.getModifiers().isAnyModifierKeyDown())
    {
        if (code == 'P' || code == 'p') { setTool (Tool::draw); return true; }
        if (code == 'E' || code == 'e') { setTool (Tool::select); return true; }
        if (code == 'C' || code == 'c') { setTool (Tool::slice); return true; }
        if (code == 'T' || code == 't') { setTool (Tool::mute); return true; }
    }
    return false;
}

bool Playlist::isInterestedInDragSource (const SourceDetails& details)
{
    const auto d = details.description.toString();
    return d.startsWith ("file:") || d.startsWith ("plugin:");
}

void Playlist::itemDropped (const SourceDetails& details)
{
    const auto d = details.description.toString();
    const auto p = details.localPosition.toFloat();

    if (d.startsWith ("file:"))
    {
        const juce::File file (d.fromFirstOccurrenceOf ("file:", false, false));
        if (ctx.engine.getFormats().findFormatForFileExtension (file.getFileExtension()) != nullptr && ctx.importAudio)
            ctx.importAudio (file, trackAtY (p.y), snap (xToTick (p.x)));
        else if (ctx.addFileAsChannel)
            ctx.addFileAsChannel (file);
    }
    else if (d.startsWith ("plugin:") && ctx.addInstrument)
    {
        ctx.addInstrument (d.fromFirstOccurrenceOf ("plugin:", false, false));
    }
}

bool Playlist::isInterestedInFileDrag (const juce::StringArray&)
{
    return true;
}

void Playlist::filesDropped (const juce::StringArray& files, int x, int y)
{
    int track = trackAtY ((float) y);
    for (auto& f : files)
    {
        const juce::File file (f);
        if (ctx.engine.getFormats().findFormatForFileExtension (file.getFileExtension()) != nullptr && ctx.importAudio)
            ctx.importAudio (file, track++, snap (xToTick (x)));
        else if (ctx.addFileAsChannel)
            ctx.addFileAsChannel (file);
    }
}

void Playlist::valueTreePropertyChanged (juce::ValueTree& t, const juce::Identifier& p)
{
    if (t.hasType (id::PROJECT) && p == id::currentPattern)
        refreshToolbar();
    if (t.hasType (id::PATTERN) && (p == id::name || p == id::colour))
        refreshToolbar();
    repaint();
}

void Playlist::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&)
{
    repaint();
}

void Playlist::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)
{
    pruneSelection();
    peakCache.clear();
    repaint();
}
}
