#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "AudioToMidi.h"
#include "OrtLoader.h"
#include "BinaryData.h"

// Runs conversions off the audio and UI threads.
class Audio2MidiProcessor::Worker : public juce::Thread
{
public:
    explicit Worker (Audio2MidiProcessor& o) : juce::Thread ("Audio2MIDI worker"), owner (o) {}

    void run() override
    {
        while (! threadShouldExit())
        {
            wait (-1);
            while (! threadShouldExit())
            {
                bool hadWork = false;
                {
                    const juce::ScopedLock sl (owner.lock);
                    hadWork = owner.pendingFile != juce::File() || owner.redecodeRequested.load();
                }
                if (! hadWork) break;
                owner.workerCycle();
            }
        }
    }

private:
    Audio2MidiProcessor& owner;
};

namespace
{
const char* kBpm = "bpm";
const char* kSens = "sensitivity";
const char* kMinNote = "minNoteMs";
const char* kMinVel = "minVelocity";
}

juce::AudioProcessorValueTreeState::ParameterLayout Audio2MidiProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { kBpm, 1 }, "Tempo (BPM)",
                                                             juce::NormalisableRange<float> (20.0f, 300.0f, 0.01f), 60.0f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { kSens, 1 }, "Sensitivity",
                                                             juce::NormalisableRange<float> (0.0f, 1.0f, 0.01f), 0.5f));
    layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { kMinNote, 1 }, "Min note (ms)",
                                                             juce::NormalisableRange<float> (20.0f, 1000.0f, 1.0f), 120.0f));
    layout.add (std::make_unique<juce::AudioParameterInt> (juce::ParameterID { kMinVel, 1 }, "Min velocity", 1, 127, 1));
    return layout;
}

Audio2MidiProcessor::Audio2MidiProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (auto id : { kBpm, kSens, kMinNote, kMinVel })
        apvts.addParameterListener (id, this);

    worker = std::make_unique<Worker> (*this);
    worker->startThread();
}

Audio2MidiProcessor::~Audio2MidiProcessor()
{
    for (auto id : { kBpm, kSens, kMinNote, kMinVel })
        apvts.removeParameterListener (id, this);

    cancel = true;
    worker->signalThreadShouldExit();
    worker->notify();
    worker->stopThread (10000);
}

bool Audio2MidiProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto out = l.getMainOutputChannelSet();
    return (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo())
           && out == l.getMainInputChannelSet();
}

void Audio2MidiProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    // In a DAW the audio is left alone. Standalone would otherwise loop the mic to the speakers.
    if (wrapperType == wrapperType_Standalone)
        buffer.clear();
    else
        for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
            buffer.clear (ch, 0, buffer.getNumSamples());
}

juce::AudioProcessorEditor* Audio2MidiProcessor::createEditor()
{
    return new Audio2MidiEditor (*this);
}

void Audio2MidiProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void Audio2MidiProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::String Audio2MidiProcessor::getStatus() const
{
    const juce::ScopedLock sl (lock);
    return status;
}

juce::File Audio2MidiProcessor::getMidiFile() const
{
    const juce::ScopedLock sl (lock);
    return midiFile;
}

void Audio2MidiProcessor::setStatus (const juce::String& text, State newState)
{
    {
        const juce::ScopedLock sl (lock);
        status = text;
    }
    state = newState;
}

void Audio2MidiProcessor::startConversion (const juce::File& audioFile)
{
    {
        const juce::ScopedLock sl (lock);
        pendingFile = audioFile;
    }
    cancel = true;   // abort whatever is running; the worker picks up the new file next
    worker->notify();
}

void Audio2MidiProcessor::parameterChanged (const juce::String&, float)
{
    if (hasPosteriors.load())
    {
        redecodeRequested = true;
        worker->notify();
    }
}

