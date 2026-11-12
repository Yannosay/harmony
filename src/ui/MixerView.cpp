#include "ui/MixerView.h"
#include "ui/Dialogs.h"
#include "engine/AudioEngine.h"
#include "engine/PluginManager.h"
#include "model/Automation.h"
#include <map>

namespace harmony::ui
{
namespace
{
    constexpr int stripWidth = 92;
    constexpr int slotHeight = 19;
}

class MixerStrip final : public juce::Component
{
public:
    MixerStrip (MixerView& v, juce::ValueTree t)
        : view (v), insert (std::move (t)), index ((int) insert[id::index])
    {
        auto* undo = view.ctx.project.um();
        fader.bind (insert, id::volume, undo);
        pan.bind (insert, id::pan, undo);
        pan.describe = [] (double value) { return value == 0.0 ? juce::String ("Centre") : juce::String (std::abs (value) * 100.0, 0) + (value < 0 ? "% L" : "% R"); };
        addAndMakeVisible (fader);
        addAndMakeVisible (pan);
        addAndMakeVisible (meter);

        for (auto* b : { &muteButton, &soloButton })
        {
            b->setClickingTogglesState (false);
            addAndMakeVisible (b);
        }
        muteButton.setColour (juce::TextButton::buttonOnColourId, colours::textFaint);
        soloButton.setColour (juce::TextButton::buttonOnColourId, colours::warning);
        muteButton.onClick = [this] { toggle (id::mute); };
        soloButton.onClick = [this] { toggle (id::solo); };
        keepFocusOnEditors ({ &muteButton, &soloButton });
        muteButton.setTooltip ("Mute");
        soloButton.setTooltip ("Solo");
        fader.onContextMenu = [this] { showAutomationMenu (automation::Kind::insertVolume); };
        pan.onContextMenu = [this] { showAutomationMenu (automation::Kind::insertPan); };
        if (index == 0)
            soloButton.setVisible (false);

        refresh();
    }

    void toggle (const juce::Identifier& property)
    {
        view.ctx.project.beginTransaction ("Mixer");
        insert.setProperty (property, ! (bool) insert[property], view.ctx.project.um());
    }

    void refresh()
    {
        muteButton.setToggleState ((bool) insert[id::mute], juce::dontSendNotification);
        soloButton.setToggleState ((bool) insert[id::solo], juce::dontSendNotification);
        repaint();
    }

    void showAutomationMenu (automation::Kind kind)
    {
        juce::PopupMenu menu;
        const bool volume = kind == automation::Kind::insertVolume;
        menu.addItem ("Create automation clip", view.ctx.createAutomation != nullptr, false, [this, kind, volume]
        {
            if (view.ctx.createAutomation)
                view.ctx.createAutomation ({ kind, index, -1 }, insert[id::name].toString() + (volume ? " volume" : " pan"));
        });
        menu.addItem (volume ? "Reset volume" : "Centre pan", [this, volume]
        {
            view.ctx.project.beginTransaction (volume ? "Reset volume" : "Centre pan");
            insert.setProperty (volume ? id::volume : id::pan, volume ? 0.8f : 0.0f, view.ctx.project.um());
        });
        menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
    }

    juce::Rectangle<int> nameArea() const { return { 4, 4, getWidth() - 8, 26 }; }

    juce::Rectangle<int> slotArea (int slot) const
    {
        return { 6, 36 + slot * slotHeight, getWidth() - 12, slotHeight - 2 };
    }

    juce::ValueTree slotTree (int slot) const
    {
        for (auto s : insert)
            if ((int) s[id::index] == slot)
                return s;
        return {};
    }

    int firstFreeSlot() const
    {
        for (int s = 0; s < numFxSlots; ++s)
            if (! slotTree (s).isValid())
                return s;
        return -1;
    }

    int slotAt (juce::Point<int> p) const
    {
        for (int s = 0; s < numFxSlots; ++s)
            if (slotArea (s).expanded (0, 1).contains (p))
                return s;
        return -1;
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (6, 0);
        r.removeFromTop (36 + numFxSlots * slotHeight + 6);
        pan.setBounds (r.removeFromTop (34).withSizeKeepingCentre (32, 32));
        auto buttons = r.removeFromTop (24);
        muteButton.setBounds (buttons.removeFromLeft (buttons.getWidth() / 2).reduced (2, 1));
        soloButton.setBounds (buttons.reduced (2, 1));
        r.removeFromTop (6);
        r.removeFromBottom (22);
        meter.setBounds (r.removeFromRight (12));
        fader.setBounds (r);
    }

