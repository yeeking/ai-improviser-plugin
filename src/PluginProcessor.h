/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

// #include <JuceHeader.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>
#include <random>
#include <thread>
#include <functional>
#include <atomic>
#include <mutex>
#include "ImproviserControlGUI.h"
#include "MarkovModelCPP/src/MarkovManager.h"
#include "ChordDetector.h"
#include "MIDIMonitor.h"
#include "Behaviours.h"

//==============================================================================
/**
*/
class MidiMarkovProcessor  : public juce::AudioProcessor, 
                             public ImproControlListener
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
    /** add some midi to be played at the sent sample offset - the piano UI bit calls this*/
    void uiAddsMidi(const juce::MidiMessage& msg, int sampleOffset);
    /** reset the model data - does not send all notes off etc. do that manually if you want */
    void resetMarkovModel();

    /** on next processBlock, send all notes off and any other midi needed in a panic */
    void sendAllNotesOff();
    /** call this from anywhere to tell the processor about some midi that was received so it can save it for the GUI to access later */
    void pushMIDIInForGUI(const juce::MidiMessage& msg);
    /** call this from the UI message thread if you want to know what the last received midi message was */
    bool pullMIDIInForGUI(int& note, float& vel, uint32_t& lastSeenStamp);
    /** push current avoid transposition to GUI mailbox */
    void pushAvoidTranspositionForGUI(int semitones);
    /** pull avoid transposition from GUI mailbox */
    bool pullAvoidTranspositionForGUI(int& semitones, uint32_t& lastSeenStamp);
    /** push the current slow-mo scalar to GUI mailbox */
    void pushSlomoScalarForGUI(float scalar);
    /** pull the latest slow-mo scalar from GUI mailbox */
    bool pullSlomoScalarForGUI(float& scalar, uint32_t& lastSeenStamp);
    /** push call/response energy to GUI mailbox */
    void pushCallResponseEnergyForGUI(float energy01);
    /** pull latest call/response energy from GUI mailbox */
    bool pullCallResponseEnergyForGUI(float& energy01, uint32_t& lastSeenStamp);
    /** push call/response phase status to GUI mailbox */
    void pushCallResponsePhaseForGUI(bool enabled, bool inResponse);
    /** pull call/response phase status for GUI */
    bool pullCallResponsePhaseForGUI(bool& enabled, bool& inResponse, uint32_t& lastSeenStamp);
    /** push model status (size/order) to GUI mailbox */
    void pushModelStatusForGUI(int pitchSize, int pitchOrder,
                               int ioiSize, int ioiOrder,
                               int durSize, int durOrder);
    /** pull model status for GUI */
    bool pullModelStatusForGUI(int& pitchSize, int& pitchOrder,
                               int& ioiSize, int& ioiOrder,
                               int& durSize, int& durOrder,
                               uint32_t& lastSeenStamp);
    /** push model IO status (loading/saving) */
    void pushModelIoStatusForGUI(ModelIoState state, const std::string& stage);
    /** pull model IO status */
    bool pullModelIoStatusForGUI(ModelIoState& state, std::string& stage, uint32_t& lastSeenStamp);
    /** call this from anywhere to tell the processor about some midi that was sent so it can save it for the GUI to access later */
    void pushMIDIOutForGUI(const juce::MidiMessage& msg);
    /** call this from the UI message thread if you want to know what the last received midi message was */
    bool pullMIDIOutForGUI(int& note, float& vel, uint32_t& lastSeenStamp);
    /** return true if a new internal clock tick is available and update lastSeenStamp */
    bool pullClockTickForGUI(uint32_t& lastSeenStamp);
    /** return a reference to the APVTS variable */
    juce::AudioProcessorValueTreeState& getAPVTState();
    /** request a BPM increment or decrement from the GUI */
    void requestBpmAdjust(int step);
    /** fetch the BPM currently in effect (host or manual) for UI display */
    void getEffectiveBpmForDisplay(float& bpm, bool& isHostClock) const;

    // implementation of the ImproControlListener interface
    bool loadModel(std::string filename) override;
    bool saveModel(std::string filename) override;
    void resetModel() override; 

