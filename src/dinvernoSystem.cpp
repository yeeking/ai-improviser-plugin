/*
  ==============================================================================

    dinvernoSystem.cpp
    Created: 30 Oct 2019 4:45:11pm
    Author:  matthew
A fair bit of the MidiBuffer handling code hacked from here: 
  https://docs.juce.com/master/tutorial_midi_message.html#tutorial_midi_message_midi_buffer
  ==============================================================================
*/

#include "dinvernoSystem.h"
#include "iostream"
#include <fstream>
#include <stdio.h>

// void DinvernoImproviser::setLogginManager(LogginManager* loggin)
// {
//   readyToLog = true;
//   this->loggin = loggin;
// }

bool DinvernoImproviser::isReadyToLog()
{
  return readyToLog;
}

double DinvernoImproviser::getElapsedTimeSamples()
{
  double nowSamples = Time::getMillisecondCounterHiRes() * 0.001 * sampleRate;
  double elapsedSamples = nowSamples - startTimeSamples;
  return elapsedSamples;
}


// this function
// finds messages with a time stamp older than 'now'
// and returns them
// call this to ask the parrot for messages that are due to be sent.
// a fair bit of this code hacked from here:
// based on https://docs.juce.com/master/tutorial_midi_message.html#tutorial_midi_message_midi_buffer
MidiBuffer DinvernoImproviser::getPendingMidiMessages()
{
    MidiBuffer messagesToSend;
    if (pendingMessages.getNumEvents() == 0) return messagesToSend;
    auto currentSampleNumber = (Time::getMillisecondCounterHiRes() * 0.001 * sampleRate) - startTimeSamples;
    //getElapsedTimeSamples();
    MidiMessage message;

    // identify the messages we want to send and add them
    // to the secondary buffer

    int sampleNumber;
    int oldest = 0;

    for (const auto meta : pendingMessages)
     {
      const auto msg = meta.getMessage();
      sampleNumber = meta.samplePosition;
      if (sampleNumber > currentSampleNumber)  break; // we are in the future. STOP!
      // remember the oldest one we processed
      if (oldest == 0) oldest = sampleNumber;
      messagesToSend.addEvent (msg, meta.samplePosition);
    }

//    MidiBuffer::Iterator iterator (pendingMessages);
    // while (iterator.getNextEvent (message, sampleNumber)) 
    // {
    //   // sampleNumber is the sampleNumber assigned to this message.
    //   // is sample Number in the future? 
    //     if (sampleNumber > currentSampleNumber)  // we are in the future. STOP!
    //         break;
    //   // remember the oldest one we processed
    //     if (oldest == 0) oldest = sampleNumber;
    //     //std::cout << "parrot adding ts: " << message.getTimeStamp() << std::endl; 
    //     messagesToSend.addEvent(message, sampleNumber);
    // }
    //std::cout << "DinvernoImproviser::getPendingMidiMessages sending " << messagesToSend.getNumEvents() << " events " << std::endl;
    // now claer any sent messages from pendingMessages
    // oldst is the oldest one we processed,currentSampleNumber
    // is the newest one
    // https://docs.juce.com/master/classMidiBuffer.html#aacd8382869c865bb8d15c0cfffe9dff1
    pendingMessages.clear (oldest, (currentSampleNumber - oldest) + 1); // [8]
    return messagesToSend;
}


DinvernoMidiParrot::DinvernoMidiParrot(int sampleRate) : DinvernoImproviser(sampleRate)
{
}
DinvernoMidiParrot::~DinvernoMidiParrot()
{
}
void DinvernoMidiParrot::tick()
{
}
void DinvernoMidiParrot::reset()
{
    std::cout << "DinvernoMidiParrot::reset" << std::endl;
    pendingMessages.clear();
    //loggin->logData("MidiParrot", "Reset applied");
}