    void paint (juce::Graphics& g) override
    {
        const bool isSelected = view.getSelectedInsert() == index;
        const bool isDropTarget = view.getDropHighlight() == index;
        auto r = getLocalBounds().toFloat().reduced (2.0f, 0.0f);
        g.setColour (isDropTarget ? colours::accent.withAlpha (0.14f) : isSelected ? colours::raised.brighter (0.05f) : colours::raised);
        g.fillRoundedRectangle (r, 8.0f);
        if (isSelected || isDropTarget)
        {
            g.setColour (colours::accent.withAlpha (isDropTarget ? 1.0f : 0.6f));
            g.drawRoundedRectangle (r.reduced (0.5f), 8.0f, isDropTarget ? 1.5f : 1.0f);
        }

        g.setColour (index == 0 ? colours::accent : colours::text);
        g.setFont (font (10.5f, Weight::semibold));
        g.drawFittedText (insert[id::name].toString(), nameArea(), juce::Justification::centred, 1);

        for (int s = 0; s < numFxSlots; ++s)
        {
            auto area = slotArea (s).toFloat();
            auto slot = slotTree (s);
            const bool filled = slot.isValid();
            const bool bypass = filled && (bool) slot[id::bypass];

            g.setColour (filled ? colours::control.brighter (0.06f) : colours::control.withAlpha (0.55f));
            g.fillRoundedRectangle (area, 4.0f);

            if (filled)
            {
                auto desc = engine::PluginManager::fromXmlString (slot[id::pluginDesc]);
                const bool loaded = view.ctx.engine.findSlotNode (slot[id::uid]) != nullptr;
                g.setColour (bypass ? colours::textFaint : (loaded ? colours::accent : colours::warning));
                g.fillEllipse (area.getX() + 5.0f, area.getCentreY() - 2.5f, 5.0f, 5.0f);
                g.setColour (bypass ? colours::textFaint : colours::text);
                g.setFont (font (9.0f, Weight::medium));
                auto label = desc.has_value() ? desc->name : juce::String ("Unknown");
                if (desc.has_value() && desc->pluginFormatName == "Harmony")
                    label = label.fromFirstOccurrenceOf ("Harmony ", false, false);
                g.drawText (label, area.withTrimmedLeft (14.0f).withTrimmedRight (3.0f), juce::Justification::centredLeft, true);
            }
            else if (s == firstFreeSlot())
            {
                g.setColour (colours::textFaint.withAlpha (0.8f));
                g.setFont (font (8.5f));
                g.drawText ("+ Add effect", area.withTrimmedLeft (6.0f), juce::Justification::centredLeft, false);
            }
        }

        g.setColour (colours::textFaint);
        g.setFont (font (9.0f, Weight::medium));
        g.drawText (index == 0 ? juce::String ("M") : juce::String (index), getLocalBounds().removeFromBottom (20), juce::Justification::centred, false);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        view.selectInsert (index);
        const auto p = e.getPosition();

        for (int s = 0; s < numFxSlots; ++s)
        {
            if (! slotArea (s).contains (p))
                continue;

            auto slot = slotTree (s);
            if (! slot.isValid() || e.mods.isPopupMenu())
                showSlotMenu (s, slot);
            else if (view.ctx.openSlotEditor)
                view.ctx.openSlotEditor (slot[id::uid]);
            return;
        }

        if (nameArea().contains (p) && e.mods.isPopupMenu())
            showInsertMenu();
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (nameArea().contains (e.getPosition()) && index > 0)
            renameInsert();
    }

    void renameInsert()
    {
        askForText ("Rename insert", "Name", insert[id::name].toString(), [this] (const juce::String& text)
        {
            if (text.trim().isNotEmpty())
            {
                view.ctx.project.beginTransaction ("Rename insert");
                insert.setProperty (id::name, text.trim(), view.ctx.project.um());
            }
        });
    }

    void showInsertMenu()
    {
        juce::PopupMenu menu;
        menu.addItem ("Rename", index > 0, false, [this] { renameInsert(); });
        menu.addItem ("Reset fader and pan", [this]
        {
            view.ctx.project.beginTransaction ("Reset insert");
            insert.setProperty (id::volume, 0.8f, view.ctx.project.um());
            insert.setProperty (id::pan, 0.0f, view.ctx.project.um());
        });
        menu.addItem ("Remove all effects", insert.getNumChildren() > 0, false, [this]
        {
            view.ctx.project.beginTransaction ("Remove effects");
            insert.removeAllChildren (view.ctx.project.um());
        });
        menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
    }

