#pragma once
#include <JuceHeader.h>
#include <atomic>

enum class ClockSource
{
    DAW,
    Internal,
    External
};

enum class ClockType
{
    MidiBeatClock,   // 24 PPQN
    MidiTimeCode,
    PPQN
};

enum class ClockRole
{
    Master,
    Slave
};

class ClockManager
{
public:
    void prepare(double sampleRate);

    void setBpm(double newBpm);
    void setClockSource(ClockSource src);
    void setClockType(ClockType type);
    void setClockRole(ClockRole role);

    void processBlock(int numSamples);
    void processMidi(const juce::MidiBuffer& midi);

    // Calculate the samples for one sixteenth (Euclidean step)
    double getSamplesPerStep() const
    {
        // 60 / BPM = secondi per quarto. 
        // (60 / BPM) / 4 = secondi per sedicesimo.
        return (60.0 / bpm / 4.0) * sampleRate;
    }

    // Calculate samples for arpeggiator step
    double getSamplesPerArpStep(double arpRateMultiplier) const
    {
        return getSamplesPerStep() * arpRateMultiplier;
    }

    // Internal sample-accurate counter, reset on reset
    void reset()
    {
        sampleCounter = 0.0;
        midiClockCounter = 0; // Essential for external sync
    }

    // ==== SAMPLE-ACCURATE ADVANCE ==== // Returns true each time a step is completed (16th note)
    bool advance(int numSamples)
    {
        sampleCounter += numSamples;
        if (sampleCounter >= getSamplesPerStep())
        {
            sampleCounter -= getSamplesPerStep();
            return true;
        }
        return false;
    }

private:
    double sampleRate = 44100.0;
    double bpm = 120.0;

    std::atomic<ClockSource> source{ ClockSource::Internal };
    std::atomic<ClockType> type{ ClockType::MidiBeatClock };
    std::atomic<ClockRole> role{ ClockRole::Master };

    // ===== MIDI CLOCK =====
    int midiClockCounter = 0;
    double samplesPerMidiClock = 0.0;
    double sampleCounter = 0.0;

    // ===== INTERNAL =====
    double internalSamplesPerStep = 0.0;
};
