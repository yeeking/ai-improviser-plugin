/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "juce_audio_basics/juce_audio_basics.h"
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <random>
#include <algorithm>
#include <limits>
#include <iterator>
#include <optional>
#include <cmath>

namespace
{
inline void appendUint32(std::string& dest, uint32_t value)
{
    dest.push_back(static_cast<char>(value & 0xFFu));
    dest.push_back(static_cast<char>((value >> 8) & 0xFFu));
    dest.push_back(static_cast<char>((value >> 16) & 0xFFu));
    dest.push_back(static_cast<char>((value >> 24) & 0xFFu));
}

inline bool readUint32(const std::string& src, size_t& offset, uint32_t& value)
{
    if (offset + 4 > src.size())
        return false;

    const auto b0 = static_cast<uint32_t>(static_cast<unsigned char>(src[offset]));
    const auto b1 = static_cast<uint32_t>(static_cast<unsigned char>(src[offset + 1]));
    const auto b2 = static_cast<uint32_t>(static_cast<unsigned char>(src[offset + 2]));
    const auto b3 = static_cast<uint32_t>(static_cast<unsigned char>(src[offset + 3]));
    value = b0 | (b1 << 8) | (b2 << 16) | (b3 << 24);
    offset += 4;
    return true;
}

}

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

    params.emplace_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "avoid", kParamVersion }, "Avoid range", false));

    params.emplace_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "slowMo", kParamVersion }, "Slow mo", false));

    params.emplace_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "overpoly", kParamVersion }, "Overpoly", false));

    params.emplace_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "callAndResponse", kParamVersion }, "Call and response", false));

    params.emplace_back(std::make_unique<AudioParameterFloat>(
        ParameterID{ "playProbability", kParamVersion }, "Play Probability",
        NormalisableRange<float>(0.0f, 1.0f), 1.0f));

    params.emplace_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "quantise", kParamVersion }, "Quantise", false));

    params.emplace_back(std::make_unique<AudioParameterBool>(
        ParameterID{ "quantUseHostClock", kParamVersion }, "Use Host Clock", false));

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
    , lastIncomingNoteOnTime{0}
    , noMidiYet{true}
    , elapsedSamples{0}
    , nextTimeToPlayANote{0}
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
    avoidParam           = apvts.getRawParameterValue("avoid");
    slowMoParam          = apvts.getRawParameterValue("slowMo");
    overpolyParam        = apvts.getRawParameterValue("overpoly");
    callResponseParam    = apvts.getRawParameterValue("callAndResponse");
    playProbabilityParam = apvts.getRawParameterValue("playProbability");
    quantiseParam        = apvts.getRawParameterValue("quantise");
    quantUseHostClockParam = apvts.getRawParameterValue("quantUseHostClock");
    quantBPMParam        = apvts.getRawParameterValue("quantBPM");
    quantDivisionParam   = apvts.getRawParameterValue("quantDivision");
    midiInChannelParam   = apvts.getRawParameterValue("midiInChannel");
    midiOutChannelParam  = apvts.getRawParameterValue("midiOutChannel");
    quantBpmParamObject  = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("quantBPM"));

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
  clockSamplesAccumulated = 0.0;
  clockSamplesPerTick = calculateClockSamplesPerTick(sampleRate);
  lastClockTickStamp.store(0, std::memory_order_relaxed);
  hostClockPositionInitialised = false;
  hostClockLastPpq = 0.0;
  hostAwaitingFirstTick = true;
  lastHostTransportPlaying = false;
  hostLastKnownTimeInSamples.reset();
  hostLastKnownPpqPosition.reset();
  hostLastKnownWasPlaying = false;
  lastProcessBlockSampleCount = 0;
  havePreviousBlockInfo = false;
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
  bool allOff = sendAllNotesOffNext.load(std::memory_order_acquire);
  const bool hostClockEnabled = (quantUseHostClockParam != nullptr) && (quantUseHostClockParam->load() > 0.5f);
  HostClockInfo hostInfo = pb_collectHostClockInfo(hostClockEnabled);
  if (hostClockEnabled)
  {
      if (double hostTick = calculateHostClockSamplesPerTick(hostInfo); hostTick > 0.0)
          clockSamplesPerTick = hostTick;
  }
  const bool hostRestarted = hostClockEnabled
                              && hostInfo.transportKnown
                              && hostInfo.transportPlaying
                              && !lastHostTransportPlaying;
  const bool hostTransportJumped = hostClockEnabled && hostInfo.transportPositionChanged;
  const bool hostAllowsPlayback = hostClockEnabled ? (hostInfo.transportKnown ? hostInfo.transportPlaying : false)
                                                   : true;
  const bool playingParamEnabled = playingParam != nullptr ? (playingParam->load() > 0.5f) : false;
  const bool wasPlaying = lastPlayingParamState.load(std::memory_order_acquire);
  const bool shouldPlayNow = playingParamEnabled && hostAllowsPlayback;
  const bool playingReactivated = shouldPlayNow && !wasPlaying;

  if (hostClockEnabled)
  {
      bool alignedForHostRestart = false;
      if (hostRestarted && playingParamEnabled)
      {
          alignModelPlayTimeToNextTick(true, hostInfo);
          hostAwaitingFirstTick = true;
          alignedForHostRestart = true;
      }

      if (hostTransportJumped && hostInfo.transportPlaying && playingParamEnabled && !alignedForHostRestart)
      {
          alignModelPlayTimeToNextTick(true, hostInfo);
          hostAwaitingFirstTick = true;
          alignedForHostRestart = true;
      }

      if (playingReactivated && !alignedForHostRestart)
      {
          alignModelPlayTimeToNextTick(true, hostInfo);
          hostAwaitingFirstTick = true;
      }
  }
  else
  {
      hostAwaitingFirstTick = false;
      if (playingReactivated)
          alignModelPlayTimeToNextTick(false, hostInfo);
  }

  const double manualBpm = quantBPMParam != nullptr ? static_cast<double>(quantBPMParam->load()) : 120.0;
  double effectiveBpm = manualBpm;
  bool usingHostBpm = false;
  if (hostClockEnabled && hostInfo.hasBpm && hostInfo.bpm > 0.0)
  {
      effectiveBpm = hostInfo.bpm;
      usingHostBpm = true;
  }

  effectiveBpmForDisplay.store(static_cast<float>(effectiveBpm), std::memory_order_relaxed);
  effectiveBpmIsHost.store(usingHostBpm, std::memory_order_relaxed);

  pb_handleMidiFromUI(midiMessages);

  if (hostClockEnabled)
      pb_tickHostClock(hostInfo.transportPlaying, hostInfo.hasPpq, hostInfo.ppqPosition);
  else
      pb_tickInternalClock(buffer);

  pb_informGuiOfIncoming(midiMessages);
  pb_learnFromIncomingMidi(midiMessages, effectiveBpm);

  const unsigned long elapsedSamplesAtStart = elapsedSamples;
  const unsigned long elapsedSamplesAtEnd = elapsedSamplesAtStart + static_cast<unsigned long>(buffer.getNumSamples());
  // DBG("from s to e " << elapsedSamplesAtStart << " : " << elapsedSamplesAtEnd << " diff " << (elapsedSamplesAtEnd - elapsedSamplesAtStart));
  juce::MidiBuffer generatedMessages;
  if (!hostAwaitingFirstTick)
      generatedMessages = generateNotesFromModel(midiMessages, elapsedSamplesAtStart, elapsedSamplesAtEnd, hostInfo);

  pb_schedulePendingNoteOffs(generatedMessages, elapsedSamplesAtStart, elapsedSamplesAtEnd);
  pb_informGuiOfOutgoing(generatedMessages);

  midiMessages.clear();
  midiMessages.addEvents(generatedMessages, generatedMessages.getFirstEventTime(), -1, 0);

  pb_applyPlayProbability(midiMessages);
  pb_logMidiEvents(midiMessages);

  allOff = pb_handlePlayingState(midiMessages, hostAllowsPlayback, allOff);

  pb_handleStuckNotes(midiMessages, elapsedSamplesAtEnd);
  pb_sendPendingAllNotesOff(midiMessages, allOff);

  elapsedSamples = elapsedSamplesAtEnd;
  lastHostTransportPlaying = hostClockEnabled && hostInfo.transportKnown ? hostInfo.transportPlaying : false;
  lastProcessBlockSampleCount = buffer.getNumSamples();
  havePreviousBlockInfo = true;
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

