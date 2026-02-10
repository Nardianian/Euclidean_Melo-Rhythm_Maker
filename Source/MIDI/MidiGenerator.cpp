#include "MidiGenerator.h"
#include "ClockManager.h"

// ================== SCALE DEFINITIONS ==================
static const std::vector<ScaleDef> scaleDefs =
{
    { "Z-DMixolydian", {2,4,6,7,9,11,0} },
    { "Z-GMixolydian", {7,9,11,0,2,4,5} },
    { "Z-PrygianDominant", {4,5,8,9,11,0,2} },
    { "Z-Ddorian", {2,4,5,7,9,10,0} },
    { "Z-Modified", {2,4,5,7,9,10,11,0} },
    { "Z-CHexatonic", {0,2,4,6,8,10} },
    { "Z-CDiminished", {0,1,3,4,6,7,9,10} },
    { "Z-CLydianDominant", {0,2,4,6,7,9,10} },
    { "Z-APentaMinor1", {9,0,2,4,7,3} },
    { "Z-APentaMinor2", {9,0,3,4,7} },
    { "Z-BlackPage", {0,1,4,7,8,10} },
    { "Z-St.Alfonso", {6,4,9,7,11,10,1} },
    { "C-Blues1", {0,3,5,6,7,10,0} },
    { "C-Blues2", {0,3,5,6,7,10} },
    { "B-Blues", {4,7,9,10,11,2,4} },
    { "A-Blues1", {9,0,2,3,4,7,9} },
    { "A-Blues2", {9,0,2,3,4,7,9} },
    { "G-Blues", {7,10,0,1,2,5,7} },
    { "Min-Jazz", {0,2,3,5,7,9,11,0} },
    { "C7-Jazz", {0,4,7,10} },
    { "Am7-Jazz", {9,0,4,7} },
    { "Dm7-Jazz", {2,5,9,0} },
    { "G7-Jazz", {7,11,2,5} },
    { "Cmaj7-Jazz", {0,4,7,11} },
    { "Fm7b5-Jazz", {5,8,11,3} },
    { "PentaMinor1", {0,3,5,7,10} },
    { "PentaMinor2", {9,0,2,4,7} },
    { "PentaMajor", {0,2,4,7,9} },
    { "Major", {0,2,4,5,7,9,11,0} },
    { "Minor", {0,2,3,5,7,8,10,0} },
    { "Lydian", {5,7,9,11,0,2,4,6} }
};

// ================== CHORD DEFINITIONS ==================
static const std::vector<ChordDef> chordDefs =
{
    { "Z-St.Alfonso1", {6,4,9,7,11,10,1} },
    { "Z-St.Alfonso2", {0,11,7} },
    { "Z-St.AlfonsoInversion", {0,1,4} },
    { "Z-Watermelon", {4,9,11} },
    { "Z-Inc", {0,4,7,11} },
    { "Z-Inseventh3", {10,0,1} },
    { "Gsus4", {0,2,7} },
    { "C-Major", {0,4,7} },
    { "1st-Inversion", {4,7,0} },
    { "2nd-Inversion", {7,0,4} },
    { "Augmented", {0,4,8} },
    { "Major 7", {0,4,7,11} },
    { "Minor 7", {0,3,7,10} },
    { "Diminished", {0,3,6} }
};

// ================= MIDI GENERATOR IMPLEMENTATION =================

// ===== SCALE / CHORD =====
std::vector<int> MidiGenerator::getScaleNotes(int scaleID) const
{
    // Each line uses its own root note
    if (scaleID < 0 || scaleID >= (int)scaleDefs.size())
        return { 60 };

    const auto& scale = scaleDefs[scaleID].intervals;
    std::vector<int> notes;

     // Find the root note for this specific instance only.
     // Note: Since getScaleNotes doesn't receive 'row' as an argument, you must either infer it or use a consistent default value.
    int rootNote = 60;

    // This function is called by getAvailableArpNotesForGUI(rhythmIndex) which passes us the correct index.

    for (int interval : scale)
        notes.push_back(interval); // Returns only relative ranges

    return notes;
}

