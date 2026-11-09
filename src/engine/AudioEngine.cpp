#include "engine/AudioEngine.h"
#include "model/Ids.h"
#include <algorithm>

namespace harmony::engine
{
void sortEvents (std::vector<MidiEvent>& events)
{
    std::stable_sort (events.begin(), events.end(), [] (const MidiEvent& a, const MidiEvent& b)
    {
        if (a.tick != b.tick)
            return a.tick < b.tick;
        return a.isNoteOff() && ! b.isNoteOff();
    });
}

namespace
{
    juce::AudioBuffer<float> makeClick (double sampleRate, double frequency, float level)
    {
        const int length = (int) (sampleRate * 0.03);
        juce::AudioBuffer<float> click (1, length);
        auto* d = click.getWritePointer (0);

        for (int i = 0; i < length; ++i)
        {
            const double t = (double) i / sampleRate;
            const double env = std::exp (-t * 160.0) * juce::jmin (1.0, (double) i / 24.0);
            d[i] = (float) (std::sin (juce::MathConstants<double>::twoPi * frequency * t) * env) * level;
        }
        return click;
    }

    void sanitize (juce::AudioBuffer<float>& buffer, int numSamples) noexcept
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* d = buffer.getWritePointer (ch);
            for (int i = 0; i < numSamples; ++i)
            {
                const float v = d[i];
                d[i] = std::isfinite (v) ? juce::jlimit (-4.0f, 4.0f, v) : 0.0f;
            }
        }
    }
}

AudioEngine::AudioEngine()
{
    formats.registerBasicFormats();

    for (int i = 0; i <= numMixerInserts; ++i)
        inserts.push_back (std::make_unique<InsertNode> (i));

    liveBuffer.ensureSize (8192);
    previewThread.startThread (juce::Thread::Priority::normal);
    prepareAll (sampleRate, blockSize);
    startTimer (50);
}

AudioEngine::~AudioEngine()
{
    stopTimer();
    closeDevice();
    previewTransport.setSource (nullptr);
    previewSource.reset();
    previewThread.stopThread (2000);
    live.store (nullptr);
    for (auto& destroy : retireAfterPublish)
        destroy();
    retireAfterPublish.clear();
    collectGarbage (true);
    channelNodes.clear();
    slotNodes.clear();
}

juce::String AudioEngine::openDevice (const juce::XmlElement* savedState)
{
    auto error = deviceManager.initialise (0, 2, savedState, true);

    if (deviceManager.getCurrentAudioDevice() == nullptr)
        error = deviceManager.initialiseWithDefaultDevices (0, 2);

    if (deviceManager.getCurrentAudioDevice() == nullptr)
    {
        for (auto* type : deviceManager.getAvailableDeviceTypes())
        {
            deviceManager.setCurrentAudioDeviceType (type->getTypeName(), true);
            juce::AudioDeviceManager::AudioDeviceSetup setup;
            setup.outputDeviceName = type->getDeviceNames (false)[type->getDefaultDeviceIndex (false)];
            setup.useDefaultOutputChannels = true;
            setup.inputChannels.clear();
            setup.useDefaultInputChannels = false;
            error = deviceManager.setAudioDeviceSetup (setup, true);
            if (deviceManager.getCurrentAudioDevice() != nullptr)
                break;
        }
    }

    deviceManager.addAudioCallback (this);
    enableMidiInputs();
    return error;
}

void AudioEngine::enableMidiInputs()
{
    for (auto& device : juce::MidiInput::getAvailableDevices())
    {
        deviceManager.setMidiInputDeviceEnabled (device.identifier, true);
        deviceManager.removeMidiInputDeviceCallback (device.identifier, this);
        deviceManager.addMidiInputDeviceCallback (device.identifier, this);
    }
}

void AudioEngine::closeDevice()
{
    deviceManager.removeAudioCallback (this);
    for (auto& device : juce::MidiInput::getAvailableDevices())
        deviceManager.removeMidiInputDeviceCallback (device.identifier, this);
    deviceManager.closeAudioDevice();
}

