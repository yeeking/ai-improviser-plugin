#include "SlomoStrategy.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kTargetSeconds = 0.5;   // pivot between "short" and "long"
constexpr double kMinSeconds    = 0.01;  // avoid division by zero
constexpr double kMinScale      = 0.25;
constexpr double kMaxScale      = 4.0;
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
