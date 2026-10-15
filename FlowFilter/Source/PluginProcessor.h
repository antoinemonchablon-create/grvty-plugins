// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Flow Filter, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "WaveEngine.h"
#include "FilterBank.h"
#include "ModLfo.h"
#include "PresetManager.h"

//==============================================================================
namespace ParamIDs
{
    // Mouvement
    inline constexpr const char* rate      = "rate";
    inline constexpr const char* sync      = "sync";
    inline constexpr const char* syncDiv   = "syncDiv";
    inline constexpr const char* reverse   = "reverse";
    inline constexpr const char* freeze    = "freeze";
    // Vague
    inline constexpr const char* multiply  = "multiply";
    inline constexpr const char* ripple    = "ripple";
    inline constexpr const char* scan      = "scan";
    inline constexpr const char* offset    = "offset";
    inline constexpr const char* invert    = "invert";
    inline constexpr const char* gravity   = "gravity";
    // Filtres
    inline constexpr const char* depth     = "depth";
    inline constexpr const char* resonance = "resonance";
    inline constexpr const char* analog    = "analog";
    inline constexpr const char* areaLow   = "areaLow";
    inline constexpr const char* areaHigh  = "areaHigh";
    inline constexpr const char* areaGain  = "areaGain";
    inline constexpr const char* areaSlope = "areaSlope";
    inline constexpr const char* release   = "release";
    inline constexpr const char* width     = "width";
    // Sortie
    inline constexpr const char* inGain    = "inGain";
    inline constexpr const char* outGain   = "outGain";
    inline constexpr const char* mix       = "mix";

    // Modulateurs : 0 -> Offset, 1 -> Invert, 2 -> Gravity
    inline juce::String modShape (int i) { return "mod" + juce::String (i + 1) + "Shape"; }
    inline juce::String modRate  (int i) { return "mod" + juce::String (i + 1) + "Rate"; }
    inline juce::String modDepth (int i) { return "mod" + juce::String (i + 1) + "Depth"; }
}

//==============================================================================
class FlowFilterAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numMods = 3;

    FlowFilterAudioProcessor();
    ~FlowFilterAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    juce::AudioProcessorValueTreeState apvts;
    PresetManager presets { apvts };   // presets d'usine, presets utilisateur, Random

    // --- Données lues par l'interface (thread graphique)
    std::array<std::atomic<float>, FilterBank::maxBands> displayBands;   // gain linéaire de chaque bande
    std::atomic<double> displayPhase { 0.0 };                             // position de la vague (après modulation)
    std::array<std::atomic<float>, 6> displayWave;                        // multiply, ripple, scan, offset, invert, gravity

    /** Paramètres effectifs de la vague (modulations comprises), pour que l'interface
        redessine exactement la même courbe que celle entendue. */
    WaveEngine::Params getDisplayWaveParams() const
    {
        WaveEngine::Params wp;
        wp.multiply = displayWave[0].load (std::memory_order_relaxed);
        wp.ripple   = displayWave[1].load (std::memory_order_relaxed);
        wp.scan     = displayWave[2].load (std::memory_order_relaxed);
        wp.offset   = displayWave[3].load (std::memory_order_relaxed);
        wp.invert   = displayWave[4].load (std::memory_order_relaxed);
        wp.gravity  = displayWave[5].load (std::memory_order_relaxed);
        return wp;
    }
    std::array<std::atomic<float>, numMods> displayMods;                 // valeur courante des modulateurs
    std::atomic<int>   displayNumBands { FilterBank::maxBands };
    std::atomic<float> inputLevel { 0.0f }, outputLevel { 0.0f };

    /** Fréquence de la bande b pour un nombre de bandes donné (même formule que FilterBank). */
    static double bandFrequency (int b, int numBands);

private:
    static double syncDivisionInQuarters (int index);
    void updateBandTargets (const WaveEngine::Params& wp, float depthDb);

    WaveEngine wave;
    FilterBank filterBank;
    std::array<ModLfo, numMods> mods;

    double sampleRate = 44100.0;
    double phase      = 0.0;   // position de la vague, en cycles

    static constexpr int controlBlockSize = 32;

    // Accès rapide aux paramètres sur le thread audio
    std::atomic<float>* p (const juce::String& id) { return apvts.getRawParameterValue (id); }

    std::atomic<float> *pRate {}, *pSync {}, *pSyncDiv {}, *pReverse {}, *pFreeze {},
                       *pMultiply {}, *pRipple {}, *pScan {}, *pOffset {}, *pInvert {}, *pGravity {},
                       *pDepth {}, *pResonance {}, *pAnalog {},
                       *pAreaLow {}, *pAreaHigh {}, *pAreaGain {}, *pAreaSlope {}, *pRelease {}, *pWidth {},
                       *pInGain {}, *pOutGain {}, *pMix {};
    std::array<std::atomic<float>*, numMods> pModShape {}, pModRate {}, pModDepth {};

    juce::SmoothedValue<float> inGainSm, outGainSm, mixSm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlowFilterAudioProcessor)
};
