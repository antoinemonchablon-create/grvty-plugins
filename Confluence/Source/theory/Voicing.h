// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "Theory.h"
#include <cstdlib>

//==============================================================================
/**
    Voicing : comment les notes d'un accord sont réparties sur le clavier.

        forme (position fondamentale serrée)
          -> limite du nombre de voix (on retire d'abord la quinte, les doublures, la fondamentale si la basse la joue)
          -> renversement (0 à 3, ou Auto : voice leading)
          -> écartement (serré, ouvert, drop 2, drop 3, large)
          -> octave et transposition globales
          -> notes ramenées entre la note la plus basse et la plus haute autorisées
          -> basse (fondamentale une ou deux octaves sous l'accord), hors de ces limites

    Sans allocation : utilisable depuis le thread audio.
*/
namespace Voicing
{
using namespace Theory;

enum Spread { spClose = 0, spOpen, spDrop2, spDrop3, spWide, numSpreads };
constexpr int inversionAuto = 4;
constexpr int maxNotes = 12;

inline const char* spreadName (int s) noexcept
{
    static const char* names[numSpreads] = { "Serr\xc3\xa9", "Ouvert", "Drop 2", "Drop 3", "Large" };
    return names[std::clamp (s, 0, (int) numSpreads - 1)];
}

struct Settings
{
    int inversion = 0;    // 0..3, inversionAuto
    int spread = spClose;
    int maxVoices = 8;    // 2..8, sans compter la basse
    int low = 0, high = 127;
    int bassOctaves = 0;  // 0 = pas de basse, 1 ou 2 octaves sous l'accord
    int shift = 0;        // octave * 12 + transposition
};

/** Notes absolues, croissantes, sans doublon. */
struct Notes
{
    int n[maxNotes] {};
    int count = 0;

