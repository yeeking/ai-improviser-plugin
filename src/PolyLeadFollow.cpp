/*
  ==============================================================================

    PolyLeadFollow.cpp
    Created: 9 Mar 2021 1:23:57pm
    Author:  matthewyk

  ==============================================================================
*/

#include "PolyLeadFollow.h"

PolyLeadFollow::PolyLeadFollow(int sampleRate)
 :  DinvernoImproviser{sampleRate},  longTermMarkov{sampleRate}, shortTermMarkov{sampleRate}, noteCounter{0}
 {
   currentPoly = &longTermMarkov;
 }

void PolyLeadFollow::tick()
{
  
}
void PolyLeadFollow::generateTick()
{
    inTick = true;
  currentPoly->generateTick();
    inTick = false;
}
void PolyLeadFollow::updateTick()
{
    inTick = true;
  currentPoly->updateTick();
    inTick = false;
}

void PolyLeadFollow::addMidiMessage(const MidiMessage& msg, bool trainFromInput)
{
  // reset is pending
  //DBG("PolyLeadFollow::addMidiMessage " << noteCounter);
  // only want notes for now 
  if (msg.isNoteOn() || msg.isNoteOff()){
    // slightly different behaviour depending
    // on if it is using the short or long term model
    // -> short term is always training
    if (currentPoly == &shortTermMarkov) currentPoly->addMidiMessage(msg, true);
    if (currentPoly == &longTermMarkov) currentPoly->addMidiMessage(msg, trainFromInput);
    
    noteCounter ++;
  
  }
  if (noteCounter > 64)
  {
      shortTermMarkov.reset();
      noteCounter = 0;
  }

}

void PolyLeadFollow::reset()
{
  longTermMarkov.reset();
  shortTermMarkov.reset();
}

MidiBuffer PolyLeadFollow::getPendingMidiMessages()
{
  return currentPoly->getPendingMidiMessages();
}     

void PolyLeadFollow::lead()
{
  currentPoly = &longTermMarkov;
}

void PolyLeadFollow::follow()
{
  currentPoly = &shortTermMarkov;
}

void PolyLeadFollow::setQuantisationMs(double ms)
{
  shortTermMarkov.setQuantisationMs(ms);
  longTermMarkov.setQuantisationMs(ms);
}


bool PolyLeadFollow::loadModel(std::string filename)
{
  return longTermMarkov.loadModel(filename);
} 

bool PolyLeadFollow::saveModel(std::string filename) 
{
  return longTermMarkov.saveModel(filename);
}
