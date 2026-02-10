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
        selected = selectedNotes; // riallineamento iniziale reale

        setInterceptsMouseClicks(true, true);
        setWantsKeyboardFocus(true);

        int y = 10;

        for (int midiNote : arpInputNotes)
        {
            auto* tb = toggles.add(new juce::ToggleButton(
                juce::MidiMessage::getMidiNoteName(midiNote, true, true, 3)));

            // Verifichiamo se il valore MIDI esatto è nella lista dei selezionati
            const bool isSelected = std::find(selected.begin(), selected.end(), midiNote) != selected.end();

            tb->setToggleState(isSelected, juce::dontSendNotification);

            tb->onClick = [this, midiNote, tb]()
                {
                    const bool isOn = tb->getToggleState();

                    // Rimuoviamo ogni traccia della nota prima di decidere se aggiungerla
                    // Questo pulisce eventuali duplicati invisibili
                    selected.erase(std::remove(selected.begin(), selected.end(), midiNote), selected.end());

                    if (isOn)
                    {
                        if (selected.size() < 4)
                        {
                            selected.push_back(midiNote);
                        }
                        else
                        {
                            // Se abbiamo già 4 note, non permettere l'accensione
                            tb->setToggleState(false, juce::dontSendNotification);
                        }
                    }

                    if (callback)
                    {
                        // Creiamo una copia locale per evitare race conditions
                        std::vector<int> notesCopy = selected;
                        callback(notesCopy);
                    }
                };

            addAndMakeVisible(tb);
            tb->setBounds(10, y, 140, 24);
            y += 26;
        }
        // Se ci sono troppe note (es. scale), limitiamo l'altezza per evitare che il popup "scappi"
        int totalHeight = juce::jmin(y + 10, 400);
        setSize(160, totalHeight);
    }
    // Callback pubblica visibile da PluginEditor
    std::function<void(std::vector<int>)> callback;

private:
    int row;
    juce::OwnedArray<juce::ToggleButton> toggles;
    std::vector<int> selected;
};
