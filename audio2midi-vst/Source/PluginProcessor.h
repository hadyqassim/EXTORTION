#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <memory>
#include "BasicPitchEngine.h"

class Audio2MidiProcessor : public juce::AudioProcessor,
                            private juce::AudioProcessorValueTreeState::Listener
{
public:
    enum class State { Idle, Working, Ready, Failed };

    Audio2MidiProcessor();
    ~Audio2MidiProcessor() override;

    // Audio passes through untouched; this plugin only converts files you drop on it.
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Audio2MIDI"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // UI-facing API (message thread)
    void startConversion (const juce::File& audioFile);
    State getState() const noexcept { return state.load(); }
    float getProgress() const noexcept { return progress.load(); }
    juce::String getStatus() const;
    juce::File getMidiFile() const;

    juce::AudioProcessorValueTreeState apvts;

private:
    class Worker;
    friend class Worker;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void parameterChanged (const juce::String&, float) override;

    // Worker-thread side
    void workerCycle();
    bool ensureEngine (juce::String& error);
    void convertFile (const juce::File& file);
    void decodeAndWrite();
    void setStatus (const juce::String& text, State newState);

    std::unique_ptr<Worker> worker;
    std::unique_ptr<bp::Engine> engine;
    bool engineFailed = false;
    juce::String engineError;

    bp::Posteriors posteriors;                 // worker thread only
    juce::File sourceFile;                     // worker thread only

    mutable juce::CriticalSection lock;        // guards the fields below
    juce::File pendingFile;
    juce::File midiFile;
    juce::String status { "Drop a WAV or MP3 here" };

    std::atomic<State> state { State::Idle };
    std::atomic<float> progress { 0.0f };
    std::atomic<bool> cancel { false };
    std::atomic<bool> redecodeRequested { false };
    std::atomic<bool> hasPosteriors { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Audio2MidiProcessor)
};
