/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

// #include <JuceHeader.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "MarkovModelCPP/src/MarkovManager.h"

//==============================================================================
/**
*/
class MidiMarkovProcessor  : public juce::AudioProcessor
                            #if JucePlugin_Enable_ARA
                             , public juce::AudioProcessorARAExtension
                            #endif
{
public:
    //==============================================================================
    MidiMarkovProcessor();
    ~MidiMarkovProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;
    /** add some midi to be played at the sent sample offset*/
    void addMidi(const juce::MidiMessage& msg, int sampleOffset);
    /** reset the model data - does not send all notes off etc. do that manually if you want */
    void resetMarkovModel();
    /** on next processBlock, send all notes off and any other midi needed in a panic */
    void sendAllNotesOff();
    /** call this from anywhere to tell the processor about some midi that was received so it can save it for the GUI to access later */
    void pushMIDIInForGUI(const juce::MidiMessage& msg);
    /** call this from the UI message thread if you want to know what the last received midi message was */
    bool pullMIDIInForGUI(int& note, float& vel, uint32_t& lastSeenStamp);
    /** call this from anywhere to tell the processor about some midi that was sent so it can save it for the GUI to access later */
    void pushMIDIOutForGUI(const juce::MidiMessage& msg);
    /** call this from the UI message thread if you want to know what the last received midi message was */
    bool pullMIDIOutForGUI(int& note, float& vel, uint32_t& lastSeenStamp);


private:

  // thread-safe atomics used for simple storage of last received midi note
    std::atomic<int>   lastNoteIn {-1};
    std::atomic<float> lastVelocityIn  {0.0f};            // 0..1
    std::atomic<uint32_t> lastNoteInStamp {0};           // increments on every new note event

    std::atomic<int>   lastNoteOut {-1};
    std::atomic<float> lastVelocityOut  {0.0f};            // 0..1
    std::atomic<uint32_t> lastNoteOutStamp {0};           // increments on every new note event


    void analysePitches(const juce::MidiBuffer& midiMessages);
    void analyseIoI(const juce::MidiBuffer& midiMessages);
    void analyseDuration(const juce::MidiBuffer& midiMessages);

    juce::MidiBuffer generateNotesFromModel(const juce::MidiBuffer& incomingMessages);
    // return true if time to play a note
    bool isTimeToPlayNote(unsigned long currentTime);
    // call after playing a note 
    void updateTimeForNextPlay();

    /** stores messages added from the addMidi function*/
    juce::MidiBuffer midiToProcess;
    MarkovManager pitchModel;
    MarkovManager iOIModel;
    MarkovManager noteDurationModel;    

    unsigned long lastNoteOnTime; 
    bool noMidiYet; 
    unsigned long noteOffTimes[127];
    unsigned long noteOnTimes[127];

    unsigned long elapsedSamples; 
    unsigned long modelPlayNoteTime;
    /** used to remember if we need to send all notes off on next processBlock */
    std::atomic<bool>   sendAllNotesOffNext {true};

      //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMarkovProcessor)
};
