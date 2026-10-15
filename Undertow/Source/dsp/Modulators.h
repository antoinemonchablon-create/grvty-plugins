// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "FastMath.h"

//==============================================================================
/**
    LFO : sinus, triangle, rampe, carré, S&H. Sortie bipolaire -1..+1.

    La phase est fournie de l'extérieur (en cycles, avec un compteur de cycles) :
    en mode synchro elle dérive directement de la position du transport, donc
    l'LFO reste calé sur Ableton même après un saut de boucle, sans dérive.
*/
class Lfo
{
public:
    enum Shape { sine = 0, triangle, ramp, square, sampleHold };

    /** @param phase       position dans le cycle (0..1)
        @param cycleIndex  numéro du cycle (sert à tirer la valeur du S&H de façon reproductible) */
    static inline float shapeValue (int shape, float phase, int64_t cycleIndex, uint32_t seed) noexcept
    {
        switch (shape)
        {
            case sine:       return FastMath::sine() (phase);
            case triangle:   return phase < 0.25f ? 4.0f * phase
                                  : (phase < 0.75f ? 2.0f - 4.0f * phase : 4.0f * phase - 4.0f);
            case ramp:       return 2.0f * phase - 1.0f;
            case square:     return phase < 0.5f ? 1.0f : -1.0f;
            case sampleHold: return FastMath::hash01 (FastMath::mix ((uint32_t) cycleIndex, seed)) * 2.0f - 1.0f;
            default:         return 0.0f;
        }
    }
};

//==============================================================================
/**
    Filtre à variables d'état (topologie TPT / trapézoïdale), sorties LP, BP et HP simultanées.
    Stable à toute fréquence de coupure et toute résonance, sans clic quand on module.
*/
struct Svf
{
    float ic1 = 0.0f, ic2 = 0.0f;

    void reset() noexcept { ic1 = ic2 = 0.0f; }

    /** g = tan(pi * fc / fs), k = 1/Q (2 = amorti, ~0 = auto-oscillation). */
    inline void process (float x, float g, float k, float& lp, float& bp, float& hp) noexcept
    {
        const float a1 = 1.0f / (1.0f + g * (g + k));
        const float a2 = g * a1;
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + g * v1;   // équivaut à ic2 + a2*ic1 + a3*v3
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        lp = v2;
        bp = v1;
        hp = x - k * v1 - v2;
    }
};