void AudioEngine::play()
{
    if (playing.load())
        return;

    playStartTick = position.load();
    playing = true;
}

void AudioEngine::stop()
{
    if (playing.exchange (false))
    {
        stopPending = true;
        setPositionTicks (playStartTick);
    }
    else
    {
        stopPending = true;
        setPositionTicks (0.0);
        playStartTick = 0.0;
    }
}

void AudioEngine::togglePlay()
{
    if (playing.load())
        stop();
    else
        play();
}

void AudioEngine::setPositionTicks (double ticks)
{
    ticks = juce::jmax (0.0, ticks);
    seekTarget = ticks;
    seekPending = true;
    position = ticks;

    if (! playing.load())
        playStartTick = ticks;
}

double AudioEngine::getDisplayPositionTicks() const noexcept
{
    const double pos = position.load();
    if (! playing.load() || offline.load() || seekPending.load())
        return pos;

    const double elapsed = juce::jlimit (0.0, maxExtrapolationMs.load(), juce::Time::getMillisecondCounterHiRes() - positionStampMs.load());
    return pos + elapsed * ticksPerMs.load();
}

void AudioEngine::setSongMode (bool shouldUseSong)
{
    if (songMode.exchange (shouldUseSong) != shouldUseSong)
        setPositionTicks (0.0);
}

void AudioEngine::panic()
{
    panicPending = true;
}

void AudioEngine::retire (std::function<void()> destroy)
{
    Retired r;
    r.destroy = std::move (destroy);
    r.realtimeCounter = realtimeGuard.counter.load();
    r.realtimeBusy = realtimeGuard.busy.load();
    r.offlineCounter = offlineGuard.counter.load();
    r.offlineBusy = offlineGuard.busy.load();
    graveyard.push_back (std::move (r));
}

void AudioEngine::collectGarbage (bool force)
{
    const auto rtCounter = realtimeGuard.counter.load();
    const auto offCounter = offlineGuard.counter.load();

    for (auto it = graveyard.begin(); it != graveyard.end();)
    {
        const bool rtSafe = ! it->realtimeBusy || rtCounter != it->realtimeCounter;
        const bool offSafe = ! it->offlineBusy || offCounter != it->offlineCounter;

        if (force || (rtSafe && offSafe))
        {
            it->destroy();
            it = graveyard.erase (it);
        }
        else
        {
            ++it;
        }
    }
}

void AudioEngine::timerCallback()
{
    collectGarbage (false);
}

void AudioEngine::publish (std::unique_ptr<Snapshot> snapshot)
{
    live.store (snapshot.get());
    auto old = std::move (published);
    published = std::move (snapshot);

    if (old != nullptr)
    {
        auto shared = std::shared_ptr<Snapshot> (old.release());
        retire ([shared]() mutable { shared.reset(); });
    }

    for (auto& destroy : retireAfterPublish)
        retire (std::move (destroy));
    retireAfterPublish.clear();
}

void AudioEngine::addChannelNode (std::unique_ptr<ChannelNode> node)
{
    jassert (node != nullptr);
    node->prepare (sampleRate, blockSize);
    node->setNonRealtime (offline.load());
    const int uid = node->uid;

    if (auto existing = channelNodes.find (uid); existing != channelNodes.end())
    {
        auto shared = std::shared_ptr<ChannelNode> (existing->second.release());
        retireAfterPublish.push_back ([shared]() mutable { shared.reset(); });
    }

    channelNodes[uid] = std::move (node);
}

void AudioEngine::retireChannelNode (int uid)
{
    if (auto it = channelNodes.find (uid); it != channelNodes.end())
    {
        auto shared = std::shared_ptr<ChannelNode> (it->second.release());
        channelNodes.erase (it);
        retire ([shared]() mutable { shared.reset(); });
    }
}

ChannelNode* AudioEngine::findChannelNode (int uid) const
{
    auto it = channelNodes.find (uid);
    return it != channelNodes.end() ? it->second.get() : nullptr;
}

