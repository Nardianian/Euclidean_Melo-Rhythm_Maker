#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Euclidean_seqAudioProcessor::Euclidean_seqAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    parameters(*this, nullptr, "Params", createParameterLayout())
{
    // ===== ENUMERATE MIDI OUTPUT PORTS =====
    availableMidiOutputs = juce::MidiOutput::getAvailableDevices();
    refreshMidiOutputs();

    // ===== INIT RHYTHM MIDI STATE =====
    for (auto& r : rhythmMidiOuts)
    {
        r.selectedPortIndex = -1;
        r.selectedChannel = 1;
    }

    // Initialize default rhythm patterns and arp notes
    midiGen.setClockManager(&clock);
    for (int i = 0; i < 6; ++i)
    {
        midiGen.rhythmPatterns[i].setPattern(16, 4);
        midiGen.rhythmArps[i].setType(ArpType::UP);
        midiGen.arpNotes[i].resize(4, 60);
    }
}

Euclidean_seqAudioProcessor::~Euclidean_seqAudioProcessor() {}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout
Euclidean_seqAudioProcessor::createParameterLayout()
{
    using namespace juce;

    AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < 6; ++i)
    {
        layout.add(std::make_unique<AudioParameterBool>(
            "rhythm" + String(i) + "_mute", "Mute", false));

        layout.add(std::make_unique<AudioParameterBool>(
            "rhythm" + String(i) + "_solo", "Solo", false));

        layout.add(std::make_unique<AudioParameterBool>(
            "rhythm" + String(i) + "_reset", "Reset", false));

        layout.add(std::make_unique<AudioParameterBool>(
            "rhythm" + String(i) + "_active", "Active", true));

        layout.add(std::make_unique<AudioParameterFloat>(
            "rhythm" + String(i) + "_note", "Note",
            NormalisableRange<float>(0.0f, 127.0f), 60.0f));

        layout.add(std::make_unique<AudioParameterInt>(
            "rhythm" + String(i) + "_noteSource", "Note Source Type", 0, 2, 0));

        // ===== SCALE OCTAVE (1–7) =====
        layout.add(std::make_unique<AudioParameterInt>(
            "rhythm" + juce::String(i) + "_scaleOctave",
            "Scale Octave",
            1, 7, 4));

        // ===== CHORD OCTAVE (1–7) =====
        layout.add(std::make_unique<AudioParameterInt>(
            "rhythm" + juce::String(i) + "_chordOctave",
            "Chord Octave",
            1, 7, 4));

        layout.add(std::make_unique<AudioParameterFloat>(
            "rhythm" + String(i) + "_steps", "Steps",
            NormalisableRange<float>(1.0f, 32.0f), 16.0f));

        layout.add(std::make_unique<AudioParameterFloat>(
            "rhythm" + String(i) + "_pulses", "Pulses",
            NormalisableRange<float>(0.0f, 32.0f), 4.0f));

        layout.add(std::make_unique<AudioParameterFloat>(
            "rhythm" + String(i) + "_swing", "Swing",
            NormalisableRange<float>(0.0f, 1.0f), 0.0f));

        layout.add(std::make_unique<AudioParameterFloat>(
            "rhythm" + String(i) + "_microtiming", "Microtiming",
            NormalisableRange<float>(-0.5f, 0.5f), 0.0f));

        layout.add(std::make_unique<AudioParameterFloat>(
            "rhythm" + String(i) + "_velocityAccent", "Velocity",
            NormalisableRange<float>(0.0f, 127.0f), 100.0f));

        layout.add(std::make_unique<AudioParameterFloat>(
            "rhythm" + String(i) + "_noteLength", "Note Length",
            NormalisableRange<float>(0.0f, 1.0f), 0.5f));

        layout.add(std::make_unique<AudioParameterBool>(
            "rhythm" + String(i) + "_arpActive", "ARP Active", false));

        juce::StringArray arpModes;
        arpModes.add("UP");
        arpModes.add("DOWN");
        arpModes.add("UP_DOWN");
        arpModes.add("RANDOM");

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            "rhythm" + juce::String(i) + "_arpMode",
            "ARP Mode",
            arpModes,
            0));

        juce::StringArray arpRateChoices;
        arpRateChoices.add("1/1");
        arpRateChoices.add("1/2");
        arpRateChoices.add("1/4");
        arpRateChoices.add("1/8");
        arpRateChoices.add("1/16");
        arpRateChoices.add("1/32");
        arpRateChoices.add("1/8T");
        arpRateChoices.add("1/16D");

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            "rhythm" + juce::String(i) + "_arpRate",
            "ARP Rate",
            arpRateChoices,
            2));

        layout.add(std::make_unique<AudioParameterInt>(
            "rhythm" + String(i) + "_arpNotesMask",
            "ARP Notes Mask",
            0,
            127,
            0b0001111)); // default: first 4 active notes

        // ===== MIDI PORT (dynamic, aligned to the ComboBox) =====
        juce::StringArray midiPortNames;
        auto midiDevices = juce::MidiOutput::getAvailableDevices();

        for (const auto& dev : midiDevices)
            midiPortNames.add(dev.name);

        if (midiPortNames.isEmpty())
            midiPortNames.add("No MIDI Outputs");

        // ===== MIDI OUTPUT PORT (for rhythm) =====
        layout.add(std::make_unique<juce::AudioParameterChoice>(
            "rhythm" + juce::String(i) + "_midiPort",
            "MIDI Port",
            midiPortNames,
            0));

        // MIDI Channel
        juce::StringArray midiChannelChoices;
        midiChannelChoices.add("1");
        midiChannelChoices.add("2");
        midiChannelChoices.add("3");
        midiChannelChoices.add("4");
        midiChannelChoices.add("5");
        midiChannelChoices.add("6");
        midiChannelChoices.add("7");
        midiChannelChoices.add("8");
        midiChannelChoices.add("9");
        midiChannelChoices.add("10");
        midiChannelChoices.add("11");
        midiChannelChoices.add("12");
        midiChannelChoices.add("13");
        midiChannelChoices.add("14");
        midiChannelChoices.add("15");
        midiChannelChoices.add("16");

        layout.add(std::make_unique<juce::AudioParameterChoice>(
            "rhythm" + juce::String(i) + "_midiChannel",
            "MIDI Channel",
            midiChannelChoices,
            0));
    }

    juce::StringArray clockSourceChoices;
    clockSourceChoices.add("Internal");
    clockSourceChoices.add("External");
    clockSourceChoices.add("DAW");

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        juce::ParameterID("clockSource", 1),
        "Clock Source",
        clockSourceChoices,
        0)); // Default su Internal

    layout.add(std::make_unique<AudioParameterFloat>(
        "clockBPM", "Clock BPM",
        NormalisableRange<float>(40.0f, 300.0f), 120.0f));

    layout.add(std::make_unique<AudioParameterFloat>(
        "bassGlideTime",
        "Bass Glide Time",
        NormalisableRange<float>(0.01f, 0.5f),
        0.08f));

    layout.add(std::make_unique<AudioParameterFloat>(
        "bassGlideCurve",
        "Bass Glide Curve",
        NormalisableRange<float>(0.2f, 2.0f),
        0.6f));

    juce::StringArray pitchBendChoices;
    pitchBendChoices.add(juce::String::fromUTF8(u8"±2 semitones"));
    pitchBendChoices.add(juce::String::fromUTF8(u8"±12 semitones"));

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "bassPitchBendRange",
        "Bass Pitch Bend Range",
        pitchBendChoices,
        1));

    layout.add(std::make_unique<AudioParameterBool>(
        "bassMode",
        "Bass Mono Mode",
        true));

    layout.add(std::make_unique<AudioParameterBool>(
        "globalPlay",
        "Global Play",
        false));

    return layout;
}
//==============================================================================
void Euclidean_seqAudioProcessor::refreshMidiOutputs()
{
    // Update the global device list
    availableMidiOutputs = juce::MidiOutput::getAvailableDevices();

    for (int r = 0; r < 6; ++r)
    {
        auto* portParam = parameters.getRawParameterValue("rhythm" + juce::String(r) + "_midiPort");
        if (portParam != nullptr)
        {
            int currentSelection = (int)portParam->load();
            // If the selection is valid, update the internal pointer
            if (currentSelection >= 0 && currentSelection < availableMidiOutputs.size())
                updateMidiOutputForRhythm(r, currentSelection);
        }
    }
}

