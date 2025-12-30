#include "MidiGenerator.h"

// ================= MIDI GENERATOR IMPLEMENTATION =================

// ===== SCALE / CHORD =====
std::vector<int> MidiGenerator::getScaleNotes(int scaleID) const
{
    switch (scaleID)
    {
    case 0: return { 60, 62, 64, 65, 67, 69, 71, 72 }; // C maggiore
    case 1: return { 62, 64, 65, 67, 69, 71, 72, 74 }; // D dorico
    default: return { 60 };
    }
}

std::vector<int> MidiGenerator::getChordNotes(int chordID) const
{
    switch (chordID)
    {
    case 0: return { 60, 64, 67 }; // C maggiore
    case 1: return { 57, 60, 64 }; // A minore
    default: return { 60 };
    }
}
// ===== HELPER: Update ARP notes based on scale or chord =====
void MidiGenerator::updateArpNotes(int scaleID, int chordID)
{
    std::vector<int> scaleNotes = getScaleNotes(scaleID);

    for (int r = 0; r < 6; ++r)
    {
        arpNotes[r].clear();

        for (size_t i = 0; i < scaleNotes.size() && i < 7; ++i)
            arpNotes[r].push_back(scaleNotes[i]);

        // Fill the rest with -1 if you always want to have 7 elements
        while (arpNotes[r].size() < 7)
            arpNotes[r].push_back(-1);

        arpEnabled[r].store(true);
    }
}

// ===== SET ARP NOTES (THREAD-SAFE LOGIC) =====
void MidiGenerator::setArpNotes(int rhythmIndex, const std::vector<int>& notes)
{
    if (rhythmIndex < 0 || rhythmIndex >= 6)
        return;

    arpNotes[rhythmIndex] = notes;

    // ===== RESET THREAD-SAFE MASK FOR NEW NOTES =====
    arpInputNotesMaskLow[rhythmIndex].store(0);
    arpInputNotesMaskHigh[rhythmIndex].store(0);

    // Always keep 7 items
    while (arpNotes[rhythmIndex].size() < 7)
        arpNotes[rhythmIndex].push_back(-1);
}

// ===== ADVANCE ARP FOR ONE LINE (WRAPPER THREAD-SAFE) =====
bool MidiGenerator::advanceArp(int rhythmIndex,
    double samplesPerStep,
    double samplesPerArpStep)
{
    if (rhythmIndex < 0 || rhythmIndex >= (int)rhythmArps.size())
        return false;

    if (!arpEnabled[rhythmIndex].load())
        return false;

    // Advance ARP (true if advanced step)
    bool stepAdvanced = rhythmArps[rhythmIndex].advance(samplesPerStep, samplesPerArpStep);

    // Read the current step via getNoteIndex()
    int arpStep = rhythmArps[rhythmIndex].getNoteIndex();
    std::vector<int>& slots = arpNotes[rhythmIndex];

    for (size_t i = 0; i < slots.size(); ++i)
    {
        if (i == arpStep % 7 && slots[i] >= 0)
        {
            int note = slots[i];
            if (note < 64)
                arpInputNotesMaskLow[rhythmIndex].fetch_or(1ULL << note);
            else
                arpInputNotesMaskHigh[rhythmIndex].fetch_or(1ULL << (note - 64));
        }
    }

    return stepAdvanced;
}
