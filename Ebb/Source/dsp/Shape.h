// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "Curves.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

//==============================================================================
/**
    Courbe perso (dessinée dans l'interface), Tension et Lissage.

    Une courbe perso est une suite de points (x de 0 à 1 dans la courbe, y = NIVEAU du son : 1 = plein volume,
    0 = coupé). Chaque segment a sa courbure (-1 à 1 : creusé vers le bas ... bombé vers le haut).
    Deux points au même x font une marche (saut de niveau). Le premier point est toujours en x = 0, le dernier en x = 1.

    Le moteur ne lit jamais les points : la courbe est rendue dans une table de baisse r = 1 - niveau
    (CurveTables::size + 1 valeurs), comme les courbes d'usine.

      - Tension (-1 à 1) : r' = r / (r + (1 - r) K), K = 2^(3 x tension). + : plus sèche (le son revient plus vite),
        - : plus molle (le son reste bas plus longtemps). 0 et 1 restent 0 et 1.
      - Lissage (0 à 1) : flou circulaire (3 passes de moyenne glissante ~ gaussienne) sur la table,
        jusqu'à 8 % de la durée de la courbe. La courbe est traitée comme périodique (ce qu'elle est en SYNC).
*/
namespace Ebb
{
    struct ShapePoint
    {
        float x = 0.0f, y = 1.0f;   // y = niveau (1 = plein volume)
        float curve = 0.0f;         // courbure du segment qui part de ce point
    };

    struct ShapeData
    {
        static constexpr int maxPoints = 64;
        std::array<ShapePoint, maxPoints> pts {};
        int count = 0;

        bool operator== (const ShapeData& o) const noexcept
        {
            auto same = [] (float a, float b) { return ! (a < b) && ! (b < a); };
            if (count != o.count) return false;
            for (int i = 0; i < count; ++i)
                if (! same (pts[(size_t) i].x, o.pts[(size_t) i].x) || ! same (pts[(size_t) i].y, o.pts[(size_t) i].y)
                    || ! same (pts[(size_t) i].curve, o.pts[(size_t) i].curve))
                    return false;
            return true;
        }
        bool operator!= (const ShapeData& o) const noexcept { return ! (*this == o); }
    };

