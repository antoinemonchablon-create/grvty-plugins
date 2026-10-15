// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "Params.h"
#include "dsp/FmVoice.h"
#include "dsp/StereoFx.h"
#include "seq/Pattern.h"
#include "seq/Sequencer.h"
#include "PresetManager.h"

#ifndef UNDERTOW_PROFILE
 #define UNDERTOW_PROFILE 0
#endif

//==============================================================================
class UndertowAudioProcessor : public juce::AudioProcessor
{
public:
    UndertowAudioProcessor();
    ~UndertowAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    const FmVoice& getVoice() const noexcept { return voice; }   // lecture seule (tests)
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    /** Motif du séquenceur : l'interface modifie pattern.edit() puis appelle pattern.publish(). */
    PatternExchange pattern;

    /** Presets d'usine (son + motif), presets utilisateur, Random. Thread de l'interface uniquement. */
    PresetManager presets { apvts, pattern };

    /** Données lues par l'interface. */
    std::atomic<float> outputLevel { 0.0f };
    std::atomic<float> displayIndex { 0.0f }, displayModEnv { 0.0f }, displayAmpEnv { 0.0f };
    std::atomic<int>   displayNote { -1 };
    std::atomic<float> displayRatio { 1.0f };
    std::atomic<double> displayPpq { 0.0 };           // position du transport (noires), pour la vague de lecture
    std::atomic<uint32_t> noteCounter { 0 };          // +1 à chaque note jouée : l'écran fait tomber une goutte

    //==============================================================================
    /**
        Oscilloscope : copie de la sortie réelle du plugin (mono), sans verrou.
          - scope    : les 8192 derniers échantillons (forme d'onde, quelques cycles)
          - envMin/Max : minimum et maximum de chaque paquet de 64 échantillons (vue d'une mesure entière)
        Le thread audio écrit, l'interface lit ; les compteurs ne font qu'augmenter.
    */
    static constexpr int scopeSize = 8192, envSize = 4096, envChunk = 64;
    std::array<std::atomic<float>, scopeSize> scope {};
    std::array<std::atomic<float>, envSize> envMin {}, envMax {};
    std::atomic<uint32_t> scopeWrite { 0 }, envWrite { 0 };
    std::atomic<float>  displayFreq { 130.8f };
    std::atomic<double> displaySampleRate { 44100.0 }, displayBpm { 120.0 };
    std::atomic<float> lastNoteVelocity { 0.8f };
    std::atomic<bool>  lastNoteAccent { false };
    std::atomic<bool>  transportPlaying { false };
    int getCurrentStep() const noexcept     { return sequencer.currentStep.load (std::memory_order_relaxed); }
    int getCurrentLaneStep() const noexcept { return sequencer.currentLaneStep.load (std::memory_order_relaxed); }

    /** Pour le banc de tests : journal des notes déclenchées (position absolue en échantillons). */
    struct TriggerLog
    {
        bool enabled = false;
        std::vector<std::pair<int64_t, int64_t>> onsets;   // (échantillon absolu, pas global)
    } triggerLog;

   #if UNDERTOW_PROFILE
    struct Profile { double voice = 0, down = 0, delay = 0, reverb = 0, other = 0; int blocks = 0; } profile;
   #endif

    static constexpr int chunkSize = 64;

private:
    void readSettings();
    void handleMidi (const juce::MidiMessage&, bool keysMode);
    void startNote (int note, float velocity, bool accent, bool slide, float lockIndex, float lockCutoff);
    void computeMods (VoiceMods& m, float& pan) noexcept;
    void advanceLfos (double qps) noexcept;

    FmVoice voice;
    Sequencer sequencer;
    Sequencer::Settings seqSettings;
    PingPongDelay delay;
    SmallReverb reverb;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 2> oversamplers;   // 2x, 4x
    int activeOs = 0, requestedOs = 0;
    float osSwitchGain = 1.0f;
    bool  osSwitching = false;
    int   osHold = 0;

    juce::AudioBuffer<float> zeroBuffer, monoBuffer, scratch;

    double sampleRate = 44100.0;
    int64_t samplesProcessed = 0;

    // --- LFO
    struct LfoState
    {
        double freePhase = 0.0, anchor = 0.0;
        int64_t freeCycle = 0;
        float out = 0.0f;
        std::array<float, Choices::numLfoDests> weight {};
    };
    std::array<LfoState, Params::numLfos> lfos;
    double lfoClock = 0.0;            // position en noires (transport ou horloge interne)
    float  lfoSmooth = 0.1f, weightSmooth = 0.01f;

    float laneValue = 0.0f;
    int   transpose = 0;
    int   lastMode = -1;
    std::array<int, 16> heldNotes {};
    int   numHeld = 0;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> outGainSm;
    float panSmoothed = 0.0f;
    float envAccMin = 0.0f, envAccMax = 0.0f;
    int   envAccCount = 0;

    // --- accès rapide aux paramètres
    std::atomic<float>* raw (const juce::String& id) { return apvts.getRawParameterValue (id); }
    struct Raw
    {
        std::atomic<float> *legato, *glide, *velSens, *shapeA, *coarseA, *fineA, *shapeB, *ratio, *free, *freeRatio, *fineB, *mix,
                           *index, *feedback, *fmMode, *modA, *modD, *modC, *modIdx, *modCut, *modPitch,
                           *fType, *fSlope, *cutoff, *res, *fEnv, *fDrive, *keyTrack, *att, *dec, *sus, *rel,
                           *fxDrive, *dTime, *dFb, *dTone, *dMix, *rSize, *rDamp, *rMix, *os, *outGain,
                           *seqMode, *seqLength, *seqRate, *swing, *scale, *root, *octave, *accent,
                           *laneDest, *laneLength, *laneDepth, *mutAmt, *mutBars;
        std::array<std::atomic<float>*, Params::numLfos> lShape {}, lSync {}, lRate {}, lSyncRate {}, lDest {}, lDepth {}, lRetrig {};
    } r {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndertowAudioProcessor)
};
