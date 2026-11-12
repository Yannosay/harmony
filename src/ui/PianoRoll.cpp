#include "ui/PianoRoll.h"
#include "ui/Dialogs.h"
#include "model/MusicTheory.h"
#include "engine/AudioEngine.h"
#include "app/Preferences.h"
#include "ui/SystemClipboard.h"
#include "model/MidiIO.h"

namespace harmony::ui
{
std::vector<NoteData> PianoRoll::clipboard;
juce::uint32 PianoRoll::clipboardSequence = 0;

namespace
{
    constexpr int toolbarHeight = 40;
    constexpr int rulerHeight = 24;
    constexpr int keysWidth = 72;
    constexpr int velocityHeight = 72;
    constexpr float edgeGrab = 6.0f;
    constexpr float stemGrab = 10.0f;
    constexpr float stemReach = 28.0f;

    bool isBlackKey (int key)
    {
        const int pc = key % 12;
        return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
    }

    juce::Colour channelColour (const juce::ValueTree& channel)
    {
        return channel.isValid() ? juce::Colour::fromString (channel[id::colour].toString()) : palette::notePink;
    }
}

PianoRoll::PianoRoll (Context& c)
    : TimelineCanvas (c.project), ctx (c)
{
    rowHeight = { 16.0, 16.0 };
    pxPerTick = { 0.09, 0.09 };
    scrollY = { (127.0 - 84.0) * 16.0, (127.0 - 84.0) * 16.0 };
    snapIndex = 0;

    for (auto* b : std::initializer_list<juce::Button*> { &drawButton, &selectButton, &sliceButton, &muteButton, &ghostButton, &scaleLockButton,
                                                       &quantizeButton, &humanizeButton, &chordButton, &followButton, &dragOutButton, &maximiseButton })
        addAndMakeVisible (b);

    drawButton.onClick = [this] { setTool (Tool::draw); };
    selectButton.onClick = [this] { setTool (Tool::select); };
    sliceButton.onClick = [this] { setTool (Tool::slice); };
    muteButton.onClick = [this] { setTool (Tool::mute); };
    ghostButton.onClick = [this] { ghosts = ! ghosts; refreshToolbar(); repaint(); };
    scaleLockButton.onClick = [this] { scaleLock = ! scaleLock; refreshToolbar(); };
    quantizeButton.onClick = [this] { editQuantize(); };
    humanizeButton.onClick = [this] { editHumanize(); };
    chordButton.onClick = [this] { showChordGenerator(); };
    followButton.onClick = [this] { showFollowGenerator(); };
    dragOutButton.onDragOut = [this] { dragMidiOut(); };
    dragOutButton.onClick = [this] { exportMidiFile(); };
    maximiseButton.onClick = [this] { if (ctx.toggleEditorMaximised) ctx.toggleEditorMaximised(); };

    addAndMakeVisible (channelButton);
    channelButton.onClick = [this] { showChannelMenu(); };
    channelButton.setTooltip ("Channel being edited");

    for (auto& s : snapOptions())
        snapBox.addItem (s.name, snapBox.getNumItems() + 1);
    snapBox.setSelectedItemIndex (0, juce::dontSendNotification);
    snapBox.setTooltip ("Snap");
    snapBox.onChange = [this] { setSnapIndex (snapBox.getSelectedItemIndex()); };
    addAndMakeVisible (snapBox);

    for (int i = 0; i < 12; ++i)
        rootBox.addItem (theory::pitchClassName (i), i + 1);
    rootBox.setTooltip ("Key");
    rootBox.onChange = [this] { project.getState().setProperty (id::scaleRoot, rootBox.getSelectedItemIndex(), nullptr); repaint(); };
    addAndMakeVisible (rootBox);

    for (auto& s : theory::allScales())
        scaleBox.addItem (s.name, scaleBox.getNumItems() + 1);
    scaleBox.setTooltip ("Scale highlighting");
    scaleBox.onChange = [this] { project.getState().setProperty (id::scaleType, scaleBox.getText(), nullptr); repaint(); };
    addAndMakeVisible (scaleBox);

    keepFocusOnEditors ({ &channelButton, &snapBox, &rootBox, &scaleBox });
    project.getState().addListener (this);
    channelUid = project.selectedChannel();
    ghosts = prefs::ghostNotes.get (ctx.settings);
    snapIndex = prefs::pianoRollSnap.get (ctx.settings);
    refreshToolbar();
}

void PianoRoll::setGhostsVisible (bool shouldShow)
{
    ghosts = shouldShow;
    refreshToolbar();
    repaint();
}

PianoRoll::~PianoRoll()
{
    stopPreview();
    project.getState().removeListener (this);
}

void PianoRoll::setChannel (int uid)
{
    if (uid == channelUid)
        return;

    channelUid = uid;
    selection.clear();

    juce::int64 lowest = 128, highest = -1;
    for (auto& n : channelNotes())
    {
        lowest = juce::jmin (lowest, (juce::int64) (int) n[id::key]);
        highest = juce::jmax (highest, (juce::int64) (int) n[id::key]);
    }

    if (highest >= 0)
        scrollToY ((127.0 - (double) (lowest + highest) * 0.5) * rowHeight.target - gridArea.getHeight() * 0.5);

    refreshToolbar();
    repaint();
}

juce::ValueTree PianoRoll::pattern() const
{
    return project.currentPattern();
}

std::vector<juce::ValueTree> PianoRoll::channelNotes() const
{
    std::vector<juce::ValueTree> notes;
    for (auto n : pattern())
        if ((int) n[id::channel] == channelUid)
            notes.push_back (n);
    return notes;
}

double PianoRoll::getContentLengthTicks() const
{
    return (double) project.patternLength (pattern()) + (double) project.ticksPerBar() * 8.0;
}

float PianoRoll::keyToY (int key) const
{
    return (float) (gridArea.getY() - scrollY.current + (127 - key) * rowHeight.current);
}

int PianoRoll::yToKey (float y) const
{
    return juce::jlimit (0, 127, 127 - (int) std::floor (((double) y - gridArea.getY() + scrollY.current) / rowHeight.current));
}

int PianoRoll::constrainKey (int key) const
{
    if (! scaleLock)
        return key;

    const auto& scale = theory::findScale (project.getState()[id::scaleType].toString());
    return theory::snapToScale (key, project.getState()[id::scaleRoot], scale);
}

juce::Rectangle<float> PianoRoll::noteRect (const NoteData& n) const
{
    const float x1 = (float) tickToX ((double) n.start);
    const float x2 = (float) tickToX ((double) (n.start + n.length));
    return { x1, keyToY (n.key), juce::jmax (3.0f, x2 - x1), (float) rowHeight.current };
}

juce::ValueTree PianoRoll::hitTest (juce::Point<float> p, bool& rightEdge, bool& leftEdge) const
{
    rightEdge = leftEdge = false;
    auto notes = channelNotes();

    for (auto it = notes.rbegin(); it != notes.rend(); ++it)
    {
        const auto r = noteRect (Project::readNote (*it));
        if (r.contains (p))
        {
            const float grab = juce::jmin (edgeGrab, r.getWidth() * 0.3f);
            rightEdge = p.x >= r.getRight() - grab;
            leftEdge = ! rightEdge && p.x <= r.getX() + grab * 0.6f;
            return *it;
        }
    }
    return {};
}

void PianoRoll::pruneSelection()
{
    for (int i = selection.size(); --i >= 0;)
        if (! selection.getReference (i).getParent().isValid())
            selection.remove (i);
}

void PianoRoll::captureOriginals()
{
    originals.clear();
    for (auto& t : selection)
        originals.push_back ({ t, Project::readNote (t) });
}

void PianoRoll::preview (int key)
{
    if (key == previewKey)
        return;

    stopPreview();

    const bool wanted = drag == Drag::keyboard
                     || (prefs::previewNotes.get (ctx.settings) && (! ctx.engine.isPlaying() || prefs::previewWhilePlaying.get (ctx.settings)));
    if (! wanted)
        return;

    previewKey = key;
    ctx.engine.sendPreviewNote (channelUid, key, lastVelocity, true);
}

void PianoRoll::stopPreview()
{
    if (previewKey >= 0)
        ctx.engine.sendPreviewNote (channelUid, previewKey, 0.0f, false);
    previewKey = -1;
}

bool PianoRoll::hasSelection() const
{
    for (auto& t : selection)
        if (t.getParent().isValid())
            return true;
    return false;
}

void PianoRoll::setTool (Tool t)
{
    tool = t;
    refreshToolbar();
}

void PianoRoll::refreshToolbar()
{
    drawButton.setToggleState (tool == Tool::draw, juce::dontSendNotification);
    selectButton.setToggleState (tool == Tool::select, juce::dontSendNotification);
    sliceButton.setToggleState (tool == Tool::slice, juce::dontSendNotification);
    muteButton.setToggleState (tool == Tool::mute, juce::dontSendNotification);
    ghostButton.setToggleState (ghosts, juce::dontSendNotification);
    scaleLockButton.setToggleState (scaleLock, juce::dontSendNotification);

    auto channel = project.findChannel (channelUid);
    channelButton.setButtonText (channel.isValid() ? channel[id::name].toString() : juce::String ("No channel"));
    channelButton.setColour (juce::TextButton::buttonColourId, channel.isValid() ? channelColour (channel).withAlpha (0.18f) : colours::control);

    rootBox.setSelectedItemIndex (juce::jlimit (0, 11, (int) project.getState()[id::scaleRoot]), juce::dontSendNotification);
    scaleBox.setText (theory::findScale (project.getState()[id::scaleType].toString()).name, juce::dontSendNotification);
    snapBox.setSelectedItemIndex (snapIndex, juce::dontSendNotification);
}

void PianoRoll::showChannelMenu()
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Edit channel");
    for (auto ch : project.channelsTree())
    {
        const int uid = ch[id::uid];
        menu.addItem (ch[id::name].toString(), true, uid == channelUid, [this, uid]
        {
            project.setSelectedChannel (uid);
            setChannel (uid);
        });
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (channelButton));
}

