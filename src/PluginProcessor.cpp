/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cstddef>
#include <fstream>

/** This is the currently preferred way (2025) of setting up params  */
static juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout()
{
    using namespace juce;
    static constexpr int kParamVersion = 1;

    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.emplace_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "playing", kParamVersion }, "Playing", false));

    params.emplace_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "learning", kParamVersion }, "Learning", false));

    params.emplace_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "playProbability", kParamVersion }, "Play Probability",
        NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    params.emplace_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "quantise", kParamVersion }, "Quantise", false));

    params.emplace_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "quantBPM", kParamVersion }, "Quant BPM",
        NormalisableRange<float>(20.0f, 300.0f), 150.0f));

    params.emplace_back(std::make_unique<AudioParameterInt>(
        ParameterID{ "quantDivision", kParamVersion }, "Quant Division",
        1, 8, 4));

    params.emplace_back(std::make_unique<AudioParameterInt>(
        ParameterID{ "midiInChannel", kParamVersion }, "MIDI In Channel",
        0, 16, 0)); // 0 = All

    params.emplace_back(std::make_unique<AudioParameterInt>(
        ParameterID{ "midiOutChannel", kParamVersion }, "MIDI Out Channel",
        1, 16, 1));

    return { params.begin(), params.end() };
}


//==============================================================================
// Constructor
MidiMarkovProcessor::MidiMarkovProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
 #if !JucePlugin_IsSynth
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
 #endif
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
      )
#endif
    // --- APVTS now initialised with a ParameterLayout + state name ---
    , apvts(*this, nullptr, "MidiMarkovState", makeParameterLayout())
    , pitchModel{}
    , iOIModel{}
    , velocityModel{}
    , lastNoteOnTime{0}
    , noMidiYet{true}
    , elapsedSamples{0}
    , modelPlayNoteTime{0}
    , chordDetect{0}
{
    // set all note on/off times to zero
    for (int i = 0; i < 127; ++i)
    {
        noteOffTimes[i] = 0;
        noteOnTimes[i]  = 0;
    }

    playingParam         = apvts.getRawParameterValue("playing");
    learningParam        = apvts.getRawParameterValue("learning");
    playProbabilityParam = apvts.getRawParameterValue("playProbability");
    quantiseParam        = apvts.getRawParameterValue("quantise");
    quantBPMParam        = apvts.getRawParameterValue("quantBPM");
    quantDivisionParam   = apvts.getRawParameterValue("quantDivision");
    midiInChannelParam   = apvts.getRawParameterValue("midiInChannel");
    midiOutChannelParam  = apvts.getRawParameterValue("midiOutChannel");


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
  double maxIntervalInSamples = sampleRate * 0.05; // 50ms - the threshold for deciding if its a chord or not
  chordDetect = ChordDetector((unsigned long) maxIntervalInSamples); 
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
  midiToProcess.addEvent(msg, sampleOffset);  // keep your existing logic
  // Notify UI via mailbox only — do NOT touch the editor from here.
  pushMIDIInForGUI(msg);
}


void MidiMarkovProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{

  // DBG("Playing on/ off param " << playingParam->load());

  bool allOff = sendAllNotesOffNext.load(std::memory_order_acquire);

  
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
        break; 
      }
    }
  }

  if (learningParam->load() == 1.0f){ // learning is on
    analysePitches(midiMessages);
    analyseDuration(midiMessages);
    analyseIoI(midiMessages);
    analyseVelocity(midiMessages);
  }
  unsigned long elapsedSamplesAtStart = elapsedSamples; 
  unsigned long elapsedSamplesAtEnd = elapsedSamplesAtStart + buffer.getNumSamples(); 
  
  juce::MidiBuffer generatedMessages = generateNotesFromModel(midiMessages, elapsedSamplesAtStart, elapsedSamplesAtEnd);


  

  // send note offs if needed  
  for (auto i = 0; i < 127; ++i)
  {
    if (noteOffTimes[i] > elapsedSamplesAtStart &&
        noteOffTimes[i] < elapsedSamplesAtEnd)
    {
      unsigned long noteSampleOffset = noteOffTimes[i] - elapsedSamplesAtStart;
      
      juce::MidiMessage nOff = juce::MidiMessage::noteOff(1, i, 0.0f);
      generatedMessages.addEvent(nOff, noteSampleOffset);
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

  // now you can clear the outgoing buffer if you want
  midiMessages.clear();
  // then add your generated messages
  midiMessages.addEvents(generatedMessages, generatedMessages.getFirstEventTime(), -1, 0);

  // if probability < 1, selectively remove note ons
   if (playProbabilityParam->load() < 1.0 && midiMessages.getNumEvents()> 0){
    // make a clear buffer
    generatedMessages.clear();
    // swap full buffer and clear buffer
    generatedMessages.swapWith(midiMessages);
    // now re-add messages that are note ons
    for (const auto metadata : generatedMessages){
      auto msg = metadata.getMessage();
      if (msg.isNoteOn()){ // only add if below prob
        if (juce::Random::getSystemRandom().nextDouble() < playProbabilityParam->load()){
            midiMessages.addEvent(msg, metadata.samplePosition);
        }
      }
      else{
        // just add it 
        midiMessages.addEvent(msg, metadata.samplePosition);
      }
    }
  }

  // if we are not playing, remove note ons from the buffer
  // let everything else go through 
  if (playingParam->load() == 1.0f){//
    if (!lastPlayingParamState.load()){// transition the last playing state
      lastPlayingParamState.store(true);
    }
  }

  if (playingParam->load() == 0.0f){// do not play
    midiMessages.clear();
    if (lastPlayingParamState.load()){// we just transitioned to not playing - call all off once when the parameter changes
      lastPlayingParamState.store(false);
      allOff = true; 
    }
  }

  if (allOff){
    midiMessages.clear();// don't send any more
    midiToProcess.clear();
    DBG("Processor sending all notes off.");
    for (int ch=1;ch<17;++ch){
      midiMessages.addEvent(MidiMessage::allNotesOff(ch), 0);
      midiMessages.addEvent(MidiMessage::allSoundOff(ch), 0);
    }
    sendAllNotesOffNext.store(false, std::memory_order_relaxed);
    return; 
  }
  elapsedSamples = elapsedSamplesAtEnd;
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

// In your processor .cpp
void MidiMarkovProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // 1) Grab the whole APVTS state tree
    auto state = apvts.copyState();

    // (Optional) add your own extra properties/child state here:
    // state.setProperty("modelVersion", 1, nullptr);
    // state.setProperty("lastPresetPath", lastPresetPath, nullptr);

    // 2) Turn it into XML and write to host’s memory block
    if (auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void MidiMarkovProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // 1) Read XML back from host
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        // 2) Convert to ValueTree and replace APVTS state
        auto restored = juce::ValueTree::fromXml(*xml);

        // (Optional) handle migrations before replacing:
        // if (auto v = restored.getProperty("modelVersion"); v.isVoid()) { /* set defaults */ }

        apvts.replaceState(std::move(restored)); // thread-safe replace with internal lock
    }
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
  // DBG("Resetting all models");
  // pitchModel.reset();
  // iOIModel.reset();
  // noteDurationModel.reset();
  // velocityModel.reset();
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
            // DBG("Note on at: " << exactNoteOnTime << " IOI " << iOI);

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
    if (message.isNoteOn()){
      chordDetect.addNote(
            message.getNoteNumber(), 
            // add the offset within this buffer
            elapsedSamples + message.getTimeStamp()
        );
      if (chordDetect.hasChord()){
          std::string notes = 
              MidiMarkovProcessor::notesToMarkovState(
                  chordDetect.getChord()
              );
          // DBG("Got notes from detector " << notes);
          pitchModel.putEvent(notes);
      }     
      noMidiYet = false;// bootstrap code
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


void MidiMarkovProcessor::analyseVelocity(const juce::MidiBuffer& midiMessages)
{
  // compute the IOI 
  for (const auto metadata : midiMessages){
      auto message = metadata.getMessage();
      if (message.isNoteOn()){   
          auto velocity = message.getVelocity();
          // DBG("Vel " << velocity);
          velocityModel.putEvent(std::to_string(velocity));
      }
  }
}

juce::MidiBuffer MidiMarkovProcessor::generateNotesFromModel(const juce::MidiBuffer& incomingNotes, unsigned long bufferStartTime, unsigned long bufferEndTime)
{
  juce::MidiBuffer generatedMessages{};


  if (isTimeToPlayNote(bufferStartTime, bufferEndTime)){
    if (!noMidiYet){ // not in bootstrapping phase 
      std::string notes = pitchModel.getEvent();
      unsigned long duration = std::stoul(noteDurationModel.getEvent(true));
      juce::uint8 velocity = std::stoi(velocityModel.getEvent(true));
      unsigned long noteOnTime = modelPlayNoteTime - bufferStartTime; 
      // DBG("Note on time " << noteOnTime);
      // jassert(noteOnTime >= bufferStartTime && noteOnTime < bufferEndTime);
      if (noteOnTime >= 0){// first one seems to be < 0
        for (const int& note : markovStateToNotes(notes)){
            juce::MidiMessage nOn = juce::MidiMessage::noteOn(1, note, velocity);
            generatedMessages.addEvent(nOn, noteOnTime);
            noteOffTimes[note] = elapsedSamples + duration; 
        }
      }
    }

    unsigned long nextIoI = std::stoul(iOIModel.getEvent());
    // unsigned long quant = quantBPMParam.load()
    // apply quantisation if necessary


    //DBG("generateNotesFromModel playing. modelPlayNoteTime passed " << modelPlayNoteTime << " elapsed " << elapsedSamples);
    if (nextIoI > 0){
      // DBG("Got non-zero ioi play at " << (elapsedSamples + nextIoI));
      modelPlayNoteTime = elapsedSamples + nextIoI;
      // DBG("generateNotesFromModel new modelPlayNoteTime passed " << modelPlayNoteTime << "from IOI " << nextIoI);
    } 
  }
  return generatedMessages;
}


bool MidiMarkovProcessor::isTimeToPlayNote(unsigned long windowStartTime, unsigned long windowEndTime)
{
  // if (modelPlayNoteTime == 0){
  //   return false; 
  // }
  // DBG("play at " << modelPlayNoteTime << " win: " << windowStartTime << ":" << windowEndTime);
  if (modelPlayNoteTime < windowStartTime) {
    // DBG("timing bootstrap phase I think " << modelPlayNoteTime);
    modelPlayNoteTime = windowEndTime;// boot strap it ... maybe a better way... 
    return false;  
  }
  if (modelPlayNoteTime >= windowStartTime && modelPlayNoteTime < windowEndTime) return true; 
  else return false; 
}

// call after playing a note 
void MidiMarkovProcessor::updateTimeForNextPlay()
{

}

std::string MidiMarkovProcessor::notesToMarkovState(
               const std::vector<int>& notesVec)
{
std::string state{""};
for (const int& note : notesVec){
  state += std::to_string(note) + "-";
}
return state; 
}

std::vector<int> MidiMarkovProcessor::markovStateToNotes(
              const std::string& notesStr)
{
  std::vector<int> notes{};
  if (notesStr == "0") return notes;
  for (const std::string& note : 
           MarkovChain::tokenise(notesStr, '-')){
    notes.push_back(std::stoi(note));
  }
  return notes; 
}

juce::AudioProcessorValueTreeState& MidiMarkovProcessor::getAPVTState()
{ 
  return apvts; 
}

// impro control listener interface

void MidiMarkovProcessor::loadModel()
{
  // suspendProcessing(true);
  DBG("Proc: load model");
  // suspendProcessing(false);

}

void MidiMarkovProcessor::saveModel()
{
  // suspendProcessing(true);

  DBG("Proc: save model");
  // suspendProcessing(false);

}
void MidiMarkovProcessor::resetModel()
{
  suspendProcessing(true);

  DBG("Proc: reset model");
  // reset this lot
    // MarkovManager pitchModel;
    // MarkovManager iOIModel;
    // MarkovManager noteDurationModel;    
    // MarkovManager velocityModel;    

    std::vector<MarkovManager*> mms = {&pitchModel, &iOIModel, &noteDurationModel, &velocityModel};
  for (MarkovManager* mm : mms)
  {
    mm->reset();
  }
  for (int i = 0; i < 127; ++i)
  {
    noteOffTimes[i] = 0;
    noteOnTimes[i]  = 0;
  }
  // next time processBlock is called, it'll send all notes off and return 
  sendAllNotesOffNext.store(true);
  suspendProcessing(false);

}



// load and save implementation from the old p[ugin]
  bool MidiMarkovProcessor::loadModel(std::string filename)
{
  if (std::ifstream in {filename})
  {
    std::ostringstream sstr{};
    sstr << in.rdbuf();
    std::string data = sstr.str();
    in.close();
    // now split the data on the header 
    std::vector<std::string> modelStrings = MarkovChain::tokenise(data, this->FILE_SEP_FOR_SAVE);
    // do some checks on the modelStrings
    if (modelStrings.size() != 4) {
      DBG("DinvernoPolyMarkov::loadModel did not find 4 model strings in file " << filename);
      return false; 
    }
    std::vector<MarkovManager*> mms = {&pitchModel, &iOIModel, &noteDurationModel, &velocityModel};
    for (size_t i = 0; i<mms.size();i++)
    {
      bool loaded = mms[i]->setupModelFromString(modelStrings[i]);
      if (!loaded){
        DBG("DinvernoPolyMarkov::loadModel error loading model "<<i << " from " << filename);
        return false; 
      }
      else{
        DBG("DinvernoPolyMarkov::loadModel loaded model "<<i << " from " << filename);
      }
    }

    return true; 


  }
  else {
    std::cout << "DinvernoPolyMarkov::loadModel failed to load from file " << filename << std::endl;
    return false; 
  }
}


bool MidiMarkovProcessor::saveModel(std::string filename)
{
  // we have four models so write each to a temp file
  // read it in as a string
  std::string data{""};
  std::vector<MarkovManager*> mms = {&pitchModel, &iOIModel, &noteDurationModel, &velocityModel};
  for (MarkovManager* mm : mms)
  {
    data += this->FILE_SEP_FOR_SAVE;
    data += mm->getModelAsString();
  }
  if (std::ofstream ofs{filename}){
    ofs << data;
    ofs.close();
    return true; 
  }
  else {
    std::cout << "DinvernoPolyMarkov::saveModel failed to save to file " << filename << std::endl;
    return false; 
  }

}
