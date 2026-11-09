#include "ui/Dialogs.h"
#include "app/History.h"
#include "model/ChordGenerator.h"
#include "model/DrumGenerator.h"
#include "model/MusicTheory.h"
#include "engine/AudioEngine.h"
#include "update/Version.h"
#include <set>

namespace harmony::ui
{
void askForText (const juce::String& title, const juce::String& label, const juce::String& initial,
                 std::function<void (const juce::String&)> onOk)
{
    auto* window = new juce::AlertWindow (title, {}, juce::MessageBoxIconType::NoIcon);
    window->addTextEditor ("text", initial, label);
    window->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    if (auto* editor = window->getTextEditor ("text"))
    {
        editor->setFont (font (12.0f));
        editor->selectAll();
    }

    window->enterModalState (true, juce::ModalCallbackFunction::create ([window, onOk] (int result)
    {
        if (result == 1 && onOk)
            onOk (window->getTextEditorContents ("text"));
    }), true);
}

namespace
{
    class Panel : public juce::Component
    {
    protected:
        struct Row
        {
            juce::String label;
            juce::Component* component;
            int height;
        };

        void addRow (const juce::String& label, juce::Component& c, int height = 30)
        {
            rows.push_back ({ label, &c, height });
            addAndMakeVisible (c);
        }

        void layoutRows (juce::Rectangle<int> area, int labelWidth = 150)
        {
            for (auto& r : rows)
            {
                auto row = area.removeFromTop (r.height);
                row.removeFromLeft (labelWidth);
                r.component->setBounds (row);
                area.removeFromTop (8);
            }
        }

        void paintRowLabels (juce::Graphics& g, int labelWidth = 150)
        {
            g.setColour (colours::textDim);
            g.setFont (font (11.0f));
            for (auto& r : rows)
                if (r.label.isNotEmpty())
                    g.drawText (r.label, r.component->getBounds().withX (r.component->getX() - labelWidth).withWidth (labelWidth - 12),
                                juce::Justification::centredLeft, false);
        }

        static void fill (juce::ComboBox& box, const juce::StringArray& items, int selectedIndex)
        {
            box.addItemList (items, 1);
            box.setSelectedItemIndex (juce::jlimit (0, items.size() - 1, selectedIndex), juce::dontSendNotification);
        }

        std::vector<Row> rows;
    };

    class ChordGeneratorPanel final : public Panel
    {
    public:
        ChordGeneratorPanel (Context& c, std::function<void (const std::vector<NoteData>&, bool)> a)
            : ctx (c), apply (std::move (a))
        {
            setSize (560, 560);
            const auto& state = ctx.project.getState();

            juce::StringArray roots;
            for (int i = 0; i < 12; ++i)
                roots.add (theory::pitchClassName (i));
            fill (rootBox, roots, state[id::scaleRoot]);

            juce::StringArray scales;
            int scaleIndex = 1;
            for (auto& s : theory::allScales())
            {
                if (s.isHeptatonic())
                {
                    if (s.name == theory::findScale (state[id::scaleType].toString()).name)
                        scaleIndex = scales.size();
                    scales.add (s.name);
                }
            }
            fill (scaleBox, scales, scaleIndex);

            juce::StringArray progressions;
            for (auto& p : chordProgressions())
                progressions.add (p.name);
            fill (progressionBox, progressions, 1);
            fill (sizeBox, { "Triads", "Seventh chords", "Ninth chords" }, 0);
            fill (octaveBox, { "2", "3", "4", "5", "6" }, 2);
            fill (barsBox, { "1 bar", "2 bars", "4 bars" }, 0);
            fill (rhythmBox, chordRhythmNames(), 0);

            velocity.setRange (0.1, 1.0, 0.01);
            velocity.setValue (0.8, juce::dontSendNotification);
            velocity.setSliderStyle (juce::Slider::LinearHorizontal);
            velocity.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);