void PianoRoll::resized()
{
    auto r = getLocalBounds();
    toolbarArea = r.removeFromTop (toolbarHeight);
    rulerArea = r.removeFromTop (rulerHeight).withTrimmedLeft (keysWidth);
    velocityArea = r.removeFromBottom (velocityHeight).withTrimmedLeft (keysWidth);
    keysArea = r.removeFromLeft (keysWidth);
    gridArea = r;

    auto bar = toolbarArea.reduced (8, 6);
    channelButton.setBounds (bar.removeFromLeft (170));
    bar.removeFromLeft (10);

    for (auto* b : { &drawButton, &selectButton, &sliceButton, &muteButton })
    {
        b->setBounds (bar.removeFromLeft (28));
        bar.removeFromLeft (2);
    }

    bar.removeFromLeft (10);
    snapBox.setBounds (bar.removeFromLeft (104));
    bar.removeFromLeft (10);
    rootBox.setBounds (bar.removeFromLeft (64));
    bar.removeFromLeft (4);
    scaleBox.setBounds (bar.removeFromLeft (150));
    bar.removeFromLeft (4);
    scaleLockButton.setBounds (bar.removeFromLeft (28));
    bar.removeFromLeft (10);

    for (auto* b : { &ghostButton, &quantizeButton, &humanizeButton, &chordButton, &followButton })
    {
        b->setBounds (bar.removeFromLeft (28));
        bar.removeFromLeft (2);
    }

    auto right = toolbarArea.reduced (8, 6);
    maximiseButton.setBounds (right.removeFromRight (28));
    right.removeFromRight (2);
    dragOutButton.setBounds (right.removeFromRight (28));
}

void PianoRoll::refreshMaximiseButton()
{
    maximiseButton.setToggleState (ctx.isEditorMaximised && ctx.isEditorMaximised(), juce::dontSendNotification);
}

void PianoRoll::dragMidiOut()
{
    auto channel = project.findChannel (channelUid);
    if (! channel.isValid())
        return;

    const auto name = juce::File::createLegalFileName (pattern()[id::name].toString() + " - " + channel[id::name].toString());
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("Harmony MIDI").getNonexistentChildFile (name, ".mid", false);
    file.getParentDirectory().createDirectory();

    if (! midi::writeFile (midi::exportPattern (project, pattern(), channelUid), file))
    {
        if (ctx.notify)
            ctx.notify ("Could not write the MIDI file", true);
        return;
    }

    juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, &dragOutButton);
}

void PianoRoll::exportMidiFile()
{
    auto channel = project.findChannel (channelUid);
    if (! channel.isValid())
        return;

    const auto name = juce::File::createLegalFileName (pattern()[id::name].toString() + " - " + channel[id::name].toString());
    chooser = std::make_unique<juce::FileChooser> ("Export notes as MIDI", prefs::projectFolder (ctx.settings).getChildFile (name + ".mid"), "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
    {
        const auto target = fc.getResult().withFileExtension (".mid");
        if (fc.getResult() == juce::File())
            return;

        const bool ok = midi::writeFile (midi::exportPattern (project, pattern(), channelUid), target);
        if (ctx.notify)
            ctx.notify (ok ? "Exported " + target.getFileName() : "Could not write " + target.getFileName(), ! ok);
    });
}

