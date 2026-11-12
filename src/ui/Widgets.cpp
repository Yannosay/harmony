#include "ui/Widgets.h"
#include "engine/Gain.h"

namespace harmony::ui
{
juce::String formatDecibels (float fader)
{
    const float db = engine::faderToDecibels (fader);
    if (db <= -95.0f)
        return juce::String::fromUTF8 ("-\xe2\x88\x9e dB");
    return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
}

void keepFocusOnEditors (std::initializer_list<juce::Component*> components)
{
    for (auto* c : components)
    {
        c->setWantsKeyboardFocus (false);
        c->setMouseClickGrabsKeyboardFocus (false);
    }
}

IconButton::IconButton (const juce::String& name, juce::Path i, bool f)
    : juce::Button (name), icon (std::move (i)), filled (f)
{
    keepFocusOnEditors ({ this });
    setTooltip (name);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void IconButton::setIcon (juce::Path newIcon, bool filledIcon)
{
    icon = std::move (newIcon);
    filled = filledIcon;
    repaint();
}

void IconButton::paintButton (juce::Graphics& g, bool over, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool on = getToggleState();

    if (framed || over || down || on)
    {
        g.setColour (on ? activeColour.withAlpha (0.16f) : (over ? colours::controlHover : colours::control));
        g.fillRoundedRectangle (r, 6.0f);
    }

    if (framed)
    {
        g.setColour (on ? activeColour.withAlpha (0.5f) : colours::line);
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }

    const float pad = juce::jmax (4.0f, r.getHeight() * 0.24f);
    auto colour = on ? activeColour : (over ? colours::text : colours::textDim);
    if (! isEnabled())
        colour = colour.withMultipliedAlpha (0.35f);

    drawIcon (g, icon, r.reduced (pad), colour, filled);
}

PropertyBinding::~PropertyBinding()
{
    if (tree.isValid())
        tree.removeListener (this);
}

void PropertyBinding::bind (juce::ValueTree t, juce::Identifier p, juce::UndoManager* u, std::function<void (const juce::var&)> cb)
{
    if (tree.isValid())
        tree.removeListener (this);

    tree = std::move (t);
    property = p;
    undo = u;
    onChange = std::move (cb);

    if (tree.isValid())
    {
        tree.addListener (this);
        if (onChange)
            onChange (tree[property]);
    }
}

void PropertyBinding::set (const juce::var& value, bool undoable)
{
    if (tree.isValid())
        tree.setProperty (property, value, undoable ? undo : nullptr);
}

juce::var PropertyBinding::get() const
{
    return tree.isValid() ? tree[property] : juce::var();
}

void PropertyBinding::beginGesture (const juce::String& name)
{
    if (undo != nullptr)
        undo->beginNewTransaction (name);
}

void PropertyBinding::valueTreePropertyChanged (juce::ValueTree& t, const juce::Identifier& p)
{
    if (t == tree && p == property && onChange)
        onChange (tree[property]);
}

Knob::Knob (double minimum, double maximum, double def, const juce::String& tip)
    : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox), defaultValue (def)
{
    setRange (minimum, maximum, 0.0);
    setValue (def, juce::dontSendNotification);
    setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    setMouseDragSensitivity (160);
    setVelocityBasedMode (false);
    setPopupDisplayEnabled (true, true, nullptr, 900);
    setTooltip (tip);
    setName (tip);
    keepFocusOnEditors ({ this });
}

void Knob::bind (juce::ValueTree tree, const juce::Identifier& property, juce::UndoManager* undo)
{
    binding.bind (std::move (tree), property, undo, [this] (const juce::var& v)
    {
        const juce::ScopedValueSetter<bool> svs (updatingFromModel, true);
        setValue (v.isVoid() ? defaultValue : (double) v, juce::dontSendNotification);
    });
}

void Knob::showTransientValue (double v)
{
    if (isMouseButtonDown() || std::abs (getValue() - v) < 1.0e-5)
        return;

    const juce::ScopedValueSetter<bool> svs (updatingFromModel, true);
    setValue (v, juce::dontSendNotification);
}

void Knob::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onContextMenu)
    {
        onContextMenu();
        return;
    }
    juce::Slider::mouseDown (e);
}

void Knob::mouseDoubleClick (const juce::MouseEvent&)
{
    binding.beginGesture ("Reset " + getName());
    setValue (defaultValue, juce::sendNotificationSync);
}

void Knob::startedDragging()
{
    binding.beginGesture ("Change " + getName());
}

void Knob::valueChanged()
{
    if (! updatingFromModel)
        binding.set ((float) getValue());
}

