// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "FastMath.h"

//==============================================================================
/**
    Delay ping-pong synchronisé :
      entrée mono -> ligne gauche -> ligne droite -> ligne gauche...
    Filtre (Tone) et saturation douce dans la boucle : les répétitions s'assombrissent
    ou s'amincissent et ne peuvent jamais s'emballer. Le temps glisse en douceur
    quand le tempo change (pas de clic).
*/
class PingPongDelay
{
public:
    void prepare (double sr, double maxSeconds)
    {
        sampleRate = sr;
        size = (int) std::ceil (maxSeconds * sr) + 4;
        bufL.assign ((size_t) size, 0.0f);
        bufR.assign ((size_t) size, 0.0f);
        timeCoeff = FastMath::onePoleCoeff (60.0f, sr);
        fadeInc = 1.0f / (float) (0.03 * sr);
        paramCoeff = FastMath::onePoleCoeff (15.0f, sr);
        reset();
    }

    void reset()
    {
        std::fill (bufL.begin(), bufL.end(), 0.0f);
        std::fill (bufR.begin(), bufR.end(), 0.0f);
        write = 0;
        lpL = lpR = hpL = hpR = 0.0f;
        currentDelay = -1.0f;
        fading = false;
        sMix = mixTarget; sFb = fbTarget;
    }

    void setParameters (float delaySamples, float feedback, float tone, float mix) noexcept
    {
        targetDelay = std::clamp (delaySamples, 1.0f, (float) (size - 3));
        if (currentDelay < 0.0f) currentDelay = targetDelay;
        fbTarget  = std::clamp (feedback, 0.0f, 0.95f);
        mixTarget = std::clamp (mix, 0.0f, 1.0f);

        // Tone < 0 : passe-bas qui descend jusqu'à ~700 Hz ; > 0 : passe-haut qui monte jusqu'à ~2.5 kHz
        const float lpHz = tone < 0.0f ? 16000.0f * std::pow (0.045f, -tone) : 16000.0f;
        const float hpHz = tone > 0.0f ? 40.0f * std::pow (60.0f, tone) : 40.0f;
        lpCoeff = 1.0f - std::exp (-FastMath::twoPi * lpHz / (float) sampleRate);
        hpCoeff = 1.0f - std::exp (-FastMath::twoPi * hpHz / (float) sampleRate);
    }

    bool isSilent() const noexcept { return mixTarget <= 0.0f && sMix < 1.0e-5f; }

    /** in : signal mono ; ajoute l'écho aux sorties gauche/droite. */
    inline void process (float in, float& outL, float& outR) noexcept
    {
        sMix += (mixTarget - sMix) * paramCoeff;
        sFb  += (fbTarget - sFb) * paramCoeff;

        // Petit écart (dérive de tempo) : on glisse. Grand écart (nouvelle division) : fondu
        // enchaîné de 30 ms entre l'ancienne et la nouvelle position de lecture, sans effet « bande ».
        if (! fading)
        {
            const float diff = targetDelay - currentDelay;
            if (std::abs (diff) > 2.0f)
            {
                nextDelay = targetDelay;
                fade = 0.0f;
                fading = true;
            }
            else
            {
                currentDelay += diff * timeCoeff;
            }
        }

        float dL = read (bufL, currentDelay), dR = read (bufR, currentDelay);
        if (fading)
        {
            fade += fadeInc;
            const float w = std::min (1.0f, fade);
            dL += (read (bufL, nextDelay) - dL) * w;
            dR += (read (bufR, nextDelay) - dR) * w;
            if (fade >= 1.0f) { currentDelay = nextDelay; fading = false; }
        }

        // filtres de boucle
        lpL += (dL - lpL) * lpCoeff;  hpL += (lpL - hpL) * hpCoeff;
        lpR += (dR - lpR) * lpCoeff;  hpR += (lpR - hpR) * hpCoeff;
        const float fL = lpL - hpL, fR = lpR - hpR;

        // ping-pong : gauche reçoit l'entrée + le retour de droite, droite reçoit le retour de gauche
        bufL[(size_t) write] = FastMath::tanh (in + fR * sFb);
        bufR[(size_t) write] = FastMath::tanh (fL * sFb);
        if (++write >= size) write = 0;

        outL += fL * sMix;
        outR += fR * sMix;
    }

private:
    double sampleRate = 44100.0;
    std::vector<float> bufL, bufR;
    int size = 0, write = 0;
    inline float read (const std::vector<float>& buf, float delaySamples) const noexcept
    {
        float rp = (float) write - delaySamples;
        if (rp < 0.0f) rp += (float) size;
        const int   i0 = (int) rp;
        const float fr = rp - (float) i0;
        const int   i1 = i0 + 1 >= size ? 0 : i0 + 1;
        return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
    }

    float targetDelay = 1000.0f, currentDelay = -1.0f, nextDelay = 1000.0f, timeCoeff = 0.001f, paramCoeff = 0.01f;
    float fade = 0.0f, fadeInc = 0.001f;
    bool  fading = false;
    float fbTarget = 0.0f, mixTarget = 0.0f, sFb = 0.0f, sMix = 0.0f;
    float lpCoeff = 1.0f, hpCoeff = 0.0f, lpL = 0.0f, lpR = 0.0f, hpL = 0.0f, hpR = 0.0f;
};

//==============================================================================
/** Petite réverbération (taille, amortissement, dosage), sans allocation pendant le traitement. */
class SmallReverb
{
public:
    void prepare (double sr)
    {
        reverb.setSampleRate (sr);
        reverb.reset();
        mixCoeff = FastMath::onePoleCoeff (15.0f, sr);
    }

    void reset() { reverb.reset(); }

    void setParameters (float size, float damp, float mix) noexcept
    {
        mixTarget = std::clamp (mix, 0.0f, 1.0f);
        juce::Reverb::Parameters p;
        p.roomSize   = 0.30f + 0.65f * std::clamp (size, 0.0f, 1.0f);
        p.damping    = std::clamp (damp, 0.0f, 1.0f);
        p.wetLevel   = 1.0f;
        p.dryLevel   = 0.0f;
        p.width      = 1.0f;
        p.freezeMode = 0.0f;
        if (std::abs (p.roomSize - last.roomSize) > 1.0e-4f || std::abs (p.damping - last.damping) > 1.0e-4f)
        {
            reverb.setParameters (p);
            last = p;
        }
    }

    bool isSilent() const noexcept { return mixTarget <= 0.0f && sMix < 1.0e-5f; }

    /** Ajoute la réverbération au signal stéréo (entièrement humide, dosée par Mix). */
    void process (float* left, float* right, float* scratchL, float* scratchR, int n) noexcept
    {
        std::copy (left, left + n, scratchL);
        std::copy (right, right + n, scratchR);
        reverb.processStereo (scratchL, scratchR, n);

        for (int i = 0; i < n; ++i)
        {
            sMix += (mixTarget - sMix) * mixCoeff;
            // loi à puissance constante : le sec baisse un peu quand la réverbe monte
            const float dry = 1.0f - 0.35f * sMix;
            const float wet = 0.55f * sMix;
            left[i]  = left[i]  * dry + scratchL[i] * wet;
            right[i] = right[i] * dry + scratchR[i] * wet;
        }
    }

private:
    juce::Reverb reverb;
    juce::Reverb::Parameters last { -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    float mixTarget = 0.0f, sMix = 0.0f, mixCoeff = 0.01f;
};
