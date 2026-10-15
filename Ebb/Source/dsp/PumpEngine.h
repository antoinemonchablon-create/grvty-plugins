// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "Curves.h"
#include "Shape.h"
#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>

//==============================================================================
/**
    Moteur d'Ebb : calcule, échantillon par échantillon, le gain à appliquer au signal.

      gain = 1 - Amount * courbe(position)        (toujours entre 0 et 1 : Ebb ne monte jamais le niveau)

    Trois façons de lancer la courbe :
      - SYNC  : calée sur le tempo et la position d'Ableton, alignée sur les mesures. Transport arrêté :
                elle continue au dernier tempo connu (pour régler sur un son tenu).
      - MIDI  : chaque note MIDI reçue relance la courbe, à l'échantillon exact de la note.
      - AUDIO : un kick sur l'entrée sidechain relance la courbe (détecteur filtré dans les graves).
    En MIDI et AUDIO, la courbe dure la valeur de Rate au tempo du projet, puis le niveau revient en douceur.

    Anti-clic : le gain ne peut pas bouger de plus de 1/(1 ms) par échantillon (il lui faut au moins 1 ms pour aller
    de 100 % à 0 %), quoi qu'il arrive : début de courbe, relance en plein milieu, changement de courbe, de Rate,
    d'Amount, saut de position (boucle), Play / Stop. Une courbe relancée part donc toujours du gain actuel.

    Aucune allocation, aucun verrou : tout est en tableaux fixes, la courbe est lue dans une table précalculée.
*/
namespace Ebb
{
    enum TriggerMode { triggerSync = 0, triggerMidi, triggerAudio, numTriggerModes };

    struct Settings
    {
        float amount = 0.8f;            // 0..1
        int   curve = curveRipple;
        int   rate = rateQuarter;
        int   trigger = triggerSync;
        float nudgeMs = 0.0f;           // SYNC : décalage de la courbe (+ = plus tard)
        float thresholdDb = -20.0f;     // AUDIO : seuil du détecteur

        // phase 2
        bool  custom = false;           // courbe perso (dessinée) au lieu de la courbe d'usine
        const float* customTable = nullptr;   // table de baisse de la courbe perso (CurveTables::size + 1 valeurs)
        uint32_t customVersion = 0;     // change à chaque nouvelle courbe perso
        float tension = 0.0f;           // -1 (molle) ... +1 (sèche)
        float smooth = 0.0f;            // lissage 0..1
    };

    /** Position de l'hôte au PREMIER échantillon du bloc. */
    struct Transport
    {
        bool   playing = false;
        bool   hasPosition = false;
        double ppq = 0.0;               // en noires
        double bpm = 120.0;
        bool   hasBpm = false;
        double barBeats = 4.0;          // durée d'une mesure en noires (4/4 -> 4)
        bool   hasBarStart = false;
        double barStartPpq = 0.0;       // début de la mesure en cours (grille des mesures)
    };

    //==============================================================================
    /**
        Détecteur de kick pour le mode AUDIO. Il réagit à une ATTAQUE dans les graves, pas à un niveau :
          1. filtre passe-bas 300 Hz (Butterworth, 12 dB/oct) : un charley ou un clap ne déclenchent rien ;
          2. enveloppe rapide (attaque 0,2 ms, retour 20 ms) ;
          3. référence = le plus haut niveau atteint entre 25 ms et 2 ms avant (par tranches de 1 ms) :
             elle couvre plusieurs périodes d'une basse, la queue du kick précédent, mais pas l'attaque en cours ;
          4. déclenchement quand l'enveloppe passe le seuil, dépasse la référence de 3 dB ET a monté d'au moins
             70 % du seuil. Une basse tenue ou liée, ou un son qui monte doucement (fondu), ne déclenche pas ;
          5. réarmement quand le niveau est retombé à 60 % du pic (ou sous le seuil), et au moins 35 ms après
             (pas de double déclenchement sur le clic puis le corps du kick).
        Coût : quelques opérations par échantillon (la référence n'est recalculée qu'une fois par milliseconde).
    */
    class KickDetector
    {
    public:
        void prepare (double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            const double w0 = 2.0 * 3.14159265358979323846 * 300.0 / sr;
            const double cs = std::cos (w0), sn = std::sin (w0);
            const double alpha = sn / (2.0 * 0.70710678118654752);
            const double a0 = 1.0 + alpha;
            b0 = (float) ((1.0 - cs) * 0.5 / a0);
            b1 = (float) ((1.0 - cs) / a0);
            b2 = b0;
            a1 = (float) (-2.0 * cs / a0);
            a2 = (float) ((1.0 - alpha) / a0);

            auto coef = [this] (double seconds) { return (float) (1.0 - std::exp (-1.0 / (seconds * sr))); };
            fastAtt = coef (0.0002);
            fastRel = coef (0.020);
            holdoff = (int) std::lround (0.035 * sr);
            bucketLen = (int) std::lround (0.001 * sr);
            if (bucketLen < 1) bucketLen = 1;
            reset();
        }

