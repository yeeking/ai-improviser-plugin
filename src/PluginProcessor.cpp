/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include <cstddef>
#include <fstream>
#include <random>
#include <algorithm>

/** This is the currently preferred way (2025) of setting up params  */
static juce::AudioProcessorValueTreeState::ParameterLayout makeParameterLayout()
{
    using namespace juce;
    static constexpr int kParamVersion = 1;

    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.emplace_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "playing", kParamVersion }, "Playing", true));

    params.emplace_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "learning", kParamVersion }, "Learning", true));

    params.emplace_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "leadFollow", kParamVersion }, "Lead/follow", true));

    params.emplace_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "playProbability", kParamVersion }, "Play Probability",
        NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    params.emplace_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "quantise", kParamVersion }, "Quantise", false));

    params.emplace_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "quantBPM", kParamVersion }, "Quant BPM",
        NormalisableRange<float>(20.0f, 300.0f), 150.0f));

    // to future self - note there is a tricky interaction 
    // between this and the gui - make sure the number of options on the combo
    // == maxValue and minValue is 1. then add all options to the
    // ImproviserControlGUI::divisionIdToValue function
    params.emplace_back(std::make_unique<AudioParameterInt>(
        ParameterID{ "quantDivision", kParamVersion }, "Quant Division",
        1, 6, 1));

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
    , polyphonyModel{}
    , iOIModel{}
    , velocityModel{}
    , lastNoteOnTime{0}
    , noMidiYet{true}
    , elapsedSamples{0}
    , modelPlayNoteTime{0}
    , chordDetect{0}
    , midiMonitor{44100}
{
    // set all note on/off times to zero
    for (int i = 0; i < 127; ++i)
    {
        noteOffTimes[i] = 0;
        noteOnTimes[i]  = 0;
    }

    playingParam         = apvts.getRawParameterValue("playing");
    learningParam        = apvts.getRawParameterValue("learning");
    leadFollowParam      = apvts.getRawParameterValue("leadFollow");
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
  midiMonitor.setSampleRate(getSampleRate());
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
// this is only currently called by the piano ui
void MidiMarkovProcessor::uiAddsMidi(const juce::MidiMessage& msg, int sampleOffset)
{
  // might not be thread safe whoops - should probaably lock
  // midiToProcess before adding things to it 
  midiReceivedFromUI.addEvent(msg, sampleOffset);  // keep your existing logic
  // Notify UI via mailbox only — do NOT touch the editor from here.
  pushMIDIInForGUI(msg);
}

void MidiMarkovProcessor::sendMidiPanic (juce::MidiBuffer& out, int samplePos)
{
    // 1) Kill sustain & reset controllers first
    for (int ch = 1; ch <= 16; ++ch)
    {
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 64, 0), samplePos);   // Sustain off
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 123, 0), samplePos);  // All notes off
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 120, 0), samplePos);  // All sound off
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 121, 0), samplePos);  // Reset all controllers
        out.addEvent (juce::MidiMessage::pitchWheel      (ch, 0x2000), samplePos);  // Center pitch bend
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 1, 0), samplePos);    // Mod wheel to 0
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 11, 127), samplePos); // Expression to default
    }

    // 2) Brute-force send NoteOff for every key on every channel
    for (int ch = 1; ch <= 16; ++ch)
        for (int note = 0; note < 128; ++note)
            out.addEvent (juce::MidiMessage::noteOff (ch, note), samplePos);

    // Optional: tiny follow-up at next sample to catch edge cases
    for (int ch = 1; ch <= 16; ++ch)
        out.addEvent (juce::MidiMessage::controllerEvent (ch, 64, 0), samplePos + 1);
}


void MidiMarkovProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{

  // DBG("Playing on/ off param " << playingParam->load());

  bool allOff = sendAllNotesOffNext.load(std::memory_order_acquire);

  
  ////////////
  // handle any midi sent from the GUI
  if (midiReceivedFromUI.getNumEvents() > 0)
  {
    midiMessages.addEvents(midiReceivedFromUI, midiReceivedFromUI.getFirstEventTime(), midiReceivedFromUI.getLastEventTime() + 1, 0);
    midiReceivedFromUI.clear();
  }

  // if we got any midi from anywhere: bits of the UI (e.g. on screen piano widget) or the MIDI input
  // set up the midi note  for the note display GUI to consume 
  if (midiMessages.getNumEvents() > 0){
    for (const auto metadata : midiMessages){
      auto msg = metadata.getMessage();
      if (msg.isNoteOnOrOff()){
        pushMIDIInForGUI(msg); // when the gui knocks, it'll get this
        break; 
      }
    }
  }

  // learn from received MIDI messages
  if (learningParam->load() > 0){ // learning is on
    unsigned long quantBlockSizeSamples = 0;
    if (quantiseParam->load() > 0){
      // calculate it as quant is enabled
      // length of a beat in seconds
      // 60.0f / quantBPMParam->load()
      // scaled by 1/quant division (e.g. 0.25 for quarter beats)
      quantBlockSizeSamples = static_cast<unsigned long>(getSampleRate() * ((ImproviserControlGUI::divisionIdToValue(quantDivisionParam->load())) * (60.0f / quantBPMParam->load())));
      // DBG("SR " << getSampleRate() << " BPM:" << quantBPMParam->load() << " beat (index) " << quantDivisionParam->load()<< " Quant is " << quantBlockSizeSamples);
    }
    analysePitches(midiMessages);
    analyseDuration(midiMessages, quantBlockSizeSamples);
    analyseIoI(midiMessages, quantBlockSizeSamples);
    analyseVelocity(midiMessages);
  }

  // now generate from models
  unsigned long elapsedSamplesAtStart = elapsedSamples; 
  unsigned long elapsedSamplesAtEnd = elapsedSamplesAtStart + static_cast<unsigned long>(buffer.getNumSamples());  
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

  // now handle telling the GUI we are sending a note
  if (generatedMessages.getNumEvents() > 0){
    for (const auto metadata : generatedMessages){
      auto msg = metadata.getMessage();
      if (msg.isNoteOnOrOff()){
        // DBG("Generated some midi");
        pushMIDIOutForGUI(msg);
        break; 
      }
    }
  }

  // clear the outgoing buffer - no midi thru... 
  midiMessages.clear();
  // then add  generated messages
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
        // it is a note off or some other midi - just add it 
        midiMessages.addEvent(msg, metadata.samplePosition);
      }
    }
  }

  // LOGGING NOTE EVENTS
  // so can detect stuck notes potentially
  for (const auto metadata : midiMessages){
      auto msg = metadata.getMessage();
      // tell the midi monitor what we are doing 
      midiMonitor.eventWasAddedToBuffer(msg, elapsedSamples + static_cast<unsigned long> (metadata.samplePosition));
  }


  // DEAL WITH playing/ not playing mode
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
      // DBG("Processblock - transitioned to not playing. requesting note off");
      allOff = true; 
    }
  }

  // DEAL WITH STUCK NOTES
  // now check if the midi monitor found any stuck notes
  std::vector<int> stuckNotes = midiMonitor.getStuckNotes(elapsedSamplesAtEnd);
  if (stuckNotes.size() > 0){
    for (auto note : stuckNotes){
      midiMessages.addEvent(MidiMessage::noteOff(1, note), 0);
      midiMonitor.unstickNote(note);
    }
  }
 


  // SEND ALL OFF IF NEEDED
  // sending alloff should at the end of processblock

  if (allOff){
      // DBG("Processblock - all off requested. Sending all off. note on time is  model play note time>> " << modelPlayNoteTime);

    midiMessages.clear();// don't send any more
    midiReceivedFromUI.clear();

  
    DBG("Processor sending all notes off.");
    sendMidiPanic(midiMessages, 0);
    // for (int ch=1;ch<17;++ch){
    //   midiMessages.addEvent(MidiMessage::allNotesOff(ch), 0);
    //   midiMessages.addEvent(MidiMessage::allSoundOff(ch), 0);
    // }
    sendAllNotesOffNext.store(false, std::memory_order_relaxed);
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
          std::vector<int> notesVec = chordDetect.getChord();

          std::string notes = 
              MidiMarkovProcessor::notesToMarkovState(notesVec);
          // DBG("Got notes from detector " << notes);
          // DBG("pushing to poly model " << notesVec.size() << " from notes " << notes);

          pitchModel.putEvent(notes);
          polyphonyModel.putEvent(std::to_string(notesVec.size()));
      }     
      noMidiYet = false;// bootstrap code
    }
  }
}



