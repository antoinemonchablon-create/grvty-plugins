// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "../theory/Theory.h"
#include "../engine/Resolver.h"

//==============================================================================
/** Textes d'accords pour l'interface : nom (Am9), degré (i), notes (A1 C3 E3...). */
namespace ChordText
{
inline juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

struct Described
{
    juce::String name, degree, notes;
    bool valid = false;
};

/** notes : bass compris ; rootPc : fondamentale sonore ; chordRootPc : fondamentale dans la tonalité. */
inline Described describe (const int* notes, int count, int bass, int rootPc, int chordRootPc, int scale, int keyRoot, bool borrowed)
{
    Described d;
    if (count <= 0) return d;
    const bool flats = Theory::useFlats (scale, keyRoot);
    char buf[64];
    Theory::chordName (rootPc, notes, count, flats, buf, (int) sizeof (buf));
    d.name = u8 (buf);
    uint16_t mask = 0;
    for (int i = 0; i < count; ++i) mask = (uint16_t) (mask | (1u << (unsigned) Theory::mod12 (notes[i] - rootPc)));
    Theory::romanNumeral (scale, keyRoot, chordRootPc, mask, buf, (int) sizeof (buf));
    d.degree = u8 (buf);
    if (borrowed) d.degree << u8 (" (emprunt)");
    juce::StringArray parts;
    for (int i = 0; i < count; ++i)
    {
        char nn[16];
        Theory::noteName (notes[i], nn, (int) sizeof (nn), flats);
        parts.add (juce::String (nn) + (notes[i] == bass ? "*" : ""));
    }
    d.notes = parts.joinIntoString ("   ");
    d.valid = true;
    return d;
}

inline Described describe (const Engine::Resolved& r, int scale, int keyRoot)
{
    int notes[Voicing::maxNotes + 1];
    int n = 0;
    if (r.voiced.bass >= 0) notes[n++] = r.voiced.bass;
    for (int i = 0; i < r.voiced.voices.count; ++i) notes[n++] = r.voiced.voices.n[i];
    return describe (notes, n, r.voiced.bass, r.voiced.rootPc, r.chordRootPc, scale, keyRoot, r.borrowed);
}

/** Degré romain d'un degré de gamme, pour un type d'accord donné (suggestions, menus). */
inline juce::String romanOfDegree (int scale, int keyRoot, int degree, int type)
{
    const auto shape = Theory::buildScaleChord (scale, degree, type);
    uint16_t m = 0;
    for (int i = 0; i < shape.count; ++i) m = (uint16_t) (m | (1u << (unsigned) Theory::mod12 (shape.offsets[i])));
    char buf[16];
    Theory::romanNumeral (scale, keyRoot, Theory::mod12 (keyRoot + Theory::degreeInterval (scale, degree)), m, buf, 16);
    return u8 (buf);
}
}
