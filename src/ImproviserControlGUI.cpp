#include "ImproviserControlGUI.h"

// ===============================================================
// ctor / dtor
// ===============================================================

ImproviserControlGUI::ImproviserControlGUI()
{
    // Chunky toggles
    playingToggle.setClickingTogglesState(true);
    learningToggle.setClickingTogglesState(true);

    // Buttons
    loadModelButton.addListener(this);
    saveModelButton.addListener(this);

    // BPM slider (chunky rotary)
    bpmSlider.setRange(60.0, 240.0, 1.0);
    bpmSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    bpmSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 24);
    bpmSlider.addListener(this);
    bpmSlider.setValue(120.0);

    bpmLabel.attachToComponent(&bpmSlider, false);
    bpmLabel.setJustificationType(juce::Justification::centred);

    // Division combo
    divisionCombo.addListener(this);
    divisionCombo.addItem("beat",     1);  // 1.0
    divisionCombo.addItem("1/3",      2);  // ~0.3333
    divisionCombo.addItem("quarter",  3);  // 0.25
    divisionCombo.addItem("1/8",      4);  // 0.125
    divisionCombo.addItem("1/12",     5);  // ~0.08333
    divisionCombo.addItem("1/16",     6);  // 0.0625
    divisionCombo.setSelectedId(1, juce::dontSendNotification);

    divisionLabel.attachToComponent(&divisionCombo, true);
    divisionLabel.setJustificationType(juce::Justification::centredRight);

    // Probability slider (0..1, chunky linear bar)
    probabilitySlider.setRange(0.0, 1.0, 0.01);
    probabilitySlider.setSliderStyle(juce::Slider::LinearBar);
    probabilitySlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 24);
    probabilitySlider.addListener(this);
    probabilitySlider.setValue(0.5);

    // MIDI combos
    midiInCombo.addListener(this);
    midiInCombo.addItem("All", 100);         // -> channel 0
    for (int ch = 1; ch <= 16; ++ch)
        midiInCombo.addItem("Ch " + juce::String(ch), 100 + ch);
    midiInCombo.setSelectedId(100, juce::dontSendNotification);

    midiInLabel.attachToComponent(&midiInCombo, true);

    midiOutCombo.addListener(this);
    for (int ch = 1; ch <= 16; ++ch)
        midiOutCombo.addItem("Ch " + juce::String(ch), 200 + ch);
    midiOutCombo.setSelectedId(201, juce::dontSendNotification); // default Ch 1
    midiOutLabel.attachToComponent(&midiOutCombo, true);

    // Group titles (just visual separators)
    quantGroup.setTextLabelPosition(juce::Justification::centredLeft);
    probGroup.setTextLabelPosition(juce::Justification::centredLeft);
    midiGroup.setTextLabelPosition(juce::Justification::centredLeft);

    // Add & make visible
    addAndMakeVisible(playingToggle);
    addAndMakeVisible(learningToggle);
    addAndMakeVisible(loadModelButton);
    addAndMakeVisible(saveModelButton);

    addAndMakeVisible(quantGroup);
    addAndMakeVisible(bpmSlider);
    addAndMakeVisible(bpmLabel);
    addAndMakeVisible(divisionCombo);
    addAndMakeVisible(divisionLabel);

    addAndMakeVisible(probGroup);
    addAndMakeVisible(probabilitySlider);

    addAndMakeVisible(midiGroup);
    addAndMakeVisible(midiInCombo);
    addAndMakeVisible(midiInLabel);
    addAndMakeVisible(midiOutCombo);
    addAndMakeVisible(midiOutLabel);

    // Toggle listeners last (so initial setValue/setSelectedId don't fire)
    playingToggle.addListener(this);
    learningToggle.addListener(this);

    configureChunkyControls();

    setDecaySeconds(3.0f);

    // Start animation timer
    updateFrameTimer();
}

ImproviserControlGUI::~ImproviserControlGUI()
{
    playingToggle.removeListener(this);
    learningToggle.removeListener(this);
    loadModelButton.removeListener(this);
    saveModelButton.removeListener(this);
    bpmSlider.removeListener(this);
    probabilitySlider.removeListener(this);
    divisionCombo.removeListener(this);
    midiInCombo.removeListener(this);
    midiOutCombo.removeListener(this);
}

// ===============================================================
// Public API
// ===============================================================

