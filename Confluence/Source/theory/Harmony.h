// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "Theory.h"
#include "../engine/Resolver.h"
#include <array>
#include <vector>
#include <cmath>

//==============================================================================
/**
    Aides harmoniques (thread de l'interface) :
      - gammes conseillées par style ;
      - détection de la gamme à partir de ce qui a été joué ;
      - degrés qui s'enchaînent bien après l'accord en cours ;
      - générateur de progressions pour les 8 pads.
*/
namespace Harmony
{
using namespace Theory;

enum Style { styleDeep = 0, styleHypnotic, styleTechHouse, styleMental, numStyles };

inline const char* styleName (int s) noexcept
{
    static const char* names[numStyles] = { "Deep / House", "Hypnotic", "Tech House", "Mental" };
    return names[std::clamp (s, 0, (int) numStyles - 1)];
}

/** Les 3 gammes conseillées pour un style. */
inline std::array<int, 3> styleScales (int style) noexcept
{
    switch (style)
    {
        case styleDeep:      return { dorian, majorScale, naturalMinor };
        case styleHypnotic:  return { naturalMinor, dorian, phrygian };
        case styleTechHouse: return { naturalMinor, mixolydian, minorPenta };
        default:             return { phrygianDominant, hungarianMinor, doubleHarmonic };
    }
}

//==============================================================================
// Détection de la gamme
//==============================================================================
struct Candidate { int root = 0, scale = 0; float score = 0.0f; };

/**
    Classe les couples (tonique, gamme) selon ce qui a été joué.
      'weight' : poids de chaque classe de hauteur (nombre de notes + durée)
      'bass'   : poids des notes jouées comme note la plus grave (indice fort de la tonique)

    Score = part des notes jouées qui tombent dans la gamme (une fausse note coûte cher)
          + importance de la tonique, de sa quinte et de sa tierce
          - notes de la gamme jamais jouées (on préfère la gamme la plus simple qui explique tout).
*/
inline std::vector<Candidate> rankScales (const std::array<float, 12>& weight, const std::array<float, 12>& bass, int count = 3)
{
    float total = 0.0f, bassTotal = 0.0f;
    for (int i = 0; i < 12; ++i) { total += weight[(size_t) i]; bassTotal += bass[(size_t) i]; }
    std::vector<Candidate> all;
    if (total <= 0.0f) return all;

    for (int s = 0; s < numScales; ++s)
    {
        if (s == chromatic) continue;
        const auto& sc = scaleInfo (s);
        for (int r = 0; r < 12; ++r)
        {
            const auto mask = scaleMask (s, r);
            float inside = 0.0f;
            int unplayed = 0;
            for (int pc = 0; pc < 12; ++pc)
            {
                const bool in = (mask >> pc) & 1u;
                if (in)
                {
                    inside += weight[(size_t) pc];
                    if (weight[(size_t) pc] < 0.02f * total) ++unplayed;
                }
            }
            const float coverage = inside / total;

            const int third = hasInterval (s, 3) ? 3 : (hasInterval (s, 4) ? 4 : -1);
            const int fifth = hasInterval (s, 7) ? 7 : -1;
            float tonic = weight[(size_t) r] / total;
            if (fifth > 0) tonic += 0.5f * weight[(size_t) mod12 (r + fifth)] / total;
            if (third > 0) tonic += 0.3f * weight[(size_t) mod12 (r + third)] / total;
            const float bassTonic = bassTotal > 0.0f ? bass[(size_t) r] / bassTotal : 0.0f;

            const float score = 4.0f * coverage + 1.6f * tonic + 0.8f * bassTonic
                              - 0.35f * (float) unplayed / (float) sc.size;
            all.push_back ({ r, s, score });
        }
    }
    std::sort (all.begin(), all.end(), [] (const Candidate& a, const Candidate& b) { return a.score > b.score; });
    if ((int) all.size() > count) all.resize ((size_t) count);
    return all;
}

//==============================================================================
// Enchaînements
//==============================================================================
/**
    Poids (0..1) des degrés qui s'enchaînent bien après le degré 'from' (0 = tonique).
    Tables de fonctions harmoniques : majeur (I IV V vi...), mineur (i iv VI VII...),
    puis petits ajustements selon le style (hypnotic : on reste près de la tonique, mental : demi-ton bII).
*/
inline std::array<float, 12> nextDegreeWeights (int s, int from, int style)
{
    std::array<float, 12> w {};
    const int size = scaleSize (s);
    from = ((from % size) + size) % size;

    if (size == 7)
    {
        static const float majorT[7][7] = {
            //  I     ii    iii   IV    V     vi    vii
            { 0.0f, 0.6f, 0.3f, 0.9f, 0.8f, 0.8f, 0.1f },   // I
            { 0.3f, 0.0f, 0.2f, 0.5f, 0.9f, 0.2f, 0.4f },   // ii
            { 0.2f, 0.4f, 0.0f, 0.6f, 0.2f, 0.9f, 0.1f },   // iii
            { 0.8f, 0.5f, 0.2f, 0.0f, 0.8f, 0.4f, 0.2f },   // IV
            { 0.9f, 0.2f, 0.3f, 0.5f, 0.0f, 0.7f, 0.1f },   // V
            { 0.3f, 0.7f, 0.4f, 0.8f, 0.6f, 0.0f, 0.1f },   // vi
            { 0.9f, 0.1f, 0.5f, 0.2f, 0.2f, 0.3f, 0.0f },   // vii
        };
        static const float minorT[7][7] = {
            //  i     ii    III   iv    v     VI    VII
            { 0.0f, 0.3f, 0.4f, 0.8f, 0.5f, 0.8f, 0.8f },   // i
            { 0.6f, 0.0f, 0.2f, 0.3f, 0.7f, 0.2f, 0.4f },   // ii
            { 0.3f, 0.1f, 0.0f, 0.6f, 0.2f, 0.7f, 0.6f },   // III
            { 0.8f, 0.1f, 0.2f, 0.0f, 0.6f, 0.4f, 0.7f },   // iv
            { 0.9f, 0.1f, 0.2f, 0.4f, 0.0f, 0.6f, 0.3f },   // v
            { 0.5f, 0.1f, 0.6f, 0.6f, 0.3f, 0.0f, 0.8f },   // VI
            { 0.9f, 0.1f, 0.7f, 0.3f, 0.2f, 0.5f, 0.0f },   // VII
        };
        const bool minorish = isMinorish (s);
        for (int d = 0; d < 7; ++d)
            w[(size_t) d] = (minorish ? minorT : majorT)[from][d];

        if (hasInterval (s, 1))   // gammes avec seconde mineure (phrygien...) : le bII est la couleur de la gamme
        {
            if (from == 0) w[1] = std::max (w[1], 0.75f);
            if (from == 1) w[0] = 0.95f;
        }
    }
    else
    {
        // pentatoniques, blues, hirajoshi : retour à la tonique, voisins proches
        for (int d = 0; d < size; ++d)
        {
            if (d == from) continue;
            const int dist = std::min (std::abs (d - from), size - std::abs (d - from));
            w[(size_t) d] = d == 0 ? 0.9f : (dist == 1 ? 0.6f : 0.35f);
        }
    }

    // couleur du style
    if (style == styleHypnotic)
        for (int d = 0; d < size; ++d) w[(size_t) d] *= d == 0 ? 1.2f : (d == size - 1 ? 1.1f : 0.85f);
    if (style == styleMental && size == 7)
        w[1] *= 1.3f;
    if (style == styleTechHouse && size == 7)
    {
        w[3] *= 1.15f;
        w[4] *= 1.15f;
    }
    for (auto& x : w) x = std::clamp (x, 0.0f, 1.0f);
    w[(size_t) from] = 0.0f;
    return w;
}

/** Les 3 meilleurs degrés après 'from'. */
inline std::array<int, 3> suggestNext (int s, int from, int style)
{
    const auto w = nextDegreeWeights (s, from, style);
    std::array<int, 3> best { -1, -1, -1 };
    std::array<float, 3> bw { 0.0f, 0.0f, 0.0f };
    for (int d = 0; d < scaleSize (s); ++d)
        for (int k = 0; k < 3; ++k)
            if (w[(size_t) d] > bw[(size_t) k] + 1.0e-4f)
            {
                for (int j = 2; j > k; --j) { best[(size_t) j] = best[(size_t) j - 1]; bw[(size_t) j] = bw[(size_t) j - 1]; }
                best[(size_t) k] = d;
                bw[(size_t) k] = w[(size_t) d];
                break;
            }
    return best;
}

//==============================================================================
// Générateur de progressions pour les pads
//==============================================================================
/**
    Remplit les 8 pads avec une progression dans la gamme, selon le style.
    Pads 1-4 : une progression type du style ; pads 5-8 : une deuxième, qui répond à la première.
    Toujours en renversement Auto (voice leading). 'seed' change la combinaison.
*/
inline Engine::PadSet generatePads (int style, int s, uint32_t seed)
{
    struct Template { int8_t deg[4]; int8_t type[4]; };
    using namespace Theory;
    const int8_t T = ctTriad, S7 = ctSeventh, N9 = ctNinth, E11 = ctEleventh, A9 = ctAdd9, S2 = ctSus2, S4 = ctSus4,
                 Q = ctQuartal, C = ctCluster;

    // [style][mineur/majeur][4 modèles]
    static const Template table[numStyles][2][4] = {
        {   // Deep / House : 7e et 9e, ii-V, progressions chaudes
            { { { 0, 3, 6, 2 }, { N9, N9, S7, N9 } }, { { 0, 5, 3, 4 }, { N9, S7, N9, S7 } },
              { { 1, 4, 0, 0 }, { S7, S7, N9, E11 } }, { { 0, 6, 5, 6 }, { N9, S7, S7, N9 } } },
            { { { 1, 4, 0, 5 }, { S7, N9, N9, S7 } }, { { 0, 5, 3, 4 }, { N9, S7, N9, S7 } },
              { { 3, 4, 2, 5 }, { N9, S7, S7, N9 } }, { { 0, 3, 0, 4 }, { A9, N9, A9, S7 } } },
        },
        {   // Hypnotic : peu d'accords, suspendus, on tourne autour de la tonique
            { { { 0, 0, 5, 0 }, { S7, S2, S7, N9 } }, { { 0, 6, 0, 5 }, { N9, S7, N9, S7 } },
              { { 0, 3, 0, 3 }, { S2, S7, S2, N9 } }, { { 0, 0, 6, 6 }, { E11, N9, S7, S4 } } },
            { { { 0, 0, 3, 0 }, { A9, S2, S7, A9 } }, { { 0, 4, 0, 3 }, { N9, S4, N9, S7 } },
              { { 0, 5, 0, 5 }, { S2, S7, S2, N9 } }, { { 0, 0, 1, 1 }, { E11, N9, S7, S4 } } },
        },
        {   // Tech House : stabs en 7e, i-iv-v
            { { { 0, 0, 3, 4 }, { S7, S7, S7, S7 } }, { { 0, 6, 5, 6 }, { S7, T, S7, T } },
              { { 0, 3, 4, 3 }, { N9, S7, S7, N9 } }, { { 0, 0, 0, 5 }, { S7, S7, S4, S7 } } },
            { { { 0, 6, 3, 0 }, { S7, S7, S7, S7 } }, { { 0, 3, 4, 0 }, { S7, S7, S7, N9 } },
              { { 0, 0, 3, 4 }, { N9, S7, S7, S7 } }, { { 0, 5, 3, 4 }, { S7, S7, T, S7 } } },
        },
        {   // Mental : demi-ton bII, clusters, quartes
            { { { 0, 1, 0, 1 }, { T, T, C, T } }, { { 0, 1, 4, 1 }, { Q, T, C, T } },
              { { 0, 0, 1, 6 }, { C, Q, T, T } }, { { 0, 4, 1, 0 }, { T, T, Q, C } } },
            { { { 0, 1, 0, 1 }, { T, T, C, T } }, { { 0, 1, 4, 1 }, { Q, T, C, T } },
              { { 0, 0, 1, 6 }, { C, Q, T, T } }, { { 0, 4, 1, 0 }, { T, T, Q, C } } },
        },
    };

    uint32_t r = seed == 0 ? 0x1234567u : seed;
    auto next = [&r] { r ^= r << 13; r ^= r >> 17; r ^= r << 5; return r; };

    style = std::clamp (style, 0, (int) numStyles - 1);
    const int family = isMinorish (s) || scaleSize (s) != 7 ? 0 : 1;
    const int a = (int) (next() % 4u);
    int b = (int) (next() % 3u);
    if (b >= a) ++b;   // deuxième modèle différent du premier

    Engine::PadSet ps;
    const int size = scaleSize (s);
    for (int half = 0; half < 2; ++half)
    {
        const auto& t = table[style][family][half == 0 ? a : b];
        for (int k = 0; k < 4; ++k)
        {
            auto& p = ps.pads[(size_t) (half * 4 + k)];
            p.degree = (int8_t) (t.deg[k] % size);
            p.type = t.type[k];
            p.inversion = 4;
            p.octave = 0;
        }
    }
    return ps;
}
} // namespace Harmony
