/*
  ==============================================================================

    ChordDetector.h
    Created: 31 Oct 2019 6:03:02pm
    Author:  matthew

  ==============================================================================
*/
#include <vector>

#pragma once

/**
 * Class that can tell you if you have chords coming in or note.
 * It measures the time between notes and stores the notes up ready 
 * to send back a chord
 */
class ChordDetector{
  public: 
  /** send it the sample rate*/
    ChordDetector(int sampleRate);
    ~ChordDetector();
    /** tell it a note was played */
    void notePlayed(int note, double timeInSamples);
    /** tell it a cc was 'played' */
    void ccPlayed(int cc, int value, double timeInSamples);
   
    /** get an appropriate set of notes 
     * could be a chord (if last received note was long enough apart from multiple, close together previos notes) 
     * or a single note (if last received note was long enough apart from the note before that)
     *  or no notes (if the last received note was close to the note before that, i.e. collecting a chord)
    */
    std::vector<int> getReadyNotes();
    /** get an appropriate set of ccs, similar logic to getReadyNotes  */
    std::vector<std::pair<int,int>> getReadyCCs();
    /** wipe all memory*/
    void reset();

  private:
    std::vector<int> storedNotes;
    std::vector<int> notesForReturn;
    std::vector<std::pair<int,int>> storedCCs;
    std::vector<std::pair<int,int>> ccsForReturn;
    
     
    double lastNoteTimeInSamples;
    double sampleRate;
};