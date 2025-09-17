/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
MidiMarkovProcessor::MidiMarkovProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                         )
#endif
      ,
      pitchModel{}, iOIModel{}, lastNoteOnTime{0}, elapsedSamples{0}, modelPlayNoteTime{0}, noMidiYet{true}
{
  // set all note off times to zero 

  for (auto i=0;i<127;++i){
    noteOffTimes[i] = 0;
    noteOnTimes[i] = 0;
}
}

MidiMarkovProcessor::~MidiMarkovProcessor()
{
}

//==============================================================================
const juce::String MidiMarkovProcessor::getName() const
{
  return JucePlugin_Name;
}

bool MidiMarkovProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
  return true;
#else
  return false;
#endif
}

bool MidiMarkovProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
  return true;
#else
  return false;
#endif
}

bool MidiMarkovProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
  return true;
#else
  return false;
#endif
}

double MidiMarkovProcessor::getTailLengthSeconds() const
{
  return 0.0;
}

int MidiMarkovProcessor::getNumPrograms()
{
  return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
            // so this should be at least 1, even if you're not really implementing programs.
}

int MidiMarkovProcessor::getCurrentProgram()
{
  return 0;
}

void MidiMarkovProcessor::setCurrentProgram(int index)
{
}

const juce::String MidiMarkovProcessor::getProgramName(int index)
{
  return {};
}

void MidiMarkovProcessor::changeProgramName(int index, const juce::String &newName)
{
}

//==============================================================================
void MidiMarkovProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
}

void MidiMarkovProcessor::releaseResources()
{
  // When playback stops, you can use this as an opportunity to free up any
  // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool MidiMarkovProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
  juce::ignoreUnused(layouts);
  return true;
#else
  // This is the place where you check if the layout is supported.
  // In this template code we only support mono or stereo.
  // Some plugin hosts, such as certain GarageBand versions, will only
  // load plugins that support stereo bus layouts.
  if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
    return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
  if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
    return false;
#endif

  return true;
#endif
}
#endif

// called from external sources to store midi 
void MidiMarkovProcessor::addMidi(const juce::MidiMessage& msg, int sampleOffset)
{
  // might not be thread safe whoops - should probaably lock
  // midiToProcess before adding things to it 
  DBG("addMidi called ");
    midiToProcess.addEvent(msg, sampleOffset);  // keep your existing logic
    // Notify UI via mailbox only — do NOT touch the editor from here.
    pushMIDIInForGUI(msg);
}


void MidiMarkovProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
  ////////////
  // deal with MIDI

  // transfer any pending notes into the midi messages and
  // clear pending - these messages come from the addMidi function
  // which the UI might call to send notes from the piano widget
  // note that midiToProcess is also accessed in addEvent above... 
  // so need to lock it here before reading and clearing 
  
  if (midiToProcess.getNumEvents() > 0)
  {
    midiMessages.addEvents(midiToProcess, midiToProcess.getFirstEventTime(), midiToProcess.getLastEventTime() + 1, 0);
    midiToProcess.clear();
  }

  if (midiMessages.getNumEvents() > 0){
    for (const auto metadata : midiMessages){
      auto msg = metadata.getMessage();
      if (msg.isNoteOnOrOff()){
        // DBG("Got MIDI " << midiMessages.getNumEvents());
        pushMIDIInForGUI(msg);
        // break; 
      }
    }
  }

  analysePitches(midiMessages);
  analyseDuration(midiMessages);
  analyseIoI(midiMessages);
  juce::MidiBuffer generatedMessages = generateNotesFromModel(midiMessages);


  // send note offs if needed  
  for (auto i = 0; i < 127; ++i)
  {
    if (noteOffTimes[i] > 0 &&
        noteOffTimes[i] < elapsedSamples)
    {
      juce::MidiMessage nOff = juce::MidiMessage::noteOff(1, i, 0.0f);
      generatedMessages.addEvent(nOff, 0);
      noteOffTimes[i] = 0;
    }
  }

  if (generatedMessages.getNumEvents() > 0){
    for (const auto metadata : generatedMessages){
      auto msg = metadata.getMessage();
      if (msg.isNoteOnOrOff()){
        // DBG("Generated some midi");
        pushMIDIOutForGUI(msg);
        // break; 
      }
    }
  }

  const bool allOff = sendAllNotesOffNext.load(std::memory_order_acquire);
  if (allOff){
    generatedMessages.clear();// don't send any more
    DBG("Processor sending all notes off.");
    for (int ch=1;ch<17;++ch){
      generatedMessages.addEvent(MidiMessage::allNotesOff(ch), 0);
      generatedMessages.addEvent(MidiMessage::allSoundOff(ch), 0);
    }
    sendAllNotesOffNext.store(false, std::memory_order_relaxed);

  }

  // now you can clear the outgoing buffer if you want
  midiMessages.clear();
  // then add your generated messages
  midiMessages.addEvents(generatedMessages, generatedMessages.getFirstEventTime(), -1, 0);

  elapsedSamples += buffer.getNumSamples();
}

