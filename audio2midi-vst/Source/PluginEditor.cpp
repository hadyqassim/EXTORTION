#include "PluginEditor.h"

namespace
{
const juce::Colour kBg      (0xff0a0a0a);
const juce::Colour kPanel   (0xff151515);
const juce::Colour kText    (0xffe8e4dc);
const juce::Colour kDim     (0xff6b6560);
const juce::Colour kAccent  (0xffb00020);
const juce::Colour kHot     (0xffff1a3c);

bool isAudioName (const juce::String& name)
{
    return name.endsWithIgnoreCase (".wav")  || name.endsWithIgnoreCase (".mp3")
        || name.endsWithIgnoreCase (".flac") || name.endsWithIgnoreCase (".ogg")
        || name.endsWithIgnoreCase (".m4a")  || name.endsWithIgnoreCase (".aif")
        || name.endsWithIgnoreCase (".aiff");
}

// Windows system fonts: heavy condensed for headlines, monospace for readouts.
juce::Font headFont (float size) { return juce::Font (juce::FontOptions ("Impact", size, juce::Font::plain)); }
juce::Font bodyFont (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions ("Consolas", size, bold ? juce::Font::bold : juce::Font::plain));
}

void drawCorners (juce::Graphics& g, juce::Rectangle<float> r, float len, float thick)
{
    g.fillRect (r.getX(), r.getY(), len, thick);
    g.fillRect (r.getX(), r.getY(), thick, len);
    g.fillRect (r.getRight() - len, r.getY(), len, thick);
    g.fillRect (r.getRight() - thick, r.getY(), thick, len);
    g.fillRect (r.getX(), r.getBottom() - thick, len, thick);
    g.fillRect (r.getX(), r.getBottom() - len, thick, len);
    g.fillRect (r.getRight() - len, r.getBottom() - thick, len, thick);
    g.fillRect (r.getRight() - thick, r.getBottom() - len, thick, len);
}
} // namespace

// ---------------------------------------------------------------- look and feel

DrillLookAndFeel::DrillLookAndFeel()
{
    setColour (juce::Slider::backgroundColourId, kPanel);
    setColour (juce::Slider::trackColourId, kAccent);
    setColour (juce::Slider::thumbColourId, kText);
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxBackgroundColourId, kPanel);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, kAccent);
    setColour (juce::Label::textColourId, kText);
    setColour (juce::TextButton::buttonColourId, kPanel);
    setColour (juce::TextButton::textColourOffId, kText);
    setColour (juce::TextButton::textColourOnId, kText);
    setColour (juce::TextEditor::backgroundColourId, kPanel);
    setColour (juce::TextEditor::textColourId, kText);
    setColour (juce::TextEditor::highlightColourId, kAccent);
    setColour (juce::CaretComponent::caretColourId, kHot);
}

void DrillLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float, float, juce::Slider::SliderStyle,
                                         juce::Slider& s)
{
    const float midY = (float) y + (float) height * 0.5f;
    const juce::Rectangle<float> track ((float) x, midY - 3.0f, (float) width, 6.0f);

    g.setColour (s.findColour (juce::Slider::backgroundColourId));
    g.fillRect (track);

    g.setColour (s.findColour (juce::Slider::trackColourId));
    g.fillRect (track.withRight (sliderPos));

    g.setColour (s.findColour (juce::Slider::thumbColourId));
    g.fillRect (juce::Rectangle<float> (sliderPos - 3.0f, (float) y + 3.0f, 6.0f, (float) height - 6.0f));
}

void DrillLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                             bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat();
    g.setColour (down ? kAccent : (highlighted ? kAccent.withAlpha (0.35f) : kPanel));
    g.fillRect (r);
    g.setColour (b.isEnabled() ? kAccent : kDim.withAlpha (0.4f));
    g.drawRect (r, 2.0f);
}

juce::Font DrillLookAndFeel::getTextButtonFont (juce::TextButton&, int)
{
    return bodyFont (14.0f, true);
}

// ---------------------------------------------------------------- drag handle

void Audio2MidiEditor::DragHandle::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (ready ? kAccent : kPanel);
    g.fillRect (r);
    g.setColour (ready ? kHot : kDim.withAlpha (0.5f));
    g.drawRect (r, 2.0f);
    g.setColour (ready ? kText : kDim);
    g.setFont (headFont (17.0f));
    g.drawText (ready ? "DRAG MIDI INTO FL STUDIO" : "MIDI APPEARS HERE", getLocalBounds(),
                juce::Justification::centred);
}