        void reset() noexcept
        {
            z1 = z2 = 0.0f;
            fast = peak = 0.0f;
            armed = true;
            sinceTrigger = 1 << 30;
            buckets.fill (0.0f);
            bucketPos = 0;
            bucketCount = 0;
            bucketMax = 0.0f;
            reference = 0.0f;
        }

        /** Un échantillon (mono). Renvoie true si un kick commence à cet échantillon. */
        bool process (float x, float thresholdLin) noexcept
        {
            // passe-bas (forme transposée directe II)
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            if (std::abs (z1) < 1.0e-15f) z1 = 0.0f;
            if (std::abs (z2) < 1.0e-15f) z2 = 0.0f;

            const float a = std::abs (y);
            fast += (a - fast) * (a > fast ? fastAtt : fastRel);
            if (fast < 1.0e-9f) fast = 0.0f;

            // tranches de 1 ms : référence = maximum des tranches âgées de 2 à 24 ms
            if (fast > bucketMax) bucketMax = fast;
            if (++bucketCount >= bucketLen)
            {
                buckets[(size_t) bucketPos] = bucketMax;
                bucketPos = (bucketPos + 1) % numBuckets;
                bucketMax = 0.0f;
                bucketCount = 0;
                float m = 0.0f;
                for (int age = 2; age < numBuckets; ++age)
                {
                    int idx = bucketPos - 1 - age;
                    if (idx < 0) idx += numBuckets;
                    if (buckets[(size_t) idx] > m) m = buckets[(size_t) idx];
                }
                reference = m;
            }

            if (sinceTrigger < (1 << 30)) ++sinceTrigger;

            if (! armed)
            {
                if (fast > peak) peak = fast;
                if (sinceTrigger >= holdoff && (fast < peak * rearmFraction || fast < thresholdLin))
                    armed = true;
                return false;
            }

            if (fast > thresholdLin && fast > reference * onsetRatio && fast - reference > riseFraction * thresholdLin
                && sinceTrigger >= holdoff)
            {
                armed = false;
                peak = fast;
                sinceTrigger = 0;
                return true;
            }
            return false;
        }

        /** Niveau des graves du sidechain (enveloppe rapide, linéaire). */
        float getLevel() const noexcept { return fast; }

        static constexpr float onsetRatio = 1.4125f;    // +3 dB au-dessus de la référence
        static constexpr float riseFraction = 0.7f;     // montée minimale, en fraction du seuil
        static constexpr float rearmFraction = 0.6f;    // réarmement à 60 % du pic

    private:
        static constexpr int numBuckets = 25;
        double sr = 48000.0;
        float b0 = 0, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        float fastAtt = 0, fastRel = 0;
        float fast = 0, peak = 0;
        bool armed = true;
        int holdoff = 1680, sinceTrigger = 1 << 30;
        std::array<float, numBuckets> buckets {};
        int bucketPos = 0, bucketCount = 0, bucketLen = 48;
        float bucketMax = 0.0f, reference = 0.0f;
    };

