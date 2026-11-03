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
} // namespace

// ---------------------------------------------------------------- look and feel

DrillLookAndFeel::DrillLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
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

void DrillLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                         float pos, float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f - 4.0f;
    const auto c = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);
    const float arcR = radius - 3.0f;

    // Dead track
    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colour (0xff200509));
    g.strokePath (track, juce::PathStrokeType (5.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));

    // Live arc, three passes for the glow
    juce::Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
    const float widths[] = { 13.0f, 9.0f, 5.0f };
    const float alphas[] = { 0.10f, 0.20f, 1.0f };
    for (int i = 0; i < 3; ++i)
    {
        g.setColour (kHot.withAlpha (alphas[i]));
        g.strokePath (value, juce::PathStrokeType (widths[i], juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    }

    // Knob body
    const float bodyR = arcR - 10.0f;
    juce::ColourGradient body (juce::Colour (0xff3b3b3b), c.x - bodyR * 0.4f, c.y - bodyR * 0.5f,
                               juce::Colour (0xff090909), c.x - bodyR * 0.4f + bodyR * 1.5f, c.y - bodyR * 0.5f, true);
    g.setGradientFill (body);
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 2.0f);

    // Knurled edge
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    for (int i = 0; i < 28; ++i)
    {
        const float a = (float) i * juce::MathConstants<float>::twoPi / 28.0f;
        const float sa = std::sin (a), ca = std::cos (a);
        g.drawLine (c.x + sa * bodyR * 0.86f, c.y - ca * bodyR * 0.86f,
                    c.x + sa * bodyR * 0.97f, c.y - ca * bodyR * 0.97f, 1.0f);
    }

    // Pointer
    const float s = std::sin (angle), co = std::cos (angle);
    const juce::Point<float> p1 (c.x + s * bodyR * 0.30f, c.y - co * bodyR * 0.30f);
    const juce::Point<float> p2 (c.x + s * bodyR * 0.88f, c.y - co * bodyR * 0.88f);
    g.setColour (kHot.withAlpha (0.25f));
    g.drawLine (juce::Line<float> (p1, p2), 8.0f);
    g.setColour (kHot);
    g.drawLine (juce::Line<float> (p1, p2), 3.0f);
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

    // Fixed pattern so the orb always has the same molten texture.
    juce::Random br (7);
    while (blobs.size() < 36)
    {
        const float bx = br.nextFloat() * 2.0f - 1.0f;
        const float by = br.nextFloat() * 2.0f - 1.0f;
        if (bx * bx + by * by > 0.8f) continue;
        blobs.push_back ({ bx, by, 0.18f + br.nextFloat() * 0.28f,
                           br.nextFloat() * juce::MathConstants<float>::twoPi,
                           0.6f + br.nextFloat() * 1.4f });
    }

    setSize (720, 480);

    addRow (bpmLabel,     bpmSlider,     "Tempo (BPM)",   "bpm");
    addRow (sensLabel,    sensSlider,    "Sensitivity",   "sensitivity");
    addRow (minNoteLabel, minNoteSlider, "Min note (ms)", "minNoteMs");
    addRow (minVelLabel,  minVelSlider,  "Min velocity",  "minVelocity");

    // BPM is typed rather than turned: click the number and type your project tempo.
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
    startTimerHz (24);
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
    label.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (label);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    slider.setColour (juce::Slider::textBoxTextColourId, kText);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    addAndMakeVisible (slider);
}

void Audio2MidiEditor::resized()
{
    auto area = getLocalBounds().reduced (24);
    titleArea = area.removeFromTop (48);
    area.removeFromTop (12);

    auto body = area.removeFromTop (280);
    auto leftCol  = body.removeFromLeft (196);
    auto rightCol = body.removeFromRight (196);
    orbArea = body.withSizeKeepingCentre (260, 260);

    auto placeKnob = [] (juce::Label& l, juce::Slider& s, juce::Rectangle<int> cell)
    {
        l.setBounds (cell.removeFromTop (18));
        s.setBounds (cell.withSizeKeepingCentre (110, cell.getHeight()));
    };

    placeKnob (sensLabel,    sensSlider,    leftCol.removeFromTop (140));
    placeKnob (minNoteLabel, minNoteSlider, leftCol);
    placeKnob (minVelLabel,  minVelSlider,  rightCol.removeFromTop (140));

    auto bpmCell = rightCol.withSizeKeepingCentre (180, 60);
    bpmLabel.setBounds (bpmCell.removeFromTop (22));
    bpmSlider.setBounds (bpmCell.removeFromTop (34));

    area.removeFromTop (8);
    statusArea = area.removeFromTop (22);

    auto bottom = area.removeFromBottom (44);
    saveButton.setBounds (bottom.removeFromRight (120));
    bottom.removeFromRight (10);
    dragHandle.setBounds (bottom);

    buildPanel();
}

