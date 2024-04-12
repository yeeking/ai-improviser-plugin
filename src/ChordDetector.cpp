/*
  ==============================================================================

    ChordDetector.cpp
    Created: 31 Oct 2019 6:03:02pm
    Author:  matthew

  ==============================================================================
*/

#include "ChordDetector.h"
#include <JuceHeader.h>
#include <iostream>

ChordDetector::ChordDetector(int sampleRate) : sampleRate(sampleRate)
{

}

ChordDetector::~ChordDetector()
{

}
void ChordDetector::reset()
{
  notesForReturn.clear();
  storedNotes.clear();
  ccsForReturn.clear();
  storedCCs.clear();
}

void ChordDetector::notePlayed(int note, double timeInSamples)
{
 // DBG("ChordDetector::notePlayed: notes " + std::to_string(storedNotes.size()));
  double maxElapsed = sampleRate / 40; // 1/40 seems to work reasonably well for chords
  double elapsed = timeInSamples - lastNoteTimeInSamples;
  //DBG("ChordDetector::notePlayed elapsed " + std::to_string(elapsed) + " max " + std::to_string(maxElapsed));
  if (elapsed > maxElapsed) // no longer a chord as too long as passed between notes
  {
    //DBG("ChordDetector::notePlayed: chord ends " + std::to_string(storedNotes.size()));

    // individual note
    // prepare the previously collected notes for returning 
    // might be no collected notes, that's fine
    notesForReturn.clear();
    for (int storedNote : storedNotes){
      notesForReturn.push_back(storedNote);
    }
    storedNotes.clear();
  }
  // store the note
  storedNotes.push_back(note);
  lastNoteTimeInSamples = timeInSamples;
}

void ChordDetector::ccPlayed(int number, int value, double timeInSamples)
{
  if (timeInSamples - lastNoteTimeInSamples > (sampleRate / 16)) // 1/16 of a second
  {
    // individual note
    // prepare the previously collected notes for returning 
    // might be no collected notes, that's fine
    ccsForReturn.clear();
    for (std::pair<int, int>& cc : storedCCs)
    {
      // only add old value if not same cc number as incoming
      // this means the new value overrides the stored
      // value.
      if (cc.first != number) ccsForReturn.push_back(cc); 
    }  
    storedCCs.clear();
  }
  storedCCs.push_back(std::pair<int, int>{number, value});
  lastNoteTimeInSamples = timeInSamples;
}



std::vector<int> ChordDetector::getReadyNotes()
{
  //for (const int& n : notesForReturn) std::cout << "ChordDetector::getReadyNotes has note " << n << std::endl;
  if (notesForReturn.size() > 0){// we hvae notes to return!
    std::vector<int> retVal{notesForReturn};
    //std::cout << "ChordDetector::getReadyNotes: notes " << retVal.size() << std::endl; 
    notesForReturn.clear(); // clear it as we've given them back now
    return retVal;
  }
  else{ // nothing ready to return yet - return empty vector
    std::vector<int> retVal{};
    return retVal;
  }
}

std::vector<std::pair<int,int>> ChordDetector::getReadyCCs()
{
  if (ccsForReturn.size() > 0){// we hvae notes to return!
    std::vector<std::pair<int,int>> retVal{ccsForReturn};
    //std::vector<int> retVal{notesForReturn};
    //std::cout << "ChordDetector::getReadyNotes: notes " << retVal.size() << std::endl; 
    ccsForReturn.clear(); // clear it as we've given them back now
    return retVal;
  }
  else{ // nothing ready to return yet - return empty vector
    std::vector<std::pair<int,int>> retVal{};
 //   std::vector<int> retVal{};
    return retVal;
  }
}