//==============================================================================
bool MidiMarkovProcessor::hasEditor() const
{
  return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *MidiMarkovProcessor::createEditor()
{
  return new MidiMarkovEditor(*this);
}

//==============================================================================
void MidiMarkovProcessor::getStateInformation(juce::MemoryBlock &destData)
{
  // You should use this method to store your parameters in the memory block.
  // You could do that either as raw data, or use the XML or ValueTree classes
  // as intermediaries to make it easy to save and load complex data.
}

void MidiMarkovProcessor::setStateInformation(const void *data, int sizeInBytes)
{
  // You should use this method to restore your parameters from this memory block,
  // whose contents will have been created by the getStateInformation() call.
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
  return new MidiMarkovProcessor();
}

// Publish note/velocity to the UI mailbox (RT-safe, no locks/allocs).
void MidiMarkovProcessor::pushMIDIInForGUI(const juce::MidiMessage& msg)
{
    if (!msg.isNoteOnOrOff())
        return;

    const int   note = msg.getNoteNumber();
    const float vel  = msg.isNoteOn() ? juce::jlimit(0.0f, 1.0f, msg.getFloatVelocity())
                                      : 0.0f;

    lastNoteIn.store(note, std::memory_order_relaxed);
    lastVelocityIn.store(vel,   std::memory_order_relaxed);
    lastNoteInStamp.fetch_add(1, std::memory_order_release);
}

// Pull latest event if stamp changed since lastSeenStamp (message thread).
bool MidiMarkovProcessor::pullMIDIInForGUI(int& note, float& vel, uint32_t& lastSeenStamp)
{
    const auto s = lastNoteInStamp.load(std::memory_order_acquire);
    if (s == lastSeenStamp) return false; // don't send same note twice

    lastSeenStamp = s;
    note = lastNoteIn.load(std::memory_order_relaxed);
    if (note == -1) return false; // starting condition is that the note is -1

    vel  = lastVelocityIn.load(std::memory_order_relaxed);
    return true;
}

void MidiMarkovProcessor::pushMIDIOutForGUI(const juce::MidiMessage& msg)
{
    if (!msg.isNoteOnOrOff())
        return;

    const int   note = msg.getNoteNumber();
    const float vel  = msg.isNoteOn() ? juce::jlimit(0.0f, 1.0f, msg.getFloatVelocity())
                                      : 0.0f;

    lastNoteOut.store(note, std::memory_order_relaxed);
    lastVelocityOut.store(vel,   std::memory_order_relaxed);
    lastNoteOutStamp.fetch_add(1, std::memory_order_release);

}

// Pull latest event if stamp changed since lastSeenStamp (message thread).
bool MidiMarkovProcessor::pullMIDIOutForGUI(int& note, float& vel, uint32_t& lastSeenStamp)
{
    const auto s = lastNoteOutStamp.load(std::memory_order_acquire);

    if (s == lastSeenStamp) return false; // don't send back same note twice

    lastSeenStamp = s;
    note = lastNoteOut.load(std::memory_order_relaxed);
    if (note == -1) return false; // starting condition is that the note is -1
    vel  = lastVelocityOut.load(std::memory_order_relaxed);
    return true;
}


void MidiMarkovProcessor::resetMarkovModel()
{
  DBG("Resetting all models");
  pitchModel.reset();
  iOIModel.reset();
  noteDurationModel.reset();
}

void MidiMarkovProcessor::sendAllNotesOff()
{
  sendAllNotesOffNext.store(true, std::memory_order_relaxed);
}


void MidiMarkovProcessor::analyseIoI(const juce::MidiBuffer& midiMessages)
{
  // compute the IOI 
  for (const auto metadata : midiMessages){
      auto message = metadata.getMessage();
      if (message.isNoteOn()){   
          unsigned long exactNoteOnTime = elapsedSamples + message.getTimeStamp();
          unsigned long iOI = exactNoteOnTime - lastNoteOnTime;
          if (iOI < getSampleRate() * 2 && 
              iOI > getSampleRate() * 0.05){
            iOIModel.putEvent(std::to_string(iOI));
            DBG("Note on at: " << exactNoteOnTime << " IOI " << iOI);

          }
          lastNoteOnTime = exactNoteOnTime; 
      }
  }
}
    
void MidiMarkovProcessor::analysePitches(const juce::MidiBuffer& midiMessages)
{
  for (const auto metadata : midiMessages)
  {
    auto message = metadata.getMessage();
    if (message.isNoteOn())
    {
      // DBG("Msg timestamp " << message.getTimeStamp());
      pitchModel.putEvent(std::to_string(message.getNoteNumber()));
      noMidiYet = false;
    }
  }
}

void MidiMarkovProcessor::analyseDuration(const juce::MidiBuffer& midiMessages)
{
  for (const auto metadata : midiMessages)
  {
    auto message = metadata.getMessage();
    if (message.isNoteOn())
    {
      noteOnTimes[message.getNoteNumber()] = elapsedSamples + message.getTimeStamp();
    }
    if (message.isNoteOff()){
      unsigned long noteOffTime = elapsedSamples + message.getTimeStamp();
      unsigned long noteLength = noteOffTime - 
                                  noteOnTimes[message.getNoteNumber()];
      noteDurationModel.putEvent(std::to_string(noteLength));
    }
  }
}


juce::MidiBuffer MidiMarkovProcessor::generateNotesFromModel(const juce::MidiBuffer& incomingNotes)
{

  juce::MidiBuffer generatedMessages{};
  if (isTimeToPlayNote(elapsedSamples)){
    if (!noMidiYet){ // not in bootstrapping phase 
      int note = std::stoi(pitchModel.getEvent(true));
      if (note != 0){ 
        juce::MidiMessage nOn = juce::MidiMessage::noteOn(1,
                                                        note,
                                                        1.0f);
        // add the messages to the temp buffer
        generatedMessages.addEvent(nOn, 0);
        unsigned int duration = std::stoi(noteDurationModel.getEvent(true));
        noteOffTimes[note] = elapsedSamples + duration; 
      }

    }
    unsigned long nextIoI = std::stoi(iOIModel.getEvent());

    //DBG("generateNotesFromModel playing. modelPlayNoteTime passed " << modelPlayNoteTime << " elapsed " << elapsedSamples);
    if (nextIoI > 0){
      modelPlayNoteTime = elapsedSamples + nextIoI;
      // DBG("generateNotesFromModel new modelPlayNoteTime passed " << modelPlayNoteTime << "from IOI " << nextIoI);
    } 
  }
  return generatedMessages;
}

bool MidiMarkovProcessor::isTimeToPlayNote(unsigned long currentTime)
{
  // if (modelPlayNoteTime == 0){
  //   return false; 
  // }
  if (currentTime >= modelPlayNoteTime){
    return true;
  }
  else {
    return false; 
  }
}

// call after playing a note 
void MidiMarkovProcessor::updateTimeForNextPlay()
{

}



