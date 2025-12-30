#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
Euclidean_seqAudioProcessor::Euclidean_seqAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    parameters(*this, nullptr, "Params", createParameterLayout())
{
    // ===== ENUMERATE MIDI OUTPUT PORTS =====
    refreshMidiOutputs();

    // ===== INIT RHYTHM MIDI STATE =====
    for (auto& r : rhythmMidiOuts)
    {
        r.selectedPortIndex = -1;
        r.selectedChannel = 1;
    }

    // Initialize default rhythm patterns and arp notes
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
            0b0001111)); // default: prime 4 note attive

        // ===== MIDI PORT (dinamico, allineato alla ComboBox) =====
        juce::StringArray midiPortNames;
        auto midiDevices = juce::MidiOutput::getAvailableDevices();

        for (const auto& dev : midiDevices)
            midiPortNames.add(dev.name);

        if (midiPortNames.isEmpty())
            midiPortNames.add("No MIDI Outputs");

        // ===== MIDI OUTPUT PORT (per rhythm) =====
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
    clockSourceChoices.add("DAW");
    clockSourceChoices.add("Internal");
    clockSourceChoices.add("External");

    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "clockSource",
        "Clock Source",
        clockSourceChoices,
        0));

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
    // 1) Legge lista attuale dispositivi MIDI
    auto currentDevices = juce::MidiOutput::getAvailableDevices();

    // 2) Confronta con lista precedente per rilevare cambi
    bool changed = (currentDevices != availableMidiOutputs);

    if (!changed)
        return; // nessuna modifica

    // 3) Aggiorna lista globale
    availableMidiOutputs = currentDevices;

    // 4) Chiudi porte non più valide
    for (auto& r : rhythmMidiOuts)
    {
        if (r.selectedPortIndex >= availableMidiOutputs.size())
        {
            r.output.reset();
            r.selectedPortIndex = -1; // reset sicuro
        }
    }
    // NOTA: aggiornamento ComboBox avverrà in Editor tramite timer o callback
}

void Euclidean_seqAudioProcessor::updateMidiOutputForRhythm(int rhythmIndex, int deviceIndex)
{
    if (rhythmIndex < 0 || rhythmIndex >= 6)
        return;

    auto& r = rhythmMidiOuts[rhythmIndex];

    if (deviceIndex < 0 || deviceIndex >= availableMidiOutputs.size())
    {
        r.output.reset();
        r.selectedPortIndex = -1;
        return;
    }

    if (r.selectedPortIndex == deviceIndex && r.output)
        return;

    r.output.reset();
    r.output = juce::MidiOutput::openDevice(
        availableMidiOutputs[deviceIndex].identifier);

    r.selectedPortIndex = deviceIndex;
}
//==============================================================================
bool Euclidean_seqAudioProcessor::isRowActive(int row) const
{
    if (row < 0 || row >= 6) return false;
    return parameters.getRawParameterValue("rhythm" + juce::String(row) + "_active")->load() > 0.5f;
}

// Aggiorna le note di ingresso per l'ARP di una riga specifica
void Euclidean_seqAudioProcessor::updateRowInputNotes(int row)
{
    std::lock_guard<std::mutex> lock(arpMutex);

    if (row < 0 || row >= 6) return;

    arpInputNotes[row].clear();

    auto noteValue = parameters.getRawParameterValue("rhythm" + juce::String(row) + "_note")->load();

    // Se singola nota
    if (noteValue < 1000.0f)
    {
        arpInputNotes[row].push_back(static_cast<int>(noteValue));
    }
    // TODO: gestire scale e chord qui, trasformando ID in note reali e popolando arpInputNotes[row]

    // Aggiorna arpNoteSlots solo se ARP attivo
    if (parameters.getRawParameterValue("rhythm" + juce::String(row) + "_arpActive")->load() > 0.5f)
    {
        midiGen.setArpNotes(row, arpInputNotes[row]);  // usa direttamente arpNotes
    }
}

//==============================================================================
void Euclidean_seqAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    globalSampleCounter = 0;  // reset contatore globale dei sample
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
{
}