std::vector<int> AudioEngine::getChannelNodeIds() const
{
    std::vector<int> ids;
    for (auto& [uid, node] : channelNodes)
        ids.push_back (uid);
    return ids;
}

void AudioEngine::addSlotNode (std::unique_ptr<FxSlotNode> node)
{
    jassert (node != nullptr);
    node->prepare (sampleRate, blockSize);
    node->getPlugin()->setNonRealtime (offline.load());
    const int uid = node->uid;

    if (auto existing = slotNodes.find (uid); existing != slotNodes.end())
    {
        auto shared = std::shared_ptr<FxSlotNode> (existing->second.release());
        retireAfterPublish.push_back ([shared]() mutable { shared.reset(); });
    }

    slotNodes[uid] = std::move (node);
}

void AudioEngine::retireSlotNode (int uid)
{
    if (auto it = slotNodes.find (uid); it != slotNodes.end())
    {
        auto shared = std::shared_ptr<FxSlotNode> (it->second.release());
        slotNodes.erase (it);
        retire ([shared]() mutable { shared.reset(); });
    }
}

FxSlotNode* AudioEngine::findSlotNode (int uid) const
{
    auto it = slotNodes.find (uid);
    return it != slotNodes.end() ? it->second.get() : nullptr;
}

std::vector<int> AudioEngine::getSlotNodeIds() const
{
    std::vector<int> ids;
    for (auto& [uid, node] : slotNodes)
        ids.push_back (uid);
    return ids;
}

InsertNode* AudioEngine::getInsertNode (int index) const noexcept
{
    return juce::isPositiveAndBelow (index, (int) inserts.size()) ? inserts[(size_t) index].get() : nullptr;
}

void AudioEngine::sendPreviewNote (int channelUid, int key, float velocity, bool on)
{
    const auto scope = previewFifo.write (1);
    if (scope.blockSize1 > 0)
        previewEvents[(size_t) scope.startIndex1] = { channelUid, key, velocity, on };
}

void AudioEngine::previewFile (const juce::File& file)
{
    previewTransport.stop();
    previewTransport.setSource (nullptr);
    previewSource.reset();

    if (auto* reader = formats.createReaderFor (file))
    {
        previewSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
        previewTransport.setSource (previewSource.get(), 32768, &previewThread, reader->sampleRate, 2);
        previewTransport.setPosition (0.0);
        previewTransport.start();
    }
}

void AudioEngine::stopPreview()
{
    previewTransport.stop();
}

void AudioEngine::drainRecorded (std::vector<RecordedEvent>& destination)
{
    const auto scope = recordFifo.read (recordFifo.getNumReady());
    for (int i = 0; i < scope.blockSize1; ++i) destination.push_back (recordEvents[(size_t) (scope.startIndex1 + i)]);
    for (int i = 0; i < scope.blockSize2; ++i) destination.push_back (recordEvents[(size_t) (scope.startIndex2 + i)]);
}

void AudioEngine::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    liveCollector.addMessageToQueue (message);
}

void AudioEngine::prepareAll (double newSampleRate, int newBlockSize)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    blockSize = juce::jlimit (32, 8192, newBlockSize);

    for (auto& [uid, node] : channelNodes) node->prepare (sampleRate, blockSize);
    for (auto& [uid, node] : slotNodes)    node->prepare (sampleRate, blockSize);
    for (auto& insert : inserts)            insert->prepare (sampleRate, blockSize);

    chunkBuffer.setSize (2, blockSize, false, true, true);
    previewBuffer.setSize (2, blockSize, false, true, true);
    liveCollector.reset (sampleRate);
    previewTransport.prepareToPlay (blockSize, sampleRate);
    clickHigh = makeClick (sampleRate, 1760.0, 0.55f);
    clickLow = makeClick (sampleRate, 1175.0, 0.4f);
    clickPosition = -1;
}

