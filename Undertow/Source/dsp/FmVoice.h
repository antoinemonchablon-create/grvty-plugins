// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "FastMath.h"
#include "Envelopes.h"
#include "Modulators.h"
#include "../Params.h"
#include <cmath>
#include <limits>

//==============================================================================
/** Réglages de la voix, relus une fois par sous-bloc (valeurs brutes des paramètres). */
struct VoiceSettings
{
    bool  legato = true;
    float glideMs = 0.0f, velSens = 0.5f;
    float shapeA = 0.0f, shapeB = 0.0f, coarseA = 0.0f, fineA = 0.0f, fineB = 0.0f, mix = 0.0f;
    int   ratioIndex = 1;
    bool  freeRatio = false;
    float freeRatioValue = 1.41f;
    float index = 1.5f, feedback = 0.0f;
    int   fmMode = 0;
    float modAttack = 0.5f, modDecay = 250.0f, modCurve = -0.4f;
    float modToIndex = 0.0f, modToCutoff = 0.0f, modToPitch = 0.0f;
    int   filterType = 0, filterSlope = 0;
    float cutoff = 20000.0f, resonance = 0.1f, filterEnv = 0.0f, filterDrive = 0.0f, keyTrack = 0.0f;
    float attack = 0.5f, decay = 300.0f, sustain = 0.8f, release = 60.0f;
    float fxDrive = 0.0f;
    float accentAmount = 0.5f;
};

/** Modulations externes (LFO + piste Mod du séquenceur), valeur par destination, par échantillon de base. */
struct VoiceMods
{
    float pitch = 0.0f;      // demi-tons
    float index = 0.0f;      // unités d'index
    float ratioOct = 0.0f;   // octaves de ratio (multiplicatif)
    int   ratioSteps = 0;    // décalage dans la table de ratios (piste Mod, ratio par pas)
    float feedback = 0.0f, cutoff = 0.0f /* demi-tons */, resonance = 0.0f;
    float shapeA = 0.0f, shapeB = 0.0f, mix = 0.0f, drive = 0.0f;
    float ampGain = 1.0f;
};

//==============================================================================
/**
    Voix FM monophonique : 2 oscillateurs (A porteur, B modulateur), filtre, enveloppes.

    Le cœur audio tourne à la fréquence suréchantillonnée (2x ou 4x) ; les contrôles
    (enveloppes, glide, lissage) sont calculés une fois par échantillon de base.
*/
class FmVoice
{
public:
    void prepare (double baseSampleRate)
    {
        baseRate = baseSampleRate;
        ampEnv.prepare (baseRate);
        modEnv.prepare (baseRate);
        smoothCoeff  = FastMath::onePoleCoeff (8.0f, baseRate);    // lissage des paramètres
        fastCoeff    = FastMath::onePoleCoeff (1.5f, baseRate);    // lissage des sauts (types de filtre, modes)
        reset();
    }

    void reset()
    {
        ampEnv.reset();
        modEnv.reset();
        svf1.reset(); svf2.reset();
        phA = phB = 0.0f;
        prevA = prevB1 = prevB2 = 0.0f;
        lastAmp = 0.0f;
        gateOn = false;
        hasPlayed = false;
        initialised = false;
    }

    void setOversampling (int factor) noexcept { osFactor = factor; osRate = baseRate * factor; }

    //==============================================================================
    /** Déclenche une note. Si une note est encore tenue (ou si slide), on glisse sans relancer les enveloppes (en legato). */
    void noteOn (int midiNote, float velocity, bool accent, bool slide,
                 float plockIndex = std::numeric_limits<float>::quiet_NaN(),
                 float plockCutoff = std::numeric_limits<float>::quiet_NaN()) noexcept
    {
        const bool transition = hasPlayed && (gateOn || slide) && ampEnv.isActive();
        targetPitch = (float) midiNote;
        currentNote = midiNote;

        if (! transition)
            pitch = targetPitch;                      // attaque franche : la hauteur saute directement

        glideActive = transition;
        slideGlide  = slide;
        glideCoeff  = FastMath::onePoleCoeff (0.25f * (slide && settings.glideMs < 1.0f ? 60.0f : settings.glideMs), baseRate);

        const bool retrigger = ! transition || (! settings.legato && ! slide);
        if (retrigger)
        {
            ampEnv.noteOn();
            modEnv.trigger();
            velocityGain = 1.0f + settings.velSens * (velocity - 1.0f);
            if (accent) velocityGain *= 1.0f + 0.3f * settings.accentAmount;   // l'accent joue aussi plus fort
            accentOn     = accent;
        }

        lockIndex  = plockIndex;
        lockCutoff = plockCutoff;
        gateOn    = true;
        hasPlayed = true;
    }

