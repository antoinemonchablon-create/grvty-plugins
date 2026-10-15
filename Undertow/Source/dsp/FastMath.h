// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <array>
#include <cmath>
#include <cstdint>

//==============================================================================
/** Outils mathématiques rapides, sans allocation, utilisables sur le thread audio. */
namespace FastMath
{
    constexpr float twoPi = 6.283185307179586f;

    /** Table de sinus précalculée (un cycle, 2048 points + garde), lecture interpolée. */
    struct SineTable
    {
        static constexpr int size = 2048;
        std::array<float, size + 1> t {};

        SineTable()
        {
            for (int i = 0; i <= size; ++i)
                t[(size_t) i] = (float) std::sin (6.283185307179586 * (double) i / (double) size);
        }

        /** phase en cycles, n'importe quelle valeur (négative ou > 1). */
        inline float operator() (float phase) const noexcept
        {
            const float p   = phase - std::floor (phase);
            const float idx = p * (float) size;
            const int   i   = (int) idx;
            const float f   = idx - (float) i;
            const float a   = t[(size_t) i];
            return a + f * (t[(size_t) i + 1] - a);
        }
    };

    inline const SineTable& sine()
    {
        static const SineTable table;
        return table;
    }

    /** tanh rationnel (erreur < 0.2 % sur ±4.5, saturé au-delà). */
    inline float tanh (float x) noexcept
    {
        if (x > 4.97f)  return 1.0f;
        if (x < -4.97f) return -1.0f;
        const float x2 = x * x;
        const float a  = x * (135135.0f + x2 * (17325.0f + x2 * (378.0f + x2)));
        const float b  = 135135.0f + x2 * (62370.0f + x2 * (3150.0f + x2 * 28.0f));
        const float r  = a / b;
        return r > 1.0f ? 1.0f : (r < -1.0f ? -1.0f : r);
    }

    /** 2^x rapide (précision ~1e-4 relative), suffisant pour la hauteur et le cutoff lissés. */
    inline float exp2 (float x) noexcept
    {
        x = x < -126.0f ? -126.0f : (x > 126.0f ? 126.0f : x);
        const float fl = std::floor (x);
        const float f  = x - fl;
        // polynôme minimax de 2^f sur [0,1)
        const float p = 1.0f + f * (0.6931472f + f * (0.2402265f + f * (0.0555041f + f * (0.0096181f + f * 0.0013334f))));
        return std::ldexp (p, (int) fl);
    }

    /** Hash déterministe 32 bits -> [0,1). Sert aux probabilités et au Mutate, reproductibles d'un rendu à l'autre. */
    inline float hash01 (uint32_t x) noexcept
    {
        x ^= x >> 16; x *= 0x7feb352dU;
        x ^= x >> 15; x *= 0x846ca68bU;
        x ^= x >> 16;
        return (float) (x >> 8) * (1.0f / 16777216.0f);
    }

    inline uint32_t mix (uint32_t a, uint32_t b) noexcept { return a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2)); }

    /** Coefficient d'un lissage exponentiel à un pôle pour un temps (ms) donné. */
    inline float onePoleCoeff (float ms, double sampleRate) noexcept
    {
        if (ms <= 0.0f) return 1.0f;
        return 1.0f - (float) std::exp (-1.0 / (0.001 * (double) ms * sampleRate));
    }
}
