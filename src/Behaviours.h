#pragma once

#include <vector>
#include <cstddef>
#include <algorithm>

/**
 * Maintains a rolling set of recent incoming notes and proposes a transposition
 * to steer away from the user's dominant register.
 */
class AvoidStrategy
{
public:
    explicit AvoidStrategy(std::size_t capacity = 12);

    /** Add a note-on pitch (0-127) into the rolling buffer.
        @return true if the transposition changed. */
    bool addNote(int noteNumber);

    /** Latest transposition in semitones based on recent input. */
    int getTransposition() const noexcept { return transposition; }

private:
    bool recompute();

    std::size_t bufferCapacity;
    std::vector<int> buffer;
    std::size_t writeIndex { 0 };
    std::size_t count { 0 };
    int transposition { 0 };
};

/**
 * Stores recent inter-onset intervals (in seconds) and provides a complementary
 * multiplier: short recent IOIs → longer generated timings, long IOIs → shorter.
 */
class SlomoStrategy
{
public:
    explicit SlomoStrategy(std::size_t capacity = 8);

    void addIoiSeconds(double seconds);
    void addIoiSamples(int samples, double sampleRate);

    /** Returns a scale factor to apply to IOIs/durations for contrast. */
    double getComplementaryMultiplier() const;

private:
    std::size_t bufferCapacity;
    std::vector<double> buffer;
    std::size_t writeIndex { 0 };
    std::size_t count { 0 };
};

/**
 * Tracks call/response energy and state. Feed incoming note-ons and block timing,
 * then query whether the engine is in the "response" phase and how much energy remains.
 */
class CallResponseEngine
{
public:
    void reset();
    void setEnabled(bool enabled);

    void startBlock(unsigned long bufferStart, unsigned long bufferEnd, double sampleRate);
    void registerIncomingNoteOn(float velocity01, unsigned long absoluteSample);
    void endBlock();

    void applyDrainForGenerated(double blockDurationSeconds, int generatedNoteOns, double generatedVelocitySum);

    bool isEnabled() const noexcept { return enabled; }
    bool isInResponse() const noexcept { return inResponse; }
    bool justEnteredResponse() const noexcept { return enteredResponseThisBlock; }
    double getEnergy() const noexcept { return energy; }
    float getEnergy01() const noexcept;
    void setSilenceSeconds(double value) { silenceSeconds = std::max(0.0, value); }
    void setPassiveDrainPerSecond(double value) { passiveDrainPerSecond = std::max(0.0, value); }
    void setGainFactor(double value) { gainFactor = std::max(0.0, value); }

private:
    static constexpr double kMaxEnergy = 20.0;              // cap for accumulated call energy
    static constexpr double kNoteDrainBase = 0.35;          // per-note energy cost during response

    double silenceSeconds { 0.3 };          // silence required before entering response
    double passiveDrainPerSecond { 1.0 };   // energy bleed per second while responding
    double gainFactor { 0.5 };              // scales incoming velocity + rate into energy gain

    double energy { 0.0 };
    unsigned long lastInputSample { 0 };
    bool inResponse { false };
    bool enabled { false };
    bool enteredResponseThisBlock { false };

    // Per-block accumulators
    bool sawNoteOn { false };
    int noteOnCount { 0 };
    double velocitySum { 0.0 };
    double blockDurationSeconds { 0.0 };
    unsigned long bufferStartSample { 0 };
    unsigned long bufferEndSample { 0 };
    double sampleRate { 0.0 };
};
