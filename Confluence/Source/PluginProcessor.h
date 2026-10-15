// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "Params.h"
#include "engine/MidiEngine.h"
#include "engine/PreviewSynth.h"
#include "engine/TripleBuffer.h"
#include "theory/Harmony.h"
#include "MidiExport.h"
#include "PresetManager.h"

//==============================================================================
/**
    Confluence : une note jouée -> un accord, envoyé en MIDI à un autre instrument.

    Dans Ableton, un plugin VST d'effet MIDI ne peut pas être placé avant un instrument :
    Confluence est donc un « instrument » qui reçoit du MIDI et renvoie du MIDI (sortie audio silencieuse,
    sauf la voix Preview). On le route vers la piste du synthé avec « MIDI From » (voir ROUTAGE-ABLETON.md).
*/
class ConfluenceAudioProcessor : public juce::AudioProcessor
{
public:
    ConfluenceAudioProcessor();
    ~ConfluenceAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    /** État complet (paramètres + pads + accord appris + nom du preset), pour les projets et les presets. */
    juce::ValueTree createStateTree();
    void applyStateTree (const juce::ValueTree&, bool keepRoutingParams);

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    // --- Thread de l'interface ---------------------------------------------------
    const Engine::PadSet& getPads() const noexcept { return padsUi; }
    void setPads (const Engine::PadSet&);
    void setPad (int index, const Engine::Pad&);
    const Engine::LearnedShape& getLearned() const noexcept { return learnedUi; }
    void setLearned (const Engine::LearnedShape&);

    /** Learn : le prochain accord joué (touches enfoncées ensemble puis relâchées) devient la forme « Appris ». */
    void startLearn()  { learnRequest.store (1); }
    void cancelLearn() { learnRequest.store (2); }
    bool isLearning() const noexcept { return learning.load(); }

    /** Analyse : classe les gammes compatibles avec ce qui est joué pendant l'analyse. */
    void startAnalysis() { analysisReset.store (true); analysing.store (true); }
    void stopAnalysis()  { analysing.store (false); }
    bool isAnalysing() const noexcept { return analysing.load(); }
    std::vector<Harmony::Candidate> getAnalysisRanking (int count = 3) const;
    int getAnalysisNoteCount() const noexcept { return analysisCount.load(); }

    void requestPanic()        { panicRequest.store (true); }
    void notifyPresetChange()  { presetPanic.store (true); }

    /** Remplit les 8 pads avec une progression (style et gamme actuels). */
    void generatePads (uint32_t seed);

    /** Export de la progression des pads en fichier .mid (4 ou 8 mesures). */
    bool exportMidi (const juce::File& file, int bars);
    MidiExport::Options exportOptions (int bars) const;
    static juce::File getExportFolder();

    /** Réglages actuels (lus dans les paramètres). */
    Engine::ChordSettings readChordSettings() const;

    /** À appeler régulièrement depuis le thread de l'interface : applique l'accord appris. */
    void pollMessageThread();

    /** Dernier accord joué, pour l'affichage. */
    struct ChordDisplay
    {
        int notes[Voicing::maxNotes + 1] {};
        int count = 0;
        int bass = -1;
        int rootPc = 0;          // fondamentale sonore (après transposition)
        int chordRootPc = 0;     // fondamentale dans la tonalité (avant transposition)
        int padIndex = -1;
        int inputNote = -1;
        bool borrowed = false;
        uint32_t counter = 0;
    };
    ChordDisplay getLastChord() { return displayX.read(); }
    std::atomic<int> activePad { -1 };      // pad tenu en ce moment (-1 : aucun)

    /** Clavier et pads de l'interface : la note passe par Confluence comme une note du clavier MIDI.
        'pad' : joue le pad (note = n° du pad 0..7) quel que soit le mode. velocity 0 = relâchement. */
    void playFromUi (int note, int velocity, bool pad);

