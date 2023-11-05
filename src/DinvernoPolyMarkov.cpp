/*
  ==============================================================================

    DinvernoPolyMarkov.cpp
    Created: 4 Nov 2019 3:52:54pm
    Author:  matthew

  ==============================================================================
*/

#include "dinvernoSystem.h"
#include <fstream>


DinvernoPolyMarkov::DinvernoPolyMarkov(int sampleRate) : 
              chordDetector(ChordDetector{sampleRate}), 
              //inLeadMode{true}, 
              DinvernoImproviser(sampleRate)
{
  lastTickSamples = 0;
  accumTimeDelta = 0;
  timeBeforeNextNote = 0;
  lastNoteOnAtSample = 0;
  pitchModel = new MarkovManager(); // pitches of notes
  lengthModel = new MarkovManager();  // length of notes
  velocityModel = new MarkovManager(); // loudness of notes
  interOnsetIntervalModel = new MarkovManager(); // time between note onts

 // gotoLeadMode();

}
DinvernoPolyMarkov::~DinvernoPolyMarkov()
{
  delete pitchModel; // pitches of notes
  delete lengthModel;  // length of notes
  delete velocityModel; // loudness of notes
  delete interOnsetIntervalModel; // time between note onts

}

void DinvernoPolyMarkov::reset()
{

    //std::cout << "DinvernoPolyMarkov::reset" << std::endl;
    pendingMessages.clear();
    // send all notes off
    pendingMessages.addEvent(MidiMessage::allNotesOff(1), 0);

    pitchModel->reset();
    lengthModel->reset();
    velocityModel->reset();
    interOnsetIntervalModel->reset();

    chordDetector.reset();
  // clear the update Q
  while(updateQ.size() > 0) updateQ.pop();

}


// this will be called periodically
// this is where do our generation
// which results in midi notes being added
// to the pendingevents list
void DinvernoPolyMarkov::tick()
{
  this->generateTick();
  this->updateTick();
}

void DinvernoPolyMarkov::generateTick()
{
  double nowSamples = (Time::getMillisecondCounterHiRes() * 0.001 * sampleRate);
  double tDelta = nowSamples - lastTickSamples;
  accumTimeDelta += tDelta;
  double timeSinceLastNoteSeconds = (nowSamples - startTimeSamples - lastNoteOnAtSample) / sampleRate;
  if (accumTimeDelta > timeBeforeNextNote &&  // time to play
      timeSinceLastNoteSeconds < 15)  // it is less than 15 secs since human played
  {
        
    // reset the wait time
    accumTimeDelta = 0;
    // same length for all notes
    int noteLen = std::stoi(lengthModel->getEvent());
    int waitLen = std::stoi(interOnsetIntervalModel->getEvent());
    if (waitLen == 0)
    {
      waitLen = (int) sampleRate; 
    }

    if (noteLen > 0){
      state_single event = pitchModel->getEvent();
      std::vector<int> notes = markovStateToNotes(event);
      //DBG("DinvernoPolyMarkov::generateTick playing notes " << notes.size());
        
      for (int& note : notes){
        if (note > 0)
        {
          uint8 velocity = std::stoi(velocityModel->getEvent());
          MidiMessage noteOn = MidiMessage::noteOn(1, note, velocity);
          MidiMessage noteOff = MidiMessage::noteOff(1, note, velocity);
          auto sampleNumber =  getElapsedTimeSamples() + sampleRate; // 1 second late
          if (pendingMessages.getNumEvents() < MAX_PENDING_MESSAGES){
            pendingMessages.addEvent(noteOn, sampleNumber);
          // length should be dictated by the length 
            pendingMessages.addEvent(noteOff, sampleNumber + noteLen);
          }
          //std::cout << "note len " << noteLen << " wait len " << waitLen << std::endl;
          timeBeforeNextNote = waitLen; // we'll use this as a wait time          
        }
      }
    }
  }

  // now dequeue an update.
  // do one at a time since the tick will run again soon anyways
  lastTickSamples = nowSamples;
}
void DinvernoPolyMarkov::updateTick()
{
  this->applyOldestModelUpdate();
}