void Audio2MidiEditor::DragHandle::mouseDrag (const juce::MouseEvent& e)
{
    if (! ready || e.getDistanceFromDragStart() < 6) return;

    const auto file = proc.getMidiFile();
    if (file.existsAsFile())
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this);
}

// ---------------------------------------------------------------- editor

Audio2MidiEditor::Audio2MidiEditor (Audio2MidiProcessor& p)
    : AudioProcessorEditor (&p), proc (p), dragHandle (p)
{
    setLookAndFeel (&lnf);
    setSize (480, 460);

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
    setLookAndFeel (nullptr);
}

void Audio2MidiEditor::addRow (juce::Label& label, juce::Slider& slider, const juce::String& text, const juce::String&)
{
    label.setText (text.toUpperCase(), juce::dontSendNotification);
    label.setColour (juce::Label::textColourId, kText);
    label.setFont (bodyFont (13.0f, true));
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
    titleArea = area.removeFromTop (48);
    area.removeFromTop (8);
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

    // Title
    g.setColour (kAccent.withAlpha (flicker));
    g.setFont (headFont (38.0f));
    g.drawText ("EXTORTION", titleArea, juce::Justification::centredLeft);
    g.setColour (kDim);
    g.setFont (bodyFont (11.0f));
    g.drawText ("AUDIO > MIDI", titleArea, juce::Justification::bottomRight);
    g.setColour (kAccent);
    g.fillRect (titleArea.getX(), titleArea.getBottom() + 2, titleArea.getWidth(), 2);

    // Drop zone: square box, heavy corner brackets
    const auto dz = dropZone.toFloat();
    g.setColour (dragOver ? kAccent.withAlpha (0.45f) : kPanel);
    g.fillRect (dz);
    g.setColour (dragOver ? kHot : kDim.withAlpha (0.5f));
    g.drawRect (dz, 1.0f);
    g.setColour (dragOver ? kHot : kAccent);
    drawCorners (g, dz, 22.0f, 3.0f);

    const bool working = lastState == Audio2MidiProcessor::State::Working;
    g.setColour (kText);
    g.setFont (headFont (34.0f));
    g.drawText (working ? "EXTRACTING NOTES" : "DROP AUDIO",
                dropZone.withTrimmedBottom (18), juce::Justification::centred);
    if (! working)
    {
        g.setColour (kDim);
        g.setFont (bodyFont (11.0f));
        g.drawText ("WAV / MP3 / FLAC   OR CLICK TO BROWSE", dropZone.withTrimmedTop (82),
                    juce::Justification::centredTop);
    }

    // Status + progress, sits between the sliders and the drag handle
    const int y = minVelSlider.getBottom() + 10;
    const auto statusArea = juce::Rectangle<int> (20, y, getWidth() - 40, 22);
    g.setColour (lastState == Audio2MidiProcessor::State::Failed ? kHot : kText);
    g.setFont (bodyFont (12.0f, true));
    g.drawFittedText ("> " + statusText.toUpperCase(), statusArea, juce::Justification::centredLeft, 1);

    if (working)
    {
        g.setColour (kPanel);
        g.fillRect (statusArea.getX(), statusArea.getBottom() + 1, statusArea.getWidth(), 4);
        g.setColour (kHot);
        g.fillRect (statusArea.getX(), statusArea.getBottom() + 1,
                    (int) (statusArea.getWidth() * juce::jlimit (0.0f, 1.0f, progressValue)), 4);
    }
}

void Audio2MidiEditor::paintOverChildren (juce::Graphics& g)
{
    // Faint scanlines over everything
    g.setColour (juce::Colours::black.withAlpha (0.16f));
    for (int y = 0; y < getHeight(); y += 3)
        g.fillRect (0, y, getWidth(), 1);
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

    // Occasional dip in the logo brightness. Only the title area repaints.
    const float prevFlicker = flicker;
    flicker = rng.nextFloat() < 0.06f ? 0.45f + 0.3f * rng.nextFloat() : 1.0f;
    if (flicker != prevFlicker) repaint (titleArea);

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
