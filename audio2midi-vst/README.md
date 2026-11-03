# Audio2MIDI (VST3)

Drop a WAV or MP3 on the plugin window, get a MIDI file. Drag the MIDI straight onto
FL Studio's Playlist or Piano roll. Notes are found by Spotify's Basic Pitch model, which
is built into the plugin. No Python needed.

Controls: Tempo (type your project BPM), Sensitivity, Min note length, Min velocity.
The sliders re-decode instantly, no need to drop the file again.

## Getting the .vst3 (no Visual Studio needed, nothing installs on your C: drive)

1. Make a free account at github.com and create a new repository (private is fine).
2. Upload everything in this folder to it, including the hidden `.github` folder.
   (Easiest: install GitHub Desktop, or on github.com use "Add file > Upload files"
   and drag the whole folder contents in.)
3. Open the repository's **Actions** tab. The "Build Audio2MIDI" run starts by itself
   (about 10 to 15 minutes the first time). If it doesn't, click it and press "Run workflow".
4. When it shows a green tick, open the run and download **Audio2MIDI-Windows** from the
   Artifacts section at the bottom.
5. Unzip it. Copy the whole `Audio2MIDI.vst3` folder to `C:\Program Files\Common Files\VST3`
   (or any VST3 folder FL Studio scans, for example one on E:).
6. In FL Studio: Options > Manage plugins > Find installed plugins.
   Load Audio2MIDI on a mixer insert slot.

The `Standalone` folder in the download is the same thing as a normal program, handy for
testing without FL Studio.

## Notes

- Keep `onnxruntime.dll` and the other DLLs inside the `.vst3` folder. The plugin loads
  them from there.
- The MIDI file is written next to the audio file you dropped, with the same name.
- Tests: `tests/` holds a headless checker that compares the C++ note detection against
  the original Python implementation.
- Licenses: JUCE is AGPLv3 or commercial. That is fine for personal use. If you ever sell
  or distribute the plugin, you need either a JUCE commercial license or to publish
  the source under the AGPL. Basic Pitch and ONNX Runtime are Apache 2.0 / MIT.
