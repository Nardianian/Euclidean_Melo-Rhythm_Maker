#include "PluginEditor.h"
#include "ArpNotesComponent.h"

namespace
{
    constexpr float mmToPx = 96.0f / 25.4f;
}

//==============================================================================
Euclidean_seqAudioProcessorEditor::Euclidean_seqAudioProcessorEditor(Euclidean_seqAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(baseWidth, baseHeight);
    setResizable(true, true);
    setResizeLimits(baseWidth / 2, baseHeight / 2, baseWidth * 2, baseHeight * 2);
    getConstrainer()->setFixedAspectRatio((double)baseWidth / (double)baseHeight);

    addAndMakeVisible(clockSettingsButton);

    // ===== CLOCK SETTINGS POPUP =====
    clockSettingsButton.onClick = [this]() { openClockSettingsPopup(); };

    // ================= GLOBAL PLAY =================
    addAndMakeVisible(playButton);
    playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);
    playButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    playButton.setClickingTogglesState(true);
    playButton.setTooltip("Global Play / Stop");

    playAttachment = std::make_unique<ButtonAttachment>(
        audioProcessor.parameters,
        "globalPlay",
        playButton);

    // ================= RANDOMIZE BUTTON =================
    addAndMakeVisible(randomizeButton);
    randomizeButton.setButtonText("Randomize");
    randomizeButton.onClick = [this]()
        {
            for (int r = 0; r < 6; ++r)
            {
                auto& row = rhythmRows[r];
                auto& params = audioProcessor.parameters;

                if (auto* noteParam = params.getParameter("rhythm" + juce::String(r) + "_note"))
                    noteParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat());
                if (auto* stepsParam = params.getParameter("rhythm" + juce::String(r) + "_steps"))
                    stepsParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat());
                if (auto* pulsesParam = params.getParameter("rhythm" + juce::String(r) + "_pulses"))
                    pulsesParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat());
                if (auto* swingParam = params.getParameter("rhythm" + juce::String(r) + "_swing"))
                    swingParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat());
                if (auto* microParam = params.getParameter("rhythm" + juce::String(r) + "_microtiming"))
                    microParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f);
                if (auto* velParam = params.getParameter("rhythm" + juce::String(r) + "_velocityAccent"))
                    velParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat());
                if (auto* lenParam = params.getParameter("rhythm" + juce::String(r) + "_noteLength"))
                    lenParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat());
            }
        };

    // ================= HEADER LABELS =================
    const char* labelsText[14] =
    {
        "Active", "Note", "Steps", "Pulses", "Swing", "Micro", "Velocity", "NoteLen",
        "ARP On", "ARP Notes", "Mode", "ARP Rate", "MIDI Port", "MIDI Ch"
    };
    for (int i = 0; i < 14; ++i)
    {
        addAndMakeVisible(headerLabels[i]);
        headerLabels[i].setText(labelsText[i], juce::dontSendNotification);
        headerLabels[i].setJustificationType(juce::Justification::centred);
    }

    // ================= RHYTHM ROWS =================
    for (int i = 0; i < 6; ++i)
    {
        auto& row = rhythmRows[i];

        // ===== ROW LABEL (R1–R6) =====
        addAndMakeVisible(row.rowLabel);
        row.rowLabel.setText("R" + juce::String(i + 1), juce::dontSendNotification);
        row.rowLabel.setJustificationType(juce::Justification::centred);
        row.rowLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        // ===== ACTIVE BUTTON =====
        addAndMakeVisible(row.activeButton);
        row.activeButton.setClickingTogglesState(true);

        rhythmAttachments[i].active =
            std::make_unique<ButtonAttachment>(
                audioProcessor.parameters,
                "rhythm" + juce::String(i) + "_active",
                row.activeButton);

        // ===== MUTE / SOLO / RESET =====
        addAndMakeVisible(row.muteButton);
        addAndMakeVisible(row.soloButton);
        addAndMakeVisible(row.resetButton);

        rhythmAttachments[i].mute =
            std::make_unique<ButtonAttachment>(
                audioProcessor.parameters,
                "rhythm" + juce::String(i) + "_mute",
                row.muteButton);

        rhythmAttachments[i].solo =
            std::make_unique<ButtonAttachment>(
                audioProcessor.parameters,
                "rhythm" + juce::String(i) + "_solo",
                row.soloButton);

        rhythmAttachments[i].reset =
            std::make_unique<ButtonAttachment>(
                audioProcessor.parameters,
                "rhythm" + juce::String(i) + "_reset",
                row.resetButton);

        // ===== BASS BUTTON (ONLY R6) =====
        if (i == 5)
        {
            addAndMakeVisible(bassSettingsButton);
            bassSettingsButton.onClick = [this]()
                {
                    //Make the type explicit to avoid 'auto' deduction errors on MSVC
                    BassSettingsComponent* comp = new BassSettingsComponent(audioProcessor.parameters);

                    // >>> MANDATORY DIMENSIONS <<<
                    comp->setSize(320, 260);

                    bassPopup.reset(new juce::CallOutBox(
                        *comp,
                        bassSettingsButton.getScreenBounds(),
                        nullptr));

                    bassPopup->setAlwaysOnTop(true);
                    bassPopup->enterModalState(true);
                };
        }

        // Setup rotary knobs
        auto setupRotary = [](juce::Slider& s)
            {
                s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 42, 16);
                s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::orange);
                s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::black);
                s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
                s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
                s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            };

        setupRotary(row.stepsKnob);
        setupRotary(row.pulsesKnob);
        setupRotary(row.swingKnob);
        setupRotary(row.microKnob);
        setupRotary(row.velocityKnob);
        setupRotary(row.noteLenKnob);

        addAndMakeVisible(row.noteSourceButton);
        row.noteSourceButton.setButtonText("Note");
        row.noteSourceButton.onClick = [this, i]()
            {
                juce::PopupMenu menu;

                // ================= SINGLE NOTE =================
                juce::PopupMenu singleMenu;
                static const char* noteNames[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

                const auto& src = audioProcessor.noteSources[i];

                int id = 100;
                for (int octave = 0; octave <= 8; ++octave)
                {
                    for (int n = 0; n < 12; ++n)
                    {
                        int midiNote = octave * 12 + n;
                        if (midiNote > 127) continue;

                        bool selected =
                            (src.type == Euclidean_seqAudioProcessor::NoteSourceType::Single &&
                                src.value == midiNote);

                        singleMenu.addItem(
                            id++,
                            juce::String(noteNames[n]) + juce::String(octave - 1),
                            true,
                            selected);
                    }
                }

                // ================= SCALE =================
                juce::PopupMenu scaleMenu;
                const char* scaleNames[] =
                {
                    "Z-DMixolydian", "Z-GMixolydian", "Z-PrygianDominant", "Z-Ddorian", "Z-Modified", "Z-CHexatonic",
                    "Z-CDiminished", "Z-CLydianDominant", "Z-APentaMinor1", "Z-APentaMinor2", "Z-BlackPage",
                    "Z-St.Alfonso", "C-Blues1", "C-Blues2", "B-Blues", "A-Blues1", "A-Blues2", "G-Blues",
                    "Min-Jazz", "C7-Jazz", "Am7-Jazz", "Dm7-Jazz", "G7-Jazz", "Cmaj7-Jazz", "Fm7b5-Jazz",
                    "PentaMinor1", "PentaMinor2", "PentaMajor", "Major", "Minor", "Lydian",
                    "G-Major", "D-Major", "F-Major", "A-Major", "A-Minor", "D-Minor", "E-Minor", "F-Minor", "G-Minor",
                    "D-Lydian", "B-Lydian", "C-LydianDominant", "Phrygian", "D-Phrygian", "E-Phrygian", "G-Phrygian",
                    "Mixolydian", "C-Mixolydian", "D-Mixolydian", "Dorian", "G-Dorian", "A-Dorian", "C-Dorian"
                };
                const int numScales = sizeof(scaleNames) / sizeof(scaleNames[0]);

                for (int s = 0; s < numScales; ++s)
                {
                        int itemID = 200 + s;
                        bool selected = (src.type == Euclidean_seqAudioProcessor::NoteSourceType::Scale &&
                            src.value == s);

                        scaleMenu.addItem(itemID, scaleNames[s], true, selected);
                }
                // ================= CHORD =================
                juce::PopupMenu chordMenu;
                const char* chordNames[] =
                {
                    "Z-St.Alfonso1", "Z-St.Alfonso2", "Z-St.AlfonsoInversion", "Z-Watermelon", "Z-Inc", "Z-Inseventh3",
                    "Gsus4", "C-Major", "1st-Inversion", "2nd-Inversion", "Augmented", "D-Augmented", "E-Augmented",
                    "F-Augmented", "G-Augmented", "A-Augmented", "B-Augmented", "Major 7", "Fmaj7", "Gmaj7",
                    "Amaj7", "Dmaj7", "Minor 7", "Amin7", "Dmin7", "Gmin7", "Emin7",
                    "Major", "D-Major", "E-Major", "F-Major", "G-Major", "A-Major", "B-Major",
                    "Minor", "D-Minor", "E-Minor", "F-Minor", "G-Minor", "A-Minor", "B-Minor",
                    "Diminished", "D-Diminished", "E-Diminished", "F-Diminished", "G-Diminished", "A-Diminished", "B-Diminished",
                    "C7-Diminished", "G7-Diminished"
                };
                const int numChords = sizeof(chordNames) / sizeof(chordNames[0]);

                for (int c = 0; c < numChords; ++c)
                {
                        int itemID = 300 + c;
                        bool selected = (src.type == Euclidean_seqAudioProcessor::NoteSourceType::Chord &&
                            src.value == c);
                        chordMenu.addItem(itemID, chordNames[c], true, selected);
                }

                // ================= MAIN MENU =================
                menu.addSubMenu("Single Note", singleMenu);
                menu.addSubMenu("Scale", scaleMenu);
                menu.addSubMenu("Chord", chordMenu);

                // ================= SHOW MENU ASYNC =================
                menu.showMenuAsync(juce::PopupMenu::Options(),
                    [this, i](int result)
                    {
                        if (result <= 0)
                            return;

                        auto& src = audioProcessor.noteSources[i];
                        auto& button = rhythmRows[i].noteSourceButton;

                        static const char* noteNames[] =
                        { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };

                        static const char* scaleNames[] =
                        {
                            "Z-DMixolydian", "Z-GMixolydian", "Z-PrygianDominant", "Z-Ddorian", "Z-Modified", "Z-CHexatonic",
                            "Z-CDiminished", "Z-CLydianDominant", "Z-APentaMinor1", "Z-APentaMinor2", "Z-BlackPage",
                            "Z-St.Alfonso", "C-Blues1", "C-Blues2", "B-Blues", "A-Blues1", "A-Blues2", "G-Blues",
                            "Min-Jazz", "C7-Jazz", "Am7-Jazz", "Dm7-Jazz", "G7-Jazz", "Cmaj7-Jazz", "Fm7b5-Jazz",
                            "PentaMinor1", "PentaMinor2", "PentaMajor", "Major", "Minor", "Lydian",
                            "G-Major", "D-Major", "F-Major", "A-Major", "A-Minor", "D-Minor", "E-Minor", "F-Minor", "G-Minor",
                            "D-Lydian", "B-Lydian", "C-LydianDominant", "Phrygian", "D-Phrygian", "E-Phrygian", "G-Phrygian",
                            "Mixolydian", "C-Mixolydian", "D-Mixolydian", "Dorian", "G-Dorian", "A-Dorian", "C-Dorian"
                        };

                        static const char* chordNames[] =
                        {
                            "Z-St.Alfonso1", "Z-St.Alfonso2", "Z-St.AlfonsoInversion", "Z-Watermelon", "Z-Inc", "Z-Inseventh3",
                            "Gsus4", "C-Major", "1st-Inversion", "2nd-Inversion", "Augmented", "D-Augmented", "E-Augmented",
                            "F-Augmented", "G-Augmented", "A-Augmented", "B-Augmented", "Major 7", "Fmaj7", "Gmaj7",
                            "Amaj7", "Dmaj7", "Minor 7", "Amin7", "Dmin7", "Gmin7", "Emin7",
                            "Major", "D-Major", "E-Major", "F-Major", "G-Major", "A-Major", "B-Major",
                            "Minor", "D-Minor", "E-Minor", "F-Minor", "G-Minor", "A-Minor", "B-Minor",
                            "Diminished", "D-Diminished", "E-Diminished", "F-Diminished", "G-Diminished", "A-Diminished", "B-Diminished",
                            "C7-Diminished", "G7-Diminished"
                        };

                        // ===== SINGLE NOTE =====
                        if (result >= 100 && result < 200)
                        {
                            int midiNote = result - 100;
                            src.type = Euclidean_seqAudioProcessor::NoteSourceType::Single;
                            src.value = midiNote;

                            int octave = midiNote / 12 - 1;
                            int note = midiNote % 12;

                            button.setButtonText(juce::String(noteNames[note]) + juce::String(octave));

                            arpNotesPopup.reset(); // closes any open ARP Notes popup

                                        // === THREAD-SAFE ARP UPDATE ===
                            audioProcessor.midiGen.setArpInputNotes(i, { midiNote });
                            audioProcessor.midiGen.setArpNotes(i, { midiNote });
                            updateNoteButton(i);
                            updateArpGui();
                        }
							
                        // ===== SCALE =====
                        else if (result >= 200 && result < 300)
                        {
                            int scaleId = result - 200;
                            src.type = Euclidean_seqAudioProcessor::NoteSourceType::Scale;
                            src.value = scaleId;

                            button.setButtonText("Scale: " + juce::String(scaleNames[scaleId]));
                            arpNotesPopup.reset(); // Closes the ARP popup if open
							
                            // ===== OCTAVE SUBMENU FOR SCALES =====
                            juce::PopupMenu octaveMenu;
                            int selectedOctaveScale = -1; // no default central octave
                            for (int o = 1; o <= 7; ++o)
                            {
                                octaveMenu.addItem(
                                    400 + o,
                                    "Octave " + juce::String(o),
                                    true,
                                    false);
                            }

                            // Show octave menu
                            octaveMenu.showMenuAsync(juce::PopupMenu::Options(),
                                [this, i, scaleId](int octaveResult)
                                {
                                    if (octaveResult < 401 || octaveResult > 407)
                                        return;

                                    int octave = octaveResult - 400;

                                    auto& src = audioProcessor.noteSources[i];
                                    src.type = Euclidean_seqAudioProcessor::NoteSourceType::Scale;
                                    src.value = scaleId;

                                    if (auto* p = audioProcessor.parameters.getParameter(
                                        "rhythm" + juce::String(i) + "_scaleOctave"))
                                    {
                                        p->setValueNotifyingHost((float)(octave - 1) / 6.0f);
                                    }

                                    // Non-transposed notes
                                    auto scaleNotes = audioProcessor.midiGen.getScaleNotes(scaleId);
                                    audioProcessor.midiGen.setArpInputNotes(i, scaleNotes);
                                    audioProcessor.midiGen.setArpNotes(i, scaleNotes);

                                    rhythmRows[i].noteSourceButton.setButtonText(
                                        "Scale: " + juce::String(scaleNames[scaleId]) + " (Oct " + juce::String(octave) + ")");

                                    arpNotesPopup.reset();
                                    updateArpGui();
                                });
                        }
                        // ===== CHORD =====
                        else if (result >= 300)
                        {
                            int chordId = result - 300;
                            src.type = Euclidean_seqAudioProcessor::NoteSourceType::Chord;
                            src.value = chordId;

                            button.setButtonText("Chord: " + juce::String(chordNames[chordId]));
                            arpNotesPopup.reset(); // Closes the ARP popup if open
                            // ===== OCTAVE PER CHORD SUBMENU =====
                            juce::PopupMenu octaveMenuChord;
                            int selectedOctaveChord = -1; // no default central octave
                            for (int o = 1; o <= 7; ++o)
                            {
                                octaveMenuChord.addItem(
                                    500 + o,
                                    "Octave " + juce::String(o));
                            }

                            // Show octave menu
                            octaveMenuChord.showMenuAsync(juce::PopupMenu::Options(),
                                [this, i, chordId](int octaveResult)
                                {
                                    if (octaveResult < 501 || octaveResult > 507)
                                        return;

                                    int octave = octaveResult - 500;

                                    auto& src = audioProcessor.noteSources[i];
                                    src.type = Euclidean_seqAudioProcessor::NoteSourceType::Chord;
                                    src.value = chordId;

                                    if (auto* p = audioProcessor.parameters.getParameter(
                                        "rhythm" + juce::String(i) + "_chordOctave"))
                                    {
                                        p->setValueNotifyingHost((float)(octave - 1) / 6.0f);
                                    }

                                    // Non-transposed notes
                                    auto chordNotes = audioProcessor.midiGen.getChordNotes(chordId);
                                    audioProcessor.midiGen.setArpInputNotes(i, chordNotes);
                                    audioProcessor.midiGen.setArpNotes(i, chordNotes);

                                    rhythmRows[i].noteSourceButton.setButtonText(
                                        "Chord: " + juce::String(chordNames[chordId]) + " (Oct " + juce::String(octave) + ")");

                                    arpNotesPopup.reset();
                                    updateArpGui();
                                });
                        }
                    });
            };

        addAndMakeVisible(row.stepsKnob);
        addAndMakeVisible(row.pulsesKnob);
        addAndMakeVisible(row.swingKnob);
        addAndMakeVisible(row.microKnob);
        addAndMakeVisible(row.velocityKnob);
        addAndMakeVisible(row.noteLenKnob);

        // ARP Buttons
        addAndMakeVisible(row.arpActive);

        addAndMakeVisible(row.arpNotesButton);
        row.arpNotesButton.setButtonText("Notes");
        // ARP Notes button for each row
        row.arpNotesButton.onClick = [this, i]()
            {
                // Closes any pop-up that may already be open
                arpNotesPopup.reset();

                // All available notes for the ARP (input + scale + chord)
                const auto& availableNotes = audioProcessor.midiGen.getArpInputNotes(i);
                const auto& selectedNotes = audioProcessor.midiGen.getArpNotes(i);

                if (availableNotes.empty())
                    return;

                // Create popup component as unique_ptr for JUCE 8
                auto comp = std::make_unique<ArpNotesComponent>(
                    i,
                    availableNotes,
                    selectedNotes,
                    [this, i](std::vector<int> newNotes)
                    {
                        if (newNotes.size() > 4)
                            newNotes.resize(4);

                        audioProcessor.midiGen.setArpNotes(i, newNotes);
                        updateArpGui();
                    });
                juce::CallOutBox::launchAsynchronously(std::move(comp), rhythmRows[i].arpNotesButton.getScreenBounds(), nullptr);
            };

        addAndMakeVisible(row.arpMode);
        row.arpMode.addItem("UP", 1);
        row.arpMode.addItem("DOWN", 2);
        row.arpMode.addItem("UP_DOWN", 3);
        row.arpMode.addItem("RANDOM", 4);

        addAndMakeVisible(row.arpRateBox);
        row.arpRateBox.addItemList({ "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16D" }, 1);

        // ===== MIDI PORT =====
        addAndMakeVisible(row.midiPortBox);
        addAndMakeVisible(row.midiChannelBox);

        // ===== MIDI CHANNEL (1–16) =====
        row.midiChannelBox.clear();
        for (int ch = 1; ch <= 16; ++ch)
            row.midiChannelBox.addItem("Ch " + juce::String(ch), ch);

        row.midiChannelBox.setSelectedId(1, juce::dontSendNotification);

        rhythmAttachments[i].midiChannel =
            std::make_unique<ComboBoxAttachment>(
                audioProcessor.parameters,
                "rhythm" + juce::String(i) + "_midiChannel",
                row.midiChannelBox);

        // Forces the device port to physically open when changing menus and saves the state.
        row.midiPortBox.onChange = [this, i]() {
            auto& box = rhythmRows[i].midiPortBox;
            int selectedId = box.getSelectedId();
            juce::String portName = box.getText();

            // 1. Update the parameter (for saving the .settings file)
            if (auto* p = audioProcessor.parameters.getParameter("rhythm" + juce::String(i) + "_midiPort"))
            {
                float valToSet = (float)(selectedId - 1);
                p->beginChangeGesture();
                p->setValueNotifyingHost(p->getNormalisableRange().convertTo0to1(valToSet));
                p->endChangeGesture();
            }

            // 2. Open/Close the physical device port
            if (selectedId <= 1 || portName == "None")
            {
                audioProcessor.changeMidiOutput(i, "");
            }
            else
            {
                juce::Logger::writeToLog("Cambio MIDI Port Riga " + juce::String(i) + " -> " + portName);
                audioProcessor.changeMidiOutput(i, portName);
            }
            };
    }

    // ===== HOT-PLUG MIDI GUI =====
    auto refreshMidiBox = [this](juce::ComboBox& box, const juce::Array<juce::MidiDeviceInfo>& midiDevices, std::atomic<float>* param)
        {
            int currentId = box.getSelectedId();
            box.clear();

            if (midiDevices.isEmpty())
            {
                box.addItem("No MIDI Outputs", 1);
                box.setSelectedId(1, juce::dontSendNotification);
                return;
            }

            for (int i = 0; i < midiDevices.size(); ++i)
                box.addItem(midiDevices[i].name, i + 1);

            if (param)
            {
                int deviceIndex = (int)param->load();
                if (deviceIndex >= 0 && deviceIndex < midiDevices.size())
                    box.setSelectedId(deviceIndex + 1, juce::dontSendNotification);
                else
                    box.setSelectedId(1, juce::dontSendNotification);
            }
            else
            {
                int deviceIndex = currentId - 1;
                if (deviceIndex >= 0 && deviceIndex < midiDevices.size())
                    box.setSelectedId(deviceIndex + 1, juce::dontSendNotification);
                else
                    box.setSelectedId(1, juce::dontSendNotification);
            }
        };

    audioProcessor.refreshMidiOutputs();
    const auto& midiDevices = audioProcessor.getAvailableMidiOutputs();

    for (int r = 0; r < 6; ++r)
    {
        auto& box = rhythmRows[r].midiPortBox;
        box.clear(juce::dontSendNotification);
        box.addItem("None", 1);

        for (int d = 0; d < midiDevices.size(); ++d)
            box.addItem(midiDevices[d].name, d + 2);

        // Retrieves the saved value (0 = None, 1 = first device, etc.)
        if (auto* p = audioProcessor.parameters.getParameter("rhythm" + juce::String(r) + "_midiPort"))
        {
            float normalizedValue = p->getValue();
            int savedIndex = juce::roundToInt(p->getNormalisableRange().convertFrom0to1(normalizedValue));
            box.setSelectedId(savedIndex + 1, juce::dontSendNotification);

            // If there is a saved device, force open on startup
            if (savedIndex > 0 && (savedIndex - 1) < midiDevices.size())
            {
                audioProcessor.changeMidiOutput(r, midiDevices[savedIndex - 1].name);
            }
        }
    }
    clockBPMAttachment = std::make_unique<SliderAttachment>(
        audioProcessor.parameters, "clockBPM", clockBpmSlider);

    addAndMakeVisible(versionLabel);
    versionLabel.setText(ProjectInfo::versionString, juce::dontSendNotification);
    versionLabel.setJustificationType(juce::Justification::centredRight);
    versionLabel.setFont(juce::Font(14.0f, juce::Font::bold));
    versionLabel.setColour(juce::Label::textColourId, juce::Colours::white);

    for (int i = 0; i < 6; ++i) 
    {
        audioProcessor.parameters.addParameterListener("rhythm" + String(i) + "_reset", this);
    }

    // Initialize buttons with generic text "Note" on startup
    for (int i = 0; i < 6; ++i)
    {
        rhythmRows[i].noteSourceButton.setButtonText("Note");
    }

    // Refresh the display of selected Arp notes
    updateArpGui();

    // Starts the timer for refreshing the MIDI port and buttons
    startTimerHz(60);
}

        // Updates the text of the Notes button of the selected row