// call this to tell the parrot about a message
// a fair bit of this code hacked from here:
// based on https://docs.juce.com/master/tutorial_midi_message.html#tutorial_midi_message_midi_buffer
void DinvernoMidiParrot::addMidiMessage(const MidiMessage& message, bool trainFromInput)
{
  // assume timestamp is 'now' + 1 - so it'll play this
  // back in 1 second's time
    auto sampleNumber =  getElapsedTimeSamples() + sampleRate; // 1 second late
    if (message.isNoteOn() || message.isNoteOff())
    {
      pendingMessages.addEvent(message, sampleNumber);
      std::string mgsDesc = message.getDescription().toStdString();
      // myk: just commenting this as
      // it generates a lot of annotations! 
      //loggin->logData("MidiParrot", "Adding midi message: " + mgsDesc);
    }
}

///////////////////
///////// level 2: the mono markov
///////////////////

DinvernoMonoMarkov::DinvernoMonoMarkov(int sampleRate) : DinvernoImproviser(sampleRate)
{
  lastTickSamples = 0;
  accumTimeDelta = 0;
  timeSinceLastNote = 0;
}
DinvernoMonoMarkov::~DinvernoMonoMarkov()
{

}

// this will be called periodically
// this is where do our generation
// which results in midi notes being added
// to the pendingevents list
void DinvernoMonoMarkov::tick()
{
  // for now, just play a note every second
  // with pitch and length from the length model.
  double nowSamples = (Time::getMillisecondCounterHiRes() * 0.001 * sampleRate);
  double tDelta = nowSamples - lastTickSamples;
  accumTimeDelta += tDelta;

  if (accumTimeDelta > timeSinceLastNote) // a second has passed... do something
  {
    // reset 
    accumTimeDelta = 0;
    // in this case, 
    // request a note 1 second from now
    int note = std::stoi(pitchModel.getEvent());
    int len = std::stoi(lengthModel.getEvent());
    MidiMessage noteOn = MidiMessage::noteOn(1, note, 0.5f);
    MidiMessage noteOff = MidiMessage::noteOff(1, note, 0.0f);

    if (note > 0 && len > 0 )
    {
      auto sampleNumber =  getElapsedTimeSamples() + sampleRate; // 1 second late
      if (pendingMessages.getNumEvents() < MAX_PENDING_MESSAGES){

        pendingMessages.addEvent(noteOn, sampleNumber);
        // length should be dictated by the length 
        pendingMessages.addEvent(noteOff, sampleNumber + len);
      }
      timeSinceLastNote = len; // we'll use this as a wait time
    }
  }
  lastTickSamples = nowSamples;
}

void DinvernoMonoMarkov::addMidiMessage(const MidiMessage& message, bool trainFromInput)
{
  if (message.isNoteOn()){
    addNoteOnToModel(message.getNoteNumber(), message.getVelocity());
    std::string mgsDesc = message.getDescription().toStdString();
    //loggin->logData("MonoMarkov", "Adding midi message: " + mgsDesc);
  }
  if (message.isNoteOff()){
    addNoteOffToModel(message.getNoteNumber());
    std::string mgsDesc = message.getDescription().toStdString();
    //loggin->logData("MonoMarkov", "Adding midi message off model: " + mgsDesc);
  }
}

void DinvernoMonoMarkov::reset()
{
  std::cout << "DinvernoMonoMarkov::reset" << std::endl;
    pendingMessages.clear();
    pitchModel.reset();
    lengthModel.reset();
    //loggin->logData("MonoMarkov", "Reset applied");

}
void DinvernoMonoMarkov::addNoteOnToModel(int note, int velocity)
{
    // remember when this note started so we can measure length later
    double elapsedSamples  = (Time::getMillisecondCounterHiRes() * 0.001 * sampleRate) - startTimeSamples;
    noteOnTimesSamples[note] = elapsedSamples;
    pitchModel.putEvent(std::to_string(note));
}
void DinvernoMonoMarkov::addNoteOffToModel(int note)
{
    double noteStart = getNoteOnTimeSamples(note);
    int elapsedSamples  = (Time::getMillisecondCounterHiRes() * 0.001 * sampleRate) - startTimeSamples;
    int noteLen = elapsedSamples - noteStart;
    lengthModel.putEvent(std::to_string(noteLen));
}

double DinvernoMonoMarkov::getNoteOnTimeSamples(int note)
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

