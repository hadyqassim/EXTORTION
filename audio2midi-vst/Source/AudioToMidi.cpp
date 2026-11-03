#include "AudioToMidi.h"
#include <cmath>

namespace a2m
{
namespace
{
// Band-limited resampler (Blackman-windowed sinc, 256 sub-sample phases).
std::vector<float> resample (const std::vector<float>& in, double srcRate, double dstRate,
                             const std::atomic<bool>* cancel)
{
    if (std::abs (srcRate - dstRate) < 0.5) return in;

    const double ratio = srcRate / dstRate;           // input samples per output sample
    const double scale = std::max (1.0, ratio) * 1.02; // low-pass just under the output Nyquist
    const int zeroCrossings = 16;
    const int half = (int) std::ceil (zeroCrossings * scale);
    const int taps = 2 * half;
    const int phases = 256;
    const double pi = 3.14159265358979323846;

    std::vector<float> table ((size_t) (phases + 1) * taps);
    for (int p = 0; p <= phases; ++p)
    {
        const double frac = (double) p / phases;
        double sum = 0.0;
        std::vector<double> h ((size_t) taps);
        for (int k = 0; k < taps; ++k)
        {
            const double x = (double) (k - half + 1) - frac;   // input offset from the output position
            const double u = x / (zeroCrossings * scale);
            double v = 0.0;
            if (std::abs (u) < 1.0)
            {
                const double a = pi * x / scale;
                const double sinc = std::abs (a) < 1e-12 ? 1.0 : std::sin (a) / a;
                const double w = 0.42 + 0.5 * std::cos (pi * u) + 0.08 * std::cos (2.0 * pi * u);
                v = sinc * w;
            }
            h[(size_t) k] = v;
            sum += v;
        }
        for (int k = 0; k < taps; ++k)
            table[(size_t) p * taps + k] = (float) (h[(size_t) k] / sum);
    }

    const size_t outN = (size_t) std::floor ((double) in.size() / ratio);
    std::vector<float> out (outN);
    const long long n = (long long) in.size();

    for (size_t j = 0; j < outN; ++j)
    {
        if ((j & 0x3FFFF) == 0 && cancel != nullptr && cancel->load()) return {};

        const double pos = (double) j * ratio;
        long long i0 = (long long) std::floor (pos);
        int phase = (int) std::lround ((pos - (double) i0) * phases);
        const float* h = &table[(size_t) phase * taps];

        double acc = 0.0;
        const long long first = i0 - half + 1;
        for (int k = 0; k < taps; ++k)
        {
            const long long idx = first + k;
            if (idx >= 0 && idx < n) acc += (double) in[(size_t) idx] * h[k];
        }
        out[j] = (float) acc;
    }
    return out;
}
} // namespace

std::vector<float> loadAudioMono22k (const juce::File& file, juce::String& error,
                                     const std::atomic<bool>* cancel)
{
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (file));
    if (reader == nullptr)
    {
        error = "Could not read this file. Try a WAV or MP3.";
        return {};
    }

    const auto total = (long long) reader->lengthInSamples;
    if (total <= 0 || reader->sampleRate <= 0.0)
    {
        error = "The file contains no audio.";
        return {};
    }

    std::vector<float> mono ((size_t) total, 0.0f);
    const int channels = (int) reader->numChannels;
    const int chunk = 1 << 16;
    juce::AudioBuffer<float> buf (channels, chunk);

    for (long long pos = 0; pos < total; pos += chunk)
    {
        if (cancel != nullptr && cancel->load()) return {};

        const int n = (int) std::min<long long> (chunk, total - pos);
        reader->read (&buf, 0, n, pos, true, true);

        for (int i = 0; i < n; ++i)
        {
            float s = 0.0f;
            for (int c = 0; c < channels; ++c) s += buf.getSample (c, i);
            mono[(size_t) (pos + i)] = s / (float) channels;
        }
    }

    auto out = resample (mono, reader->sampleRate, (double) bp::kSampleRate, cancel);
    if (out.empty() && (cancel == nullptr || ! cancel->load()))
        error = "Resampling failed.";
    return out;
}

bool writeMidiFile (const std::vector<bp::NoteEvent>& notes, double bpm,
                    int minVelocity, const juce::File& dest)
{
    constexpr int ppq = 480;
    bpm = juce::jlimit (20.0, 400.0, bpm);
    const double ticksPerSecond = bpm / 60.0 * ppq;

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60'000'000.0 / bpm)), 0.0);

    for (const auto& n : notes)
    {
        const int vel = juce::jlimit (1, 127, (int) std::lround (127.0 * n.amplitude));
        if (vel < minVelocity) continue;

        const double on  = std::max (0.0, n.start) * ticksPerSecond;
        const double off = std::max (on + 1.0, n.end * ticksPerSecond);
        seq.addEvent (juce::MidiMessage::noteOn  (1, n.pitch, (juce::uint8) vel), std::round (on));
        seq.addEvent (juce::MidiMessage::noteOff (1, n.pitch), std::round (off));
    }
    seq.sort();
    seq.updateMatchedPairs();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ppq);
    file.addTrack (seq);

    dest.getParentDirectory().createDirectory();
    dest.deleteFile();
    juce::FileOutputStream out (dest);
    if (! out.openedOk()) return false;
    return file.writeTo (out);
}
} // namespace a2m