    /** Notes qui sonnent en sortie / touches enfoncées en entrée (pour l'affichage), mises à jour à chaque bloc. */
    bool isSounding (int note) const noexcept { return note >= 0 && note < 128 && ((soundingBits[(size_t) (note >> 5)].load (std::memory_order_relaxed) >> (note & 31)) & 1u); }
    bool isInputHeld (int note) const noexcept { return note >= 0 && note < 128 && ((inputBits[(size_t) (note >> 5)].load (std::memory_order_relaxed) >> (note & 31)) & 1u); }
    bool anySounding() const noexcept
    {
        for (auto& b : soundingBits) if (b.load (std::memory_order_relaxed) != 0) return true;
        return false;
    }

    /** Presets d'usine et utilisateur, dé (thread de l'interface). */
    PresetManager presets { *this };

    /** Banc de tests. */
    Engine::MidiEngine& getEngine() noexcept { return engine; }
    double getBpm() const noexcept { return lastBpm.load(); }

private:
    void readPlaySettings (Engine::PlaySettings&, double bpm) const;
    void handleNoteOn (int offset, int ch, int note, int vel, const Engine::ChordSettings&, const Engine::PlaySettings&);
    void handleNoteOff (int offset, int ch, int note, const Engine::ChordSettings&, const Engine::PlaySettings&);
    void collectAnalysis (bool on, int ch, int note, int offset);
    void finishBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&, int numSamples);

    Engine::MidiEngine engine;
    PreviewSynth preview;
    juce::MidiBuffer out;

    // pads et accord appris : copie de l'interface + échange sans verrou vers l'audio
    Engine::PadSet padsUi;
    Engine::LearnedShape learnedUi;
    TripleBuffer<Engine::PadSet> padsX;
    TripleBuffer<Engine::LearnedShape> learnedX;
    TripleBuffer<ChordDisplay> displayX;
    uint32_t displayCounter = 0;

    // Learn (le thread audio écrit le résultat, l'interface l'applique)
    std::atomic<int> learnRequest { 0 };
    std::atomic<bool> learning { false };
    std::array<bool, 128> learnHeld {}, learnSet {};
    std::array<std::atomic<int>, Theory::maxChordTones> learnResult {};
    std::atomic<int> learnResultCount { 0 };
    std::atomic<uint32_t> learnResultSeq { 0 };
    uint32_t learnSeenSeq = 0;

    // Analyse de la gamme
    std::atomic<bool> analysing { false }, analysisReset { false };
    std::array<std::atomic<float>, 12> analysisWeight {}, analysisBass {};
    std::atomic<int> analysisCount { 0 };
    std::array<float, 12> aWeight {}, aBass {};
    std::array<std::array<int64_t, 128>, 16> onsetSample {};
    std::array<int, 128> inputHeld {};

    // Coupures de toutes les notes
    std::atomic<bool> panicRequest { false }, presetPanic { false }, deactivatePanic { false };
    bool wasPlaying = false, wasBypassed = false, lastLatch = false, previewWasOn = false;

    // notes jouées depuis l'interface (file sans verrou interface -> audio)
    struct UiNote { int16_t note; int16_t velocity; bool pad; };
    std::array<UiNote, 256> uiNotes {};
    juce::AbstractFifo uiFifo { 256 };
    void drainUiNotes (const Engine::ChordSettings&, const Engine::PlaySettings&, bool bypassed);

    std::array<std::atomic<uint32_t>, 4> soundingBits {}, inputBits {};

    double sampleRate = 48000.0;
    int64_t sampleCounter = 0;
    std::atomic<double> lastBpm { 120.0 };

    // accès rapide aux paramètres
    std::atomic<float>* raw (const char* id) const { return apvts.getRawParameterValue (id); }
    struct Raw
    {
        std::atomic<float> *mode, *root, *scale, *outOfScale, *chordType, *blackKeys, *fixedShape, *style, *padBase,
                           *inversion, *spread, *maxVoices, *lowNote, *highNote, *bass, *octave, *transpose,
                           *strum, *strumDir, *strumSync, *strumSyncRate, *humanTime, *humanVel, *velMode, *velFixed, *velCurve,
                           *balBass, *balMid, *balTop, *durMode, *duration, *latch, *outChannel, *preview, *previewLevel;
    } r {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfluenceAudioProcessor)
};
