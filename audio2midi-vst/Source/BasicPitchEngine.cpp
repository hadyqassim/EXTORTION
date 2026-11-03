#include "BasicPitchEngine.h"

#include <onnxruntime_cxx_api.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>

namespace bp
{
namespace
{
constexpr int    kWindowSamples  = kSampleRate * 2 - kFftHop;          // 43844
constexpr int    kOverlapFrames  = 30;
constexpr int    kOverlapSamples = kOverlapFrames * kFftHop;            // 7680
constexpr int    kHopSamples     = kWindowSamples - kOverlapSamples;    // 36164
constexpr int    kAnnotFps       = kSampleRate / kFftHop;               // 86 (integer division, as in the original)
constexpr int    kAnnotWindowFrames = kAnnotFps * 2;                    // 172
constexpr int    kEnergyTol      = 11;

constexpr const char* kInputName   = "serving_default_input_2:0";
constexpr const char* kNoteName    = "StatefulPartitionedCall:1";
constexpr const char* kOnsetName   = "StatefulPartitionedCall:2";

// Frame index -> seconds, including the original's per-window drift correction.
double frameToSeconds (int idx)
{
    const double hopSec = (double) kFftHop / (double) kSampleRate;
    const double base = idx * hopSec;
    const double windowNumber = std::floor ((double) idx / (double) kAnnotWindowFrames);
    const double windowOffset = hopSec * ((double) kAnnotWindowFrames - (double) kWindowSamples / (double) kFftHop) + 0.0018;
    return base - windowOffset * windowNumber;
}
} // namespace

struct Engine::Impl
{
    Ort::Env env { ORT_LOGGING_LEVEL_WARNING, "audio2midi" };
    std::unique_ptr<Ort::Session> session;
};

Engine::Engine() : impl (new Impl) {}
Engine::~Engine() = default;

bool Engine::loadModel (const void* data, size_t size, std::string& error)
{
    try
    {
        Ort::SessionOptions opts;
        opts.SetGraphOptimizationLevel (GraphOptimizationLevel::ORT_ENABLE_ALL);
        impl->session = std::make_unique<Ort::Session> (impl->env, data, size, opts);
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}

bool Engine::runModel (const std::vector<float>& audio,
                       Posteriors& out,
                       std::string& error,
                       const std::function<void (float)>& progress,
                       const std::atomic<bool>* cancel)
{
    if (! impl->session) { error = "Model not loaded."; return false; }
    if (audio.empty())   { error = "No audio."; return false; }

    try
    {
        const size_t originalLength = audio.size();

        // Front-pad by half the overlap so the first frames have context.
        std::vector<float> padded ((size_t) (kOverlapSamples / 2) + originalLength, 0.0f);
        std::copy (audio.begin(), audio.end(), padded.begin() + kOverlapSamples / 2);

        Ort::MemoryInfo mem = Ort::MemoryInfo::CreateCpu (OrtArenaAllocator, OrtMemTypeDefault);
        const std::array<int64_t, 3> inShape { 1, kWindowSamples, 1 };
        const char* inNames[]  = { kInputName };
        const char* outNames[] = { kNoteName, kOnsetName };

        std::vector<float> window (kWindowSamples);
        std::vector<float> allNotes, allOnsets;
        int framesKept = 0;

        const int nWindows = (int) ((padded.size() + kHopSamples - 1) / kHopSamples);
        int w = 0;

        for (size_t start = 0; start < padded.size(); start += kHopSamples, ++w)
        {
            if (cancel != nullptr && cancel->load()) { error = "Cancelled."; return false; }

            std::fill (window.begin(), window.end(), 0.0f);
            const size_t n = std::min<size_t> (kWindowSamples, padded.size() - start);
            std::copy (padded.begin() + (long) start, padded.begin() + (long) (start + n), window.begin());

            Ort::Value input = Ort::Value::CreateTensor<float> (mem, window.data(), window.size(),
                                                                inShape.data(), inShape.size());
            auto outputs = impl->session->Run (Ort::RunOptions { nullptr }, inNames, &input, 1, outNames, 2);

            const auto shape = outputs[0].GetTensorTypeAndShapeInfo().GetShape(); // 1 x frames x 88
            const int nf = (int) shape[1];
            const int trim = kOverlapFrames / 2;
            const int keep = nf - 2 * trim;
            if (keep <= 0) { error = "Unexpected model output shape."; return false; }

            const float* note  = outputs[0].GetTensorData<float>();
            const float* onset = outputs[1].GetTensorData<float>();
            allNotes.insert  (allNotes.end(),  note  + (size_t) trim * kNumPitches, note  + (size_t) (trim + keep) * kNumPitches);
            allOnsets.insert (allOnsets.end(), onset + (size_t) trim * kNumPitches, onset + (size_t) (trim + keep) * kNumPitches);
            framesKept += keep;

            if (progress) progress ((float) (w + 1) / (float) nWindows);
        }

        // Trim to the original audio length, as the reference implementation does.
        const int expected = (int) std::floor ((double) originalLength * ((double) kAnnotFps / (double) kSampleRate));
        const int nFrames = std::min (framesKept, expected);

        out.nFrames = nFrames;
        out.frames.assign (allNotes.begin(),  allNotes.begin()  + (size_t) nFrames * kNumPitches);
        out.onsets.assign (allOnsets.begin(), allOnsets.begin() + (size_t) nFrames * kNumPitches);
        return true;
    }
    catch (const std::exception& e)
    {
        error = e.what();
        return false;
    }
}

std::vector<NoteEvent> Engine::decode (const Posteriors& p, const DecodeParams& dp, const std::atomic<bool>* cancel)
{
    const int T = p.nFrames;
    const int F = kNumPitches;
    std::vector<NoteEvent> result;
    if (T < 3) return result;

    const std::vector<float>& frames = p.frames;
    std::vector<float> onsets = p.onsets;
    const int minLen = std::max (0, dp.minNoteFrames);
    const float frameThr = dp.frameThreshold;

    auto at = [F] (int t, int f) { return (size_t) t * F + (size_t) f; };

    // Infer extra onsets from large rises in frame energy.
    {
        float maxOnset = 0.0f, maxDiff = 0.0f;
        for (float v : onsets) maxOnset = std::max (maxOnset, v);

        std::vector<float> diff ((size_t) T * F, 0.0f);
        for (int t = 2; t < T; ++t)
            for (int f = 0; f < F; ++f)
            {
                const float cur = frames[at (t, f)];
                const float d = std::min (cur - frames[at (t - 1, f)], cur - frames[at (t - 2, f)]);
                const float c = d < 0.0f ? 0.0f : d;
                diff[at (t, f)] = c;
                maxDiff = std::max (maxDiff, c);
            }
        if (maxDiff > 0.0f)
        {
            const float scale = maxOnset / maxDiff;
            for (size_t i = 0; i < onsets.size(); ++i)
                onsets[i] = std::max (onsets[i], diff[i] * scale);
        }
    }

    struct RawNote { int start, end, pitch; float amp; };
    std::vector<RawNote> raw;

    std::vector<float> remaining = frames;

    auto zeroAround = [&] (int t, int f)
    {
        remaining[at (t, f)] = 0.0f;
        if (f < F - 1) remaining[at (t, f + 1)] = 0.0f;
        if (f > 0)     remaining[at (t, f - 1)] = 0.0f;
    };

    auto meanFrames = [&] (int a, int b, int f)
    {
        double s = 0.0;
        for (int t = a; t < b; ++t) s += frames[at (t, f)];
        return (float) (s / (double) (b - a));
    };

    // Pass 1: walk onset peaks from the end of the audio backwards.
    for (int t = T - 2; t >= 1; --t)
    {
        if (cancel != nullptr && cancel->load()) return {};

        for (int f = F - 1; f >= 0; --f)
        {
            const float v = onsets[at (t, f)];
            if (! (v > onsets[at (t - 1, f)] && v > onsets[at (t + 1, f)])) continue; // local max in time
            if (v < dp.onsetThreshold) continue;

            int i = t + 1, k = 0;
            while (i < T - 1 && k < kEnergyTol)
            {
                if (remaining[at (i, f)] < frameThr) ++k; else k = 0;
                ++i;
            }
            i -= k;

            if (i - t <= minLen) continue;

            for (int tt = t; tt < i; ++tt) zeroAround (tt, f);
            raw.push_back ({ t, i, f + kMidiOffset, meanFrames (t, i, f) });
        }
    }

    // Pass 2: "melodia trick", pick up sustained energy that had no onset.
    for (;;)
    {
        if (cancel != nullptr && cancel->load()) return {};

        float best = frameThr;
        size_t bestIdx = (size_t) -1;
        for (size_t i = 0; i < remaining.size(); ++i)
            if (remaining[i] > best) { best = remaining[i]; bestIdx = i; }
        if (bestIdx == (size_t) -1) break;

        const int mid = (int) (bestIdx / F);
        const int f   = (int) (bestIdx % F);
        remaining[bestIdx] = 0.0f;

        int i = mid + 1, k = 0;
        while (i < T - 1 && k < kEnergyTol)
        {
            if (remaining[at (i, f)] < frameThr) ++k; else k = 0;
            zeroAround (i, f);
            ++i;
        }
        const int iEnd = i - 1 - k;

        i = mid - 1; k = 0;
        while (i > 0 && k < kEnergyTol)
        {
            if (remaining[at (i, f)] < frameThr) ++k; else k = 0;
            zeroAround (i, f);
            --i;
        }
        const int iStart = i + 1 + k;

        if (iEnd - iStart <= minLen) continue;
        raw.push_back ({ iStart, iEnd, f + kMidiOffset, meanFrames (iStart, iEnd, f) });
    }

    result.reserve (raw.size());
    for (const auto& n : raw)
        result.push_back ({ frameToSeconds (n.start), frameToSeconds (n.end), n.pitch, n.amp });

    std::sort (result.begin(), result.end(), [] (const NoteEvent& a, const NoteEvent& b)
    {
        return a.start != b.start ? a.start < b.start : a.pitch < b.pitch;
    });
    return result;
}
} // namespace bp