void AudioEngine::prepareStandalone (double newSampleRate, int newBlockSize)
{
    prepareAll (newSampleRate, newBlockSize);
}

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    if (! offline.load())
        prepareAll (device->getCurrentSampleRate(), device->getCurrentBufferSizeSamples());

    if (onDeviceStarted)
        juce::MessageManager::callAsync ([this] { if (onDeviceStarted) onDeviceStarted(); });
}

void AudioEngine::audioDeviceStopped()
{
    previewTransport.releaseResources();
}

ChannelNode* AudioEngine::liveTarget (const Snapshot& snap) const noexcept
{
    const int uid = liveChannel.load (std::memory_order_relaxed);
    for (auto& plan : snap.channels)
        if (plan.uid == uid)
            return plan.node;
    return nullptr;
}

void AudioEngine::releaseAll (const Snapshot& snap, int sampleOffset)
{
    for (auto& plan : snap.channels)
        if (plan.node != nullptr)
            plan.node->releaseHeldNotes (sampleOffset);
}

void AudioEngine::schedule (const Snapshot& snap, bool song, int patternIndex, double from, double to,
                            int sampleOffset, double samplesPerTick, int numSamples)
{
    const auto toOffset = [&] (double tick)
    {
        return juce::jlimit (0, numSamples - 1, sampleOffset + (int) ((tick - from) * samplesPerTick));
    };

    for (auto& plan : snap.channels)
    {
        if (plan.node == nullptr)
            continue;

        const std::vector<MidiEvent>* events = nullptr;
        if (song)
            events = &plan.song;
        else if (juce::isPositiveAndBelow (patternIndex, (int) plan.patterns.size()))
            events = &plan.patterns[(size_t) patternIndex];

        if (events != nullptr && ! events->empty())
        {
            auto it = std::lower_bound (events->begin(), events->end(), from,
                                        [] (const MidiEvent& e, double t) { return (double) e.tick < t; });

            for (; it != events->end() && (double) it->tick < to; ++it)
            {
                if (it->isNoteOff())
                    plan.node->addNoteOff (it->key, toOffset ((double) it->tick));
                else
                    plan.node->addNoteOn (it->key, it->velocity / 127.0f, toOffset ((double) it->tick));
            }
        }

        if (song)
        {
            for (auto& clip : plan.audioClips)
            {
                const double begin = juce::jmax (from, (double) clip.start);
                const double end = juce::jmin (to, (double) clip.end);
                if (end <= begin)
                    continue;

                ClipSegment segment;
                segment.destStart = sampleOffset + (int) std::round ((begin - from) * samplesPerTick);
                segment.numSamples = (int) std::ceil ((end - begin) * samplesPerTick);
                segment.sourcePosition = (begin - (double) clip.start + (double) clip.sourceOffset) * samplesPerTick;
                segment.gain = clip.gain;
                plan.node->addClipSegment (segment);
            }
        }
    }

    if (metronome.load (std::memory_order_relaxed) && ! offline.load (std::memory_order_relaxed))
    {
        const auto beat = juce::jmax ((juce::int64) 1, snap.ticksPerBeat);
        for (auto b = (juce::int64) std::ceil (from / (double) beat) * beat; (double) b < to; b += beat)
            if (numPendingClicks < (int) pendingClicks.size())
                pendingClicks[(size_t) numPendingClicks++] = { toOffset ((double) b), b % snap.ticksPerBar == 0 };
    }
}

