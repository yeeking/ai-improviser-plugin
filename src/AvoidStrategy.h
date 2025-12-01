#pragma once

#include <vector>

class AvoidStrategy
{
public:
    explicit AvoidStrategy(size_t capacity = 12);

    /** Add a note-on pitch (0-127) into the rolling buffer.
        @return true if the transposition changed. */
    bool addNote(int noteNumber);

    /** Latest transposition in semitones based on recent input. */
    int getTransposition() const noexcept { return transposition; }

private:
    bool recompute();

    size_t bufferCapacity;
    std::vector<int> buffer;
    size_t writeIndex { 0 };
    size_t count { 0 };
    int transposition { 0 };
};
