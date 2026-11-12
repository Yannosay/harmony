#pragma once

#include "model/Project.h"
#include <juce_audio_basics/juce_audio_basics.h>

namespace harmony::midi
{
struct ImportedTrack
{
    juce::String name;
    std::vector<NoteData> notes;
};

juce::MidiFile exportNotes (const Project& project, const std::vector<NoteData>& notes, const juce::String& trackName);
juce::MidiFile exportPattern (const Project& project, const juce::ValueTree& pattern, int onlyChannel = 0);
juce::MidiFile exportSong (const Project& project);
bool writeFile (const juce::MidiFile& midi, const juce::File& target);

std::optional<juce::MidiFile> readFile (const juce::File& source);
std::optional<juce::MidiFile> readData (const void* data, size_t size);
std::vector<ImportedTrack> importTracks (const juce::MidiFile& midi);
std::vector<NoteData> flatten (const std::vector<ImportedTrack>& tracks);
bool isMidiFile (const juce::File& file);
}
