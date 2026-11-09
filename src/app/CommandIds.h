#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace harmony::cmd
{
enum : juce::CommandID
{
    newProject = 0x2000,
    openProject,
    saveProject,
    saveProjectAs,
    exportAudio,
    projectHistory,
    quit,

    undo,
    redo,
    cut,
    copy,
    paste,
    deleteSelection,
    selectAll,
    duplicate,
    quantize,
    humanize,
    toggleMuteSelection,

    playPause,
    stop,
    record,
    songMode,
    metronome,
    returnToStart,
    panic,
    tapTempo,

    showPlaylist,
    showChannelRack,
    showPianoRoll,
    showMixer,
    toggleBrowser,
    commandPalette,
    fullscreen,

    addInstrument,
    addSoundFont,
    addSampler,
    newPattern,
    clonePattern,
    nextPattern,
    previousPattern,

    chordGenerator,
    drumGenerator,
    scanPlugins,
    audioSettings,
    preferences,

    toolDraw,
    toolSelect,
    toolSlice,
    toolMute,

    shortcuts,
    checkUpdates,
    about,
    website
};
}
