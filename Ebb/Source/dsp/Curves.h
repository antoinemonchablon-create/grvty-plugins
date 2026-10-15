// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <array>
#include <cmath>

//==============================================================================
/**
    Les 12 courbes d'Ebb et les durées (Rate).

    Une courbe donne la BAISSE r(x) entre 0 (aucune baisse) et 1 (baisse maximale) pour x dans [0, 1],
    x = position dans la courbe (0 = le temps, le kick, la note MIDI). Le gain appliqué vaut 1 - Amount * r(x) :
    Ebb ne monte jamais le niveau.

    Rangées de la plus sèche à la plus douce, puis les deux formes spéciales (rampe et inverse).
    Les valeurs sont précalculées en tables (8192 points, interpolation linéaire) à la construction du plugin.
*/
namespace Ebb
{
    enum CurveId
    {
        curveSplash = 0,   // chute très courte : ne libère que l'attaque du kick
        curveSnap,         // courte et rapide
        curveRipple,       // la pompe classique (défaut)
        curveCurrent,      // remontée linéaire
        curveLedge,        // palier : retombe à mi-hauteur, attend, puis remonte
        curveTwin,         // double pompe (une seconde baisse au milieu)
        curveGate,         // gate net : coupé pendant la première moitié
        curveSwell,        // en S, longue
        curveDrift,        // très douce, sur presque toute la durée
        curveBreath,       // respiration : cosinus, sans aucun angle
        curveSurge,        // rampe : le niveau remonte sur toute la durée
        curveBackwash,     // inverse : le niveau baisse en approchant du temps suivant
        numCurves
    };

    inline const char* curveName (int c)
    {
        static const char* names[numCurves] = { "Splash", "Snap", "Ripple", "Current", "Ledge", "Twin",
                                                "Gate", "Swell", "Drift", "Breath", "Surge", "Backwash" };
        return names[c < 0 || c >= numCurves ? curveRipple : c];
    }

    //==============================================================================
    namespace Shape
    {
        inline double clamp01 (double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }
        inline double smooth (double u)  { u = clamp01 (u); return u * u * (3.0 - 2.0 * u); }

        /** Retombée de 1 à 0 sur [0, 1] : exponentielle normalisée (remontée rapide du niveau puis plus lente). */
        inline double fastExp (double u, double k) { u = clamp01 (u); return (std::exp (-k * u) - std::exp (-k)) / (1.0 - std::exp (-k)); }

        /** Maintien à 1 jusqu'à 'hold', puis retombée sur 'len' avec la forme f(u) (u de 0 à 1). */
        template <typename F>
        inline double release (double x, double hold, double len, F f)
        {
            if (x < hold) return 1.0;
            const double u = (x - hold) / len;
            return u >= 1.0 ? 0.0 : f (u);
        }
    }

    /** Valeur exacte de la courbe (double précision) : sert à remplir les tables. */
    inline double curveValue (int c, double x)
    {
        using namespace Shape;
        x = clamp01 (x);
        constexpr double pi = 3.14159265358979323846;

        switch (c)
        {
            case curveSplash:   return release (x, 0.03, 0.15, [] (double u) { return fastExp (u, 4.0); });
            case curveSnap:     return release (x, 0.05, 0.30, [] (double u) { return fastExp (u, 3.0); });
            case curveRipple:   return release (x, 0.02, 0.60, [] (double u) { return (1.0 - u) * (1.0 - u); });
            case curveCurrent:  return release (x, 0.00, 0.70, [] (double u) { return 1.0 - u; });
            case curveLedge:
                if (x < 0.08) return 1.0;
                if (x < 0.16) return 1.0 - 0.5 * smooth ((x - 0.08) / 0.08);
                if (x < 0.50) return 0.5;
                if (x < 0.62) return 0.5 * (1.0 - smooth ((x - 0.50) / 0.12));
                return 0.0;
            case curveTwin:
            {
                // seconde baisse (75 %) au milieu, avec sa propre attaque sur 1 % de la durée : pas de marche dans la courbe
                auto dip = [] (double u) { return release (u, 0.01, 0.32, [] (double v) { return (1.0 - v) * (1.0 - v); }); };
                if (x < 0.5)  return dip (x);
                if (x < 0.51) return 0.75 * smooth ((x - 0.5) / 0.01);
                return 0.75 * dip (x - 0.5);
            }
            case curveGate:
                if (x < 0.45) return 1.0;
                return 1.0 - smooth ((x - 0.45) / 0.05);
            case curveSwell:    return release (x, 0.10, 0.60, [] (double u) { return 1.0 - smooth (u); });
            case curveDrift:    return release (x, 0.00, 0.95, [] (double u) { return 0.5 * (1.0 + std::cos (3.14159265358979323846 * u)); });
            case curveBreath:   return 0.5 * (1.0 + std::cos (2.0 * pi * x));
            case curveSurge:    return 1.0 - x;
            case curveBackwash: return x < 0.30 ? 0.0 : std::pow ((x - 0.30) / 0.70, 2.0);
            default:            return 0.0;
        }
    }

    //==============================================================================
    /** Tables des 12 courbes (une seule instance, remplie à la première utilisation, hors du thread audio). */
    class CurveTables
    {
    public:
        static constexpr int size = 8192;

        static const CurveTables& get()
        {
            static const CurveTables t;
            return t;
        }

        /** x dans [0, 1] ; interpolation linéaire entre deux points de la table. */
        float lookup (int c, float x) const noexcept { return lookupTable (table (c), x); }

        /** La table d'une courbe : size + 1 valeurs (la dernière est la valeur en x = 1). */
        const float* table (int c) const noexcept { return data[(size_t) (c < 0 || c >= numCurves ? curveRipple : c)].data(); }

        /** Lecture interpolée de n'importe quelle table de size + 1 valeurs (courbes d'usine, courbe perso, courbe lissée). */
        static float lookupTable (const float* tab, float x) noexcept
        {
            const float pos = (x <= 0.0f ? 0.0f : (x >= 1.0f ? 1.0f : x)) * (float) size;
            const int i = (int) pos;
            if (i >= size) return tab[size];
            const float f = pos - (float) i;
            return tab[i] + f * (tab[i + 1] - tab[i]);
        }

    private:
        CurveTables()
        {
            for (int c = 0; c < numCurves; ++c)
                for (int i = 0; i <= size; ++i)
                    data[(size_t) c][(size_t) i] = (float) curveValue (c, (double) i / (double) size);
        }

        std::array<std::array<float, size + 1>, numCurves> data {};
    };

    //==============================================================================
    enum RateId
    {
        rate16th = 0,
        rate8th,
        rateQuarterTriplet,
        rateDotted8th,
        rateQuarter,       // défaut
        rateHalf,
        rateBar,
        numRates
    };

    inline const char* rateName (int r)
    {
        static const char* names[numRates] = { "1/16", "1/8", "1/4 triolet", "1/8 point\xc3\xa9", "1/4", "1/2", "1 mesure" };
        return names[r < 0 || r >= numRates ? rateQuarter : r];
    }

    /** Durée de la courbe en noires. 'barBeats' : durée d'une mesure en noires (4 en 4/4). */
    inline double rateBeats (int r, double barBeats)
    {
        switch (r)
        {
            case rate16th:            return 0.25;
            case rate8th:             return 0.5;
            case rateQuarterTriplet:  return 2.0 / 3.0;
            case rateDotted8th:       return 0.75;
            case rateQuarter:         return 1.0;
            case rateHalf:            return 2.0;
            case rateBar:             return barBeats > 0.0 ? barBeats : 4.0;
            default:                  return 1.0;
        }
    }
}
