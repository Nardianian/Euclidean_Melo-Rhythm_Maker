#pragma once
#include <JuceHeader.h>
#include <cmath> // Required for std::fmod

enum class ArpType { UP, DOWN, UP_DOWN, RANDOM };

class Arp
{
public:
    Arp() { reset(); }
    Arp(ArpType type, int noteCount, double rate) : arpType(type), numNotes(noteCount), arpRate(rate) { reset(); }
    ~Arp() = default;

    void setType(ArpType type) { arpType = type; }
    void setNumNotes(int count) { if (count <= 0) return; numNotes = count; if (currentStep >= numNotes) currentStep = 0; }
    void setRate(double r) { arpRate = r; }
    double getRate() const { return arpRate; }

    bool advance(double samplesToAdvance, double samplesPerArpStep)
    {
        if (numNotes <= 0 || samplesPerArpStep <= 0.0) return false;
        arpSampleCounter += samplesToAdvance;
        if (arpSampleCounter >= samplesPerArpStep) {
            arpSampleCounter = std::fmod(arpSampleCounter, samplesPerArpStep);
            switch (arpType) {
            case ArpType::UP: currentStep = (currentStep + 1) % numNotes; break;
            case ArpType::DOWN: currentStep = (currentStep - 1 + numNotes) % numNotes; break;
            case ArpType::UP_DOWN:
                if (directionUp) { currentStep++; if (currentStep >= numNotes - 1) directionUp = false; }
                else { currentStep--; if (currentStep <= 0) directionUp = true; }
                break;
            case ArpType::RANDOM: currentStep = juce::Random::getSystemRandom().nextInt(numNotes); break;
            }
            return true;
        }
        return false;
    }

    int getNoteIndex() const { return currentStep; }
    void reset() { currentStep = 0; directionUp = true; arpSampleCounter = 0.0; }

    Arp(const Arp&) = delete;
    Arp& operator=(const Arp&) = delete;
    Arp(Arp&&) noexcept = default;
    Arp& operator=(Arp&&) noexcept = default;

private:
    ArpType arpType = ArpType::UP;
    int numNotes = 1;
    double arpRate = 1.0;
    int currentStep = 0;
    double arpSampleCounter = 0.0;
    bool directionUp = true;
    JUCE_LEAK_DETECTOR(Arp)
};
