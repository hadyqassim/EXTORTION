#include "PluginEditor.h"

namespace
{
const juce::Colour kBg      (0xff111111);
const juce::Colour kPanel   (0xff1b1b1b);
const juce::Colour kText    (0xffcfc8bb);
const juce::Colour kDim     (0xff7d776d);
const juce::Colour kAccent  (0xff9a2f2f);

bool isAudioName (const juce::String& name)
{
    return name.endsWithIgnoreCase (".wav")  || name.endsWithIgnoreCase (".mp3")
        || name.endsWithIgnoreCase (".flac") || name.endsWithIgnoreCase (".ogg")
        || name.endsWithIgnoreCase (".m4a")  || name.endsWithIgnoreCase (".aif")
        || name.endsWithIgnoreCase (".aiff");
}

juce::Font makeFont (float size, bool serif = false)
{
    juce::FontOptions o (size);
    if (serif) o = o.withName ("Georgia");
    return juce::Font (o);
}
} // namespace

void Audio2MidiEditor::DragHandle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (ready ? kAccent : kPanel);
    g.fillRoundedRectangle (r, 6.0f);
    g.setColour (ready ? kText : kDim);
    g.setFont (makeFont (15.0f, true));
    g.drawText (ready ? "Drag MIDI into FL Studio" : "MIDI appears here", getLocalBounds(),
                juce::Justification::centred);
}

void Audio2MidiEditor::DragHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (! ready || e.getDistanceFromDragStart() < 6) return;

    const auto file = proc.getMidiFile();
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

Audio2MidiEditor::Audio2MidiEditor (Audio2MidiProcessor& p)
    : AudioProcessorEditor (&p), proc (p), dragHandle (p)
{
    setSize (480, 400);

    addRow (bpmLabel,     bpmSlider,     "Tempo (BPM)",   "bpm");
    addRow (sensLabel,    sensSlider,    "Sensitivity",   "sensitivity");
    addRow (minNoteLabel, minNoteSlider, "Min note (ms)", "minNoteMs");
    addRow (minVelLabel,  minVelSlider,  "Min velocity",  "minVelocity");

    // BPM is typed rather than dragged: click the number and type your project tempo.
    bpmSlider.setSliderStyle (juce::Slider::IncDecButtons);
    bpmSlider.setIncDecButtonsMode (juce::Slider::incDecButtonsDraggable_Vertical);
    bpmSlider.setTextBoxStyle (juce::Slider::TextBoxLeft, false, 70, 24);

    bpmAttach     = std::make_unique<Attach> (proc.apvts, "bpm", bpmSlider);
    sensAttach    = std::make_unique<Attach> (proc.apvts, "sensitivity", sensSlider);
    minNoteAttach = std::make_unique<Attach> (proc.apvts, "minNoteMs", minNoteSlider);
    minVelAttach  = std::make_unique<Attach> (proc.apvts, "minVelocity", minVelSlider);

    addAndMakeVisible (dragHandle);
    addAndMakeVisible (saveButton);
    saveButton.setEnabled (false);
    saveButton.onClick = [this] { saveAs(); };

    statusText = proc.getStatus();
    startTimerHz (10);
}

Audio2MidiEditor::~Audio2MidiEditor()
{
    stopTimer();
}

void Audio2MidiEditor::addRow (juce::Label& label, juce::Slider& slider, const juce::String& text, const juce::String&)
{
    label.setText (text, juce::dontSendNotification);
    label.setColour (juce::Label::textColourId, kText);
    label.setFont (makeFont (14.0f));
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 24);
    slider.setColour (juce::Slider::trackColourId, kAccent);
    slider.setColour (juce::Slider::thumbColourId, kText);
    slider.setColour (juce::Slider::backgroundColourId, kPanel);
    slider.setColour (juce::Slider::textBoxTextColourId, kText);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);
}

void Audio2MidiEditor::resized()
{
    auto area = getLocalBounds().reduced (20);
    dropZone = area.removeFromTop (120);
    area.removeFromTop (16);

    auto row = [&] (juce::Label& l, juce::Slider& s)
    {
        auto r = area.removeFromTop (30);
        l.setBounds (r.removeFromLeft (110));
        s.setBounds (r);
        area.removeFromTop (6);
    };
    row (bpmLabel, bpmSlider);
    row (sensLabel, sensSlider);
    row (minNoteLabel, minNoteSlider);
    row (minVelLabel, minVelSlider);

    area.removeFromTop (28); // status line is painted here

    auto bottom = area.removeFromTop (44);
    saveButton.setBounds (bottom.removeFromRight (120));
    bottom.removeFromRight (10);
    dragHandle.setBounds (bottom);
}