    //==============================================================================
    class PumpEngine
    {
    public:
        static constexpr int logSize = 4096;

        void prepare (double sampleRate)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            slewStep = (float) (1.0 / (0.001 * sr));                   // 0 -> 1 en 1 ms au plus vite
            tailCoef = (float) std::exp (-1.0 / (0.015 * sr));         // retour en douceur après une courbe déclenchée
            amountRampLen = imax (1, (int) std::lround (0.030 * sr)); // Amount lissé sur 30 ms
            detector.prepare (sr);
            (void) CurveTables::get();
            reset();
        }

        /** Remet tout au repos (gain 1, aucune courbe en cours). */
        void reset() noexcept
        {
            gain = 1.0f;
            phase = -1.0;
            freePhase = 0.0;
            tail = 0.0f;
            lastKey = INT64_MIN;
            amountCur = amountTarget = -1.0f;
            amountLeft = 0;
            tensionCur = tensionTarget = 0.0f;
            tensionLeft = 0;
            lastBase = nullptr;
            lastH = -1;
            detector.reset();
            samplePos = 0;
            logCount = 0;
            wasSyncPlaying = false;
            lastMode = -1;
        }

        /**
            Calcule les gains de 'numSamples' échantillons.
              midiOffsets : positions (triées) des notes MIDI reçues dans ce bloc ;
              sc / scChannels : entrée sidechain (peut être absente : scChannels = 0).
        */
        void process (const Settings& s, const Transport& t, int numSamples,
                      const int* midiOffsets, int numMidi,
                      const float* const* sc, int scChannels,
                      float* gains) noexcept
        {
            const int curve = s.curve < 0 || s.curve >= numCurves ? curveRipple : s.curve;
            const int mode = s.trigger < 0 || s.trigger >= numTriggerModes ? triggerSync : s.trigger;
            const float* table = activeTable (s, curve);

            if (t.hasBpm && t.bpm > 1.0) bpm = t.bpm;
            const double barLen = t.barBeats > 0.0 ? t.barBeats : 4.0;
            const double cycleQ = rateBeats (s.rate, barLen);
            const double qps = bpm / (60.0 * sr);                     // noires par échantillon
            const double dPhase = qps / cycleQ;
            const double maxCycIdx = std::ceil (barLen / cycleQ - 1.0e-9) - 1.0;

            if (mode != lastMode)
            {
                // nouveau mode : on repart sans courbe en cours (le gain glisse vers sa nouvelle valeur)
                if (mode != triggerSync) { phase = -1.0; tail = 0.0f; }
                lastKey = INT64_MIN;
                lastMode = mode;
            }

            // Amount : rampe linéaire vers la nouvelle valeur (30 ms)
            const float amt = s.amount < 0.0f ? 0.0f : (s.amount > 1.0f ? 1.0f : s.amount);
            if (amountCur < 0.0f) { amountCur = amountTarget = amt; amountLeft = 0; }
            else if (std::abs (amt - amountTarget) > 1.0e-7f)
            {
                amountTarget = amt;
                amountLeft = amountRampLen;
                amountStep = (amountTarget - amountCur) / (float) amountLeft;
            }

            // Tension : rampe linéaire (30 ms) ; à 0 et immobile, la courbe est lue telle quelle (aucun calcul en plus)
            const float ten = s.tension < -1.0f ? -1.0f : (s.tension > 1.0f ? 1.0f : s.tension);
            if (std::abs (ten - tensionTarget) > 1.0e-7f)
            {
                tensionTarget = ten;
                tensionLeft = amountRampLen;
                tensionStep = (tensionTarget - tensionCur) / (float) tensionLeft;
            }
            float tensionK = ShapeMath::tensionK (tensionCur);

            const float thrLin = std::pow (10.0f, s.thresholdDb / 20.0f);
            const bool syncPlaying = mode == triggerSync && t.playing && t.hasPosition;
            const double nudgeQ = (double) s.nudgeMs * 0.001 * bpm / 60.0;
            const double barOrigin = t.hasBarStart ? t.barStartPpq : 0.0;
            int nextMidi = 0;

            if (mode == triggerSync && ! syncPlaying && wasSyncPlaying)
                freePhase = phase >= 0.0 ? phase : 0.0;               // Stop : on continue là où on en était
            wasSyncPlaying = syncPlaying;

            for (int n = 0; n < numSamples; ++n)
            {
                // --- sidechain (le détecteur tourne toujours : il alimente aussi l'affichage du niveau)
                float scIn = 0.0f;
                if (scChannels > 0)
                {
                    for (int c = 0; c < scChannels; ++c) scIn += sc[c][n];
                    scIn /= (float) scChannels;
                }
                const bool kick = detector.process (scIn, thrLin);

                float r;
                if (mode == triggerSync)
                {
                    if (syncPlaying)
                    {
                        // + 1e-7 noire (< 0,003 échantillon) : un temps qui tombe sur un échantillon, à l'arrondi près, y commence
                        const double pos = t.ppq + (double) n * qps - nudgeQ + 1.0e-7;
                        const double rel = pos - barOrigin;
                        const double barIdx = std::floor (rel / barLen);
                        const double inBar = rel - barIdx * barLen;
                        // les courbes repartent à chaque mesure ; si la durée ne divise pas la mesure (1/8 pointé),
                        // la dernière courbe de la mesure est coupée par la première de la mesure suivante
                        double cycIdx = std::floor (inBar / cycleQ);
                        if (cycIdx > maxCycIdx) cycIdx = maxCycIdx;
                        double within = inBar - cycIdx * cycleQ;
                        if (within < 0.0) within = 0.0;
                        phase = within / cycleQ;
                        if (phase >= 1.0) phase = 0.999999;
                        // numéro de mesure absolu (la grille de l'hôte est donnée par rapport à la mesure en cours)
                        const int64_t key = ((int64_t) std::llround (barOrigin / barLen) + (int64_t) barIdx) * 4096 + (int64_t) cycIdx;
                        if (key != lastKey)
                        {
                            if (within < qps) logTrigger (samplePos + n);  // une courbe commence à cet échantillon
                            lastKey = key;
                        }
                    }
                    else
                    {
                        freePhase += dPhase;
                        if (freePhase >= 1.0) { freePhase -= std::floor (freePhase); logTrigger (samplePos + n); }
                        phase = freePhase;
                        lastKey = INT64_MIN;
                    }
                    r = CurveTables::lookupTable (table, (float) phase);
                }
                else
                {
                    bool trig = false;
                    if (mode == triggerMidi)
                    {
                        while (nextMidi < numMidi && midiOffsets[nextMidi] <= n)
                        {
                            trig = true;
                            ++nextMidi;
                        }
                    }
                    else
                        trig = kick;

                    if (trig) { phase = 0.0; logTrigger (samplePos + n); }

                    if (phase >= 0.0)
                    {
                        r = CurveTables::lookupTable (table, (float) phase);
                        phase += dPhase;
                        if (phase >= 1.0) { phase = -1.0; tail = table[CurveTables::size]; }
                    }
                    else
                    {
                        tail *= tailCoef;
                        if (tail < 1.0e-6f) tail = 0.0f;
                        r = tail;
                    }
                }

                // --- Tension
                if (tensionLeft > 0)
                {
                    tensionCur += tensionStep;
                    if (--tensionLeft == 0) tensionCur = tensionTarget;
                    tensionK = ShapeMath::tensionK (tensionCur);
                }
                if (tensionLeft > 0 || tensionCur > 1.0e-6f || tensionCur < -1.0e-6f)
                    r = ShapeMath::applyTension (r, tensionK);

                // --- Amount lissé
                if (amountLeft > 0)
                {
                    amountCur += amountStep;
                    if (--amountLeft == 0) amountCur = amountTarget;
                }

                // --- gain, limité en vitesse (anti-clic)
                const float target = 1.0f - amountCur * r;
                float d = target - gain;
                if (d > slewStep) d = slewStep;
                else if (d < -slewStep) d = -slewStep;
                gain += d;
                if (gain > 1.0f) gain = 1.0f;
                if (gain < 0.0f) gain = 0.0f;
                gains[n] = gain;
            }

            samplePos += numSamples;
            uiGain.store (gain, std::memory_order_relaxed);
            uiPhase.store ((float) phase, std::memory_order_relaxed);
            uiLevel.store (detector.getLevel(), std::memory_order_relaxed);
        }