void Euclidean_seqAudioProcessorEditor::updateNoteButton(int i)
{
    if (i < 0 || i >= 6) return;

    auto& button = rhythmRows[i].noteSourceButton;
    auto* pNote = audioProcessor.parameters.getRawParameterValue("rhythm" + juce::String(i) + "_note");
    auto* pType = audioProcessor.parameters.getRawParameterValue("rhythm" + juce::String(i) + "_noteSource");

    if (pNote == nullptr || pType == nullptr) return;

    int type = (int)pType->load();
    int val = (int)pNote->load();

    if (type == 0) // Single Note
    {
        // Use standard 3 to ensure C3 = 60
        button.setButtonText(juce::MidiMessage::getMidiNoteName(val, true, true, 3));
    }
    else if (type == 1) // Scale
    {
        static const char* scaleNames[] = { "Z-DMixolydian", "Z-GMixolydian", "Z-PrygianDominant", "Z-Ddorian", "Z-Modified", "Z-CHexatonic", "Z-CDiminished", "Z-CLydianDominant", "Z-APentaMinor1", "Z-APentaMinor2", "Z-BlackPage", "Z-St.Alfonso", "C-Blues1", "C-Blues2", "B-Blues", "A-Blues1", "A-Blues2", "G-Blues", "Min-Jazz", "C7-Jazz", "Am7-Jazz", "Dm7-Jazz", "G7-Jazz", "Cmaj7-Jazz", "Fm7b5-Jazz", "PentaMinor1", "PentaMinor2", "PentaMajor", "Major", "Minor", "Lydian", "G-Major", "D-Major", "F-Major", "A-Major", "A-Minor", "D-Minor", "E-Minor", "F-Minor", "G-Minor", "D-Lydian", "B-Lydian", "C-LydianDominant", "Phrygian", "D-Phrygian", "E-Phrygian", "G-Phrygian", "Mixolydian", "C-Mixolydian", "D-Mixolydian", "Dorian", "G-Dorian", "A-Dorian", "C-Dorian" };

        int octave = 3; // Default
        if (auto* pOct = audioProcessor.parameters.getRawParameterValue("rhythm" + juce::String(i) + "_scaleOctave"))
            octave = juce::roundToInt(pOct->load() * 6.0f);

        juce::String sName = (val < 53) ? scaleNames[val] : "Scale";
        button.setButtonText("S:" + sName.substring(0, 5) + "\nO:" + juce::String(octave));    // It only takes the first 5 characters of the name to fit in the button
    }
    else if (type == 2) // Chord
    {
        static const char* chordNames[] = { "Z-St.Alfonso1", "Z-St.Alfonso2", "Z-St.AlfonsoInversion", "Z-Watermelon", "Z-Inc", "Z-Inseventh3", "Gsus4", "C-Major", "1st-Inversion", "2nd-Inversion", "Augmented", "D-Augmented", "E-Augmented", "F-Augmented", "G-Augmented", "A-Augmented", "B-Augmented", "Major 7", "Fmaj7", "Gmaj7", "Amaj7", "Dmaj7", "Minor 7", "Amin7", "Dmin7", "Gmin7", "Emin7", "Major", "D-Major", "E-Major", "F-Major", "G-Major", "A-Major", "B-Major", "Minor", "D-Minor", "E-Minor", "F-Minor", "G-Minor", "A-Minor", "B-Minor", "Diminished", "D-Diminished", "E-Diminished", "F-Diminished", "G-Diminished", "A-Diminished", "B-Diminished", "C7-Diminished", "G7-Diminished" };

        int octave = 3; // Default
        if (auto* pOct = audioProcessor.parameters.getRawParameterValue("rhythm" + juce::String(i) + "_chordOctave"))
            octave = juce::roundToInt(pOct->load() * 6.0f);

        juce::String cName = (val < 50) ? chordNames[val] : "Chord";
        button.setButtonText("C:" + cName.substring(0, 5) + "\nO:" + juce::String(octave));
    }
}
        // ARP GUI Update