std::vector<int> MidiGenerator::getChordNotes(int chordID) const
{
    if (chordID < 0 || chordID >= (int)chordDefs.size())
        return { 60 };

    const auto& chord = chordDefs[chordID].intervals;
    std::vector<int> notes;

    for (int interval : chord)
        notes.push_back(interval); // Returns relative ranges

    return notes;
}

std::vector<int> MidiGenerator::getAvailableArpNotesForGUI(int rhythmIndex) const
{
    if (rhythmIndex < 0 || rhythmIndex >= 6) return {};

    std::vector<int> availableNotes;
    if (inputNotes[rhythmIndex].empty()) return {};

    // Takes the base note and isolates its "pitch class" (C, C#, D...) to ensure that the octave is handled only by the Note parameter
    int root = inputNotes[rhythmIndex][0] % 12;
    int octaveOffset = (inputNotes[rhythmIndex][0] / 12) * 12;

    // Se stiamo usando una scala o accordo, generiamo le note reali
    if (selectedScale[rhythmIndex] >= 0 && selectedScale[rhythmIndex] < (int)scaleDefs.size())
    {
        for (int interval : scaleDefs[selectedScale[rhythmIndex]].intervals)
            availableNotes.push_back(octaveOffset + interval);
    }
    else if (selectedChord[rhythmIndex] >= 0 && selectedChord[rhythmIndex] < (int)chordDefs.size())
    {
        for (int interval : chordDefs[selectedChord[rhythmIndex]].intervals)
            availableNotes.push_back(octaveOffset + interval);
    }
    else
    {
        // If single note, use direct input notes
        availableNotes = inputNotes[rhythmIndex];
    }
    return availableNotes;
}

int MidiGenerator::getNoteFromScaleDegree(int degree, int rootNote, const std::vector<int>& scale) const
{
    if (scale.empty())
        return rootNote;

    int scaleSize = (int)scale.size();
    int octave = degree / scaleSize;
    int noteIndex = degree % scaleSize;

    return rootNote + octave * 12 + scale[noteIndex];
}

// ===== SET ARP INPUT NOTES (DO NOT TOUCH USER SELECTION) =====
void MidiGenerator::setArpInputNotes(int rhythmIndex, const std::vector<int>& notes)
{
    if (rhythmIndex < 0 || rhythmIndex >= 6)
        return;

    uint64_t lowMask = 0;
    uint64_t highMask = 0;

    for (int note : notes)
    {
        if (note < 0 || note > 127)
            continue;

        if (note < 64)
            lowMask |= (1ULL << note);
        else
            highMask |= (1ULL << (note - 64));
    }

    arpInputNotesMaskLow[rhythmIndex].store(lowMask);
    arpInputNotesMaskHigh[rhythmIndex].store(highMask);
}

// ===== SET ARP NOTES (USER SELECTION -> FILTER INPUT NOTES) =====
void MidiGenerator::setArpNotes(int rhythmIndex, const std::vector<int>& notes)
{
    if (rhythmIndex < 0 || rhythmIndex >= 6)
        return;

    std::lock_guard<std::mutex> lock(arpNotesMutex[rhythmIndex]);

    // === 1. Save ONLY max 4 notes selected by the user ===
    arpSelectedNotes[rhythmIndex].clear();
    for (size_t i = 0; i < notes.size() && i < 4; ++i)
        arpSelectedNotes[rhythmIndex].push_back(notes[i]);

    // === 2. Build actual ARP notes (selected ONLY) ===
    arpNotes[rhythmIndex].clear();
    for (int n : inputNotes[rhythmIndex])
    {
        if (std::find(arpSelectedNotes[rhythmIndex].begin(),
            arpSelectedNotes[rhythmIndex].end(),
            n) != arpSelectedNotes[rhythmIndex].end())
        {
            arpNotes[rhythmIndex].push_back(n);
        }
    }

    // === 3.Update internal ARP ===
    rhythmArps[rhythmIndex].setNumNotes((int)arpNotes[rhythmIndex].size());
    rhythmArps[rhythmIndex].reset();
}