bool Audio2MidiProcessor::ensureEngine (juce::String& error)
{
    if (engine != nullptr) return true;
    if (engineFailed) { error = engineError; return false; }

    if (! a2m::initOnnxRuntime (error)) { engineFailed = true; engineError = error; return false; }

    try
    {
        auto e = std::make_unique<bp::Engine>();
        std::string err;
        if (! e->loadModel (BinaryData::nmp_onnx, (size_t) BinaryData::nmp_onnxSize, err))
        {
            error = "Could not load the note model: " + juce::String (err);
            engineFailed = true; engineError = error;
            return false;
        }
        engine = std::move (e);
        return true;
    }
    catch (const std::exception& ex)
    {
        error = "ONNX Runtime failed to start: " + juce::String (ex.what());
        engineFailed = true; engineError = error;
        return false;
    }
}

void Audio2MidiProcessor::workerCycle()
{
    juce::File file;
    {
        const juce::ScopedLock sl (lock);
        file = pendingFile;
        pendingFile = juce::File();
    }

    if (file != juce::File())
    {
        cancel = false;
        redecodeRequested = false;
        convertFile (file);
    }
    else if (redecodeRequested.exchange (false) && hasPosteriors.load())
    {
        cancel = false;
        decodeAndWrite();
    }
}

void Audio2MidiProcessor::convertFile (const juce::File& file)
{
    hasPosteriors = false;
    progress = 0.0f;
    sourceFile = file;

    setStatus ("Loading engine...", State::Working);
    juce::String error;
    if (! ensureEngine (error)) { setStatus (error, State::Failed); return; }

    setStatus ("Reading " + file.getFileName() + " ...", State::Working);
    auto audio = a2m::loadAudioMono22k (file, error, &cancel);
    if (cancel.load()) return;   // superseded by a newer request
    if (audio.empty()) { setStatus (error.isEmpty() ? "Could not read the audio." : error, State::Failed); return; }

    setStatus ("Detecting notes ...", State::Working);
    std::string err;
    const bool ok = engine->runModel (audio, posteriors, err,
                                      [this] (float p) { progress = p; }, &cancel);
    if (cancel.load()) return;
    if (! ok) { setStatus ("Note detection failed: " + juce::String (err), State::Failed); return; }

    hasPosteriors = true;
    progress = 1.0f;
    decodeAndWrite();
}

void Audio2MidiProcessor::decodeAndWrite()
{
    const float sens = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (kSens)->load());
    const double bpm = apvts.getRawParameterValue (kBpm)->load();
    const float minMs = apvts.getRawParameterValue (kMinNote)->load();
    const int minVel = (int) std::lround (apvts.getRawParameterValue (kMinVel)->load());

    bp::DecodeParams dp;
    dp.onsetThreshold = 0.9f - 0.6f * sens;
    dp.frameThreshold = 0.5f - 0.3f * sens;
    dp.minNoteFrames  = (int) std::lround (minMs / 1000.0 * ((double) bp::kSampleRate / bp::kFftHop));

    auto notes = bp::Engine::decode (posteriors, dp, &cancel);
    if (cancel.load()) return;

    int kept = 0;
    for (const auto& n : notes)
        if ((int) std::lround (127.0 * n.amplitude) >= minVel) ++kept;

    juce::File dest = sourceFile.withFileExtension ("mid");
    bool wrote = a2m::writeMidiFile (notes, bpm, minVel, dest);
    if (! wrote)
    {
        dest = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Audio2MIDI").getChildFile (sourceFile.getFileNameWithoutExtension() + ".mid");
        wrote = a2m::writeMidiFile (notes, bpm, minVel, dest);
    }

    if (! wrote) { setStatus ("Could not write the MIDI file.", State::Failed); return; }

    {
        const juce::ScopedLock sl (lock);
        midiFile = dest;
    }
    setStatus (juce::String (kept) + " notes  -  " + dest.getFileName(), State::Ready);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Audio2MidiProcessor();
}