void Euclidean_seqAudioProcessorEditor::updateArpGui()
{
    for (int row = 0; row < 6; ++row)
    {
        auto& arpButton = rhythmRows[row].arpNotesButton;
        const auto& selectedNotes = audioProcessor.midiGen.getArpNotes(row);

        if (selectedNotes.empty())
        {
            arpButton.setButtonText("Select...");
        }
        else
        {
            juce::StringArray names;
            for (int note : selectedNotes)
            {
                // Maintain consistency C3=60 (parameter 3)
                names.add(juce::MidiMessage::getMidiNoteName(note, true, true, 3));
            }
            arpButton.setButtonText(names.joinIntoString(", "));
        }
    }
}

void Euclidean_seqAudioProcessorEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    if (juce::MessageManager::getInstanceWithoutCreating() == nullptr)
        return;

    // Use SafePointer to avoid crashes if the editor is closed while the message is queued.
    juce::Component::SafePointer<Euclidean_seqAudioProcessorEditor> safeThis(this);

    juce::MessageManager::callAsync([safeThis, parameterID]() {
        // If the editor has been removed from memory, safeThis will be null and exit silently.
        if (safeThis == nullptr)
            return;

        if (parameterID.contains("note") || parameterID.contains("active") || parameterID.contains("reset"))
        {
            for (int i = 0; i < 6; ++i)
                safeThis->updateNoteButton(i);

            safeThis->updateArpGui();
        }
        });
}

