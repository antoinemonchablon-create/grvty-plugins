// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "Pattern.h"
#include "../Params.h"

//==============================================================================
/**
    Moteur du séquenceur (thread audio, aucune allocation).

    Toutes les positions sont calculées directement depuis la position du transport
    d'Ableton (en noires) : le pas n commence exactement à n × durée_du_pas (+ swing).
    Rien n'est accumulé d'un bloc à l'autre, donc aucune dérive possible, même sur
    des heures, et le séquenceur suit les boucles et les changements de tempo.
*/
class Sequencer
{
public:
    struct Settings
    {
        int    length = 16, rate = 1, scale = 0, root = 0, octave = 0;
        float  swing = 0.5f;
        int    laneLength = 16;
        float  mutateAmount = 0.0f;
        int    mutateBars = 4;
        int    transpose = 0;      // demi-tons (note MIDI reçue - C3)
    };

    struct Event
    {
        enum Type { noteOff = 0, noteOn = 1, lane = 2 };
        int   sample = 0;
        Type  type = noteOn;
        int   note = 60;
        float velocity = 0.8f;
        bool  accent = false, slide = false;
        float lockIndex = 0.0f, lockCutoff = 0.0f;
        float laneValue = 0.0f;
        int   step = 0;
        int64_t globalStep = 0;
    };

    static constexpr int maxEvents = 96;

    void reset() noexcept
    {
        wasPlaying = false;
        numPending = 0;
        lastFired = std::numeric_limits<int64_t>::min();
        awaitingSlide = false;
        noteSounding = false;
        cachedVariation = -1;
    }

    /**
        Calcule les événements du bloc.
        @param playing      le transport joue
        @param ppqStart     position (en noires) du premier échantillon du bloc
        @param bpm          tempo
        @param barQuarters  durée d'une mesure en noires (4 en 4/4)
    */
    int process (const Pattern& pattern, const Settings& st, bool playing, double ppqStart, double bpm,
                 double barQuarters, double sampleRate, int numSamples) noexcept
    {
        numEvents = 0;

        if (! playing || bpm <= 0.0)
        {
            if (wasPlaying && noteSounding)
                push ({ 0, Event::noteOff });
            if (wasPlaying)
                reset();
            wasPlaying = false;
            return sortEvents();
        }

        const double qps    = bpm / (60.0 * sampleRate);   // noires par échantillon
        const double ppqEnd = ppqStart + qps * numSamples;

        // --- saut de position (boucle Ableton, déplacement du curseur, démarrage)
        if (! wasPlaying || std::abs (ppqStart - expectedPpq) > qps * 0.5)
        {
            if (wasPlaying && noteSounding)
                push ({ 0, Event::noteOff });
            numPending = 0;
            awaitingSlide = false;
            noteSounding = false;
            lastFired = std::numeric_limits<int64_t>::min();
        }
        wasPlaying  = true;
        expectedPpq = ppqEnd;

        auto toSample = [&] (double ppq, bool& inBlock) -> int
        {
            const double x = (ppq - ppqStart) / qps;
            int off = (int) std::ceil (x - 1.0e-6);
            if (off < 0 && x > -1.0) off = 0;
            inBlock = off >= 0 && off < numSamples;
            return off;
        };

        // --- événements en attente (répétitions de ratchet, fins de notes)
        {
            int keep = 0;
            for (int i = 0; i < numPending; ++i)
            {
                bool in = false;
                auto e = pending[(size_t) i];
                const int off = toSample (pendingPpq[(size_t) i], in);
                if (in || off < 0)
                {
                    e.sample = std::max (0, off);
                    push (e);
                }
                else
                {
                    pending[(size_t) keep] = pending[(size_t) i];
                    pendingPpq[(size_t) keep] = pendingPpq[(size_t) i];
                    ++keep;
                }
            }
            numPending = keep;
        }

        // --- nouveaux pas
        const double stepQ  = Choices::seqRateQuarters[juce::jlimit (0, 5, st.rate)];
        const double swingQ = (juce::jlimit (0.5f, 0.75f, st.swing) - 0.5) * 2.0 * stepQ;
        const int    len    = juce::jlimit (1, Pattern::maxSteps, st.length);
        const int    lanLen = juce::jlimit (1, Pattern::maxSteps, st.laneLength);

        auto stepStart = [&] (int64_t n) { return (double) n * stepQ + ((n & 1) ? swingQ : 0.0); };

        // changement de cadence (preset, réglage RATE) : le numéro du dernier pas joué est converti dans la
        // nouvelle grille. Sans ça, passer de 1/32 à 1/16 attendait que le compteur rattrape l'ancien
        // numéro : silence d'autant plus long qu'on est loin dans le morceau.
        if (stepQ != lastStepQ)
        {
            if (lastFired != std::numeric_limits<int64_t>::min())
                lastFired = (int64_t) std::floor (lastFiredPpq / stepQ + 1.0e-9);
            lastStepQ = stepQ;
        }

        const int64_t nFirst = (int64_t) std::floor (ppqStart / stepQ) - 1;
        const int64_t nLast  = (int64_t) std::floor (ppqEnd / stepQ);

        for (int64_t n = std::max<int64_t> (0, nFirst); n <= nLast; ++n)
        {
            if (n <= lastFired)
                continue;

            const double start = stepStart (n);
            bool in = false;
            const int off = toSample (start, in);
            if (! in)
                continue;

            lastFired = n;
            lastFiredPpq = start;
            const int idx  = (int) (n % len);
            const int lidx = (int) (n % lanLen);
            currentStep.store (idx, std::memory_order_relaxed);
            currentLaneStep.store (lidx, std::memory_order_relaxed);

            // piste Mod : change à chaque pas, avec sa propre longueur (polymétrie)
            Event le { off, Event::lane };
            le.laneValue = pattern.modLane[(size_t) lidx];
            le.step = lidx;
            push (le);

            // Mutate : une mesure sur deux (par groupes de N mesures) joue une variation du motif d'origine
            const Pattern* pat = &pattern;
            if (st.mutateAmount > 0.0f)
            {
                const int64_t bar = (int64_t) std::floor (start / std::max (0.25, barQuarters));
                const int64_t var = bar / std::max (1, st.mutateBars);
                if (var & 1)
                {
                    if (var != cachedVariation || std::abs (st.mutateAmount - cachedAmount) > 1.0e-6f || ! (pattern == lastSource))
                    {
                        PatternOps::mutate (pattern, mutated, len, st.scale, st.mutateAmount, (uint32_t) var * 2654435761u);
                        cachedVariation = var;
                        cachedAmount = st.mutateAmount;
                        lastSource = pattern;
                    }
                    pat = &mutated;
                }
            }

            const auto& s = pat->steps[(size_t) idx];
            const double dur = stepStart (n + 1) - start;

            const bool plays = s.on && (s.probability >= 1.0f
                                        || FastMath::hash01 (FastMath::mix ((uint32_t) n, 0x51A7u + (uint32_t) (n >> 32))) < s.probability);

            if (! plays)
            {
                if (awaitingSlide && noteSounding)
                {
                    push ({ off, Event::noteOff });
                    noteSounding = false;
                }
                awaitingSlide = false;
                continue;
            }

            const int quant = Scales::quantize (s.note, st.scale);
            const int note  = juce::jlimit (0, 127, 48 + st.root + 12 * (s.octave + st.octave) + quant + st.transpose);
            const int reps  = juce::jlimit (1, 4, s.ratchet);
            const double sub = dur / reps;
            const double gateLen = sub * juce::jlimit (0.05f, 1.0f, s.gate);

            for (int r = 0; r < reps; ++r)
            {
                Event on { off, Event::noteOn };
                on.note = note;
                on.velocity = s.velocity * (r == 0 ? 1.0f : 0.85f);
                on.accent = s.accent && r == 0;
                on.slide = (r == 0) && awaitingSlide;
                on.lockIndex  = s.lockIndex;
                on.lockCutoff = s.lockCutoff;
                on.step = idx;
                on.globalStep = n;

                const double t = start + sub * r;
                if (r == 0) push (on);
                else        schedule (on, t);

                const bool lastRep = r == reps - 1;
                if (! (lastRep && s.slide))
                    schedule ({ 0, Event::noteOff }, t + gateLen);
            }

            noteSounding  = true;
            awaitingSlide = s.slide;
        }

        // événements en attente qui tombent déjà dans ce bloc (ratchets rapides, gates courts)
        {
            int keep = 0;
            for (int i = 0; i < numPending; ++i)
            {
                bool in = false;
                const int off = toSample (pendingPpq[(size_t) i], in);
                if (in)
                {
                    auto e = pending[(size_t) i];
                    e.sample = off;
                    push (e);
                }
                else
                {
                    pending[(size_t) keep] = pending[(size_t) i];
                    pendingPpq[(size_t) keep] = pendingPpq[(size_t) i];
                    ++keep;
                }
            }
            numPending = keep;
        }

        return sortEvents();
    }

