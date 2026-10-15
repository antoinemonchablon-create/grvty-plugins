// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "../dsp/FastMath.h"
#include <array>
#include <atomic>
#include <cmath>
#include <limits>

//==============================================================================
/** Un pas du séquenceur. Structure simple (copiable sans allocation) pour le thread audio. */
struct Step
{
    bool  on          = false;
    int   note        = 0;      // demi-tons au-dessus de la tonique (0..11), ramené dans la gamme à la lecture
    int   octave      = 0;      // -2..+2
    float velocity    = 0.8f;   // 0..1
    bool  accent      = false;
    float gate        = 0.5f;   // durée de la note en fraction du pas (0.05..1)
    bool  slide       = false;  // la note glisse vers le pas suivant sans relancer les enveloppes
    float probability = 1.0f;   // 0..1
    int   ratchet     = 1;      // 1..4 répétitions dans le pas
    float lockIndex   = std::numeric_limits<float>::quiet_NaN();   // p-lock : Index FM (NaN = pas de verrou)
    float lockCutoff  = std::numeric_limits<float>::quiet_NaN();   // p-lock : Cutoff en Hz

    bool hasLockIndex() const noexcept  { return ! std::isnan (lockIndex); }
    bool hasLockCutoff() const noexcept { return ! std::isnan (lockCutoff); }

    bool operator== (const Step& o) const noexcept
    {
        auto same = [] (float a, float b) { return (std::isnan (a) && std::isnan (b)) || std::abs (a - b) < 1.0e-5f; };
        return on == o.on && note == o.note && octave == o.octave && same (velocity, o.velocity) && accent == o.accent
            && same (gate, o.gate) && slide == o.slide && same (probability, o.probability) && ratchet == o.ratchet
            && same (lockIndex, o.lockIndex) && same (lockCutoff, o.lockCutoff);
    }
};

/** Motif complet : 32 pas de notes + 32 valeurs de la piste Mod (bipolaires, -1..+1). */
struct Pattern
{
    static constexpr int maxSteps = 32;
    std::array<Step, maxSteps>  steps {};
    std::array<float, maxSteps> modLane {};

    bool operator== (const Pattern& o) const noexcept
    {
        for (int i = 0; i < maxSteps; ++i)
            if (! (steps[(size_t) i] == o.steps[(size_t) i]) || std::abs (modLane[(size_t) i] - o.modLane[(size_t) i]) > 1.0e-5f)
                return false;
        return true;
    }
};

//==============================================================================
namespace Scales
{
    /** Degrés autorisés de chaque gamme (dans l'ordre de Choices::scales). */
    inline const std::array<std::array<bool, 12>, 6>& table()
    {
        static const std::array<std::array<bool, 12>, 6> t {{
            //  C  C#  D  D#  E  F  F#  G  G#  A  A#  B
            { { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 0 } },   // mineur naturel
            { { 1, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0 } },   // phrygien
            { { 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 1, 0 } },   // dorien
            { { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1 } },   // mineur harmonique
            { { 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0 } },   // pentatonique mineure
            { { 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1 } },   // chromatique
        }};
        return t;
    }

    /** Ramène un intervalle (demi-tons, n'importe quelle octave) sur le degré autorisé le plus proche (vers le bas en cas d'égalité). */
    inline int quantize (int semis, int scale) noexcept
    {
        const auto& allowed = table()[(size_t) juce::jlimit (0, 5, scale)];
        const int oct = (int) std::floor (semis / 12.0);
        const int pc  = semis - oct * 12;
        for (int d = 0; d < 12; ++d)
        {
            if (allowed[(size_t) ((pc - d + 12) % 12)]) return semis - d;
            if (allowed[(size_t) ((pc + d) % 12)])      return semis + d;
        }
        return semis;
    }

    /** Liste des degrés autorisés (0..11) d'une gamme. */
    inline int degrees (int scale, std::array<int, 12>& out) noexcept
    {
        int n = 0;
        const auto& allowed = table()[(size_t) juce::jlimit (0, 5, scale)];
        for (int i = 0; i < 12; ++i)
            if (allowed[(size_t) i]) out[(size_t) n++] = i;
        return n;
    }
}

//==============================================================================
/** Opérations sur les motifs (générateurs, Mutate). Sans allocation : utilisables partout. */
namespace PatternOps
{
    /** Rythme euclidien : 'fill' coups répartis au mieux sur 'steps' pas, décalés de 'rotation'. */
    inline void euclidean (Pattern& p, int steps, int fill, int rotation)
    {
        steps = juce::jlimit (1, Pattern::maxSteps, steps);
        fill  = juce::jlimit (0, steps, fill);
        for (int i = 0; i < steps; ++i)
        {
            const int src = ((i - rotation) % steps + steps) % steps;
            // méthode de Bresenham : un coup quand le compteur déborde
            const bool hit = fill > 0 && ((src * fill) % steps) < fill;
            p.steps[(size_t) i].on = hit;
        }
    }