            voiceLeading.setButtonText ("Smooth voice leading");
            voiceLeading.setToggleState (true, juce::dontSendNotification);
            bass.setButtonText ("Add bass notes");
            bass.setToggleState (true, juce::dontSendNotification);
            replace.setButtonText ("Replace the channel's notes in this pattern");
            replace.setToggleState (true, juce::dontSendNotification);

            addRow ("Key", rootBox);
            addRow ("Scale", scaleBox);
            addRow ("Progression", progressionBox);
            addRow ("Chords", sizeBox);
            addRow ("Octave", octaveBox);
            addRow ("Length", barsBox);
            addRow ("Rhythm", rhythmBox);
            addRow ("Velocity", velocity);
            addRow ({}, voiceLeading, 24);
            addRow ({}, bass, 24);
            addRow ({}, replace, 24);

            for (auto* box : { &rootBox, &scaleBox, &progressionBox, &sizeBox, &octaveBox, &barsBox, &rhythmBox })
                box->onChange = [this] { updatePreview(); };
            for (auto* t : { &voiceLeading, &bass })
                t->onClick = [this] { updatePreview(); };

            randomButton.onClick = [this]
            {
                seed = juce::Random::getSystemRandom().nextInt (1 << 30) + 1;
                progressionBox.setSelectedItemIndex (0, juce::dontSendNotification);
                updatePreview();
            };
            generateButton.setColour (juce::TextButton::buttonColourId, colours::accent);
            generateButton.setColour (juce::TextButton::textColourOffId, colours::background);
            generateButton.onClick = [this]
            {
                const auto result = generate();
                apply (result.notes, replace.getToggleState());
                if (ctx.notify)
                    ctx.notify ("Generated " + result.chordNames.joinIntoString ("  "), false);
                if (ctx.closePanel)
                    ctx.closePanel();
            };
            addAndMakeVisible (randomButton);
            addAndMakeVisible (generateButton);
            updatePreview();
        }

        ChordGenResult generate() const
        {
            ChordGenSettings s;
            s.root = rootBox.getSelectedItemIndex();
            s.scale = scaleBox.getText();
            s.progression = progressionBox.getSelectedItemIndex();
            s.chordSize = 3 + sizeBox.getSelectedItemIndex();
            s.octave = octaveBox.getText().getIntValue();
            s.barsPerChord = 1 << barsBox.getSelectedItemIndex();
            s.rhythm = (ChordRhythm) rhythmBox.getSelectedItemIndex();
            s.voiceLeading = voiceLeading.getToggleState();
            s.addBass = bass.getToggleState();
            s.velocity = (float) velocity.getValue();
            s.seed = (juce::uint32) seed;
            return generateChords (s, ctx.project.ticksPerBar(), ctx.project.ticksPerBeat());
        }

        void updatePreview()
        {
            const auto result = generate();
            chords = result.chordNames;
            numerals = result.numerals;
            repaint();
        }

        void paint (juce::Graphics& g) override
        {
            paintRowLabels (g);

            auto strip = previewArea.toFloat();
            g.setColour (colours::background);
            g.fillRoundedRectangle (strip, 8.0f);

            const int n = juce::jmax (1, chords.size());
            const float w = strip.getWidth() / (float) n;
            for (int i = 0; i < chords.size(); ++i)
            {
                auto cell = juce::Rectangle<float> (strip.getX() + w * (float) i, strip.getY(), w, strip.getHeight()).reduced (4.0f);
                g.setColour (palette::notePink.withAlpha (0.12f + 0.04f * (float) (i % 2)));
                g.fillRoundedRectangle (cell, 6.0f);
                g.setColour (colours::accent);
                g.setFont (font (13.0f, Weight::semibold));
                g.drawText (chords[i], cell.withTrimmedBottom (cell.getHeight() * 0.45f), juce::Justification::centredBottom, false);
                g.setColour (colours::textDim);
                g.setFont (font (10.0f));
                g.drawText (numerals[i], cell.withTrimmedTop (cell.getHeight() * 0.58f), juce::Justification::centredTop, false);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (22, 16);
            previewArea = r.removeFromTop (64);
            r.removeFromTop (16);
            auto buttons = r.removeFromBottom (36);
            generateButton.setBounds (buttons.removeFromRight (150));
            buttons.removeFromRight (8);
            randomButton.setBounds (buttons.removeFromRight (130));
            layoutRows (r);
        }

    private:
        Context& ctx;
        std::function<void (const std::vector<NoteData>&, bool)> apply;
        juce::ComboBox rootBox, scaleBox, progressionBox, sizeBox, octaveBox, barsBox, rhythmBox;
        juce::Slider velocity;
        juce::ToggleButton voiceLeading, bass, replace;
        juce::TextButton randomButton { "Surprise me" }, generateButton { "Generate" };
        juce::StringArray chords, numerals;
        juce::Rectangle<int> previewArea;
        int seed = 7;
    };