Euclidean_seqAudioProcessorEditor::~Euclidean_seqAudioProcessorEditor() {}

void Euclidean_seqAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
    g.addTransform(juce::AffineTransform::scale(guiScale));
}

void Euclidean_seqAudioProcessorEditor::resized()
{
    // ===== AUTO SCALE CALCULATION =====
    const float scaleX = (float)getWidth() / (float)baseWidth;
    const float scaleY = (float)getHeight() / (float)baseHeight;
    guiScale = juce::jmin(scaleX, scaleY);
    repaint();

    const int headerHeight = 40;
    const int rowHeight = 90;
    const int knobSize = 100;

    // ================= COLUMN DEFINITIONS =================
    const int startX = 40;
    const int colSpacing = 85;

    enum Column
    {
        COL_ACTIVE = 0,
        COL_NOTE,
        COL_STEPS,
        COL_PULSES,
        COL_SWING,
        COL_MICRO,
        COL_VELOCITY,
        COL_NOTELEN,
        COL_ARP_ON,
        COL_ARP_NOTES,
        COL_ARP_MODE,
        COL_ARP_RATE,
        COL_MIDI_PORT,
        COL_MIDI_CH,
        NUM_COLUMNS
    };

    int columnX[NUM_COLUMNS];
    for (int c = 0; c < NUM_COLUMNS; ++c)
        columnX[c] = startX + c * colSpacing;

    // ===== HEADER =====
    for (int i = 0; i < NUM_COLUMNS; ++i)
    {
        headerLabels[i].setBounds(
            columnX[i] - 20,
            0,
            colSpacing,
            headerHeight);
    }

    // ================= TRANSPORT =================
    const int bottomMargin = 50;
    const int buttonHeight = 30;

    clockSettingsButton.setBounds(getWidth() - 180, getHeight() - bottomMargin, 170, buttonHeight);
    clockBpmSlider.setBounds(getWidth() - 360, getHeight() - bottomMargin, 170, buttonHeight);
    playButton.setBounds((getWidth() - 90) / 2, getHeight() - bottomMargin, 90, buttonHeight);

    // ================= RHYTHM ROWS =================
    const int rowGapPx = juce::roundToInt(5.0f * mmToPx * getDesktopScaleFactor());

    for (int r = 0; r < 6; ++r)
    {
        auto& row = rhythmRows[r];

        int y = headerHeight + r * rowHeight;
        if (r > 0)
            y += r * rowGapPx;

        // Row label
        row.rowLabel.setBounds(columnX[COL_ACTIVE] - 35, y, 30, 20);

        // Active Button
        row.activeButton.setBounds(columnX[COL_ACTIVE] - 2, y+10, 30, 30);
        row.activeButton.setClickingTogglesState(true);

        // M/S/R below Active
        const int subY = y + 45;
        row.muteButton.setBounds(columnX[COL_ACTIVE] - 6, subY, 24, 24);
        row.soloButton.setBounds(columnX[COL_ACTIVE] + 22, subY, 24, 24);
        row.resetButton.setBounds(columnX[COL_ACTIVE] + 50, subY, 24, 24);

        if (r == 5)
            bassSettingsButton.setBounds(columnX[COL_ACTIVE] - 6, subY + 30, 80, 26);

        // Note source
        row.noteSourceButton.setBounds(columnX[COL_NOTE], y, 80, 80);

        // Knobs
        row.stepsKnob.setBounds(columnX[COL_STEPS], y, knobSize, knobSize);
        row.pulsesKnob.setBounds(columnX[COL_PULSES], y, knobSize, knobSize);
        row.swingKnob.setBounds(columnX[COL_SWING], y, knobSize, knobSize);
        row.microKnob.setBounds(columnX[COL_MICRO], y, knobSize, knobSize);
        row.velocityKnob.setBounds(columnX[COL_VELOCITY], y, knobSize, knobSize);
        row.noteLenKnob.setBounds(columnX[COL_NOTELEN], y, knobSize, knobSize);

        // ARP
        row.arpActive.setBounds(columnX[COL_ARP_ON] - 2, y, 26, 26);
        row.arpNotesButton.setBounds(columnX[COL_ARP_NOTES], y, 80, 25);
        row.arpMode.setBounds(columnX[COL_ARP_MODE], y, 80, 25);
        row.arpRateBox.setBounds(columnX[COL_ARP_RATE], y, 80, 25);

		// MIDI OUTPUT
        row.midiPortBox.setBounds(columnX[COL_MIDI_PORT], y, 90, 25);
        row.midiChannelBox.setBounds(columnX[COL_MIDI_CH] + 10, y, 60, 25);
    }

    versionLabel.setBounds(getWidth() - 120, 5, 110, 20);
}

