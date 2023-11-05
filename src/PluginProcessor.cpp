/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
AimusoAudioProcessor::AimusoAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       )
#endif
{
    // calculate 
    // threadedImprovisor.setImproviser(currentImproviser);
    // threadedImprovisor.startThread();
    // // call tick on the improviser every 'n'ms
    updateTicker.setImproviser(this->currentImproviser);
    generateTicker.setImproviser(this->currentImproviser);
    updateTicker.startTimer(100);
    generateTicker.startTimer(20);
}

AimusoAudioProcessor::~AimusoAudioProcessor()
{
    //threadedImprovisor.stopThread(30);
    updateTicker.stopTimer();
    generateTicker.stopTimer();
}

//==============================================================================
const juce::String AimusoAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool AimusoAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool AimusoAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool AimusoAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double AimusoAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int AimusoAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int AimusoAudioProcessor::getCurrentProgram()
{
    return 0;
}

void AimusoAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String AimusoAudioProcessor::getProgramName (int index)
{
    return {};
}

void AimusoAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void AimusoAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
}

void AimusoAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool AimusoAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
   #endif

    return true;
  #endif
}
#endif

void AimusoAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{   

    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    
    // passing incoming midi messages
    // to the improviser
    for (const auto meta : midiMessages){
        auto msg = meta.getMessage();
        if (msg.isController()){
            this->handleCC(msg);
        }
        if (midiInChannel == 0 || msg.getChannel() == midiInChannel){
        
            // pass iAmTraining to tell it if it should
            // learn from the inputs as well as responding
            //DBG("AimusoAudioProcessor::processBlock adding midi messages to impro");

            currentImproviser->addMidiMessage(msg, iAmTraining);
            //DBG("AimusoAudioProcessor::processBlock done adding midi messages to impro");

        }
    }
        
    // Get Midi Messages from Improvisor: add to buffer if it is time to send
    int sampleNumber;
    //currentImproviser->tick();
    //DBG("AimusoAudioProcessor::processBlock getting midi messages from impro");
    juce::MidiBuffer toSend;
    // note always pull notes from AI - that
    // means it does not get clogged up :) 
    toSend = currentImproviser->getPendingMidiMessages();

    juce::MidiBuffer generatedMidi{};
    // apply 'play' mode filters on
    // here
    if (iAmPlaying &&
        toSend.getNumEvents() > 0 &&
        playbackProb > 0 &&
        rng.nextDouble() < playbackProb){
      //  DBG("ai plays " << playbackProb);

        // only add them if prob high enough
        
        for (const auto meta : toSend){
            auto msg = meta.getMessage();
            msg.setTimeStamp(juce::Time::getApproximateMillisecondCounter() * 0.001);
            msg.setChannel(midiOutChannel);
            generatedMidi.addEvent(msg, 0);
        }
    }
    
    if (clearMidiBuffer) {
        for (auto ch = 1; ch < 17; ++ch){
            juce::MidiMessage allOff = juce::MidiMessage::allNotesOff(ch);
            generatedMidi.addEvent(allOff,0);

        }
        //midiMessages.addEvent(allOff,0);
        clearMidiBuffer = false;
    }
    
    // Remove Raw midi input and only transmit dinverno generated messages
    midiMessages.swapWith(generatedMidi); 
}


//==============================================================================
bool AimusoAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* AimusoAudioProcessor::createEditor()
{
    return new AimusoAudioProcessorEditor (*this);
}

//==============================================================================
void AimusoAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void AimusoAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
}


void AimusoAudioProcessor::leadMode()
{
    clearMidiBuffer = true;
    polyLeadFollow.lead();
}

void AimusoAudioProcessor::followMode()
{
    clearMidiBuffer = true;
    polyLeadFollow.follow();
}

void AimusoAudioProcessor::resetModels()
{
    clearMidiBuffer = true;
    polyLeadFollow.reset();
}

void AimusoAudioProcessor::setQuantisationMs(double ms)
{
    if (ms < 0) return;
    polyLeadFollow.setQuantisationMs(ms);
}


void AimusoAudioProcessor::setMidiInChannel(int ch)
{
    clearMidiBuffer = true;
    // if in is zero, listen to all channels
    if (ch < 0 || ch > 16) return; 
    midiInChannel = ch;
}

void AimusoAudioProcessor::setMidiOutChannel(int ch)
{
    clearMidiBuffer = true;
    // out in range 1-16
    if (ch < 1 || ch > 16) return; 
    midiOutChannel = ch;
}


bool AimusoAudioProcessor::isTraining()
{
    return iAmTraining;
}
void AimusoAudioProcessor::enableTraining()
{
    clearMidiBuffer = true;
    iAmTraining = true;
}
void AimusoAudioProcessor::disableTraining()
{
    clearMidiBuffer = true;
    iAmTraining = false; 
}

bool AimusoAudioProcessor::isPlaying()
{
    return iAmPlaying;
}
void AimusoAudioProcessor::enablePlaying()
{
    iAmPlaying = true;
}
void AimusoAudioProcessor::disablePlaying()
{
    iAmPlaying = false; 
}


bool AimusoAudioProcessor::loadModel(std::string filename)
{
    return this->polyLeadFollow.loadModel(filename);
}

bool AimusoAudioProcessor::saveModel(std::string filename)
{
    return this->polyLeadFollow.saveModel(filename);
}


//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new AimusoAudioProcessor();
}




void AimusoAudioProcessor::setPlayProb(double _playProb)
{
    if (_playProb >= 0 && _playProb <= 1){
        this->playbackProb = _playProb;
    }
}

void  AimusoAudioProcessor::setPlayProbCC(int ccNum)
{
    if (ccNum >= 0 && ccNum < 127){
        this->playbackProbCC = ccNum;
    }
}

double AimusoAudioProcessor::getPlayProb()
{
    return this->playbackProb;
}

void AimusoAudioProcessor::handleCC(MidiMessage& ccMsg)
{
    
    //DBG("got cc " << ccMsg.getControllerNumber() << " : " << ccMsg.getControllerValue());
    // now update the playback prob if needed
    if (ccMsg.getControllerNumber() == this->playbackProbCC){
        this->playbackProb = ccMsg.getControllerValue() / 127.0;
    }
}