void MidiGenerator::setArpNotesThreadSafe(int rhythmIndex, const std::vector<int>& notes)
{
    jassert(rhythmIndex >= 0 && rhythmIndex < 6);
    std::vector<int> newSelected;
    for (size_t i = 0; i < notes.size() && i < 5; ++i)
        newSelected.push_back(notes[i]);
    arpSelectedNotes[rhythmIndex] = newSelected;
    std::vector<int> newArpNotes;
    for (int n : inputNotes[rhythmIndex])
    {
        if (std::find(newSelected.begin(), newSelected.end(), n) != newSelected.end())
            newArpNotes.push_back(n);
    }
    // LOCK to update thread-safely
    {
        std::lock_guard<std::mutex> lock(arpNotesMutex[rhythmIndex]);
        arpNotes[rhythmIndex] = newArpNotes;
    }
    rhythmArps[rhythmIndex].setNumNotes((int)newArpNotes.size());
    rhythmArps[rhythmIndex].reset();
}

// Returns all notes in a row, considering note type and ARP selection
MidiGenerator::NoteInfo MidiGenerator::getNoteInfo(int row) const
{
    MidiGenerator::NoteInfo info;
    // Otteniamo le note trasposte reali che la riga sta effettivamente usando
    info.notes = const_cast<MidiGenerator*>(this)->getAvailableArpNotesForGUI(row);
    info.octave.clear();
    info.arpSelected.clear();

    for (size_t i = 0; i < info.notes.size(); ++i)
    {
        int note = info.notes[i];
        // JUCE/MIDI Standard: 60 = C3. // 60 / 12 = 5. To have 3 as a result: (note / 12) - 2.
        // If you prefer the Yamaha standard (60 = C4), use -1.
        int octave = (note / 12) - 2;
        info.octave.push_back(octave);

        bool selected = false;
        // Check if this specific note is present in the user's selection
        for (int sel : arpSelectedNotes[row])
        {
            if (sel == note)
            {
                selected = true;
                break;
            }
        }
        info.arpSelected.push_back(selected);
    }
    return info;
}

void MidiGenerator::setArpSelection(int row, const std::vector<bool>& selection)
{
    if (row < 0 || row >= 6) return;

    std::vector<int> available = getAvailableArpNotesForGUI(row);
    std::vector<int> newSelection;

    for (size_t i = 0; i < available.size() && i < selection.size(); ++i)
    {
        if (selection[i])
        {
            newSelection.push_back(available[i]);
            if (newSelection.size() >= 4) break;
        }
    }

    arpSelectedNotes[row] = newSelection;

    std::lock_guard<std::mutex> lock(arpNotesMutex[row]);
    // If the user does not select anything, the arpeggiator uses all available notes for the row.
    arpNotes[row] = newSelection.empty() ? available : newSelection;

    // Synchronize the Arp object with the correct number of notes
    rhythmArps[row].setNumNotes((int)arpNotes[row].size());
}

// ===== SET INPUT NOTES (SOURCE NOTES, DO NOT TOUCH USER SELECTION) =====
void MidiGenerator::setInputNotes(int rhythmIndex, const std::vector<int>& notes)
{
    if (rhythmIndex < 0 || rhythmIndex >= 6) return;

    // 1. Refresh the main input without resetting everything
    inputNotes[rhythmIndex] = notes;

    // 2. Synchronize arpNotes to reflect only the existing user selection
    std::lock_guard<std::mutex> lock(arpNotesMutex[rhythmIndex]);

    std::vector<int> filteredNotes;
    for (int n : notes)
    {
        // If the user has not selected anything yet, the Arp includes everything
        if (arpSelectedNotes[rhythmIndex].empty())
        {
            filteredNotes.push_back(n);
        }
        else
        {
            // otherwise only include if present in the persistent selection
            if (std::find(arpSelectedNotes[rhythmIndex].begin(),
                arpSelectedNotes[rhythmIndex].end(), n) != arpSelectedNotes[rhythmIndex].end())
            {
                filteredNotes.push_back(n);
            }
        }
    }

    arpNotes[rhythmIndex] = filteredNotes;
    rhythmArps[rhythmIndex].setNumNotes((int)arpNotes[rhythmIndex].size());
}