juce::String Knob::getTextFromValue (double v)
{
    if (describe)
        return describe (v);
    return juce::String (v, 2);
}

Fader::Fader()
    : juce::Slider (juce::Slider::LinearVertical, juce::Slider::NoTextBox)
{
    setRange (0.0, 1.25, 0.0);
    setValue (engine::unityFader, juce::dontSendNotification);
    setSkewFactor (1.0);
    setPopupDisplayEnabled (true, true, nullptr, 900);
    setName ("Volume");
    keepFocusOnEditors ({ this });
}

void Fader::bind (juce::ValueTree tree, const juce::Identifier& property, juce::UndoManager* undo)
{
    binding.bind (std::move (tree), property, undo, [this] (const juce::var& v)
    {
        const juce::ScopedValueSetter<bool> svs (updatingFromModel, true);
        setValue (v.isVoid() ? engine::unityFader : (double) v, juce::dontSendNotification);
    });
}

void Fader::showTransientValue (double v)
{
    if (isMouseButtonDown() || std::abs (getValue() - v) < 1.0e-5)
        return;

    const juce::ScopedValueSetter<bool> svs (updatingFromModel, true);
    setValue (v, juce::dontSendNotification);
}

void Fader::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onContextMenu)
    {
        onContextMenu();
        return;
    }
    juce::Slider::mouseDown (e);
}

void Fader::mouseDoubleClick (const juce::MouseEvent&)
{
    binding.beginGesture ("Reset volume");
    setValue (engine::unityFader, juce::sendNotificationSync);
}

void Fader::startedDragging()
{
    binding.beginGesture ("Change volume");
}

void Fader::valueChanged()
{
    if (! updatingFromModel)
        binding.set ((float) getValue());
}

juce::String Fader::getTextFromValue (double v)
{
    return formatDecibels ((float) v);
}

void Fader::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    const float pos = (float) valueToProportionOfLength (getValue());
    const float unity = (float) valueToProportionOfLength (engine::unityFader);
    const float top = 8.0f, bottom = r.getHeight() - 8.0f;
    const auto yFor = [&] (float p) { return bottom - p * (bottom - top); };

    auto track = juce::Rectangle<float> (r.getCentreX() - 2.0f, top, 4.0f, bottom - top);
    g.setColour (colours::control.brighter (0.12f));
    g.fillRoundedRectangle (track, 2.0f);

    g.setColour (colours::accent);
    g.fillRoundedRectangle (track.withTop (yFor (pos)), 2.0f);

    g.setColour (colours::textFaint);
    g.fillRect (r.getCentreX() - 9.0f, yFor (unity), 4.0f, 1.0f);
    g.fillRect (r.getCentreX() + 5.0f, yFor (unity), 4.0f, 1.0f);

    const auto thumb = juce::Rectangle<float> (r.getCentreX() - 11.0f, yFor (pos) - 5.0f, 22.0f, 10.0f);
    g.setColour (isMouseOverOrDragging() ? colours::text : colours::text.withAlpha (0.92f));
    g.fillRoundedRectangle (thumb, 3.0f);
    g.setColour (colours::background.withAlpha (0.5f));
    g.fillRect (thumb.withSizeKeepingCentre (12.0f, 1.0f));
}

NumberBox::NumberBox (double mn, double mx, double st, int d)
    : minimum (mn), maximum (mx), step (st), decimals (d)
{
    keepFocusOnEditors ({ this });
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
}

void NumberBox::setValue (double v, juce::NotificationType notification)
{
    v = juce::jlimit (minimum, maximum, std::round (v / step) * step);
    if (std::abs (v - value) < 1.0e-9)
        return;

    value = v;
    repaint();

    if (notification != juce::dontSendNotification && onChange)
        onChange (value);
}

void NumberBox::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    g.setColour (isMouseOver() ? colours::controlHover : colours::control);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (r, 6.0f, 1.0f);

    auto text = getLocalBounds().reduced (8, 0);
    if (label.isNotEmpty())
    {
        g.setColour (colours::textFaint);
        g.setFont (font (9.0f, Weight::medium));
        g.drawText (label, text, juce::Justification::centredLeft, false);
    }

    g.setColour (colours::text);
    g.setFont (font (13.0f, Weight::medium));
    g.drawText (juce::String (value, decimals), text, label.isNotEmpty() ? juce::Justification::centredRight : juce::Justification::centred, false);
}

void NumberBox::mouseDown (const juce::MouseEvent&)
{
    dragStartValue = value;
    if (onGestureStart)
        onGestureStart();
}

