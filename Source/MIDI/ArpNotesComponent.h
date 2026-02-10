#pragma once
#include <JuceHeader.h>
#include <vector>
#include <algorithm>
#include <functional>

class ArpNotesComponent : public juce::Component
{
public:
    ArpNotesComponent(int rowIndex,
        const std::vector<int>& arpInputNotes,
        const std::vector<int>& selectedNotes,
        std::function<void(std::vector<int>)> onChange)
        : row(rowIndex), callback(onChange)
    {
        selected = selectedNotes; // real initial realignment

        setInterceptsMouseClicks(true, true);
        setWantsKeyboardFocus(true);

        int y = 10;

        for (int midiNote : arpInputNotes)
        {
            auto* tb = toggles.add(new juce::ToggleButton(
                juce::MidiMessage::getMidiNoteName(midiNote, true, true, 3)));

            // Check if the exact MIDI value is in the list of selected ones
            const bool isSelected = std::find(selected.begin(), selected.end(), midiNote) != selected.end();

            tb->setToggleState(isSelected, juce::dontSendNotification);

            tb->onClick = [this, midiNote, tb]()
                {
                    const bool isOn = tb->getToggleState();

                    // Removes all traces of the note before deciding whether to add it, to clean up any invisible duplicates
                    selected.erase(std::remove(selected.begin(), selected.end(), midiNote), selected.end());

                    if (isOn)
                    {
                        if (selected.size() < 4)
                        {
                            selected.push_back(midiNote);
                        }
                        else
                        {
                            // If there are already 4 notes, do not allow ignition
                            tb->setToggleState(false, juce::dontSendNotification);
                        }
                    }

                    if (callback)
                    {
                        // Create a local copy to avoid race conditions
                        std::vector<int> notesCopy = selected;
                        callback(notesCopy);
                    }
                };

            addAndMakeVisible(tb);
            tb->setBounds(10, y, 140, 24);
            y += 26;
        }
        // If there are too many notes (e.g. scales), we limit the pitch to prevent the popup from "running away"
        int totalHeight = juce::jmin(y + 10, 400);
        setSize(160, totalHeight);
    }
    // Public callback visible from PluginEditor
    std::function<void(std::vector<int>)> callback;

private:
    int row;
    juce::OwnedArray<juce::ToggleButton> toggles;
    std::vector<int> selected;
};

