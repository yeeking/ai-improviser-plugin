/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AimusoAudioProcessorEditor::AimusoAudioProcessorEditor (AimusoAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);
    setResizable(true, true);
    
    setupUI();
    startTimer(250);
    //DBG("PluginEditor::cons done.");
    
}
void AimusoAudioProcessorEditor::setupUI()
{
    // model load and save controls
    addAndMakeVisible(loadModelBtn);
    loadModelBtn.setButtonText("Load model");
    loadModelBtn.addListener(this);
    
    addAndMakeVisible(saveModelBtn);
    saveModelBtn.setButtonText("Save model");
    saveModelBtn.addListener(this);

    addAndMakeVisible(currentModelLabel);


    addAndMakeVisible(trainToggle);
    trainToggle.setButtonText("AI is learning");
    trainToggle.setColour(juce::TextButton::ColourIds::buttonColourId, Colours::green);
    trainToggle.addListener(this);

    addAndMakeVisible(aiPlayingToggle);
    aiPlayingToggle.setButtonText("AI is playing");
    aiPlayingToggle.setColour(juce::TextButton::ColourIds::buttonColourId, Colours::green);
    aiPlayingToggle.addListener(this);


   // addAndMakeVisible(trainModeLabel);

    // midi channel select controls
    addAndMakeVisible(midiInSelector);
    addAndMakeVisible(midiInLabel);
    midiInLabel.setText("MIDI IN: ", juce::NotificationType::dontSendNotification);
    midiInSelector.setSliderStyle(Slider::IncDecButtons);
    //midiInSelector.setTextBoxStyle (Slider::TextBoxLeft, false, 50, 20);
    midiInSelector.setIncDecButtonsMode (Slider::incDecButtonsDraggable_Vertical);
    
    midiInSelector.setRange(0, 16, 1);
    midiInSelector.addListener(this);
    // midi out 1-16
    addAndMakeVisible(midiOutSelector);
    addAndMakeVisible(midiOutLabel);
    midiOutSelector.setSliderStyle(Slider::IncDecButtons);
   // midiOutSelector.setTextBoxStyle (Slider::TextBoxLeft, false, 50, 20);
    midiOutSelector.setIncDecButtonsMode (Slider::incDecButtonsDraggable_Vertical);
    midiOutLabel.setText("MIDI OUT: ", juce::NotificationType::dontSendNotification);
    midiOutSelector.setRange(1, 16, 1);
    midiOutSelector.addListener(this);
                            

    // quantise
    addAndMakeVisible(quantiseLabel);
    quantiseLabel.setText("QUANT", juce::NotificationType::dontSendNotification);
    addAndMakeVisible(quantiseSelector);
    quantiseSelector.setRange(0, 250, 1);
    quantiseSelector.setTextValueSuffix("ms");
    quantiseSelector.addListener(this);
    quantiseSelector.setValue(50);// start with a 25ms quant.
    
    // probability override slider
    // cc select
    addAndMakeVisible(playProbCCLabel);
    addAndMakeVisible(playProbCCSelect);
    playProbCCLabel.setText("PROB CC", juce::NotificationType::dontSendNotification);
    playProbCCSelect.setSliderStyle(Slider::IncDecButtons);
   // playProbCCSelect.setTextBoxStyle (Slider::TextBoxLeft, false, 50, 20);
    playProbCCSelect.setIncDecButtonsMode (Slider::incDecButtonsDraggable_Vertical);
    playProbCCSelect.setRange(1, 127, 1);
    playProbCCSelect.addListener(this);
    
    // prob setter
    addAndMakeVisible(playProbLabel);
    playProbLabel.setText("PROB", juce::NotificationType::dontSendNotification);
    addAndMakeVisible(playProbSlider);
    playProbSlider.setRange(0, 1);
    playProbSlider.addListener(this);
    playProbSlider.setValue(1);
    
    
    // group for mode buttons
    //addAndMakeVisible(modeBox);
    addAndMakeVisible(leadModeBtn);
    leadModeBtn.setButtonText("Algo lead");
    leadModeBtn.addListener(this);
    leadModeBtn.setColour(juce::TextButton::ColourIds::buttonColourId, Colours::green);
    // make sure the mode matches the highlighted button
    audioProcessor.leadMode(); 

    // addAndMakeVisible(interactModeBtn);
    // interactModeBtn.setButtonText("Algo interact");
    // interactModeBtn.addListener(this);

    addAndMakeVisible(followModeBtn);
    followModeBtn.setButtonText("Algo follow");
    followModeBtn.addListener(this);

    addAndMakeVisible(resetModelBtn);
    resetModelBtn.setButtonText("Reset model");
    resetModelBtn.addListener(this);
}

