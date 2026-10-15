// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "LiquidUI.h"
#include "../dsp/Shape.h"

class EbbAudioProcessor;

//==============================================================================
/**
    Les 12 courbes d'usine en vignettes, plus « MA COURBE » (la courbe perso), en une rangée.
    Clic : choisit la courbe (en édition : la courbe d'usine est copiée dans la courbe perso, pour la retoucher).
*/
class CurveStrip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit CurveStrip (EbbAudioProcessor&);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** Appelé à 60 Hz : redessine si la sélection ou la courbe perso ont changé. */
    bool tick();

    static constexpr int numCells = Ebb::numCurves + 1;   // la dernière : MA COURBE
    std::function<void (int)> onPick;
    juce::Rectangle<float> cellBounds (int i) const;

    /** Vignette d'une table de baisse (niveau = 1 - r). */
    static void drawThumb (juce::Graphics&, juce::Rectangle<float>, const float* table, bool selected, bool hot, bool custom);

private:
    int indexAt (juce::Point<float>) const;

    EbbAudioProcessor& processor;
    int selected = -1, hovered = -1;
    uint32_t shapeVersion = 0;
};

//==============================================================================
/**
    Panneau « Enregistrer ma courbe » dessiné DANS la fenêtre du plugin (comme dans Confluence : dans Ableton sur Mac,
    une fenêtre de dialogue s'ouvrirait derrière le plugin). Entrée = enregistrer, Échap ou clic à côté = annuler.
*/
class ShapeSavePanel : public juce::Component
{
public:
    ShapeSavePanel (const Ebb::ShapeData&, const juce::String& suggestedName, std::function<void (bool saved, juce::String name)> onDone);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void parentHierarchyChanged() override;

    void commit();
    void cancel();
    juce::TextEditor& getEditor() { return editor; }

private:
    void updateHint();

    Ebb::ShapeData shape;
    std::function<void (bool, juce::String)> done;
    juce::TextEditor editor;
    juce::Rectangle<int> panelR, saveR, cancelR;
    bool exists = false, finished = false;
    int hot = 0;
};