    class DrumGeneratorPanel final : public Panel
    {
    public:
        explicit DrumGeneratorPanel (Context& c) : ctx (c)
        {
            setSize (600, 640);
            fill (styleBox, drumStyleNames(), 0);
            fill (barsBox, { "1 bar", "2 bars", "4 bars" }, 1);

            for (auto* s : { &density, &humanize, &variation })
            {
                s->setRange (0.0, 1.0, 0.01);
                s->setSliderStyle (juce::Slider::LinearHorizontal);
                s->setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
            }
            density.setValue (0.5, juce::dontSendNotification);
            humanize.setValue (0.2, juce::dontSendNotification);
            variation.setValue (0.4, juce::dontSendNotification);

            addRow ("Style", styleBox);
            addRow ("Length", barsBox);
            addRow ("Density", density);
            addRow ("Humanize", humanize);
            addRow ("Variation", variation);

            juce::StringArray channelNames { "None" };
            for (auto ch : ctx.project.channelsTree())
            {
                channelNames.add (ch[id::name].toString());
                channelIds.add (ch[id::uid]);
            }

            for (int r = 0; r < (int) DrumRole::count; ++r)
            {
                auto* box = roleBoxes.add (new juce::ComboBox());
                box->addItemList (channelNames, 1);
                box->setSelectedItemIndex (0, juce::dontSendNotification);

                for (int i = 0; i < channelIds.size(); ++i)
                {
                    bool matched = false;
                    if (guessDrumRole (channelNames[i + 1], matched) == (DrumRole) r && matched)
                    {
                        box->setSelectedItemIndex (i + 1, juce::dontSendNotification);
                        break;
                    }
                }
                addRow (drumRoleName ((DrumRole) r), *box, 28);
            }

            randomButton.onClick = [this] { seed = juce::Random::getSystemRandom().nextInt (1 << 30) + 1; generate(); };
            generateButton.setColour (juce::TextButton::buttonColourId, colours::accent);
            generateButton.setColour (juce::TextButton::textColourOffId, colours::background);
            generateButton.onClick = [this] { generate(); };
            addAndMakeVisible (randomButton);
            addAndMakeVisible (generateButton);
        }

