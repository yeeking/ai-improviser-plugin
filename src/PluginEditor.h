/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

//#include <JuceHeader.h>
#include "PluginProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include "ImproviserControlGUI.h"

//==============================================================================
/**
*/
class MidiMarkovEditor  :   public juce::AudioProcessorEditor,
                          // listen to buttons
                          public juce::Button::Listener, 
                          // listen to sliders
                          public juce::Slider::Listener, 
                          // listen to piano keyboard widget
                          private juce::MidiKeyboardState::Listener, 
                          private ImproviserControlListener

{
public:
    MidiMarkovEditor (MidiMarkovProcessor&);
    ~MidiMarkovEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

    void sliderValueChanged (juce::Slider *slider) override;
    void buttonClicked(juce::Button* btn) override;
    // from MidiKeyboardState
    void handleNoteOn(juce::MidiKeyboardState *source, int midiChannel, int midiNoteNumber, float
 velocity) override; 
     // from MidiKeyboardState
    void handleNoteOff(juce::MidiKeyboardState *source, int midiChannel, int midiNoteNumber, float velocity) override; 
    

    void playingOff() override ;
    void playingOn()  override ;

    void learningOn()  override ;
    void learningOff() override ;

    void setPlayProbability(float prob) override ;

    void setQuantBPM(float bpm)         override ;
    void setQuantDivision(float division) override ;

    void setMIDIInChannel(int ch)  override ;  // 0 = All, 1-16 explicit
    void setMIDIOutChannel(int ch) override ;  // 1-16

    void loadModelDialogue() override ;
    void saveModelDialogue() override ;

      // from improviser control listener


private:

    // needed for the mini piano keyboard
    ImproviserControlGUI improControlUI;

    juce::MidiKeyboardState kbdState;
    juce::MidiKeyboardComponent miniPianoKbd; 
    juce::TextButton resetButton; 

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    MidiMarkovProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMarkovEditor)
};