void AudioEngine::mixDown (const Snapshot& snap, int numSamples)
{
    for (auto& insert : inserts)
        insert->clear (numSamples);

    bool anyChannelSolo = false;
    for (auto& plan : snap.channels)
        anyChannelSolo = anyChannelSolo || (plan.node != nullptr && plan.node->solo.load (std::memory_order_relaxed));

    auto* master = inserts[0].get();

    for (auto& plan : snap.channels)
    {
        if (plan.node == nullptr)
            continue;

        plan.node->gain.silencedBySolo.store (anyChannelSolo && ! plan.node->solo.load (std::memory_order_relaxed), std::memory_order_relaxed);
        plan.node->process (numSamples);

        auto* target = getInsertNode (plan.insert);
        auto& dest = (target != nullptr ? target : master)->getBuffer();
        auto& src = plan.node->getOutput();
        dest.addFrom (0, 0, src, 0, 0, numSamples);
        dest.addFrom (1, 0, src, 1, 0, numSamples);
    }

    bool anyInsertSolo = false;
    int maxLatency = 0;
    std::array<int, numMixerInserts + 1> latencies {};

    for (auto& plan : snap.inserts)
    {
        if (plan.index <= 0 || plan.index > numMixerInserts)
            continue;

        int latency = 0;
        for (auto* slot : plan.slots)
            if (slot != nullptr)
                latency += slot->getLatencySamples();

        latencies[(size_t) plan.index] = latency;
        maxLatency = juce::jmax (maxLatency, latency);
        anyInsertSolo = anyInsertSolo || inserts[(size_t) plan.index]->solo.load (std::memory_order_relaxed);
    }

    for (auto& plan : snap.inserts)
    {
        if (plan.index <= 0 || plan.index > numMixerInserts)
            continue;

        auto* insert = inserts[(size_t) plan.index].get();
        auto& buffer = insert->getBuffer();

        for (auto* slot : plan.slots)
            if (slot != nullptr)
                slot->process (buffer, numSamples);

        insert->compensator.process (buffer, numSamples, maxLatency - latencies[(size_t) plan.index]);
        insert->gain.silencedBySolo.store (anyInsertSolo && ! insert->solo.load (std::memory_order_relaxed), std::memory_order_relaxed);
        insert->gain.process (buffer, numSamples);
        insert->meter.push (buffer, numSamples);

        master->getBuffer().addFrom (0, 0, buffer, 0, 0, numSamples);
        master->getBuffer().addFrom (1, 0, buffer, 1, 0, numSamples);
    }

    for (auto& plan : snap.inserts)
        if (plan.index == 0)
            for (auto* slot : plan.slots)
                if (slot != nullptr)
                    slot->process (master->getBuffer(), numSamples);

    master->gain.process (master->getBuffer(), numSamples);
    sanitize (master->getBuffer(), numSamples);
    master->meter.push (master->getBuffer(), numSamples);
}

void AudioEngine::addMetronome (juce::AudioBuffer<float>& out, int numSamples)
{
    auto renderTail = [this, &out] (int start, int count)
    {
        if (clickPosition < 0 || count <= 0)
            return;

        auto& click = clickIsHigh ? clickHigh : clickLow;
        const int n = juce::jmin (count, click.getNumSamples() - clickPosition);
        out.addFrom (0, start, click, 0, clickPosition, n);
        out.addFrom (1, start, click, 0, clickPosition, n);
        clickPosition += n;

        if (clickPosition >= click.getNumSamples())
            clickPosition = -1;
    };

    int cursor = 0;
    for (int c = 0; c <= numPendingClicks; ++c)
    {
        const int segmentEnd = c < numPendingClicks ? juce::jlimit (cursor, numSamples, pendingClicks[(size_t) c].first) : numSamples;
        renderTail (cursor, segmentEnd - cursor);
        cursor = segmentEnd;

        if (c < numPendingClicks)
        {
            clickIsHigh = pendingClicks[(size_t) c].second;
            clickPosition = 0;
        }
    }

    numPendingClicks = 0;
}