void ImproviserControlGUI::addImproviserControlListener(ImproviserControlListener* l)
{
    listener = l;
}

void ImproviserControlGUI::setGridDimensions(int columns, int rows)
{
    gridColumns = juce::jmax(1, columns);
    gridRows    = juce::jmax(1, rows);
    resized();
}

void ImproviserControlGUI::setFrameRateHz(int hz)
{
    frameRateHz = juce::jlimit(1, 240, hz);
    updateFrameTimer();
}

void ImproviserControlGUI::setDecaySeconds(float seconds)
{
    decaySeconds = juce::jlimit(0.05f, 5.0f, seconds);
}



void ImproviserControlGUI::midiReceived(const juce::MidiMessage& msg)
{
    // if (!msg.isNoteOnOrOff())
    //     return;
    if (!msg.isNoteOn()) return; 
    
    const int note = msg.getNoteNumber();
    
    const float brightness = msg.isNoteOn() ? static_cast<float>(msg.getVelocity()) / 127.0f : 0.0f;

    // Update UI state on the message thread.
    juce::MessageManager::callAsync([this, note, brightness]
    {
        lastNoteNumber.store(note, std::memory_order_relaxed);
        noteBrightness.store(brightness, std::memory_order_relaxed);

        if (brightness > brightnessRedrawThreshold)
            repaint(midiLightBounds);
    });
}

void ImproviserControlGUI::midiSent(const juce::MidiMessage& msg)
{
    // if (!msg.isNoteOnOrOff())
    //     return;
    if (!msg.isNoteOn()) return; 
    
    const int note = msg.getNoteNumber();
    
    const float brightness = msg.isNoteOn() ? static_cast<float>(msg.getVelocity()) / 127.0f : 0.0f;

    // Update UI state on the message thread.
    juce::MessageManager::callAsync([this, note, brightness]
    {
        lastNoteNumber.store(note, std::memory_order_relaxed);
        noteBrightness.store(brightness, std::memory_order_relaxed);

        if (brightness > brightnessRedrawThreshold)
            repaint(midiLightBounds);
    });
}

// ===============================================================
// Painting / layout
// ===============================================================

void ImproviserControlGUI::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));

    // --- MIDI Light rendering (inside probGroup, below slider) ---
    if (! midiLightBounds.isEmpty())
    {
        const float b = noteBrightness.load(std::memory_order_relaxed);

        // Backplate
        auto outlineColour = findColour(juce::GroupComponent::outlineColourId);
        auto fillColour    = findColour(juce::TextButton::buttonColourId).withMultipliedAlpha(0.12f);

        g.setColour(fillColour);
        g.fillRoundedRectangle(midiLightBounds.toFloat(), 6.0f);

        g.setColour(outlineColour);
        g.drawRoundedRectangle(midiLightBounds.toFloat(), 6.0f, 1.0f);

        // // Bright overlay proportional to velocity/brightness
        // if (b > 0.0f)
        // {
        //     auto fon
        //     auto activeColour = findColour(juce::TextButton::buttonOnColourId).withAlpha(juce::jlimit(0.0f, 1.0f, b));
        //     g.setColour(activeColour);
        //     g.fillRoundedRectangle(midiLightBounds.toFloat(), 6.0f);
        // }

        // Note number text
        juce::String labelText = "-";
        int note = lastNoteNumber.load(std::memory_order_relaxed);
        if (note >= 0 && note <= 127)
            labelText = juce::String(note);

        // Choose a clear, large font size that fits
        const float h = (float) midiLightBounds.getHeight();
        const float w = (float) midiLightBounds.getWidth();
        float fontSize = juce::jmin(h * 0.70f, w * 0.45f);
        g.setFont(juce::Font(fontSize, juce::Font::bold));


    // Use brightness to interpolate between base text color and white
        juce::Colour baseTextColour = findColour(juce::Label::textColourId);
        // juce::Colour textColour = baseTextColour.interpolatedWith(juce::Colours::white, b);
        const uint8 shade = static_cast<uint8>(b * 255.0f);
        juce::Colour textColour = juce::Colour::fromRGB(shade, shade, shade);
        
        g.setColour(textColour);
        g.drawFittedText(labelText, midiLightBounds, juce::Justification::centred, 1);

        // // Contrast-aware text color
        // juce::Colour textColour = (b > 0.4f) ? juce::Colours::black : findColour(juce::Label::textColourId);
        // g.setColour(textColour);

        g.drawFittedText(labelText, midiLightBounds, juce::Justification::centred, 1);
    }
}