bool MidiMarkovProcessor::pullClockTickForGUI(uint32_t& lastSeenStamp)
{
    const auto s = lastClockTickStamp.load(std::memory_order_acquire);
    if (s == lastSeenStamp)
        return false;

    lastSeenStamp = s;
    return true;
}

void MidiMarkovProcessor::pushClockTickForGUI()
{
    lastClockTickStamp.fetch_add(1, std::memory_order_release);
}

void MidiMarkovProcessor::requestBpmAdjust(int step)
{
    if (step == 0)
        return;

    auto* param = quantBpmParamObject;
    if (param == nullptr)
        return;

    const juce::SpinLock::ScopedLockType lock(bpmAdjustLock);
    const float current = param->get();
    const auto& range = param->getNormalisableRange();
    const float newValue = juce::jlimit(range.start, range.end, current + static_cast<float>(step));
    if (juce::approximatelyEqual(current, newValue))
        return;

    param->beginChangeGesture();
    param->setValueNotifyingHost(param->convertTo0to1(newValue));
    param->endChangeGesture();
}

double MidiMarkovProcessor::calculateClockSamplesPerTick(double sampleRate) const
{
    if (sampleRate <= 0.0)
        return 0.0;

    const double bpmParam = quantBPMParam != nullptr ? static_cast<double>(quantBPMParam->load())
                                                     : 120.0;
    const double bpm = juce::jlimit(20.0, 300.0, bpmParam);

    const double divisionParam = quantDivisionParam != nullptr ? static_cast<double>(quantDivisionParam->load())
                                                               : 1.0;
    const double divisionValue = static_cast<double>(ImproviserControlGUI::divisionIdToValue(static_cast<int>(divisionParam)));
    const double safeDivision = juce::jmax(0.001, divisionValue);

    const double secondsPerBeat = 60.0 / bpm;
    const double secondsPerDivision = secondsPerBeat * safeDivision;
    const double samplesPerDivision = secondsPerDivision * sampleRate;
    return juce::jmax(1.0, samplesPerDivision);
}