int MidiMarkovProcessor::quantiseInterval(int interval, int quantBlock)
{
    if (quantBlock == 0) return interval; 
    int q = interval / quantBlock;
    int r = interval % quantBlock;
    int absR = std::abs(r);
    int halfX = std::abs(quantBlock) / 2;

    if (absR > halfX) return (interval >= 0) ? (q + 1) * quantBlock : (q - 1) * quantBlock;
    if (absR < halfX) return q * quantBlock;

    // exactly halfway → round to even
    return ((q % 2 == 0) ? q : ((interval >= 0) ? q + 1 : q - 1)) * quantBlock;
}
void MidiMarkovProcessor::analyseIoI(const juce::MidiBuffer& midiMessages, int quantBlockSizeSamples)
{
  // compute the IOI 
  for (const auto metadata : midiMessages){
      auto message = metadata.getMessage();
      if (message.isNoteOn()){   
          unsigned long exactNoteOnTime = elapsedSamples + message.getTimeStamp();
          int iOI = static_cast<int>(exactNoteOnTime - lastNoteOnTime);
          if (iOI < getSampleRate() * 2 && 
              iOI > getSampleRate() * 0.05){
            if (quantBlockSizeSamples != 0){// quantise it
              // DBG("analyseIoI quant from " << iOI << " to " << MidiMarkovProcessor::quantiseInterval(iOI, quantBlockSizeSamples));

              iOI = MidiMarkovProcessor::quantiseInterval(iOI, quantBlockSizeSamples);
              if (iOI == 0) iOI = quantBlockSizeSamples;
            }
            if (iOI > 0){// ignore zero iois
              iOIModel.putEvent(std::to_string(iOI));
            }   

          }
          lastNoteOnTime = exactNoteOnTime; 
      }
  }
}

