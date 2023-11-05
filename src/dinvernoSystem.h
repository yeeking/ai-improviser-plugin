/*
  ==============================================================================

    dinvernoSystem.h
    This file contains all the  a bridge between the JUCE layer, i.e. midi events etc.
    and the modelling layer, i.e. the markov chains
    Created: 30 Oct 2019 4:45:11pm
    Author:  matthew

  ==============================================================================
*/

#include "../JuceLibraryCode/JuceHeader.h"
#include "MarkovModelCPP/src/MarkovManager.h"
#include "ChordDetector.h"
//#include "LogginManager.h"
//#include "MusicCircleClient.h"
#include "FeedbackControls.h"
#include <queue>
#pragma once


/**
 * This is an abstract class that specifies an interface for
 * the DinvernoImproviser type of improviser
 * The various versions of these improvisers will implement this interface
 * The outer JUCE program can therefore interact with this interface.
 */
class DinvernoImproviser : public FeedbackListener{
public:
/**
 * Constructor sets up sampleRate and starttime for convenience 
 */
   DinvernoImproviser(int sampleRate) : sampleRate(sampleRate), 
                                        startTimeSamples (juce::Time::getMillisecondCounterHiRes() * 0.001 * sampleRate), 
                                        readyToLog{false} {}
    virtual ~DinvernoImproviser() {}
    /** general purpose tick call this from a thread or timer- would normally call both
     * updateTick and generateTick
    */
    virtual void tick() = 0;
    /** periodically update the model (probably from a thread)*/
    virtual void updateTick() {}
    /** periodically generate from the model (probably from a thread) */
    virtual void generateTick() {}
    /** set quanitisation in ms. comes with a default empty implementation*/
    virtual void setQuantisationMs(double msD) {} 
    /** single entry point for passing incoming midi to the improviser. 
     * Typically it will use this input for training and 
     * for triggering a response. if trainFromInput is false, the model 
     * should not train from the input. If true, it should train from the input
    */
    virtual void addMidiMessage(const juce::MidiMessage& msg, bool trainFromInput) = 0;
    virtual void reset() = 0;
    virtual juce::MidiBuffer getPendingMidiMessages();
    //void setLogginManager(LogginManager* loggin);
    //LogginManager* loggin;
    bool isReadyToLog();
    virtual void feedback(FeedbackEventType fbType) override {}
    //MusicCircleClient mcClient{"teresa", "mjlcdm07"};;
   
    /** load model data from the sent filename
     * and use it to setup the 'lead' model
     * default implementation returns ImproviserError::unknown
    */
    virtual bool loadModel(std::string filename){return false;}
    /**
     * Save the lead model to the sent filename 
     * default implementation returns ImproviserError::unknown
     */
    virtual bool saveModel(std::string filename){return false;}
    
protected:
  double getElapsedTimeSamples();   
  double sampleRate;
  double startTimeSamples;
  juce::MidiBuffer pendingMessages;
  const int MAX_PENDING_MESSAGES = 32;
private:
   bool readyToLog;
};

/**
 * Super simple improviser that takes in midi notes
 * and sends back those notes a little while later
 * Made this to test out the buffered midi note construction.
 */
class DinvernoMidiParrot : public DinvernoImproviser {
public:
   DinvernoMidiParrot(int sampleRate);
   ~DinvernoMidiParrot();
   virtual void tick() override;
   virtual void addMidiMessage(const juce::MidiMessage& msg, bool trainFromInput) override;
    virtual void reset() override;
private:
};

/**
 * Super simple improviser that takes in midi notes
 * and sends back those notes a little while later
 * Made this to test out the buffered midi note construction.
 */
class DinvernoRandomMidi : public DinvernoImproviser {
public:
   DinvernoRandomMidi(int sampleRate);
   ~DinvernoRandomMidi();
   virtual void tick() override;
   virtual void addMidiMessage(const juce::MidiMessage& msg, bool trainFromInput) override;
    virtual void reset() override;
    /** prepare a note seq starting at sent time, returns the length.*/
    virtual double prepareRandomNoteSequence(double startTime);

protected:
    juce::Random random{juce::Time::currentTimeMillis()};
    double maxWaitBeteeenRiffs;
    double waitTimeSamples;
    double timeSinceLastPlayed;
    double lastTick;
    
};

class DinvernoRandomEnergy : public DinvernoRandomMidi {
  public:
    DinvernoRandomEnergy(int sampleRate);
    ~DinvernoRandomEnergy();
    virtual void tick() override;
    double prepareRandomNoteSequence(double startTime, double energy);

  private:
    double energy; 
    void addMidiMessage(const juce::MidiMessage& msg, bool trainFromInput) override;
};