    /** Notes au hasard dans la gamme (les pas actifs gardent leur rythme). */
    inline void randomNotes (Pattern& p, int length, int scale, uint32_t seed, int octaveSpread = 1)
    {
        std::array<int, 12> deg {};
        const int n = Scales::degrees (scale, deg);
        for (int i = 0; i < juce::jlimit (1, Pattern::maxSteps, length); ++i)
        {
            auto& s = p.steps[(size_t) i];
            const float r1 = FastMath::hash01 (FastMath::mix (seed, (uint32_t) i * 3u));
            const float r2 = FastMath::hash01 (FastMath::mix (seed, (uint32_t) i * 3u + 1u));
            s.note = deg[(size_t) juce::jlimit (0, n - 1, (int) (r1 * (float) n))];
            // la tonique revient plus souvent : c'est ce qui rend une boucle « hypnotique »
            if (r2 < 0.3f) s.note = 0;
            s.octave = octaveSpread > 0 && r2 > 0.85f ? 1 : (octaveSpread > 0 && r2 < 0.05f ? -1 : 0);
        }
    }

    /** Décale tout le motif d'un pas (direction -1 = gauche, +1 = droite) sur 'length' pas. */
    inline void shift (Pattern& p, int length, int direction)
    {
        length = juce::jlimit (1, Pattern::maxSteps, length);
        Pattern copy = p;
        for (int i = 0; i < length; ++i)
        {
            const int src = ((i - direction) % length + length) % length;
            p.steps[(size_t) i]   = copy.steps[(size_t) src];
            p.modLane[(size_t) i] = copy.modLane[(size_t) src];
        }
    }

    /**
        Variation « Mutate » : quelques pas changent légèrement (note voisine dans la gamme,
        octave, note fantôme, ratchet). Calculée depuis le motif D'ORIGINE à chaque fois :
        le motif ne dérive jamais, il revient toujours à sa forme de départ.
    */
    inline void mutate (const Pattern& original, Pattern& out, int length, int scale, float amount, uint32_t seed)
    {
        out = original;
        if (amount <= 0.0f) return;

        std::array<int, 12> deg {};
        const int n = Scales::degrees (scale, deg);

        for (int i = 0; i < juce::jlimit (1, Pattern::maxSteps, length); ++i)
        {
            const uint32_t h = FastMath::mix (seed, (uint32_t) i);
            if (FastMath::hash01 (h) >= amount)
                continue;

            auto& s = out.steps[(size_t) i];
            const float kind = FastMath::hash01 (FastMath::mix (h, 7u));
            const float r    = FastMath::hash01 (FastMath::mix (h, 13u));

            if (! s.on)
            {
                if (kind < 0.35f) { s.on = true; s.velocity = 0.45f; s.gate = 0.3f; }   // note fantôme
                continue;
            }

            if (kind < 0.45f)        // note voisine dans la gamme
            {
                int idx = 0;
                for (int d = 0; d < n; ++d) if (deg[(size_t) d] == ((s.note % 12) + 12) % 12) idx = d;
                idx += r < 0.5f ? -1 : 1;
                if (idx < 0)  { idx += n; s.octave = std::max (-2, s.octave - 1); }
                if (idx >= n) { idx -= n; s.octave = std::min (2, s.octave + 1); }
                s.note = deg[(size_t) idx];
            }
            else if (kind < 0.65f)   // saut d'octave
                s.octave = juce::jlimit (-2, 2, s.octave + (r < 0.5f ? -1 : 1));
            else if (kind < 0.80f)   // ratchet
                s.ratchet = r < 0.5f ? 2 : 3;
            else if (kind < 0.90f)   // accent
                s.accent = ! s.accent;
            else                     // silence
                s.on = false;
        }
    }

    /** Motif de départ d'un nouveau son : une boucle simple de 16 pas en croches pointées. */
    inline Pattern makeDefault()
    {
        Pattern p;
        const int notes[16] = { 0, 0, 7, 0, 0, 3, 0, 10, 0, 0, 7, 0, 5, 0, 3, 0 };
        const bool hits[16] = { 1, 0, 1, 1, 0, 1, 0, 1, 1, 0, 1, 1, 0, 1, 0, 1 };
        for (int i = 0; i < 16; ++i)
        {
            auto& s = p.steps[(size_t) i];
            s.on = hits[i];
            s.note = notes[i];
            s.accent = (i % 4) == 0;
        }
        return p;
    }
}