void AudioEngine::processChunk (const Snapshot& snap, juce::AudioBuffer<float>& out, int numSamples, bool realtime)
{
    numPendingClicks = 0;

    if (panicPending.exchange (false))
    {
        releaseAll (snap, 0);
        for (auto& plan : snap.channels)
            if (plan.node != nullptr)
                for (int ch = 1; ch <= 16; ++ch)
                    plan.node->addMessage (juce::MidiMessage::allControllersOff (ch), 0);
    }

    if (stopPending.exchange (false))
        releaseAll (snap, 0);

    double pos = position.load();
    if (seekPending.exchange (false))
    {
        releaseAll (snap, 0);
        pos = seekTarget.load();
    }

    const double samplesPerTick = sampleRate * 60.0 / (juce::jmax (10.0, snap.bpm) * ppq);
    const double ticksPerSample = 1.0 / samplesPerTick;
    const bool isPlaying = playing.load();

    if (realtime)
    {
        auto findNode = [&snap] (int uid) -> ChannelNode*
        {
            for (auto& plan : snap.channels)
                if (plan.uid == uid)
                    return plan.node;
            return nullptr;
        };

        const auto scope = previewFifo.read (previewFifo.getNumReady());
        auto handlePreview = [&] (int start, int count)
        {
            for (int i = 0; i < count; ++i)
            {
                const auto& e = previewEvents[(size_t) (start + i)];
                if (auto* node = findNode (e.channel))
                {
                    if (e.on) node->addNoteOn (e.key, e.velocity, 0);
                    else      node->addNoteOff (e.key, 0);
                }
            }
        };
        handlePreview (scope.startIndex1, scope.blockSize1);
        handlePreview (scope.startIndex2, scope.blockSize2);

        liveBuffer.clear();
        liveCollector.removeNextBlockOfMessages (liveBuffer, numSamples);
        if (! liveBuffer.isEmpty())
        {
            auto* target = liveTarget (snap);
            const bool capture = isPlaying && recording.load (std::memory_order_relaxed) && target != nullptr;

            for (const auto metadata : liveBuffer)
            {
                const auto msg = metadata.getMessage();
                if (target != nullptr)
                    target->addMessage (msg, metadata.samplePosition);

                if (capture && (msg.isNoteOn() || msg.isNoteOff()))
                {
                    const auto w = recordFifo.write (1);
                    if (w.blockSize1 > 0)
                        recordEvents[(size_t) w.startIndex1] = { pos + metadata.samplePosition * ticksPerSample, target->uid,
                                                                 msg.getNoteNumber(), msg.getFloatVelocity(), msg.isNoteOn() };
                }
            }
        }
    }

    if (isPlaying)
    {
        const bool song = offline.load() ? offlineSongMode : songMode.load();
        const int patternIndex = song ? -1 : snap.patternIndex (currentPattern.load());

        juce::int64 loopStart = 0, loopEnd = snap.ticksPerBar;
        if (offline.load())
            loopEnd = std::numeric_limits<juce::int64>::max() / 4;
        else if (song)
        {
            const bool hasLoop = snap.loopEnd > snap.loopStart;
            loopStart = hasLoop ? snap.loopStart : 0;
            loopEnd = hasLoop ? snap.loopEnd : snap.songLoopEnd();
        }
        else if (patternIndex >= 0)
            loopEnd = snap.patterns[(size_t) patternIndex].length;

        loopEnd = juce::jmax (loopStart + 1, loopEnd);

        int offset = 0;
        while (offset < numSamples)
        {
            if (pos >= (double) loopEnd)
            {
                releaseAll (snap, offset);
                pos = (double) loopStart;
            }

            const double samplesLeft = ((double) loopEnd - pos) * samplesPerTick;
            const int remaining = numSamples - offset;
            const int n = samplesLeft >= (double) remaining ? remaining : juce::jlimit (1, remaining, (int) std::ceil (samplesLeft));
            const double end = juce::jmin ((double) loopEnd, pos + n * ticksPerSample);

            schedule (snap, song, patternIndex, pos, end, offset, samplesPerTick, numSamples);
            pos = end;
            offset += n;

            if (pos >= (double) loopEnd)
            {
                releaseAll (snap, juce::jmin (offset, numSamples - 1));
                pos = (double) loopStart;
            }
        }

        if (! seekPending.load())
        {
            position.store (pos);
            if (realtime)
            {
                ticksPerMs.store (ticksPerSample * sampleRate / 1000.0);
                maxExtrapolationMs.store (2000.0 * blockSize / sampleRate);
                positionStampMs.store (juce::Time::getMillisecondCounterHiRes());
            }
        }
    }

    mixDown (snap, numSamples);

    auto& master = inserts[0]->getBuffer();
    out.copyFrom (0, 0, master, 0, 0, numSamples);
    out.copyFrom (1, 0, master, 1, 0, numSamples);

    if (realtime)
    {
        addMetronome (out, numSamples);

        if (previewTransport.isPlaying())
        {
            juce::AudioSourceChannelInfo info (&previewBuffer, 0, numSamples);
            previewTransport.getNextAudioBlock (info);
            out.addFrom (0, 0, previewBuffer, 0, 0, numSamples, 0.8f);
            out.addFrom (1, 0, previewBuffer, 1, 0, numSamples, 0.8f);
        }
    }
}

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const*, int,
                                                    float* const* outputs, int numOutputs, int numSamples,
                                                    const juce::AudioIODeviceCallbackContext&)
{
    realtimeGuard.busy.store (true);
    const auto* snap = live.load();

    if (snap == nullptr || offline.load() || numOutputs <= 0)
    {
        for (int ch = 0; ch < numOutputs; ++ch)
            if (outputs[ch] != nullptr)
                juce::FloatVectorOperations::clear (outputs[ch], numSamples);
    }
    else
    {
        juce::ScopedNoDenormals noDenormals;
        int done = 0;
        while (done < numSamples)
        {
            const int n = juce::jmin (blockSize, numSamples - done);
            juce::AudioBuffer<float> chunk (chunkBuffer.getArrayOfWritePointers(), 2, n);
            processChunk (*snap, chunk, n, true);

            for (int ch = 0; ch < numOutputs; ++ch)
                if (outputs[ch] != nullptr)
                {
                    if (ch < 2) juce::FloatVectorOperations::copy (outputs[ch] + done, chunk.getReadPointer (ch), n);
                    else        juce::FloatVectorOperations::clear (outputs[ch] + done, n);
                }

            done += n;
        }
    }

    realtimeGuard.counter.fetch_add (1);
    realtimeGuard.busy.store (false);
}