bool MidiGenerator::advanceArp(int rhythmIndex,
    double samplesPerStep,
    double stepSamples)
{
    // In Standalone, if the clock manager is active, use its values
    if (clock != nullptr)
    {
        double internalStep = clock->getSamplesPerStep();
        if (internalStep > 0) samplesPerStep = internalStep;
    }
    if (rhythmIndex < 0 || rhythmIndex >= (int)rhythmArps.size())
        return false;

    // Apply the Rate
    double rate = arpRates[rhythmIndex];
    return rhythmArps[rhythmIndex].advance(samplesPerStep * rate, stepSamples * rate);
}

// ===== TRIGGER STEP (SAFE, NO VECTOR OUT-OF-RANGE) =====
bool MidiGenerator::triggerStep(int rhythmIndex, int baseNote, int& outMidiNote)
{
    if (rhythmIndex < 0 || rhythmIndex >= 6)
        return false;

    // Euclidean Step
    int euclidStep = rhythmPatterns[rhythmIndex].getCurrentStep();

    // Constructs vector of available Euclidean notes
    std::vector<int> euclidNotes;
    for (int n : inputNotes[rhythmIndex])
    {
        // If ARP is active, it excludes selected notes
        if (arpEnabled[rhythmIndex] && !arpSelectedNotes[rhythmIndex].empty())
        {
            if (std::find(arpSelectedNotes[rhythmIndex].begin(),
                arpSelectedNotes[rhythmIndex].end(), n) != arpSelectedNotes[rhythmIndex].end())
                continue; // note managed by the ARP
        }
        euclidNotes.push_back(n);
    }

    // If there are Euclidean notes available and the current step is active
    if (!euclidNotes.empty() && rhythmPatterns[rhythmIndex].isStepOn(euclidStep))
    {
        outMidiNote = euclidNotes[euclidStep % euclidNotes.size()];
        return true;
    }

    // If ARP enabled, read note from ARP
    if (arpEnabled[rhythmIndex] && !arpNotes[rhythmIndex].empty())
    {
        // ensures correct index and real notes
        int noteIndex = rhythmArps[rhythmIndex].getNoteIndex() % (int)arpNotes[rhythmIndex].size();
        outMidiNote = arpNotes[rhythmIndex][noteIndex];
        return true;
    }

    return false;
}

void MidiGenerator::setArpRate(int rhythmIndex, double rate)
{
    if (rhythmIndex < 0 || rhythmIndex >= 6) return;
    if (rate <= 0.0) rate = 1.0; // sicurezza
    arpRates[rhythmIndex] = rate;
}

double MidiGenerator::getArpRate(int rhythmIndex) const
{
    if (rhythmIndex < 0 || rhythmIndex >= 6) return 1.0;
    return arpRates[rhythmIndex];
}

void MidiGenerator::prepareForPlay(double sampleRate)
{
    for (int i = 0; i < 6; ++i)
    {
        euclideanCounters[i] = 0.0;
        rhythmPatterns[i].reset();        // Reset Euclidean patterns to the beginning (step 0)
        rhythmArps[i].reset();        // Resetting the arpeggiators

        // Cleaning the atomic masks
        arpInputNotesMaskLow[i].store(0);
        arpInputNotesMaskHigh[i].store(0);
    }
}