void PianoRoll::pasteNotesAt (const std::vector<NoteData>& notes, juce::int64 at, const juce::String& undoName)
{
    if (notes.empty() || ! project.findChannel (channelUid).isValid())
        return;

    juce::int64 first = std::numeric_limits<juce::int64>::max();
    for (auto& n : notes)
        first = juce::jmin (first, n.start);

    const auto beat = project.ticksPerBeat();
    const auto aligned = (first / beat) * beat;

    project.beginTransaction (undoName);
    selection.clear();
    for (auto n : notes)
    {
        n.channel = channelUid;
        n.start = n.start - aligned + at;
        selection.add (project.addNote (pattern(), n));
    }
    updateChordLabel();
    repaint();
}

void PianoRoll::importMidiFile (const juce::File& file, juce::int64 at)
{
    auto midiFile = midi::readFile (file);
    const auto notes = midiFile ? midi::flatten (midi::importTracks (*midiFile)) : std::vector<NoteData>();
    if (notes.empty())
    {
        if (ctx.notify)
            ctx.notify (file.getFileName() + " contains no notes", true);
        return;
    }

    pasteNotesAt (notes, at, "Import MIDI");
    if (ctx.notify)
        ctx.notify ("Added " + juce::String ((int) notes.size()) + " notes from " + file.getFileName(), false);
}

bool PianoRoll::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files)
        if (midi::isMidiFile (juce::File (f)))
            return true;
    return false;
}

void PianoRoll::filesDropped (const juce::StringArray& files, int x, int)
{
    for (auto& f : files)
        if (midi::isMidiFile (juce::File (f)))
            importMidiFile (juce::File (f), snap (juce::jmax (0.0, xToTick (x))));
}

bool PianoRoll::isInterestedInDragSource (const SourceDetails& details)
{
    const auto d = details.description.toString();
    return d.startsWith ("file:") && midi::isMidiFile (juce::File (d.fromFirstOccurrenceOf ("file:", false, false)));
}

void PianoRoll::itemDropped (const SourceDetails& details)
{
    importMidiFile (juce::File (details.description.toString().fromFirstOccurrenceOf ("file:", false, false)),
                    snap (juce::jmax (0.0, xToTick (details.localPosition.x))));
}

void PianoRoll::drawRows (juce::Graphics& g) const
{
    const auto& scale = theory::findScale (project.getState()[id::scaleType].toString());
    const int root = project.getState()[id::scaleRoot];
    const int top = yToKey ((float) gridArea.getY());
    const int bottom = yToKey ((float) gridArea.getBottom());

    for (int key = bottom; key <= top; ++key)
    {
        const float y = keyToY (key);
        juce::Colour c = isBlackKey (key) ? juce::Colour (0xff0d0d0f) : juce::Colour (0xff121215);
        if (! theory::isInScale (key, root, scale))
            c = c.darker (0.35f);
        if (key == hoverKey)
            c = c.brighter (0.08f);

        g.setColour (c);
        g.fillRect ((float) gridArea.getX(), y, (float) gridArea.getWidth(), (float) rowHeight.current);

        if (key % 12 == 0)
        {
            g.setColour (colours::gridBar);
            g.fillRect ((float) gridArea.getX(), y + (float) rowHeight.current - 1.0f, (float) gridArea.getWidth(), 1.0f);
        }
    }

    drawTimeGrid (g, gridArea);

    const auto patternEnd = (float) tickToX ((double) project.patternLength (pattern()));
    if (patternEnd < (float) gridArea.getRight())
    {
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.fillRect (juce::Rectangle<float> (patternEnd, (float) gridArea.getY(), (float) gridArea.getRight() - patternEnd, (float) gridArea.getHeight()));
    }
}

void PianoRoll::drawNotes (juce::Graphics& g) const
{
    const double visibleStart = xToTick (gridArea.getX());
    const double visibleEnd = xToTick (gridArea.getRight());

    if (ghosts)
    {
        for (auto n : pattern())
        {
            if ((int) n[id::channel] == channelUid)
                continue;

            const auto note = Project::readNote (n);
            if ((double) (note.start + note.length) < visibleStart || (double) note.start > visibleEnd)
                continue;

            const auto r = noteRect (note).reduced (0.5f, 1.5f);
            const auto ghostColour = channelColour (project.findChannel (note.channel));
            g.setColour (ghostColour.withAlpha (0.07f));
            g.fillRoundedRectangle (r, 2.5f);
            g.setColour (ghostColour.withAlpha (0.32f));
            const float dashes[] = { 3.0f, 2.0f };
            juce::Path outline;
            outline.addRoundedRectangle (r, 2.5f);
            juce::Path dashed;
            juce::PathStrokeType (1.0f).createDashedStroke (dashed, outline, dashes, 2);
            g.fillPath (dashed);
        }
    }

    const auto base = channelColour (project.findChannel (channelUid));
    const bool showNames = rowHeight.current >= 12.0 && prefs::noteNames.get (ctx.settings);
    g.setFont (font (juce::jmin (10.0f, (float) rowHeight.current - 5.0f), Weight::medium));

    for (auto& n : channelNotes())
    {
        const auto note = Project::readNote (n);
        if ((double) (note.start + note.length) < visibleStart || (double) note.start > visibleEnd)
            continue;

        const auto r = noteRect (note).reduced (0.0f, 1.0f);
        const bool selected = selection.contains (n);
        auto fill = base.withMultipliedSaturation (0.55f + 0.45f * note.velocity).withMultipliedBrightness (0.72f + 0.28f * note.velocity);

        if (note.muted)
            fill = colours::control.brighter (0.15f);

        g.setColour (fill);
        g.fillRoundedRectangle (r, 3.0f);

        g.setColour (selected ? colours::text : fill.darker (0.4f));
        g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, selected ? 1.6f : 1.0f);

        const float velWidth = juce::jmax (0.0f, (r.getWidth() - 4.0f) * note.velocity);
        g.setColour (juce::Colours::white.withAlpha (note.muted ? 0.1f : 0.28f));
        g.fillRect (r.getX() + 2.0f, r.getY() + 2.0f, velWidth, 1.5f);

        if (showNames && r.getWidth() > 26.0f)
        {
            g.setColour (note.muted ? colours::textDim : colours::background.withAlpha (0.85f));
            g.drawText (theory::noteName (note.key), r.reduced (4.0f, 0.0f), juce::Justification::centredLeft, false);
        }
    }

    if (drag == Drag::marquee)
    {
        g.setColour (colours::accent.withAlpha (0.12f));
        g.fillRect (marquee);
        g.setColour (colours::accent.withAlpha (0.8f));
        g.drawRect (marquee, 1.0f);
    }
}