void NumberBox::mouseDrag (const juce::MouseEvent& e)
{
    const double sensitivity = e.mods.isShiftDown() ? 0.1 : 1.0;
    setValue (dragStartValue - e.getDistanceFromDragStartY() * step * sensitivity * (decimals > 0 ? 1.0 : 0.5));
}

void NumberBox::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel)
{
    if (onGestureStart)
        onGestureStart();
    setValue (value + (wheel.deltaY > 0 ? step : -step));
}

void NumberBox::mouseDoubleClick (const juce::MouseEvent&)
{
    editor = std::make_unique<juce::TextEditor>();
    editor->setFont (font (13.0f, Weight::medium));
    editor->setJustification (juce::Justification::centred);
    editor->setText (juce::String (value, decimals), false);
    editor->selectAll();
    editor->setInputRestrictions (8, "0123456789.");
    addAndMakeVisible (*editor);
    editor->setBounds (getLocalBounds());
    editor->grabKeyboardFocus();

    auto commit = [this]
    {
        if (editor == nullptr)
            return;

        const auto text = editor->getText();
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<NumberBox> (this), text]
        {
            if (safe == nullptr)
                return;
            if (safe->onGestureStart)
                safe->onGestureStart();
            if (text.isNotEmpty())
                safe->setValue (text.getDoubleValue());
            safe->editor.reset();
        });
    };

    editor->onReturnKey = commit;
    editor->onFocusLost = commit;
    editor->onEscapeKey = [this] { juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<NumberBox> (this)] { if (safe) safe->editor.reset(); }); };
}

LevelMeter::LevelMeter (bool v) : vertical (v)
{
    setInterceptsMouseClicks (false, false);
}

void LevelMeter::push (float left, float right)
{
    const auto step = [] (float& level, float& peak, int& hold, float input)
    {
        level = input > level ? input : level * 0.86f;
        if (input >= peak) { peak = input; hold = 40; }
        else if (--hold < 0) peak *= 0.95f;
    };

    step (levelL, peakL, peakHoldL, left);
    step (levelR, peakR, peakHoldR, right);
    repaint();
}

void LevelMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const auto toProportion = [] (float gain)
    {
        const float db = juce::Decibels::gainToDecibels (gain, -60.0f);
        return juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 66.0f);
    };

    auto drawBar = [&] (juce::Rectangle<float> bar, float level, float peak)
    {
        g.setColour (colours::control);
        g.fillRoundedRectangle (bar, 1.5f);

        const float p = toProportion (level);
        auto filled = vertical ? bar.withTop (bar.getBottom() - bar.getHeight() * p) : bar.withWidth (bar.getWidth() * p);
        juce::ColourGradient grad = vertical
            ? juce::ColourGradient (colours::lilac, 0.0f, bar.getBottom(), colours::accent, 0.0f, bar.getY(), false)
            : juce::ColourGradient (colours::lilac, bar.getX(), 0.0f, colours::accent, bar.getRight(), 0.0f, false);
        if (level >= 0.999f)
            grad = juce::ColourGradient (colours::danger, 0.0f, 0.0f, colours::danger, 1.0f, 1.0f, false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (filled, 1.5f);

        const float pk = toProportion (peak);
        g.setColour (peak >= 0.999f ? colours::danger : colours::text.withAlpha (0.8f));
        if (vertical)
            g.fillRect (bar.getX(), bar.getBottom() - bar.getHeight() * pk, bar.getWidth(), 1.5f);
        else
            g.fillRect (bar.getX() + bar.getWidth() * pk - 1.5f, bar.getY(), 1.5f, bar.getHeight());
    };

    if (vertical)
    {
        const float w = (r.getWidth() - 2.0f) * 0.5f;
        drawBar (r.removeFromLeft (w), levelL, peakL);
        r.removeFromLeft (2.0f);
        drawBar (r, levelR, peakR);
    }
    else
    {
        const float h = (r.getHeight() - 2.0f) * 0.5f;
        drawBar (r.removeFromTop (h), levelL, peakL);
        r.removeFromTop (2.0f);
        drawBar (r, levelR, peakR);
    }
}

Overlay::Overlay (std::unique_ptr<juce::Component> c, const juce::String& t)
    : content (std::move (c)), title (t), preferredSize (content->getWidth(), content->getHeight())
{
    setWantsKeyboardFocus (true);
    content->setBounds ({});
    addAndMakeVisible (*content);
    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { dismiss(); };
}

