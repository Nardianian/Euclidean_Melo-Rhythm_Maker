#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================

Euclidean_seqAudioProcessorEditor::Euclidean_seqAudioProcessorEditor(Euclidean_seqAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    setSize(1350, 765);

    addAndMakeVisible(clockSettingsButton);
    clockSettingsButton.onClick = [this]()
        {
            openClockSettingsPopup();
        };
    // ================= GLOBAL PLAY =================
    addAndMakeVisible(playButton);
    playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);
    playButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    playButton.setClickingTogglesState(true);
    playButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
    playButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::green);
    playButton.setColour(juce::TextButton::textColourOnId, juce::Colours::black);
    playButton.setTooltip("Global Play / Stop");

    playAttachment = std::make_unique<ButtonAttachment>(
        audioProcessor.parameters,
        "globalPlay",
        playButton);

    // ===== Bass (R6) Settings Button =====
    addAndMakeVisible(bassSettingsButton);
    bassSettingsButton.setTooltip("Open Bass (R6) Settings");

    bassSettingsButton.onClick = [this]()
        {
            auto* bassComp = new BassSettingsComponent(audioProcessor.parameters);
            bassComp->setSize(320, 260);

            bassPopup.reset(new juce::CallOutBox(
                *bassComp,
                bassSettingsButton.getScreenBounds(),
                nullptr));

            bassPopup->setAlwaysOnTop(true);
            bassPopup->enterModalState(true);
        };

    // ===================== RANDOMIZE BUTTON =====================
    addAndMakeVisible(randomizeButton);
    randomizeButton.setButtonText("Randomize");
    randomizeButton.onClick = [this]()
        {
            for (int r = 0; r < 6; ++r)
            {
                auto& row = rhythmRows[r];
                auto& params = audioProcessor.parameters;

                // Random note
                if (auto* noteParam = params.getParameter("rhythm" + juce::String(r) + "_note"))
                    noteParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat());

                // Random steps
                if (auto* stepsParam = params.getParameter("rhythm" + juce::String(r) + "_steps"))
                    stepsParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat());

                // Random pulses
                if (auto* pulsesParam = params.getParameter("rhythm" + juce::String(r) + "_pulses"))
                    pulsesParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat());

                // Random swing
                if (auto* swingParam = params.getParameter("rhythm" + juce::String(r) + "_swing"))
                    swingParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat());

                // Random microtiming
                if (auto* microParam = params.getParameter("rhythm" + juce::String(r) + "_microtiming"))
                    microParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat() * 2.0f - 1.0f); // [-1,1]

                // Random velocity
                if (auto* velParam = params.getParameter("rhythm" + juce::String(r) + "_velocityAccent"))
                    velParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat());

                // Random note length
                if (auto* lenParam = params.getParameter("rhythm" + juce::String(r) + "_noteLength"))
                    lenParam->setValueNotifyingHost(juce::Random::getSystemRandom().nextFloat());
            }
        };

    //==================== CLOCK BPM SLIDER ====================
    addAndMakeVisible(clockBpmSlider);
    clockBpmSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    clockBpmSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 60, 20);

    // Header labels
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

    // Rhythm rows
    for (int i = 0; i < 6; ++i)
    {
        auto& row = rhythmRows[i];
        // ===== NOTE SOURCE BUTTON (Single / Scale / Chord) PRIMA PARTE =====
        addAndMakeVisible(row.noteSourceButton);
        row.noteSourceButton.setTooltip("Select Note, Scale or Chord source");

        auto setupRotary = [](juce::Slider& s)
            {
                s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
                s.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 42, 16);
                // Garantisce visibilità su Windows 11 / LookAndFeel V4
                s.setColour(juce::Slider::rotarySliderFillColourId, juce::Colours::orange);
                s.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::black);
                s.setColour(juce::Slider::thumbColourId, juce::Colours::white);
                s.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
                s.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            };

        addAndMakeVisible(row.activeButton);

        row.rowLabel.setText("R" + juce::String(i + 1), juce::dontSendNotification);
        row.rowLabel.setJustificationType(juce::Justification::centredLeft);
        addAndMakeVisible(row.rowLabel);

        // ===== NOTE SOURCE BUTTON (Single / Scale / Chord) =====
        row.noteSourceButton.onClick = [this, i]()
            {
                juce::PopupMenu menu;

                // ================= SINGLE NOTES =================
                juce::PopupMenu noteMenu;
                for (int note = 21; note <= 108; ++note) 
                    noteMenu.addItem(note, juce::MidiMessage::getMidiNoteName(note, true, true, 3));
                menu.addSubMenu("Single Note", noteMenu);

                // ================= SCALES =================
                juce::PopupMenu scaleMenu;
                scaleMenu.addItem(1001, "Major");
                scaleMenu.addItem(1002, "Minor");
                scaleMenu.addItem(1003, "Dorian");
                scaleMenu.addItem(1004, "Phrygian");
                scaleMenu.addItem(1005, "Lydian");
                scaleMenu.addItem(1006, "Mixolydian");
                scaleMenu.addItem(1007, "Locrian");
                scaleMenu.addItem(1008, "Pentatonic Major");
                scaleMenu.addItem(1009, "Pentatonic Minor");
                menu.addSubMenu("Scale", scaleMenu);

                // ================= CHORDS =================
                juce::PopupMenu chordMenu;
                chordMenu.addItem(2001, "Major");
                chordMenu.addItem(2002, "Minor");
                chordMenu.addItem(2003, "Diminished");
                chordMenu.addItem(2004, "Augmented");
                chordMenu.addItem(2005, "Major 7");
                chordMenu.addItem(2006, "Minor 7");
                chordMenu.addItem(2007, "Dominant 7");
                chordMenu.addItem(2008, "Suspended 2");
                chordMenu.addItem(2009, "Suspended 4");
                menu.addSubMenu("Chord", chordMenu);

                // ================= CALLBACK =================
                menu.showMenuAsync(
                    juce::PopupMenu::Options(),
                    [this, i](int result)
                    {
                        if (result <= 0)
                            return;

                        auto& button = rhythmRows[i].noteSourceButton;

                        // ===== FEEDBACK GUI =====
                        if (result < 1000)
                            button.setButtonText(juce::MidiMessage::getMidiNoteName(result, true, true, 3));
                        else if (result < 2000)
                            button.setButtonText("Scale");
                        else
                            button.setButtonText("Chord");

                        // ===== AGGIORNAMENTO PARAMETRO =====
                        if (auto* param = audioProcessor.parameters.getParameter("rhythm" + juce::String(i) + "_note"))
                        {
                            // Normalizza valore fra 0.0f e 1.0f se necessario, oppure usa direttamente l'ID
                            param->setValueNotifyingHost((float)result);
                        }
                    });
            };

        setupRotary(row.stepsKnob);
        setupRotary(row.pulsesKnob);
        setupRotary(row.swingKnob);
        setupRotary(row.microKnob);
        setupRotary(row.velocityKnob);
        setupRotary(row.noteLenKnob);

        addAndMakeVisible(row.stepsKnob);
        addAndMakeVisible(row.pulsesKnob);
        addAndMakeVisible(row.swingKnob);
        addAndMakeVisible(row.microKnob);
        addAndMakeVisible(row.velocityKnob);
        addAndMakeVisible(row.noteLenKnob);

        // ARP
        addAndMakeVisible(row.arpActive);
        row.arpMode.addItem("UP", 1);
        row.arpMode.addItem("DOWN", 2);
        row.arpMode.addItem("UP_DOWN", 3);
        row.arpMode.addItem("RANDOM", 4);
        addAndMakeVisible(row.arpMode);
        row.arpRateBox.addItemList(
            { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32", "1/8T", "1/16D" }, 1);
        addAndMakeVisible(row.arpRateBox);

        addAndMakeVisible(row.arpNotesButton);
        row.arpNotesButton.setButtonText("Notes");
        row.arpNotesButton.onClick = [this, i]()
            {
                juce::PopupMenu menu;

                // Note realmente ricevute dal ritmo
                auto inputNotes = audioProcessor.midiGen.getArpInputNotes(i);
                auto& slots = audioProcessor.midiGen.arpNoteSlots[i];

                if (inputNotes.empty())
                {
                    menu.addItem(1, "No notes received yet", false);
                }
                else
                {
                    for (int midiNote : inputNotes)
                    {
                        bool selected = false;

                        for (int s = 0; s < 7; ++s)
                        {
                            if (slots[s].load() == midiNote)
                            {
                                selected = true;
                                break;
                            }
                        }

                        menu.addItem(
                            1 + midiNote,
                            juce::MidiMessage::getMidiNoteName(midiNote, true, true, 3),
                            true,
                            selected);
                    }
                }

                menu.showMenuAsync(
                    juce::PopupMenu::Options(),
                    [this, i](int result)
                    {
                        if (result <= 0)
                            return;

                        int midiNote = result - 1;
                        auto& slots = audioProcessor.midiGen.arpNoteSlots[i];

                        // Toggle select / deselect
                        for (int s = 0; s < 7; ++s)
                        {
                            if (slots[s].load() == midiNote)
                            {
                                slots[s].store(-1);
                                return;
                            }
                        }

                        // Add if free slot exists
                        for (int s = 0; s < 7; ++s)
                        {
                            if (slots[s].load() < 0)
                            {
                                slots[s].store(midiNote);
                                return;
                            }
                        }
                    });
            };

        // MIDI
        addAndMakeVisible(row.midiPortLabel);
        row.midiPortLabel.setText("Port", juce::dontSendNotification);
        addAndMakeVisible(row.midiPortBox);
        addAndMakeVisible(row.midiChannelLabel);
        row.midiChannelLabel.setText("Ch", juce::dontSendNotification);
        addAndMakeVisible(row.midiChannelBox);
        row.midiChannelBox.clear();
        for (int ch = 1; ch <= 16; ++ch)
            row.midiChannelBox.addItem(juce::String(ch), ch);

        clockSettingsButton.setTooltip("Open Clock Settings");
        clockBpmSlider.setTooltip("Set Internal Clock BPM");
        for (int r = 0; r < 6; ++r)
        {
            rhythmRows[r].noteSourceButton.setTooltip("Select Note, Scale or Chord source");
            rhythmRows[r].stepsKnob.setTooltip("Number of steps");
            rhythmRows[r].pulsesKnob.setTooltip("Number of pulses in pattern");
            rhythmRows[r].swingKnob.setTooltip("Swing amount");
            rhythmRows[r].microKnob.setTooltip("Microtiming offset");
            rhythmRows[r].velocityKnob.setTooltip("Velocity / Accent");
            rhythmRows[r].noteLenKnob.setTooltip("Note length");
            rhythmRows[r].arpActive.setTooltip("Enable arpeggiator");
            rhythmRows[r].arpMode.setTooltip("ARP Mode (UP, DOWN, RANDOM, etc.)");
            rhythmRows[r].arpRateBox.setTooltip("ARP musical rate (1/4, 1/8T, 1/16D, etc.)");
            rhythmRows[r].arpNotesButton.setTooltip("Number of ARP notes");
            rhythmRows[r].midiPortBox.setTooltip("Select MIDI Output Port");
            rhythmRows[r].midiChannelBox.setTooltip("Select MIDI Channel");
        }

        addAndMakeVisible(row.muteButton);
        addAndMakeVisible(row.soloButton);
        addAndMakeVisible(row.resetButton);

        // ===== Force text visibility for M / S =====
        row.muteButton.setButtonText("M");
        row.soloButton.setButtonText("S");

        // ===== MUTE / SOLO as illuminated buttons =====
        row.muteButton.setClickingTogglesState(true);
        row.soloButton.setClickingTogglesState(true);

        auto& att = rhythmAttachments[i];
        auto& params = audioProcessor.parameters;

        att.active = std::make_unique<ButtonAttachment>(
            params, "rhythm" + juce::String(i) + "_active", row.activeButton);

        // Colore bottone normale
        row.muteButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);
        row.soloButton.setColour(juce::TextButton::buttonColourId, juce::Colours::darkgrey);

        // Colore bottone quando premuto
        row.muteButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkred);
        row.soloButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::darkgreen);

        // Colore testo
        row.muteButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        row.soloButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
        row.muteButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);
        row.soloButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

        att.mute = std::make_unique<ButtonAttachment>(
            params, "rhythm" + juce::String(i) + "_mute", row.muteButton);

        att.solo = std::make_unique<ButtonAttachment>(
            params, "rhythm" + juce::String(i) + "_solo", row.soloButton);

        att.reset = std::make_unique<ButtonAttachment>(
            params, "rhythm" + juce::String(i) + "_reset", row.resetButton);

        att.steps = std::make_unique<SliderAttachment>(
            params, "rhythm" + juce::String(i) + "_steps", row.stepsKnob);

        att.pulses = std::make_unique<SliderAttachment>(
            params, "rhythm" + juce::String(i) + "_pulses", row.pulsesKnob);

        att.swing = std::make_unique<SliderAttachment>(
            params, "rhythm" + juce::String(i) + "_swing", row.swingKnob);

        att.micro = std::make_unique<SliderAttachment>(
            params, "rhythm" + juce::String(i) + "_microtiming", row.microKnob);

        att.velocity = std::make_unique<SliderAttachment>(
            params, "rhythm" + juce::String(i) + "_velocityAccent", row.velocityKnob);

        att.noteLen = std::make_unique<SliderAttachment>(
            params, "rhythm" + juce::String(i) + "_noteLength", row.noteLenKnob);

        att.arpActive = std::make_unique<ButtonAttachment>(
            params, "rhythm" + juce::String(i) + "_arpActive", row.arpActive);

        att.arpMode = std::make_unique<ComboBoxAttachment>(
            params, "rhythm" + juce::String(i) + "_arpMode", row.arpMode);

        att.arpRate = std::make_unique<ComboBoxAttachment>(
            params, "rhythm" + juce::String(i) + "_arpRate", row.arpRateBox);

        // MIDI Port
        att.midiPort = std::make_unique<ComboBoxAttachment>(
            params, "rhythm" + juce::String(i) + "_midiPort", row.midiPortBox);

        // MIDI Channel
        att.midiChannel = std::make_unique<ComboBoxAttachment>(
            params, "rhythm" + juce::String(i) + "_midiChannel", row.midiChannelBox);
    }

    // ================= MIDI OUTPUT PORT ENUMERATION =================
    auto refreshMidiBox = [this](juce::ComboBox& box, const juce::Array<juce::MidiDeviceInfo>& midiDevices, std::atomic<float>* param)
        {
            int currentId = box.getSelectedId();  // salva selezione corrente
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
                // fallback: ripristina selezione se ancora valida
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
        auto* param = audioProcessor.parameters.getRawParameterValue("rhythm" + juce::String(r) + "_midiPort");
        refreshMidiBox(rhythmRows[r].midiPortBox, midiDevices, param);
    }

    clockBPMAttachment = std::make_unique<SliderAttachment>(
        audioProcessor.parameters, "clockBPM", clockBpmSlider);

    startTimerHz(60);
}

