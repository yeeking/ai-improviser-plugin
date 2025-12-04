#include "Behaviours.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPivotNote = 60.0; // middle C
constexpr double kOctaveSemitones = 12.0;

constexpr double kTargetSeconds = 0.5;   // pivot between "short" and "long"
constexpr double kMinSeconds    = 0.01;  // avoid division by zero
constexpr double kMinScale      = 0.25;
constexpr double kMaxScale      = 4.0;
}

AvoidStrategy::AvoidStrategy(std::size_t capacity)
    : bufferCapacity(capacity),
      buffer(capacity, 0)
{
}

bool AvoidStrategy::addNote(int noteNumber)
{
    if (bufferCapacity == 0)
        return false;

    if (noteNumber < 0 || noteNumber > 127)
        return false;

    buffer[writeIndex] = noteNumber;
    writeIndex = (writeIndex + 1u) % bufferCapacity;
    if (count < bufferCapacity)
        ++count;

    return recompute();
}

bool AvoidStrategy::recompute()
{
    if (count == 0)
    {
        bool changed = (transposition != 0);
        transposition = 0;
        return changed;
    }

    const int previous = transposition;

    std::vector<int> window(buffer.begin(), buffer.begin() + static_cast<long>(count));
    std::sort(window.begin(), window.end());

    const std::size_t mid = count / 2;
    double median = 0.0;
    if (count % 2 == 0 && count > 1)
        median = (static_cast<double>(window[mid - 1]) + static_cast<double>(window[mid])) / 2.0;
    else
        median = static_cast<double>(window[mid]);

    double sum = 0.0;
    for (std::size_t i = 0; i < count; ++i)
        sum += static_cast<double>(buffer[i]);

    const double mean = sum / static_cast<double>(count);
    double variance = 0.0;
    for (std::size_t i = 0; i < count; ++i)
    {
        const double diff = static_cast<double>(buffer[i]) - mean;
        variance += diff * diff;
    }
    variance /= static_cast<double>(count);
    const double stdDev = std::sqrt(variance);

    const int direction = (median >= kPivotNote) ? -1 : 1;
    const int magnitude = (stdDev >= kOctaveSemitones) ? 24 : 12;
    transposition = direction * magnitude;
    return transposition != previous;
}

SlomoStrategy::SlomoStrategy(std::size_t capacity)
    : bufferCapacity(std::max<std::size_t>(1, capacity)),
      buffer(bufferCapacity, kTargetSeconds)
{
}

void SlomoStrategy::addIoiSeconds(double seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0.0)
        return;

    buffer[writeIndex] = seconds;
    writeIndex = (writeIndex + 1) % bufferCapacity;
    if (count < bufferCapacity)
        ++count;
}

void SlomoStrategy::addIoiSamples(int samples, double sampleRate)
{
    if (sampleRate <= 0.0)
        return;

    const double seconds = static_cast<double>(samples) / sampleRate;
    addIoiSeconds(seconds);
}

double SlomoStrategy::getComplementaryMultiplier() const
{
    if (count == 0)
        return 1.0;

    double sum = 0.0;
    for (std::size_t i = 0; i < count; ++i)
        sum += buffer[i];

    const double avgSeconds = sum / static_cast<double>(count);
    const double multiplier = kTargetSeconds / std::max(kMinSeconds, avgSeconds);
    return std::clamp(multiplier, kMinScale, kMaxScale);
}

void CallResponseEngine::reset()
{
    energy = 0.0;
    lastInputSample = 0;
    inResponse = false;
    enteredResponseThisBlock = false;
    sawNoteOn = false;
    noteOnCount = 0;
    velocitySum = 0.0;
    blockDurationSeconds = 0.0;
    bufferStartSample = 0;
    bufferEndSample = 0;
    sampleRate = 0.0;
}

void CallResponseEngine::setEnabled(bool shouldEnable)
{
    enabled = shouldEnable;
    if (!enabled)
        reset();
}

void CallResponseEngine::startBlock(unsigned long bufferStart, unsigned long bufferEnd, double sr)
{
    enteredResponseThisBlock = false;
    sawNoteOn = false;
    noteOnCount = 0;
    velocitySum = 0.0;
    blockDurationSeconds = 0.0;

    bufferStartSample = bufferStart;
    bufferEndSample = bufferEnd;
    sampleRate = sr;
    if (sampleRate > 0.0)
        blockDurationSeconds = static_cast<double>(bufferEndSample - bufferStartSample) / sampleRate;
}

void CallResponseEngine::registerIncomingNoteOn(float velocity01, unsigned long absoluteSample)
{
    if (!enabled)
        return;

    sawNoteOn = true;
    ++noteOnCount;
    velocitySum += std::clamp(static_cast<double>(velocity01), 0.0, 1.0);
    lastInputSample = std::max(lastInputSample, absoluteSample);
}

void CallResponseEngine::endBlock()
{
    if (!enabled || sampleRate <= 0.0)
    {
        return;
    }

    if (sawNoteOn)
    {
        const double notesPerSecond = blockDurationSeconds > 0.0
            ? static_cast<double>(noteOnCount) / blockDurationSeconds
            : 0.0;
        const double energyGain = (velocitySum + notesPerSecond) * gainFactor;
        energy = std::clamp(energy + energyGain, 0.0, kMaxEnergy);
        inResponse = false; // stay silent during user call
    }
    else
    {
        const unsigned long silenceThresholdSamples =
            static_cast<unsigned long>(sampleRate * silenceSeconds);
        const bool silenceElapsed = (bufferEndSample > lastInputSample)
            && ((bufferEndSample - lastInputSample) >= silenceThresholdSamples);

        if (silenceElapsed && energy > 0.0 && !inResponse)
        {
            inResponse = true;
            enteredResponseThisBlock = true;
        }
    }
}

void CallResponseEngine::applyDrainForGenerated(double blockDurationSec,
                                                int generatedNoteOns,
                                                double generatedVelocitySum)
{
    if (!inResponse || !enabled)
        return;

    double drain = blockDurationSec * passiveDrainPerSecond;
    drain += generatedNoteOns * kNoteDrainBase;
    drain += std::clamp(generatedVelocitySum, 0.0, static_cast<double>(generatedNoteOns));

    energy = std::max(0.0, energy - drain);
    if (energy <= 0.0)
        inResponse = false;
}

float CallResponseEngine::getEnergy01() const noexcept
{
    return static_cast<float>(std::clamp(energy / kMaxEnergy, 0.0, 1.0));
}
