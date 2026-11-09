#pragma once

#include "ui/Widgets.h"
#include "app/Context.h"
#include <map>

namespace harmony::ui
{
class WindowManager final : private juce::Timer
{
public:
    explicit WindowManager (Context& context);
    ~WindowManager() override;

    void openChannel (int channelUid);
    void openSlot (int slotUid);
    void closeAll();

private:
    class PluginWindow;

    void timerCallback() override;
    void openPlugin (const juce::String& key, juce::AudioPluginInstance& plugin, const juce::String& title);

    Context& ctx;
    std::map<juce::String, std::unique_ptr<juce::DocumentWindow>> windows;
};

std::unique_ptr<juce::Component> createSamplerEditor (Context& ctx, int channelUid);
std::unique_ptr<juce::Component> createSoundFontEditor (Context& ctx, int channelUid);
}
