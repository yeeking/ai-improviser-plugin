/*
  ==============================================================================

    DinvernoRandomEnergy.cpp
    Created: 4 Nov 2019 4:47:49pm
    Author:  matthew

  ==============================================================================
*/

#include "dinvernoSystem.h"

DinvernoRandomEnergy::DinvernoRandomEnergy(int sampleRate) : DinvernoRandomMidi(sampleRate)
{
  energy = 0.0;
}

DinvernoRandomEnergy::~DinvernoRandomEnergy()
{

}

void DinvernoRandomEnergy::addMidiMessage(const MidiMessage& message, bool trainFromInput)
{
  if (message.isNoteOn()){
    std::string mgsDesc = message.getDescription().toStdString();
   // loggin->logData("RandomEnergy", "Adding midi message: " + mgsDesc);
    std::cout << "DinvernoRandomEnergy::addMidiMessage NRG: " << energy << std::endl;
    energy += 0.1;
    if (energy > 1.0) energy = 1.0;
  }
}

void DinvernoRandomEnergy::tick()
{
  double now, diff;
  now = getElapsedTimeSamples();
  diff = now - lastTick;
  timeSinceLastPlayed += diff;
  //std::cout << "elapsed " << getElapsedTimeSamples() << " waited " << timeSinceLastPlayed << " wait " << waitTimeSamples << std::endl;
  if (timeSinceLastPlayed > waitTimeSamples && 
      energy > 0)
  {
    // play some notes innit? 
    timeSinceLastPlayed = 0;
    // waitTime is the length of the sequence
    waitTimeSamples = prepareRandomNoteSequence(now, energy);
    // reduce the energy by the length of the sequence
    energy -= 0.1 ;//* (waitTimeSamples / sampleRate); // 10 seconds wipes energy completly. 
    std::cout << "DinvernoRandomEnergy::tick ernergy " << energy << std::endl;
    if (energy < 0) energy = 0;
    // add some silence after the sequence; 
    //waitTimeSamples += (random.nextDouble() * sampleRate * maxWaitBeteeenRiffs) + sampleRate;
  }
  lastTick = now;
}

double DinvernoRandomEnergy::prepareRandomNoteSequence(double startTime, double energy)
{
  double seqLengthSamples, noteEndTime;
  noteEndTime = startTime;
  double count = random.nextDouble() * energy * 10;
  std::cout << "DinvernoRandomMidi::prepareRandomNoteSequence playing " << count << " notes " << std::endl; 
    for (auto i=0;i<count;i++)
    {
      // random length and note number
        // note length is controlled by energy
        double noteLenSamples = (random.nextDouble() * sampleRate) * (1.0 - energy);
        int noteNumber = random.nextInt(64) + 32;

        uint8 vel = random.nextInt(64) + 32;
        int channel = 1;
        MidiMessage msgOn = MidiMessage::noteOn(channel, noteNumber, vel);
        MidiMessage msgOff = MidiMessage::noteOn(channel, noteNumber, (uint8) 0);
    // don't queue up too many notes
    // the parent class deals with clearing out and sending pendingMessages
      if (pendingMessages.getNumEvents() < MAX_PENDING_MESSAGES){
          pendingMessages.addEvent(msgOn, noteEndTime);
          pendingMessages.addEvent(msgOff, noteEndTime + noteLenSamples);
      }
      noteEndTime += noteLenSamples;
    }
    // final start time
    seqLengthSamples = noteEndTime - startTime;
    return seqLengthSamples;
}