void Euclidean_seqAudioProcessor::updateMidiOutputForRhythm(int row, int portIndex)
{
    if (row < 0 || row >= 6)
        return;

    // Closes any previous output
    if (rhythmMidiOuts[row].output != nullptr)
    {
        rhythmMidiOuts[row].output->stopBackgroundThread();
        rhythmMidiOuts[row].output.reset();
    }

    // If the port is valid, open new output
    if (portIndex >= 0 && portIndex < availableMidiOutputs.size())
    {
        rhythmMidiOuts[row].output = juce::MidiOutput::openDevice(
            availableMidiOutputs[portIndex].identifier);
        rhythmMidiOuts[row].selectedPortIndex = portIndex;
    }
    else
    {
        rhythmMidiOuts[row].selectedPortIndex = -1;
    }
}

// Inside the MIDI management logic in the Processor:
void Euclidean_seqAudioProcessor::changeMidiOutput(int row, const juce::String& deviceName)
{
    if (row < 0 || row >= 6) return;

    if (deviceName == "None") {
        rhythmMidiOuts[row].output.reset();
        rhythmMidiOuts[row].selectedPortIndex = -1;
        return;
    }

    // Search for the actual device corresponding to the name
    juce::MidiDeviceInfo targetDevice;
    bool found = false;

    for (auto& d : availableMidiOutputs) {
        if (d.name == deviceName) {
            targetDevice = d;
            found = true;
            break;
        }
    }

    if (!found) return;

    // If the port is already open and has the same identifier, do not do anything
    if (rhythmMidiOuts[row].output != nullptr && rhythmMidiOuts[row].output->getDeviceInfo().identifier == targetDevice.identifier)
        return;

    // Opening using the found real identifier
    rhythmMidiOuts[row].output = juce::MidiOutput::openDevice(targetDevice.identifier);

    if (rhythmMidiOuts[row].output != nullptr)
    {
        rhythmMidiOuts[row].output->startBackgroundThread();
        rhythmMidiOuts[row].selectedPortIndex = availableMidiOutputs.indexOf(targetDevice);
    }
}
//==============================================================================
bool Euclidean_seqAudioProcessor::isRowActive(int row) const
{
    if (row < 0 || row >= 6) return false;
    return parameters.getRawParameterValue("rhythm" + juce::String(row) + "_active")->load() > 0.5f;
}