    void noteOff() noexcept
    {
        gateOn = false;
        ampEnv.noteOff();
    }

    /** Relâche tout de suite (fin de transport) en passant par le Release : pas de clic. */
    void allNotesOff() noexcept { noteOff(); }

    bool isActive() const noexcept { return ampEnv.isActive(); }
    bool isGateOn() const noexcept { return gateOn; }
    int  getCurrentNote() const noexcept { return currentNote; }
    float getModEnvValue() const noexcept { return modEnv.getValue(); }
    float getAmpEnvValue() const noexcept { return ampEnv.getLevel(); }
    float getCurrentIndex() const noexcept { return sIndexOut; }
    float getCurrentRatio() const noexcept { return FastMath::exp2 (sRatioLog); }
    float getCurrentFrequency() const noexcept { return lastFreqA; }

    VoiceSettings settings;

    //==============================================================================
    /** Met à jour les temps d'enveloppe (à appeler une fois par sous-bloc). */
    void updateEnvelopeTimes() noexcept
    {
        ampEnv.setTimes (settings.attack, settings.decay, settings.sustain, settings.release);
        modEnv.setTimes (settings.modAttack, settings.modDecay, settings.modCurve);
        glideCoeff = FastMath::onePoleCoeff (0.25f * (slideGlide && settings.glideMs < 1.0f ? 60.0f : settings.glideMs), baseRate);
    }

