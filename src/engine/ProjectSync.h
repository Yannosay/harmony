#pragma once

#include "engine/AudioEngine.h"
#include "engine/PluginManager.h"
#include "model/Project.h"
#include <set>

namespace harmony::engine
{
class ProjectSync final : private juce::ValueTree::Listener,
                          private juce::AsyncUpdater
{
public:
    ProjectSync (Project& project, AudioEngine& engine, PluginManager& plugins);
    ~ProjectSync() override;

    void syncNow();
    void captureStates();
    bool isLoading() const noexcept { return ! loading.empty(); }

    juce::AudioPluginInstance* pluginForChannel (int uid) const;
    juce::AudioPluginInstance* pluginForSlot (int uid) const;
    juce::StringArray soundFontPresets (int channelUid) const;
    std::shared_ptr<const SampleData> sampleForChannel (int channelUid) const;

    std::function<void (const juce::String&)> onError;
    std::function<void()> onNodesChanged;

    static juce::String signatureFor (const juce::ValueTree& channelOrSlot);

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;
    void valueTreeChildOrderChanged (juce::ValueTree&, int, int) override;
    void valueTreeRedirected (juce::ValueTree&) override;
    void handleAsyncUpdate() override;

    void reconcile();
    void loadChannel (const juce::ValueTree& channel, const juce::String& signature);
    void loadSlot (const juce::ValueTree& slot, const juce::String& signature);
    void applyChannelParameters (const juce::ValueTree& channel);
    void applyInsertParameters (const juce::ValueTree& insert);
    void applySlotParameters (const juce::ValueTree& slot);
    void finishChannelLoad (int uid, const juce::String& signature, std::unique_ptr<ChannelNode> node);
    void finishSlotLoad (int uid, const juce::String& signature, std::unique_ptr<FxSlotNode> node);
    void reportError (const juce::String& message);
    static void restoreState (juce::AudioPluginInstance& instance, const juce::String& base64);

    Project& project;
    AudioEngine& engine;
    PluginManager& plugins;
    std::set<juce::String> loading, failed;
    juce::ThreadPool loaderPool { juce::ThreadPoolOptions{}.withThreadName ("Harmony loader").withNumberOfThreads (2) };
    std::shared_ptr<bool> alive = std::make_shared<bool> (true);
};
}
