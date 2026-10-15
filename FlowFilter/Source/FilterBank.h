// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Flow Filter, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <array>
#include <cmath>
#include <complex>
#include <cstdint>

//==============================================================================
/**
    Filtre passe-bande "state variable" (topologie TPT d'Andrew Simper / Cytomic).
    Stable même quand on module vite, gain de crête = 1 (0 dB) à la fréquence centrale.
*/
struct BandpassSVF
{
    void setup (double freqHz, double q, double sampleRate) noexcept
    {
        const double maxF = 0.45 * sampleRate;
        const double f    = freqHz < maxF ? freqHz : maxF;
        const double g    = std::tan (3.141592653589793 * f / sampleRate);
        k  = 1.0 / q;
        a1 = 1.0 / (1.0 + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    void reset() noexcept { ic1 = ic2 = 0.0; }

    inline float process (float in) noexcept
    {
        const double v3 = (double) in - ic2;
        const double v1 = a1 * ic1 + a2 * v3;
        const double v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0 * v1 - ic1;
        ic2 = 2.0 * v2 - ic2;
        return (float) (k * v1);   // passe-bande normalisé
    }

    double k = 1.0, a1 = 0.0, a2 = 0.0, a3 = 0.0;
    double ic1 = 0.0, ic2 = 0.0;
};

//==============================================================================
/**
    Banc de 64 passe-bandes à fréquences fixes (espacement logarithmique).

    Répartition stéréo : les bandes paires sont envoyées à gauche, les impaires à droite.
    Width = 1 -> alternance complète (effet stéréo maximal)
    Width = 0 -> chaque bande va dans les deux canaux (mono)
    Comme L + R contient toujours toutes les bandes, le résultat reste compatible mono.

*/
class FilterBank
{
public:
    static constexpr int maxBands = 64;
    static constexpr double lowestHz  = 40.0;
    static constexpr double highestHz = 16000.0;

    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate;
        numBands = 0;
        currentQ = -1.0;
        currentWidth = -1.0f;
        setNumBands (maxBands);
        setQ (3.0);
        setWidth (1.0f);

        for (auto& g : gains)       g = 1.0f;
        for (auto& g : targetGains) g = 1.0f;
        reset();
    }

    void reset()
    {
        for (auto& f : filtersL) f.reset();
        for (auto& f : filtersR) f.reset();
    }

    void setNumBands (int n)
    {
        n = n < 2 ? 2 : (n > maxBands ? maxBands : n);
        if (n == numBands)
            return;

        numBands = n;
        for (int b = 0; b < numBands; ++b)
            freqs[(size_t) b] = lowestHz * std::pow (highestHz / lowestHz, (double) b / (numBands - 1));

        reset();
        updateWeights();
        updateFilters();
    }

    void setQ (double q)
    {
        if (std::abs (q - currentQ) < 1.0e-4)
            return;

        currentQ = q;
        updateFilters();
    }

    void setWidth (float w)
    {
        if (std::abs (w - currentWidth) < 1.0e-4f)
            return;

        currentWidth = w;
        updateWeights();
        updateCompensation();
    }

    /** Temps (ms) mis par une bande pour rejoindre sa nouvelle valeur. */
    void setReleaseMs (float ms)
    {
        const double t = (ms < 1.0f ? 1.0 : (double) ms) * 0.001;
        smoothCoef = (float) std::exp (-1.0 / (t * sampleRate));
    }

    void setTargetGain (int band, float linearGain) noexcept { targetGains[(size_t) band] = linearGain; }

    /** Mode analogique : dérive lente et aléatoire de chaque bande (fréquence + niveau)
        et légère saturation. amount = 0..1, appelé à chaque bloc de contrôle. */
    void updateAnalog (float amount, double dt)
    {
        analogAmount = amount;
        if (amount <= 0.0001f)
        {
            if (analogWasOn)                    // retour aux fréquences exactes
            {
                for (auto& d : driftF) d = 0.0f;
                for (auto& d : driftG) d = 0.0f;
                updateFilters();
                analogWasOn = false;
            }
            return;
        }

        analogWasOn = true;

        // la dérive est lente : inutile de recalculer les 128 filtres à chaque bloc
        analogTimer += dt;
        if (analogTimer < 0.003)
            return;
        dt = analogTimer;
        analogTimer = 0.0;

        const float smooth = (float) std::exp (-dt / 0.35);   // dérive "lente" (~0.35 s)
        const double q = effectiveQ();

        for (int b = 0; b < numBands; ++b)
        {
            driftTargetF[(size_t) b] += (nextRandom() * 0.35f) * (float) std::sqrt (dt * 20.0);
            driftTargetF[(size_t) b] *= 0.995f;
            driftTargetG[(size_t) b] += (nextRandom() * 0.35f) * (float) std::sqrt (dt * 20.0);
            driftTargetG[(size_t) b] *= 0.995f;

            driftF[(size_t) b] = driftTargetF[(size_t) b] + (driftF[(size_t) b] - driftTargetF[(size_t) b]) * smooth;
            driftG[(size_t) b] = driftTargetG[(size_t) b] + (driftG[(size_t) b] - driftTargetG[(size_t) b]) * smooth;

            const double f = freqs[(size_t) b] * std::pow (2.0, (double) (amount * 0.12f * clampUnit (driftF[(size_t) b])));
            filtersL[(size_t) b].setup (f, q, sampleRate);
            filtersR[(size_t) b].setup (f * (1.0 + 0.004 * amount), q, sampleRate);   // léger écart L/R
        }
    }

    inline void processSample (float inL, float inR, float& outL, float& outR) noexcept
    {
        float sumL = 0.0f, sumR = 0.0f;
        const float c = smoothCoef;

        for (int b = 0; b < numBands; ++b)
        {
            auto& g = gains[(size_t) b];
            g = targetGains[(size_t) b] + (g - targetGains[(size_t) b]) * c;

            const float wobble = 1.0f + analogAmount * 0.25f * clampUnit (driftG[(size_t) b]);
            sumL += filtersL[(size_t) b].process (inL) * weightL[(size_t) b] * g * wobble;
            sumR += filtersR[(size_t) b].process (inR) * weightR[(size_t) b] * g * wobble;
        }

        outL = sumL * compensation;
        outR = sumR * compensation;

        if (analogAmount > 0.0001f)   // saturation douce, transparente à bas niveau
        {
            const float drive = 1.0f + 2.5f * analogAmount;
            outL = std::tanh (outL * drive) / drive;
            outR = std::tanh (outR * drive) / drive;
        }
    }

    int    getNumBands() const noexcept              { return numBands; }
    double getBandFrequency (int b) const noexcept   { return freqs[(size_t) b]; }

private:
    static float clampUnit (float v) noexcept { return v < -1.0f ? -1.0f : (v > 1.0f ? 1.0f : v); }

    float nextRandom() noexcept
    {
        rng = rng * 1664525u + 1013904223u;
        return (float) ((double) rng / 4294967295.0 * 2.0 - 1.0);
    }

    double effectiveQ() const noexcept
    {
        return currentQ < 0.3 ? 0.3 : currentQ;
    }

    void updateWeights()
    {
        const float w = currentWidth < 0.0f ? 1.0f : currentWidth;
        for (int b = 0; b < maxBands; ++b)
        {
            const bool even = (b % 2) == 0;
            weightL[(size_t) b] = even ? 1.0f : 1.0f - w;
            weightR[(size_t) b] = even ? 1.0f - w : 1.0f;
        }
    }

    void updateFilters()
    {
        if (currentQ <= 0.0 || numBands == 0)
            return;

        const double q = effectiveQ();
        for (int b = 0; b < numBands; ++b)
        {
            filtersL[(size_t) b].setup (freqs[(size_t) b], q, sampleRate);
            filtersR[(size_t) b].setup (freqs[(size_t) b], q, sampleRate);
        }
        updateCompensation();
    }

    /** Gain de rattrapage pour que "toutes bandes ouvertes" ≈ 0 dB (limité à +12 dB). */
    void updateCompensation()
    {
        if (currentQ <= 0.0 || numBands == 0)
            return;

        constexpr int numPoints = 96;
        const double q = effectiveQ();
        double sum = 0.0;

        for (int p = 0; p < numPoints; ++p)
        {
            const double f = 80.0 * std::pow (10000.0 / 80.0, (double) p / (numPoints - 1));
            std::complex<double> h (0.0, 0.0);

            for (int b = 0; b < numBands; ++b)
            {
                // prototype analogique du passe-bande normalisé : (s/Q) / (s² + s/Q + 1)
                const std::complex<double> s (0.0, f / freqs[(size_t) b]);
                h += (s / q) / (s * s + s / q + 1.0) * (double) weightL[(size_t) b];
            }
            sum += std::abs (h);
        }

        const double avg = sum / numPoints;
        const double comp = avg > 1.0e-3 ? 1.0 / avg : 1.0;
        compensation = (float) (comp > 4.0 ? 4.0 : comp);
    }

    double sampleRate = 44100.0;
    int    numBands   = 0;
    double currentQ   = -1.0;
    float  currentWidth = -1.0f;
    float  smoothCoef = 0.99f;
    float  compensation = 1.0f;

    std::array<double, maxBands> freqs {};
    std::array<BandpassSVF, maxBands> filtersL, filtersR;
    std::array<float, maxBands> gains {}, targetGains {}, weightL {}, weightR {};
    std::array<float, maxBands> driftF {}, driftG {}, driftTargetF {}, driftTargetG {};
    float analogAmount = 0.0f;
    double analogTimer = 0.0;
    bool  analogWasOn  = false;
    uint32_t rng = 12345u;
};
