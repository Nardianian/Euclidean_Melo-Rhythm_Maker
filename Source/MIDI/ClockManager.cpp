#include "ClockManager.h"

void ClockManager::prepare(double sr)
{
    sampleRate = sr;
    samplesPerMidiClock = (60.0 / bpm) * sampleRate / 24.0;
    internalSamplesPerStep = (60.0 / bpm) * sampleRate / 4.0; // 16th note
    sampleCounter = 0.0;
}

void ClockManager::setClockSource(ClockSource src) { source.store(src); }
void ClockManager::setClockType(ClockType t) { type.store(t); }
void ClockManager::setClockRole(ClockRole r) { role.store(r); }

void ClockManager::processMidi(const juce::MidiBuffer& midi)
{
    if (role.load() != ClockRole::Slave)
        return;

    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (role.load() == ClockRole::Slave && msg.isMidiClock())
        {
            // Advances the sample-accurate counter for each MIDI tick // 24 PPQN -> 16th note
            sampleCounter += samplesPerMidiClock;
        }
    }
}

void ClockManager::processBlock(int numSamples)
{
    // Don't calculate internalSamplesPerStep every block // Advance the sample-accurate counter
    sampleCounter += numSamples;
}