juce::Rectangle<int> ImproviserControlGUI::cellBounds(int cx, int cy, int wCells, int hCells) const
{
    auto r = getLocalBounds().reduced(gridGapPx);
    const int cellW = juce::jmax(1, (r.getWidth()  - gridGapPx * (gridColumns - 1)) / gridColumns);
    const int cellH = juce::jmax(1, (r.getHeight() - gridGapPx * (gridRows    - 1)) / gridRows);

    const int x = r.getX() + cx * (cellW + gridGapPx);
    const int y = r.getY() + cy * (cellH + gridGapPx);
    const int w = cellW * wCells + gridGapPx * (wCells - 1);
    const int h = cellH * hCells + gridGapPx * (hCells - 1);
    return { x, y, w, h };
}

void ImproviserControlGUI::resized()
{
    // Row 0: [AI playing] [AI learning] [load model] [save model]
    playingToggle.setBounds(cellBounds(0, 0));
    learningToggle.setBounds(cellBounds(1, 0));
    loadModelButton.setBounds(cellBounds(2, 0));
    saveModelButton.setBounds(cellBounds(3, 0));

    // Row 1-2: Quantisation group spanning 2x2, with BPM (rotary) and Division (combo)
    quantGroup.setBounds(cellBounds(0, 1, 2, 2).reduced(4));
    auto quantArea = quantGroup.getBounds().reduced(10);
    auto leftHalf  = quantArea.removeFromLeft(quantArea.getWidth() / 2).reduced(6);
    auto rightHalf = quantArea.reduced(6);

    bpmSlider.setBounds(leftHalf.withSizeKeepingCentre(leftHalf.getWidth(), leftHalf.getHeight()));
    // Label is attached above/below by TextBox setting

    // Division
    const int comboH = 28;
    divisionCombo.setBounds(rightHalf.removeFromTop(comboH));
    // divisionLabel is attached to the left of combo by attachToComponent()

    // Row 1-2: Probability group spanning 2x2 on the right
    probGroup.setBounds(cellBounds(2, 1, 2, 2).reduced(4));
    auto probArea = probGroup.getBounds().reduced(10);

    probabilitySlider.setBounds(probArea.removeFromTop(32));
    // MIDI Light goes into the remaining area inside probGroup
    midiLightBounds = probArea.reduced(6); // nice padding around the light

    // Row 3: MIDI group spanning all columns
    midiGroup.setBounds(cellBounds(0, 3, gridColumns, 1).reduced(4));
    auto midiArea = midiGroup.getBounds().reduced(10);

    auto midiLeft  = midiArea.removeFromLeft(midiArea.getWidth() / 2).reduced(6);
    auto midiRight = midiArea.reduced(6);

    midiInCombo.setBounds(midiLeft.removeFromTop(comboH));
    midiOutCombo.setBounds(midiRight.removeFromTop(comboH));
}

// ===============================================================
// Helpers
// ===============================================================

void ImproviserControlGUI::configureChunkyControls()
{
    // Make toggles visually chunky
    playingToggle.setSize(140, 40);
    learningToggle.setSize(140, 40);
    playingToggle.setToggleState(false, juce::dontSendNotification);
    learningToggle.setToggleState(false, juce::dontSendNotification);

    auto makeBold = [](juce::Component& c)
    {
        if (auto* lb = dynamic_cast<juce::Label*>(&c))
            lb->setFont(lb->getFont().withHeight(14.0f).boldened());
    };

    makeBold(bpmLabel);
    makeBold(divisionLabel);
    makeBold(midiInLabel);
    makeBold(midiOutLabel);

    // Tooltips for clarity
    playingToggle.setTooltip("Toggle AI playback on/off");
    learningToggle.setTooltip("Toggle AI learning on/off");
    loadModelButton.setTooltip("Load a trained model from disk");
    saveModelButton.setTooltip("Save the current model to disk");
    bpmSlider.setTooltip("Beats per minute (60-240)");
    divisionCombo.setTooltip("Quantisation division (fraction of a beat)");
    probabilitySlider.setTooltip("Probability of AI playing (0.0-1.0)");
    midiInCombo.setTooltip("Select MIDI Input channel (All or 1-16)");
    midiOutCombo.setTooltip("Select MIDI Output channel (1-16)");
}

