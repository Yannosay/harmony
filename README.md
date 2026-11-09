<p align="center">
  <img src="assets/brand/harmony-icon-256.png" width="96" alt="Harmony logo">
</p>

<h1 align="center">Yannosay Harmony</h1>

<p align="center">
  A fast, dark, pattern-based digital audio workstation by Yannosay Productions.<br>
  <a href="https://harmony.yannosay.com">harmony.yannosay.com</a> ·
  <a href="https://github.com/Yannosay/harmony/releases/latest">Download for Windows</a>
</p>

![Playlist and channel rack](docs/harmony-playlist.png)

## What it is

Harmony is a native C++20 DAW built on JUCE 8. It ships with **no built-in instruments or
effects**: it is a host. Bring your own VST, VST3, LV2 plugins, SoundFonts and samples.

- **Plugin hosting**: VST3, VST 2.x and LV2 instruments and effects, with out-of-process
  scanning so a crashing plugin can never take the DAW down, automatic plugin delay
  compensation, and plugin state saved with your project.
- **SoundFonts**: load any `.sf2` as a channel and browse its presets.
- **Sampler**: one-shot or pitched sample playback with high-quality interpolation.
- **Channel rack and step sequencer**, **pattern and song modes**, **playlist** with pattern
  and audio clips, **piano roll** with ghost notes, scale highlighting and scale lock.
- **Mixer** with 24 inserts plus master, 10 effect slots each, mute, solo, pan and meters.
- **Chord generator** with 15 progressions, smooth voice leading and arpeggio rhythms.
- **Drum generator** for 11 styles (trap, drill, house, techno, drum & bass and more).
- **Rendering** to WAV (16/24/32-bit float), FLAC, OGG Vorbis and AIFF, with stems,
  normalisation, dither and background rendering. Headless rendering from the command line.
- **Time machine**: automatic project snapshots you can open at any time, plus crash recovery.
- **Command palette** (`Ctrl+K`) for every action, FL-style shortcuts, smooth inertial
  scrolling and zooming everywhere.
- **Automatic updates** from GitHub Releases, verified with SHA-256 before installing.

![Piano roll](docs/harmony-pianoroll.png)

## Shortcuts

| Action | Keys |
|--------|------|
| Play / pause, stop | `Space`, `Ctrl+Space` |
| Pattern / song mode | `L` |
| Channel rack, piano roll, browser, mixer | `F6`, `F7`, `F8`, `F9` |
| Command palette | `Ctrl+K` |
| Render | `Ctrl+R` |
| Chord generator, drum generator | `Ctrl+G`, `Ctrl+Shift+G` |
| Draw, select, slice, mute tools | `P`, `E`, `C`, `T` |
| Duplicate, quantize, humanize | `Ctrl+D`, `Ctrl+Shift+Q`, `Alt+H` |
| Zoom, scroll, vertical zoom | `Ctrl+wheel`, `wheel` / `Shift+wheel`, `Alt+wheel` |
| Time machine | `Ctrl+H` |

Press `F1` in Harmony for the complete list.

## Command line

```
Harmony.exe project.hmy                      open a project
Harmony.exe --render project.hmy out.wav     render without opening a window (wav, flac, ogg, aiff)
```

## Building

Requirements: CMake 3.25+, Ninja, and Visual Studio 2022 (MSVC) on Windows.

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
build/HarmonyTests_artefacts/Release/HarmonyTests
```

The installer is built with Inno Setup 6: `iscc /DAppVersion=1.0.0 installer/harmony.iss`.

## Releasing

1. Bump `VERSION`, commit, and push a matching tag (`v1.0.1`). The release workflow builds,
   tests and packages `YannosayHarmony-<version>-Setup-x64.exe` plus `SHA256SUMS.txt`.
2. Publish it on the website, which is also the primary update feed:

   ```
   cd website
   node scripts/sync-release.mjs path/to/YannosayHarmony-1.0.1-Setup-x64.exe
   npm run deploy
   ```

`sync-release.mjs` writes `public/latest.json` (version, download URL, SHA-256). Every installed
copy of Harmony checks that manifest (falling back to GitHub Releases), downloads the new
installer in the background, verifies its SHA-256 and installs it silently when Harmony closes.

## License

Harmony is free software under the [GNU AGPL v3](LICENSE). Third-party components are
listed in [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