void AudioEngine::beginOffline (double renderSampleRate, int renderBlockSize, juce::int64 startTick, bool song)
{
    offline = true;

    const auto counter = realtimeGuard.counter.load();
    for (int i = 0; i < 400 && realtimeGuard.busy.load() && realtimeGuard.counter.load() == counter; ++i)
        juce::Thread::sleep (5);

    playing = false;
    prepareAll (renderSampleRate, renderBlockSize);

    for (auto& [uid, node] : channelNodes) node->setNonRealtime (true);
    for (auto& [uid, node] : slotNodes)    node->getPlugin()->setNonRealtime (true);

    offlineSongMode = song;
    position = (double) startTick;
    seekPending = false;
    stopPending = true;
    playing = true;
}

void AudioEngine::processOffline (juce::AudioBuffer<float>& output, int numSamples)
{
    offlineGuard.busy.store (true);

    if (const auto* snap = live.load())
    {
        juce::ScopedNoDenormals noDenormals;
        processChunk (*snap, output, juce::jmin (numSamples, blockSize), false);
    }
    else
    {
        output.clear();
    }

    offlineGuard.counter.fetch_add (1);
    offlineGuard.busy.store (false);
}

void AudioEngine::endOffline()
{
    playing = false;

    for (auto& [uid, node] : channelNodes) node->setNonRealtime (false);
    for (auto& [uid, node] : slotNodes)    node->getPlugin()->setNonRealtime (false);

    double deviceRate = 48000.0;
    int deviceBlock = 512;
    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        deviceRate = device->getCurrentSampleRate();
        deviceBlock = device->getCurrentBufferSizeSamples();
    }

    prepareAll (deviceRate, deviceBlock);
    position = 0.0;
    playStartTick = 0.0;
    stopPending = true;
    offline = false;
}
}