double MidiMarkovProcessor::calculateHostClockSamplesPerTick(const HostClockInfo& info) const
{
    if (!info.hostClockEnabled || !info.hasBpm)
        return 0.0;

    const double sampleRate = getSampleRate();
    if (sampleRate <= 0.0)
        return 0.0;

    const double divisionParam = quantDivisionParam != nullptr ? static_cast<double>(quantDivisionParam->load())
                                                               : 1.0;
    const double divisionValue = static_cast<double>(ImproviserControlGUI::divisionIdToValue(static_cast<int>(divisionParam)));
    const double safeDivision = juce::jmax(0.001, divisionValue);

    const double safeBpm = juce::jmax(1.0, info.bpm);
    const double secondsPerBeat = 60.0 / safeBpm;
    const double secondsPerDivision = secondsPerBeat * safeDivision;
    const double samplesPerDivision = secondsPerDivision * sampleRate;
    return juce::jmax(1.0, samplesPerDivision);
}

std::optional<unsigned long> MidiMarkovProcessor::computeNextInternalTickSample() const
{
    const double sampleRate = getSampleRate();
    if (sampleRate <= 0.0)
        return std::nullopt;

    double interval = clockSamplesPerTick;
    if (interval <= 0.0)
        interval = calculateClockSamplesPerTick(sampleRate);

    if (interval <= 0.0 || !std::isfinite(interval))
        return std::nullopt;

    double accumulated = clockSamplesAccumulated;
    if (!std::isfinite(accumulated) || accumulated < 0.0)
        accumulated = 0.0;
    if (accumulated >= interval)
        accumulated = std::fmod(accumulated, interval);

    double samplesUntilTick = interval - accumulated;
    if (!std::isfinite(samplesUntilTick) || samplesUntilTick <= 0.0)
        samplesUntilTick = interval;

    const auto deltaSamples = static_cast<unsigned long>(std::ceil(samplesUntilTick));
    return elapsedSamples + deltaSamples;
}

