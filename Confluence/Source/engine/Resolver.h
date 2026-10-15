// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "../theory/Voicing.h"
#include <array>

//==============================================================================
/**
    De la note jouée à l'accord : les 4 modes de jeu.

      GAMME           la note est ramenée dans la gamme, l'accord est empilé sur ce degré
      TOUCHES BLANCHES  Do..Si = degrés I..VII de la gamme choisie, quelle que soit la tonique ;
                      les touches noires jouent une variante du degré de la touche blanche juste en dessous
      ACCORD FIXE     une forme d'accord transposée chromatiquement sur la note jouée (ou la forme apprise)
      PADS            8 notes consécutives déclenchent 8 accords réglables

    Sans allocation : appelé par le thread audio à chaque note.
*/
namespace Engine
{
using namespace Theory;

enum Mode { modeScale = 0, modeWhiteKeys, modeFixed, modePads, numModes };
enum BlackKeys { bkSus4 = 0, bkSus2, bkNinth, bkBorrowed, bkMute, numBlackKeys };
constexpr int numPads = 8;

/** Un pad : degré de la gamme (0 = tonique), type d'accord, renversement (4 = Auto), octave. */
struct Pad
{
    int8_t degree = 0;
    int8_t type = ctSeventh;
    int8_t inversion = Voicing::inversionAuto;
    int8_t octave = 0;
    bool operator== (const Pad& o) const noexcept { return degree == o.degree && type == o.type && inversion == o.inversion && octave == o.octave; }
};

struct PadSet
{
    std::array<Pad, numPads> pads {};
    bool operator== (const PadSet& o) const noexcept { return pads == o.pads; }
};

/** Accord appris (bouton Learn) : intervalles depuis la note la plus basse. */
struct LearnedShape
{
    int8_t iv[maxChordTones] {};
    int count = 0;
    bool operator== (const LearnedShape& o) const noexcept
    {
        if (count != o.count) return false;
        for (int i = 0; i < count; ++i) if (iv[i] != o.iv[i]) return false;
        return true;
    }
};

struct ChordSettings
{
    int mode = modeScale;
    int rootPc = 9;
    int scale = naturalMinor;
    int outOfScale = oosNearest;
    int chordType = ctSeventh;
    int blackKeys = bkSus4;
    int fixedShape = 3;
    int padBase = 36;
    Voicing::Settings voicing;
};

/** Ce qui a été joué : les notes, et de quoi afficher le nom et le degré. */
struct Resolved
{
    Voicing::Result voiced;
    int chordRootPc = 0;     // fondamentale (avant transposition)
    int padIndex = -1;
    bool borrowed = false;
};

/** Tonique placée dans l'octave de la note jouée (entre F#-1 et F en dessous / au-dessus du Do de cette octave). */
inline int tonicNear (int note, int rootPc) noexcept
{
    return 12 * floorDiv (note, 12) + rootPc - (rootPc >= 6 ? 12 : 0);
}

inline ChordShape learnedOrDefault (const LearnedShape& l) noexcept
{
    if (l.count >= 2)
        return shapeFromIntervals (l.iv, l.count, true);
    return fixedShape (1);   // rien d'appris : accord mineur
}

/**
    Calcule l'accord pour une note. Renvoie false si la note ne joue rien
    (note ignorée hors gamme, touche noire muette, note hors des 8 pads).
*/
inline bool resolve (const ChordSettings& cs, const PadSet& pads, const LearnedShape& learned,
                     int note, const Voicing::Notes& previous, Resolved& out) noexcept
{
    out = Resolved {};
    ChordShape shape;
    int rootAbs = 0;
    int inversionOverride = -1;

    switch (cs.mode)
    {
        case modeScale:
        {
            const int q = quantize (cs.scale, cs.rootPc, note, cs.outOfScale);
            if (q < 0) return false;
            const int deg = degreeOfPc (cs.scale, cs.rootPc, q);
            shape = buildScaleChord (cs.scale, deg, cs.chordType);
            rootAbs = q;
            break;
        }
        case modeWhiteKeys:
        {
            static const int8_t whiteIndex[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
            static const bool   isBlack[12]    = { false, true, false, true, false, false, true, false, true, false, true, false };
            const int pc = mod12 (note);
            const int deg = whiteIndex[pc];
            const int tonic = tonicNear (note, cs.rootPc);
            int type = cs.chordType;
            int scale = cs.scale;

            if (isBlack[pc])
            {
                switch (cs.blackKeys)
                {
                    case bkSus4:  type = ctSus4; break;
                    case bkSus2:  type = ctSus2; break;
                    case bkNinth: type = ctNinth; break;
                    case bkBorrowed: scale = parallelScale (cs.scale); out.borrowed = true; break;
                    default: return false;
                }
            }
            shape = buildScaleChord (scale, deg, type);
            rootAbs = tonic + degreeInterval (scale, deg);
            break;
        }
        case modeFixed:
            shape = cs.fixedShape == fixedLearned ? learnedOrDefault (learned) : fixedShape (cs.fixedShape);
            rootAbs = note;
            break;
        case modePads:
        {
            const int k = note - cs.padBase;
            if (k < 0 || k >= numPads) return false;
            const auto& p = pads.pads[(size_t) k];
            shape = buildScaleChord (cs.scale, p.degree, p.type);
            rootAbs = 60 + cs.rootPc - (cs.rootPc >= 6 ? 12 : 0) + degreeInterval (cs.scale, p.degree) + 12 * p.octave;
            inversionOverride = p.inversion;
            out.padIndex = k;
            break;
        }
        default:
            return false;
    }

    if (shape.count == 0) return false;
    out.chordRootPc = mod12 (rootAbs);
    out.voiced = Voicing::voice (shape, rootAbs, cs.voicing, previous, inversionOverride);
    return out.voiced.valid();
}
} // namespace Engine