void Audio2MidiEditor::paint (juce::Graphics& g)
{
    g.fillAll (kBg);

    // Drop zone
    g.setColour (dragOver ? kAccent.withAlpha (0.35f) : kPanel);
    g.fillRoundedRectangle (dropZone.toFloat(), 8.0f);
    g.setColour (dragOver ? kAccent : kDim.withAlpha (0.5f));
    g.drawRoundedRectangle (dropZone.toFloat().reduced (0.5f), 8.0f, 1.0f);

    g.setColour (kText);
    g.setFont (makeFont (18.0f, true));
    const bool working = lastState == Audio2MidiProcessor::State::Working;
    g.drawText (working ? "Converting..." : "Drop a WAV or MP3 here",
                dropZone.withTrimmedBottom (18), juce::Justification::centred);
    if (! working)
    {
        g.setColour (kDim);
        g.setFont (makeFont (12.0f));
        g.drawText ("or click to choose a file", dropZone.withTrimmedTop (70),
                    juce::Justification::centredTop);
    }

    // Status + progress, sits between the sliders and the drag handle
    const int y = minVelSlider.getBottom() + 10;
    const auto statusArea = juce::Rectangle<int> (20, y, getWidth() - 40, 22);
    g.setColour (lastState == Audio2MidiProcessor::State::Failed ? juce::Colour (0xffd16a6a) : kText);
    g.setFont (makeFont (13.0f));
    g.drawFittedText (statusText, statusArea, juce::Justification::centredLeft, 1);

    if (working)
    {
        g.setColour (kPanel);
        g.fillRect (statusArea.getX(), statusArea.getBottom() + 1, statusArea.getWidth(), 3);
        g.setColour (kAccent);
        g.fillRect (statusArea.getX(), statusArea.getBottom() + 1,
                    (int) (statusArea.getWidth() * juce::jlimit (0.0f, 1.0f, progressValue)), 3);
    }
}

void Audio2MidiEditor::timerCallback()
{
    const auto newStatus = proc.getStatus();
    const auto newState = proc.getState();
    const auto newProgress = proc.getProgress();

    const bool changed = newStatus != statusText || newState != lastState
                         || std::abs (newProgress - progressValue) > 0.001f;
    statusText = newStatus;
    lastState = newState;
    progressValue = newProgress;

    const bool haveMidi = proc.getMidiFile().existsAsFile() && newState == Audio2MidiProcessor::State::Ready;
    dragHandle.setReady (haveMidi);
    saveButton.setEnabled (haveMidi);

    if (changed) repaint();
}

void Audio2MidiEditor::mouseUp (const juce::MouseEvent& e)
{
    if (dropZone.contains (e.getPosition()) && ! e.mouseWasDraggedSinceMouseDown())
        chooseFile();
}

bool Audio2MidiEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (auto& f : files) if (isAudioName (f)) return true;
    return false;
}

void Audio2MidiEditor::fileDragEnter (const juce::StringArray&, int, int) { dragOver = true;  repaint(); }
void Audio2MidiEditor::fileDragExit  (const juce::StringArray&)           { dragOver = false; repaint(); }

void Audio2MidiEditor::filesDropped (const juce::StringArray& files, int, int)
{
    dragOver = false;
    for (auto& f : files)
        if (isAudioName (f)) { proc.startConversion (juce::File (f)); break; }
    repaint();
}

void Audio2MidiEditor::chooseFile()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose an audio file", juce::File(),
                                                   "*.wav;*.mp3;*.flac;*.ogg;*.m4a;*.aif;*.aiff");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto f = fc.getResult();
                              if (f.existsAsFile()) proc.startConversion (f);
                          });
}

void Audio2MidiEditor::saveAs()
{
    const auto src = proc.getMidiFile();
    if (! src.existsAsFile()) return;

    chooser = std::make_unique<juce::FileChooser> ("Save MIDI file", src, "*.mid");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::warnAboutOverwriting,
                          [src] (const juce::FileChooser& fc)
                          {
                              auto dest = fc.getResult();
                              if (dest == juce::File()) return;
                              if (! dest.hasFileExtension ("mid")) dest = dest.withFileExtension ("mid");
                              src.copyFileTo (dest);
                          });
}
