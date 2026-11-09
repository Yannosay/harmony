#pragma once

#include "ui/Widgets.h"
#include "app/Context.h"

namespace harmony::ui
{
class Browser final : public juce::Component, private juce::ChangeListener
{
public:
    explicit Browser (Context& context);
    ~Browser() override;

    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

    static juce::StringArray defaultSampleFolders();
    static bool isAudioFile (const juce::File&);
    static bool isSoundFont (const juce::File&);
    static bool isProject (const juce::File&);

    Context& ctx;

private:
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void addFolder();

    juce::TextEditor search;
    juce::TreeView tree;
    std::unique_ptr<juce::TreeViewItem> root;
    IconButton addFolderButton { "Add a folder to the browser", icons::folder() };
};
}
