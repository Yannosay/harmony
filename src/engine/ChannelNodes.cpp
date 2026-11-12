#include "engine/ChannelNodes.h"
#include "tsf/tsf.h"

namespace harmony::engine
{
namespace
{
    constexpr int maxMidiEventsPerBlock = 4096;

    void prepareStereoBuses (juce::AudioPluginInstance& plugin, bool wantsInput)
    {
        plugin.enableAllBuses();
        auto layout = plugin.getBusesLayout();

        if (! layout.outputBuses.isEmpty())
            layout.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
        if (wantsInput && ! layout.inputBuses.isEmpty())
            layout.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();

        if (plugin.checkBusesLayoutSupported (layout))
            plugin.setBusesLayout (layout);
    }

    inline float hermite (const float* data, int length, double position) noexcept
    {
        const int i = (int) position;
        const float f = (float) (position - (double) i);
        auto at = [data, length] (int index) noexcept { return index >= 0 && index < length ? data[index] : 0.0f; };
        const float x0 = at (i - 1), x1 = at (i), x2 = at (i + 1), x3 = at (i + 2);
        const float c1 = 0.5f * (x2 - x0);
        const float c2 = x0 - 2.5f * x1 + 2.0f * x2 - 0.5f * x3;
        const float c3 = 0.5f * (x3 - x0) + 1.5f * (x1 - x2);
        return ((c3 * f + c2) * f + c1) * f + x1;
    }
}

ChannelNode::ChannelNode (int u, juce::String sig)
    : uid (u), signature (std::move (sig))
{
    pending.ensureSize (maxMidiEventsPerBlock * 4);
}

void ChannelNode::prepare (double sr, int maxBlockSize)
{
    sampleRate = sr;
    output.setSize (2, maxBlockSize, false, true, true);
    gain.prepare (sr);
    held.reset();
    pending.clear();
    numClips = 0;
    prepareNode (sr, maxBlockSize);
}

void ChannelNode::addNoteOn (int key, float velocity, int sampleOffset)
{
    if (key < 0 || key > 127)
        return;

    if (held[(size_t) key])
        pending.addEvent (juce::MidiMessage::noteOff (1, key), sampleOffset);

    pending.addEvent (juce::MidiMessage::noteOn (1, key, juce::jlimit (0.01f, 1.0f, velocity)), sampleOffset);
    held.set ((size_t) key);
}

void ChannelNode::addNoteOff (int key, int sampleOffset)
{
    if (key < 0 || key > 127 || ! held[(size_t) key])
        return;

    pending.addEvent (juce::MidiMessage::noteOff (1, key), sampleOffset);
    held.reset ((size_t) key);
}

void ChannelNode::addMessage (const juce::MidiMessage& message, int sampleOffset)
{
    if (message.isNoteOn())
        addNoteOn (message.getNoteNumber(), message.getFloatVelocity(), sampleOffset);
    else if (message.isNoteOff())
        addNoteOff (message.getNoteNumber(), sampleOffset);
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        releaseHeldNotes (sampleOffset);
    else
        pending.addEvent (message, sampleOffset);
}

void ChannelNode::releaseHeldNotes (int sampleOffset)
{
    if (held.none())
        return;

    for (int key = 0; key < 128; ++key)
        if (held[(size_t) key])
            pending.addEvent (juce::MidiMessage::noteOff (1, key), sampleOffset);

    held.reset();
}

void ChannelNode::addClipSegment (const ClipSegment& segment) noexcept
{
    if (numClips < (int) clips.size())
        clips[(size_t) numClips++] = segment;
}

void ChannelNode::process (int numSamples)
{
    output.clear (0, numSamples);
    renderNode (output, numSamples, pending);

    for (int i = 0; i < numClips; ++i)
        renderClip (output, clips[(size_t) i]);

    pending.clear();
    numClips = 0;

    gain.process (output, numSamples);
    meter.push (output, numSamples);
}

PluginChannelNode::PluginChannelNode (int u, juce::String sig, std::unique_ptr<juce::AudioPluginInstance> instance)
    : ChannelNode (u, std::move (sig)), plugin (std::move (instance))
{
    jassert (plugin != nullptr);
    midiCopy.ensureSize (maxMidiEventsPerBlock * 4);
    prepareStereoBuses (*plugin, false);
}

PluginChannelNode::~PluginChannelNode()
{
    plugin->releaseResources();
}

void PluginChannelNode::prepareNode (double sr, int maxBlockSize)
{
    plugin->releaseResources();
    plugin->setRateAndBufferSizeDetails (sr, maxBlockSize);
    plugin->prepareToPlay (sr, maxBlockSize);
    const int channels = juce::jmax (2, plugin->getTotalNumInputChannels(), plugin->getTotalNumOutputChannels());
    work.setSize (channels, maxBlockSize, false, true, true);
}

void PluginChannelNode::renderNode (juce::AudioBuffer<float>& out, int numSamples, const juce::MidiBuffer& midi)
{
    midiCopy.clear();
    midiCopy.addEvents (midi, 0, numSamples, 0);

    juce::AudioBuffer<float> view (work.getArrayOfWritePointers(), work.getNumChannels(), numSamples);
    view.clear();

    {
        const juce::ScopedLock lock (plugin->getCallbackLock());
        if (plugin->isSuspended())
            return;

        plugin->processBlock (view, midiCopy);
    }

    const int outs = plugin->getTotalNumOutputChannels();
    if (outs <= 0)
        return;

    out.copyFrom (0, 0, view, 0, 0, numSamples);
    out.copyFrom (1, 0, view, outs > 1 ? 1 : 0, 0, numSamples);
}

SoundFontChannelNode::SoundFontChannelNode (int u, juce::String sig, tsf* f)
    : ChannelNode (u, std::move (sig)), font (f)
{
    jassert (font != nullptr);
}

SoundFontChannelNode::~SoundFontChannelNode()
{
    tsf_close (font);
}

tsf* SoundFontChannelNode::loadFont (const juce::File& file, juce::String& error)
{
    juce::MemoryBlock data;
    if (! file.loadFileAsData (data) || data.getSize() < 12 || data.getSize() > (size_t) std::numeric_limits<int>::max())
    {
        error = "Could not read " + file.getFileName();
        return nullptr;
    }

    auto* loaded = tsf_load_memory (data.getData(), (int) data.getSize());
    if (loaded == nullptr || tsf_get_presetcount (loaded) <= 0)
    {
        if (loaded != nullptr)
            tsf_close (loaded);
        error = file.getFileName() + " is not a valid SoundFont (.sf2).";
        return nullptr;
    }
    return loaded;
}

juce::StringArray SoundFontChannelNode::getPresetNames() const
{
    juce::StringArray names;
    const int count = tsf_get_presetcount (font);
    for (int i = 0; i < count; ++i)
        names.add (juce::String::fromUTF8 (tsf_get_presetname (font, i)));
    return names;
}

void SoundFontChannelNode::prepareNode (double sr, int maxBlockSize)
{
    tsf_set_output (font, TSF_STEREO_INTERLEAVED, (int) sr, 0.0f);
    tsf_set_max_voices (font, 256);
    tsf_channel_set_presetindex (font, 0, juce::jmax (0, preset.load()));
    appliedPreset = preset.load();

    if (interleavedCapacity < maxBlockSize * 2)
    {
        interleaved.allocate ((size_t) maxBlockSize * 2, true);
        interleavedCapacity = maxBlockSize * 2;
    }
}

void SoundFontChannelNode::renderSpan (juce::AudioBuffer<float>& out, int start, int count)
{
    if (count <= 0)
        return;

    tsf_render_float (font, interleaved.get(), count, 0);
    auto* l = out.getWritePointer (0, start);
    auto* r = out.getWritePointer (1, start);
    const float* src = interleaved.get();

    for (int i = 0; i < count; ++i)
    {
        l[i] = src[i * 2];
        r[i] = src[i * 2 + 1];
    }
}

void SoundFontChannelNode::renderNode (juce::AudioBuffer<float>& out, int numSamples, const juce::MidiBuffer& midi)
{
    const int wanted = preset.load (std::memory_order_relaxed);
    if (wanted != appliedPreset && wanted >= 0 && wanted < tsf_get_presetcount (font))
    {
        tsf_channel_sounds_off_all (font, 0);
        tsf_channel_set_presetindex (font, 0, wanted);
        appliedPreset = wanted;
    }

    int cursor = 0;
    for (const auto metadata : midi)
    {
        const int position = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderSpan (out, cursor, position - cursor);
        cursor = position;

        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            tsf_channel_note_on (font, 0, msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff())
            tsf_channel_note_off (font, 0, msg.getNoteNumber());
        else if (msg.isPitchWheel())
            tsf_channel_set_pitchwheel (font, 0, msg.getPitchWheelValue());
        else if (msg.isController())
            tsf_channel_midi_control (font, 0, msg.getControllerNumber(), msg.getControllerValue());
    }

    renderSpan (out, cursor, numSamples - cursor);
}

SamplerChannelNode::SamplerChannelNode (int u, juce::String sig, std::shared_ptr<const SampleData> s)
    : ChannelNode (u, std::move (sig)), sample (std::move (s))
{
    jassert (sample != nullptr);
}

std::shared_ptr<const SampleData> SamplerChannelNode::loadSample (const juce::File& file, juce::AudioFormatManager& formats, juce::String& error)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
        if (auto stream = file.createInputStream())
            reader.reset (formats.createReaderFor (std::move (stream)));