void Euclidean_seqAudioProcessorEditor::updateArpGui()
{
    // Aggiorna la GUI leggendo direttamente le note dagli arpNotes
    for (int row = 0; row < 6; ++row)
    {
        const auto& notes = audioProcessor.midiGen.arpNotes[row];

        // Stampa su console le note (solo per debug, sostituire con GUI reale)
        DBG("Ritmo " << row << " ARP Notes: ");
        for (auto note : notes)
            DBG(" " << note);
    }

    // Ridisegna editor
    repaint();
}



Euclidean_seqAudioProcessorEditor::~Euclidean_seqAudioProcessorEditor() {}

void Euclidean_seqAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::darkgrey);
}

void Euclidean_seqAudioProcessorEditor::openClockSettingsPopup()
{
    auto* dialog = new ClockSettingsDialog([this](int newSource)
        {
            if (auto* param = audioProcessor.parameters.getParameter("clockSource"))
                param->setValueNotifyingHost(newSource - 1); // ComboBox ID 1 = DAW = value 0
        });

    dialog->setSize(220, 100);

    // Ancora il popup SOPRA il bottone ClockSettings
    clockPopup.reset(new juce::CallOutBox(
        *dialog,
        clockSettingsButton.getScreenBounds(),
        nullptr));

    clockPopup->setAlwaysOnTop(true);
    clockPopup->enterModalState(true);
}

