#pragma once

#include "model/Project.h"

namespace harmony::automation
{
enum class Kind { none, channelVolume, channelPan, insertVolume, insertPan, slotParameter, channelParameter };

struct Target
{
    Kind kind = Kind::none;
    int id = 0;
    int parameter = -1;

    bool isValid() const noexcept { return kind != Kind::none; }
    bool controlsGain() const noexcept { return kind == Kind::channelVolume || kind == Kind::channelPan || kind == Kind::insertVolume || kind == Kind::insertPan; }
    bool operator== (const Target& other) const noexcept { return kind == other.kind && id == other.id && parameter == other.parameter; }
};

struct Point
{
    double time = 0.0;
    float value = 0.0f;
    float tension = 0.0f;
};

Target parse (const juce::String& text);
juce::String toString (const Target& target);

float shape (float t, float tension) noexcept;
float valueAt (const std::vector<Point>& points, double time) noexcept;
std::vector<Point> readPoints (const juce::ValueTree& clip);
void writePoints (juce::ValueTree& clip, const std::vector<Point>& points, juce::UndoManager* undo);

float faderFromNormalised (float v) noexcept;
float normalisedFromFader (float fader) noexcept;
float panFromNormalised (float v) noexcept;
float normalisedFromPan (float pan) noexcept;

float currentNormalisedValue (const Project& project, const Target& target);
juce::String describeTarget (const Project& project, const Target& target, const juce::String& parameterName = {});

juce::ValueTree addClip (Project& project, juce::ValueTree track, const Target& target, juce::int64 start, juce::int64 length,
                         float initialValue, const juce::String& name);
}