    if (reader == nullptr)
    {
        error = "Unsupported or unreadable audio file: " + file.getFileName();
        return nullptr;
    }

    const auto maxSamples = (juce::int64) (reader->sampleRate * 60.0 * 30.0);
    if (reader->lengthInSamples <= 0 || reader->lengthInSamples > maxSamples)
    {
        error = file.getFileName() + " is empty or longer than 30 minutes.";
        return nullptr;
    }

    auto data = std::make_shared<SampleData>();
    data->sampleRate = reader->sampleRate;
    const int length = (int) reader->lengthInSamples;
    data->audio.setSize (2, length);
    reader->read (&data->audio, 0, length, 0, true, true);

    if (reader->numChannels == 1)
        data->audio.copyFrom (1, 0, data->audio, 0, 0, length);

    return data;
}

void SamplerChannelNode::prepareNode (double, int)
{
    for (auto& v : voices)
        v.active = false;
}

void SamplerChannelNode::startVoice (int key, float velocity)
{
    stopVoices (key, true);

    Voice* target = nullptr;
    for (auto& v : voices)
        if (! v.active) { target = &v; break; }

    if (target == nullptr)
    {
        target = &voices[0];
        for (auto& v : voices)
            if (v.age < target->age)
                target = &v;
    }

    const float attack = juce::jmax (0.0005f, attackSeconds.load (std::memory_order_relaxed));
    const double pitch = std::pow (2.0, (key - rootNote.load (std::memory_order_relaxed)) / 12.0);

    target->active = true;
    target->key = key;
    target->position = 0.0;
    target->increment = pitch * sample->sampleRate / sampleRate;
    target->velocityGain = std::pow (juce::jlimit (0.0f, 1.0f, velocity), 1.3f);
    target->envelope = 0.0f;
    target->attackStep = 1.0f / (float) juce::jmax (1.0, attack * sampleRate);
    target->releasing = false;
    target->releaseStep = 0.0f;
    target->age = ++voiceCounter;
}