void PianoRoll::drawKeyboard (juce::Graphics& g) const
{
    g.setColour (colours::panel);
    g.fillRect (keysArea);

    const int top = yToKey ((float) keysArea.getY());
    const int bottom = yToKey ((float) keysArea.getBottom());
    const float rh = (float) rowHeight.current;

    for (int key = bottom; key <= top; ++key)
    {
        const float y = keyToY (key);
        juce::Rectangle<float> r ((float) keysArea.getX(), y, (float) keysArea.getWidth() - 1.0f, rh);

        if (isBlackKey (key))
        {
            g.setColour (key == previewKey ? colours::accentDeep : juce::Colour (0xff1a1a1e));
            g.fillRect (r.withWidth (r.getWidth() * 0.62f).reduced (0.0f, 0.5f));
        }
        else
        {
            g.setColour (key == previewKey ? colours::accent : juce::Colour (0xffe6e6ea));
            g.fillRect (r.reduced (0.0f, 0.5f));

            if (key % 12 == 0 && rh >= 9.0f)
            {
                g.setColour (colours::background.withAlpha (0.7f));
                g.setFont (font (juce::jmin (9.5f, rh - 3.0f), Weight::medium));
                g.drawText (theory::noteName (key), r.withTrimmedRight (6.0f), juce::Justification::centredRight, false);
            }
        }

        if (isBlackKey (key))
        {
            g.setColour (juce::Colour (0xffe6e6ea));
            g.fillRect (r.withLeft (r.getX() + r.getWidth() * 0.62f).reduced (0.0f, 0.5f));
        }
    }

    g.setColour (colours::line);
    g.fillRect (keysArea.getRight() - 1, keysArea.getY(), 1, keysArea.getHeight());
}

void PianoRoll::drawVelocityLane (juce::Graphics& g) const
{
    g.setColour (colours::panel);
    g.fillRect (velocityArea.withLeft (0));
    g.setColour (colours::line);
    g.fillRect (0, velocityArea.getY(), getWidth(), 1);

    g.setColour (colours::textFaint);
    g.setFont (font (9.0f, Weight::medium));
    g.drawText ("VELOCITY", juce::Rectangle<int> (0, velocityArea.getY(), keysWidth, 24), juce::Justification::centred, false);

    const auto lane = velocityArea.reduced (0, 8).toFloat();
    const auto base = channelColour (project.findChannel (channelUid));

    for (auto& n : channelNotes())
    {
        const auto note = Project::readNote (n);
        const float x = (float) tickToX ((double) note.start);
        if (x < lane.getX() - 4.0f || x > lane.getRight())
            continue;

        const float h = lane.getHeight() * note.velocity;
        const bool selected = selection.contains (n);
        const bool hovered = n == hoverStem || std::find (velocityTargets.begin(), velocityTargets.end(), n) != velocityTargets.end();
        auto colour = (note.muted ? colours::textFaint : base).withAlpha (selected || hovered ? 1.0f : 0.7f);
        if (hovered)
            colour = colour.brighter (0.3f);

        g.setColour (colour);
        g.fillRect (x, lane.getBottom() - h, hovered ? 3.0f : 2.0f, h);
        const float knob = hovered ? 10.0f : 7.0f;
        g.fillEllipse (x + 1.0f - knob * 0.5f, lane.getBottom() - h - knob * 0.5f, knob, knob);

        if (hovered)
        {
            g.setColour (colours::text);
            g.setFont (font (9.0f, Weight::medium));
            g.drawText (juce::String (juce::roundToInt (note.velocity * 100.0f)) + "%",
                        juce::Rectangle<float> (x + 8.0f, juce::jmax (lane.getY(), lane.getBottom() - h - 8.0f), 40.0f, 12.0f),
                        juce::Justification::centredLeft, false);
        }
    }
}

void PianoRoll::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (gridArea);
        drawRows (g);
        drawNotes (g);
    }

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (keysArea);
        drawKeyboard (g);
    }

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (velocityArea.withLeft (0));
        drawVelocityLane (g);
    }

    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (rulerArea);
        drawRuler (g, rulerArea, 0, 0);
    }

    g.setColour (colours::panel);
    g.fillRect (toolbarArea);
    g.fillRect (0, rulerArea.getY(), keysWidth, rulerHeight);
    g.setColour (colours::line);
    g.fillRect (0, toolbarArea.getBottom() - 1, getWidth(), 1);

    if (chordText.isNotEmpty())
    {
        g.setColour (colours::accent);
        g.setFont (font (12.0f, Weight::semibold));
        g.drawText (chordText, toolbarArea.withTrimmedRight (14), juce::Justification::centredRight, false);
    }

    if (const double tick = playheadTick(); tick >= 0.0)
    {
        juce::Graphics::ScopedSaveState s (g);
        g.reduceClipRegion (gridArea.getUnion (rulerArea));
        drawPlayhead (g, rulerArea.getUnion (gridArea), tick);
    }

    if (! project.findChannel (channelUid).isValid())
    {
        g.setColour (colours::textDim);
        g.setFont (font (13.0f, Weight::medium));
        g.drawText ("Add a channel in the Channel Rack to start writing notes", gridArea, juce::Justification::centred, false);
    }
}

double PianoRoll::playheadTick() const
{
    const double tick = ctx.engine.getDisplayPositionTicks();
    if (! ctx.engine.isSongMode())
        return tick;

    if (auto local = project.patternPositionAt (pattern()[id::uid], tick))
        return *local;
    return -1.0;
}

double PianoRoll::rulerTickToTransport (double patternTick) const
{
    if (! ctx.engine.isSongMode())
        return patternTick;

    const int uid = pattern()[id::uid];
    const double now = ctx.engine.getDisplayPositionTicks();
    auto clip = project.findPatternClipAt (uid, now);

    if (! clip.isValid())
    {
        double best = std::numeric_limits<double>::max();
        for (auto track : project.tracksTree())
            for (auto c : track)
                if (c[id::kind].toString() == kind::pattern && (int) c[id::pattern] == uid && (double) (juce::int64) c[id::start] < best)
                {
                    best = (double) (juce::int64) c[id::start];
                    clip = c;
                }
    }

    if (! clip.isValid())
    {
        ctx.engine.setSongMode (false);
        return patternTick;
    }

    const double start = (double) (juce::int64) clip[id::start];
    const double length = (double) (juce::int64) clip[id::length];
    const double offset = (double) (juce::int64) clip[id::offset];
    const double patternLength = (double) juce::jmax ((juce::int64) 1, project.patternLength (pattern()));
    const double clipLocal = now >= start && now < start + length ? now - start + offset : offset;
    const double cycleStart = std::floor (clipLocal / patternLength) * patternLength;
    return juce::jlimit (start, start + length - 1.0, start + cycleStart + patternTick - offset);
}

void PianoRoll::animationTick()
{
    const double tick = ctx.engine.isPlaying() ? playheadTick() : -1.0;
    if (tick < 0.0)
    {
        if (lastPlayheadX >= 0.0f)
        {
            lastPlayheadX = -1.0f;
            repaint();
        }
        return;
    }

    followPlayhead (tick);
    const float x = (float) tickToX (tick);
    if (std::abs (x - lastPlayheadX) >= 0.5f)
    {
        const int top = rulerArea.getY();
        const int h = gridArea.getBottom() - top;
        repaint (juce::Rectangle<int> ((int) lastPlayheadX - 12, top, 25, h));
        repaint (juce::Rectangle<int> ((int) x - 12, top, 25, h));
        lastPlayheadX = x;
        updateChordLabel();
    }
}

