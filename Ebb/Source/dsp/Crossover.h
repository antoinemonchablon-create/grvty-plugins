// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <array>
#include <cmath>

//==============================================================================
/**
    Séparation graves / aigus pour le mode multibande : Linkwitz-Riley 24 dB/oct (deux Butterworth 12 dB/oct en
    cascade, structure TPT stable quand on bouge la fréquence). graves + aigus = passe-tout (amplitude plate) :
    quand rien ne pompe, le son garde exactement son niveau à toutes les fréquences.
    Même algorithme que juce::dsp::LinkwitzRileyFilter, en tableaux fixes (2 canaux, aucune allocation).
*/
namespace Ebb
{
    class Crossover
    {
    public:
        static constexpr int maxChannels = 2;

        void prepare (double sampleRate) noexcept
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            setCutoff (freq);
            reset();
        }

        void reset() noexcept
        {
            s1.fill (0.0f); s2.fill (0.0f); s3.fill (0.0f); s4.fill (0.0f);
        }

        void setCutoff (float hz) noexcept
        {
            const float nyq = (float) (sr * 0.45);
            freq = hz < 10.0f ? 10.0f : (hz > nyq ? nyq : hz);
            g = (float) std::tan (3.14159265358979323846 * (double) freq / sr);
            h = 1.0f / (1.0f + r2 * g + g * g);
        }

        float getCutoff() const noexcept { return freq; }

        /** Un échantillon : graves et aigus. */
        void process (int ch, float x, float& low, float& high) noexcept
        {
            auto& a = s1[(size_t) ch];
            auto& b = s2[(size_t) ch];
            auto& c = s3[(size_t) ch];
            auto& d = s4[(size_t) ch];

            const float yH = (x - (r2 + g) * a - b) * h;
            const float yB = g * yH + a;
            a = g * yH + yB;
            const float yL = g * yB + b;
            b = g * yB + yL;

            const float yH2 = (yL - (r2 + g) * c - d) * h;
            const float yB2 = g * yH2 + c;
            c = g * yH2 + yB2;
            const float yL2 = g * yB2 + d;
            d = g * yB2 + yL2;

            low = yL2;
            high = yL - r2 * yB + yH - yL2;
        }

        /** Remet à zéro les états minuscules (silence prolongé). */
        void snapToZero() noexcept
        {
            for (auto* arr : { &s1, &s2, &s3, &s4 })
                for (auto& v : *arr)
                    if (std::abs (v) < 1.0e-20f) v = 0.0f;
        }

    private:
        double sr = 48000.0;
        float freq = 150.0f, g = 0.0f, h = 1.0f;
        const float r2 = 1.41421356237f;
        std::array<float, maxChannels> s1 {}, s2 {}, s3 {}, s4 {};
    };
}