void DinvernoPolyMarkov::addMidiMessage(const MidiMessage& message, bool trainFromInput)
{

  if (message.isNoteOn()){
    // how long since we started running the algorithm?
    double elapsedSamples  = getElapsedTimeSamples();//(Time::getMillisecondCounterHiRes() * 0.001 * sampleRate) - startTimeSamples;
    noteOnTimesSamples[message.getNoteNumber()] = elapsedSamples;
    // now tell the chord detector about the note
    chordDetector.notePlayed(message.getNoteNumber(), elapsedSamples);
    // see if the chord detector has anything for us
    std::vector<int> notes = chordDetector.getReadyNotes();
    if (notes.size() > 0){
      // Q the update 
      int interOnsetInterval = elapsedSamples - lastNoteOnAtSample;
      PolyUpdateData update{
        notes, message.getVelocity(), interOnsetInterval, 0, false
      };
      // only queue the model update
      // if we are training
      if (trainFromInput) this->queueModelUpdate(update);

      lastNoteOnAtSample = elapsedSamples; 
      // test: how long did it take?
      double elapsedSamplesNow  = (Time::getMillisecondCounterHiRes() * 0.001 * sampleRate) - startTimeSamples;
      //DBG("DinvernoPolyMarkov::addMidiMessage complete. Took samples: " + std::to_string(elapsedSamplesNow - elapsedSamples));
    }
  }
  // probably we need a chord detector 
  // for note offs since otherwise we end with chords adding 
  // > 1entry to the length model 
  if (message.isNoteOff()){
     PolyUpdateData update{
        length:getNoteLengthForModel(message.getNoteNumber()), lengthOnly:true
    };
    if (trainFromInput) this->queueModelUpdate(update);
  }  
}
void DinvernoPolyMarkov::addNotesToModel(std::vector<int> notes)
{
  state_single n_state = notesToMarkovState(notes);
  // remember when this note started so we can measure length later
  pitchModel->putEvent(n_state);
}
state_single DinvernoPolyMarkov::notesToMarkovState(std::vector<int> notes)
{ 
  state_single mstate = "";
  for (int& note : notes){
    mstate += std::to_string(note) + "-";
  }
  return mstate;
}
std::vector<int> DinvernoPolyMarkov::markovStateToNotes(state_single n_state)
{
  std::vector<int> notes;
  if (n_state == "0") return notes;
  //std::cout << "DinvernoPolyMarkov::markovStateToNotes got state " << n_state << std::endl;
  for (std::string& note_s : ImproviserUtils::tokenise(n_state, '-'))
  {
    //std::cout << "DinvernoPolyMarkov::markovStateToNotes got substate " << note_s << std::endl;
    notes.push_back(std::stoi(note_s));
  }
  return notes;
}


int DinvernoPolyMarkov::getNoteLengthForModel(int note)
{
    double noteStart = getNoteOnTimeSamples(note);
    int elapsedSamples  = (Time::getMillisecondCounterHiRes() * 0.001 * sampleRate) - startTimeSamples;
    int noteLen = elapsedSamples - noteStart;
    return noteLen;
}

double DinvernoPolyMarkov::getNoteOnTimeSamples(int note)
{
  double time;
  try
  {
    time = noteOnTimesSamples.at(note);
  }
  catch (const std::out_of_range& oor)
  {
    time = getElapsedTimeSamples() - sampleRate;// set it to 1 second ago as we don't have a start time
  }
  return time; 
}


void DinvernoPolyMarkov::feedback(FeedbackEventType fbType)
{

    switch (fbType) 
    // this is considered to be bad engineering because it is 'control coupling'
    // where one module sends a control flag to another one
    // which dictates its execution path
    // what I should do is make the FeedbackListener interface
    // more complex but I don't want to do that 
    {
        case FeedbackEventType::negative:
            pitchModel->giveNegativeFeedback(); 
            lengthModel->giveNegativeFeedback();  
            velocityModel->giveNegativeFeedback(); 
            interOnsetIntervalModel->giveNegativeFeedback(); 

            break;
        case FeedbackEventType::positive:
            pitchModel->givePositiveFeedback();             
            lengthModel->givePositiveFeedback();
            velocityModel->givePositiveFeedback(); 
            interOnsetIntervalModel->givePositiveFeedback(); 
            break;
        // case FeedbackEventType::follow:
        //     gotoFollowMode();
        //     break;
        // case FeedbackEventType::lead:
        //     gotoLeadMode();
        //     break;  
    }
}


