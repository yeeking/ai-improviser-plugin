#include "MIDIMonitor.h"

MIDIMonitor::MIDIMonitor(float sampleRate, float maxHoldTimeSecs) 
: maxHoldTimeSecs{maxHoldTimeSecs}, 
  maxHoldTimeSamples{0} // we'll work this out shortly...
{
    setSampleRate(sampleRate);

}

void MIDIMonitor::setSampleRate(float sampleRate)
{
    maxHoldTimeSamples = static_cast<unsigned long> (maxHoldTimeSecs * sampleRate); 
    for (int i=0;i<127;++i){
        noteOnTimes[i] = 0;
    }
}


void MIDIMonitor::eventWasAddedToBuffer(juce::MidiMessage& msg, unsigned long elapsedSamples)
{
    if (msg.isNoteOn()){
        noteOnTimes[msg.getNoteNumber()] = elapsedSamples;
    }
    if (msg.isNoteOff()){
        unsigned long noteLen = elapsedSamples - noteOnTimes[msg.getNoteNumber()]; 
        // DBG("Note " << msg.getNoteNumber() << " off ; dur " << noteLen << " allowed " << maxHoldTimeSamples);
        noteOnTimes[msg.getNoteNumber()] = 0; 
    }
    
}

std::vector<int> MIDIMonitor::getStuckNotes(unsigned long elapsedTimeSamples)
{
    std::vector<int> stuckNotes{};
    for (int note=0;note < 127; ++note){
        if (noteOnTimes[note] != 0){// held note
            // hold time
            unsigned long holdTime = elapsedTimeSamples - noteOnTimes[note];
            if (holdTime > maxHoldTimeSamples){// held too long
                stuckNotes.push_back(note);
            }
        }
    }
    return stuckNotes;
}
        
void MIDIMonitor::unstickNote(int note)
{
    noteOnTimes[note] = 0;    
}