    const Event& getEvent (int i) const noexcept { return events[(size_t) i]; }
    int getNumEvents() const noexcept { return numEvents; }

    /** Pas en cours de lecture (pour l'interface). */
    std::atomic<int> currentStep { 0 }, currentLaneStep { 0 };

private:
    void push (const Event& e) noexcept
    {
        if (numEvents < maxEvents)
            events[(size_t) numEvents++] = e;
    }

    void schedule (const Event& e, double ppq) noexcept
    {
        if (numPending < maxPending)
        {
            pending[(size_t) numPending] = e;
            pendingPpq[(size_t) numPending] = ppq;
            ++numPending;
        }
    }

    /** Tri stable par position ; à position égale : fin de note, puis piste Mod, puis début de note. */
    int sortEvents() noexcept
    {
        for (int i = 1; i < numEvents; ++i)
        {
            const Event e = events[(size_t) i];
            int j = i - 1;
            while (j >= 0 && (events[(size_t) j].sample > e.sample
                              || (events[(size_t) j].sample == e.sample && order (events[(size_t) j].type) > order (e.type))))
            {
                events[(size_t) j + 1] = events[(size_t) j];
                --j;
            }
            events[(size_t) j + 1] = e;
        }

        // une note est-elle encore tenue à la fin du bloc ?
        return numEvents;
    }

    static int order (Event::Type t) noexcept { return t == Event::noteOff ? 0 : (t == Event::lane ? 1 : 2); }

    static constexpr int maxPending = 32;
    std::array<Event, maxEvents> events {};
    std::array<Event, maxPending> pending {};
    std::array<double, maxPending> pendingPpq {};
    int numEvents = 0, numPending = 0;

    bool wasPlaying = false, awaitingSlide = false, noteSounding = false;
    double lastStepQ = 0.0, lastFiredPpq = 0.0;
    double expectedPpq = 0.0;
    int64_t lastFired = std::numeric_limits<int64_t>::min();

    Pattern mutated, lastSource;
    int64_t cachedVariation = -1;
    float cachedAmount = 0.0f;
};