void DinvernoPolyMarkov::queueModelUpdate(PolyUpdateData update)
{
  updateQ.push(update);
  //DBG("DinvernoPolyMarkov::queueModelUpdate pending updates " + std::to_string(updateQ.size()));
}
/** dequeues and applies one model update*/
void DinvernoPolyMarkov::applyOldestModelUpdate()
{
  if (updateQ.size() == 0) return; 
  //DBG("DinvernoPolyMarkov::applyOldestModelUpdate pending updates " + std::to_string(updateQ.size()));

  PolyUpdateData update = updateQ.front();
  updateQ.pop();

  // now process it 
  if (update.lengthOnly) {// note off - update length
    //DBG("DinvernoPolyMarkov::applyOldestModelUpdate len  " << update.length) ;
    if (update.length > 0) {
      // apply quaunt
      if (this->quantisationSamples != 0){
        update.length = ImproviserUtils::round(update.length, this->quantisationSamples);
      }
      lengthModel->putEvent(std::to_string(update.length));
    }
  }
  else{// note on - update pitch, ioi and velocity
    if (update.notes.size() > 0){
      //DBG("DinvernoPolyMarkov::applyOldestModelUpdate got notes  " << update.notes.size());
      addNotesToModel(update.notes);
      velocityModel->putEvent(std::to_string(update.velocity));
      if (update.interOnsetTime < sampleRate * 3) {// 3 seconds or less
          // apply quantisation
          if (this->quantisationSamples != 0){
            update.interOnsetTime = ImproviserUtils::round(update.interOnsetTime, this->quantisationSamples);
          }
          interOnsetIntervalModel->putEvent(std::to_string(update.interOnsetTime));
      }
    }
  }
  //DBG("DinvernoPolyMarkov::applyOldestModelUpdate pending updates " + std::to_string(updateQ.size()));

}

void DinvernoPolyMarkov::setQuantisationMs(double msD)
{
  this->quantisationSamples = (int) (msD * 0.001 * sampleRate);
}

bool DinvernoPolyMarkov::loadModel(std::string filename)
{
  if (std::ifstream in {filename})
  {
    std::ostringstream sstr{};
    sstr << in.rdbuf();
    std::string data = sstr.str();
    in.close();
    // now split the data on the header 
    std::vector<std::string> modelStrings = MarkovChain::tokenise(data, this->FILE_SEP_FOR_SAVE);
    // do some checks on the modelStrings
    if (modelStrings.size() != 4) {
      DBG("DinvernoPolyMarkov::loadModel did not find 4 model strings in file " << filename);
      return false; 
    }
    std::vector<MarkovManager*> mms = {pitchModel, lengthModel, velocityModel, interOnsetIntervalModel};
    for (auto i = 0; i<mms.size();i++)
    {
      bool loaded = mms[i]->setupModelFromString(modelStrings[i]);
      if (!loaded){
        DBG("DinvernoPolyMarkov::loadModel error loading model "<<i << " from " << filename);
        return false; 
      }
      else{
        DBG("DinvernoPolyMarkov::loadModel loaded model "<<i << " from " << filename);
      }
    }

    return true; 


  }
  else {
    std::cout << "DinvernoPolyMarkov::loadModel failed to load from file " << filename << std::endl;
    return false; 
  }
}
bool DinvernoPolyMarkov::saveModel(std::string filename)
{
  // we have four models so write each to a temp file
  // read it in as a string
  std::string data{""};
  std::vector<MarkovManager*> mms = {pitchModel, lengthModel, velocityModel, interOnsetIntervalModel};
  for (MarkovManager* mm : mms)
  {
    data += this->FILE_SEP_FOR_SAVE;
    data += mm->getModelAsString();
  }
  if (std::ofstream ofs{filename}){
    ofs << data;
    ofs.close();
    return true; 
  }
  else {
    std::cout << "DinvernoPolyMarkov::saveModel failed to save to file " << filename << std::endl;
    return false; 
  }

}

