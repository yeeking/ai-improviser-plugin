/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MidiMarkovEditor::MidiMarkovEditor (MidiMarkovProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), 
    miniPianoKbd{kbdState, juce::MidiKeyboardComponent::horizontalKeyboard}, playing{true}
, learning{true}, sendAllNotesOff{true}
{    


    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (700, 350);

    // listen to the mini piano
    kbdState.addListener(this);  
    addAndMakeVisible(miniPianoKbd);
    addAndMakeVisible(resetButton);
    resetButton.addListener(this);

    addAndMakeVisible(improControlUI);
    improControlUI.addImproviserControlListener(this);

    startTimerHz(30); 

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
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    float rowHeight = getHeight()/5; 
    float colWidth = getWidth() / 3;
    float row = 0;


    miniPianoKbd.setBounds(0, rowHeight*row, getWidth(), rowHeight);
    row ++ ; 
    improControlUI.setBounds(0, rowHeight*row, getWidth(), rowHeight * 4);
    // resetButton.setBounds(0, rowHeight*row, getWidth(), rowHeight);
    // row ++ ;
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
    audioProcessor.addMidi(msg1, 0);
    
}

void MidiMarkovEditor::handleNoteOff(juce::MidiKeyboardState *source, int midiChannel, int midiNoteNumber, float velocity)
{
    juce::MidiMessage msg2 = juce::MidiMessage::noteOff(midiChannel, midiNoteNumber, velocity);
    audioProcessor.addMidi(msg2, 0); 
}


// // directly receiving midi
// void MidiMarkovEditor::midiReceived(const juce::MidiMessage& msg)
// {
//     DBG("Editor received midi "<< msg);
//     improControlUI.midiReceived(msg);
// }

// polling the processor in a thread safe manner to
void MidiMarkovEditor::timerCallback()
{
    int note; float vel;
    if (audioProcessor.pullUiMidi(note, vel, lastSeenStamp))
    {
        // Synthesize a small message to feed the GUI indicator.
        const bool isOn = vel > 0.0f;
        const int channel = 1; // arbitrary; GUI only uses note/velocity
        juce::MidiMessage m = isOn ? juce::MidiMessage::noteOn(channel, note, vel)
                                   : juce::MidiMessage::noteOff(channel, note);
        improControlUI.midiReceived(m); // runs on message thread → safe
    }
}


// Improviser control listener interface
void MidiMarkovEditor::playingOff()
{
    playing = false; 
    audioProcessor.sendAllNotesOff();

    // now trigger all notes off
}

void MidiMarkovEditor::playingOn() 
{

}

void MidiMarkovEditor::learningOn() {}
void MidiMarkovEditor::learningOff(){}

void MidiMarkovEditor::setPlayProbability(float prob){}

void MidiMarkovEditor::setQuantBPM(float bpm)        {}
void MidiMarkovEditor::setQuantDivision(float division){}

void MidiMarkovEditor::setMIDIInChannel(int ch) {}  // 0 = All, 1-16 explicit
void MidiMarkovEditor::setMIDIOutChannel(int ch){}  // 1-16

void MidiMarkovEditor::loadModelDialogue(){}
void MidiMarkovEditor::saveModelDialogue(){}