    namespace ShapeMath
    {
        inline float clamp01 (float v) noexcept { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

        /** Courbure d'un segment : t de 0 à 1 -> 0 à 1, bombé vers le haut si c > 0, creusé si c < 0. */
        inline float bend (float t, float c) noexcept
        {
            if (c > -1.0e-4f && c < 1.0e-4f) return t;
            const float k = std::exp2 (-4.0f * c);
            return t / (t + (1.0f - t) * k);
        }

        /** Tension appliquée à une baisse r (0..1). k = 2^(3 x tension). */
        inline float tensionK (float tension) noexcept { return std::exp2 (3.0f * tension); }
        inline float applyTension (float r, float k) noexcept
        {
            const float d = r + (1.0f - r) * k;
            return d > 0.0f ? r / d : 0.0f;
        }

        /** Remet une courbe en ordre : bornes x = 0 et x = 1, x croissants, valeurs dans 0..1, au moins 2 points. */
        inline void sanitize (ShapeData& s) noexcept
        {
            if (s.count < 2)
            {
                s.pts[0] = { 0.0f, 0.0f, 0.0f };
                s.pts[1] = { 1.0f, 1.0f, 0.0f };
                s.count = 2;
            }
            s.count = std::min (s.count, ShapeData::maxPoints);
            for (int i = 0; i < s.count; ++i)
            {
                auto& p = s.pts[(size_t) i];
                p.x = clamp01 (p.x);
                p.y = clamp01 (p.y);
                p.curve = p.curve < -1.0f ? -1.0f : (p.curve > 1.0f ? 1.0f : p.curve);
                if (i > 0 && p.x < s.pts[(size_t) i - 1].x) p.x = s.pts[(size_t) i - 1].x;
            }
            s.pts[0].x = 0.0f;
            s.pts[(size_t) s.count - 1].x = 1.0f;
        }

        /** Niveau de la courbe en x (marche : à droite du saut, on prend le point le plus à droite). */
        inline float levelAt (const ShapeData& s, float x) noexcept
        {
            if (s.count <= 0) return 1.0f;
            if (x <= s.pts[0].x) return s.pts[0].y;
            for (int i = 0; i + 1 < s.count; ++i)
            {
                const auto& a = s.pts[(size_t) i];
                const auto& b = s.pts[(size_t) i + 1];
                if (x < b.x)
                {
                    const float w = b.x - a.x;
                    const float t = w > 0.0f ? (x - a.x) / w : 1.0f;
                    return a.y + (b.y - a.y) * bend (t, a.curve);
                }
            }
            return s.pts[(size_t) s.count - 1].y;
        }

        /** Rend la courbe dans une table de BAISSE (n + 1 valeurs, r = 1 - niveau). Parcours en une passe. */
        inline void render (const ShapeData& s, float* r, int n) noexcept
        {
            int seg = 0;
            for (int i = 0; i <= n; ++i)
            {
                const float x = (float) i / (float) n;
                while (seg + 1 < s.count && x >= s.pts[(size_t) seg + 1].x && seg + 2 < s.count) ++seg;
                float level;
                if (s.count < 2) level = 1.0f;
                else if (i == n) level = s.pts[(size_t) s.count - 1].y;
                else
                {
                    const auto& a = s.pts[(size_t) seg];
                    const auto& b = s.pts[(size_t) seg + 1];
                    const float w = b.x - a.x;
                    const float t = w > 0.0f ? clamp01 ((x - a.x) / w) : 1.0f;
                    level = a.y + (b.y - a.y) * bend (t, a.curve);
                }
                r[i] = 1.0f - clamp01 (level);
            }
        }

        /** Courbe de départ : une pompe classique (chute au temps, remontée bombée). */
        inline ShapeData defaultShape() noexcept
        {
            ShapeData s;
            s.pts[0] = { 0.0f, 0.0f, 0.55f };
            s.pts[1] = { 0.6f, 1.0f, 0.0f };
            s.pts[2] = { 1.0f, 1.0f, 0.0f };
            s.count = 3;
            return s;
        }

        /** Convertit une table de baisse (courbe d'usine) en points (Ramer-Douglas-Peucker sur le niveau),
            pour pouvoir la retoucher. 'tolerance' : écart maximal toléré (niveau 0..1). */
        inline ShapeData fromTable (const float* r, int n, float tolerance = 0.006f)
        {
            // échantillonnage : 1024 points suffisent pour les formes, en gardant les marches nettes
            constexpr int m = 1024;
            std::vector<float> xs ((size_t) m + 1), ys ((size_t) m + 1);
            for (int i = 0; i <= m; ++i)
            {
                xs[(size_t) i] = (float) i / (float) m;
                ys[(size_t) i] = 1.0f - r[(size_t) ((int64_t) i * n / m)];
            }
            std::vector<char> keep ((size_t) m + 1, 0);
            keep[0] = keep[(size_t) m] = 1;
            std::vector<std::pair<int, int>> stack { { 0, m } };
            while (! stack.empty())
            {
                const auto [a, b] = stack.back();
                stack.pop_back();
                float worst = 0.0f;
                int idx = -1;
                for (int i = a + 1; i < b; ++i)
                {
                    const float t = (xs[(size_t) i] - xs[(size_t) a]) / (xs[(size_t) b] - xs[(size_t) a]);
                    const float yl = ys[(size_t) a] + t * (ys[(size_t) b] - ys[(size_t) a]);
                    const float d = std::abs (ys[(size_t) i] - yl);
                    if (d > worst) { worst = d; idx = i; }
                }
                if (idx >= 0 && worst > tolerance)
                {
                    keep[(size_t) idx] = 1;
                    stack.push_back ({ a, idx });
                    stack.push_back ({ idx, b });
                }
            }
            ShapeData s;
            for (int i = 0; i <= m && s.count < ShapeData::maxPoints; ++i)
                if (keep[(size_t) i])
                    s.pts[(size_t) s.count++] = { xs[(size_t) i], ys[(size_t) i], 0.0f };
            // trop de points (forme très détaillée) : on recommence avec une tolérance plus large
            if (s.count >= ShapeData::maxPoints && tolerance < 0.1f)
                return fromTable (r, n, tolerance * 2.0f);
            sanitize (s);
            return s;
        }

        /** Lissage circulaire : 3 passes de moyenne glissante de demi-largeur h (0 = copie). in et out : n + 1 valeurs.
            tmp : n valeurs de travail. Coût : O(n) quelle que soit la largeur. */
        inline void smoothCircular (const float* in, float* out, float* tmp, int n, int h) noexcept
        {
            if (h <= 0)
            {
                for (int i = 0; i <= n; ++i) out[i] = in[i];
                return;
            }
            if (h > n / 2 - 1) h = n / 2 - 1;
            const float inv = 1.0f / (float) (2 * h + 1);
            const float* src = in;
            for (int pass = 0; pass < 3; ++pass)
            {
                float* dst = (pass % 2 == 0) ? tmp : out;
                double sum = 0.0;
                for (int k = -h; k <= h; ++k) sum += src[(k + n) % n];
                for (int i = 0; i < n; ++i)
                {
                    dst[i] = (float) (sum * inv);
                    sum += src[(i + h + 1) % n];
                    sum -= src[(i - h + n) % n];
                }
                src = dst;
            }
            // 3 passes : tmp -> out -> tmp -> la dernière est dans tmp
            for (int i = 0; i < n; ++i) out[i] = tmp[i];
            out[n] = out[0];
        }

        /** Demi-largeur du lissage (en points de table) pour un réglage 0..1. */
        inline int smoothRadius (float smooth, int n) noexcept
        {
            const float s = clamp01 (smooth);
            return (int) std::lround (std::pow (s, 1.5f) * 0.08f * (float) n);
        }
    }
}
