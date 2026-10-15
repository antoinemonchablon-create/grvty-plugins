// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Flow Filter, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <cmath>
#include <cstdint>

//==============================================================================
/**
    LFO de modulation interne (sinus, triangle, rampe, carré, aléatoire).
    Sortie bipolaire -1..+1. Avancé par blocs de contrôle (pas à chaque échantillon).
*/
class ModLfo
{
public:
    enum Shape { sine = 0, triangle, ramp, square, random };

    void reset() noexcept { phase = 0.0; held = 0.0f; }

    /** Avance de dt secondes et renvoie la valeur courante. */
    float advance (double rateHz, int shape, double dtSeconds) noexcept
    {
        double next = phase + rateHz * dtSeconds;
        const bool wrapped = next >= 1.0;
        next -= std::floor (next);
        phase = next;

        if (wrapped || ! seeded)
        {
            held   = nextRandom();
            seeded = true;
        }

        constexpr double twoPi = 6.283185307179586;
        switch (shape)
        {
            case sine:     return (float) std::sin (twoPi * phase);
            case triangle: return (float) (phase < 0.5 ? 4.0 * phase - 1.0 : 3.0 - 4.0 * phase);
            case ramp:     return (float) (2.0 * phase - 1.0);
            case square:   return phase < 0.5 ? 1.0f : -1.0f;
            case random:   return held;
            default:       return 0.0f;
        }
    }

private:
    float nextRandom() noexcept
    {
        state = state * 1664525u + 1013904223u;
        return (float) ((double) state / 4294967295.0 * 2.0 - 1.0);
    }

    double   phase  = 0.0;
    float    held   = 0.0f;
    bool     seeded = false;
    uint32_t state  = 22222u;
};
