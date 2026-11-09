#include "engine/MixerNode.h"

namespace harmony::engine
{
FxSlotNode::FxSlotNode (int u, juce::String sig, std::unique_ptr<juce::AudioPluginInstance> instance)
    : uid (u), signature (std::move (sig)), plugin (std::move (instance))
{
    jassert (plugin != nullptr);
    plugin->enableAllBuses();

    auto layout = plugin->getBusesLayout();
    if (! layout.inputBuses.isEmpty())
        layout.inputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    if (! layout.outputBuses.isEmpty())
        layout.outputBuses.getReference (0) = juce::AudioChannelSet::stereo();
    if (plugin->checkBusesLayoutSupported (layout))
        plugin->setBusesLayout (layout);

    midi.ensureSize (1024);
}

FxSlotNode::~FxSlotNode()
{
    plugin->releaseResources();
}

void FxSlotNode::prepare (double sampleRate, int maxBlockSize)
{
    plugin->releaseResources();
    plugin->setRateAndBufferSizeDetails (sampleRate, maxBlockSize);
    plugin->prepareToPlay (sampleRate, maxBlockSize);
    work.setSize (juce::jmax (2, plugin->getTotalNumInputChannels(), plugin->getTotalNumOutputChannels()),
                  maxBlockSize, false, true, true);
}

int FxSlotNode::getLatencySamples() const noexcept
{
    return bypassed.load (std::memory_order_relaxed) ? 0 : juce::jmax (0, plugin->getLatencySamples());
}

void FxSlotNode::process (juce::AudioBuffer<float>& io, int numSamples)
{
    if (bypassed.load (std::memory_order_relaxed))
        return;

    juce::AudioBuffer<float> view (work.getArrayOfWritePointers(), work.getNumChannels(), numSamples);
    view.clear();

    const int ins = plugin->getTotalNumInputChannels();
    if (ins > 0) view.copyFrom (0, 0, io, 0, 0, numSamples);
    if (ins > 1) view.copyFrom (1, 0, io, 1, 0, numSamples);

    midi.clear();
    {
        const juce::ScopedLock lock (plugin->getCallbackLock());
        if (plugin->isSuspended())
            return;

        plugin->processBlock (view, midi);
    }

    const int outs = plugin->getTotalNumOutputChannels();
    if (outs <= 0)
        return;

    io.copyFrom (0, 0, view, 0, 0, numSamples);
    io.copyFrom (1, 0, view, outs > 1 ? 1 : 0, 0, numSamples);
}

void DelayCompensator::prepare (int maxDelaySamples, int maxBlockSize)
{
    ring.setSize (2, juce::nextPowerOfTwo (maxDelaySamples + maxBlockSize + 1));
    ring.clear();
    writePosition = 0;
    currentDelay = 0;
}

void DelayCompensator::process (juce::AudioBuffer<float>& io, int numSamples, int delaySamples) noexcept
{
    const int size = ring.getNumSamples();
    delaySamples = juce::jlimit (0, size - numSamples - 1, delaySamples);

    if (delaySamples == 0 && currentDelay == 0)
        return;

    currentDelay = delaySamples;
    const int mask = size - 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        auto* data = io.getWritePointer (ch);
        auto* r = ring.getWritePointer (ch);
        int w = writePosition;

        for (int i = 0; i < numSamples; ++i)
        {
            r[w] = data[i];
            data[i] = r[(w - delaySamples) & mask];
            w = (w + 1) & mask;
        }
    }

    writePosition = (writePosition + numSamples) & mask;
}

InsertNode::InsertNode (int i)
    : index (i)
{
    gain.volume = unityFader;
}

void InsertNode::prepare (double sampleRate, int maxBlockSize)
{
    buffer.setSize (2, maxBlockSize, false, true, true);
    gain.prepare (sampleRate);
    compensator.prepare ((int) (sampleRate * 2.0), maxBlockSize);
}
}
