#include "ui/EffectEditor.h"
#include "engine/BuiltinEffects.h"

namespace harmony::ui
{
namespace
{
    constexpr int cellWidth = 92;
    constexpr int knobRowHeight = 104;
    constexpr int headerHeight = 58;
    constexpr int curveHeight = 150;
    constexpr int meterHeight = 34;

    class ParameterSlider final : public juce::Slider
    {
    public:
        ParameterSlider() : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox)
        {
            setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            setMouseDragSensitivity (180);
            setPopupDisplayEnabled (true, true, nullptr, 900);
            keepFocusOnEditors ({ this });
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && onContextMenu)
            {
                onContextMenu();
                return;
            }
            juce::Slider::mouseDown (e);
        }

        std::function<void()> onContextMenu;
    };

    class EffectEditor final : public juce::AudioProcessorEditor, private juce::Timer
    {
    public:
        EffectEditor (engine::builtin::Effect& e, Context& c)
            : juce::AudioProcessorEditor (e), effect (e), ctx (c)
        {
            const auto& params = effect.getParameters();
            for (int i = 0; i < params.size(); ++i)
            {
                auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (params[i]);
                if (ranged == nullptr)
                    continue;

                auto control = std::make_unique<Control>();
                control->parameter = ranged;
                control->index = i;
                const auto paramId = ranged->getParameterID();

                if (dynamic_cast<juce::AudioParameterBool*> (ranged) != nullptr)
                {
                    control->toggle = std::make_unique<juce::ToggleButton> (ranged->getName (40));
                    control->buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (effect.state, paramId, *control->toggle);
                    addAndMakeVisible (*control->toggle);
                    toggles.push_back (control.get());
                }
                else if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*> (ranged))
                {
                    control->combo = std::make_unique<juce::ComboBox>();
                    control->combo->addItemList (choiceParam->choices, 1);
                    control->comboAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (effect.state, paramId, *control->combo);
                    addAndMakeVisible (*control->combo);
                    choices.push_back (control.get());
                }
                else
                {
                    control->slider = std::make_unique<ParameterSlider>();
                    control->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (effect.state, paramId, *control->slider);
                    control->slider->setDoubleClickReturnValue (true, ranged->convertFrom0to1 (ranged->getDefaultValue()));
                    control->slider->setTooltip (ranged->getName (60) + ". Double-click to reset, right-click to automate.");
                    control->slider->onContextMenu = [this, index = i] { showParameterMenu (index); };
                    addAndMakeVisible (*control->slider);
                    knobs.push_back (control.get());
                }

                controls.push_back (std::move (control));
            }

            const bool dynamics = effect.info.group == "Dynamics";
            const int columns = juce::jlimit (3, 6, (int) knobs.size());
            const int knobRows = ((int) knobs.size() + columns - 1) / columns;
            const int extras = (int) (toggles.size() + choices.size());
            const int width = juce::jmax (440, columns * cellWidth + 48);
            const int height = headerHeight + (effect.hasResponseCurve() ? curveHeight + 12 : 0) + (dynamics ? meterHeight + 8 : 0)
                             + (extras > 0 ? 44 : 0) + knobRows * knobRowHeight + 16;

            setSize (width, height);
            startTimerHz (30);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (colours::panel);

            auto header = getLocalBounds().removeFromTop (headerHeight).reduced (20, 10);
            g.setColour (colours::text);
            g.setFont (font (15.0f, Weight::semibold));
            g.drawText (effect.info.name, header.removeFromTop (22), juce::Justification::centredLeft, false);
            g.setColour (colours::textDim);
            g.setFont (font (10.0f));
            g.drawText (effect.info.description, header, juce::Justification::centredLeft, true);

            if (effect.hasResponseCurve())
                paintCurve (g);
            if (! meterArea.isEmpty())
                paintMeter (g);

            g.setFont (font (9.5f, Weight::medium));
            for (auto* c : knobs)
            {
                const auto b = c->slider->getBounds();
                g.setColour (colours::textDim);
                g.drawText (c->parameter->getName (20), b.getX() - 8, b.getBottom() + 2, b.getWidth() + 16, 14, juce::Justification::centred, false);
                g.setColour (colours::text);
                g.drawText (c->slider->getTextFromValue (c->slider->getValue()), b.getX() - 8, b.getBottom() + 16, b.getWidth() + 16, 14,
                            juce::Justification::centred, false);
            }

            for (auto* c : choices)
            {
                g.setColour (colours::textDim);
                g.drawText (c->parameter->getName (20), c->combo->getBounds().withY (c->combo->getY() - 14).withHeight (14),
                            juce::Justification::centredLeft, false);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (20, 0);
            r.removeFromTop (headerHeight);

            if (effect.hasResponseCurve())
            {
                curveArea = r.removeFromTop (curveHeight);
                r.removeFromTop (12);
            }

            if (effect.info.group == "Dynamics")
            {
                meterArea = r.removeFromTop (meterHeight);
                r.removeFromTop (8);
            }

            if (! toggles.empty() || ! choices.empty())
            {
                auto row = r.removeFromTop (44).withTrimmedTop (14);
                for (auto* c : choices)
                {
                    c->combo->setBounds (row.removeFromLeft (150).withHeight (26));
                    row.removeFromLeft (12);
                }
                for (auto* c : toggles)
                {
                    c->toggle->setBounds (row.removeFromLeft (140).withHeight (26));
                    row.removeFromLeft (8);
                }
            }

            const int columns = juce::jlimit (3, 6, (int) knobs.size());
            for (size_t i = 0; i < knobs.size(); ++i)
            {
                const int col = (int) i % columns;
                const int row = (int) i / columns;
                const int x = r.getX() + col * cellWidth + (cellWidth - 52) / 2;
                const int y = r.getY() + row * knobRowHeight + 8;
                knobs[i]->slider->setBounds (x, y, 52, 52);
            }
        }

    private:
        struct Control
        {
            juce::RangedAudioParameter* parameter = nullptr;
            int index = 0;
            float lastValue = -1.0f;
            std::unique_ptr<ParameterSlider> slider;
            std::unique_ptr<juce::ToggleButton> toggle;
            std::unique_ptr<juce::ComboBox> combo;
            std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;
            std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
        };

        void showParameterMenu (int index)
        {
            juce::PopupMenu menu;
            menu.addItem ("Create automation clip", ctx.automateParameter != nullptr, false, [this, index]
            {
                if (ctx.automateParameter)
                    ctx.automateParameter (effect, index);
            });
            menu.addItem ("Reset to default", [this, index]
            {
                auto* p = effect.getParameters()[index];
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->getDefaultValue());
                p->endChangeGesture();
            });
            menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
        }

        void timerCallback() override
        {
            bool changed = false;
            for (auto& c : controls)
            {
                const float v = c->parameter->getValue();
                if (v != c->lastValue)
                {
                    c->lastValue = v;
                    changed = true;
                }
            }

            if (! meterArea.isEmpty())
            {
                const float target = effect.getReductionDecibels();
                reductionShown = target < reductionShown ? target : reductionShown + (target - reductionShown) * 0.25f;
                changed = true;
            }

            if (changed)
                repaint();
        }

        void paintCurve (juce::Graphics& g)
        {
            const auto area = curveArea.toFloat();
            g.setColour (colours::background);
            g.fillRoundedRectangle (area, 8.0f);

            constexpr double minHz = 20.0, maxHz = 20000.0;
            constexpr float rangeDb = 24.0f;
            const auto xFor = [&] (double hz) { return area.getX() + area.getWidth() * (float) (std::log (hz / minHz) / std::log (maxHz / minHz)); };
            const auto yFor = [&] (float db) { return area.getCentreY() - juce::jlimit (-1.0f, 1.0f, db / rangeDb) * area.getHeight() * 0.45f; };

            g.setColour (colours::lineSoft);
            for (double hz : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0 })
                g.fillRect (xFor (hz), area.getY() + 4.0f, 1.0f, area.getHeight() - 8.0f);
            for (float db : { -12.0f, 12.0f })
                g.fillRect (area.getX() + 4.0f, yFor (db), area.getWidth() - 8.0f, 1.0f);
            g.setColour (colours::line);
            g.fillRect (area.getX() + 4.0f, yFor (0.0f), area.getWidth() - 8.0f, 1.0f);

            g.setColour (colours::textFaint);
            g.setFont (font (8.5f));
            for (auto [hz, label] : { std::pair<double, const char*> { 100.0, "100" }, { 1000.0, "1k" }, { 10000.0, "10k" } })
                g.drawText (label, juce::Rectangle<float> (xFor (hz) + 3.0f, area.getBottom() - 14.0f, 30.0f, 12.0f), juce::Justification::centredLeft, false);

            const int points = juce::jmax (64, (int) area.getWidth());
            if ((int) frequencies.size() != points)
            {
                frequencies.resize ((size_t) points);
                for (int i = 0; i < points; ++i)
                    frequencies[(size_t) i] = minHz * std::pow (maxHz / minHz, (double) i / (points - 1));
            }
            effect.computeResponse (frequencies, response);

            juce::Path curve;
            for (int i = 0; i < points; ++i)
            {
                const float x = xFor (frequencies[(size_t) i]);
                const float y = yFor (response[(size_t) i]);
                if (i == 0) curve.startNewSubPath (x, y);
                else        curve.lineTo (x, y);
            }

            juce::Path fill (curve);
            fill.lineTo (area.getRight(), yFor (0.0f));
            fill.lineTo (area.getX(), yFor (0.0f));
            fill.closeSubPath();

            juce::Graphics::ScopedSaveState state (g);
            g.reduceClipRegion (curveArea);
            g.setColour (colours::accent.withAlpha (0.12f));
            g.fillPath (fill);
            g.setColour (colours::accent);
            g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved));
        }

        void paintMeter (juce::Graphics& g)
        {
            auto area = meterArea.toFloat();
            g.setColour (colours::textDim);
            g.setFont (font (9.5f, Weight::medium));
            g.drawText ("GAIN REDUCTION", area.removeFromLeft (120.0f), juce::Justification::centredLeft, false);

            auto bar = area.withSizeKeepingCentre (area.getWidth() - 70.0f, 8.0f).withX (area.getX());
            g.setColour (colours::control);
            g.fillRoundedRectangle (bar, 4.0f);

            const float amount = juce::jlimit (0.0f, 1.0f, -reductionShown / 24.0f);
            g.setColour (colours::accent);
            g.fillRoundedRectangle (bar.withLeft (bar.getRight() - bar.getWidth() * amount), 4.0f);

            g.setColour (colours::text);
            g.drawText (juce::String (reductionShown, 1) + " dB", area.withLeft (bar.getRight() + 8.0f), juce::Justification::centredLeft, false);
        }

        engine::builtin::Effect& effect;
        Context& ctx;
        std::vector<std::unique_ptr<Control>> controls;
        std::vector<Control*> knobs, toggles, choices;
        juce::Rectangle<int> curveArea, meterArea;
        std::vector<double> frequencies;
        std::vector<float> response;
        float reductionShown = 0.0f;
    };
}

void installEffectEditors (Context& ctx)
{
    engine::builtin::Effect::editorFactory = [&ctx] (engine::builtin::Effect& effect) -> juce::AudioProcessorEditor*
    {
        return new EffectEditor (effect, ctx);
    };
}

void removeEffectEditors()
{
    engine::builtin::Effect::editorFactory = nullptr;
}
}
