/*
  ==============================================================================

    Arp.h
    Created: 9 Oct 2022 9:12:18pm
    Author:  Andreas Sandersen
    Modified: 13 Dec. 2025 00.25:00 am by Nard.
  ==============================================================================
*/
#pragma once
#include <JuceHeader.h>

// =======================================
// Arp Type Enum
// =======================================
enum class ArpType
{
    UP,
    DOWN,
    UP_DOWN,
    RANDOM
};

// =======================================
// Arp Class
// =======================================
class Arp
{
public:
    Arp() = default;
    Arp(ArpType type, int noteCount, double rate)
        : arpType(type), numNotes(noteCount), arpRate(rate)
    {
        currentStep = 0;
        directionUp = true;
    }

    ~Arp() = default;

    void setType(ArpType type) { arpType = type; reset(); }
    void setNumNotes(int count) { numNotes = count; reset(); }
    void setRate(double r) { arpRate = r; }
    double getRate() const { return arpRate; }

    int nextStep()
    {
        if (numNotes <= 0) return 0;

        int note = 0;

        switch (arpType)
        {
        case ArpType::UP:
            note = currentStep;
            currentStep = (currentStep + 1) % numNotes;
            break;

        case ArpType::DOWN:
            note = (numNotes - 1) - currentStep;
            currentStep = (currentStep + 1) % numNotes;
            break;

        case ArpType::UP_DOWN:
            note = currentStep;
            if (directionUp)
            {
                currentStep++;
                if (currentStep >= numNotes - 1) directionUp = false;
            }
            else
            {
                currentStep--;
                if (currentStep <= 0) directionUp = true;
            }
            break;

        case ArpType::RANDOM:
            note = juce::Random::getSystemRandom().nextInt(numNotes);
            break;
        }

        return note;
    }

    bool advance(double samplesPerStep, double samplesPerArpStep)
    {
        arpSampleCounter += samplesPerStep;

        if (arpSampleCounter < samplesPerArpStep)
            return false;

        arpSampleCounter -= samplesPerArpStep;

        if (numNotes <= 0)
            return false;

        switch (arpType)
        {
        case ArpType::UP:
            currentStep = (currentStep + 1) % numNotes;
            break;

        case ArpType::DOWN:
            currentStep = (currentStep - 1 + numNotes) % numNotes;
            break;

        case ArpType::UP_DOWN:
            if (directionUp)
            {
                currentStep++;
                if (currentStep >= numNotes - 1)
                    directionUp = false;
            }
            else
            {
                currentStep--;
                if (currentStep <= 0)
                    directionUp = true;
            }
            break;

        case ArpType::RANDOM:
            currentStep = juce::Random::getSystemRandom().nextInt(numNotes);
            break;
        }

        return true;
    }

    // ===== READ CURRENT ARP STEP (OUTPUT) =====
    int getCurrentStep() const
    {
        return currentStep;
    }

    void reset()
    {
        currentStep = 0;
        directionUp = true;
        arpSampleCounter = 0.0;
    }

private:
    ArpType arpType = ArpType::UP;
    int numNotes = 1;
    double arpRate = 1.0;  // in beats per step
    int currentStep = 0;
    double arpSampleCounter = 0.0;
    bool directionUp = true;

public:
    // Copia DISABILITATA (FONDAMENTALE)
    Arp(const Arp&) = delete;
    Arp& operator=(const Arp&) = delete;

    // Move ABILITATO
    Arp(Arp&&) noexcept = default;
    Arp& operator=(Arp&&) noexcept = default;

private:
    JUCE_LEAK_DETECTOR(Arp)
};
#pragma once