void PianoRoll::updateChordLabel()
{
    const double tick = ctx.engine.isPlaying() ? playheadTick() : -1.0;
    std::vector<int> keys;

    if (tick >= 0.0)
    {
        for (auto& n : channelNotes())
        {
            const auto note = Project::readNote (n);
            if (! note.muted && (double) note.start <= tick && (double) (note.start + note.length) > tick)
                keys.push_back (note.key);
        }
    }
    else
    {
        for (auto& n : selection)
            keys.push_back (Project::readNote (n).key);
    }

    std::sort (keys.begin(), keys.end());
    keys.erase (std::unique (keys.begin(), keys.end()), keys.end());
    const auto text = keys.size() >= 2 ? theory::detectChord (keys) : juce::String();

    if (text != chordText)
    {
        chordText = text;
        repaint (toolbarArea);
    }
}

void PianoRoll::mouseMove (const juce::MouseEvent& e)
{
    const auto p = e.position;
    int newHover = -1;

    if (gridArea.contains (p.toInt()))
    {
        bool right, left;
        auto hit = hitTest (p, right, left);
        setMouseCursor (hit.isValid() && (right || left) ? juce::MouseCursor::LeftRightResizeCursor
                        : tool == Tool::slice ? juce::MouseCursor::IBeamCursor
                        : hit.isValid() ? juce::MouseCursor::DraggingHandCursor
                                        : juce::MouseCursor::NormalCursor);
        newHover = yToKey (p.y);

        if (ctx.setHint)
            ctx.setHint (theory::noteName (newHover) + "   Click to draw, right-click to delete, Ctrl+drag to select, Shift+drag to clone, Alt+scroll on a note for velocity, double-click a ghost note to edit its channel");
    }
    else
    {
        setMouseCursor (keysArea.contains (p.toInt()) || velocityArea.contains (p.toInt()) ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }

    juce::ValueTree stem;
    if (velocityArea.contains (p.toInt()))
    {
        const auto targets = velocityTargetsAt (p.x);
        if (! targets.empty())
            stem = targets.front();
        if (ctx.setHint)
            ctx.setHint ("Drag a stem to set its velocity (chords move together), Ctrl+drag to paint across notes, Alt+scroll to fine tune");
    }

    if (stem != hoverStem)
    {
        hoverStem = stem;
        repaint (velocityArea);
    }

    if (newHover != hoverKey)
    {
        hoverKey = newHover;
        repaint (gridArea);
    }
}

void PianoRoll::mouseExit (const juce::MouseEvent&)
{
    if (hoverKey != -1 || hoverStem.isValid())
    {
        hoverKey = -1;
        hoverStem = {};
        repaint();
    }
}

void PianoRoll::mouseDown (const juce::MouseEvent& e)
{
    grabKeyboardFocus();
    if (beginPan (e))
        return;

    const auto p = e.position;
    dragStart = p;
    dragStartTick = xToTick (p.x);
    dragStartKey = yToKey (p.y);
    pruneSelection();

    if (! project.findChannel (channelUid).isValid())
        return;

    if (rulerArea.contains (p.toInt()))
    {
        drag = Drag::ruler;
        const bool snapHead = prefs::snapPlayhead.get (ctx.settings) && ! e.mods.isAltDown();
        const double tick = juce::jmax (0.0, snapHead ? (double) snap (dragStartTick, true) : dragStartTick);
        ctx.engine.jumpTo (rulerTickToTransport (tick));
        repaint();
        return;
    }

    if (keysArea.contains (p.toInt()))
    {
        drag = Drag::keyboard;
        preview (dragStartKey);
        repaint (keysArea);
        return;
    }

    if (velocityArea.contains (p.toInt()))
    {
        drag = Drag::velocity;
        project.beginTransaction ("Change velocity");
        velocityTargets.clear();
        velocityOriginals.clear();

        if (! e.mods.isCtrlDown())
        {
            auto targets = velocityTargetsAt (p.x);
            const bool fromSelection = ! targets.empty() && selection.size() > 1 && selection.contains (targets.front());
            if (fromSelection)
            {
                targets.clear();
                for (auto& t : selection)
                    targets.push_back (t);
            }

            velocityTargets = targets;
            for (auto& t : velocityTargets)
                velocityOriginals.push_back ((float) t.getProperty (id::velocity, 0.78f));
            velocityGrabValue = fromSelection ? (float) velocityTargetsAt (p.x).front().getProperty (id::velocity, 0.78f) : -1.0f;
        }

        applyVelocity (p, e.mods.isCtrlDown());
        repaint (velocityArea);
        return;
    }

    if (! gridArea.contains (p.toInt()))
        return;

    bool rightEdge, leftEdge;
    auto hit = hitTest (p, rightEdge, leftEdge);

    if (e.getNumberOfClicks() >= 2 && ! e.mods.isPopupMenu() && (! hit.isValid() || hit == lastAddedNote))
    {
        if (auto ghost = ghostAt (p); ghost.isValid())
        {
            if (hit.isValid() && hit == lastAddedNote)
            {
                selection.removeAllInstancesOf (hit);
                pattern().removeChild (hit, project.um());
            }

            lastAddedNote = {};
            const int uid = ghost[id::channel];
            project.setSelectedChannel (uid);
            setChannel (uid);
            if (ctx.notify)
                ctx.notify ("Now editing " + project.findChannel (uid)[id::name].toString(), false);
            return;
        }
    }

    lastAddedNote = {};

    if (e.mods.isPopupMenu())
    {
        project.beginTransaction ("Delete notes");
        drag = Drag::erase;
        eraseAt (p);
        return;
    }

    if (tool == Tool::slice)
    {
        project.beginTransaction ("Slice notes");
        sliceAt (dragStartTick, hit);
        return;
    }

    if (tool == Tool::mute && hit.isValid())
    {
        project.beginTransaction ("Mute note");
        auto targets = selection.contains (hit) ? selection : juce::Array<juce::ValueTree> { hit };
        const bool newState = ! (bool) hit[id::muted];
        for (auto& t : targets)
            t.setProperty (id::muted, newState, project.um());
        return;
    }

    if (! hit.isValid() && (tool == Tool::select || e.mods.isCtrlDown()))
    {
        drag = Drag::marquee;
        marqueeBase = e.mods.isShiftDown() ? selection : juce::Array<juce::ValueTree>();
        if (! e.mods.isShiftDown())
            selection.clear();
        marquee = { p, p };
        repaint();
        return;
    }

    project.beginTransaction (hit.isValid() ? "Edit notes" : "Add note");

    if (hit.isValid())
    {
        if (! selection.contains (hit))
        {
            if (! e.mods.isShiftDown() && ! e.mods.isCtrlDown())
                selection.clear();
            selection.add (hit);
        }

        if (e.mods.isShiftDown() && ! rightEdge && ! leftEdge)
        {
            juce::Array<juce::ValueTree> clones;
            for (auto& t : selection)
            {
                auto copy = Project::readNote (t);
                auto added = project.addNote (pattern(), copy);
                clones.add (added);
                if (t == hit)
                    hit = added;
            }
            selection = clones;
        }

        anchor = hit;
        drag = rightEdge ? Drag::resizeRight : leftEdge ? Drag::resizeLeft : Drag::move;
        lastVelocity = (float) hit.getProperty (id::velocity, 0.78f);
        preview ((int) hit[id::key]);
    }
    else
    {
        NoteData n;
        n.channel = channelUid;
        n.key = constrainKey (dragStartKey);
        n.start = e.mods.isAltDown() ? (juce::int64) dragStartTick : snap (dragStartTick);
        n.length = lastLength;
        n.velocity = lastVelocity;

        auto added = project.addNote (pattern(), n);
        selection.clear();
        selection.add (added);
        anchor = added;
        lastAddedNote = added;
        drag = Drag::move;
        preview (n.key);
    }

    captureOriginals();
    updateChordLabel();
    repaint();
}

void PianoRoll::mouseDrag (const juce::MouseEvent& e)
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

        case Drag::keyboard:
            preview (yToKey (p.y));
            repaint (keysArea);
            break;

        case Drag::velocity:
            applyVelocity (p, e.mods.isCtrlDown() && velocityTargets.empty());
            repaint (velocityArea);
            break;

        case Drag::erase:
            eraseAt (p);
            break;

        case Drag::marquee:
        {
            marquee = juce::Rectangle<float> (dragStart, p);
            selection = marqueeBase;
            for (auto& n : channelNotes())
                if (noteRect (Project::readNote (n)).intersects (marquee) && ! selection.contains (n))
                    selection.add (n);
            updateChordLabel();
            repaint();
            break;
        }

        case Drag::move:
        {
            if (! anchor.isValid() || originals.empty())
                break;

            const auto anchorOriginal = std::find_if (originals.begin(), originals.end(), [this] (const Original& o) { return o.tree == anchor; });
            if (anchorOriginal == originals.end())
                break;

            const double rawStart = (double) anchorOriginal->data.start + (tick - dragStartTick);
            const juce::int64 newAnchorStart = free ? (juce::int64) rawStart : snap (rawStart, true);
            juce::int64 deltaTicks = newAnchorStart - anchorOriginal->data.start;
            int deltaKey = constrainKey (anchorOriginal->data.key + (yToKey (p.y) - dragStartKey)) - anchorOriginal->data.key;

            for (auto& o : originals)
            {
                deltaTicks = juce::jmax (deltaTicks, -o.data.start);
                deltaKey = juce::jlimit (-o.data.key, 127 - o.data.key, deltaKey);
            }

            for (auto& o : originals)
            {
                auto d = o.data;
                d.start += deltaTicks;
                d.key += deltaKey;
                o.tree.setProperty (id::start, d.start, project.um());
                o.tree.setProperty (id::key, d.key, project.um());
            }

            preview (anchorOriginal->data.key + deltaKey);
            break;
        }

        case Drag::resizeRight:
        case Drag::resizeLeft:
        {
            const double delta = tick - dragStartTick;
            const auto minLen = free ? (juce::int64) 1 : juce::jmax ((juce::int64) 1, snapTicks());

            for (auto& o : originals)
            {
                if (drag == Drag::resizeRight)
                {
                    const double rawEnd = (double) (o.data.start + o.data.length) + delta;
                    const juce::int64 end = free ? (juce::int64) rawEnd : snap (rawEnd, true);
                    const auto len = juce::jmax (minLen, end - o.data.start);
                    o.tree.setProperty (id::length, len, project.um());
                    if (o.tree == anchor)
                        lastLength = len;
                }
                else
                {
                    const juce::int64 end = o.data.start + o.data.length;
                    const double rawStart = (double) o.data.start + delta;
                    juce::int64 start = free ? (juce::int64) rawStart : snap (rawStart, true);
                    start = juce::jlimit ((juce::int64) 0, end - minLen, start);
                    o.tree.setProperty (id::start, start, project.um());
                    o.tree.setProperty (id::length, end - start, project.um());
                }
            }
            break;
        }

        case Drag::none:
            break;
    }
}