        void generate()
        {
            DrumGenSettings s;
            s.style = styleBox.getText();
            s.bars = 1 << barsBox.getSelectedItemIndex();
            s.density = (float) density.getValue();
            s.humanize = (float) humanize.getValue();
            s.variation = (float) variation.getValue();
            s.seed = (juce::uint32) seed;

            std::array<int, (size_t) DrumRole::count> mapping {};
            bool any = false;
            for (int r = 0; r < (int) DrumRole::count; ++r)
            {
                const int idx = roleBoxes[r]->getSelectedItemIndex();
                mapping[(size_t) r] = idx > 0 ? channelIds[idx - 1] : 0;
                any = any || idx > 0;
            }

            if (! any)
            {
                if (ctx.notify)
                    ctx.notify ("Assign at least one channel to a drum role", true);
                return;
            }

            auto& project = ctx.project;
            auto pattern = project.currentPattern();
            project.beginTransaction ("Generate drums");

            for (int i = pattern.getNumChildren(); --i >= 0;)
            {
                const int ch = pattern.getChild (i)[id::channel];
                if (std::find (mapping.begin(), mapping.end(), ch) != mapping.end() && ch != 0)
                    pattern.removeChild (i, project.um());
            }

            int count = 0;
            for (auto& hit : generateDrums (s, ticksPerStep))
            {
                const int ch = mapping[(size_t) hit.role];
                if (ch == 0)
                    continue;

                NoteData n;
                n.channel = ch;
                n.key = 60;
                n.start = hit.start;
                n.length = hit.length;
                n.velocity = hit.velocity;
                project.addNote (pattern, n);
                ++count;
            }

            pattern.setProperty (id::steps, s.bars * 16, project.um());
            if (ctx.notify)
                ctx.notify (s.style + " groove: " + juce::String (count) + " hits over " + juce::String (s.bars) + (s.bars == 1 ? " bar" : " bars"), false);
        }

        void paint (juce::Graphics& g) override
        {
            paintRowLabels (g);
            if (channelIds.isEmpty())
            {
                g.setColour (colours::warning);
                g.setFont (font (11.0f));
                g.drawText ("Add drum channels (samples or a drum plugin) first, then map them to roles.", getLocalBounds().reduced (22).removeFromBottom (70),
                            juce::Justification::centredTop, true);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (22, 16);
            auto buttons = r.removeFromBottom (36);
            generateButton.setBounds (buttons.removeFromRight (150));
            buttons.removeFromRight (8);
            randomButton.setBounds (buttons.removeFromRight (130));
            layoutRows (r);
        }

    private:
        Context& ctx;
        juce::ComboBox styleBox, barsBox;
        juce::Slider density, humanize, variation;
        juce::OwnedArray<juce::ComboBox> roleBoxes;
        juce::Array<int> channelIds;
        juce::TextButton randomButton { "New variation" }, generateButton { "Generate" };
        int seed = 11;
    };

    class RenderPanel final : public Panel
    {
    public:
        RenderPanel (Context& c, std::function<void (engine::RenderSettings, bool)> s)
            : ctx (c), start (std::move (s))
        {
            setSize (560, 520);
            fill (formatBox, engine::Renderer::formatNames(), 0);
            formatBox.onChange = [this] { updateBits(); updatePath(); };
            fill (rateBox, { "44100 Hz", "48000 Hz", "88200 Hz", "96000 Hz" }, 1);
            fill (rangeBox, { "Song", "Current pattern", "Loop region" }, ctx.project.songLength() > 0 ? 0 : 1);
            tail.setRange (0.0, 10.0, 0.5);
            tail.setValue (2.0, juce::dontSendNotification);
            tail.setSliderStyle (juce::Slider::LinearHorizontal);
            tail.setTextBoxStyle (juce::Slider::TextBoxRight, false, 50, 22);
            tail.setTextValueSuffix (" s");

            normalize.setButtonText ("Normalize to -0.3 dBFS");
            stems.setButtonText ("Also export stems (one file per used mixer insert)");
            minimise.setButtonText ("Render in background and minimise Harmony");

            addRow ("Format", formatBox);
            addRow ("Bit depth", bitsBox);
            addRow ("Sample rate", rateBox);
            addRow ("Range", rangeBox);
            addRow ("Tail", tail);
            addRow ("Output", pathButton);
            addRow ({}, normalize, 24);
            addRow ({}, stems, 24);
            addRow ({}, minimise, 24);

            pathButton.onClick = [this] { choosePath(); };
            renderButton.setColour (juce::TextButton::buttonColourId, colours::accent);
            renderButton.setColour (juce::TextButton::textColourOffId, colours::background);
            renderButton.onClick = [this] { begin(); };
            addAndMakeVisible (renderButton);

            const auto defaultDir = juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Harmony Renders");
            output = defaultDir.getChildFile (juce::File::createLegalFileName (ctx.project.getDisplayName()) + ".wav");
            updateBits();
            updatePath();
        }