// ===== RECEIVE ARP SELECTION FROM GUI AND FORWARD TO MIDI GENERATOR =====
void Euclidean_seqAudioProcessor::setArpSelectionFromGUI(
    int row,
    const std::vector<bool>& selection)
{
    if (row < 0 || row >= 6)
        return;

    // Thread-safe update of ARP notes
    {
        std::lock_guard<std::mutex> lock(arpNotesMutex[row]);
        midiGen.setArpSelection(row, selection);
    }
}

//==============================================================================
void Euclidean_seqAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    refreshMidiOutputs();    // Force MIDI device refresh when audio engine starts

    currentSampleRate = sampleRate;
    if (auto* bpmParam = parameters.getRawParameterValue("clockBPM"))
        clock.prepare(sampleRate);

    globalSampleCounter = 0;  // reset global sample counter
    for (int r = 0; r < 6; ++r)
    {
        nextStepSamplePerRhythm[r] = samplesPerStep * r;
        stepCounterPerRhythm[r] = 0;     // reset step counter
        stepMicrotiming[r].clear();      // microtiming inizializzato vuoto
    }

    midiClockCounter = 0;                // reset contatore clock esterno
    externalClockRunning = false;        // reset stato clock esterno
}

void Euclidean_seqAudioProcessor::releaseResources()
{}

