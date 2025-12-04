/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "ImproviserControlGUI.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "StatusWidgets.h"

//==============================================================================
MidiMarkovEditor::MidiMarkovEditor (MidiMarkovProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), 
    improControlUI{p.getAPVTState(), p}, 
    miniPianoKbd{kbdState, juce::MidiKeyboardComponent::horizontalKeyboard}, playing{true}
, learning{true}, sendAllNotesOff{true}
{    


    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize(800, 600);
    // listen to the mini piano
    kbdState.addListener(this);  
    mainTabContainer.addAndMakeVisible(miniPianoKbd);
    mainTabContainer.addAndMakeVisible(resetButton);
    resetButton.addListener(this);

    mainTabContainer.addAndMakeVisible(improControlUI);
    tabComponent.addTab("Controls", juce::Colours::darkgrey, &mainTabContainer, false);
    tabComponent.addTab("Status", juce::Colours::darkgrey, &blankTabContainer, false);
    blankTabContainer.addAndMakeVisible(pitchOrderCircle);
    blankTabContainer.addAndMakeVisible(callResponseMeter);
    addAndMakeVisible(tabComponent);

    startTimerHz(30); 

    // DBG("Lets' go");

    layoutMainTab();
}

MidiMarkovEditor::~MidiMarkovEditor()
{
    stopTimer();
}

//==============================================================================
void MidiMarkovEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

   // g.setColour (juce::Colours::white);
   // g.setFont (15.0f);
   // g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);
}

void MidiMarkovEditor::resized()
{
    tabComponent.setBounds(getLocalBounds());
    layoutMainTab();
}

void MidiMarkovEditor::layoutMainTab()
{
    auto area = tabComponent.getLocalBounds();
    area.removeFromTop(tabComponent.getTabBarDepth());

    mainTabContainer.setBounds(area);
    blankTabContainer.setBounds(area);

    const float keyboardHeight = static_cast<float>(area.getHeight()) * 0.14f;
    miniPianoKbd.setBounds(0, 0, area.getWidth(), static_cast<int>(keyboardHeight));

    resetButton.setBounds(area.getWidth() - 100, 4, 96, 28);

    const int remainingHeight = area.getHeight() - static_cast<int>(keyboardHeight);
    improControlUI.setBounds(0, static_cast<int>(keyboardHeight), area.getWidth(), remainingHeight);

    auto statusArea = area.reduced(30);
    auto meterArea = statusArea.removeFromTop(80);
    callResponseMeter.setBounds(meterArea);
    pitchOrderCircle.setBounds(statusArea);
}

 void MidiMarkovEditor::sliderValueChanged (juce::Slider *slider)
{

}

void MidiMarkovEditor::buttonClicked(juce::Button* btn)
{
    if (btn == &resetButton){
        audioProcessor.resetMarkovModel();
        audioProcessor.sendAllNotesOff();
    }
}

void MidiMarkovEditor::handleNoteOn(juce::MidiKeyboardState *source, int midiChannel, int midiNoteNumber, float velocity)
{
    juce::MidiMessage msg1 = juce::MidiMessage::noteOn(midiChannel, midiNoteNumber, velocity);
    // DBG(msg1.getNoteNumber());
    audioProcessor.uiAddsMidi(msg1, 0);
    
}

void MidiMarkovEditor::handleNoteOff(juce::MidiKeyboardState *source, int midiChannel, int midiNoteNumber, float velocity)
{
    juce::MidiMessage msg2 = juce::MidiMessage::noteOff(midiChannel, midiNoteNumber, velocity);
    audioProcessor.uiAddsMidi(msg2, 0); 
}



// polling the processor in a thread safe manner to
void MidiMarkovEditor::timerCallback()
{
    int noteIn; float velIn; int noteOut; float velOut;

    if (audioProcessor.pullMIDIInForGUI(noteIn, velIn, lastMidiInStamp))
    {
        // Synthesize a small message to feed the GUI indicator.
        const bool isOn = velIn > 0.0f;
        const int channel = 1; // arbitrary; GUI only uses note/velocity
        juce::MidiMessage m = isOn ? juce::MidiMessage::noteOn(channel, noteIn, velIn)
                                   : juce::MidiMessage::noteOff(channel, noteIn);
        improControlUI.midiReceived(m); // runs on message thread → safe
    }
    if (audioProcessor.pullMIDIOutForGUI(noteOut, velOut, lastMidiOutStamp))
    {
        // Synthesize a small message to feed the GUI indicator.
        const bool isOn = velOut > 0.0f;
        const int channel = 1; // arbitrary; GUI only uses note/velocity
        juce::MidiMessage m = isOn ? juce::MidiMessage::noteOn(channel, noteOut, velOut)
                                   : juce::MidiMessage::noteOff(channel, noteOut);
        improControlUI.midiSent(m); // runs on message thread → safe
    }

    if (audioProcessor.pullClockTickForGUI(lastClockTickStamp))
    {
        improControlUI.clockTicked();
    }

    int avoidSemitone = 0;
    if (audioProcessor.pullAvoidTranspositionForGUI(avoidSemitone, lastAvoidTransposeStamp))
    {
        improControlUI.setAvoidTransposition(avoidSemitone);
    }

    float slomoScalar = 1.0f;
    if (audioProcessor.pullSlomoScalarForGUI(slomoScalar, lastSlomoScalarStamp))
    {
        improControlUI.setSlowMoScalar(slomoScalar);
    }

    int overpolyExtra = 0;
    if (audioProcessor.pullOverpolyExtraForGUI(overpolyExtra, lastOverpolyExtraStamp))
    {
        improControlUI.setOverpolyExtra(overpolyExtra);
    }

    float callRespEnergy = 0.0f;
    if (audioProcessor.pullCallResponseEnergyForGUI(callRespEnergy, lastCallResponseEnergyStamp))
    {
        improControlUI.setCallResponseEnergy(callRespEnergy);
        callResponseMeter.setEnergy(callRespEnergy);
    }

    bool callRespEnabled = false;
    bool callRespInResponse = false;
    if (audioProcessor.pullCallResponsePhaseForGUI(callRespEnabled, callRespInResponse, lastCallResponsePhaseStamp))
    {
        improControlUI.setCallResponsePhase(callRespEnabled, callRespInResponse);
        callResponseMeter.setState(callRespEnabled, callRespInResponse);
    }

    int pitchSize = 0, pitchOrder = 0;
    int ioiSize = 0, ioiOrder = 0;
    int durSize = 0, durOrder = 0;
    if (audioProcessor.pullModelStatusForGUI(pitchSize, pitchOrder, ioiSize, ioiOrder, durSize, durOrder, lastModelStatusStamp))
    {
        improControlUI.setModelStatus(pitchSize, pitchOrder, ioiSize, ioiOrder, durSize, durOrder);
        pitchOrderCircle.setOrder(pitchOrder);
    }

    ModelIoState ioState = ModelIoState::Idle;
    std::string ioStage;
    if (audioProcessor.pullModelIoStatusForGUI(ioState, ioStage, lastModelIoStamp))
    {
        improControlUI.setModelIoStatus(ioState, ioStage);
    }

    float displayBpm = 0.0f;
    bool displayHost = false;
    audioProcessor.getEffectiveBpmForDisplay(displayBpm, displayHost);
    improControlUI.setExternalBpmDisplay(displayBpm, displayHost);
}