Overlay* Overlay::show (juce::Component& host, std::unique_ptr<juce::Component> c, const juce::String& t)
{
    closeAll (host);
    auto* overlay = new Overlay (std::move (c), t);
    host.addAndMakeVisible (overlay);
    overlay->setBounds (host.getLocalBounds());
    overlay->setAlpha (0.0f);
    juce::Desktop::getInstance().getAnimator().animateComponent (overlay, overlay->getBounds(), 1.0f, 140, false, 1.0, 0.0);
    overlay->grabKeyboardFocus();
    return overlay;
}

void Overlay::closeAll (juce::Component& host)
{
    for (int i = host.getNumChildComponents(); --i >= 0;)
        if (auto* o = dynamic_cast<Overlay*> (host.getChildComponent (i)))
            o->dismiss();
}

void Overlay::dismiss()
{
    if (onDismiss)
    {
        auto cb = std::move (onDismiss);
        onDismiss = nullptr;
        cb();
    }

    setVisible (false);
    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Overlay> (this)]
    {
        if (safe != nullptr)
            delete safe.getComponent();
    });
}

void Overlay::resized()
{
    const int headerHeight = 48;
    const int w = juce::jmin (getWidth() - 40, preferredSize.x);
    const int h = juce::jmin (getHeight() - 40, preferredSize.y + headerHeight);
    card = getLocalBounds().withSizeKeepingCentre (w, h);

    auto inner = card;
    auto header = inner.removeFromTop (headerHeight);
    closeButton.setBounds (header.removeFromRight (headerHeight).reduced (10));
    content->setBounds (inner);
}

void Overlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.55f));

    juce::DropShadow (juce::Colours::black.withAlpha (0.6f), 40, { 0, 12 }).drawForRectangle (g, card);
    g.setColour (colours::raised);
    g.fillRoundedRectangle (card.toFloat(), 12.0f);
    g.setColour (colours::line);
    g.drawRoundedRectangle (card.toFloat().reduced (0.5f), 12.0f, 1.0f);

    g.setColour (colours::text);
    g.setFont (font (14.0f, Weight::semibold));
    g.drawText (title, card.withHeight (48).reduced (20, 0), juce::Justification::centredLeft, true);
    g.setColour (colours::line);
    g.fillRect (card.getX(), card.getY() + 48, card.getWidth(), 1);
}

void Overlay::mouseDown (const juce::MouseEvent& e)
{
    if (! card.contains (e.getPosition()))
        dismiss();
}

bool Overlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        dismiss();
        return true;
    }
    return false;
}

Toasts::Toasts()
{
    setInterceptsMouseClicks (false, false);
}

void Toasts::show (const juce::String& message, bool isError)
{
    items.push_back ({ message, isError, juce::Time::getMillisecondCounter() });
    if (items.size() > 4)
        items.erase (items.begin());
    startTimerHz (30);
    repaint();
}

void Toasts::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounter();
    items.erase (std::remove_if (items.begin(), items.end(), [now] (const Item& i) { return now - i.born > 4800; }), items.end());
    if (items.empty())
        stopTimer();
    repaint();
}

void Toasts::paint (juce::Graphics& g)
{
    const auto now = juce::Time::getMillisecondCounter();
    int y = getHeight() - 16;

    for (auto it = items.rbegin(); it != items.rend(); ++it)
    {
        const float age = (float) (now - it->born);
        const float alpha = juce::jlimit (0.0f, 1.0f, juce::jmin (age / 160.0f, (4800.0f - age) / 400.0f));
        const float rise = (1.0f - juce::jmin (1.0f, age / 220.0f)) * 10.0f;

        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (font (11.5f), it->text, 0.0f, 0.0f);
        const int w = juce::jmin (getWidth() - 40, (int) glyphs.getBoundingBox (0, -1, true).getWidth() + 44);
        const juce::Rectangle<float> box ((float) (getWidth() - w) * 0.5f, (float) (y - 38) + rise, (float) w, 38.0f);

        g.setColour (colours::raised.withAlpha (alpha));
        g.fillRoundedRectangle (box, 10.0f);
        g.setColour ((it->error ? colours::danger : colours::line).withAlpha (alpha));
        g.drawRoundedRectangle (box.reduced (0.5f), 10.0f, 1.0f);
        g.setColour ((it->error ? colours::danger : colours::accent).withAlpha (alpha));
        g.fillEllipse (box.getX() + 14.0f, box.getCentreY() - 3.0f, 6.0f, 6.0f);
        g.setColour (colours::text.withAlpha (alpha));
        g.setFont (font (11.5f));
        g.drawText (it->text, box.withTrimmedLeft (28.0f).withTrimmedRight (12.0f), juce::Justification::centredLeft, true);
        y -= 46;
    }
}
}
