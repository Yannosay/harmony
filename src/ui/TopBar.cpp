#include "ui/TopBar.h"
#include "app/CommandIds.h"
#include "engine/AudioEngine.h"

namespace harmony::ui
{
TopBar::TopBar (Context& c, juce::MenuBarModel& model)
    : ctx (c), menuBar (&model)
{
    addAndMakeVisible (menuBar);

    for (auto* b : { &patternModeButton, &songModeButton })
    {
        b->setClickingTogglesState (false);
        b->setColour (juce::TextButton::buttonOnColourId, colours::accent);
        addAndMakeVisible (b);
    }
    patternModeButton.setTooltip ("Pattern mode: loop the current pattern (L)");
    songModeButton.setTooltip ("Song mode: play the playlist (L)");
    patternModeButton.setConnectedEdges (juce::Button::ConnectedOnRight);
    songModeButton.setConnectedEdges (juce::Button::ConnectedOnLeft);
    patternModeButton.onClick = [this] { ctx.engine.setSongMode (false); refreshTransport(); };
    songModeButton.onClick = [this] { ctx.engine.setSongMode (true); refreshTransport(); };

    for (auto* b : { &playButton, &stopButton, &recordButton, &metronomeButton, &paletteButton, &updateButton })
    {
        b->setFramed (true);
        addAndMakeVisible (b);
    }

    playButton.onClick = [this] { ctx.commands.invokeDirectly (cmd::playPause, true); };
    stopButton.onClick = [this] { ctx.commands.invokeDirectly (cmd::stop, true); };
    recordButton.onClick = [this] { ctx.commands.invokeDirectly (cmd::record, true); };
    recordButton.setActiveColour (colours::danger);
    metronomeButton.onClick = [this] { ctx.commands.invokeDirectly (cmd::metronome, true); };
    paletteButton.onClick = [this] { ctx.commands.invokeDirectly (cmd::commandPalette, true); };
    updateButton.setActiveColour (colours::success);
    updateButton.setToggleState (true, juce::dontSendNotification);
    updateButton.setVisible (false);
    updateButton.onClick = [this] { if (onUpdateClicked) onUpdateClicked(); };

    bpmBox.setLabel ("BPM");
    bpmBox.setTooltip ("Tempo: drag, scroll or double-click to type");
    bpmBox.setValue (ctx.project.getBpm(), juce::dontSendNotification);
    bpmBox.onGestureStart = [this] { ctx.project.beginTransaction ("Change tempo"); };
    bpmBox.onChange = [this] (double v) { ctx.project.setBpm (v); };
    addAndMakeVisible (bpmBox);

    addAndMakeVisible (masterMeter);

    ctx.project.getState().addListener (this);
    refreshTransport();
    startTimerHz (30);
}

TopBar::~TopBar()
{
    ctx.project.getState().removeListener (this);
}

void TopBar::setUpdateReady (const juce::String& version)
{
    updateButton.setVisible (version.isNotEmpty());
    updateButton.setTooltip ("Harmony " + version + " is ready. Click to restart and update.");
    resized();
}

void TopBar::refreshTransport()
{
    const bool song = ctx.engine.isSongMode();
    patternModeButton.setToggleState (! song, juce::dontSendNotification);
    songModeButton.setToggleState (song, juce::dontSendNotification);

    const bool playing = ctx.engine.isPlaying();
    playButton.setIcon (playing ? icons::pause() : icons::play(), true);
    playButton.setToggleState (playing, juce::dontSendNotification);
    recordButton.setToggleState (ctx.engine.isRecording(), juce::dontSendNotification);
    metronomeButton.setToggleState (ctx.engine.isMetronomeEnabled(), juce::dontSendNotification);
}

void TopBar::valueTreePropertyChanged (juce::ValueTree& t, const juce::Identifier& p)
{
    if (t.hasType (id::PROJECT) && p == id::bpm)
        bpmBox.setValue (ctx.project.getBpm(), juce::dontSendNotification);
}

juce::String TopBar::positionText() const
{
    const double tick = ctx.engine.getPositionTicks();

    if (showSeconds)
    {
        const double seconds = tick / ppq * 60.0 / ctx.project.getBpm();
        const int minutes = (int) (seconds / 60.0);
        return juce::String (minutes) + ":" + juce::String (seconds - minutes * 60.0, 2).paddedLeft ('0', 5);
    }

    const auto bar = ctx.project.ticksPerBar();
    const auto beat = ctx.project.ticksPerBeat();
    const auto t = (juce::int64) tick;
    return juce::String (t / bar + 1) + ":" + juce::String ((t % bar) / beat + 1) + ":" + juce::String ((t % beat) / ticksPerStep + 1).paddedLeft ('0', 2);
}

void TopBar::timerCallback()
{
    refreshTransport();

    const auto text = positionText();
    if (text != timeText)
    {
        timeText = text;
        repaint (timeArea);
    }

    const auto cpu = juce::String (juce::roundToInt (ctx.engine.getCpuLoad() * 100.0)) + "%";
    if (cpu != cpuText)
    {
        cpuText = cpu;
        repaint (cpuArea);
    }

    if (auto* master = ctx.engine.getInsertNode (0))
        masterMeter.push (master->meter.takeLeft(), master->meter.takeRight());
}

void TopBar::resized()
{
    auto r = getLocalBounds().reduced (10, 8);
    brandArea = r.removeFromLeft (124);
    menuBar.setBounds (r.removeFromLeft (370));
    r.removeFromLeft (8);

    patternModeButton.setBounds (r.removeFromLeft (52));
    songModeButton.setBounds (r.removeFromLeft (56));
    r.removeFromLeft (10);

    for (auto* b : { &playButton, &stopButton, &recordButton })
    {
        b->setBounds (r.removeFromLeft (36));
        r.removeFromLeft (4);
    }
    r.removeFromLeft (6);
    bpmBox.setBounds (r.removeFromLeft (120));
    r.removeFromLeft (8);
    timeArea = r.removeFromLeft (116);
    r.removeFromLeft (8);
    metronomeButton.setBounds (r.removeFromLeft (36));

    paletteButton.setBounds (r.removeFromRight (36));
    r.removeFromRight (8);
    if (updateButton.isVisible())
    {
        updateButton.setBounds (r.removeFromRight (36));
        r.removeFromRight (8);
    }
    masterMeter.setBounds (r.removeFromRight (120).withSizeKeepingCentre (120, 14));
    r.removeFromRight (10);
    cpuArea = r.removeFromRight (70);
}

void TopBar::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::line);
    g.fillRect (0, getHeight() - 1, getWidth(), 1);

    if (auto* mark = brandMark())
        mark->drawWithin (g, brandArea.withWidth (26).toFloat().reduced (0.0f, 4.0f), juce::RectanglePlacement::centred, 1.0f);

    g.setColour (colours::text);
    g.setFont (font (14.0f, Weight::semibold));
    g.drawText ("Harmony", brandArea.withTrimmedLeft (34), juce::Justification::centredLeft, false);

    auto time = timeArea.toFloat();
    g.setColour (colours::background);
    g.fillRoundedRectangle (time, 6.0f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (time.reduced (0.5f), 6.0f, 1.0f);
    g.setColour (colours::accent);
    g.setFont (font (14.0f, Weight::medium));
    g.drawText (timeText, timeArea, juce::Justification::centred, false);

    drawIcon (g, icons::cpu(), cpuArea.toFloat().removeFromLeft (18.0f).withSizeKeepingCentre (16.0f, 16.0f), colours::textFaint);
    g.setColour (colours::textDim);
    g.setFont (font (10.5f, Weight::medium));
    g.drawText (cpuText, cpuArea.withTrimmedLeft (22), juce::Justification::centredLeft, false);
}

void TopBar::mouseDown (const juce::MouseEvent& e)
{
    if (timeArea.contains (e.getPosition()))
    {
        showSeconds = ! showSeconds;
        timeText = positionText();
        repaint (timeArea);
    }
}
}