// Static faceplate, painted once into an image so the animation stays cheap.
void Audio2MidiEditor::buildPanel()
{
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;

    panel = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (panel);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff1b1b1b), 0.0f, 0.0f,
                                             juce::Colour (0xff080808), 0.0f, (float) h, false));
    g.fillAll();

    // Brushed grain
    juce::Random r (1337);
    for (int y = 0; y < h; ++y)
    {
        g.setColour ((r.nextBool() ? juce::Colours::white : juce::Colours::black).withAlpha (r.nextFloat() * 0.03f));
        g.fillRect (0, y, w, 1);
    }
    for (int i = 0; i < 1200; ++i)
    {
        g.setColour (juce::Colours::white.withAlpha (r.nextFloat() * 0.04f));
        g.fillRect (r.nextInt (w), r.nextInt (h), 8 + r.nextInt (60), 1);
    }

    // Recessed wells behind the knobs
    auto well = [&] (const juce::Slider& s)
    {
        const auto b = s.getBounds().toFloat();
        const float cx = b.getCentreX();
        const float cy = b.getY() + (b.getHeight() - 18.0f) * 0.5f;
        const float rr = 58.0f;
        g.setColour (juce::Colour (0xff050505));
        g.fillEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f);
        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawEllipse (cx - rr, cy - rr, rr * 2.0f, rr * 2.0f, 1.5f);
    };
    well (sensSlider);
    well (minNoteSlider);
    well (minVelSlider);

    // Recessed plate behind the BPM readout
    const auto plate = bpmSlider.getBounds().expanded (10, 6).toFloat();
    g.setColour (juce::Colour (0xff050505));
    g.fillRect (plate);
    g.setColour (kAccent.withAlpha (0.6f));
    g.drawRect (plate, 1.0f);

    // Title rule and tag
    g.setColour (kAccent);
    g.fillRect (titleArea.getX(), titleArea.getBottom() + 2, titleArea.getWidth(), 2);
    g.setColour (kDim);
    g.setFont (bodyFont (11.0f));
    g.drawText ("AUDIO > MIDI", titleArea, juce::Justification::bottomRight);

    // Edge bevel and screws
    g.setColour (juce::Colours::black);
    g.drawRect (0, 0, w, h, 2);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRect (2, 2, w - 4, h - 4, 1);

    const juce::Point<float> screws[] = { { 12.0f, 12.0f }, { (float) w - 12.0f, 12.0f },
                                          { 12.0f, (float) h - 12.0f }, { (float) w - 12.0f, (float) h - 12.0f } };
    for (auto p : screws)
    {
        g.setColour (juce::Colour (0xff2a2a2a));
        g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colours::black);
        g.drawEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f, 1.0f);
        g.drawLine (p.x - 3.0f, p.y + 2.0f, p.x + 3.0f, p.y - 2.0f, 1.5f);
    }
}