        void updateBits()
        {
            const auto depths = engine::Renderer::bitDepthsFor (formatBox.getText());
            bitsBox.clear (juce::dontSendNotification);
            for (auto d : depths)
                bitsBox.addItem (juce::String (d) + (d == 32 ? "-bit float" : "-bit"), d);
            bitsBox.setSelectedId (depths.contains (24) ? 24 : depths.getFirst(), juce::dontSendNotification);
        }

        void updatePath()
        {
            output = output.withFileExtension (engine::Renderer::extensionFor (formatBox.getText()));
            pathButton.setButtonText (output.getFullPathName());
        }

        void choosePath()
        {
            chooser = std::make_unique<juce::FileChooser> ("Render to", output, "*" + engine::Renderer::extensionFor (formatBox.getText()));
            chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting, [this] (const juce::FileChooser& fc)
            {
                if (fc.getResult() != juce::File())
                {
                    output = fc.getResult();
                    updatePath();
                }
            });
        }

        void begin()
        {
            engine::RenderSettings s;
            s.output = output;
            s.format = formatBox.getText();
            s.bitDepth = bitsBox.getSelectedId();
            s.sampleRate = rateBox.getText().getDoubleValue();
            s.bpm = ctx.project.getBpm();
            s.tailSeconds = tail.getValue();
            s.normalize = normalize.getToggleState();

            const auto& state = ctx.project.getState();
            switch (rangeBox.getSelectedItemIndex())
            {
                case 1:
                    s.songMode = false;
                    s.startTick = 0;
                    s.endTick = ctx.project.patternLength (ctx.project.currentPattern());
                    break;
                case 2:
                    s.songMode = true;
                    s.startTick = state[id::loopStart];
                    s.endTick = state[id::loopEnd];
                    break;
                default:
                    s.songMode = true;
                    s.startTick = 0;
                    s.endTick = ctx.project.songLength();
                    break;
            }

            if (s.endTick <= s.startTick)
            {
                if (ctx.notify)
                    ctx.notify (rangeBox.getSelectedItemIndex() == 2 ? "Set a loop region on the playlist ruler first" : "There is nothing to render in this range", true);
                return;
            }

            if (stems.getToggleState())
            {
                std::set<int> used;
                for (auto ch : ctx.project.channelsTree())
                    used.insert ((int) ch[id::insert]);
                for (int index : used)
                    if (index > 0)
                        s.stemInserts.push_back ({ index, ctx.project.findInsert (index)[id::name].toString() });
            }

            start (s, minimise.getToggleState());
        }

        void paint (juce::Graphics& g) override { paintRowLabels (g); }

        void resized() override
        {
            auto r = getLocalBounds().reduced (22, 16);
            renderButton.setBounds (r.removeFromBottom (38).removeFromRight (160));
            layoutRows (r, 130);
        }

    private:
        Context& ctx;
        std::function<void (engine::RenderSettings, bool)> start;
        juce::ComboBox formatBox, bitsBox, rateBox, rangeBox;
        juce::Slider tail;
        juce::ToggleButton normalize, stems, minimise;
        juce::TextButton pathButton, renderButton { "Render" };
        juce::File output;
        std::unique_ptr<juce::FileChooser> chooser;
    };

    class AudioSettingsPanel final : public juce::Component
    {
    public:
        explicit AudioSettingsPanel (Context& ctx)
            : selector (ctx.engine.getDeviceManager(), 0, 2, 2, 2, true, false, true, false)
        {
            setSize (620, 520);
            addAndMakeVisible (selector);
        }

        void resized() override { selector.setBounds (getLocalBounds().reduced (10)); }

    private:
        juce::AudioDeviceSelectorComponent selector;
    };