class DinvernoMonoMarkov : public DinvernoImproviser {
public:
   DinvernoMonoMarkov(int sampleRate);
   ~DinvernoMonoMarkov();
   virtual void tick() override;
   virtual void addMidiMessage(const juce::MidiMessage& msg, bool trainFromInput) override;
    virtual void reset() override;

private:
  void addNoteOnToModel(int note, int velocity);
  void addNoteOffToModel(int note);
  double getNoteOnTimeSamples(int note);
  double lastTickSamples;
  double accumTimeDelta;
  double timeSinceLastNote;
  std::map<int,double> noteOnTimesSamples;
  MarkovManager pitchModel;
  MarkovManager lengthModel; 
};



class DinvernoPolyMarkov : public DinvernoImproviser {
/** data needed when adding an observation to the 
 * polymarkov model: pitch, velocity and length
 * if lengthOnly, just add to the length model
*/
struct PolyUpdateData{
  std::vector<int> notes{};
  int velocity{0};
  int interOnsetTime{0}; 
  int length{0};
  bool lengthOnly{false}; 
};
public:
   DinvernoPolyMarkov(int sampleRate);
   ~DinvernoPolyMarkov();
   virtual void tick() override;
   virtual void updateTick() override; 
   virtual void generateTick() override; 
   virtual void setQuantisationMs(double ms) override; 

   
   virtual void addMidiMessage(const juce::MidiMessage& msg, bool trainFromInput) override;
    virtual void reset() override;
    virtual void feedback(FeedbackEventType fbType) override;
    virtual bool loadModel(std::string filename) override;
    virtual bool saveModel(std::string filename) override;


private:
/** stores queued updates for the model */
  std::queue<PolyUpdateData> updateQ;
  /** adds the sent update to the q of updates*/
  void queueModelUpdate(PolyUpdateData update);
/** calls front to get oldest update then pops it off and applies it to the*/
  void applyOldestModelUpdate();

/**add a vector of notes to the model. If it is a chord, there will be > 1 note*/
  void addNotesToModel(std::vector<int> notes);
  /** calculate the length for the length mode of the sent note */
  int getNoteLengthForModel(int note);
  /** helper function to convert a vector of notes into a state string */
  state_single notesToMarkovState(std::vector<int> notes);
  /** does the opposite of notesToMarkovState - converts a state back to a vector */
  std::vector<int> markovStateToNotes(state_single n_state);
  /** query the noteOnTimesSamples map with error checking */
  double getNoteOnTimeSamples(int note);
  // void gotoFollowMode();
  // void gotoLeadMode();

  double lastTickSamples;
  double accumTimeDelta;
  double timeBeforeNextNote;
  double lastNoteOnAtSample;
  /** used to round off length and ioi values ignored if zero*/
  int quantisationSamples{0}; 
  std::map<int,double> noteOnTimesSamples;

  // MarkovManager followPitchModel{}; // pitches of notes
  // MarkovManager followLengthModel;  // length of notes
  // MarkovManager followVelocityModel; // loudness of notes
  // MarkovManager followInterOnsetIntervalModel; // time between note onts
  
  // MarkovManager leadPitchModel; // pitches of notes
  // MarkovManager leadLengthModel;  // length of notes
  // MarkovManager leadVelocityModel; // loudness of notes
  // MarkovManager leadInterOnsetIntervalModel; // time between note onts
  

  MarkovManager* pitchModel; // pitches of notes
  MarkovManager* lengthModel;  // length of notes
  MarkovManager* velocityModel; // loudness of notes
  MarkovManager* interOnsetIntervalModel; // time between note onts

  //bool inLeadMode;

  juce::Random random{};

  char FILE_SEP_FOR_SAVE{'@'};

  ChordDetector chordDetector;

};

class ImproviserUtils {
  public:
static int round(int val, int quant)
{
  int a = val % quant;
  int z = val - a;
  if (z == 0) return quant;
  if (a < quant / 2) return z;
  else return z + quant;
}
static std::vector<std::string> tokenise(const std::string& input, char sep)
{
  std::vector<std::string> vec;
  int end;
  auto start = input.find_first_not_of(sep, 0);
  std::string token;
  // find index of sep
  do{
    end = input.find_first_of(sep, start);
    //std::cout << start << " - " << end << std::endl;
    if (start == input.length() || start == end){// whitespace at the end
      break;
    }
    if (end >= 0){// we found it - use end for substr
      token = input.substr(start, end - start);
    }
    else { // we did not find it, use the remaining length of the string
      token = input.substr(start, input.length() - start);
    }
    //std::cout << "got token " << token << std::endl;
    vec.push_back(token);
    // did we find it?
    start = end + 1;
  }while (end > 0);  // yes? continue
  return vec;
}

};

