// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "LiquidUI.h"
#include "../seq/Pattern.h"

class UndertowAudioProcessor;

//==============================================================================
/**
    Grille du séquenceur : chaque pas est une colonne d'eau qui se remplit selon la hauteur de la note.
    La tête de lecture est une vague qui traverse la grille et soulève l'eau des colonnes qu'elle touche.

      clic                 allumer / éteindre le pas
      glisser haut / bas   hauteur de la note (3 octaves, la note jouée s'affiche)
      Maj + clic           accent           Alt + clic   slide
      clic droit           ratchet, probabilité, gate, vélocité, p-locks
      rangée du bas        piste Mod : glisser pour dessiner
*/
class SeqGrid : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit SeqGrid (UndertowAudioProcessor&);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    /** Appelé par le minuteur de l'éditeur. */
    void tick();

private:
    static constexpr int pitchLow = -12, pitchRange = 36;

    int visibleSteps() const;
    juce::Rectangle<float> notesArea() const;
    juce::Rectangle<float> laneArea() const;
    int stepAt (float x) const;
    void setPitchFromY (int step, float y);
    void setLaneFromY (int step, float y);
    void showStepMenu (int step);
    void publish();
    juce::String playedNoteName (const Step&) const;

    UndertowAudioProcessor& processor;
    enum class Drag { none, pitch, lane } drag = Drag::none;
    int dragStep = -1, hoverStep = -1;
    bool dragMoved = false;
    float playPos = -1.0f;   // position de lecture en pas (fractionnaire), -1 = arrêt
    juce::String hintText;
    juce::Path crest;
};