    class PreferencesPanel final : public Panel
    {
    public:
        PreferencesPanel (Context& c, std::function<void()> scan)
            : ctx (c), onScan (std::move (scan))
        {
            setSize (600, 470);

            folders.setMultiLine (true);
            folders.setReturnKeyStartsNewLine (true);
            folders.setText (ctx.settings.getValue ("pluginFolders"), false);
            folders.setTextToShowWhenEmpty ("Extra VST / VST3 folders, one per line", colours::textFaint);
            folders.onTextChange = [this]
            {
                ctx.settings.setValue ("pluginFolders", folders.getText());
                ctx.plugins.extraFolders = juce::StringArray::fromLines (folders.getText());
                ctx.plugins.extraFolders.removeEmptyStrings();
            };

            fill (autosaveBox, { "Off", "Every minute", "Every 3 minutes", "Every 5 minutes", "Every 10 minutes" }, ctx.settings.getIntValue ("autosaveChoice", 2));
            autosaveBox.onChange = [this]
            {
                static const int minutes[] = { 0, 1, 3, 5, 10 };
                ctx.settings.setValue ("autosaveChoice", autosaveBox.getSelectedItemIndex());
                ctx.history.setIntervalMinutes (minutes[juce::jlimit (0, 4, autosaveBox.getSelectedItemIndex())]);
            };

            autoUpdate.setButtonText ("Download updates automatically and install on restart");
            autoUpdate.setToggleState (ctx.settings.getBoolValue ("autoUpdate", true), juce::dontSendNotification);
            autoUpdate.onClick = [this] { ctx.settings.setValue ("autoUpdate", autoUpdate.getToggleState()); };

            scanButton.onClick = [this] { if (onScan) onScan(); };
            dataButton.onClick = [this] { ctx.settings.getFile().getParentDirectory().revealToUser(); };
            checkButton.onClick = [this] { ctx.commands.invokeDirectly (cmd::checkUpdates, true); };

            addRow ("Plugin folders", folders, 110);
            addRow ({}, scanButton, 32);
            addRow ("Time machine", autosaveBox);
            addRow ({}, autoUpdate, 24);
            addRow ({}, checkButton, 32);
            addRow ({}, dataButton, 32);
        }

        void paint (juce::Graphics& g) override
        {
            paintRowLabels (g);
            g.setColour (colours::textFaint);
            g.setFont (font (10.0f));
            g.drawText (juce::String (ctx.plugins.getKnownPlugins().getNumTypes()) + " plugins known", scanButton.getBounds().translated (scanButton.getWidth() + 12, 0),
                        juce::Justification::centredLeft, false);
        }

        void resized() override
        {
            layoutRows (getLocalBounds().reduced (22, 16));
            for (auto* b : { &scanButton, &checkButton, &dataButton })
                b->setSize (220, b->getHeight());
        }

    private:
        Context& ctx;
        std::function<void()> onScan;
        juce::TextEditor folders;
        juce::ComboBox autosaveBox;
        juce::ToggleButton autoUpdate;
        juce::TextButton scanButton { "Scan for plugins" }, checkButton { "Check for updates now" }, dataButton { "Open user data folder" };
    };

    class HistoryPanel final : public juce::Component, private juce::ListBoxModel
    {
    public:
        HistoryPanel (Context& c, std::function<void (const juce::File&)> r)
            : ctx (c), restore (std::move (r)), entries (c.history.list())
        {
            setSize (560, 520);
            list.setModel (this);
            list.setRowHeight (40);
            addAndMakeVisible (list);
            restoreButton.setColour (juce::TextButton::buttonColourId, colours::accent);
            restoreButton.setColour (juce::TextButton::textColourOffId, colours::background);
            restoreButton.onClick = [this]
            {
                const int row = list.getSelectedRow();
                if (juce::isPositiveAndBelow (row, entries.size()) && restore)
                    restore (entries[row].file);
            };
            snapshotButton.onClick = [this]
            {
                ctx.history.snapshot ("Manual snapshot");
                entries = ctx.history.list();
                list.updateContent();
                list.repaint();
            };
            addAndMakeVisible (restoreButton);
            addAndMakeVisible (snapshotButton);
        }