private:
    static bool hasExtensionIgnoreCase(const std::string& filename, const std::string& ext);
    static bool shouldCompressForSave(const std::string& filename);
    static bool decompressModelData(const std::string& compressed, std::string& out);
    static bool compressModelData(const std::string& input, std::string& out);
    struct HostClockInfo
    {
        bool hostClockEnabled { false };
        bool transportKnown { false };
        bool transportPlaying { false };
        bool hasPpq { false };
        double ppqPosition { 0.0 };
        bool hasBpm { false };
        double bpm { 0.0 };
        bool hasTimeInSamples { false };
        double timeInSamples { 0.0 };
        bool transportPositionChanged { false };
    };
    bool loadModelString(const std::string& filename);
    bool loadModelBinary(const std::string& filename);
    bool saveModelString(const std::string& filename);
    bool saveModelBinary(const std::string& filename);
    bool startModelIOTask(ModelIoState state, std::string stage, std::function<bool()> ioTask);
    void waitForActiveProcessBlocks() const;

    char FILE_SEP_FOR_SAVE{'@'};

    /** quantise the sent time interval to the nearest multiple of quantBlock */
    static int quantiseInterval(int interval, int quantBlock);
    /** Convert the current BPM/division parameters into samples per internal tick */
    double calculateClockSamplesPerTick(double sampleRate) const;
    double calculateHostClockSamplesPerTick(const HostClockInfo& info) const;
    void pushClockTickForGUI();

  // thread-safe atomics used for simple storage of last received midi note
    std::atomic<int>   lastNoteIn {-1};
    std::atomic<float> lastVelocityIn  {0.0f};            // 0..1
    std::atomic<uint32_t> lastNoteInStamp {0};           // increments on every new note event
    std::atomic<int>   lastAvoidTranspose {0};
    std::atomic<uint32_t> lastAvoidTransposeStamp {0};
    std::atomic<float> lastSlomoScalar {1.0f};
    std::atomic<uint32_t> lastSlomoScalarStamp {0};

    std::atomic<int>   lastNoteOut {-1};
    std::atomic<float> lastVelocityOut  {0.0f};            // 0..1
    std::atomic<uint32_t> lastNoteOutStamp {0};           // increments on every new note event

    /** used to remember if we need to send all notes off on next processBlock */
    std::atomic<bool>   sendAllNotesOffNext {true};
    /** panic function to stop a synth that gets into a bad state.  */
    void sendMidiPanic (juce::MidiBuffer& out, int samplePos);

    juce::AudioProcessorValueTreeState apvts;
    std::mt19937 callResponseRng { std::random_device{}() };

    // these atomics are used to cache the atomics from inside the parameter
    // tree to avoid doing expensive string searches when accessing them in processBlock
    std::atomic<float>* playingParam        = nullptr;
    std::atomic<bool>   lastPlayingParamState {false};
    
    std::atomic<float>* learningParam       = nullptr;
    std::atomic<float>* leadFollowParam       = nullptr;
    std::atomic<float>* avoidParam            = nullptr;
    std::atomic<float>* slowMoParam           = nullptr;
    std::atomic<float>* overpolyParam         = nullptr;
    std::atomic<float>* callResponseParam     = nullptr;
    std::atomic<float>* callResponseGainParam   = nullptr;
    std::atomic<float>* callResponseSilenceParam = nullptr;
    std::atomic<float>* callResponseDrainParam   = nullptr;
    
    std::atomic<float>* playProbabilityParam = nullptr;
    std::atomic<float>* quantiseParam        = nullptr;
    std::atomic<float>* quantUseHostClockParam = nullptr;

    std::atomic<float>* quantBPMParam       = nullptr;
    std::atomic<float>* quantDivisionParam  = nullptr;
    std::atomic<float>* midiInChannelParam  = nullptr;
    std::atomic<float>* midiOutChannelParam = nullptr;
    juce::AudioParameterFloat* quantBpmParamObject = nullptr;
    juce::SpinLock bpmAdjustLock;

    std::atomic<uint32_t> lastClockTickStamp {0};
    double clockSamplesPerTick { 0.0 };
    double clockSamplesAccumulated { 0.0 };
    bool   hostClockPositionInitialised { false };
    double hostClockLastPpq { 0.0 };
    bool   lastHostTransportPlaying { false };
    bool   hostAwaitingFirstTick { true };
    std::optional<double> hostLastKnownTimeInSamples;
    std::optional<double> hostLastKnownPpqPosition;
    bool   hostLastKnownWasPlaying { false };
    int    lastProcessBlockSampleCount { 0 };
    bool   havePreviousBlockInfo { false };
    std::atomic<float> effectiveBpmForDisplay { 120.0f };
    std::atomic<bool>  effectiveBpmIsHost { false };
    std::atomic<float> callResponseEnergyForGui { 0.0f };
    std::atomic<uint32_t> callResponseEnergyStamp { 0 };
    std::atomic<uint32_t> callResponsePhaseStamp { 0 };
    std::atomic<bool> callResponsePhaseEnabled { false };
    std::atomic<bool> callResponsePhaseInResponse { false };
    std::atomic<int> modelSizePitch { 0 };
    std::atomic<int> modelSizeIoI { 0 };
    std::atomic<int> modelSizeDur { 0 };
    std::atomic<int> modelOrderPitch { 0 };
    std::atomic<int> modelOrderIoI { 0 };
    std::atomic<int> modelOrderDur { 0 };
    std::atomic<uint32_t> modelStatusStamp { 0 };
    std::atomic<int> modelIoState { static_cast<int>(ModelIoState::Idle) };
    std::atomic<uint32_t> modelIoStamp { 0 };
    std::string modelIoStage;
    std::mutex modelIoStageMutex;
    CallResponseEngine callResponseEngine;
    std::atomic<bool> modelIoInProgress { false };
    std::atomic<int> processBlockActiveCount { 0 };
    std::thread modelIoThread;

    void analysePitches(const juce::MidiBuffer& midiMessages);
    void analyseIoI(const juce::MidiBuffer& midiMessages, int quantBlockSizeSamples);
    void analyseDuration(const juce::MidiBuffer& midiMessages, int quantBlockSizeSamples);
    void analyseVelocity(const juce::MidiBuffer& midiMessages);

    // processBlock helper steps
    /** in case the UI directly sent us midi */
    void pb_handleMidiFromUI(juce::MidiBuffer& midiMessages);
    /** store the last incoming note for display*/
    void pb_informGuiOfIncoming(const juce::MidiBuffer& midiMessages);
    /** figure out the time */
    HostClockInfo pb_collectHostClockInfo(bool hostClockEnabled);
    /** tick in internal clocl mode */
    void pb_tickInternalClock(const juce::AudioBuffer<float>& buffer);
    /** tick in host clock mode */
    void pb_tickHostClock(bool transportPlaying, bool hostHasPpq, double hostPpqPosition);
    /** update the model with new midi */
    void pb_learnFromIncomingMidi(const juce::MidiBuffer& midiMessages, double effectiveBpm);
    /** peg note offs for future refenec */
    void pb_schedulePendingNoteOffs(juce::MidiBuffer& buffer, unsigned long blockStart, unsigned long blockEnd);
    /** store last sent midi for the UI to pick up  */
    void pb_informGuiOfOutgoing(const juce::MidiBuffer& midiMessages);
    /** remove generated notes if probablity set */
    void pb_applyPlayProbability(juce::MidiBuffer& midiMessages);
    /** tell the midi logger about our notes */
    void pb_logMidiEvents(const juce::MidiBuffer& midiMessages);
    /**  */
    bool pb_handlePlayingState(juce::MidiBuffer& midiMessages, bool hostAllowsPlayback, bool allOffRequested);
    /** feed incoming note-ons into avoid strategy buffer */
    void pb_recordIncomingNotesForAvoid(const juce::MidiBuffer& midiMessages);
    /** track call/response inputs */
    void pb_trackCallResponseInput(const juce::MidiBuffer& midiMessages, unsigned long bufferStart);
    /** randomise other behaviour toggles when entering response */
    void pb_randomiseBehaviourTogglesForResponse();
    /** deal with stuck notes */
    void pb_handleStuckNotes(juce::MidiBuffer& midiMessages, unsigned long elapsedSamplesAtEnd);
    /** send all notes off if needed */
    void pb_sendPendingAllNotesOff(juce::MidiBuffer& midiMessages, bool allOffRequested);

    std::optional<unsigned long> computeNextInternalTickSample() const;
    std::optional<unsigned long> computeNextHostTickSample(const HostClockInfo& info) const;
    void alignModelPlayTimeToNextTick(bool hostClockEnabled, const HostClockInfo& info);

    std::string notesToMarkovState (const std::vector<int>& notesVec);
    std::vector<int> markovStateToNotes (const std::string& notesStr);
    juce::MidiBuffer generateNotesFromModel(const juce::MidiBuffer& incomingNotes, unsigned long bufferStartTime, unsigned long bufferEndTime, const HostClockInfo& hostInfo);

    // juce::MidiBuffer generateNotesFromModel(const juce::MidiBuffer& incomingMessages);
    // return true if time to play a note
    // bool isTimeToPlayNote(unsigned long currentTime);
    bool isTimeToPlayNote(unsigned long windowStartTime, unsigned long windowEndTime);

    // call after playing a note 
    void updateTimeForNextPlay();
    void syncNextTimeToClock(const HostClockInfo& info);
    int sanitiseNote(int note) const;

    /** stores messages added from the addMidi function*/
    juce::MidiBuffer midiReceivedFromUI;

    MarkovManager pitchModel;
    MarkovManager polyphonyModel; 
    MarkovManager iOIModel;
    MarkovManager noteDurationModel;    
    MarkovManager velocityModel;    

    unsigned long lastIncomingNoteOnTime; 
    bool noMidiYet; 
    unsigned long noteOffTimes[127];
    unsigned long noteOnTimes[127];
    
    unsigned long elapsedSamples; 
    unsigned long lastOutgoingNoteOnTime; 
    
    unsigned long nextTimeToPlayANote;

    ChordDetector chordDetect;
    MIDIMonitor midiMonitor;
    AvoidStrategy avoidStrategy {};
    SlomoStrategy slomoStrategy {};


      //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiMarkovProcessor)
};
