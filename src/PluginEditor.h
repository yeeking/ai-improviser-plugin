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
                          // private ImproviserControlListener, 
                          private juce::Timer // for polling the processor for new midi

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
    void handleNoteOn(juce::MidiKeyboardState *source, int midiChannel, int midiNoteNumber, float velocity) override; 
    // from MidiKeyboardState
    void handleNoteOff(juce::MidiKeyboardState *source, int midiChannel, int midiNoteNumber, float velocity) override; 
  
    // from Timer
    void timerCallback() override; // polls processor mailbox


private:

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    MidiMarkovProcessor& audioProcessor;


    uint32_t lastSeenStamp { 0 };

    // needed for the mini piano keyboard
    ImproviserControlGUI improControlUI;

    juce::MidiKeyboardState kbdState;
    juce::MidiKeyboardComponent miniPianoKbd; 
    juce::TextButton resetButton; 
    
    bool playing;
    bool learning;
    bool sendAllNotesOff; 


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMarkovEditor)
};