void Audio2MidiEditor::paint (juce::Graphics& g)
{
    g.drawImageAt (panel, 0, 0);

    // Title with a hard glow
    g.setFont (headFont (40.0f));
    g.setColour (kHot.withAlpha (0.06f * flicker));
    for (int dx = -2; dx <= 2; dx += 2)
        for (int dy = -2; dy <= 2; dy += 2)
            if (dx != 0 || dy != 0)
                g.drawText ("EXTORTION", titleArea.translated (dx, dy), juce::Justification::centredLeft);
    g.setColour (kHot.withAlpha (flicker));
    g.drawText ("EXTORTION", titleArea, juce::Justification::centredLeft);

    paintOrb (g);

    // Status + progress
    const bool working = lastState == Audio2MidiProcessor::State::Working;
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

void Audio2MidiEditor::paintOrb (juce::Graphics& g)
{
    const double t = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const bool working = lastState == Audio2MidiProcessor::State::Working;
    const float energy = dragOver ? 1.0f : (working ? 0.9f : 0.55f + 0.1f * (float) std::sin (t * 1.3));
    const float speed  = working ? 3.0f : 0.8f;

    const auto c = orbArea.toFloat().getCentre();
    const float R = (float) orbArea.getWidth() * 0.5f - 14.0f;

    // Outer glow
    for (int i = 0; i < 8; ++i)
    {
        const float rr = R + 6.0f + (float) i * 4.0f;
        g.setColour (kHot.withAlpha (energy * 0.09f * (1.0f - (float) i / 8.0f)));
        g.drawEllipse (c.x - rr, c.y - rr, rr * 2.0f, rr * 2.0f, 4.0f);
    }

    // Metal bezel
    const float bz = R + 8.0f;
    g.setColour (juce::Colour (0xff1e1e1e));
    g.fillEllipse (c.x - bz, c.y - bz, bz * 2.0f, bz * 2.0f);
    g.setColour (juce::Colours::black);
    g.drawEllipse (c.x - bz, c.y - bz, bz * 2.0f, bz * 2.0f, 2.0f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.drawEllipse (c.x - bz + 2.0f, c.y - bz + 2.0f, bz * 2.0f - 4.0f, bz * 2.0f - 4.0f, 1.0f);

    {
        juce::Path disc;
        disc.addEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (disc);

        // Dark core
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xff2a0006), c.x, c.y,
                                                 juce::Colour (0xff050202), c.x + R, c.y, true));
        g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);

        // Molten blobs drifting and pulsing
        for (const auto& b : blobs)
        {
            const float px = c.x + R * (b.x + 0.06f * (float) std::sin (t * 0.4 * b.speed + b.phase));
            const float py = c.y + R * (b.y + 0.06f * (float) std::cos (t * 0.5 * b.speed + b.phase));
            const float rr = R * b.r;
            const float bright = 0.5f + 0.5f * (float) std::sin (t * speed * b.speed + b.phase);
            const float a = energy * (0.15f + 0.5f * bright);
            const auto col = kAccent.interpolatedWith (juce::Colour (0xffff5a1f), bright);

            g.setGradientFill (juce::ColourGradient (col.withAlpha (a), px, py,
                                                     col.withAlpha (0.0f), px + rr, py, true));
            g.fillEllipse (px - rr, py - rr, rr * 2.0f, rr * 2.0f);
        }

        // Spherical shading
        juce::ColourGradient vign (juce::Colours::black.withAlpha (0.0f), c.x, c.y,
                                   juce::Colours::black.withAlpha (0.85f), c.x + R, c.y, true);
        vign.addColour (0.65, juce::Colours::black.withAlpha (0.0f));
        g.setGradientFill (vign);
        g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);

        // Glint
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f),
                                                 c.x - R * 0.35f, c.y - R * 0.4f,
                                                 juce::Colours::white.withAlpha (0.0f),
                                                 c.x - R * 0.35f + R * 0.5f, c.y - R * 0.4f, true));
        g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);
    }

    // Progress ring while converting
    if (working)
    {
        const float pr = R + 14.0f;
        juce::Path ring;
        ring.addCentredArc (c.x, c.y, pr, pr, 0.0f, 0.0f,
                            juce::MathConstants<float>::twoPi * juce::jlimit (0.001f, 1.0f, progressValue), true);
        g.setColour (kHot);
        g.strokePath (ring, juce::PathStrokeType (4.0f, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    }

    // Text on the orb
    const juce::Rectangle<int> big ((int) orbArea.getX(), (int) c.y - 34, orbArea.getWidth(), 40);
    const juce::Rectangle<int> small ((int) orbArea.getX(), (int) c.y + 8, orbArea.getWidth(), 18);
    const juce::String line1 = working ? "EXTRACTING" : "DROP AUDIO";
    const juce::String line2 = working ? "NOTES" : "WAV / MP3 / CLICK TO BROWSE";

    g.setFont (headFont (32.0f));
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawText (line1, big.translated (2, 2), juce::Justification::centred);
    g.setColour (kText);
    g.drawText (line1, big, juce::Justification::centred);

    g.setFont (bodyFont (11.0f, true));
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawText (line2, small.translated (1, 1), juce::Justification::centred);
    g.setColour (kDim.brighter (0.4f));
    g.drawText (line2, small, juce::Justification::centred);
}

void Audio2MidiEditor::paintOverChildren (juce::Graphics& g)
{
    // Faint scanlines, only across the area being repainted
    g.setColour (juce::Colours::black.withAlpha (0.12f));
    const auto clip = g.getClipBounds();
    for (int y = clip.getY() - (clip.getY() % 3); y < clip.getBottom(); y += 3)
        g.fillRect (clip.getX(), y, clip.getWidth(), 1);
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

    // Occasional dip in the logo brightness
    const float prevFlicker = flicker;
    flicker = rng.nextFloat() < 0.04f ? 0.45f + 0.3f * rng.nextFloat() : 1.0f;
    if (flicker != prevFlicker) repaint (titleArea);

    // Keep the orb moving
    repaint (orbArea.expanded (40));

    if (changed) repaint();
}

void Audio2MidiEditor::mouseUp (const juce::MouseEvent& e)
{
    const auto c = orbArea.toFloat().getCentre();
    if (e.position.getDistanceFrom (c) <= (float) orbArea.getWidth() * 0.5f
        && ! e.mouseWasDraggedSinceMouseDown())
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
