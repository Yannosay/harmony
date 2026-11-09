#include "engine/ProjectSync.h"
#include "engine/SnapshotBuilder.h"

namespace harmony::engine
{
ProjectSync::ProjectSync (Project& p, AudioEngine& e, PluginManager& pm)
    : project (p), engine (e), plugins (pm)
{
    project.getState().addListener (this);
    syncNow();
}

ProjectSync::~ProjectSync()
{
    project.getState().removeListener (this);
    loaderPool.removeAllJobs (true, 10000);
}

juce::String ProjectSync::signatureFor (const juce::ValueTree& t)
{
    if (t.hasType (id::SLOT))
    {
        if (auto desc = PluginManager::fromXmlString (t[id::pluginDesc]))
            return "fx|" + desc->createIdentifierString();
        return {};
    }

    const auto k = t[id::kind].toString();
    if (k == kind::plugin)
    {
        if (auto desc = PluginManager::fromXmlString (t[id::pluginDesc]))
            return "plugin|" + desc->createIdentifierString();
        return {};
    }
    if (k == kind::soundfont)
        return t[id::sfPath].toString().isNotEmpty() ? "sf|" + t[id::sfPath].toString() : juce::String();
    if (k == kind::sampler || k == kind::audio)
        return t[id::samplePath].toString().isNotEmpty() ? "sample|" + t[id::samplePath].toString() : juce::String();

    return {};
}

void ProjectSync::syncNow()
{
    cancelPendingUpdate();
    reconcile();
}

void ProjectSync::handleAsyncUpdate()
{
    reconcile();
}

void ProjectSync::reconcile()
{
    std::set<int> channelIds, slotIds;

    for (auto channel : project.channelsTree())
    {
        const int uid = channel[id::uid];
        channelIds.insert (uid);
        const auto sig = signatureFor (channel);
        auto* node = engine.findChannelNode (uid);

        if (sig.isEmpty())
            channelIds.erase (uid);
        else if (node == nullptr || node->signature != sig)
            loadChannel (channel, sig);
        else
            applyChannelParameters (channel);
    }

    for (auto insert : project.mixerTree())
    {
        applyInsertParameters (insert);

        for (auto slot : insert)
        {
            const int uid = slot[id::uid];
            slotIds.insert (uid);
            const auto sig = signatureFor (slot);
            auto* node = engine.findSlotNode (uid);

            if (sig.isEmpty())
                slotIds.erase (uid);
            else if (node == nullptr || node->signature != sig)
                loadSlot (slot, sig);
            else
                applySlotParameters (slot);
        }
    }

    engine.setCurrentPattern (project.currentPattern()[id::uid]);
    engine.setLiveChannel (project.selectedChannel());
    engine.publish (buildSnapshot (project, engine));

    bool removed = false;
    for (auto uid : engine.getChannelNodeIds())
        if (channelIds.count (uid) == 0)
        {
            engine.retireChannelNode (uid);
            removed = true;
        }

    for (auto uid : engine.getSlotNodeIds())
        if (slotIds.count (uid) == 0)
        {
            engine.retireSlotNode (uid);
            removed = true;
        }

    if (removed && onNodesChanged)
        onNodesChanged();
}

void ProjectSync::restoreState (juce::AudioPluginInstance& instance, const juce::String& base64)
{
    if (base64.isEmpty())
        return;

    juce::MemoryBlock block;
    if (block.fromBase64Encoding (base64) && block.getSize() > 0)
        instance.setStateInformation (block.getData(), (int) block.getSize());
}

void ProjectSync::loadChannel (const juce::ValueTree& channel, const juce::String& sig)
{
    const int uid = channel[id::uid];
    const auto key = "c" + juce::String (uid) + "|" + sig;
    if (loading.count (key) > 0 || failed.count (key) > 0)
        return;

    loading.insert (key);
    std::weak_ptr<bool> weak = alive;
    const auto k = channel[id::kind].toString();

    auto finish = [this, weak, uid, sig, key] (std::unique_ptr<ChannelNode> node, juce::String error)
    {
        if (weak.expired())
            return;

        loading.erase (key);
        if (node != nullptr)
            finishChannelLoad (uid, sig, std::move (node));
        else
        {
            failed.insert (key);
            reportError (error);
        }
    };

    if (k == kind::plugin)
    {
        auto desc = PluginManager::fromXmlString (channel[id::pluginDesc]);
        const auto state = channel[id::pluginState].toString();

        plugins.createInstance (*desc, engine.getSampleRate(), engine.getBlockSize(),
            [finish, uid, sig, state, name = desc->name] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
            {
                if (instance == nullptr)
                {
                    finish (nullptr, "Could not load " + name + (error.isNotEmpty() ? ": " + error : juce::String()));
                    return;
                }

                restoreState (*instance, state);
                finish (std::make_unique<PluginChannelNode> (uid, sig, std::move (instance)), {});
            });
        return;
    }

    const juce::File file (k == kind::soundfont ? channel[id::sfPath].toString() : channel[id::samplePath].toString());
    auto& formats = engine.getFormats();

    loaderPool.addJob ([finish, file, k, uid, sig, &formats]
    {
        juce::String error;
        std::unique_ptr<ChannelNode> node;

        if (! file.existsAsFile())
            error = "Missing file: " + file.getFullPathName();
        else if (k == kind::soundfont)
        {
            if (auto* font = SoundFontChannelNode::loadFont (file, error))
                node = std::make_unique<SoundFontChannelNode> (uid, sig, font);
        }
        else if (auto sample = SamplerChannelNode::loadSample (file, formats, error))
        {
            node = std::make_unique<SamplerChannelNode> (uid, sig, sample);
        }

        auto shared = std::make_shared<std::unique_ptr<ChannelNode>> (std::move (node));
        juce::MessageManager::callAsync ([finish, shared, error] { finish (std::move (*shared), error); });
    });
}

void ProjectSync::finishChannelLoad (int uid, const juce::String& sig, std::unique_ptr<ChannelNode> node)
{
    auto channel = project.findChannel (uid);
    if (! channel.isValid() || signatureFor (channel) != sig)
        return;

    engine.addChannelNode (std::move (node));
    applyChannelParameters (channel);
    reconcile();

    if (onNodesChanged)
        onNodesChanged();
}

void ProjectSync::loadSlot (const juce::ValueTree& slot, const juce::String& sig)
{
    const int uid = slot[id::uid];
    const auto key = "s" + juce::String (uid) + "|" + sig;
    if (loading.count (key) > 0 || failed.count (key) > 0)
        return;

    loading.insert (key);
    std::weak_ptr<bool> weak = alive;
    auto desc = PluginManager::fromXmlString (slot[id::pluginDesc]);
    const auto state = slot[id::pluginState].toString();

    plugins.createInstance (*desc, engine.getSampleRate(), engine.getBlockSize(),
        [this, weak, uid, sig, key, state, name = desc->name] (std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
        {
            if (weak.expired())
                return;

            loading.erase (key);
            if (instance == nullptr)
            {
                failed.insert (key);
                reportError ("Could not load " + name + (error.isNotEmpty() ? ": " + error : juce::String()));
                return;
            }

            restoreState (*instance, state);
            finishSlotLoad (uid, sig, std::make_unique<FxSlotNode> (uid, sig, std::move (instance)));
        });
}

void ProjectSync::finishSlotLoad (int uid, const juce::String& sig, std::unique_ptr<FxSlotNode> node)
{
    for (auto insert : project.mixerTree())
    {
        auto slot = findChildWithUid (insert, uid);
        if (slot.isValid() && signatureFor (slot) == sig)
        {
            engine.addSlotNode (std::move (node));
            applySlotParameters (slot);
            reconcile();

            if (onNodesChanged)
                onNodesChanged();
            return;
        }
    }
}

void ProjectSync::applyChannelParameters (const juce::ValueTree& channel)
{
    auto* node = engine.findChannelNode (channel[id::uid]);
    if (node == nullptr)
        return;

    node->gain.volume = (float) channel.getProperty (id::volume, 0.78f);
    node->gain.pan = (float) channel.getProperty (id::pan, 0.0f);
    node->gain.muted = (bool) channel[id::mute];
    node->solo = (bool) channel[id::solo];

    if (auto* sf = dynamic_cast<SoundFontChannelNode*> (node))
        sf->preset = juce::jmax (0, (int) channel[id::sfPreset]);

    if (auto* sampler = dynamic_cast<SamplerChannelNode*> (node))
    {
        sampler->rootNote = juce::jlimit (0, 127, (int) channel.getProperty (id::rootNote, 60));
        sampler->oneShot = (bool) channel.getProperty (id::oneShot, true);
        sampler->attackSeconds = (float) channel.getProperty (id::attack, 0.001f);
        sampler->releaseSeconds = (float) channel.getProperty (id::release, 0.08f);
    }
}

void ProjectSync::applyInsertParameters (const juce::ValueTree& insert)
{
    if (auto* node = engine.getInsertNode (insert[id::index]))
    {
        node->gain.volume = (float) insert.getProperty (id::volume, 0.8f);
        node->gain.pan = (float) insert.getProperty (id::pan, 0.0f);
        node->gain.muted = (bool) insert[id::mute];
        node->solo = (bool) insert[id::solo];
    }
}

void ProjectSync::applySlotParameters (const juce::ValueTree& slot)
{
    if (auto* node = engine.findSlotNode (slot[id::uid]))
        node->bypassed = (bool) slot[id::bypass];
}

void ProjectSync::captureStates()
{
    auto capture = [] (juce::AudioPluginInstance* instance, juce::ValueTree tree)
    {
        if (instance == nullptr)
            return;

        juce::MemoryBlock block;
        instance->getStateInformation (block);
        tree.setProperty (id::pluginState, block.toBase64Encoding(), nullptr);
    };

    for (auto channel : project.channelsTree())
        capture (pluginForChannel (channel[id::uid]), channel);

    for (auto insert : project.mixerTree())
        for (auto slot : insert)
            capture (pluginForSlot (slot[id::uid]), slot);
}

juce::AudioPluginInstance* ProjectSync::pluginForChannel (int uid) const
{
    auto* node = engine.findChannelNode (uid);
    return node != nullptr ? node->getPlugin() : nullptr;
}

juce::AudioPluginInstance* ProjectSync::pluginForSlot (int uid) const
{
    auto* node = engine.findSlotNode (uid);
    return node != nullptr ? node->getPlugin() : nullptr;
}

juce::StringArray ProjectSync::soundFontPresets (int channelUid) const
{
    if (auto* sf = dynamic_cast<SoundFontChannelNode*> (engine.findChannelNode (channelUid)))
        return sf->getPresetNames();
    return {};
}

std::shared_ptr<const SampleData> ProjectSync::sampleForChannel (int channelUid) const
{
    if (auto* s = dynamic_cast<SamplerChannelNode*> (engine.findChannelNode (channelUid)))
        return s->getSample();
    return nullptr;
}

void ProjectSync::reportError (const juce::String& message)
{
    if (onError)
        onError (message);
}

void ProjectSync::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier& property)
{
    if (property == id::pluginState || property == id::name || property == id::colour
        || property == id::appVersion || property == id::nextUid)
        return;

    if (property == id::selectedChannel) { engine.setLiveChannel (project.selectedChannel()); return; }
    if (property == id::currentPattern)  { engine.setCurrentPattern (project.currentPattern()[id::uid]); return; }

    const bool liveParameter = property == id::volume || property == id::pan || property == id::mute
                            || property == id::solo || property == id::sfPreset || property == id::rootNote
                            || property == id::oneShot || property == id::attack || property == id::release
                            || property == id::bypass;

    if (liveParameter)
    {
        if (tree.hasType (id::CHANNEL))     { applyChannelParameters (tree); return; }
        if (tree.hasType (id::INSERT))      { applyInsertParameters (tree); return; }
        if (tree.hasType (id::SLOT))        { applySlotParameters (tree); return; }
    }

    triggerAsyncUpdate();
}

void ProjectSync::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&)          { triggerAsyncUpdate(); }
void ProjectSync::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)   { triggerAsyncUpdate(); }
void ProjectSync::valueTreeChildOrderChanged (juce::ValueTree&, int, int)           { triggerAsyncUpdate(); }
void ProjectSync::valueTreeRedirected (juce::ValueTree&)                            { triggerAsyncUpdate(); }
}
