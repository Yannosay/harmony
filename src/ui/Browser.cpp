#include "ui/Browser.h"
#include "engine/AudioEngine.h"
#include "engine/PluginManager.h"

namespace harmony::ui
{
namespace
{
    constexpr int browserRowHeight = 26;

    void paintRow (juce::Graphics& g, int width, int height, const juce::Path& icon, juce::Colour iconColour,
                   const juce::String& text, const juce::String& tag, bool selected, bool header = false)
    {
        if (selected)
        {
            g.setColour (colours::controlHover);
            g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 1.0f, (float) width - 4.0f, (float) height - 2.0f), 5.0f);
        }

        auto r = juce::Rectangle<int> (0, 0, width, height);
        drawIcon (g, icon, r.removeFromLeft (22).toFloat().reduced (3.0f, 5.0f), iconColour);
        r.removeFromLeft (4);

        if (tag.isNotEmpty())
        {
            g.setColour (colours::textFaint);
            g.setFont (font (8.5f, Weight::medium));
            g.drawText (tag, r.removeFromRight (40).withTrimmedRight (8), juce::Justification::centredRight, false);
        }

        g.setColour (header ? colours::text : colours::textDim.brighter (selected ? 0.4f : 0.0f));
        g.setFont (font (header ? 11.0f : 10.5f, header ? Weight::semibold : Weight::regular));
        g.drawText (text, r, juce::Justification::centredLeft, true);
    }

    class BaseItem : public juce::TreeViewItem
    {
    public:
        explicit BaseItem (Browser& b) : browser (b) {}
        int getItemHeight() const override { return browserRowHeight; }
        bool canBeSelected() const override { return true; }
        virtual juce::String sortName() const { return getUniqueName(); }

    protected:
        Browser& browser;
    };

    class PluginItem final : public BaseItem
    {
    public:
        PluginItem (Browser& b, juce::PluginDescription d) : BaseItem (b), desc (std::move (d)) {}

        bool mightContainSubItems() override { return false; }
        juce::String getUniqueName() const override { return desc.createIdentifierString(); }
        juce::String sortName() const override { return desc.name; }
        juce::var getDragSourceDescription() override { return "plugin:" + desc.createIdentifierString(); }
        juce::String getTooltip() override { return desc.manufacturerName + "  " + desc.version + "  " + desc.fileOrIdentifier; }

        void paintItem (juce::Graphics& g, int w, int h) override
        {
            paintRow (g, w, h, desc.isInstrument ? icons::piano() : icons::plug(), desc.isInstrument ? colours::accent : colours::lilac,
                      desc.name, desc.pluginFormatName, isSelected());
        }

        void itemDoubleClicked (const juce::MouseEvent&) override
        {
            auto& ctx = browser.ctx;
            if (desc.isInstrument)
            {
                if (ctx.addInstrument)
                    ctx.addInstrument (desc.createIdentifierString());
                return;
            }

            auto channel = ctx.project.findChannel (ctx.project.selectedChannel());
            const int insertIndex = channel.isValid() ? (int) channel[id::insert] : 0;
            auto insert = ctx.project.findInsert (insertIndex);

            for (int slot = 0; slot < numFxSlots; ++slot)
            {
                bool used = false;
                for (auto s : insert)
                    used = used || (int) s[id::index] == slot;

                if (! used)
                {
                    ctx.project.beginTransaction ("Add effect");
                    ctx.project.addSlot (insertIndex, slot, engine::PluginManager::toXmlString (desc));
                    if (ctx.notify)
                        ctx.notify ("Added " + desc.name + " to " + insert[id::name].toString(), false);
                    if (ctx.focusMixerInsert)
                        ctx.focusMixerInsert (insertIndex);
                    return;
                }
            }

            if (ctx.notify)
                ctx.notify ("All effect slots on " + insert[id::name].toString() + " are in use", true);
        }

    private:
        juce::PluginDescription desc;
    };

    class FileItem final : public BaseItem
    {
    public:
        FileItem (Browser& b, juce::File f) : BaseItem (b), file (std::move (f)) {}

        bool mightContainSubItems() override { return false; }
        juce::String getUniqueName() const override { return file.getFullPathName(); }
        juce::var getDragSourceDescription() override { return "file:" + file.getFullPathName(); }
        juce::String getTooltip() override { return file.getFullPathName(); }

        void paintItem (juce::Graphics& g, int w, int h) override
        {
            const bool audio = Browser::isAudioFile (file);
            const bool sf = Browser::isSoundFont (file);
            paintRow (g, w, h, audio ? icons::wave() : sf ? icons::piano() : icons::playlist(),
                      audio ? colours::success : sf ? colours::warning : colours::accent,
                      file.getFileNameWithoutExtension(), file.getFileExtension().substring (1).toUpperCase(), isSelected());
        }

        void itemClicked (const juce::MouseEvent&) override
        {
            if (Browser::isAudioFile (file))
                browser.ctx.engine.previewFile (file);
        }

        void itemDoubleClicked (const juce::MouseEvent&) override
        {
            if (browser.ctx.addFileAsChannel)
                browser.ctx.addFileAsChannel (file);
        }

    private:
        juce::File file;
    };

    class FolderItem final : public BaseItem
    {
    public:
        FolderItem (Browser& b, juce::File dir, bool removable) : BaseItem (b), folder (std::move (dir)), canRemove (removable) {}

        bool mightContainSubItems() override { return true; }
        juce::String getUniqueName() const override { return folder.getFullPathName(); }

        void paintItem (juce::Graphics& g, int w, int h) override
        {
            paintRow (g, w, h, icons::folder(), colours::textDim, folder.getFileName().isNotEmpty() ? folder.getFileName() : folder.getFullPathName(),
                      {}, isSelected());
        }

        void itemOpennessChanged (bool isNowOpen) override
        {
            if (! isNowOpen || getNumSubItems() > 0)
                return;

            juce::Array<juce::File> dirs, files;
            for (const auto& entry : juce::RangedDirectoryIterator (folder, false, "*", juce::File::findFilesAndDirectories | juce::File::ignoreHiddenFiles))
            {
                const auto f = entry.getFile();
                if (entry.isDirectory())
                    dirs.add (f);
                else if (Browser::isAudioFile (f) || Browser::isSoundFont (f) || Browser::isProject (f))
                    files.add (f);
            }

            juce::File::NaturalFileComparator comparator { false };
            dirs.sort (comparator);
            files.sort (comparator);

            for (auto& d : dirs)  addSubItem (new FolderItem (browser, d, false));
            for (auto& f : files) addSubItem (new FileItem (browser, f));
        }

        void itemClicked (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && canRemove)
            {
                juce::PopupMenu menu;
                menu.addItem ("Remove from browser", [this]
                {
                    auto& settings = browser.ctx.settings;
                    auto folders = juce::StringArray::fromLines (settings.getValue ("browserFolders"));
                    folders.removeString (folder.getFullPathName());
                    settings.setValue ("browserFolders", folders.joinIntoString ("\n"));
                    juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Browser> (&browser)] { if (safe) safe->refresh(); });
                });
                menu.addItem ("Show in Explorer", [this] { folder.revealToUser(); });
                menu.showMenuAsync ({});
            }
            else
            {
                setOpen (! isOpen());
            }
        }

    private:
        juce::File folder;
        bool canRemove;
    };

    class SectionItem final : public BaseItem
    {
    public:
        SectionItem (Browser& b, juce::String t, juce::Path i, juce::Colour c) : BaseItem (b), title (std::move (t)), icon (std::move (i)), colour (c) {}

        bool mightContainSubItems() override { return true; }
        juce::String getUniqueName() const override { return "section:" + title; }
        bool canBeSelected() const override { return false; }
        void itemClicked (const juce::MouseEvent&) override { setOpen (! isOpen()); }

        void paintItem (juce::Graphics& g, int w, int h) override
        {
            paintRow (g, w, h, icon, colour, title, getNumSubItems() > 0 ? juce::String (getNumSubItems()) : juce::String(), false, true);
        }

    private:
        juce::String title;
        juce::Path icon;
        juce::Colour colour;
    };

    struct NameSorter
    {
        static int compareElements (juce::TreeViewItem* a, juce::TreeViewItem* b)
        {
            return static_cast<BaseItem*> (a)->sortName().compareNatural (static_cast<BaseItem*> (b)->sortName());
        }
    };

    class RootItem final : public BaseItem
    {
    public:
        using BaseItem::BaseItem;
        bool mightContainSubItems() override { return true; }
        juce::String getUniqueName() const override { return "root"; }
    };
}

