#include "app/History.h"

namespace harmony
{
History::History (Project& p, const juce::File& dataDirectory, std::function<void()> captureStates)
    : project (p), root (dataDirectory.getChildFile ("History")), marker (dataDirectory.getChildFile ("session.lock")),
      capture (std::move (captureStates))
{
    crashed = marker.existsAsFile();
    dataDirectory.createDirectory();
    marker.replaceWithText (juce::String (juce::Time::getCurrentTime().toMilliseconds()));
    setIntervalMinutes (3);
}

History::~History()
{
    stopTimer();
}

void History::setIntervalMinutes (int minutes)
{
    if (minutes <= 0)
        stopTimer();
    else
        startTimer (minutes * 60 * 1000);
}

void History::timerCallback()
{
    snapshot ("Autosave");
}

bool History::snapshot (const juce::String& reason)
{
    if (project.getChangeCounter() == lastSavedCounter && reason == "Autosave")
        return false;

    if (project.channelsTree().getNumChildren() == 0 && project.songLength() == 0)
        return false;

    if (capture)
        capture();

    const auto folder = root.getChildFile (juce::File::createLegalFileName (project.getDisplayName()));
    folder.createDirectory();

    auto state = project.getState().createCopy();
    state.setProperty ("historyReason", reason, nullptr);
    state.setProperty ("historySource", project.getFile().getFullPathName(), nullptr);

    const auto file = folder.getChildFile (juce::Time::getCurrentTime().formatted ("%Y-%m-%d_%H-%M-%S") + ".hmy");
    if (! Project::writeStateToFile (state, file))
        return false;

    lastSavedCounter = project.getChangeCounter();
    prune (folder);
    return true;
}

void History::prune (const juce::File& folder) const
{
    auto files = folder.findChildFiles (juce::File::findFiles, false, "*.hmy");
    files.sort();
    while (files.size() > 80)
    {
        files.getFirst().deleteFile();
        files.remove (0);
    }
}

juce::Array<History::Entry> History::list() const
{
    juce::Array<Entry> entries;
    for (auto& f : root.findChildFiles (juce::File::findFiles, true, "*.hmy"))
    {
        Entry e;
        e.file = f;
        e.time = f.getLastModificationTime();
        e.project = f.getParentDirectory().getFileName();
        entries.add (e);
    }

    std::sort (entries.begin(), entries.end(), [] (const Entry& a, const Entry& b) { return a.time > b.time; });
    return entries;
}

juce::File History::latestSnapshot() const
{
    auto entries = list();
    return entries.isEmpty() ? juce::File() : entries.getFirst().file;
}

void History::markCleanExit()
{
    marker.deleteFile();
}
}
