# Third-party notices

Yannosay Harmony is distributed under the GNU Affero General Public License v3.0 (see `LICENSE`).
It includes or links the following third-party components.

| Component | Use | License |
|-----------|-----|---------|
| [JUCE](https://github.com/juce-framework/JUCE) 8.0.15 | Application framework, audio devices, plugin hosting (VST3, LV2) | AGPLv3 |
| [VST3 SDK](https://github.com/steinbergmedia/vst3sdk) (bundled with JUCE) | VST3 plugin hosting | MIT |
| [FST](https://git.iem.at/zmoelnig/FST) | Reverse-engineered VST 2.x interface headers for hosting VST2 plugins | GPLv3 or later |
| [TinySoundFont](https://github.com/schellingb/TinySoundFont) | SoundFont 2 (.sf2) playback | MIT |
| [Unbounded](https://github.com/googlefonts/unbounded) | User interface typeface | SIL Open Font License 1.1 |
| FLAC, Ogg Vorbis (bundled with JUCE) | Audio file import and export | BSD-style |

The FST header (`third_party/fst/fst.h`) was changed in one place: the `keyName` member of
`MidiKeyName` uses a fixed size of 64 characters so the structure is complete in C++.

VST is a trademark of Steinberg Media Technologies GmbH.
