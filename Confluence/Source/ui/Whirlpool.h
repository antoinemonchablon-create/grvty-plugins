// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "LiquidUI.h"
#include "ChordText.h"

class ConfluenceAudioProcessor;

//==============================================================================
/**
    Les enchaînements sur un cercle des quintes en tourbillon :
      - les notes de la gamme sont éclairées sur le cercle ;
      - la fondamentale de l'accord joué est en corail ;
      - les 3 degrés qui s'enchaînent le mieux ensuite brillent, reliés par un courant, avec leur chiffrage (iv, VI...).
    Les bras du tourbillon tournent lentement ; le reste est dessiné une fois dans une image.
*/
class Whirlpool : public juce::Component
{
public:
    explicit Whirlpool (ConfluenceAudioProcessor& p) : processor (p) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    void resized() override { dirty = true; }

    /** Renvoie true si l'image fixe a changé (nouvel accord, autre gamme). */
    bool tick();

private:
    void render (float scale);

    ConfluenceAudioProcessor& processor;
    juce::Image nodes;
    float imgScale = 0.0f;
    bool dirty = true;
    int root = -1, scale = -1, style = -1, current = -2, mode = -1, type = -1;
    std::array<int, 3> next { -1, -1, -1 };
    uint32_t lastCounter = 0;
};
