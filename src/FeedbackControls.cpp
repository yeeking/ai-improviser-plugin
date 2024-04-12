/*
  ==============================================================================

    FeedbackControls.cpp
    Created: 20 Jul 2021 2:20:00pm
    Author:  matthewyk

  ==============================================================================
*/

#include <JuceHeader.h>
#include "FeedbackControls.h"

//==============================================================================
FeedbackControls::FeedbackControls()
{
    // In your constructor, you should add any child components, and
    // initialise any special settings that your component needs.
    addAndMakeVisible(posFBButton);
    addAndMakeVisible(negFBButton);
    addAndMakeVisible(leadButton);
    addAndMakeVisible(followButton);

    posFBButton.setButtonText("POS");
    negFBButton.setButtonText("NEG");
    leadButton.setButtonText("LEAD");
    followButton.setButtonText("FOLL");

    posFBButton.addListener(this);
    negFBButton.addListener(this);
    leadButton.addListener(this);
    followButton.addListener(this);


}

FeedbackControls::~FeedbackControls()
{
}

void FeedbackControls::paint (juce::Graphics& g)
{
    /* This demo code just fills the component's background and
       draws some placeholder text to get you started.

       You should replace everything in this method with your own
       drawing code..
    */

    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));   // clear the background

    g.setColour (juce::Colours::grey);
    g.drawRect (getLocalBounds(), 1);   // draw an outline around the component

    g.setColour (juce::Colours::white);
    g.setFont (14.0f);
    g.drawText ("FeedbackControls", getLocalBounds(),
                juce::Justification::centred, true);   // draw some placeholder text
}

void FeedbackControls::resized()
{
    // This method is where you should set the bounds of any child
    // components that your component contains..
    int col = getWidth()/2;
    int row = getHeight()/2;
    int xPos = 0;
    int yPos = 0;
    posFBButton.setBounds(xPos, yPos, col, row);
    xPos += col;
    negFBButton.setBounds(xPos, yPos, col, row);
    xPos = 0;
    yPos += row; 
    leadButton.setBounds(xPos, yPos, col, row);
    xPos += col;
    followButton.setBounds(xPos, yPos, col, row);
}

void FeedbackControls::addFeedbackListener(FeedbackListener* fbListener)
{
  this->fbListener = fbListener;
}

void FeedbackControls::buttonClicked(Button* button)
{
  if (&negFBButton == button)
  {
    fbListener->feedback(FeedbackEventType::negative);
  }
  if (&posFBButton == button)
  {
    fbListener->feedback(FeedbackEventType::positive);
  }
  if (&followButton == button)
  {
    fbListener->feedback(FeedbackEventType::follow);
  }
  if (&leadButton == button)
  {
    fbListener->feedback(FeedbackEventType::lead);
  }
}