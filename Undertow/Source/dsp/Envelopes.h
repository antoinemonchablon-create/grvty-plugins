// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "FastMath.h"
#include <algorithm>

//==============================================================================
/**
    Enveloppe d'ampli ADSR.
    - Attaque linéaire, repart du niveau courant (pas de retour à zéro = pas de clic au retrigger).
    - Decay et Release exponentiels ; le temps affiché correspond à une chute d'environ 60 dB.
*/
class AdsrEnvelope
{
public:
    enum class Stage { idle, attack, decay, sustain, release };

    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        sustainCoeff = FastMath::onePoleCoeff (5.0f, sr);   // un changement de Sustain glisse en 5 ms
        reset();
    }
    void reset() noexcept { level = 0.0f; stage = Stage::idle; }

    void setTimes (float attackMs, float decayMs, float sustainLevel, float releaseMs) noexcept
    {
        attackRate   = 1.0f / std::max (1.0f, (float) (attackMs * 0.001 * sampleRate));
        decayCoeff   = coeffFor (decayMs);
        releaseCoeff = coeffFor (releaseMs);
        sustain      = std::clamp (sustainLevel, 0.0f, 1.0f);
    }

    void noteOn() noexcept  { stage = Stage::attack; }
    void noteOff() noexcept { if (stage != Stage::idle) stage = Stage::release; }

    bool isActive() const noexcept { return stage != Stage::idle; }
    bool isReleasing() const noexcept { return stage == Stage::release; }
    float getLevel() const noexcept { return level; }

    inline float next() noexcept
    {
        switch (stage)
        {
            case Stage::attack:
                level += attackRate;
                if (level >= 1.0f) { level = 1.0f; stage = Stage::decay; }
                break;
            case Stage::decay:
                level = sustain + (level - sustain) * decayCoeff;
                if (std::abs (level - sustain) < 1.0e-4f) { level = sustain; stage = Stage::sustain; }   // descend OU remonte en douceur
                break;
            case Stage::sustain:
                level += (sustain - level) * sustainCoeff;
                if (sustain <= 0.0f && level < 1.0e-5f) { level = 0.0f; stage = Stage::idle; }
                break;
            case Stage::release:
                level *= releaseCoeff;
                if (level < 1.0e-5f) { level = 0.0f; stage = Stage::idle; }
                break;
            case Stage::idle:
            default:
                level = 0.0f;
                break;
        }
        return level;
    }

private:
    float coeffFor (float ms) const noexcept
    {
        const double n = std::max (1.0, (double) ms * 0.001 * sampleRate);
        return (float) std::exp (-6.907755278982137 / n);   // ln(1000) : -60 dB sur la durée
    }

    double sampleRate = 44100.0;
    Stage stage = Stage::idle;
    float level = 0.0f, attackRate = 1.0f, decayCoeff = 0.99f, releaseCoeff = 0.99f, sustain = 1.0f, sustainCoeff = 0.01f;
};

//==============================================================================
/**
    Enveloppe de modulation AD (attaque puis descente à zéro) avec courbe réglable.
      curve = -1 : chute très rapide au début (percussif)
      curve =  0 : linéaire
      curve = +1 : reste haut puis chute à la fin
*/
class ModEnvelope
{
public:
    void prepare (double sr) noexcept { sampleRate = sr; reset(); }
    void reset() noexcept { value = 0.0f; progress = 1.0f; attacking = false; }

    void setTimes (float attackMs, float decayMs, float curve) noexcept
    {
        attackInc = 1.0f / std::max (1.0f, (float) (attackMs * 0.001 * sampleRate));
        decayInc  = 1.0f / std::max (1.0f, (float) (decayMs * 0.001 * sampleRate));
        exponent  = std::pow (2.0f, -std::clamp (curve, -1.0f, 1.0f) * 3.0f);
    }

    /** Redémarre depuis la valeur courante : pas de saut. */
    void trigger() noexcept { attacking = true; }

    float getValue() const noexcept { return value; }

    inline float next() noexcept
    {
        if (attacking)
        {
            value += attackInc;
            if (value >= 1.0f) { value = 1.0f; attacking = false; progress = 0.0f; }
            return value;
        }

        if (progress < 1.0f)
        {
            progress = std::min (1.0f, progress + decayInc);
            const float rem = 1.0f - progress;
            value = rem <= 0.0f ? 0.0f : std::pow (rem, exponent);
        }
        else
        {
            value = 0.0f;
        }
        return value;
    }

private:
    double sampleRate = 44100.0;
    float value = 0.0f, progress = 1.0f, attackInc = 1.0f, decayInc = 0.001f, exponent = 1.0f;
    bool  attacking = false;
};