        int getNumRows() override { return entries.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) override
        {
            if (! juce::isPositiveAndBelow (row, entries.size()))
                return;

            const auto& e = entries.getReference (row);
            if (selected)
            {
                g.setColour (colours::controlHover);
                g.fillRoundedRectangle (juce::Rectangle<float> (0.0f, 2.0f, (float) w, (float) h - 4.0f), 6.0f);
            }

            g.setColour (colours::text);
            g.setFont (font (11.5f, Weight::medium));
            g.drawText (e.project, 14, 4, w - 28, h / 2, juce::Justification::bottomLeft, true);
            g.setColour (colours::textDim);
            g.setFont (font (10.0f));
            g.drawText (e.time.formatted ("%d %b %Y  %H:%M:%S") + "   " + juce::File::descriptionOfSizeInBytes (e.file.getSize()),
                        14, h / 2, w - 28, h / 2 - 4, juce::Justification::topLeft, true);
        }

        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
        {
            if (juce::isPositiveAndBelow (row, entries.size()) && restore)
                restore (entries[row].file);
        }

        void paint (juce::Graphics& g) override
        {
            if (entries.isEmpty())
            {
                g.setColour (colours::textDim);
                g.setFont (font (11.5f));
                g.drawText ("No snapshots yet. Harmony saves a version of your project every few minutes while you work.",
                            getLocalBounds().reduced (30), juce::Justification::centred, true);
            }
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (16, 12);
            auto buttons = r.removeFromBottom (36);
            restoreButton.setBounds (buttons.removeFromRight (180));
            buttons.removeFromRight (8);
            snapshotButton.setBounds (buttons.removeFromRight (170));
            r.removeFromBottom (10);
            list.setBounds (r);
        }

    private:
        Context& ctx;
        std::function<void (const juce::File&)> restore;
        juce::Array<History::Entry> entries;
        juce::ListBox list;
        juce::TextButton restoreButton { "Open this version" }, snapshotButton { "Snapshot now" };
    };

    class AboutPanel final : public juce::Component
    {
    public:
        AboutPanel()
        {
            setSize (520, 460);
            website.setURL (juce::URL ("https://harmony.yannosay.com"));
            website.setButtonText ("harmony.yannosay.com");
            website.setFont (font (11.5f, Weight::medium), false);
            addAndMakeVisible (website);
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().reduced (30, 20);
            if (auto* mark = brandMark())
                mark->drawWithin (g, r.removeFromTop (90).toFloat(), juce::RectanglePlacement::centred, 1.0f);

            g.setColour (colours::text);
            g.setFont (font (22.0f, Weight::semibold));
            g.drawText ("Yannosay Harmony", r.removeFromTop (40), juce::Justification::centred, false);
            g.setColour (colours::accent);
            g.setFont (font (11.5f, Weight::medium));
            g.drawText (update::currentVersion().display(), r.removeFromTop (22), juce::Justification::centred, false);
            g.setColour (colours::textDim);
            g.setFont (font (10.5f));
            g.drawText (juce::String::fromUTF8 ("\xc2\xa9 Yannosay Productions"), r.removeFromTop (22), juce::Justification::centred, false);
            r.removeFromTop (54);
            g.setColour (colours::textFaint);
            g.setFont (font (9.5f));
            g.drawFittedText ("Built with JUCE (AGPLv3), TinySoundFont (MIT) and FST (GPLv3) for VST2 hosting.\n"
                              "VST is a trademark of Steinberg Media Technologies GmbH.\nUnbounded typeface under the SIL Open Font License.",
                              r.removeFromTop (70), juce::Justification::centred, 4);
        }

        void resized() override
        {
            website.setBounds (getLocalBounds().withSizeKeepingCentre (220, 24).translated (0, 40));
        }

    private:
        juce::HyperlinkButton website;
    };