void Euclidean_seqAudioProcessorEditor::openClockSettingsPopup() {
    bool isStandalone = juce::JUCEApplication::isStandaloneApp();

    int clockSourceValue = 0;
    if (auto* p = audioProcessor.parameters.getRawParameterValue("clockSource"))
        clockSourceValue = (int)p->load();

    auto* dialog = new ClockSettingsDialog(
        [this, isStandalone](int selectedId)
        {
            if (selectedId <= 0) return;

            // Mapping Standalone: 1(Internal) -> Value 1, 2(External) -> Value 2
            // Mapping DAW: 1(DAW) -> Value 0, 2(Internal) -> Value 1, 3(External) -> Value 2
            int targetValue = isStandalone ? selectedId : selectedId - 1;

            if (auto* param = audioProcessor.parameters.getParameter("clockSource"))
            {
                param->beginChangeGesture();
                // Use setValueNotifyingHost with the correct normalized value for JUCE 8
                param->setValueNotifyingHost(param->getNormalisableRange().convertTo0to1((float)targetValue));
                param->endChangeGesture();

                audioProcessor.updateClockSource();
                repaint();
            }
        },
        isStandalone,
        clockSourceValue
    );

    dialog->setSize(220, 100);
    clockPopup.reset(new juce::CallOutBox(*dialog, clockSettingsButton.getScreenBounds(), nullptr));
    clockPopup->setAlwaysOnTop(true);
    clockPopup->enterModalState(true);
}

