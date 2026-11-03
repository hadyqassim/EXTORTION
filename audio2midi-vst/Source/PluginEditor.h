#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

class Audio2MidiEditor : public juce::AudioProcessorEditor,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    explicit Audio2MidiEditor (Audio2MidiProcessor&);
    ~Audio2MidiEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void fileDragEnter (const juce::StringArray&, int, int) override;
    void fileDragExit (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    // Grab this and drag it onto the FL Studio Playlist or Piano roll.
    class DragHandle : public juce::Component
    {
    public:
        explicit DragHandle (Audio2MidiProcessor& p) : proc (p) {}
        void paint (juce::Graphics&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void setReady (bool r) { if (ready != r) { ready = r; repaint(); } }
    private:
        Audio2MidiProcessor& proc;
        bool ready = false;
    };

    void timerCallback() override;
    void chooseFile();
    void saveAs();
    void addRow (juce::Label&, juce::Slider&, const juce::String& text, const juce::String& paramId);

    Audio2MidiProcessor& proc;

    juce::Label bpmLabel, sensLabel, minNoteLabel, minVelLabel;
    juce::Slider bpmSlider, sensSlider, minNoteSlider, minVelSlider;
    using Attach = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attach> bpmAttach, sensAttach, minNoteAttach, minVelAttach;

    DragHandle dragHandle;
    juce::TextButton saveButton { "Save as..." };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> dropZone;
    bool dragOver = false;
    juce::String statusText;
    Audio2MidiProcessor::State lastState = Audio2MidiProcessor::State::Idle;
    float progressValue = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Audio2MidiEditor)
};
