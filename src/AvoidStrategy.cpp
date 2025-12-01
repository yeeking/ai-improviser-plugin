#include "AvoidStrategy.h"

#include <algorithm>
#include <cmath>

namespace
{
constexpr double kPivotNote = 60.0; // middle C
constexpr double kOctaveSemitones = 12.0;
}

AvoidStrategy::AvoidStrategy(size_t capacity)
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

    const size_t mid = count / 2;
    double median = 0.0;
    if (count % 2 == 0 && count > 1)
        median = (static_cast<double>(window[mid - 1]) + static_cast<double>(window[mid])) / 2.0;
    else
        median = static_cast<double>(window[mid]);

    double sum = 0.0;
    for (size_t i = 0; i < count; ++i)
        sum += static_cast<double>(buffer[i]);

    const double mean = sum / static_cast<double>(count);
    double variance = 0.0;
    for (size_t i = 0; i < count; ++i)
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
