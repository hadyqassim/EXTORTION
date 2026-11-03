#pragma once
// Port of Spotify's Basic Pitch inference + note decoding (Apache 2.0).
// Plain C++ plus ONNX Runtime, no JUCE, so it can be tested headless.

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace bp
{
constexpr int kSampleRate  = 22050;
constexpr int kFftHop      = 256;
constexpr int kNumPitches  = 88;   // piano range, MIDI 21..108
constexpr int kMidiOffset  = 21;

struct NoteEvent
{
    double start = 0.0;  // seconds
    double end   = 0.0;  // seconds
    int    pitch = 0;    // MIDI note number
    float  amplitude = 0.0f; // 0..1, mean frame activation
};

// Raw model output, kept so sliders can re-decode without re-running the network.
struct Posteriors
{
    int nFrames = 0;
    std::vector<float> frames;  // nFrames x 88, row-major
    std::vector<float> onsets;  // nFrames x 88, row-major
};

struct DecodeParams
{
    float onsetThreshold = 0.5f;
    float frameThreshold = 0.3f;
    int   minNoteFrames  = 11;
};

class Engine
{
public:
    Engine();
    ~Engine();

    // Load the ONNX model from memory. Returns false and fills `error` on failure.
    bool loadModel (const void* data, size_t size, std::string& error);

    // Audio must be mono, 22050 Hz, float. Returns false on failure or cancel.
    bool runModel (const std::vector<float>& audio22k,
                   Posteriors& out,
                   std::string& error,
                   const std::function<void (float)>& progress = {},
                   const std::atomic<bool>* cancel = nullptr);

    // Pure post-processing, cheap enough to rerun on every slider change.
    static std::vector<NoteEvent> decode (const Posteriors& p,
                                          const DecodeParams& params,
                                          const std::atomic<bool>* cancel = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
} // namespace bp