void MidiMarkovProcessor::analyseDuration(const juce::MidiBuffer& midiMessages, int quantBlockSizeSamples)
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
      int noteLength = static_cast<int> (noteOffTime - 
                                  noteOnTimes[message.getNoteNumber()]);
      if (quantBlockSizeSamples != 0){// quantise it
        // DBG("analyseDuration quant from " << noteLength << " to " << MidiMarkovProcessor::quantiseInterval(noteLength, quantBlockSizeSamples));
        noteLength = MidiMarkovProcessor::quantiseInterval(noteLength, quantBlockSizeSamples);
        if (noteLength == 0) noteLength = quantBlockSizeSamples;
      }
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
  if (pitchModel.getModelSize() < 2){// only play once we've got something!
    return generatedMessages;
  }

  unsigned long nextIoI = 0;
  // dicates if we use the incoming midi as context
  // or the previous model output as context
  bool inputIsContextMode = !leadFollowParam->load();

  if (isTimeToPlayNote(bufferStartTime, bufferEndTime)){
    if (!noMidiYet){ // not in bootstrapping phase 
      std::string notes = pitchModel.getEvent(true, inputIsContextMode);
      unsigned long duration = std::stoul(noteDurationModel.getEvent(true, inputIsContextMode));
      juce::uint8 velocity = std::stoi(velocityModel.getEvent(true, inputIsContextMode));
      unsigned long noteOnTime = modelPlayNoteTime - bufferStartTime; 
      // DBG("Note on time " << noteOnTime);
      // jassert(noteOnTime >= bufferStartTime && noteOnTime < bufferEndTime);
      if (noteOnTime >= 0){// valid note on time
        // get notes from the pitch model 
        std::vector<int> gotNotes = markovStateToNotes(notes);// model gave us this
        std::vector<int> playNotes{};// apply polyphony then play these

        //  apply the polyphony model
        int wantPolyphony = std::stoi(polyphonyModel.getEvent(true, inputIsContextMode));
        int gotPolyphony = static_cast<int>(gotNotes.size());
        if (gotPolyphony > wantPolyphony){// kill some notes
          thread_local std::mt19937 rng{std::random_device{}()};

          // Shuffle the indices, then take the first `samples` of them.
          std::shuffle(gotNotes.begin(), gotNotes.end(), rng);
          for (int i=0;i<wantPolyphony; ++i){
            playNotes.push_back(gotNotes[i]);
          }
          // DBG("gen notes: trimeed " << gotPolyphony << " to " << wantPolyphony << ":" << playNotes.size());
        }
        else{// got correct number of notes - just play them all
          playNotes = std::move(gotNotes);
        }
        for (const int& note : playNotes){
            juce::MidiMessage nOn = juce::MidiMessage::noteOn(1, note, velocity);
            // DBG("generateNotesFromModel adding a note " << note << " v: " << velocity );

            generatedMessages.addEvent(nOn, noteOnTime);// note to be played in this block
            // ptocess Block deals with note offs - we just peg em here 
            // but if this note is already playing
            // then to avoid a double trigger/ note hold problem
            // we need to add a note off to generatedmessage
            if (noteOffTimes[note] > 0){// already playing this note
              // DBG("generatemidi: " << note << " currently playing and want to play again ");
              juce::MidiMessage nOff = juce::MidiMessage::noteOff(1, note);
              generatedMessages.addEvent(nOff, 0);// send note off at the start of the block
            } 
            noteOffTimes[note] = elapsedSamples + duration; 
        }
      }
    }


    // how long to wait before we play next note/ chord
    nextIoI = std::stoul(iOIModel.getEvent(true, inputIsContextMode));

    // unsigned long quant = quantBPMParam.load()
    // apply quantisation if necessary

    //DBG("generateNotesFromModel playing. modelPlayNoteTime passed " << modelPlayNoteTime << " elapsed " << elapsedSamples);
    if (nextIoI > 0){
      // DBG("Got non-zero ioi play at " << (elapsedSamples + nextIoI));
      modelPlayNoteTime = elapsedSamples + nextIoI;
      // DBG("generateNotesFromModel new modelPlayNoteTime passed " << modelPlayNoteTime << "from IOI " << nextIoI);
    } 
  }
  // if (generatedMessages.getNumEvents() > 0){
  //   DBG("generateNotesFromModel:: retuirning notes " << generatedMessages.getNumEvents() << " ioi " << nextIoI);
  // }
  if (nextIoI == 0 && generatedMessages.getNumEvents() > 0){// stuck note badness. clear the notes
    // DBG("Clearing notes....");
    generatedMessages.clear();
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
    // shift the start time along to bootstrap
    // playback. Weird but necessary
    modelPlayNoteTime = windowEndTime;// force the note play time on. eventually we'll want to play, right? 
    return false;  
  }
  if (modelPlayNoteTime >= windowStartTime && modelPlayNoteTime < windowEndTime){
    // DBG("time to play: [ win s "<< windowStartTime << " m: " << modelPlayNoteTime << " win e " << windowEndTime << " ]");
    return true; 
  }
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

    std::vector<MarkovManager*> mms = {&pitchModel, &polyphonyModel,  &iOIModel, &noteDurationModel, &velocityModel};
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
    std::vector<MarkovManager*> mms = {&pitchModel, &polyphonyModel, &iOIModel, &noteDurationModel, &velocityModel};
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
  std::vector<MarkovManager*> mms = {&pitchModel, &polyphonyModel, &iOIModel, &noteDurationModel, &velocityModel};
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