    void showSlotMenu (int s, juce::ValueTree slot)
    {
        auto& ctx = view.ctx;
        juce::PopupMenu menu;

        if (slot.isValid())
        {
            menu.addItem ("Open editor", [&ctx, slot] { if (ctx.openSlotEditor) ctx.openSlotEditor (slot[id::uid]); });
            if (auto* plugin = ctx.sync.pluginForSlot (slot[id::uid]))
            {
                juce::PopupMenu automate;
                const auto& params = plugin->getParameters();
                for (int i = 0; i < juce::jmin (params.size(), 512); ++i)
                    if (params[i]->isAutomatable())
                        automate.addItem (params[i]->getName (48), [&ctx, plugin, i] { if (ctx.automateParameter) ctx.automateParameter (*plugin, i); });
                menu.addSubMenu ("Automate parameter", automate, automate.getNumItems() > 0);
            }
            menu.addItem ("Bypass", true, (bool) slot[id::bypass], [&ctx, slot] () mutable
            {
                ctx.project.beginTransaction ("Bypass effect");
                slot.setProperty (id::bypass, ! (bool) slot[id::bypass], ctx.project.um());
            });
            menu.addItem ("Move up", s > 0, false, [this, s] { swapSlots (s, s - 1); });
            menu.addItem ("Move down", s < numFxSlots - 1, false, [this, s] { swapSlots (s, s + 1); });
            menu.addItem ("Remove", [this, slot]
            {
                view.ctx.project.beginTransaction ("Remove effect");
                insert.removeChild (slot, view.ctx.project.um());
            });
            menu.addSeparator();
        }

        auto effects = MixerView::buildEffectMenu (ctx, [&ctx, idx = index, s] (const juce::String& identifier)
        {
            if (ctx.addEffect)
                ctx.addEffect (idx, identifier, s);
        });

        if (slot.isValid())
            menu.addSubMenu ("Replace with", effects);
        else
            menu = effects;
        menu.showMenuAsync (juce::PopupMenu::Options().withMousePosition());
    }

    void swapSlots (int a, int b)
    {
        auto sa = slotTree (a), sb = slotTree (b);
        view.ctx.project.beginTransaction ("Reorder effects");
        if (sa.isValid()) sa.setProperty (id::index, b, view.ctx.project.um());
        if (sb.isValid()) sb.setProperty (id::index, a, view.ctx.project.um());
    }

    void updateMeter()
    {
        if (auto* node = view.ctx.engine.getInsertNode (index))
        {
            meter.push (node->meter.takeLeft(), node->meter.takeRight());
            const float autoVolume = node->gain.automatedVolume.load();
            const float autoPan = node->gain.automatedPan.load();
            fader.showTransientValue (autoVolume >= 0.0f ? autoVolume : (float) insert.getProperty (id::volume, 0.8f));
            pan.showTransientValue (autoPan >= -1.0f ? autoPan : (float) insert.getProperty (id::pan, 0.0f));
        }
    }

