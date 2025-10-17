#pragma once 

#include <juce_audio_processors/juce_audio_processors.h>

/** This class is used to keep track of note on and off
 * events to allow detection of held notes and other issues
 */
class MIDIMonitor{
    public:
    /** create a midi monitor which will look for stuck notes defined as held for longer than maxHoldTimeSecs*/
        MIDIMonitor(float sampleRate, float maxHoldTimeSecs = 5.0f);
        /** tell the monitor the sample rate changed from constructor value */
        void setSampleRate(float sampleRate);
        /** tell the monitor about a midi event */
        void eventWasAddedToBuffer(juce::MidiMessage& msg, unsigned long elapsedSamples);
        /** return the list of currently stuck notes, i.e. notes where note on was sent 
         * but no note off after maxHoldTimeSecs
         */
        std::vector<int> getStuckNotes(unsigned long elapsedTimeSamples);
        /** tell the monitor that we 'unstuck' a note so it doesn't keep telling us about it */
        void unstickNote(int note);
    private:
        float maxHoldTimeSecs;
        unsigned long maxHoldTimeSamples;
        unsigned long noteOnTimes[127];
};