juce::StringArray Browser::defaultSampleFolders()
{
    juce::StringArray folders;
    const auto music = juce::File::getSpecialLocation (juce::File::userMusicDirectory);
    if (music.isDirectory())
        folders.add (music.getFullPathName());
    return folders;
}

bool Browser::isAudioFile (const juce::File& f)
{
    return f.hasFileExtension ("wav;aif;aiff;flac;ogg;mp3");
}

bool Browser::isSoundFont (const juce::File& f)
{
    return f.hasFileExtension ("sf2");
}

bool Browser::isProject (const juce::File& f)
{
    return f.hasFileExtension ("hmy");
}

Browser::Browser (Context& c)
    : ctx (c)
{
    search.setTextToShowWhenEmpty ("Search plugins and sounds", colours::textFaint);
    search.setFont (font (11.0f));
    search.setIndents (30, 7);
    search.onTextChange = [this] { refresh(); };
    search.onEscapeKey = [this] { search.clear(); refresh(); };
    addAndMakeVisible (search);

    tree.setRootItemVisible (false);
    tree.setIndentSize (14);
    tree.setDefaultOpenness (false);
    tree.setColour (juce::TreeView::backgroundColourId, colours::panel);
    addAndMakeVisible (tree);

    addFolderButton.onClick = [this] { addFolder(); };
    addAndMakeVisible (addFolderButton);

    ctx.plugins.getKnownPlugins().addChangeListener (this);
    refresh();
}

