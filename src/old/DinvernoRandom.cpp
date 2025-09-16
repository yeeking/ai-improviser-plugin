/*
  ==============================================================================

    DinvernoRandom.cpp
    Created: 4 Nov 2019 3:50:05pm
    Author:  matthew

  ==============================================================================
*/

#include "dinvernoSystem.h"

DinvernoRandomMidi::DinvernoRandomMidi(int sampleRate) : DinvernoImproviser(sampleRate)
{
  waitTimeSamples = random.nextDouble() * sampleRate;
  timeSinceLastPlayed = 0;
  maxWaitBeteeenRiffs = 5;
}
DinvernoRandomMidi::~DinvernoRandomMidi()
{
}
void DinvernoRandomMidi::tick()
{
  double now, diff;
  now = getElapsedTimeSamples();
  diff = now - lastTick;
  timeSinceLastPlayed += diff;
  //std::cout << "elapsed " << getElapsedTimeSamples() << " waited " << timeSinceLastPlayed << " wait " << waitTimeSamples << std::endl;
  if (timeSinceLastPlayed > waitTimeSamples)
  {
    // play some notes innit? 
    timeSinceLastPlayed = 0;
    // waitTime is the length of the sequence
    waitTimeSamples = prepareRandomNoteSequence(now);
    // add some silence after the sequence; 
    waitTimeSamples += (random.nextDouble() * sampleRate * maxWaitBeteeenRiffs) + sampleRate;
  }
  lastTick = now;
}

void DinvernoRandomMidi::reset()
{
  std::cout << "DinvernoRandomMidi::reset" << std::endl;
    pendingMessages.clear();
  //loggin->logData("RandomMidi", "Reset applied.");
}

double DinvernoRandomMidi::prepareRandomNoteSequence(double startTime)
{
  double seqLengthSamples, noteEndTime;
  noteEndTime = startTime;
  int count = random.nextInt(5);
  std::cout << "DinvernoRandomMidi::prepareRandomNoteSequence playing " << count << " notes " << std::endl; 
  //loggin->logData("RandomMidi", "Playing " + std::to_string(count) + " notes ");
    for (auto i=0;i<count;i++)
    {
      // random length and note number
        double noteLenSamples = random.nextDouble() * sampleRate;
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

// don't respond to incoming midi
void DinvernoRandomMidi::addMidiMessage(const MidiMessage& message, bool trainFromInput)
{

}