        /** Saute 'numSamples' échantillons sans rien calculer (bypass) : le gain repart de 1 ensuite. */
        void skip (int numSamples) noexcept
        {
            samplePos += numSamples;
            gain = 1.0f;
            phase = -1.0;
            tail = 0.0f;
            lastKey = INT64_MIN;
            uiGain.store (1.0f, std::memory_order_relaxed);
        }

        float getGain() const noexcept { return gain; }
        double getBpm() const noexcept { return bpm; }

        // --- affichage (lus par l'interface)
        std::atomic<float> uiGain { 1.0f }, uiPhase { -1.0f }, uiLevel { 0.0f };
        std::atomic<uint32_t> uiTriggers { 0 };

        // --- banc de tests : position (en échantillons depuis prepare) des derniers débuts de courbe
        int64_t getTriggerCount() const noexcept { return logCount; }
        int64_t getTrigger (int64_t i) const noexcept { return log[(size_t) (i % logSize)]; }
        int64_t getSamplePos() const noexcept { return samplePos; }

        /** La table effectivement lue (courbe d'usine ou perso, lissée) : pour l'affichage et le banc de tests. */
        const float* getActiveTable() const noexcept { return lastActive; }

    private:
        static int imax (int a, int b) { return a > b ? a : b; }

        /** Courbe d'usine ou perso, puis lissage (recalculé seulement quand la courbe ou le lissage changent). */
        const float* activeTable (const Settings& s, int curve) noexcept
        {
            const float* base = (s.custom && s.customTable != nullptr) ? s.customTable : CurveTables::get().table (curve);
            const uint32_t version = (s.custom && s.customTable != nullptr) ? s.customVersion : 0u;
            const int h = ShapeMath::smoothRadius (s.smooth, CurveTables::size);
            if (base != lastBase || version != lastVersion || h != lastH)
            {
                lastBase = base;
                lastVersion = version;
                lastH = h;
                if (h <= 0)
                    lastActive = base;
                else
                {
                    ShapeMath::smoothCircular (base, smoothTable.data(), smoothTmp.data(), CurveTables::size, h);
                    lastActive = smoothTable.data();
                }
            }
            return lastActive;
        }

        void logTrigger (int64_t at) noexcept
        {
            log[(size_t) (logCount % logSize)] = at;
            ++logCount;
            uiTriggers.fetch_add (1, std::memory_order_relaxed);
        }

        double sr = 48000.0, bpm = 120.0;
        float slewStep = 1.0f / 48.0f, tailCoef = 0.99f;
        float gain = 1.0f, tail = 0.0f;
        double phase = -1.0, freePhase = 0.0;
        int64_t lastKey = INT64_MIN;
        float amountCur = -1.0f, amountTarget = -1.0f, amountStep = 0.0f;
        int amountLeft = 0, amountRampLen = 1440;
        float tensionCur = 0.0f, tensionTarget = 0.0f, tensionStep = 0.0f;
        int tensionLeft = 0;
        const float* lastBase = nullptr;
        const float* lastActive = nullptr;
        uint32_t lastVersion = 0;
        int lastH = -1;
        std::array<float, CurveTables::size + 1> smoothTable {}, smoothTmp {};
        bool wasSyncPlaying = false;
        int lastMode = -1;
        KickDetector detector;
        int64_t samplePos = 0, logCount = 0;
        std::array<int64_t, logSize> log {};
    };
}
