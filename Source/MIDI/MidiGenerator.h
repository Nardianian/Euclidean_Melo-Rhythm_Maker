/*
  ==============================================================================

    MidiGenerator.h
    Created: 9 Oct 2022 9:09:12pm
    Author:  Andreas Sandersen
    Modified: 14 Dec. 2025 16:29:00pm by Nard.
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>
#include "Euclidean.h"
#include "Arp.h"

class MidiGenerator
{
public:
    MidiGenerator()
    {
        rhythmPatterns.resize(6);
        rhythmArps.resize(6);
        arpNotes.resize(6);
    }

    std::vector<EuclideanPattern> rhythmPatterns;
    std::vector<Arp> rhythmArps;
    std::vector<std::vector<int>> arpNotes;

    bool triggerStep(int rhythmIndex, int& outMidiNote)
    {
        if (rhythmIndex < 0 || rhythmIndex >= rhythmPatterns.size())
            return false;

        auto& pattern = rhythmPatterns[rhythmIndex];

        // Avanza il pattern euclideo (1 step)
        if (!pattern.nextStep())
            return false;

        auto& arp = rhythmArps[rhythmIndex];
        auto& notes = arpNotes[rhythmIndex];

        if (notes.empty())
        {
            outMidiNote = 60;
            return true;
        }

        int noteIndex = arp.getCurrentStep();
        noteIndex %= notes.size();
        outMidiNote = notes[noteIndex];

        return true;
    }

    bool advanceArp(int rhythmIndex, double samplesPerStep, double samplesPerArpStep)
    {
        if (rhythmIndex < 0 || rhythmIndex >= rhythmArps.size())
            return false;

        return rhythmArps[rhythmIndex].advance(samplesPerStep, samplesPerArpStep);
    }
};
#pragma once
