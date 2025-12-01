#pragma once

#include <vector>
#include <cstddef>

/**
 * Stores recent inter-onset intervals (in seconds) and provides a complementary
 * multiplier: short recent IOIs → longer generated timings, long IOIs → shorter.
 */
class SlomoStrategy
{
public:
    explicit SlomoStrategy(std::size_t capacity = 64);

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
