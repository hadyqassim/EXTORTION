#pragma once
#include <juce_audio_formats/juce_audio_formats.h>
#include "BasicPitchEngine.h"

namespace a2m
{
// Reads WAV/MP3/FLAC/OGG (and M4A via Windows Media), mixes to mono, resamples to 22050 Hz.
// Returns an empty vector and fills `error` on failure.
std::vector<float> loadAudioMono22k (const juce::File& file, juce::String& error,
                                     const std::atomic<bool>* cancel = nullptr);

// Writes notes to a Standard MIDI File. Seconds are converted to ticks using `bpm`,
// so the file lines up with a project running at that tempo.
bool writeMidiFile (const std::vector<bp::NoteEvent>& notes, double bpm,
                    int minVelocity, const juce::File& dest);
} // namespace a2m