void Euclidean_seqAudioProcessor::updateClockSource()
{
    if (auto* p = parameters.getRawParameterValue("clockSource"))
    {
        int sourceValue = (int)p->load();

        if (midiGen.clock != nullptr)
        {
            // Convert the int to the ClockSource type required by the function
            midiGen.clock->setClockSource(static_cast<ClockSource>(sourceValue));

            // Reset counters for new sync
            midiGen.prepareForPlay(getSampleRate());
        }
    }
}

//==============================================================================
void Euclidean_seqAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    auto* clockSourceParam = parameters.getRawParameterValue("clockSource");
    int source = (clockSourceParam != nullptr) ? (int)clockSourceParam->load() : 0;
    bool isStandalone = juce::JUCEApplication::isStandaloneApp();

    // If it is in Standalone and the user has selected DAW (index 2), force Internal (index 0) otherwise leave the choice to the user.
    if (isStandalone && source == 2) source = 0;
    const int numSamples = buffer.getNumSamples();

    // Update clockSource if changed
    if (auto* clockSourceParam = parameters.getRawParameterValue("clockSource"))
    {
        ClockSource newSource = static_cast<ClockSource>((int)clockSourceParam->load());
        if (newSource != clockSource)
        {
            clockSource = newSource;
            clock.setClockSource(clockSource);
        }
    }

    // Reading PlayHead for BPM and play state
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            if (auto bpm = pos->getBpm())
                internalBpm = *bpm;

            isPlaying = pos->getIsPlaying();
        }
    }

    // 1. Reading Play Status (GUI + DAW)
    bool globalPlayActive = false;
    if (auto* p = parameters.getRawParameterValue("globalPlay"))
        globalPlayActive = p->load() > 0.5f;

    // The sequencer should play if is in DAW and the DAW is in Play or the GUI Play button is active
    bool shouldBePlaying = isPlaying || globalPlayActive;
    static bool lastPlayState = false;
    if (shouldBePlaying && !lastPlayState)
    {
        // Pressed Play: Reset everything!
        midiGen.prepareForPlay(getSampleRate());
        clock.reset(); // Make sure the clock also restarts from zero
    }
    lastPlayState = shouldBePlaying;

    // 2. Stop Logic: If the state is changed from Play to Stop
    if (!shouldBePlaying && lastPlayState)
    {
        for (int r = 0; r < 6; ++r)
        {
            if (auto* out = rhythmMidiOuts[r].output.get())
            {
                // Turn off only the used channels or send a general All Notes Off
                for (int ch = 1; ch <= 16; ++ch)
                    out->sendMessageNow(juce::MidiMessage::allNotesOff(ch));
            }
        }
        stagedMidiEvents.clear(); 
    }

    lastPlayState = shouldBePlaying; // Aggiorna lo stato per il prossimo ciclo

    if (!shouldBePlaying)
        return;
    // 3. Clock processing (proceeds only if shouldBePlaying is true)
    clock.processMidi(midiMessages);
    clock.processBlock(numSamples);
    const double samplesPerStep = clock.getSamplesPerStep();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        // Sample-accurate arpeggiator update
        for (int r = 0; r < 6; ++r)
        {
            double arpRate = midiGen.getArpRate(r);
            double samplesPerArpStep = clock.getSamplesPerArpStep(arpRate);
            midiGen.rhythmArps[r].advance(1.0, samplesPerArpStep);
        }

        for (int r = 0; r < 6; ++r)
        {
            bool stepTriggered = false;

            // ===== CLOCK LOGIC =====
            if (clockSource == ClockSource::External && externalClockRunning)
            {
                constexpr double ticksPerBeat = 24.0;
                const double samplesPerTick = samplesPerBeat / ticksPerBeat;

                if (midiClockCounter > 0)
                {
                    extClockSampleAccum[r] += midiClockCounter * samplesPerTick;
                    midiClockCounter = 0;
                }

                if (extClockSampleAccum[r] >= samplesPerStep)
                {
                    extClockSampleAccum[r] -= samplesPerStep;
                    stepTriggered = true;
                }
            }
            else
            {
                if (globalSampleCounter >= nextStepSamplePerRhythm[r])
                    stepTriggered = true;
            }

            if (!stepTriggered)
                continue;

            // Silent atomic reset only if the parameter is actually high
            if (auto* pReset = parameters.getRawParameterValue("rhythm" + juce::String(r) + "_reset"))
            {
                if (pReset->load() > 0.5f)
                {
                    pReset->store(0.0f);
                    // Call the pattern reset logic here if needed
                }
            }
            // MIDI Channel reading
            int midiChannel = 1;
            if (auto* chParam = parameters.getRawParameterValue("rhythm" + juce::String(r) + "_midiChannel"))
                midiChannel = (int)chParam->load() + 1;

            // Generation of Euclidean notes
            std::vector<int> generatedNotes;
            int stepIndex = (int)stepCounterPerRhythm[r];
            int rootNote = noteSources[r].value;

            // Reading octaves
            if (noteSources[r].type == NoteSourceType::Scale)
                noteSources[r].octave = parameters.getRawParameterValue("rhythm" + juce::String(r) + "_scaleOctave")->load();
            else if (noteSources[r].type == NoteSourceType::Chord)
                noteSources[r].octave = parameters.getRawParameterValue("rhythm" + juce::String(r) + "_chordOctave")->load();

            switch (noteSources[r].type)
            {
            case NoteSourceType::Single:
                generatedNotes.push_back(noteSources[r].value);
                break;
            case NoteSourceType::Scale:
            {
                auto intervals = midiGen.getScaleNotes(noteSources[r].value);
                // Alignment: Octave 3 (GUI default) + 1 * 12 = MIDI 48 (C2)
                // This makes the audible octave consistent with the standard JUCE display
                int base = (int)(noteSources[r].octave + 1) * 12;
                for (int i : intervals) generatedNotes.push_back(base + i);
                break;
            }
            case NoteSourceType::Chord:
            {
                auto intervals = midiGen.getChordNotes(noteSources[r].value);
                int base = (int)(noteSources[r].octave + 1) * 12;
                for (int i : intervals) generatedNotes.push_back(base + i);
                break;
            }
            }

            // Update ARP input
            midiGen.setArpInputNotes(r, generatedNotes);

            // Euclidean Step
            bool euclidHit = midiGen.rhythmPatterns[r].nextStep();
            if (!euclidHit) continue;

            std::vector<int> finalNotes;

            // ARP
            bool arpActive = parameters.getRawParameterValue("rhythm" + juce::String(r) + "_arpActive")->load() > 0.5f;
            if (!arpActive)
            {
                finalNotes = generatedNotes;
            }
            else
            {
                // Gets the selected notes for the arpeggio from the mask or popup
                std::vector<int> arpNotes = midiGen.getArpNotes(r);

                if (!arpNotes.empty())
                {
                    // Retrieves the current index calculated by the Arp object
                    int arpIndex = midiGen.rhythmArps[r].getNoteIndex();

                    // Select the corresponding note
                    int finalNote = arpNotes[arpIndex % arpNotes.size()];
                    finalNotes.push_back(finalNote);
                }
            }

            // ===== SEND MIDI =====
            int midiPortIndex = rhythmMidiOuts[r].selectedPortIndex;
            float noteLengthParam = parameters.getRawParameterValue("rhythm" + juce::String(r) + "_noteLength")->load();

            for (int note : finalNotes)
            {
                // 1. Send NoteOn immediately (offset of current sample)
                stagedMidiEvents.push_back({
                    r,
                    midiPortIndex,
                    juce::MidiMessage::noteOn(midiChannel, note, (juce::uint8)100),
                    sample
                    });

                // 2. Calculate the exact duration
                double durationSamples = (samplesPerStep * noteLengthParam) - sample;

                // 3. Explicit creation
                PendingNoteOff pno;
                pno.note = note;
                pno.channel = midiChannel;
                pno.row = r;
                pno.remainingSamples = durationSamples;

                pendingNoteOffs.push_back(pno);
            }
            // ===== SWING + MICROTIMING =====
            double swingAmount = 0.0;
            if (auto* swingParam = parameters.getRawParameterValue("rhythm" + juce::String(r) + "_swing"))
                swingAmount = swingParam->load();

            bool isOffBeat = (stepCounterPerRhythm[r] % 2) == 1;
            double swingOffset = isOffBeat ? samplesPerStep * 0.5 * swingAmount : 0.0;

            double microOffset = 0.0;
            if (!stepMicrotiming[r].empty())
            {
                int idx = stepCounterPerRhythm[r] % stepMicrotiming[r].size();
                microOffset = stepMicrotiming[r][idx] * samplesPerStep;
            }

            nextStepSamplePerRhythm[r] += int(samplesPerStep + swingOffset + microOffset);
            stepCounterPerRhythm[r]++;
        }

}

    // 1. NOTE OFF HANDLING (Synchronization outside the sample loop)