    MixerView& view;
    juce::ValueTree insert;
    const int index;
    Fader fader;
    Knob pan { -1.0, 1.0, 0.0, "Pan" };
    LevelMeter meter;
    juce::TextButton muteButton { "M" }, soloButton { "S" };
};

juce::PopupMenu MixerView::buildEffectMenu (Context& ctx, std::function<void (const juce::String&)> onChosen)
{
    juce::PopupMenu menu;
    menu.addSectionHeader ("Harmony effects");

    for (auto& d : engine::PluginManager::getBuiltinEffects())
        menu.addItem (d.name.fromFirstOccurrenceOf ("Harmony ", false, false) + "   " + d.category.toLowerCase(),
                      [onChosen, identifier = d.createIdentifierString()] { onChosen (identifier); });

    std::map<juce::String, juce::PopupMenu> byVendor;
    for (auto& d : ctx.plugins.getPlugins (false))
        byVendor[d.manufacturerName.isNotEmpty() ? d.manufacturerName : juce::String ("Other")]
            .addItem (d.name + "  (" + d.pluginFormatName + ")", [onChosen, identifier = d.createIdentifierString()] { onChosen (identifier); });

    menu.addSeparator();
    menu.addSectionHeader ("Plugins");
    for (auto& [vendor, sub] : byVendor)
        menu.addSubMenu (vendor, sub);
    if (byVendor.empty())
        menu.addItem ("No plugin effects yet. Scan for plugins...", [&ctx] { ctx.commands.invokeDirectly (cmd::scanPlugins, true); });

    return menu;
}

MixerStrip* MixerView::stripAt (juce::Point<int> positionInView) const
{
    for (auto& s : strips)
        if (s->getBoundsInParent().contains (holder.getLocalPoint (this, positionInView)))
            return s.get();
    return nullptr;
}

bool MixerView::isInterestedInDragSource (const SourceDetails& details)
{
    return details.description.toString().startsWith ("plugin:");
}

void MixerView::itemDragMove (const SourceDetails& details)
{
    auto* strip = stripAt (details.localPosition);
    const int target = strip != nullptr ? strip->index : -1;
    if (target != dropTarget)
    {
        dropTarget = target;
        for (auto& s : strips)
            s->repaint();
    }
}

void MixerView::itemDragExit (const SourceDetails&)
{
    dropTarget = -1;
    for (auto& s : strips)
        s->repaint();
}

void MixerView::itemDropped (const SourceDetails& details)
{
    dropTarget = -1;
    for (auto& s : strips)
        s->repaint();

    auto* strip = stripAt (details.localPosition);
    if (strip == nullptr || ! ctx.addEffect)
        return;

    const auto local = strip->getLocalPoint (this, details.localPosition);
    const int slot = strip->slotAt (local);
    selectInsert (strip->index);
    ctx.addEffect (strip->index, details.description.toString().fromFirstOccurrenceOf ("plugin:", false, false), slot);
}

MixerView::MixerView (Context& c)
    : ctx (c)
{
    addAndMakeVisible (viewport);
    viewport.setViewedComponent (&holder, false);
    viewport.setScrollBarsShown (false, true);
    ctx.project.getState().addListener (this);
    rebuild();
    startTimerHz (30);
}

MixerView::~MixerView()
{
    ctx.project.getState().removeListener (this);
}

void MixerView::rebuild()
{
    strips.clear();
    for (auto insert : ctx.project.mixerTree())
    {
        auto strip = std::make_unique<MixerStrip> (*this, insert);
        holder.addAndMakeVisible (*strip);
        strips.push_back (std::move (strip));
    }
    resized();
}

void MixerView::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
}

void MixerView::resized()
{
    viewport.setBounds (getLocalBounds());
    const int h = juce::jmax (360, viewport.getHeight() - viewport.getScrollBarThickness());
    holder.setSize ((int) strips.size() * stripWidth + 16, h);

    int x = 8;
    for (auto& s : strips)
    {
        s->setBounds (x, 6, stripWidth - 4, h - 12);
        x += stripWidth + (s.get() == strips.front().get() ? 8 : 0);
    }
    holder.setSize (x + 8, h);
}

void MixerView::selectInsert (int index)
{
    selected = index;
    for (auto& s : strips)
        s->repaint();
}

void MixerView::focusInsert (int index)
{
    selectInsert (index);
    for (auto& s : strips)
        if (s->index == index)
            viewport.setViewPosition (juce::jmax (0, s->getX() - viewport.getWidth() / 2 + stripWidth / 2), 0);
}

void MixerView::timerCallback()
{
    if (! isShowing())
        return;

    for (auto& s : strips)
        s->updateMeter();
}

void MixerView::valueTreeChildAdded (juce::ValueTree& parent, juce::ValueTree& child)
{
    if (child.hasType (id::MIXER) || child.hasType (id::INSERT))
        rebuild();
    else if (parent.hasType (id::INSERT))
        for (auto& s : strips)
            s->repaint();
}

void MixerView::valueTreeChildRemoved (juce::ValueTree& parent, juce::ValueTree& child, int)
{
    valueTreeChildAdded (parent, child);
}

void MixerView::valueTreePropertyChanged (juce::ValueTree& t, const juce::Identifier&)
{
    if (t.hasType (id::INSERT))
    {
        for (auto& s : strips)
            if (s->index == (int) t[id::index])
                s->refresh();
    }
    else if (t.hasType (id::SLOT))
    {
        for (auto& s : strips)
            s->repaint();
    }
}

void MixerView::valueTreeRedirected (juce::ValueTree&)
{
    rebuild();
}
}