    /**
        Calcule un échantillon de base : écrit 'osFactor' échantillons suréchantillonnés dans 'out'.
        Renvoie false si la voix est silencieuse (rien à calculer : 'out' est rempli de zéros).
    */
    inline void renderBaseSample (const VoiceMods& m, float* out) noexcept
    {
        const auto& s = settings;

        // --- contrôles (fréquence de base)
        const float amp = ampEnv.next();
        const float mod = modEnv.next();

        if (glideActive || slideGlide)
            pitch += (targetPitch - pitch) * glideCoeff;
        else
            pitch = targetPitch;

        const float accentBoost = accentOn ? 1.0f + s.accentAmount : 1.0f;

        // cibles des paramètres continus
        const float ratioBase = s.freeRatio ? s.freeRatioValue
                                            : Choices::ratioValues[std::clamp (s.ratioIndex + m.ratioSteps, 0, Choices::numRatios - 1)];
        const float tRatioLog = std::log2 (std::max (0.01f, ratioBase)) + m.ratioOct + s.fineB * (1.0f / 1200.0f);
        const float tPitchOff = s.coarseA + s.fineA * 0.01f + m.pitch;
        const float baseIndex = std::isnan (lockIndex) ? s.index : lockIndex;
        const float tIndex    = baseIndex + m.index;
        const float baseCut   = std::isnan (lockCutoff) ? s.cutoff : lockCutoff;
        const float tCutSemis = 12.0f * std::log2 (std::max (20.0f, baseCut) * (1.0f / 440.0f)) + 69.0f
                              + s.keyTrack * ((float) currentNote - 60.0f) + m.cutoff;
        const float tCross    = s.fmMode == 1 ? 1.0f : 0.0f;
        const float tLp = s.filterType == 0 ? 1.0f : 0.0f, tBp = s.filterType == 1 ? 1.0f : 0.0f, tHp = s.filterType == 2 ? 1.0f : 0.0f;
        const float t24 = s.filterSlope == 1 ? 1.0f : 0.0f;

        if (! initialised)
        {
            sRatioLog = tRatioLog; sPitchOff = tPitchOff; sIndex = tIndex; sFeedback = s.feedback + m.feedback;
            sShapeA = s.shapeA + m.shapeA; sShapeB = s.shapeB + m.shapeB; sMix = s.mix + m.mix;
            sCut = tCutSemis; sRes = s.resonance + m.resonance; sDrive = s.filterDrive + m.drive; sFx = s.fxDrive;
            sCross = tCross; wLp = tLp; wBp = tBp; wHp = tHp; w24 = t24;
            sFiltEnv = s.filterEnv; sModIdx = s.modToIndex; sModCut = s.modToCutoff; sModPitch = s.modToPitch;
            initialised = true;
        }

        const float c = smoothCoeff, f = fastCoeff;
        sRatioLog += (tRatioLog - sRatioLog) * f;
        sPitchOff += (tPitchOff - sPitchOff) * f;
        sIndex    += (tIndex - sIndex) * c;
        sFeedback += (s.feedback + m.feedback - sFeedback) * c;
        sShapeA   += (s.shapeA + m.shapeA - sShapeA) * c;
        sShapeB   += (s.shapeB + m.shapeB - sShapeB) * c;
        sMix      += (s.mix + m.mix - sMix) * c;
        sCut      += (tCutSemis - sCut) * c;
        sRes      += (s.resonance + m.resonance - sRes) * c;
        sDrive    += (s.filterDrive + m.drive - sDrive) * c;
        sFx       += (s.fxDrive - sFx) * c;
        sCross    += (tCross - sCross) * f;
        wLp += (tLp - wLp) * c; wBp += (tBp - wBp) * c; wHp += (tHp - wHp) * c; w24 += (t24 - w24) * c;
        sFiltEnv  += (s.filterEnv - sFiltEnv) * c;
        sModIdx   += (s.modToIndex - sModIdx) * c;
        sModCut   += (s.modToCutoff - sModCut) * c;
        sModPitch += (s.modToPitch - sModPitch) * c;

        const float ampTarget = amp * velocityGain * std::max (0.0f, m.ampGain);

        if (! ampEnv.isActive() && lastAmp < 1.0e-6f && ampTarget < 1.0e-6f)
        {
            for (int i = 0; i < osFactor; ++i) out[i] = 0.0f;
            lastAmp = 0.0f;
            prevA = prevB1 = prevB2 = 0.0f;
            svf1.reset(); svf2.reset();
            return;
        }

        // hauteur finale (enveloppe Mod comprise) -> incréments de phase
        const float pitchA   = pitch + sPitchOff + mod * sModPitch * accentBoost;
        const float freqA    = 440.0f * FastMath::exp2 ((pitchA - 69.0f) * (1.0f / 12.0f));
        lastFreqA = freqA;
        const float nyq      = (float) (0.45 * osRate);
        const float incA     = std::min (freqA, nyq) / (float) osRate;
        const float incB     = std::min (freqA * FastMath::exp2 (sRatioLog), nyq) / (float) osRate;

        const float index    = std::clamp (sIndex + mod * sModIdx * accentBoost, 0.0f, 12.0f);
        sIndexOut = index;
        const float idxCyc   = index * (1.0f / FastMath::twoPi);
        const float fb       = std::clamp (sFeedback, 0.0f, 1.0f);
        const float fbCyc    = fb * fb * 0.9f;
        const float shA      = std::clamp (sShapeA, 0.0f, 1.0f);
        const float shB      = std::clamp (sShapeB, 0.0f, 1.0f);
        const float mix      = std::clamp (sMix, 0.0f, 1.0f);
        const float cross    = sCross;

        // filtre
        const float cutSemis = sCut + mod * sModCut * accentBoost + amp * sFiltEnv;
        const float fc       = std::clamp (440.0f * FastMath::exp2 ((cutSemis - 69.0f) * (1.0f / 12.0f)), 16.0f, (float) (0.45 * osRate));
        const float g        = std::tan (3.14159265f * fc / (float) osRate);
        const float res      = std::clamp (sRes, 0.0f, 1.0f);
        const float k        = 2.0f * (1.0f - 0.985f * res);
        const float drive    = std::clamp (sDrive, 0.0f, 1.0f);
        const float dGain    = 1.0f + 7.0f * drive;
        const float dNorm    = 1.0f / FastMath::tanh (dGain);
        const float dWet     = std::min (1.0f, drive * 8.0f);
        const float fx       = std::clamp (sFx, 0.0f, 1.0f);
        const float fxGain   = 1.0f + 15.0f * fx * fx;   // courbe progressive : réglage fin en bas de course
        const float fxNorm   = 1.0f / std::sqrt (fxGain);
        const float fxWet    = std::min (1.0f, fx * 10.0f);

        // mise en forme : 0 = sinus, 0.5 = triangle, 1 = carré (arrondi)
        auto shapeCoeffs = [] (float sh, float& wSin, float& wTri, float& wSq, float& sqK)
        {
            if (sh < 0.5f) { const float u = sh * 2.0f; wSin = 1.0f - u; wTri = u; wSq = 0.0f; sqK = 2.0f; }
            else           { const float u = sh * 2.0f - 1.0f; wSin = 0.0f; wTri = 1.0f - u; wSq = u; sqK = 2.0f + 10.0f * u; }
        };
        float aS, aT, aQ, aK, bS, bT, bQ, bK;
        shapeCoeffs (shA, aS, aT, aQ, aK);
        shapeCoeffs (shB, bS, bT, bQ, bK);
        const float aKn = 1.0f / FastMath::tanh (aK), bKn = 1.0f / FastMath::tanh (bK);

        const auto& sine = FastMath::sine();
        auto osc = [&sine] (float ph, float wS, float wT, float wQ, float kq, float kn) noexcept
        {
            const float sn = sine (ph);
            float v = wS * sn;
            if (wT > 0.0f)
            {
                float p = ph + 0.25f;
                p -= std::floor (p);
                v += wT * (1.0f - 4.0f * std::abs (p - 0.5f));
            }
            if (wQ > 0.0f)
                v += wQ * FastMath::tanh (kq * sn) * kn;
            return v;
        };

        const float ampStep = (ampTarget - lastAmp) / (float) osFactor;
        float a = lastAmp;

        for (int i = 0; i < osFactor; ++i)
        {
            // --- FM : B module A (et A module B en mode croisé), feedback de B sur lui-même
            const float fbTerm = fbCyc * 0.5f * (prevB1 + prevB2);
            const float outB   = osc (phB + fbTerm + cross * idxCyc * prevA, bS, bT, bQ, bK, bKn);
            const float outA   = osc (phA + idxCyc * outB, aS, aT, aQ, aK, aKn);
            prevB2 = prevB1; prevB1 = outB; prevA = outA;

            phA += incA; phA -= (float) (int) phA;
            phB += incB; phB -= (float) (int) phB;

            float x = outA + (outB - outA) * mix;

            // --- saturation avant filtre
            if (dWet > 0.0f)
                x += (FastMath::tanh (x * dGain) * dNorm - x) * dWet;

            // --- filtre 12 dB, puis 2e étage pour 24 dB
            float lp, bp, hp;
            svf1.process (x, g, k, lp, bp, hp);
            const float y1 = wLp * lp + wBp * (bp * k) + wHp * hp;
            // le 2e étage tourne toujours : passer en 24 dB ne part jamais d'un état vide (pas de clic)
            svf2.process (y1, g, 1.41421356f, lp, bp, hp);
            const float y2 = wLp * lp + wBp * (bp * 1.41421356f) + wHp * hp;
            float y = y1 + (y2 - y1) * w24;

            // --- ampli (interpolé entre deux échantillons de base) puis Drive
            a += ampStep;
            y *= a;

            if (fxWet > 0.0f)
                y += (FastMath::tanh (y * fxGain) * fxNorm - y) * fxWet;

            out[i] = y;
        }

        lastAmp = ampTarget;

        if (! std::isfinite (out[osFactor - 1]))   // garde-fou : jamais de NaN en sortie
        {
            svf1.reset(); svf2.reset();
            prevA = prevB1 = prevB2 = 0.0f;
            for (int i = 0; i < osFactor; ++i) out[i] = 0.0f;
        }
    }

private:
    double baseRate = 44100.0, osRate = 88200.0;
    int    osFactor = 2;