std::optional<unsigned long> MidiMarkovProcessor::computeNextHostTickSample(const HostClockInfo& info) const
{
    if (!info.hostClockEnabled || !info.hasPpq || !info.hasBpm)
        return std::nullopt;

    const double sampleRate = getSampleRate();
    if (sampleRate <= 0.0)
        return std::nullopt;

    const double divisionParam = quantDivisionParam != nullptr ? static_cast<double>(quantDivisionParam->load())
                                                               : 1.0;
    const double ppqPerTick = juce::jmax(1.0e-5,
        static_cast<double>(ImproviserControlGUI::divisionIdToValue(static_cast<int>(divisionParam))));

    if (ppqPerTick <= 0.0 || !std::isfinite(ppqPerTick))
        return std::nullopt;

    const double ticksElapsed = std::floor(info.ppqPosition / ppqPerTick);
    const double nextTickPpq = (ticksElapsed + 1.0) * ppqPerTick;
    double deltaPpq = nextTickPpq - info.ppqPosition;
    if (!std::isfinite(deltaPpq) || deltaPpq <= 0.0)
        deltaPpq = ppqPerTick;

    const double secondsPerBeat = info.bpm > 0.0 ? (60.0 / info.bpm) : 0.0;
    if (secondsPerBeat <= 0.0)
        return std::nullopt;

    const double secondsUntilTick = deltaPpq * secondsPerBeat;
    if (!std::isfinite(secondsUntilTick) || secondsUntilTick < 0.0)
        return std::nullopt;

    const auto samplesUntilTick = secondsUntilTick * sampleRate;
    const auto deltaSamples = static_cast<unsigned long>(std::ceil(samplesUntilTick));
    // DBG("BPM " << info.bpm << " Secs per beat " << secondsPerBeat << " secs to tick " << secondsUntilTick << " samples to tick " << samplesUntilTick);
    return elapsedSamples + deltaSamples;
}

