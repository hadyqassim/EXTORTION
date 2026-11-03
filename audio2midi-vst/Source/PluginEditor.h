#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

// Dark metal, glowing red. Hard edges everywhere except the knobs and the orb.
class DrillLookAndFeel : public juce::LookAndFeel_V4
{
public:
    DrillLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;
};

class Audio2MidiEditor : public juce::AudioProcessorEditor,
                         public juce::FileDragAndDropTarget,
                         private juce::Timer
{
public:
    explicit Audio2MidiEditor (Audio2MidiProcessor&);
    ~Audio2MidiEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
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

    struct Blob { float x, y, r, phase, speed; };

    void timerCallback() override;
    void chooseFile();
    void saveAs();
    void addRow (juce::Label&, juce::Slider&, const juce::String& text, const juce::String& paramId);
    void paintOrb (juce::Graphics&);
    void buildPanel();

    Audio2MidiProcessor& proc;
    DrillLookAndFeel lnf; // declared before the components so it outlives them

    juce::Label bpmLabel, sensLabel, minNoteLabel, minVelLabel;
    juce::Slider bpmSlider, sensSlider, minNoteSlider, minVelSlider;
    using Attach = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<Attach> bpmAttach, sensAttach, minNoteAttach, minVelAttach;

    DragHandle dragHandle;
    juce::TextButton saveButton { "SAVE AS" };
    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<int> titleArea, orbArea, statusArea;
    juce::Image panel;
    std::vector<Blob> blobs;

    bool dragOver = false;
    juce::String statusText;
    Audio2MidiProcessor::State lastState = Audio2MidiProcessor::State::Idle;
    float progressValue = 0.0f;

    juce::Random rng;
    float flicker = 1.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Audio2MidiEditor)
};