//==============================================================================
void Euclidean_seqAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    const int numSamples = buffer.getNumSamples();

    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int r = 0; r < 6; ++r)
        {
            // ===== SAMPLES PER ARP STEP =====
            double samplesPerArpStep = samplesPerStep; // default = 1 step
            if (parameters.getRawParameterValue("rhythm" + juce::String(r) + "_arpActive")->load() > 0.5f)
            {
                auto* arpRateParam = parameters.getRawParameterValue("rhythm" + juce::String(r) + "_arpRate");
                static const double arpRateTable[] = { 1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125, 0.1666667, 0.375 };
                int rateIndex = juce::jlimit(0, 7, (int)arpRateParam->load());
                double arpRate = arpRateTable[rateIndex];
                samplesPerArpStep = samplesPerStep * arpRate;
            }

            // ===== MUTE =====
            if (isMuted)
            {
                midiGen.advanceArp(r, samplesPerStep, samplesPerArpStep);
                continue;
            }

            // ===== RESET ONE-SHOT =====
            auto* resetParam = parameters.getParameter("rhythm" + juce::String(r) + "_reset");
            if (resetParam != nullptr && resetParam->getValue() > 0.5f)
            {
                midiGen.rhythmPatterns[r].reset();
                midiGen.rhythmArps[r].reset();
                stepCounterPerRhythm[r] = 0;
                nextStepSamplePerRhythm[r] = globalSampleCounter;
                stepMicrotiming[r].clear();

                if (r == 5)
                {
                    bassNoteActive = false;
                    bassGlideActive = false;
                    lastBassNote = -1;
                }

                resetParam->setValueNotifyingHost(0.0f);
            }

            // ===== READ MIDI OUTPUT PORT =====
            int midiPort = (int)parameters.getRawParameterValue(
                "rhythm" + juce::String(r) + "_midiPort")->load();
            updateMidiOutputForRhythm(r, midiPort);

            bool stepTriggered = false;
            bool arpStepAdvanced = false;

            // ===== ARP SAMPLE-ACCURATE ADVANCE =====
            if (parameters.getRawParameterValue("rhythm" + juce::String(r) + "_arpActive")->load() > 0.5f)
            {
                auto* arpRateParam = parameters.getRawParameterValue("rhythm" + juce::String(r) + "_arpRate");
                static const double arpRateTable[] = { 1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125, 0.1666667, 0.375 };
                int rateIndex = juce::jlimit(0, 7, (int)arpRateParam->load());
                double arpRate = arpRateTable[rateIndex];

                samplesPerArpStep = samplesPerStep * arpRate;
                arpStepAdvanced = midiGen.rhythmArps[r].advanceSamples(1.0, samplesPerArpStep);
            }

            // ===== EXTERNAL MIDI CLOCK =====
            if (clockSource == ClockSource::External && externalClockRunning)
            {
                constexpr double ticksPerBeat = 24.0;
                const double samplesPerTick = samplesPerBeat / ticksPerBeat;
                static double extClockSampleAccum[6] = { 0.0 };

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

            // ===== READ MIDI CHANNEL =====
            int midiChannel = 1;
            if (auto* chParam = parameters.getRawParameterValue("rhythm" + juce::String(r) + "_midiChannel"))
                midiChannel = (int)chParam->load() + 1;

            int baseNote = (int)parameters.getRawParameterValue("rhythm" + juce::String(r) + "_note")->load();

            // ===== GENERATE NOTES =====
            std::vector<int> generatedNotes;
            switch (noteSources[r].type)
            {
            case NoteSourceType::Single:
                generatedNotes.push_back(noteSources[r].value);
                break;
            case NoteSourceType::Scale:
                generatedNotes = midiGen.getScaleNotes(noteSources[r].value);
                break;
            case NoteSourceType::Chord:
                generatedNotes = midiGen.getChordNotes(noteSources[r].value);
                break;
            }

            // ===== UPDATE ARP NOTES (MAX 7, SOLO NOTE IN INGRESSO) =====
            if (parameters.getRawParameterValue("rhythm" + juce::String(r) + "_arpActive")->load() > 0.5f)
            {
                // Limita a massimo 7 note
                std::vector<int> arpCandidates = generatedNotes;
                if (arpCandidates.size() > 7)
                    arpCandidates.resize(7);

                // Salva solo le note ricevute in ingresso, non fare selezione automatica
                midiGen.setArpNotes(r, arpCandidates);
            }

            if (!generatedNotes.empty())
                baseNote = generatedNotes[0];

            bool euclidHit = midiGen.rhythmPatterns[r].nextStep();

            if (!euclidHit)
                continue;

            // ===== CALCOLO NOTE FINALI =====
            std::vector<int> finalNotes;

            bool arpActive = parameters.getRawParameterValue("rhythm" + juce::String(r) + "_arpActive")->load() > 0.5f;

            if (!arpActive)
            {
                // ARP OFF → tutte le note in ingresso passano senza modifica
                finalNotes = generatedNotes;
            }
            else
            {
                // ARP ON → una nota arpeggiata per step
                if (arpStepAdvanced)
                {
                    int arpIndex = midiGen.rhythmArps[r].getNoteIndex();
                    auto& arpNotes = midiGen.getArpNotes(r);

                    if (!arpNotes.empty())
                    {
                        int idx = arpIndex % arpNotes.size();
                        finalNotes.push_back(arpNotes[idx]); // nota arpeggiata
                    }
                }

                // Pass-through per note non selezionate dall'ARP (mix con note arpeggiate)
                for (int n : generatedNotes)
                {
                    auto& arpNotes = midiGen.getArpNotes(r);
                    if (std::find(arpNotes.begin(), arpNotes.end(), n) == arpNotes.end())
                        finalNotes.push_back(n);
                }
            }
            // ===== MIDI EVENTS =====
            for (int outMidiNote : finalNotes)
            {
                int midiPortIndex = midiPort;
                stagedMidiEvents.push_back({ r, midiPortIndex,
                    juce::MidiMessage::noteOn(midiChannel, outMidiNote, (juce::uint8)100), sample });
                stagedMidiEvents.push_back({ r, midiPortIndex,
                    juce::MidiMessage::noteOff(midiChannel, outMidiNote), sample + int(samplesPerStep * 0.5) });
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

        // ===== APPLY PENDING NOTE-OFFS =====
        for (auto it = pendingNoteOffs.begin(); it != pendingNoteOffs.end(); )
        {
            it->remainingSamples--;
            if (it->remainingSamples <= 0)
            {
                midiMessages.addEvent(juce::MidiMessage::noteOff(it->channel, it->note), sample);
                it = pendingNoteOffs.erase(it);
            }
            else ++it;
        }

        globalSampleCounter++;
    }

    // ===== FLUSH MIDI EVENTS =====
    for (const auto& e : stagedMidiEvents)
        midiMessages.addEvent(e.message, e.sampleOffset);

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
    auto state = parameters.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Euclidean_seqAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState != nullptr)
        parameters.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Euclidean_seqAudioProcessor();
}