AimusoAudioProcessorEditor::~AimusoAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void AimusoAudioProcessorEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white);
    g.setFont (15.0f);
    //g.drawFittedText ("Hello World!", getLocalBounds(), juce::Justification::centred, 1);

}

void AimusoAudioProcessorEditor::resized()
{
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
    int colCount = 4;
    int colWidth = getWidth() / colCount;
    int rowCount = 7;
    int rowHeight = getHeight() / rowCount;
    int xPos = 0;
    int yPos = 0;
    
    // model load and save controls
    // [load][save][current model]
    trainToggle.setBounds(xPos, yPos, colWidth, rowHeight);
    xPos += colWidth;
    aiPlayingToggle.setBounds(xPos, yPos, colWidth, rowHeight);
    xPos += colWidth;
    loadModelBtn.setBounds(xPos, yPos, colWidth, rowHeight);
    xPos += colWidth;
    saveModelBtn.setBounds(xPos, yPos, colWidth, rowHeight);
    // midi channel select controls
    // [midi in 15 [-----]]
    xPos = 0;
    yPos += rowHeight;
    midiInLabel.setBounds(xPos, yPos, colWidth, rowHeight);
    xPos += colWidth;
    midiInSelector.setBounds(xPos, yPos, colWidth*3, rowHeight);
    // [midi out 1 [----]]
    xPos = 0;
    yPos += rowHeight;
    midiOutLabel.setBounds(xPos, yPos, colWidth, rowHeight);
    xPos += colWidth;
    midiOutSelector.setBounds(xPos, yPos, colWidth*3, rowHeight);
   

    // quantise
    xPos = 0;
    yPos += rowHeight;
    quantiseLabel.setBounds(xPos, yPos, colWidth, rowHeight);
    xPos += colWidth;
    quantiseSelector.setBounds(xPos, yPos, colWidth*3, rowHeight);
       
    // playback prob
    xPos = 0;
    yPos += rowHeight;
    playProbCCLabel.setBounds(xPos, yPos, colWidth, rowHeight);
    xPos += colWidth;
    playProbCCSelect.setBounds(xPos, yPos, colWidth*3, rowHeight);
    
    xPos = 0;
    yPos += rowHeight;
    playProbLabel.setBounds(xPos, yPos, colWidth, rowHeight);
    xPos += colWidth;
    playProbSlider.setBounds(xPos, yPos, colWidth*3, rowHeight);
    
       
    // group for mode buttons
    //modeBox
    xPos = 0;
    yPos += rowHeight;
    leadModeBtn.setBounds(xPos, yPos, colWidth, rowHeight);
    xPos += colWidth;
    followModeBtn.setBounds(xPos, yPos, colWidth, rowHeight);
    xPos += colWidth;
    interactModeBtn.setBounds(xPos, yPos, colWidth, rowHeight);
    
    xPos += colWidth;
    resetModelBtn.setBounds(xPos, yPos, colWidth, rowHeight);
    
}


void AimusoAudioProcessorEditor::sliderValueChanged(Slider* slider)
{
    if (slider == &this->quantiseSelector)
        audioProcessor.setQuantisationMs(slider->getValue());
    if (slider == &this->midiInSelector)
        audioProcessor.setMidiInChannel(slider->getValue());
    if (slider == &this->midiOutSelector)
        audioProcessor.setMidiOutChannel(slider->getValue());
    if (slider == &this->playProbSlider){
        //DBG("play prob " << slider->getValue());
        this->audioProcessor.setPlayProb(slider->getValue());
    }
    if (slider == &this->playProbCCSelect){
        //DBG("play prob cc " << slider->getValue());
        this->audioProcessor.setPlayProbCC(slider->getValue());
    }
}


