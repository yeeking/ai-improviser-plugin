#pragma once

#include <JuceHeader.h>
#include <atomic>

class NoteIndicatorComponent : public juce::Component,
                               private juce::Timer
{
public:
    NoteIndicatorComponent();
    ~NoteIndicatorComponent() override;

    // Set the current note (0..127) and brightness based on velocity (0..1).
    // Thread-safe: if called off the message thread it will marshal to it.
    void setNote(int noteNumber, float velocity01);

    // Animation / decay tuning
    void setFrameRateHz(int hz);
    void setDecaySeconds(float seconds);
    void setRedrawThreshold(float threshold01);

    // JUCE
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    // Timer
    void timerCallback() override;
    void updateFrameTimer();

    // Internal setter (message-thread only)
    void setNoteOnMessageThread(int noteNumber, float velocity01);

    // State
    std::atomic<float> brightness { 0.0f };   // 0..1, decays over time
    std::atomic<int>   lastNote   { -1 };     // 0..127, or -1 if none

    int   frameRateHz   = 30;      // animation fps
    float decaySeconds  = 0.4f;    // time from 1.0 -> 0.0
    float redrawThresh  = 0.02f;   // repaint while above this

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NoteIndicatorComponent)
};
