/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "ImproviserControlGUI.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MidiMarkovEditor::MidiMarkovEditor (MidiMarkovProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), 
    improControlUI{p.getAPVTState()}, 
    miniPianoKbd{kbdState, juce::MidiKeyboardComponent::horizontalKeyboard}, playing{true}
, learning{true}, sendAllNotesOff{true}
{    


    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize(1024, 768);
    // listen to the mini piano
    kbdState.addListener(this);  
    addAndMakeVisible(miniPianoKbd);
    addAndMakeVisible(resetButton);
    resetButton.addListener(this);

    addAndMakeVisible(improControlUI);

    startTimerHz(30); 

    DBG("Lets' go");
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
    // float colWidth = getWidth() / 3;
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



// polling the processor in a thread safe manner to
void MidiMarkovEditor::timerCallback()
{
    int noteIn; float velIn; int noteOut; float velOut;

    if (audioProcessor.pullMIDIInForGUI(noteIn, velIn, lastSeenStamp))
    {
        // Synthesize a small message to feed the GUI indicator.
        const bool isOn = velIn > 0.0f;
        const int channel = 1; // arbitrary; GUI only uses note/velocity
        juce::MidiMessage m = isOn ? juce::MidiMessage::noteOn(channel, noteIn, velIn)
                                   : juce::MidiMessage::noteOff(channel, noteIn);
        improControlUI.midiReceived(m); // runs on message thread → safe
    }
    if (audioProcessor.pullMIDIOutForGUI(noteOut, velOut, lastSeenStamp))
    {
        // Synthesize a small message to feed the GUI indicator.
        const bool isOn = velOut > 0.0f;
        const int channel = 1; // arbitrary; GUI only uses note/velocity
        juce::MidiMessage m = isOn ? juce::MidiMessage::noteOn(channel, noteOut, velOut)
                                   : juce::MidiMessage::noteOff(channel, noteOut);
        improControlUI.midiSent(m); // runs on message thread → safe
    }
}

