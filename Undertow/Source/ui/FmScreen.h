// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "LiquidUI.h"

class UndertowAudioProcessor;

//==============================================================================
/**
    Écran FM (à gauche dans la maquette A).
      - la surface de l'eau est la vraie forme d'onde FM (2 cycles), calculée avec les réglages en cours ;
        elle respire avec l'enveloppe d'ampli
      - à chaque note, une goutte tombe : des ondes concentriques dont le nombre et la force suivent l'Index FM
        (corail sur les accents)
      - glisser verticalement = Index, horizontalement = Ratio ; double-clic = Index par défaut
      - LIVE : la courbe principale est le signal réel qui sort du plugin (oscilloscope),
        vue ONDE (2 cycles calés sur la note) ou MESURE (enveloppe de la dernière mesure) ;
        la forme théorique reste en fantôme derrière
*/
class FmScreen : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit FmScreen (UndertowAudioProcessor&);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

    void setViewMode (int m) { viewMode = juce::jlimit (0, 1, m); repaint(); }

    /** Appelé par le minuteur de l'éditeur. Renvoie true tant que des ondes sont visibles. */
    bool tick();

private:
    struct Drop { float born = -100.0f, x = 0.5f, strength = 0.0f; int rings = 0; bool accent = false; };

    void setParamNormalised (const char* id, float norm);
    float getNorm (const char* id) const;

    UndertowAudioProcessor& processor;
    std::array<Drop, 12> drops {};
    int nextDrop = 0;
    uint32_t lastCounter = 0;
    float envSmoothed = 0.0f;

    bool dragging = false;
    float dragIndex0 = 0.0f, dragRatio0 = 0.0f;

    juce::Rectangle<float> viewPill (int mode) const;

    juce::Path wave, body, live;
    int   viewMode = 0;          // 0 = ONDE (cycles de la note), 1 = MESURE (enveloppe de la dernière mesure)
    float scopeGain = 1.0f;      // mise à l'échelle automatique, lissée
    juce::String noteText;
    int noteKey = -2, levelKey = -2;
    juce::String infoText, dragText;
    int infoKey = -1;
};