Browser::~Browser()
{
    ctx.plugins.getKnownPlugins().removeChangeListener (this);
    tree.setRootItem (nullptr);
}

void Browser::changeListenerCallback (juce::ChangeBroadcaster*)
{
    refresh();
}

void Browser::addFolder()
{
    auto chooser = std::make_shared<juce::FileChooser> ("Add a folder with samples or SoundFonts",
                                                        juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories, [this, chooser] (const juce::FileChooser& fc)
    {
        const auto dir = fc.getResult();
        if (! dir.isDirectory())
            return;

        auto folders = juce::StringArray::fromLines (ctx.settings.getValue ("browserFolders"));
        folders.removeEmptyStrings();
        folders.addIfNotAlreadyThere (dir.getFullPathName());
        ctx.settings.setValue ("browserFolders", folders.joinIntoString ("\n"));
        refresh();
    });
}

void Browser::refresh()
{
    const auto openness = tree.getOpennessState (true);
    tree.setRootItem (nullptr);
    root = std::make_unique<RootItem> (*this);

    const auto query = search.getText().trim();
    auto matches = [&query] (const juce::String& s) { return query.isEmpty() || s.containsIgnoreCase (query); };

    auto* instruments = new SectionItem (*this, "Instruments", icons::piano(), colours::accent);
    auto* effects = new SectionItem (*this, "Effects", icons::plug(), colours::lilac);
    for (auto& d : ctx.plugins.getKnownPlugins().getTypes())
        if (matches (d.name) || matches (d.manufacturerName))
            (d.isInstrument ? instruments : effects)->addSubItem (new PluginItem (*this, d));

    NameSorter sorter;
    instruments->sortSubItems (sorter);
    effects->sortSubItems (sorter);
    root->addSubItem (instruments);
    root->addSubItem (effects);

    auto folders = juce::StringArray::fromLines (ctx.settings.getValue ("browserFolders", defaultSampleFolders().joinIntoString ("\n")));
    folders.removeEmptyStrings();

    auto* library = new SectionItem (*this, query.isEmpty() ? "Library" : "Matching files", icons::folder(), colours::success);

    if (query.isEmpty())
    {
        for (auto& f : folders)
            if (juce::File (f).isDirectory())
                library->addSubItem (new FolderItem (*this, juce::File (f), true));
    }
    else
    {
        int found = 0, visited = 0;
        for (auto& f : folders)
        {
            for (const auto& entry : juce::RangedDirectoryIterator (juce::File (f), true, "*", juce::File::findFiles | juce::File::ignoreHiddenFiles))
            {
                if (++visited > 30000 || found >= 300)
                    break;

                const auto file = entry.getFile();
                if ((isAudioFile (file) || isSoundFont (file) || isProject (file)) && file.getFileName().containsIgnoreCase (query))
                {
                    library->addSubItem (new FileItem (*this, file));
                    ++found;
                }
            }
        }
    }

    root->addSubItem (library);

    auto recent = juce::StringArray::fromLines (ctx.settings.getValue ("recentProjects"));
    recent.removeEmptyStrings();
    auto* projects = new SectionItem (*this, "Recent projects", icons::history(), colours::warning);
    for (auto& p : recent)
        if (juce::File (p).existsAsFile() && matches (juce::File (p).getFileName()))
            projects->addSubItem (new FileItem (*this, juce::File (p)));
    root->addSubItem (projects);

    tree.setRootItem (root.get());

    if (openness != nullptr && query.isEmpty())
        tree.restoreOpennessState (*openness, true);
    else
        for (int i = 0; i < root->getNumSubItems(); ++i)
            root->getSubItem (i)->setOpen (query.isNotEmpty() || i < 1);
}

void Browser::paint (juce::Graphics& g)
{
    g.fillAll (colours::panel);
    g.setColour (colours::text);
    g.setFont (font (12.0f, Weight::semibold));
    g.drawText ("Browser", juce::Rectangle<int> (14, 0, getWidth() - 60, 40), juce::Justification::centredLeft, false);

    drawIcon (g, icons::search(), search.getBounds().toFloat().removeFromLeft (30.0f).reduced (8.0f), colours::textFaint);
    g.setColour (colours::line);
    g.fillRect (getWidth() - 1, 0, 1, getHeight());
}

void Browser::resized()
{
    auto r = getLocalBounds().withTrimmedRight (1);
    auto header = r.removeFromTop (40);
    addFolderButton.setBounds (header.removeFromRight (40).reduced (7));
    search.setBounds (r.removeFromTop (34).reduced (10, 2));
    r.removeFromTop (6);
    tree.setBounds (r.reduced (6, 0));
}
}
