#pragma once

#include "ui/Widgets.h"
#include "app/Context.h"
#include "app/CommandIds.h"
#include "engine/Renderer.h"

namespace harmony::ui
{
void askForText (const juce::String& title, const juce::String& label, const juce::String& initial,
                 std::function<void (const juce::String&)> onOk);

std::unique_ptr<juce::Component> createChordGeneratorPanel (Context& ctx, std::function<void (const std::vector<NoteData>&, bool replace)> apply);
std::unique_ptr<juce::Component> createDrumGeneratorPanel (Context& ctx);
std::unique_ptr<juce::Component> createRenderPanel (Context& ctx, std::function<void (engine::RenderSettings, bool minimise)> start);
std::unique_ptr<juce::Component> createSettingsPanel (Context& ctx, std::function<void()> scanPlugins, int page = 0);
std::unique_ptr<juce::Component> createHistoryPanel (Context& ctx, std::function<void (const juce::File&)> restore);
std::unique_ptr<juce::Component> createAboutPanel();
std::unique_ptr<juce::Component> createShortcutsPanel (juce::ApplicationCommandManager& commands);
}