void PianoRoll::mouseUp (const juce::MouseEvent& e)
{
    if (endPan (e))
        return;

    stopPreview();
    drag = Drag::none;
    originals.clear();
    marqueeBase.clear();
    velocityTargets.clear();
    velocityOriginals.clear();
    updateChordLabel();
    repaint();
}

juce::ValueTree PianoRoll::ghostAt (juce::Point<float> p) const
{
    if (! ghosts)
        return {};

    for (int i = pattern().getNumChildren(); --i >= 0;)
    {
        auto n = pattern().getChild (i);
        if ((int) n[id::channel] != channelUid && noteRect (Project::readNote (n)).contains (p))
            return n;
    }
    return {};
}

void PianoRoll::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (gridArea.contains (e.position.toInt()) && tool == Tool::select)
    {
        bool r, l;
        auto hit = hitTest (e.position, r, l);
        if (hit.isValid())
        {
            selection.clear();
            const int key = hit[id::key];
            for (auto& n : channelNotes())
                if ((int) n[id::key] == key)
                    selection.add (n);
            repaint();
        }
    }
}

std::vector<juce::ValueTree> PianoRoll::velocityTargetsAt (float x) const
{
    float bestDistance = stemReach;
    float bestX = 0.0f;
    for (auto& n : channelNotes())
    {
        const float nx = (float) tickToX ((double) (juce::int64) n[id::start]);
        const float d = std::abs (nx - x);
        if (d < bestDistance || (d <= stemGrab && selection.contains (n) && d <= bestDistance + 1.0f))
        {
            bestDistance = d;
            bestX = nx;
        }
    }

    std::vector<juce::ValueTree> result;
    if (bestDistance >= stemReach)
        return result;

    for (auto& n : channelNotes())
        if (std::abs ((float) tickToX ((double) (juce::int64) n[id::start]) - bestX) < 1.5f)
            result.push_back (n);

    std::stable_sort (result.begin(), result.end(), [this] (const juce::ValueTree& a, const juce::ValueTree& b)
    {
        return selection.contains (a) && ! selection.contains (b);
    });
    return result;
}