    class ShortcutsPanel final : public juce::Component, private juce::ListBoxModel
    {
    public:
        explicit ShortcutsPanel (juce::ApplicationCommandManager& commands)
        {
            setSize (560, 600);
            auto* mappings = commands.getKeyMappings();

            for (auto category : commands.getCommandCategories())
                for (auto commandId : commands.getCommandsInCategory (category))
                    if (auto* info = commands.getCommandForID (commandId))
                    {
                        juce::StringArray keys;
                        for (auto& k : mappings->getKeyPressesAssignedToCommand (commandId))
                            keys.add (k.getTextDescriptionWithIcons());
                        if (! keys.isEmpty())
                            rows.push_back ({ category, info->shortName, keys.joinIntoString ("  /  ") });
                    }

            rows.push_back ({ "Editors", "Draw / select / slice / mute tool", "P / E / C / T" });
            rows.push_back ({ "Editors", "Transpose selection (octave)", "Up / Down (+Shift)" });
            rows.push_back ({ "Editors", "Move selection by snap (bar)", "Left / Right (+Shift)" });
            rows.push_back ({ "Editors", "Clone while dragging", "Shift + drag" });
            rows.push_back ({ "Editors", "Bypass snapping", "Alt + drag" });
            rows.push_back ({ "Editors", "Zoom / scroll / vertical zoom", "Ctrl+wheel / wheel / Alt+wheel" });
            rows.push_back ({ "Editors", "Pan with inertia", "Middle mouse drag" });

            list.setModel (this);
            list.setRowHeight (30);
            addAndMakeVisible (list);
        }

        int getNumRows() override { return (int) rows.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool) override
        {
            if (! juce::isPositiveAndBelow (row, (int) rows.size()))
                return;

            const auto& r = rows[(size_t) row];
            g.setColour (colours::textFaint);
            g.setFont (font (9.0f, Weight::medium));
            g.drawText (r.category.toUpperCase(), 12, 0, 90, h, juce::Justification::centredLeft, true);
            g.setColour (colours::text);
            g.setFont (font (11.0f));
            g.drawText (r.name, 104, 0, w - 300, h, juce::Justification::centredLeft, true);
            g.setColour (colours::accent);
            g.setFont (font (10.5f, Weight::medium));
            g.drawText (r.keys, w - 200, 0, 188, h, juce::Justification::centredRight, true);
        }

        void resized() override { list.setBounds (getLocalBounds().reduced (12)); }

    private:
        struct Item
        {
            juce::String category, name, keys;
        };

        std::vector<Item> rows;
        juce::ListBox list;
    };
}

std::unique_ptr<juce::Component> createChordGeneratorPanel (Context& ctx, std::function<void (const std::vector<NoteData>&, bool)> apply)
{
    return std::make_unique<ChordGeneratorPanel> (ctx, std::move (apply));
}

std::unique_ptr<juce::Component> createDrumGeneratorPanel (Context& ctx)
{
    return std::make_unique<DrumGeneratorPanel> (ctx);
}

std::unique_ptr<juce::Component> createRenderPanel (Context& ctx, std::function<void (engine::RenderSettings, bool)> start)
{
    return std::make_unique<RenderPanel> (ctx, std::move (start));
}

std::unique_ptr<juce::Component> createAudioSettingsPanel (Context& ctx)
{
    return std::make_unique<AudioSettingsPanel> (ctx);
}

std::unique_ptr<juce::Component> createPreferencesPanel (Context& ctx, std::function<void()> scanPlugins)
{
    return std::make_unique<PreferencesPanel> (ctx, std::move (scanPlugins));
}

std::unique_ptr<juce::Component> createHistoryPanel (Context& ctx, std::function<void (const juce::File&)> restore)
{
    return std::make_unique<HistoryPanel> (ctx, std::move (restore));
}

std::unique_ptr<juce::Component> createAboutPanel()
{
    return std::make_unique<AboutPanel>();
}

std::unique_ptr<juce::Component> createShortcutsPanel (juce::ApplicationCommandManager& commands)
{
    return std::make_unique<ShortcutsPanel> (commands);
}
}
