/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PolyLeadFollow.h"

//==============================================================================
/**
*/
class AimusoAudioProcessor  : public juce::AudioProcessor
{
public:
    //==============================================================================
    AimusoAudioProcessor();
    ~AimusoAudioProcessor() override;

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


    //==== custom stuff relating to controlling the algorithms ///
    /** sets the quantisation on the improvisers in ms */
    void setQuantisationMs(double ms);
    /** set the improviser to lead mode */
    void leadMode();
    /** set the improviser to follow mode */
    void followMode(); 
    /** clears the memories on all the models*/   
    void resetModels();
    /** sets the midi output channel range 1-16*/
    void setMidiOutChannel(int ch); 
    /** sets the midi input channel range 0-16. If zero, listens to all channels*/
    void setMidiInChannel(int ch); 
    /** returns true if the model is training
     * returns false if the model is not training
    */
    bool isTraining();
    /** allow the model to train */
    void enableTraining();
    /** stop the model from training */
    void disableTraining();
    /** are you playing? */
    bool isPlaying();
    /** ai will play*/
    void enablePlaying();
    /** ai will not play*/
    void disablePlaying();
    /** load model data from the sent filename
     * and use it to setup the 'lead' model
     * return true if it works, false otherwise
    */
    bool loadModel(std::string filename);
    /**
     * Save the lead model to the sent filename 
     * return true if it works, false otherwise
     */
    bool saveModel(std::string filename);
    
    /** update the play probability  value */
    void setPlayProb(double playProb);
    double getPlayProb();

    // set the cc number for updateing play prob
    void setPlayProbCC(int ccNum);
private: // private fields for PluginProcessor
    int midiOutChannel{1};
    int midiInChannel{0}; 
    bool clearMidiBuffer{false};
    bool iAmTraining{true};
    bool iAmPlaying{true};
    // override for the playback probablity
    double playbackProb{1};
    int playbackProbCC{1};// default to mod wheel
    juce::Random rng;
    void handleCC(MidiMessage& ccMsg);

    //ThreadedImprovisor threadedImprovisor;
    /** initialise a polylead follow*/    
    PolyLeadFollow polyLeadFollow{44100};    
    /** assign the polyleadfollow to the currentImproviser
     * in case at some point we want other improvisers available 
    */
    DinvernoImproviser* currentImproviser{&polyLeadFollow}; 
    //==============================================================================
    /** background thread that 
     * passes queud updates to the model
    */
    class UpdateTicker : public juce::Timer
    {
        public: 
        void timerCallback() override {
            if (improviser != 0) improviser->updateTick();
            else DBG("UpdateTicker: no improviser. call setImproviser");
        }
        void setImproviser(DinvernoImproviser* impro) {improviser = impro;}
        private:
        DinvernoImproviser* improviser{0}; 
    };
    class GenerateTicker : public juce::Timer
    {
        public: 
        void timerCallback() override {
            if (improviser != 0) improviser->generateTick();
            else DBG("GenerateTicker: no improviser. call setImproviser");
        }        
        void setImproviser(DinvernoImproviser* impro) {improviser = impro;}
        private:
        DinvernoImproviser* improviser{0}; 
    };
    UpdateTicker updateTicker{};
    GenerateTicker generateTicker{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AimusoAudioProcessor)
};