void Euclidean_seqAudioProcessorEditor::updateMidiOutputs()
{
    audioProcessor.refreshMidiOutputs();
    const auto& midiDevices = audioProcessor.getAvailableMidiOutputs();

    for (int i = 0; i < 6; ++i)
    {
        auto& box = rhythmRows[i].midiPortBox;
        juce::String currentPort = box.getText();

        box.clear(juce::dontSendNotification);
        box.addItem("None", 1);

        bool stillExists = false;
        for (int d = 0; d < midiDevices.size(); ++d)
        {
            box.addItem(midiDevices[d].name, d + 2);
            if (midiDevices[d].name == currentPort)
                stillExists = true;
        }

        if (stillExists)
            box.setText(currentPort, juce::dontSendNotification);
        else if (currentPort == "None")
            box.setSelectedId(1, juce::dontSendNotification);
        // If the device is gone, do not forcefully change anything to avoid glitches
    }
}

void Euclidean_seqAudioProcessorEditor::timerCallback()
{
    static int midiCounter = 0;
    if (++midiCounter >= 60) // Runs approximately every second
    {
        updateMidiOutputs(); // Update MIDI Channel ComboBoxes

        // Gets the list of system devices
        auto midiDevices = juce::MidiOutput::getAvailableDevices();

        for (int r = 0; r < 6; ++r)
        {
            auto& box = rhythmRows[r].midiPortBox;
            if (box.getNumItems() != (int)midiDevices.size() + 1)
            {
                int currentId = box.getSelectedId();
                box.clear(juce::dontSendNotification);
                box.addItem("None", 1);
                for (int d = 0; d < (int)midiDevices.size(); ++d)
                    box.addItem(midiDevices[d].name, d + 2);

                if (currentId > 0 && currentId <= box.getNumItems())
                    box.setSelectedId(currentId, juce::dontSendNotification);
                else
                    box.setSelectedId(1, juce::dontSendNotification);
            }
        }
        midiCounter = 0;
    }
}