    void sortUnique() noexcept
    {
        std::sort (n, n + count);
        count = (int) (std::unique (n, n + count) - n);
    }
    void push (int v) noexcept { if (count < maxNotes) n[count++] = v; }
    float centroid() const noexcept
    {
        if (count == 0) return 0.0f;
        float s = 0.0f;
        for (int i = 0; i < count; ++i) s += (float) n[i];
        return s / (float) count;
    }
};

/** Accord final : voix (croissantes) + éventuelle basse. */
struct Result
{
    Notes voices;
    int bass = -1;
    int rootPc = 0;
    bool valid() const noexcept { return voices.count > 0; }
};

//==============================================================================
/** Retire les notes les moins importantes jusqu'à 'maxVoices'. */
inline void reduceVoices (ChordShape& c, int maxVoices, bool bassPlaysRoot) noexcept
{
    maxVoices = std::clamp (maxVoices, 1, maxChordTones);
    while (c.count > maxVoices)
    {
        int worst = -1;
        float worstPrio = 1.0e9f;
        for (int i = 0; i < c.count; ++i)
        {
            float prio;
            if (c.orderedRemoval)
                prio = i == 0 ? 50.0f : 10.0f - (float) i;          // on garde la note du bas, on retire par le haut
            else
                switch (c.roles[i])
                {
                    case rThird:      prio = 10.0f; break;
                    case rSus:        prio = 9.5f; break;
                    case rSeventh:    prio = 9.0f; break;
                    case rSixth:      prio = 8.0f; break;
                    case rNinth:      prio = 7.0f; break;
                    case rThirteenth: prio = 7.0f; break;
                    case rRoot:       prio = bassPlaysRoot ? 2.0f : 6.0f; break;
                    case rEleventh:   prio = 5.0f; break;
                    case rFifth:      prio = 3.0f; break;
                    case rDouble:     prio = 1.0f; break;
                    default:          prio = 4.0f; break;
                }
            if (prio <= worstPrio)   // à égalité, la plus haute (on parcourt de bas en haut)
            {
                worstPrio = prio;
                worst = i;
            }
        }
        for (int i = worst; i < c.count - 1; ++i)
        {
            c.offsets[i] = c.offsets[i + 1];
            c.roles[i] = c.roles[i + 1];
        }
        --c.count;
    }
}

/** Renversement k : la note du bas monte d'une octave, k fois (k pris modulo le nombre de notes). */
inline void invert (Notes& v, int k) noexcept
{
    if (v.count < 2) return;
    k = ((k % v.count) + v.count) % v.count;
    for (int i = 0; i < k; ++i)
    {
        v.n[0] += 12;
        std::sort (v.n, v.n + v.count);
    }
}

inline void applySpread (Notes& v, int spread) noexcept
{
    const int c = v.count;
    switch (spread)
    {
        case spOpen:
        case spWide:
            if (c >= 3)
                for (int i = 1; i < c; i += 2)
                    v.n[i] += 12;
            std::sort (v.n, v.n + c);
            if (spread == spWide && c >= 2)
                v.n[0] -= 12;
            break;
        case spDrop2:
            if (c >= 3) v.n[c - 2] -= 12;
            break;
        case spDrop3:
            if (c >= 4) v.n[c - 3] -= 12;
            else if (c == 3) v.n[0] -= 12;
            break;
        default: break;
    }
    v.sortUnique();
}

/** Ramène les notes entre low et high : d'abord en déplaçant tout l'accord par octaves, puis note par note. */
inline void fitRange (Notes& v, int low, int high) noexcept
{
    if (v.count == 0) return;
    int lo = std::clamp (std::min (low, high), 0, 127), hi = std::clamp (std::max (low, high), 0, 127);
    std::sort (v.n, v.n + v.count);

    if (v.n[0] < lo)
    {
        const int up = 12 * ((lo - v.n[0] + 11) / 12);
        for (int i = 0; i < v.count; ++i) v.n[i] += up;
    }
    else if (v.n[v.count - 1] > hi)
    {
        const int down = 12 * ((v.n[v.count - 1] - hi + 11) / 12);
        for (int i = 0; i < v.count; ++i) v.n[i] -= down;
    }

    // l'accord est plus large que la plage : chaque note dépassant est repliée à l'octave près
    int out = 0;
    for (int i = 0; i < v.count; ++i)
    {
        int x = v.n[i];
        while (x < lo) x += 12;
        while (x > hi) x -= 12;
        if (x >= lo && x <= hi)
            v.n[out++] = x;
    }
    v.count = out;
    v.sortUnique();
}

/** Mouvement des voix entre deux accords (somme des déplacements, appariement au plus proche). */
inline float movement (const Notes& a, const Notes& b) noexcept
{
    if (a.count == 0 || b.count == 0) return 0.0f;
    if (a.count == b.count)
    {
        float s = 0.0f;
        for (int i = 0; i < a.count; ++i) s += (float) std::abs (a.n[i] - b.n[i]);
        return s;
    }
    auto nearest = [] (const Notes& from, const Notes& to)
    {
        float s = 0.0f;
        for (int i = 0; i < from.count; ++i)
        {
            int best = 1000;
            for (int j = 0; j < to.count; ++j) best = std::min (best, std::abs (from.n[i] - to.n[j]));
            s += (float) best;
        }
        return s;
    };
    return 0.5f * (nearest (a, b) + nearest (b, a));
}

/** Construit les voix pour un renversement et un décalage d'octave donnés. */
inline Notes place (const ChordShape& c, int rootAbs, int inversion, int octaveShift, const Settings& s) noexcept
{
    Notes v;
    for (int i = 0; i < c.count; ++i)
        v.push (rootAbs + c.offsets[i]);
    v.sortUnique();
    invert (v, inversion);
    applySpread (v, s.spread);
    for (int i = 0; i < v.count; ++i)
        v.n[i] += s.shift + octaveShift;
    fitRange (v, s.low, s.high);
    return v;
}

/**
    Accord final. 'previous' = voix de l'accord précédent (pour le renversement Auto), vide s'il n'y en a pas.
    'inversionOverride' >= 0 remplace le renversement des réglages (pads).
*/
inline Result voice (ChordShape c, int rootAbs, const Settings& s, const Notes& previous, int inversionOverride = -1) noexcept
{
    Result r;
    r.rootPc = mod12 (rootAbs + s.shift);
    reduceVoices (c, s.maxVoices, s.bassOctaves > 0);
    if (c.count == 0) return r;

    const int inv = inversionOverride >= 0 ? inversionOverride : s.inversion;
    if (inv == inversionAuto)
    {
        const Notes home = place (c, rootAbs, 0, 0, s);
        if (previous.count == 0)
            r.voices = home;
        else
        {
            const float homeCentre = home.centroid();
            float best = 1.0e9f;
            for (int k = 0; k < c.count; ++k)
                for (int o = -12; o <= 12; o += 12)
                {
                    const Notes cand = place (c, rootAbs, k, o, s);
                    if (cand.count == 0) continue;
                    // au plus proche de l'accord précédent, sans dériver loin de la position de départ
                    const float cost = movement (cand, previous) + 0.35f * std::abs (cand.centroid() - homeCentre);
                    if (cost < best - 1.0e-4f)
                    {
                        best = cost;
                        r.voices = cand;
                    }
                }
        }
    }
    else
        r.voices = place (c, rootAbs, std::clamp (inv, 0, 3), 0, s);

    if (r.voices.count > 0 && s.bassOctaves > 0)
    {
        int b = rootAbs + s.shift - 12 * s.bassOctaves;
        while (b >= r.voices.n[0]) b -= 12;
        if (b >= 0) r.bass = b;
    }
    return r;
}
} // namespace Voicing