void Euclidean_seqAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(10);

    const int headerHeight = 40;
    const int rowHeight = 90;   // Altezza della riga per la prima riga
    const int rowHeightWithSpacing = 110;   // Aggiungiamo un margine (con altezza extra) per le righe 2-6
    const int knobSize = 100;
    const int valueHeight = 15;

    // ================= COLUMN DEFINITIONS =================
    const int startX = 40;   // margine sinistro
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

    // ================= HEADER =================
    for (int i = 0; i < NUM_COLUMNS; ++i)
    {
        headerLabels[i].setBounds(
            columnX[i] - 20,
            0,
            colSpacing,
            headerHeight);
    }

// ================= CLOCK + TRANSPORT =================

    const int bottomMargin = 50;
    const int buttonHeight = 30;
    const int playButtonWidth = 90;
    const int playClockSpacing = 10; // spazio tra PLAY e Clock Settings

    // Clock (destra)
    clockSettingsButton.setBounds(
        getWidth() - 180,
        getHeight() - bottomMargin,
        170,
        buttonHeight);

    clockBpmSlider.setBounds(
        getWidth() - 360,
        getHeight() - bottomMargin,
        170,
        buttonHeight);

    // PLAY (in basso al centro)
    playButton.setBounds(
        (getWidth() - playButtonWidth) / 2,  // Posizionamento centrato orizzontalmente
        getHeight() - bottomMargin,          // Allineato in basso
        playButtonWidth,
        buttonHeight);

    // ================= RHYTHM ROWS =================
    // 5 mm reali per OGNI riga (DPI aware)
    const float mmToPx = 96.0f / 25.4f;
    const int rowGapPx = juce::roundToInt(5.0f * mmToPx * getDesktopScaleFactor());

    for (int r = 0; r < 6; ++r)
    {
        auto& row = rhythmRows[r];

        int y = headerHeight + r * rowHeight;

        // Sposta SOLO da R2 in poi
        if (r >= 1)
            y += r * rowGapPx;

        // Row label R1–R6
        row.rowLabel.setBounds(columnX[COL_ACTIVE] - 30, y + 6, 25, 20);

        // Row Active Button (mantieni solo il primo)
        row.activeButton.setBounds(columnX[COL_ACTIVE] - 2, y, 26, 26);  // (Box attivazione 1)

        // M / S / R sotto ACTIVE (sotto R1–R6)
        const int subY = y + 30;
        const int subX = columnX[COL_ACTIVE] - 6;

        row.muteButton.setBounds(subX, subY, 20, 20);
        row.soloButton.setBounds(subX + 22, subY, 20, 20);
        row.resetButton.setBounds(subX + 44, subY, 20, 20);

        // Bass Settings Button (only for R6) - ANCORATO a M/S/R
        if (r == 5)
        {
            const int msrHeight = 20;
            const int msrSpacing = 6;
            bassSettingsButton.setBounds(
                subX,
                subY + msrHeight + msrSpacing,
                66,
                22);
        }

        // Note (Notes Scales Cords) Button
        row.noteSourceButton.setBounds(columnX[COL_NOTE], y, knobSize, knobSize);

        // Rotary knobs + value boxes
        // Distanziamo i rotary knobs dai box sottostanti
        const int knobSpacing = 10;  // Aggiungiamo un piccolo spazio tra knob e box valore
        row.stepsKnob.setBounds(columnX[COL_STEPS], y, knobSize, knobSize);
        row.pulsesKnob.setBounds(columnX[COL_PULSES], y, knobSize, knobSize);
        row.swingKnob.setBounds(columnX[COL_SWING], y, knobSize, knobSize);
        row.microKnob.setBounds(columnX[COL_MICRO], y, knobSize, knobSize);
        row.velocityKnob.setBounds(columnX[COL_VELOCITY], y, knobSize, knobSize);
        row.noteLenKnob.setBounds(
            columnX[COL_NOTELEN],          // Assicurati che columnX sia trattato come un int
            y,                             // Cast esplicito per la somma
            knobSize,                      // Cast esplicito per knobSize
            knobSize                       // Cast esplicito per knobSize
        );

        // Arp On
        row.arpActive.setBounds(columnX[COL_ARP_ON] - 2, y, 26, 26);

        // Arp Notes
        row.arpNotesButton.setBounds(columnX[COL_ARP_NOTES], y, 80, 25);

        // Arp Mode
        row.arpMode.setBounds(columnX[COL_ARP_MODE], y, 80, 25);

        // Arp Rate
        row.arpRateBox.setBounds(columnX[COL_ARP_RATE], y, 80, 25);

        // MIDI
        row.midiPortBox.setBounds(columnX[COL_MIDI_PORT], y, 80, 25);
        row.midiChannelBox.setBounds(columnX[COL_MIDI_CH], y, 60, 25);
    }
}

void Euclidean_seqAudioProcessorEditor::timerCallback()
{
    // ===== HOT-PLUG MIDI (GUI THREAD SAFE) =====
    audioProcessor.refreshMidiOutputs();
    const auto& midiDevices = audioProcessor.getAvailableMidiOutputs();

    for (int r = 0; r < 6; ++r)
    {
        auto& box = rhythmRows[r].midiPortBox;

        // Salva selezione corrente (ComboBox ID)
        int currentId = box.getSelectedId();

        // Ricostruisci lista
        box.clear();
        for (int i = 0; i < midiDevices.size(); ++i)
            box.addItem(midiDevices[i].name, i + 1);

        if (midiDevices.isEmpty())
            box.addItem("No MIDI Outputs", 1);

        // Ripristina selezione se ancora valida
        int deviceIndex = currentId - 1; // ID → index
        if (deviceIndex >= 0 && deviceIndex < midiDevices.size())
            box.setSelectedId(deviceIndex + 1, juce::dontSendNotification);
        else
            box.setSelectedId(0, juce::dontSendNotification);
    }

    updateArpGui(); // chiama la funzione che legge le arpNotes

    // ===== GUI REFRESH =====
    repaint();
}