void PianoRoll::applyVelocity (juce::Point<float> p, bool paint)
{
    const float v = juce::jlimit (0.02f, 1.0f, (float) (velocityArea.getBottom() - 8 - p.y) / (float) (velocityArea.getHeight() - 16));

    if (paint)
    {
        const bool limitToSelection = ! selection.isEmpty();
        for (auto& n : channelNotes())
        {
            if (limitToSelection && ! selection.contains (n))
                continue;

            const float x = (float) tickToX ((double) (juce::int64) n[id::start]);
            if (std::abs (x - p.x) <= 6.0f)
                n.setProperty (id::velocity, v, project.um());
        }
        lastVelocity = v;
        return;
    }

    if (velocityTargets.empty())
        return;

    if (velocityGrabValue > 0.0f)
    {
        const float delta = v - velocityGrabValue;
        for (size_t i = 0; i < velocityTargets.size(); ++i)
            velocityTargets[i].setProperty (id::velocity, juce::jlimit (0.02f, 1.0f, velocityOriginals[i] + delta), project.um());
    }
    else
    {
        for (auto& t : velocityTargets)
            t.setProperty (id::velocity, v, project.um());
    }

    lastVelocity = v;
    if (ctx.setHint)
        ctx.setHint ("Velocity " + juce::String (juce::roundToInt (v * 100.0f)) + "%");
}

void PianoRoll::nudgeVelocity (const std::vector<juce::ValueTree>& notes, float delta)
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (now - lastWheelEdit > 700.0)
        project.beginTransaction ("Change velocity");
    lastWheelEdit = now;

    float shown = 0.0f;
    for (auto n : notes)
    {
        shown = juce::jlimit (0.02f, 1.0f, (float) n.getProperty (id::velocity, 0.78f) + delta);
        n.setProperty (id::velocity, shown, project.um());
    }

    lastVelocity = shown;
    if (ctx.setHint)
        ctx.setHint ("Velocity " + juce::String (juce::roundToInt (shown * 100.0f)) + "%" + (notes.size() > 1 ? "  (" + juce::String ((int) notes.size()) + " notes)" : juce::String()));
}

void PianoRoll::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    if (e.mods.isAltDown() && ! e.mods.isCtrlDown())
    {
        std::vector<juce::ValueTree> targets;
        const auto p = e.position;

        if (gridArea.contains (p.toInt()))
        {
            bool r, l;
            auto hit = hitTest (p, r, l);
            if (hit.isValid())
            {
                if (selection.contains (hit))
                    for (auto& s : selection)
                        targets.push_back (s);
                else
                    targets.push_back (hit);
            }
        }
        else if (velocityArea.contains (p.toInt()))
        {
            targets = velocityTargetsAt (p.x);
            if (! targets.empty() && selection.size() > 1 && selection.contains (targets.front()))
            {
                targets.clear();
                for (auto& s : selection)
                    targets.push_back (s);
            }
        }

        if (! targets.empty())
        {
            const float raw = std::abs (wheel.deltaY) > std::abs (wheel.deltaX) ? wheel.deltaY : wheel.deltaX;
            const float step = wheel.isSmooth ? raw * 0.25f : (raw > 0.0f ? 0.02f : -0.02f);
            nudgeVelocity (targets, e.mods.isShiftDown() ? step * 0.25f : step);
            return;
        }
    }

    TimelineCanvas::mouseWheelMove (e, wheel);
}

void PianoRoll::eraseAt (juce::Point<float> p)
{
    bool r, l;
    auto hit = hitTest (p, r, l);
    if (hit.isValid())
    {
        selection.removeAllInstancesOf (hit);
        pattern().removeChild (hit, project.um());
    }
}

void PianoRoll::sliceAt (double tick, const juce::ValueTree& hit)
{
    const auto at = snap (tick, true);
    std::vector<juce::ValueTree> targets;

    if (hit.isValid())
        targets.push_back (hit);
    else
        for (auto& n : channelNotes())
            targets.push_back (n);

    for (auto& t : targets)
    {
        auto n = Project::readNote (t);
        if (at <= n.start || at >= n.start + n.length)
            continue;

        auto tail = n;
        tail.start = at;
        tail.length = n.start + n.length - at;
        t.setProperty (id::length, at - n.start, project.um());
        project.addNote (pattern(), tail);
    }
}

void PianoRoll::editSelectAll()
{
    selection.clear();
    for (auto& n : channelNotes())
        selection.add (n);
    updateChordLabel();
    repaint();
}

void PianoRoll::editCopy()
{
    pruneSelection();
    if (selection.isEmpty())
        return;

    clipboard.clear();
    clipboardSequence = clipboard::sequenceNumber();
    juce::int64 first = std::numeric_limits<juce::int64>::max();
    for (auto& t : selection)
        first = juce::jmin (first, (juce::int64) t[id::start]);

    const auto barAligned = (first / project.ticksPerBeat()) * project.ticksPerBeat();
    for (auto& t : selection)
    {
        auto n = Project::readNote (t);
        n.start -= barAligned;
        clipboard.push_back (n);
    }
}

void PianoRoll::editCut()
{
    editCopy();
    editDelete();
}

void PianoRoll::editPaste()
{
    if (! project.findChannel (channelUid).isValid())
        return;

    const auto mouse = getMouseXYRelative();
    const auto at = gridArea.contains (mouse) ? snap (xToTick (mouse.x)) : snap (viewTick.current, true);

    if (clipboard.empty() || clipboard::sequenceNumber() != clipboardSequence)
    {
        if (auto external = clipboard::readMidi())
        {
            const auto notes = midi::flatten (midi::importTracks (*external));
            if (! notes.empty())
            {
                pasteNotesAt (notes, at, "Paste MIDI");
                if (ctx.notify)
                    ctx.notify ("Pasted " + juce::String ((int) notes.size()) + " notes from the clipboard", false);
                return;
            }
        }
    }

    if (clipboard.empty())
        return;

    project.beginTransaction ("Paste notes");
    selection.clear();
    for (auto n : clipboard)
    {
        n.channel = channelUid;
        n.start += at;
        selection.add (project.addNote (pattern(), n));
    }
    repaint();
}

void PianoRoll::editDelete()
{
    pruneSelection();
    if (selection.isEmpty())
        return;

    project.beginTransaction ("Delete notes");
    const auto targets = selection;
    selection.clear();
    auto notes = pattern();
    for (const auto& t : targets)
        notes.removeChild (t, project.um());
    repaint();
}

void PianoRoll::editDuplicate()
{
    pruneSelection();
    if (selection.isEmpty())
        editSelectAll();
    if (selection.isEmpty())
        return;

    juce::int64 first = std::numeric_limits<juce::int64>::max(), last = 0;
    for (auto& t : selection)
    {
        const auto n = Project::readNote (t);
        first = juce::jmin (first, n.start);
        last = juce::jmax (last, n.start + n.length);
    }

    const auto beat = project.ticksPerBeat();
    const auto span = juce::jmax (beat, ((last - first + beat - 1) / beat) * beat);

    project.beginTransaction ("Duplicate notes");
    juce::Array<juce::ValueTree> copies;
    for (auto& t : selection)
    {
        auto n = Project::readNote (t);
        n.start += span;
        copies.add (project.addNote (pattern(), n));
    }
    selection = copies;
    repaint();
}