    AdsrEnvelope ampEnv;
    ModEnvelope  modEnv;
    Svf svf1, svf2;

    float phA = 0.0f, phB = 0.0f, prevA = 0.0f, prevB1 = 0.0f, prevB2 = 0.0f;
    float pitch = 60.0f, targetPitch = 60.0f, glideCoeff = 1.0f;
    int   currentNote = 60;
    bool  glideActive = false, slideGlide = false, gateOn = false, hasPlayed = false, accentOn = false, initialised = false;
    float velocityGain = 1.0f, lastAmp = 0.0f;
    float lockIndex  = std::numeric_limits<float>::quiet_NaN();
    float lockCutoff = std::numeric_limits<float>::quiet_NaN();

    float smoothCoeff = 0.01f, fastCoeff = 0.1f;
    float sRatioLog = 0.0f, sPitchOff = 0.0f, sIndex = 0.0f, sFeedback = 0.0f, sShapeA = 0.0f, sShapeB = 0.0f, sMix = 0.0f,
          sCut = 100.0f, sRes = 0.0f, sDrive = 0.0f, sFx = 0.0f, sCross = 0.0f, wLp = 1.0f, wBp = 0.0f, wHp = 0.0f, w24 = 0.0f;
    float sFiltEnv = 0.0f, sModIdx = 0.0f, sModCut = 0.0f, sModPitch = 0.0f;
    float sIndexOut = 0.0f;
    float lastFreqA = 130.8f;
};