void AimusoAudioProcessorEditor::buttonClicked(Button* btn)
{
    //juce::Colour bg = getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId);
    //interactModeBtn.setColour(juce::TextButton::ColourIds::buttonColourId, Colours::darkgrey);
    

    if (btn == &this->followModeBtn){
        // reset mode buttons
        leadModeBtn.setColour(juce::TextButton::ColourIds::buttonColourId, Colours::darkgrey);   
        // highlight this button
        followModeBtn.setColour(juce::TextButton::ColourIds::buttonColourId, Colours::green);
        audioProcessor.followMode();
    }
    if (btn == &this->leadModeBtn){
        // reset mode buttons
        followModeBtn.setColour(juce::TextButton::ColourIds::buttonColourId, Colours::darkgrey);
        // highlight correct button
        leadModeBtn.setColour(juce::TextButton::ColourIds::buttonColourId, Colours::green);
        audioProcessor.leadMode(); 
    } 
    if (btn == &this->resetModelBtn){
        audioProcessor.resetModels();
        followModeBtn.setColour(juce::TextButton::ColourIds::buttonColourId, Colours::darkgrey);
        leadModeBtn.setColour(juce::TextButton::ColourIds::buttonColourId, Colours::green);
        audioProcessor.leadMode(); 
    }
    if (btn == &this->trainToggle){
        if (audioProcessor.isTraining()){
            AimusoAudioProcessorEditor::setButtonMsgAndColour(trainToggle, "AI is not learning", Colours::darkgrey);
            audioProcessor.disableTraining();
        }
        else {
            AimusoAudioProcessorEditor::setButtonMsgAndColour(trainToggle, "AI is learning", Colours::green);
            audioProcessor.enableTraining();
        }     
    }
     if (btn == &this->aiPlayingToggle){
        if (audioProcessor.isPlaying()){
            AimusoAudioProcessorEditor::setButtonMsgAndColour(aiPlayingToggle, "AI is not playing", Colours::darkgrey);
            audioProcessor.disablePlaying();
        }
        else {
            AimusoAudioProcessorEditor::setButtonMsgAndColour(aiPlayingToggle, "AI is playing", Colours::green);
            audioProcessor.enablePlaying();
        }     
    }



    if (btn == &this->loadModelBtn){
    
        auto fileChooserFlags = 
        FileBrowserComponent::canSelectFiles;
        fChooser.launchAsync(fileChooserFlags, [this](const FileChooser& chooser)
        {
            File chosenFile = chooser.getResult();
            if (chosenFile.exists()){
                std::cout << "AimusoAudioProcessorEditor::buttonClicked loading model file " << chosenFile.getFullPathName() << std::endl;
                bool res = audioProcessor.loadModel(chosenFile.getFullPathName().toStdString());
                if (!res){
                    AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon, 
                        "Whoops", 
                        "Could not load model sorry", 
                        "OK");                 	
                }
                else {
                    AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::InfoIcon, 
                        "", 
                        "Model file loaded", 
                        "OK");   

                }
            }
            // here is my code to handle chosen files data
        });
        //FileBrowserComponent::openMode | FileBrowserComponent::canSelectDirectories;

    }
    if (btn == &this->saveModelBtn){
           auto fileChooserFlags = 
        FileBrowserComponent::saveMode;
        fChooser.launchAsync(fileChooserFlags, [this](const FileChooser& chooser)
        {
            File chosenFile = chooser.getResult();
            std::cout << "AimusoAudioProcessorEditor::buttonClicked saving model file " << chosenFile.getFullPathName() << std::endl;
            bool res = audioProcessor.saveModel(chosenFile.getFullPathName().toStdString());            
              if (!res){
                    AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::WarningIcon, 
                        "Whoops", 
                        "Could not save model sorry", 
                        "OK");                 	
                }
                else {
                    AlertWindow::showMessageBoxAsync(
                        juce::MessageBoxIconType::InfoIcon, 
                        "", 
                        "Model file saved", 
                        "OK");   

                }
        });
    }


}

void AimusoAudioProcessorEditor::setButtonMsgAndColour(TextButton& btn, String msg, Colour col)
{
    btn.setColour(juce::TextButton::ColourIds::buttonColourId, col);
    btn.setButtonText(msg);
}




void AimusoAudioProcessorEditor::timerCallback()
{
    // check we are in sync with processor state
    //DBG("UI checking if prob changed... procs: " << audioProcessor.getPlayProb() << " mine " << playProbSlider.getValue() );
    if (audioProcessor.getPlayProb() != playProbSlider.getValue()){
        //DBG("UI: prob changed" );
        playProbSlider.setValue(audioProcessor.getPlayProb());
    }
}