void PianoRoll::editQuantize()
{
    pruneSelection();
    auto targets = selection.isEmpty() ? juce::Array<juce::ValueTree>() : selection;
    if (targets.isEmpty())
        for (auto& n : channelNotes())
            targets.add (n);

    project.beginTransaction ("Quantize");
    for (auto& t : targets)
        t.setProperty (id::start, snap ((double) (juce::int64) t[id::start], true), project.um());

    if (ctx.notify)
        ctx.notify ("Quantized " + juce::String (targets.size()) + " notes to " + snapOptions()[(size_t) snapIndex].name.toLowerCase(), false);
}

void PianoRoll::editHumanize()
{
    pruneSelection();
    auto targets = selection;
    if (targets.isEmpty())
        for (auto& n : channelNotes())
            targets.add (n);

    juce::Random rng;
    const auto range = juce::jmax (2, ticksPerStep / 12);

    project.beginTransaction ("Humanize");
    for (auto& t : targets)
    {
        auto n = Project::readNote (t);
        n.start = juce::jmax ((juce::int64) 0, n.start + rng.nextInt ({ -range, range + 1 }));
        n.velocity = juce::jlimit (0.05f, 1.0f, n.velocity + (rng.nextFloat() - 0.5f) * 0.16f);
        Project::writeNote (t, n, project.um());
    }

    if (ctx.notify)
        ctx.notify ("Humanized " + juce::String (targets.size()) + " notes", false);
}

void PianoRoll::editToggleMute()
{
    pruneSelection();
    if (selection.isEmpty())
        return;

    project.beginTransaction ("Toggle mute");
    const bool newState = ! (bool) selection.getFirst()[id::muted];
    for (auto& t : selection)
        t.setProperty (id::muted, newState, project.um());
}

void PianoRoll::insertNotes (const std::vector<NoteData>& notes, bool replaceExisting, bool appendAfterExisting)
{
    if (! project.findChannel (channelUid).isValid())
        return;

    project.beginTransaction ("Generate notes");

    if (replaceExisting)
        for (auto& n : channelNotes())
            pattern().removeChild (n, project.um());

    juce::int64 offset = 0;
    if (! replaceExisting && appendAfterExisting)
        for (auto& n : channelNotes())
            offset = juce::jmax (offset, (juce::int64) n[id::start] + (juce::int64) n[id::length]);

    const auto bar = project.ticksPerBar();
    offset = ((offset + bar - 1) / bar) * bar;

    selection.clear();
    for (auto n : notes)
    {
        n.channel = channelUid;
        n.start += offset;
        selection.add (project.addNote (pattern(), n));
    }

    scrollToTick ((double) offset);
    updateChordLabel();
    repaint();
}

void PianoRoll::showChordGenerator()
{
    if (! project.findChannel (channelUid).isValid())
    {
        if (ctx.notify)
            ctx.notify ("Select or add a channel first", true);
        return;
    }

    if (ctx.showPanel)
        ctx.showPanel (createChordGeneratorPanel (ctx, [this] (const std::vector<NoteData>& notes, bool replace)
        {
            insertNotes (notes, replace);
        }), "Chord Generator");
}

void PianoRoll::showFollowGenerator()
{
    if (! project.findChannel (channelUid).isValid())
    {
        if (ctx.notify)
            ctx.notify ("Select or add a channel first", true);
        return;
    }

    if (ctx.showPanel)
        ctx.showPanel (createFollowPanel (ctx, channelUid, [this] (const std::vector<NoteData>& notes, bool replace)
        {
            insertNotes (notes, replace, false);
            scrollToTick (0.0);
        }), "Follow Chords");
}

bool PianoRoll::keyPressed (const juce::KeyPress& key)
{
    const auto mods = key.getModifiers();
    const int code = key.getKeyCode();

    if (code == juce::KeyPress::escapeKey)
    {
        if (! hasSelection() && ctx.isEditorMaximised && ctx.isEditorMaximised() && ctx.toggleEditorMaximised)
        {
            ctx.toggleEditorMaximised();
            return true;
        }
        selection.clear();
        repaint();
        return true;
    }

    if (! mods.isAnyModifierKeyDown() || mods.isShiftDown())
    {
        if (code == 'P' || code == 'p') { setTool (Tool::draw); return true; }
        if (code == 'E' || code == 'e') { setTool (Tool::select); return true; }
        if (code == 'C' || code == 'c') { setTool (Tool::slice); return true; }
        if (code == 'T' || code == 't') { setTool (Tool::mute); return true; }
    }

    if ((code == juce::KeyPress::upKey || code == juce::KeyPress::downKey) && ! selection.isEmpty())
    {
        pruneSelection();
        const int amount = (code == juce::KeyPress::upKey ? 1 : -1) * (mods.isShiftDown() ? 12 : 1);
        project.beginTransaction ("Transpose");
        for (auto& t : selection)
            t.setProperty (id::key, juce::jlimit (0, 127, (int) t[id::key] + amount), project.um());
        if (! selection.isEmpty())
        {
            preview ((int) selection.getFirst()[id::key]);
            juce::Timer::callAfterDelay (160, [safe = juce::Component::SafePointer<PianoRoll> (this)] { if (safe) safe->stopPreview(); });
        }
        return true;
    }

    if ((code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey) && ! selection.isEmpty())
    {
        pruneSelection();
        const auto step = mods.isShiftDown() ? project.ticksPerBar() : juce::jmax ((juce::int64) 1, snapTicks());
        const auto delta = code == juce::KeyPress::rightKey ? step : -step;
        project.beginTransaction ("Move notes");
        for (auto& t : selection)
            t.setProperty (id::start, juce::jmax ((juce::int64) 0, (juce::int64) t[id::start] + delta), project.um());
        return true;
    }

    return false;
}

void PianoRoll::valueTreePropertyChanged (juce::ValueTree& t, const juce::Identifier& p)
{
    if (p == id::selectedChannel)
    {
        setChannel (project.selectedChannel());
        return;
    }

    if (p == id::currentPattern)
        selection.clear();

    if (t.hasType (id::CHANNEL) || t.hasType (id::PROJECT))
        refreshToolbar();

    if (t.hasType (id::NOTE) || t.hasType (id::PATTERN) || t.hasType (id::PROJECT) || t.hasType (id::CHANNEL))
        repaint();
}

void PianoRoll::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree&)
{
    if (parent.hasType (id::PATTERN) || parent.hasType (id::CHANNELS))
    {
        refreshToolbar();
        repaint();
    }
}

void PianoRoll::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree&, int)
{
    if (parent.hasType (id::PATTERN) || parent.hasType (id::CHANNELS))
    {
        refreshToolbar();
        repaint();
    }
}
}
