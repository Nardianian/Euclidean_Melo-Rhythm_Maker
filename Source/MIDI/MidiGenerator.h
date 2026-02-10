#pragma once
#include <JuceHeader.h>
#include <algorithm>
#include <atomic>
#include <array>
#include <vector>
#include "Euclidean.h"
#include "Arp.h"           // required because this uses std::array<Arp,6>
class ClockManager;       // forward declaration for pointer, avoid unnecessary include

// ===== DEFINIZIONE SCALE E CHORD =====
struct ScaleDef
{
    const char* name;
    std::vector<int> intervals;
};

struct ChordDef
{
    const char* name;
    std::vector<int> intervals;
};

class MidiGenerator
{
public:
    struct NoteInfo
    {
        std::vector<int> notes;        // MIDI note numbers
        std::vector<int> octave;       // corresponding octaves
        std::vector<bool> arpSelected; // flag note selected for ARP
    };

    MidiGenerator()
    {
        arpNotes.resize(6);
        inputNotes.resize(6);
    }

    // ===== SCALA / ACCORDO SELECTED FOR EACH ROW =====
    int selectedScale[6] = { -1, -1, -1, -1, -1, -1 }; // -1 = nessuna
    int selectedChord[6] = { -1, -1, -1, -1, -1, -1 }; // -1 = nessuno

    // ===== DATI =====
    std::array<EuclideanPattern, 6> rhythmPatterns;
    std::array<Arp, 6> rhythmArps;
    std::vector<std::vector<int>> arpNotes; // actual ARP notes for each line

    // ===== ARP NOTES SELECTED BY USER =====
    std::vector<int> arpSelectedNotes[6];       // Array to save the selected notes in the popup for each row (max 4 ARP notes)
    std::mutex arpNotesMutex[6];                // one for each rhythm row
    std::vector<int> getAvailableArpNotesForGUI(int rhythmIndex) const; // Returns available notes for ARP including scales/chords

    // Update ARP notes by filtering input Notes + scale + chord (max 4) 
    void setArpNotes(int rhythmIndex, const std::vector<int>& notes);
    void setArpNotesThreadSafe(int rhythmIndex, const std::vector<int>& notes);

    // Returns the ARP notes for a specific rhythm
    std::vector<int> getNotesForRow(int row) const
    {
        if (row < 0 || row >= 6)
            return {};

        return arpNotes[row]; // arpNotes è il vettore delle note per ciascun ritmo
    }

    // Returns all notes available for the row's ARP
    inline std::vector<int> getAvailableArpNotes(int rhythmIndex) const
    {
        jassert(rhythmIndex >= 0 && rhythmIndex < 6);

        const auto& notes = inputNotes[rhythmIndex];

        if (notes.empty())
            return {};

        std::vector<int> result;
        for (auto n : notes)
            if (n >= 0 && n <= 127)
                result.push_back(n);

        return result;
    }

    // ===== MIDI FLOW STARTING NOTES (pre-ARP) =====
    const std::vector<std::vector<int>>& getInputNotes() const { return inputNotes; }

    // ===== NOTES RECEIVED BY ARP (GUI / HOST) =====
    std::array<std::atomic<uint64_t>, 6> arpInputNotesMaskLow{};   // note 0–63
    std::array<std::atomic<uint64_t>, 6> arpInputNotesMaskHigh{};  // note 64–127

    // ===== FUNCTIONS =====
    bool triggerStep(int rhythmIndex, int baseNote, int& outMidiNote);
    bool advanceArp(int rhythmIndex, double samplesPerStep, double samplesPerArpStep);

    // ===== ARP RATE =====
    void setArpRate(int rhythmIndex, double rate);
    double getArpRate(int rhythmIndex) const;

    // ================= THREAD-SAFE RETRIEVAL OF ARP INPUT NOTES =================
    inline std::vector<int> getArpInputNotes(int rhythmIndex) const
    {
        jassert(rhythmIndex >= 0 && rhythmIndex < 6);

        std::vector<int> notes;
        auto lowMask = arpInputNotesMaskLow[rhythmIndex].load();
        auto highMask = arpInputNotesMaskHigh[rhythmIndex].load();

        for (int i = 0; i < 64; ++i)
            if (lowMask & (1ULL << i))
                notes.push_back(i);
        for (int i = 0; i < 64; ++i)
            if (highMask & (1ULL << i))
                notes.push_back(i + 64);

        return notes;
    }

    // ===== SCALE and CHORD NOTE RETRIEVAL =====
    std::vector<int> getScaleNotes(int scaleID) const;

    std::vector<int> getChordNotes(int chordID) const;

    // ===== SCALE / CHORD DEGREE MAPPING =====
    int getNoteFromScaleDegree(int degree, int rootNote, const std::vector<int>& scale) const;

    // ===== INPUT NOTES FOR ARP (thread-safe, GUI-visible) =====
    void setArpInputNotes(int rhythmIndex, const std::vector<int>& notes);

    // ARP Notes see Scale and Chord
    std::vector<int> getArpNotes(int rhythmIndex) const
    {
        jassert(rhythmIndex >= 0 && rhythmIndex < 6);

        // If the user has NOT selected notes -> use all input notes
        if (arpSelectedNotes[rhythmIndex].empty())
            return inputNotes[rhythmIndex];

        // otherwise it returns ONLY the selection (max 4)
        std::vector<int> result;
        for (size_t i = 0; i < arpSelectedNotes[rhythmIndex].size() && i < 4; ++i)
            result.push_back(arpSelectedNotes[rhythmIndex][i]);

        return result;
    }

    // Set the rhythm input notes
    void setInputNotes(int rhythmIndex, const std::vector<int>& notes);

    // Starting notes for each rhythm (row)
    std::vector<std::vector<int>> inputNotes;
    // ARP status for each rhythm (row)
    bool arpEnabled[6] = { false, false, false, false, false, false };
    std::vector<std::vector<int>>& getInputNotes() { return inputNotes; }

    // ARP/NoteInfo Functions
    NoteInfo getNoteInfo(int row) const;
    void setArpSelection(int row, const std::vector<bool>& selection);

    ClockManager* clock = nullptr;

    private:
        double euclideanCounters[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        double rhythmSamplesPerStep[6] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 }; // calculated from BPM and steps
        std::array<double, 6> arpRates{}; // default 0.0, then set 1.0 in prepare
        // Structure to track active notes and their remaining off time
        struct ActiveNote {
            int note;
            int channel;
            double samplesLeft;
        };
        std::vector<ActiveNote> activeNotes[6]; // One list for each row

    // Advances Euclidean and sample-accurate ARP using global clock

    struct PendingNoteOff {
        int note;
        int channel;
        double samplesRemaining;
    };
    std::vector<PendingNoteOff> pendingNoteOffs;

public:
    void setClockManager(ClockManager* cm) { clock = cm; }
    // Initialize sample-accurate counters on startup
    void prepareForPlay(double sampleRate);
};