float ImproviserControlGUI::divisionIdToValue(int itemId)
{
    switch (itemId)
    {
        case 1: return 1.0f;                 // beat
        case 2: return 1.0f / 3.0f;          // 1/3
        case 3: return 0.25f;                // quarter
        case 4: return 0.125f;               // 1/8
        case 5: return 1.0f / 12.0f;         // 1/12
        case 6: return 1.0f / 16.0f;         // 1/16
        default: return 1.0f;
    }
}

int ImproviserControlGUI::midiInIdToChannel(int itemId)
{
    if (itemId == 100) return 0; // All
    if (itemId >= 101 && itemId <= 116) return itemId - 100; // 1..16
    return 0;
}

int ImproviserControlGUI::midiOutIdToChannel(int itemId)
{
    if (itemId >= 201 && itemId <= 216) return itemId - 200; // 1..16
    return 1; // default
}

// ===============================================================
// Listeners (UI -> external listener)
// ===============================================================

void ImproviserControlGUI::buttonClicked(juce::Button* button)
{
    if (button == &loadModelButton)
    {
        if (listener) listener->loadModelDialogue();
        return;
    }
    if (button == &saveModelButton)
    {
        if (listener) listener->saveModelDialogue();
        return;
    }
    if (button == &playingToggle)
    {
        if (!listener) return;
        if (playingToggle.getToggleState()) listener->playingOn();
        else                                 listener->playingOff();
        return;
    }
    if (button == &learningToggle)
    {
        if (!listener) return;
        if (learningToggle.getToggleState()) listener->learningOn();
        else                                  listener->learningOff();
        return;
    }
}

void ImproviserControlGUI::sliderValueChanged(juce::Slider* slider)
{
    if (!listener) return;

    if (slider == &bpmSlider)
    {
        listener->setQuantBPM((float) bpmSlider.getValue());
        return;
    }
    if (slider == &probabilitySlider)
    {
        listener->setPlayProbability((float) probabilitySlider.getValue());
        return;
    }
}

void ImproviserControlGUI::comboBoxChanged(juce::ComboBox* combo)
{
    if (!listener) return;

    if (combo == &divisionCombo)
    {
        const float div = divisionIdToValue(divisionCombo.getSelectedId());
        listener->setQuantDivision(div);
        return;
    }
    if (combo == &midiInCombo)
    {
        const int ch = midiInIdToChannel(midiInCombo.getSelectedId());
        listener->setMIDIInChannel(ch);
        return;
    }
    if (combo == &midiOutCombo)
    {
        const int ch = midiOutIdToChannel(midiOutCombo.getSelectedId());
        listener->setMIDIOutChannel(ch);
        return;
    }
}

// ===============================================================
// Timer (animation / decay)
// ===============================================================

void ImproviserControlGUI::timerCallback()
{

    // Decay linearly from 1.0 to 0.0 over `decaySeconds`
    float prev = noteBrightness.load(std::memory_order_relaxed);
    prev *= 0.99f;
    const float decayPerTick = 1.0f / (decaySeconds * (float) frameRateHz);
    float next = juce::jmax(0.0f, prev - decayPerTick);
    noteBrightness.store(next, std::memory_order_relaxed);

    // DBG("Note brightness " << next);

    const bool needRepaint = (prev > 0.1);// || (next > brightnessRedrawThreshold);

    // if (prev <= 0.0f || decaySeconds <= 0.0f || frameRateHz <= 0)
    //     return;

    // const float decayPerTick = 1.0f / (decaySeconds * (float) frameRateHz);
    // float next = juce::jmax(0.0f, prev - decayPerTick);

    // noteBrightness.store(next, std::memory_order_relaxed);

    // // Repaint while above threshold; also repaint once when crossing below to clear
    // const bool needRepaint = (prev > brightnessRedrawThreshold) || (next > brightnessRedrawThreshold);
    if (needRepaint && !midiLightBounds.isEmpty()){
        // DBG("Note brightness " << prev);
        repaint(midiLightBounds);
    }
}

void ImproviserControlGUI::updateFrameTimer()
{
    stopTimer();
    if (frameRateHz > 0)
        startTimerHz(frameRateHz);
}