for (auto it = pendingNoteOffs.begin(); it != pendingNoteOffs.end();)
{
    it->remainingSamples -= numSamples;

    if (it->remainingSamples <= 0)
    {
        // Precise offset calculation for this block
        int offset = juce::jlimit(0, numSamples - 1, (int)(it->remainingSamples + numSamples));
        auto msg = juce::MidiMessage::noteOff(it->channel, it->note);

        midiMessages.addEvent(msg, offset); // Invia alla DAW

#if JUCE_StandaloneFilterWindow
        if (auto* out = rhythmMidiOuts[it->row].output.get())
            out->sendMessageNow(msg);
#endif
        it = pendingNoteOffs.erase(it);
    }
    else
    {
        ++it;
    }
}

// 2. GLOBAL COUNTER INCREASE
globalSampleCounter += numSamples;

// 3. SEND GENERATED EVENTS (NoteOn)
for (const auto& e : stagedMidiEvents)
{
    // A. Send to DAW
    midiMessages.addEvent(e.message, e.sampleOffset);

    // B. Send to physical ports (Standalone)
#if JUCE_StandaloneFilterWindow
    if (e.rhythmIndex >= 0 && e.rhythmIndex < 6)
    {
        if (auto* out = rhythmMidiOuts[e.rhythmIndex].output.get())
            out->sendMessageNow(e.message);
    }
#endif
}

// 4. FINAL CLEANING
#if JUCE_StandaloneFilterWindow
midiMessages.clear(); // Evita doppioni nel Master Output di JUCE Standalone
#endif
stagedMidiEvents.clear();
}

//==============================================================================
juce::AudioProcessorEditor* Euclidean_seqAudioProcessor::createEditor()
{
    return new Euclidean_seqAudioProcessorEditor(*this);
}

//==============================================================================
void Euclidean_seqAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Transforms parameters into XML and saves them
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Euclidean_seqAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    // Load parameters from saved XML
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
    {
        if (xmlState->hasTagName(parameters.state.getType()))
        {
            parameters.replaceState(juce::ValueTree::fromXml(*xmlState));

            // Force MIDI ports to refresh after loading
            refreshMidiOutputs();
        }
    }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Euclidean_seqAudioProcessor();
}
