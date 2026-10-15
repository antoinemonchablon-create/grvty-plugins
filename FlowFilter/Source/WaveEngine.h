// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Flow Filter, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <cmath>

//==============================================================================
/**
    Moteur de vague "eau", continu : l'amplitude (0..1) peut être évaluée
    à n'importe quelle position x (0 = grave, 1 = aigu), sans interpolation.

    - Multiply : nombre de cycles de vague sur tout le spectre
    - Ripple   : ajoute une contre-vague plus rapide, en sens inverse
                 -> interférences irrégulières "aquatiques"
    - Scan     : fondu entre une vague qui voyage (0) et une vague stationnaire (1)
    - Offset   : décale la vague (avec Rate = 0 -> contrôle manuel d'un cycle)
    - Invert   : -1..+1, profondeur signée de la modulation (0 = plat)
    - Gravity  : 0..1, courbe l'amplitude vers le bas (>0.5) ou vers le haut (<0.5)

    Utilisation : setState() une fois par bloc de contrôle, puis valueAt() autant
    de fois que nécessaire (une fois par bande côté audio, par pixel côté interface).
*/
class WaveEngine
{
public:
    struct Params
    {
        float multiply = 1.5f;   // 0.25 .. 8
        float ripple   = 0.3f;   // 0 .. 1
        float scan     = 0.0f;   // 0 .. 1
        float offset   = 0.0f;   // 0 .. 1 (en cycles)
        float invert   = 1.0f;   // -1 .. 1
        float gravity  = 0.5f;   // 0 .. 1
    };

    void setState (double phaseCycles, const Params& p) noexcept
    {
        params  = p;
        ph      = phaseCycles + (double) p.offset;
        gravExp = std::pow (6.0, (double) (p.gravity - 0.5f) * 2.0);

        // termes qui ne dépendent que du temps : calculés une seule fois
        standingT = std::cos (twoPi * ph);
        counterT  = -1.618 * ph;
        counterEnvT = 0.41 * ph;
    }

    /** Amplitude 0..1 à la position x (0..1) le long du spectre. */
    float valueAt (float x) const noexcept
    {
        const double k   = (double) params.multiply * (double) x;
        const double rip = (double) params.ripple;
        const double scn = (double) params.scan;

        const double travelling = std::sin (twoPi * (ph + k));
        const double standing   = std::sin (twoPi * (k + 0.25)) * standingT;
        double w = (1.0 - scn) * travelling + scn * standing;

        if (rip > 0.0)
        {
            const double counter = std::sin (twoPi * (counterT + 2.37 * k + 0.13))
                                 * std::cos (twoPi * (counterEnvT + 0.5 * k));
            w = (1.0 - 0.5 * rip) * w + rip * counter;
        }

        double a = 0.5 + 0.5 * clamp (w, -1.0, 1.0);
        a = 0.5 + (a - 0.5) * (double) params.invert;
        a = clamp (a, 0.0, 1.0);

        if (gravExp != 1.0)
            a = std::pow (a, gravExp);

        return (float) a;
    }

private:
    static constexpr double twoPi = 6.283185307179586;
    static double clamp (double v, double lo, double hi) noexcept { return v < lo ? lo : (v > hi ? hi : v); }

    Params params;
    double ph = 0.0, gravExp = 1.0;
    double standingT = 1.0, counterT = 0.0, counterEnvT = 0.0;
};