void MidiMarkovProcessor::alignModelPlayTimeToNextTick(bool hostClockEnabled, const HostClockInfo& info)
{
    std::optional<unsigned long> nextTick = hostClockEnabled
        ? computeNextHostTickSample(info)
        : computeNextInternalTickSample();

    if (nextTick.has_value())
        nextTimeToPlayANote = *nextTick;
    else
        nextTimeToPlayANote = elapsedSamples;
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
          int iOI = static_cast<int>(exactNoteOnTime - lastIncomingNoteOnTime);
          if (iOI < getSampleRate() * 2 && 
              iOI > getSampleRate() * 0.05){
            if (quantBlockSizeSamples != 0){// quantise it
              // DBG("analyseIoI quant block " << quantBlockSizeSamples << " quant from " << iOI << " to " << MidiMarkovProcessor::quantiseInterval(iOI, quantBlockSizeSamples));

              iOI = MidiMarkovProcessor::quantiseInterval(iOI, quantBlockSizeSamples);
              if (iOI == 0) iOI = quantBlockSizeSamples;
            }
            if (iOI > 0){// ignore zero iois
              iOIModel.putEvent(std::to_string(iOI));
            }   

          }
          lastIncomingNoteOnTime = exactNoteOnTime; 
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

void MidiMarkovProcessor::syncNextTimeToClock(const HostClockInfo& info)
{
    const double tickLength = clockSamplesPerTick;
    if (tickLength <= 0.0 || !std::isfinite(tickLength))
        return;

    const auto nextTickSample = info.hostClockEnabled
        ? computeNextHostTickSample(info)
        : computeNextInternalTickSample();

    if (!nextTickSample.has_value())
        return;

    const double target = static_cast<double>(*nextTickSample);
    const double diff = static_cast<double>(nextTimeToPlayANote) - target;
    double remainder = std::fmod(diff, tickLength);
    if (!std::isfinite(remainder))
        return;

    if (remainder < 0.0)
        remainder += tickLength;

    const double epsilon = 1.0e-4;
    if (remainder <= epsilon || std::abs(tickLength - remainder) <= epsilon)
        return;

    const double adjustment = tickLength - remainder;
    const auto adjustmentSamples = static_cast<long long>(std::llround(adjustment));
    if (adjustmentSamples > 0)
        nextTimeToPlayANote += static_cast<unsigned long>(adjustmentSamples);
}

juce::MidiBuffer MidiMarkovProcessor::generateNotesFromModel(const juce::MidiBuffer& incomingNotes, unsigned long bufferStartTime, unsigned long bufferEndTime, const HostClockInfo& hostInfo)
{
  juce::MidiBuffer generatedMessages{};
  if (pitchModel.getModelSize() < 2){// only play once we've got something!
    return generatedMessages;
  }

  unsigned long nextIoI = 0;
  // dicates if we use the incoming midi as context
  // or the previous model output as context
  bool inputIsContextMode = !leadFollowParam->load();
 unsigned long noteOnTime{0};
  if (isTimeToPlayNote(bufferStartTime, bufferEndTime)){
    if (!noMidiYet){ // not in bootstrapping phase 
      std::string notes = pitchModel.getEvent(true, inputIsContextMode);
      unsigned long duration = std::stoul(noteDurationModel.getEvent(true, inputIsContextMode));
      juce::uint8 velocity = std::stoi(velocityModel.getEvent(true, inputIsContextMode));
      noteOnTime = nextTimeToPlayANote - bufferStartTime; 
      // DBG("model wants note at "<< modelPlayNoteTime << " buffer starts at " << bufferStartTime << " boffset " << noteOnTime);

      // DBG("Note on time " << noteOnTime);
      // jassert(noteOnTime >= bufferStartTime && noteOnTime < bufferEndTime);
      if (noteOnTime >= 0){// valid note on time
        // DBG("got note on time [offset in buffer]" << noteOnTime << " added to buffer start = " << bufferStartTime);

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

            // ptocess Block deals with note offs - we just peg em here 
            // but if this note is already playing
            // then to avoid a double trigger/ note hold problem
            // we need to add a note off to generatedmessage
            if (noteOffTimes[note] > 0){// already playing this note
              // force a note off at frame zero in the next frame
              juce::MidiMessage nOff = juce::MidiMessage::noteOff(1, note);
              generatedMessages.addEvent(nOff, 0);// send note off at the start of the block
        
              if (noteOnTime < 5){// ensure we have at least 5 samples before the next note on
                noteOnTime = 5; 
              }
            } 
            generatedMessages.addEvent(nOn, noteOnTime);// note to be played in this block

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
      lastOutgoingNoteOnTime = nextTimeToPlayANote; // satore the last one 
      // elapsedSamples is the 'start of the buffer' 
      nextTimeToPlayANote = bufferStartTime + nextIoI + noteOnTime;
      syncNextTimeToClock(hostInfo);
      // DBG("Next IOI " << nextIoI << " since last one " << (nextTimeToPlayANote -lastOutgoingNoteOnTime) << " buff " << getBlockSize());

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
  if (nextTimeToPlayANote < windowStartTime) {
    // shift the start time along to bootstrap
    // playback. Weird but necessary
    nextTimeToPlayANote = windowEndTime;// force the note play time on. eventually we'll want to play, right? 
    return false;  
  }
  if (nextTimeToPlayANote >= windowStartTime && nextTimeToPlayANote < windowEndTime){
    // DBG("time to play: [ win s "<< windowStartTime << " m: " << nextTimeToPlayANote << " win e " << windowEndTime << " ]");
    
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

void MidiMarkovProcessor::getEffectiveBpmForDisplay(float& bpm, bool& isHostClock) const
{
    bpm = effectiveBpmForDisplay.load(std::memory_order_relaxed);
    isHostClock = effectiveBpmIsHost.load(std::memory_order_relaxed);
}

// impro control listener interface

void MidiMarkovProcessor::resetModel()
{
  suspendProcessing(true);

  DBG("Proc: reset model");

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
  return loadModelBinary(filename);
}

bool MidiMarkovProcessor::saveModel(std::string filename)
{
  return saveModelBinary(filename);
}

bool MidiMarkovProcessor::loadModelString(const std::string& filename)
{
  if (std::ifstream in{filename})
  {
    std::ostringstream sstr{};
    sstr << in.rdbuf();
    std::string data = sstr.str();
    in.close();

    std::vector<std::string> modelStrings = MarkovChain::tokenise(data, FILE_SEP_FOR_SAVE);
    std::vector<MarkovManager*> managers = {
        &pitchModel, &polyphonyModel, &iOIModel, &noteDurationModel, &velocityModel};

    if (modelStrings.size() != managers.size())
    {
      DBG("DinvernoPolyMarkov::loadModel did not find " << managers.size()
          << " model strings in file " << filename);
      return false;
    }

    for (size_t i = 0; i < managers.size(); ++i)
    {
      if (!managers[i]->setupModelFromString(modelStrings[i]))
      {
        DBG("DinvernoPolyMarkov::loadModel error loading model " << i << " from " << filename);
        return false;
      }

      DBG("DinvernoPolyMarkov::loadModel loaded model " << i << " from " << filename);
    }

    return true;
  }

  std::cout << "DinvernoPolyMarkov::loadModel failed to load from file " << filename << std::endl;
  return false;
}

bool MidiMarkovProcessor::saveModelString(const std::string& filename)
{
  std::string data;
  std::vector<MarkovManager*> managers = {
      &pitchModel, &polyphonyModel, &iOIModel, &noteDurationModel, &velocityModel};

  for (MarkovManager* mm : managers)
  {
    data += FILE_SEP_FOR_SAVE;
    data += mm->getModelAsString();
  }

  if (std::ofstream ofs{filename})
  {
    ofs << data;
    ofs.close();
    return true;
  }

  std::cout << "DinvernoPolyMarkov::saveModel failed to save to file " << filename << std::endl;
  return false;
}

bool MidiMarkovProcessor::saveModelBinary(const std::string& filename)
{
  std::vector<MarkovManager*> managers = {
      &pitchModel, &polyphonyModel, &iOIModel, &noteDurationModel, &velocityModel};

  std::string blob;
  appendUint32(blob, static_cast<uint32_t>(managers.size()));

  for (size_t i = 0; i < managers.size(); ++i)
  {
    std::string modelData = managers[i]->getModelAsBinaryString();

    if (modelData.size() > std::numeric_limits<uint32_t>::max())
    {
      std::cout << "DinvernoPolyMarkov::saveModelBinary model " << i << " too large to serialise"
                << std::endl;
      return false;
    }

    appendUint32(blob, static_cast<uint32_t>(modelData.size()));
    blob.append(modelData);
  }

  if (std::ofstream ofs{filename, std::ios::binary})
  {
    ofs.write(blob.data(), static_cast<std::streamsize>(blob.size()));
    ofs.close();
    return true;
  }

  std::cout << "DinvernoPolyMarkov::saveModelBinary failed to save to file " << filename
            << std::endl;
  return false;
}

bool MidiMarkovProcessor::loadModelBinary(const std::string& filename)
{
  std::ifstream in{filename, std::ios::binary};
  if (!in)
  {
    std::cout << "DinvernoPolyMarkov::loadModelBinary failed to open file " << filename << std::endl;
    return false;
  }

  std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
  in.close();

  size_t offset = 0;
  uint32_t entryCount = 0;

  if (!readUint32(data, offset, entryCount))
  {
    std::cout << "DinvernoPolyMarkov::loadModelBinary missing entry count header" << std::endl;
    return false;
  }

  std::vector<MarkovManager*> managers = {
      &pitchModel, &polyphonyModel, &iOIModel, &noteDurationModel, &velocityModel};

  if (entryCount != managers.size())
  {
    // DBG("DinvernoPolyMarkov::loadModelBinary expected " << managers.size()
        // << " entries but file contains " << entryCount);
    return false;
  }

  for (size_t i = 0; i < managers.size(); ++i)
  {
    uint32_t length = 0;
    if (!readUint32(data, offset, length))
    {
      DBG("DinvernoPolyMarkov::loadModelBinary failed to read length for model " << i);
      return false;
    }

    if (offset + length > data.size())
    {
      DBG("DinvernoPolyMarkov::loadModelBinary truncated data for model " << i);
      return false;
    }

    std::string modelData(data.data() + offset, length);
    offset += length;

    if (!managers[i]->setupModelFromBinaryString(modelData))
    {
      DBG("DinvernoPolyMarkov::loadModelBinary error loading model " << i << " from " << filename);
      return false;
    }

    DBG("DinvernoPolyMarkov::loadModelBinary loaded model " << i << " from " << filename);
  }

  return true;
}
MidiMarkovProcessor::HostClockInfo MidiMarkovProcessor::pb_collectHostClockInfo(bool hostClockEnabled)
{
    HostClockInfo info;
    info.hostClockEnabled = hostClockEnabled;

    
    if (!hostClockEnabled)
    {
        hostLastKnownTimeInSamples.reset();
        hostLastKnownPpqPosition.reset();
        hostLastKnownWasPlaying = false;
        return info;
    }

    if (auto* playHead = getPlayHead())
    {
        if (auto playPos = playHead->getPosition())
        {
            info.transportPlaying = playPos->getIsPlaying();
            if (!info.transportPlaying)
                info.transportPlaying = playPos->getIsRecording();

            info.transportKnown = true;

            if (auto ppq = playPos->getPpqPosition())
            {
                info.hasPpq = true;
                info.ppqPosition = *ppq;
            }

            if (auto bpm = playPos->getBpm())
            {
                info.hasBpm = true;
                info.bpm = *bpm;
            }

            if (auto timeSamples = playPos->getTimeInSamples())
            {
                info.hasTimeInSamples = true;
                info.timeInSamples = static_cast<double>(*timeSamples);
            }
        }
    }

    if (info.transportKnown)
    {
        bool transportMoved = false;

        if (info.hasTimeInSamples && hostLastKnownTimeInSamples.has_value())
        {
            double expected = hostLastKnownTimeInSamples.value();
            if (hostLastKnownWasPlaying && info.transportPlaying && havePreviousBlockInfo)
                expected += static_cast<double>(lastProcessBlockSampleCount);

            const double toleranceSamples = (hostLastKnownWasPlaying || info.transportPlaying) ? 4.0 : 1.0;
            if (std::abs(info.timeInSamples - expected) > toleranceSamples)
                transportMoved = true;
        }
        else if (info.hasPpq && hostLastKnownPpqPosition.has_value())
        {
            double expected = hostLastKnownPpqPosition.value();
            if (info.transportPlaying && hostLastKnownWasPlaying && havePreviousBlockInfo && info.hasBpm)
            {
                if (const double sampleRate = getSampleRate(); sampleRate > 0.0)
                {
                    const double secondsSinceLastBlock =
                        static_cast<double>(lastProcessBlockSampleCount) / sampleRate;
                    expected += secondsSinceLastBlock * (info.bpm / 60.0);
                }
            }

            const double tolerancePpq = 1.0e-4;
            if (std::abs(info.ppqPosition - expected) > tolerancePpq)
                transportMoved = true;
        }

        info.transportPositionChanged = transportMoved;

        if (info.hasTimeInSamples)
            hostLastKnownTimeInSamples = info.timeInSamples;
        if (info.hasPpq)
            hostLastKnownPpqPosition = info.ppqPosition;
        hostLastKnownWasPlaying = info.transportPlaying;
    }

    return info;
}

void MidiMarkovProcessor::pb_handleMidiFromUI(juce::MidiBuffer& midiMessages)
{
    if (midiReceivedFromUI.getNumEvents() > 0)
    {
        midiMessages.addEvents(midiReceivedFromUI,
                               midiReceivedFromUI.getFirstEventTime(),
                               midiReceivedFromUI.getLastEventTime() + 1,
                               0);
        midiReceivedFromUI.clear();
    }
}

void MidiMarkovProcessor::pb_informGuiOfIncoming(const juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOnOrOff())
        {
            pushMIDIInForGUI(msg);
            break;
        }
    }
}

void MidiMarkovProcessor::pb_tickInternalClock(const juce::AudioBuffer<float>& buffer)
{
    if (const double sr = getSampleRate(); sr > 0.0)
    {
        const double newInterval = calculateClockSamplesPerTick(sr);
        if (newInterval > 0.0)
        {
            if (std::abs(newInterval - clockSamplesPerTick) > 0.5)
            {
                clockSamplesPerTick = newInterval;
                clockSamplesAccumulated = juce::jmin(clockSamplesAccumulated, clockSamplesPerTick);
            }

            clockSamplesAccumulated += static_cast<double>(buffer.getNumSamples());

            while (clockSamplesPerTick > 0.0 && clockSamplesAccumulated >= clockSamplesPerTick)
            {
                clockSamplesAccumulated -= clockSamplesPerTick;
                pushClockTickForGUI();
            }
        }
    }

    hostClockPositionInitialised = false;
    hostAwaitingFirstTick = false;
}

void MidiMarkovProcessor::pb_tickHostClock(bool transportPlaying, bool hostHasPpq, double hostPpqPosition)
{
    clockSamplesAccumulated = 0.0;

    if (transportPlaying && hostHasPpq)
    {
        const double divisionBeats = static_cast<double>(ImproviserControlGUI::divisionIdToValue(static_cast<int>(quantDivisionParam->load())));
        const double ppqPerTick = juce::jmax(1.0e-4, divisionBeats);

        if (!hostClockPositionInitialised)
        {
            hostClockPositionInitialised = true;
            hostClockLastPpq = hostPpqPosition;
        }

        double diff = hostPpqPosition - hostClockLastPpq;
        if (diff < 0.0)
        {
            hostClockLastPpq = hostPpqPosition;
            diff = 0.0;
        }

        while (diff >= ppqPerTick)
        {
            hostClockLastPpq += ppqPerTick;
            diff = hostPpqPosition - hostClockLastPpq;
            pushClockTickForGUI();
            if (hostAwaitingFirstTick)
            {
                hostAwaitingFirstTick = false;
            }
        }
    }
    else
    {
        hostClockPositionInitialised = false;
    }
}

void MidiMarkovProcessor::pb_learnFromIncomingMidi(const juce::MidiBuffer& midiMessages, double effectiveBpm)
{
    if (learningParam->load() <= 0.0f)
        return;

    unsigned long quantBlockSizeSamples = 0;
    if (quantiseParam->load() > 0.0f && effectiveBpm > 0.0)
    {
        const double division = ImproviserControlGUI::divisionIdToValue(static_cast<int>(quantDivisionParam->load()));
        const double bpm = juce::jmax(20.0, effectiveBpm);
        const double secondsPerBeat = 60.0 / bpm;
        quantBlockSizeSamples = static_cast<unsigned long>(getSampleRate() * (division * secondsPerBeat));
    }

    analysePitches(midiMessages);
    analyseDuration(midiMessages, quantBlockSizeSamples);
    analyseIoI(midiMessages, quantBlockSizeSamples);
    analyseVelocity(midiMessages);
}

void MidiMarkovProcessor::pb_schedulePendingNoteOffs(juce::MidiBuffer& buffer, unsigned long blockStart, unsigned long blockEnd)
{
    for (auto i = 0; i < 127; ++i)
    {
        if (noteOffTimes[i] > blockStart && noteOffTimes[i] < blockEnd)
        {
            const auto noteSampleOffset = static_cast<int>(noteOffTimes[i] - blockStart);
            buffer.addEvent(juce::MidiMessage::noteOff(1, i, 0.0f), noteSampleOffset);
            noteOffTimes[i] = 0;
        }
    }
}

void MidiMarkovProcessor::pb_informGuiOfOutgoing(const juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOnOrOff())
        {
            pushMIDIOutForGUI(msg);
            break;
        }
    }
}

void MidiMarkovProcessor::pb_applyPlayProbability(juce::MidiBuffer& midiMessages)
{
    if (playProbabilityParam->load() >= 1.0f || midiMessages.getNumEvents() == 0)
        return;

    juce::MidiBuffer filtered;
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        if (msg.isNoteOn())
        {
            if (juce::Random::getSystemRandom().nextDouble() < playProbabilityParam->load())
                filtered.addEvent(msg, metadata.samplePosition);
        }
        else
        {
            filtered.addEvent(msg, metadata.samplePosition);
        }
    }

    midiMessages.swapWith(filtered);
}

void MidiMarkovProcessor::pb_logMidiEvents(const juce::MidiBuffer& midiMessages)
{
    for (const auto metadata : midiMessages)
    {
        auto msg = metadata.getMessage();
        midiMonitor.eventWasAddedToBuffer(msg, elapsedSamples + static_cast<unsigned long>(metadata.samplePosition));
    }
}

bool MidiMarkovProcessor::pb_handlePlayingState(juce::MidiBuffer& midiMessages, bool hostAllowsPlayback, bool allOffRequested)
{
    const bool playingParamEnabled = playingParam->load() == 1.0f;
    const bool shouldPlay = playingParamEnabled && hostAllowsPlayback;

    if (shouldPlay)
    {
        if (!lastPlayingParamState.load())
            lastPlayingParamState.store(true);
        return allOffRequested;
    }

    midiMessages.clear();
    if (lastPlayingParamState.load())
    {
        lastPlayingParamState.store(false);
        return true;
    }

    return allOffRequested;
}

void MidiMarkovProcessor::pb_handleStuckNotes(juce::MidiBuffer& midiMessages, unsigned long elapsedSamplesAtEnd)
{
    std::vector<int> stuckNotes = midiMonitor.getStuckNotes(elapsedSamplesAtEnd);
    for (auto note : stuckNotes)
    {
        midiMessages.addEvent(juce::MidiMessage::noteOff(1, note), 0);
        midiMonitor.unstickNote(note);
    }
}

void MidiMarkovProcessor::pb_sendPendingAllNotesOff(juce::MidiBuffer& midiMessages, bool allOffRequested)
{
    if (!allOffRequested)
        return;

    midiMessages.clear();
    midiReceivedFromUI.clear();

    DBG("Processor sending all notes off.");
    sendMidiPanic(midiMessages, 0);
    sendAllNotesOffNext.store(false, std::memory_order_relaxed);
}
