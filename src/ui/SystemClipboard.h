#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <optional>

namespace harmony::ui::clipboard
{
juce::uint32 sequenceNumber();
std::optional<juce::MidiFile> readMidi();
}
