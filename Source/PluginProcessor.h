#pragma once 
#include <JuceHeader.h>
#include "MidiGenerator.h"
#include "ClockManager.h" // For thread-safety in ARP
#include <mutex>
#include <atomic>
#include <vector>
#include <array>

struct MidiPortEvent
{
    int rhythmIndex;        // 0–5
    int deviceIndex;        // MIDI device
    juce::MidiMessage message;
    int sampleOffset;
};

class Euclidean_seqAudioProcessor : public juce::AudioProcessor
{
public:
    Euclidean_seqAudioProcessor();
    ~Euclidean_seqAudioProcessor() override;

    //==============================================================================
    // Class members
    double samplesPerArpStep = 0.0;
    bool arpStepAdvanced = false;
    bool arpActive = false;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void updateClockSource();
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Euclidean_seq"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState parameters;
    MidiGenerator midiGen;

    // Mutex to protect access to ARP notes in MidiGenerator
    std::array<std::mutex, 6> arpNotesMutex;

    ClockManager clock;

    // ================== NOTE SOURCE TYPES ==================
    enum class NoteSourceType
    {
        Single,
        Scale,
        Chord
    };

    struct NoteSource
    {
        NoteSourceType type = NoteSourceType::Single;
        int value = 60;    // nota singola oppure ID scala/accordo
        int octave = 4;   // MIDI OCTAVE (1–7), used only for Scale/Chord
    };

    NoteSource noteSources[6];

    const juce::Array<juce::MidiDeviceInfo>& getAvailableMidiOutputs() const
    {
        return availableMidiOutputs;
    }
    bool isArpActive(int rhythmIndex) const
    {
        return parameters.getRawParameterValue("rhythm" + juce::String(rhythmIndex) + "_arpActive")->load() > 0.5f;
    }

    double getArpRate(int rhythmIndex) const
    {
        static const double arpRateTable[] = { 1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125, 0.1666667, 0.375 };
        int rateIndex = (int)parameters.getRawParameterValue("rhythm" + juce::String(rhythmIndex) + "_arpRate")->load();
        return arpRateTable[juce::jlimit(0, 7, rateIndex)];
    }

    void refreshMidiOutputs();
    void updateMidiOutputForRhythm(int rhythmIndex, int deviceIndex);
    void changeMidiOutput(int row, const juce::String& deviceName);
    bool isRowActive(int row) const;
    // ===== ARP SELECTION FROM GUI =====
    void setArpSelectionFromGUI(int row, const std::vector<bool>& selection);

    // Funzione thread-safe per leggere le note ARP di un ritmo
    std::vector<int> getArpNotesThreadSafe(int row)
    {
        if (row < 0 || row >= 6)
            return {};

        std::vector<int> notesCopy;
        {
            std::lock_guard<std::mutex> lock(arpNotesMutex[row]);
            notesCopy = midiGen.getNotesForRow(row); // thread-safe copy
        }
        return notesCopy;
    }

private:
    juce::Array<juce::MidiDeviceInfo> availableMidiOutputs;

    struct RhythmMidiOut
    {
        int selectedPortIndex = -1;
        int selectedChannel = 1;
        std::unique_ptr<juce::MidiOutput> output;
    };
    std::array<RhythmMidiOut, 6> rhythmMidiOuts;
    // Array of active MIDI output pointers for each rhythm (MIDI port management)
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    std::vector<MidiPortEvent> stagedMidiEvents;

    int lastBassNote = -1;
    bool bassNoteActive = false;

    double bassGlideTimeSeconds = 0.08;
    double bassGlideProgress = 0.0;
    int bassStartNote = -1;
    int bassTargetNote = -1;
    bool bassGlideActive = false;

    float bassGlideCurve = 0.6f;
    int bassPitchBendRange = 12;

    // Use the global enum defined in ClockManager.h to avoid duplicate types
    ::ClockSource clockSource = ::ClockSource::Internal;
    bool externalClockRunning = false;
    bool isMuted = false;

    double currentSampleRate = 44100.0;
    double samplesPerBeat = 0.0;
    double samplesPerStep = 0.0;
    double internalBpm = 120.0;

    int64_t globalSampleCounter = 0;
    std::array<int64_t, 6> nextStepSamplePerRhythm{};
    std::array<int64_t, 6> stepCounterPerRhythm{};
    std::array<std::vector<double>, 6> stepMicrotiming;

    struct PendingNoteOff
    {
        int note = 0;
        int channel = 1;
        int row = 0;              // Line (0-5) to address the correct MIDI port
        double remainingSamples = 0.0; // double precision for timing
    };
    std::vector<PendingNoteOff> pendingNoteOffs;

    int midiClockCounter = 0;
    int64_t globalStepCounter = 0;
    bool isPlaying = false;
    std::atomic<bool> globalPlayState{ false };
    std::array<double, 6> extClockSampleAccum{};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Euclidean_seqAudioProcessor)
};

