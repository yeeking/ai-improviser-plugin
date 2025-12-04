#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <deque>

class ThrobbingOrderCircle : public juce::Component,
                             private juce::Timer
{
public:
    ThrobbingOrderCircle();
    void setOrder(int order);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    float currentOrder { 0.0f };
    float targetOrder { 0.0f };
    float maxOrderSeen { 1.0f };
    float phase { 0.0f };
    std::deque<float> history;
};

class CallResponseMeter : public juce::Component,
                          private juce::Timer
{
public:
    CallResponseMeter();
    void setEnergy(float energy01);
    void setState(bool enabled, bool inResponse);
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    float currentEnergy { 0.0f };
    float targetEnergy { 0.0f };
    bool enabled { false };
    bool inResponse { false };
    float pulsePhase { 0.0f };
};
