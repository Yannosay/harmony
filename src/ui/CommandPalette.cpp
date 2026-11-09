#include "ui/CommandPalette.h"

namespace harmony::ui
{
int CommandPalette::fuzzyScore (const juce::String& query, const juce::String& text)
{
    if (query.isEmpty())
        return 1;

    const auto q = query.toLowerCase();
    const auto t = text.toLowerCase();

    if (t.startsWith (q))
        return 1000 - t.length();
    if (t.contains (" " + q))
        return 800 - t.length();
    if (t.contains (q))
        return 600 - t.length();

    int score = 0, ti = 0, streak = 0;
    for (int qi = 0; qi < q.length(); ++qi)
    {
        bool found = false;
        while (ti < t.length())
        {
            if (t[ti++] == q[qi])
            {
                found = true;
                streak = (ti >= 2 && qi > 0 && t[ti - 2] == q[qi - 1]) ? streak + 1 : 0;
                score += 10 + streak * 5 + (ti == 1 || t[ti - 2] == ' ' ? 15 : 0);
                break;
            }
        }
        if (! found)
            return 0;
    }
    return score;
}

CommandPalette::CommandPalette (juce::ApplicationCommandManager& c)
    : commands (c)
{
    setSize (600, 440);
    setWantsKeyboardFocus (false);

    for (auto category : commands.getCommandCategories())
        for (auto commandId : commands.getCommandsInCategory (category))
            if (auto* info = commands.getCommandForID (commandId))
            {
                juce::StringArray keys;
                for (auto& k : commands.getKeyMappings()->getKeyPressesAssignedToCommand (commandId))
                    keys.add (k.getTextDescriptionWithIcons());
                all.push_back ({ commandId, info->shortName, category, keys.joinIntoString (" / "), 0 });
            }

    input.setFont (font (14.0f));
    input.setTextToShowWhenEmpty ("Type a command...", colours::textFaint);
    input.setIndents (14, 10);
    input.onTextChange = [this] { filter(); };
    input.onReturnKey = [this] { run (list.getSelectedRow()); };
    input.onEscapeKey = [this] { if (onDone) onDone(); };
    input.addKeyListener (this);
    addAndMakeVisible (input);

    list.setModel (this);
    list.setRowHeight (34);
    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (list);
    filter();
}

void CommandPalette::visibilityChanged()
{
    if (isShowing())
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<CommandPalette> (this)] { if (safe) safe->input.grabKeyboardFocus(); });
}

void CommandPalette::filter()
{
    const auto query = input.getText().trim();
    visible.clear();

    for (auto item : all)
    {
        item.score = juce::jmax (fuzzyScore (query, item.name), fuzzyScore (query, item.category + " " + item.name) / 2);
        if (item.score > 0)
            visible.push_back (item);
    }

    if (query.isNotEmpty())
        std::stable_sort (visible.begin(), visible.end(), [] (const Item& a, const Item& b) { return a.score > b.score; });

    list.updateContent();
    list.selectRow (0);
    list.repaint();
}

void CommandPalette::run (int row)
{
    if (! juce::isPositiveAndBelow (row, (int) visible.size()))
        return;

    const auto command = visible[(size_t) row].command;
    if (onDone)
        onDone();
    juce::MessageManager::callAsync ([&cm = commands, command] { cm.invokeDirectly (command, true); });
}

bool CommandPalette::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    if (key == juce::KeyPress::downKey || key == juce::KeyPress::upKey)
    {
        const int delta = key == juce::KeyPress::downKey ? 1 : -1;
        list.selectRow (juce::jlimit (0, juce::jmax (0, (int) visible.size() - 1), list.getSelectedRow() + delta));
        return true;
    }
    return false;
}

void CommandPalette::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (! juce::isPositiveAndBelow (row, (int) visible.size()))
        return;

    const auto& item = visible[(size_t) row];
    if (selected)
    {
        g.setColour (colours::accent.withAlpha (0.14f));
        g.fillRoundedRectangle (juce::Rectangle<float> (4.0f, 1.0f, (float) w - 8.0f, (float) h - 2.0f), 6.0f);
    }

    g.setColour (colours::textFaint);
    g.setFont (font (9.0f, Weight::medium));
    g.drawText (item.category.toUpperCase(), 16, 0, 90, h, juce::Justification::centredLeft, true);

    g.setColour (selected ? colours::text : colours::textDim.brighter (0.3f));
    g.setFont (font (12.0f, selected ? Weight::medium : Weight::regular));
    g.drawText (item.name, 110, 0, w - 280, h, juce::Justification::centredLeft, true);

    g.setColour (colours::accent.withAlpha (0.85f));
    g.setFont (font (10.0f, Weight::medium));
    g.drawText (item.keys, w - 170, 0, 154, h, juce::Justification::centredRight, true);
}

void CommandPalette::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    run (row);
}

void CommandPalette::returnKeyPressed (int row)
{
    run (row);
}

void CommandPalette::paint (juce::Graphics&) {}

void CommandPalette::resized()
{
    auto r = getLocalBounds().reduced (14, 10);
    input.setBounds (r.removeFromTop (42));
    r.removeFromTop (8);
    list.setBounds (r);
}
}