void SamplerChannelNode::stopVoices (int key, bool fast)
{
    const float seconds = fast ? 0.004f : juce::jmax (0.002f, releaseSeconds.load (std::memory_order_relaxed));
    const float step = 1.0f / (float) juce::jmax (1.0, seconds * sampleRate);

    for (auto& v : voices)
        if (v.active && v.key == key && ! v.releasing)
        {
            v.releasing = true;
            v.releaseStep = step;
        }
}

void SamplerChannelNode::renderVoices (juce::AudioBuffer<float>& out, int start, int count)
{
    if (count <= 0)
        return;

    const int length = sample->audio.getNumSamples();
    const float* srcL = sample->audio.getReadPointer (0);
    const float* srcR = sample->audio.getReadPointer (1);
    auto* dstL = out.getWritePointer (0, start);
    auto* dstR = out.getWritePointer (1, start);

    for (auto& v : voices)
    {
        if (! v.active)
            continue;

        for (int i = 0; i < count; ++i)
        {
            if (v.position >= length)
            {
                v.active = false;
                break;
            }

            if (v.releasing)
            {
                v.envelope -= v.releaseStep;
                if (v.envelope <= 0.0f)
                {
                    v.active = false;
                    break;
                }
            }
            else if (v.envelope < 1.0f)
            {
                v.envelope = juce::jmin (1.0f, v.envelope + v.attackStep);
            }

            const float g = v.envelope * v.velocityGain;
            if (v.increment == 1.0)
            {
                const int idx = (int) v.position;
                dstL[i] += srcL[idx] * g;
                dstR[i] += srcR[idx] * g;
            }
            else
            {
                dstL[i] += hermite (srcL, length, v.position) * g;
                dstR[i] += hermite (srcR, length, v.position) * g;
            }
            v.position += v.increment;
        }
    }
}

void SamplerChannelNode::renderNode (juce::AudioBuffer<float>& out, int numSamples, const juce::MidiBuffer& midi)
{
    int cursor = 0;
    const bool isOneShot = oneShot.load (std::memory_order_relaxed);

    for (const auto metadata : midi)
    {
        const int position = juce::jlimit (0, numSamples, metadata.samplePosition);
        renderVoices (out, cursor, position - cursor);
        cursor = position;

        const auto msg = metadata.getMessage();
        if (msg.isNoteOn())
            startVoice (msg.getNoteNumber(), msg.getFloatVelocity());
        else if (msg.isNoteOff() && ! isOneShot)
            stopVoices (msg.getNoteNumber(), false);
    }

    renderVoices (out, cursor, numSamples - cursor);
}

void SamplerChannelNode::renderClip (juce::AudioBuffer<float>& out, const ClipSegment& segment)
{
    const int length = sample->audio.getNumSamples();
    const double ratio = sample->sampleRate / sampleRate;
    double position = segment.sourcePosition * ratio;
    const int end = juce::jmin (out.getNumSamples(), segment.destStart + segment.numSamples);

    const float* srcL = sample->audio.getReadPointer (0);
    const float* srcR = sample->audio.getReadPointer (1);
    auto* dstL = out.getWritePointer (0);
    auto* dstR = out.getWritePointer (1);

    for (int i = juce::jmax (0, segment.destStart); i < end && position < length; ++i, position += ratio)
    {
        if (position < 0.0)
            continue;

        dstL[i] += hermite (srcL, length, position) * segment.gain;
        dstR[i] += hermite (srcR, length, position) * segment.gain;
    }
}
}