//==============================================================================
/** Lecture / écriture du motif dans l'état du plugin (ValueTree) : projet Ableton et presets. */
namespace PatternIO
{
    inline juce::ValueTree toValueTree (const Pattern& p)
    {
        juce::ValueTree t ("PATTERN");
        t.setProperty ("version", 1, nullptr);
        for (int i = 0; i < Pattern::maxSteps; ++i)
        {
            const auto& s = p.steps[(size_t) i];
            juce::ValueTree st ("STEP");
            st.setProperty ("i", i, nullptr);
            st.setProperty ("on", s.on, nullptr);
            st.setProperty ("note", s.note, nullptr);
            st.setProperty ("oct", s.octave, nullptr);
            st.setProperty ("vel", s.velocity, nullptr);
            st.setProperty ("acc", s.accent, nullptr);
            st.setProperty ("gate", s.gate, nullptr);
            st.setProperty ("slide", s.slide, nullptr);
            st.setProperty ("prob", s.probability, nullptr);
            st.setProperty ("rat", s.ratchet, nullptr);
            if (s.hasLockIndex())  st.setProperty ("lkIdx", s.lockIndex, nullptr);
            if (s.hasLockCutoff()) st.setProperty ("lkCut", s.lockCutoff, nullptr);
            st.setProperty ("mod", p.modLane[(size_t) i], nullptr);
            t.appendChild (st, nullptr);
        }
        return t;
    }

    inline Pattern fromValueTree (const juce::ValueTree& t)
    {
        Pattern p;
        if (! t.hasType ("PATTERN"))
            return PatternOps::makeDefault();

        const float nan = std::numeric_limits<float>::quiet_NaN();
        for (const auto& st : t)
        {
            const int i = st.getProperty ("i", -1);
            if (! juce::isPositiveAndBelow (i, Pattern::maxSteps))
                continue;
            auto& s = p.steps[(size_t) i];
            s.on          = st.getProperty ("on", false);
            s.note        = juce::jlimit (-24, 36, (int) st.getProperty ("note", 0));
            s.octave      = juce::jlimit (-2, 2, (int) st.getProperty ("oct", 0));
            s.velocity    = juce::jlimit (0.0f, 1.0f, (float) st.getProperty ("vel", 0.8f));
            s.accent      = st.getProperty ("acc", false);
            s.gate        = juce::jlimit (0.05f, 1.0f, (float) st.getProperty ("gate", 0.5f));
            s.slide       = st.getProperty ("slide", false);
            s.probability = juce::jlimit (0.0f, 1.0f, (float) st.getProperty ("prob", 1.0f));
            s.ratchet     = juce::jlimit (1, 4, (int) st.getProperty ("rat", 1));
            s.lockIndex   = st.hasProperty ("lkIdx") ? juce::jlimit (0.0f, 10.0f, (float) st.getProperty ("lkIdx")) : nan;
            s.lockCutoff  = st.hasProperty ("lkCut") ? juce::jlimit (20.0f, 20000.0f, (float) st.getProperty ("lkCut")) : nan;
            p.modLane[(size_t) i] = juce::jlimit (-1.0f, 1.0f, (float) st.getProperty ("mod", 0.0f));
        }
        return p;
    }
}

//==============================================================================
/**
    Transmission sans verrou du motif de l'interface vers le thread audio (triple tampon).
      - thread de l'interface : edit() puis publish()
      - thread audio : acquire() en début de bloc (ne bloque jamais, n'alloue jamais)
*/
class PatternExchange
{
public:
    PatternExchange()
    {
        for (auto& b : buffers) b = PatternOps::makeDefault();
        editable = buffers[0];
    }

    /** Copie modifiable, côté interface. */
    Pattern& edit() noexcept { return editable; }
    const Pattern& get() const noexcept { return editable; }

    void publish() noexcept
    {
        buffers[(size_t) back] = editable;
        const int prev = middle.exchange (back | freshBit, std::memory_order_acq_rel);
        back = prev & indexMask;
    }

    /** Côté audio : récupère la dernière version publiée si elle existe. */
    const Pattern& acquire() noexcept
    {
        if (middle.load (std::memory_order_relaxed) & freshBit)
        {
            const int prev = middle.exchange (front, std::memory_order_acq_rel);
            front = prev & indexMask;
        }
        return buffers[(size_t) front];
    }

private:
    static constexpr int freshBit = 4, indexMask = 3;
    std::array<Pattern, 3> buffers;
    Pattern editable;
    int back = 0, front = 2;
    std::atomic<int> middle { 1 };
};
