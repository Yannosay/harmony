#include "model/Automation.h"

namespace harmony::automation
{
Target parse (const juce::String& text)
{
    const auto parts = juce::StringArray::fromTokens (text, ":", {});
    if (parts.size() < 3)
        return {};

    Target t;
    t.id = parts[1].getIntValue();
    const auto& what = parts[2];

    if (parts[0] == "ch")
    {
        if (what == "vol")        t.kind = Kind::channelVolume;
        else if (what == "pan")   t.kind = Kind::channelPan;
        else if (what == "param") t.kind = Kind::channelParameter;
    }
    else if (parts[0] == "mix")
    {
        if (what == "vol")        t.kind = Kind::insertVolume;
        else if (what == "pan")   t.kind = Kind::insertPan;
    }
    else if (parts[0] == "fx" && what == "param")
    {
        t.kind = Kind::slotParameter;
    }

    if (t.kind == Kind::slotParameter || t.kind == Kind::channelParameter)
    {
        t.parameter = parts.size() > 3 && parts[3].containsOnly ("0123456789") ? parts[3].getIntValue() : -1;
        if (t.parameter < 0)
            return {};
    }

    return t;
}

juce::String toString (const Target& t)
{
    switch (t.kind)
    {
        case Kind::channelVolume:    return "ch:" + juce::String (t.id) + ":vol";
        case Kind::channelPan:       return "ch:" + juce::String (t.id) + ":pan";
        case Kind::insertVolume:     return "mix:" + juce::String (t.id) + ":vol";
        case Kind::insertPan:        return "mix:" + juce::String (t.id) + ":pan";
        case Kind::slotParameter:    return "fx:" + juce::String (t.id) + ":param:" + juce::String (t.parameter);
        case Kind::channelParameter: return "ch:" + juce::String (t.id) + ":param:" + juce::String (t.parameter);
        case Kind::none:             break;
    }
    return {};
}

float shape (float t, float tension) noexcept
{
    t = juce::jlimit (0.0f, 1.0f, t);
    if (std::abs (tension) < 1.0e-3f)
        return t;

    return std::pow (t, std::pow (2.0f, -tension * 3.0f));
}

float valueAt (const std::vector<Point>& points, double time) noexcept
{
    if (points.empty())
        return 0.0f;
    if (time <= points.front().time)
        return points.front().value;
    if (time >= points.back().time)
        return points.back().value;

    const auto next = std::upper_bound (points.begin(), points.end(), time, [] (double t, const Point& p) { return t < p.time; });
    const auto& b = *next;
    const auto& a = *(next - 1);
    const double span = b.time - a.time;
    if (span <= 0.0)
        return b.value;

    return a.value + (b.value - a.value) * shape ((float) ((time - a.time) / span), a.tension);
}

std::vector<Point> readPoints (const juce::ValueTree& clip)
{
    std::vector<Point> points;
    for (auto p : clip)
        if (p.hasType (id::POINT))
            points.push_back ({ juce::jmax (0.0, (double) p[id::time]),
                                juce::jlimit (0.0f, 1.0f, (float) p[id::value]),
                                juce::jlimit (-1.0f, 1.0f, (float) p.getProperty (id::tension, 0.0f)) });

    std::stable_sort (points.begin(), points.end(), [] (const Point& a, const Point& b) { return a.time < b.time; });
    return points;
}

void writePoints (juce::ValueTree& clip, const std::vector<Point>& points, juce::UndoManager* undo)
{
    for (int i = clip.getNumChildren(); --i >= 0;)
        if (clip.getChild (i).hasType (id::POINT))
            clip.removeChild (i, undo);

    for (auto& p : points)
    {
        juce::ValueTree node (id::POINT);
        node.setProperty (id::time, juce::jmax (0.0, p.time), nullptr);
        node.setProperty (id::value, juce::jlimit (0.0f, 1.0f, p.value), nullptr);
        node.setProperty (id::tension, juce::jlimit (-1.0f, 1.0f, p.tension), nullptr);
        clip.appendChild (node, undo);
    }
}

float faderFromNormalised (float v) noexcept    { return juce::jlimit (0.0f, 1.0f, v) * 1.25f; }
float normalisedFromFader (float fader) noexcept { return juce::jlimit (0.0f, 1.0f, fader / 1.25f); }
float panFromNormalised (float v) noexcept       { return juce::jlimit (0.0f, 1.0f, v) * 2.0f - 1.0f; }
float normalisedFromPan (float pan) noexcept     { return juce::jlimit (0.0f, 1.0f, (pan + 1.0f) * 0.5f); }

float currentNormalisedValue (const Project& project, const Target& t)
{
    switch (t.kind)
    {
        case Kind::channelVolume: return normalisedFromFader ((float) project.findChannel (t.id).getProperty (id::volume, 0.78f));
        case Kind::channelPan:    return normalisedFromPan ((float) project.findChannel (t.id).getProperty (id::pan, 0.0f));
        case Kind::insertVolume:  return normalisedFromFader ((float) project.findInsert (t.id).getProperty (id::volume, 0.8f));
        case Kind::insertPan:     return normalisedFromPan ((float) project.findInsert (t.id).getProperty (id::pan, 0.0f));
        case Kind::slotParameter:
        case Kind::channelParameter:
        case Kind::none:          break;
    }
    return 0.5f;
}

juce::String describeTarget (const Project& project, const Target& t, const juce::String& parameterName)
{
    const auto insertName = [&project] (int index) { return project.findInsert (index)[id::name].toString(); };
    const auto channelName = [&project] (int uid) { return project.findChannel (uid)[id::name].toString(); };

    switch (t.kind)
    {
        case Kind::channelVolume:    return channelName (t.id) + " volume";
        case Kind::channelPan:       return channelName (t.id) + " pan";
        case Kind::insertVolume:     return insertName (t.id) + " volume";
        case Kind::insertPan:        return insertName (t.id) + " pan";
        case Kind::channelParameter: return channelName (t.id) + " " + (parameterName.isNotEmpty() ? parameterName : "parameter " + juce::String (t.parameter + 1));
        case Kind::slotParameter:    return parameterName.isNotEmpty() ? parameterName : "Effect parameter " + juce::String (t.parameter + 1);
        case Kind::none:             break;
    }
    return "Automation";
}

juce::ValueTree addClip (Project& project, juce::ValueTree track, const Target& target, juce::int64 start, juce::int64 length,
                         float initialValue, const juce::String& name)
{
    length = juce::jmax ((juce::int64) 1, length);

    juce::ValueTree clip (id::CLIP);
    clip.setProperty (id::uid, project.allocateUid(), nullptr);
    clip.setProperty (id::kind, kind::automation, nullptr);
    clip.setProperty (id::target, toString (target), nullptr);
    clip.setProperty (id::name, name, nullptr);
    clip.setProperty (id::start, juce::jmax ((juce::int64) 0, start), nullptr);
    clip.setProperty (id::length, length, nullptr);
    clip.setProperty (id::offset, 0, nullptr);
    clip.setProperty (id::muted, false, nullptr);
    writePoints (clip, { { 0.0, initialValue, 0.0f }, { (double) length, initialValue, 0.0f } }, nullptr);
    track.appendChild (clip, project.um());
    return clip;
}
}
