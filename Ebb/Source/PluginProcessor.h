// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "Params.h"
#include "dsp/PumpEngine.h"
#include "dsp/Crossover.h"
#include "dsp/TripleBuffer.h"
#include "ShapeLibrary.h"

//==============================================================================
/**
    Ebb : sidechain « volume shaper ». Une courbe de volume calée sur le tempo (ou relancée par une note MIDI
    ou par un kick sur l'entrée sidechain) fait pomper le son de la piste. Le niveau ne monte jamais.

    Bus : entrée / sortie principales (mono ou stéréo, même format), entrée « Sidechain » (mono, stéréo ou absente).
    Entrée MIDI : chaque note relance la courbe en mode MIDI (voir ROUTAGE-ABLETON.md).

    Phase 2 : courbe perso dessinée dans l'interface (points + courbures), Tension, Lissage, et multibande
    (ne faire pomper que les graves ou que les aigus, séparation Linkwitz-Riley 24 dB/oct).
*/
class EbbAudioProcessor : public juce::AudioProcessor
{
public:
    EbbAudioProcessor();
    ~EbbAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    using AudioProcessor::processBlockBypassed;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override  { return true; }
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

    /** État complet (paramètres + courbe perso + réglages de l'éditeur). */
    juce::ValueTree createStateTree();
    void applyStateTree (const juce::ValueTree&);

    //==============================================================================
    juce::AudioProcessorValueTreeState apvts;

    /** Réglages actuels (lus dans les paramètres). La table de la courbe perso n'y est pas (rôle du thread audio). */
    Ebb::Settings readSettings() const noexcept;
    int getBand() const noexcept { return (int) std::lround (r.band->load()); }
    float getCrossover() const noexcept { return r.crossover->load(); }

    /** Pour l'interface : gain actuel (0..1), position dans la courbe (-1 : aucune), niveau des graves du sidechain. */
    float getGain() const noexcept           { return engine.uiGain.load (std::memory_order_relaxed); }
    float getPhase() const noexcept          { return engine.uiPhase.load (std::memory_order_relaxed); }
    float getSidechainLevel() const noexcept { return engine.uiLevel.load (std::memory_order_relaxed); }
    uint32_t getTriggerCount() const noexcept { return engine.uiTriggers.load (std::memory_order_relaxed); }
    bool isSidechainConnected() const noexcept { return sidechainLive.load (std::memory_order_relaxed); }
    double getBpm() const noexcept           { return lastBpm.load (std::memory_order_relaxed); }

    //==============================================================================
    // Courbe perso (thread de l'interface uniquement)
    const Ebb::ShapeData& getShape() const noexcept { return shapeUi; }
    /** Remplace la courbe perso (rendue et envoyée au thread audio). 'undoable' : l'ancienne va dans « annuler ».
        'selectCustom' : active « Ma courbe » (la courbe perso remplace la courbe d'usine). */
    void setShape (const Ebb::ShapeData&, bool undoable, bool selectCustom = true);
    /** Pendant un glisser à la souris : une seule entrée « annuler » pour tout le geste. */
    void beginShapeGesture();
    void endShapeGesture();
    bool undoShape();
    bool redoShape();
    bool canUndo() const noexcept { return ! undoStack.empty(); }
    bool canRedo() const noexcept { return ! redoStack.empty(); }
    uint32_t getShapeVersion() const noexcept { return shapeVersion; }
    /** Table de baisse de la courbe perso (copie de l'interface, pour l'affichage). */
    const float* getShapeTable() const noexcept { return uiTable.r.data(); }
    /** Copie une courbe d'usine dans la courbe perso (pour la retoucher). */
    void copyFactoryToShape (int curve, bool undoable);
    /** Grille de l'éditeur (0 : sans, sinon nombre de divisions). */
    int getGrid() const noexcept { return grid; }
    void setGrid (int g) { grid = g; }

    void setParam (const char* id, float realValue);

    /** Banc de tests. */
    Ebb::PumpEngine& getEngine() noexcept { return engine; }

private:
    struct CustomTable
    {
        std::array<float, Ebb::CurveTables::size + 1> r {};
        uint32_t version = 0;
    };

    void publishShape();

    Ebb::PumpEngine engine;
    Ebb::Crossover crossover;
    std::array<float, 1024> gains {};
    std::array<int, 512> midiOffsets {};
    bool wasBypassed = false;
    std::atomic<bool> sidechainLive { false };
    std::atomic<double> lastBpm { 120.0 };

    // multibande : poids (plein / graves / aigus) en fondu de 40 ms quand on change de mode
    std::array<float, 3> bandW { 1.0f, 0.0f, 0.0f }, bandStep {};
    int bandRampLeft = 0, bandRampLen = 1920, lastBand = 0;
    float xoverCur = 150.0f, xoverCoef = 0.01f;
    double sampleRate = 48000.0;

    // courbe perso : copie de l'interface, échange sans verrou vers le thread audio
    Ebb::ShapeData shapeUi;
    CustomTable uiTable;
    TripleBuffer<CustomTable> customX;
    uint32_t shapeVersion = 1;
    std::vector<Ebb::ShapeData> undoStack, redoStack;
    bool gestureOpen = false;
    int grid = 8;

    struct Raw { std::atomic<float> *amount, *curve, *rate, *trigger, *nudge, *threshold,
                                    *custom, *tension, *smooth, *band, *crossover; } r {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EbbAudioProcessor)
};
