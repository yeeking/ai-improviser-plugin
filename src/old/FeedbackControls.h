/*
  ==============================================================================

    FeedbackControls.h
    Created: 20 Jul 2021 2:20:00pm
    Author:  matthewyk

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

enum class FeedbackEventType{positive, negative, lead, follow};

class FeedbackListener
{
  public:
    FeedbackListener(){};
    /** override this to reeive feedback*/
    virtual void feedback(FeedbackEventType fbType) = 0;
};

//===================== =========================================================
/*
*/
class FeedbackControls  : public juce::Component, public Button::Listener
{
public:
    FeedbackControls();
    ~FeedbackControls() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void addFeedbackListener(FeedbackListener* fbListener);
    void buttonClicked(Button* button) override;
private:
    TextButton posFBButton;
    TextButton negFBButton;
    TextButton leadButton;
    TextButton followButton;
    FeedbackListener* fbListener;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FeedbackControls)
};
