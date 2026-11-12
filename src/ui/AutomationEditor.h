#pragma once

#include "ui/Widgets.h"
#include "app/Context.h"

namespace harmony::ui
{
std::unique_ptr<juce::Component> createAutomationEditor (Context& context, juce::ValueTree clip);
juce::String automationTargetName (Context& context, const juce::ValueTree& clip);
}
