// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <algorithm>

//==============================================================================
/**
    Théorie musicale de Confluence : gammes, quantification d'une note dans la gamme, construction des accords.

    Tout ici est sans allocation et sans verrou : utilisable depuis le thread audio.
    Convention des notes : MIDI 60 = C3 (convention d'Ableton).
*/
namespace Theory
{
//==============================================================================
// Gammes
//==============================================================================
enum ScaleId
{
    majorScale = 0, naturalMinor, dorian, phrygian, phrygianDominant, harmonicMinor, melodicMinor,
    lydian, mixolydian, locrian, minorPenta, majorPenta, blues, hungarianMinor, doubleHarmonic,
    hirajoshi, chromatic, numScales
};

struct ScaleInfo
{
    const char* name;     // UTF-8
    const char* mood;     // étiquette d'ambiance affichée
    int size;
    int8_t steps[12];     // intervalles depuis la tonique, croissants
};

inline const ScaleInfo& scaleInfo (int s) noexcept
{
    static const ScaleInfo table[numScales] = {
        { "Majeur",              "lumineux",                    7, { 0, 2, 4, 5, 7, 9, 11 } },
        { "Mineur naturel",      "sombre, la base techno",      7, { 0, 2, 3, 5, 7, 8, 10 } },
        { "Dorien",              "deep, m\xc3\xa9lancolique et chaud", 7, { 0, 2, 3, 5, 7, 9, 10 } },
        { "Phrygien",            "hypnotique, oriental",        7, { 0, 1, 3, 5, 7, 8, 10 } },
        { "Phrygien dominant",   "mental, tension",             7, { 0, 1, 4, 5, 7, 8, 10 } },
        { "Mineur harmonique",   "dramatique",                  7, { 0, 2, 3, 5, 7, 8, 11 } },
        { "Mineur m\xc3\xa9lodique", "jazzy",                   7, { 0, 2, 3, 5, 7, 9, 11 } },
        { "Lydien",              "a\xc3\xa9rien",               7, { 0, 2, 4, 6, 7, 9, 11 } },
        { "Mixolydien",          "funky, jackin'",              7, { 0, 2, 4, 5, 7, 9, 10 } },
        { "Locrien",             "instable",                    7, { 0, 1, 3, 5, 6, 8, 10 } },
        { "Pentatonique mineure", "aucune fausse note",         5, { 0, 3, 5, 7, 10 } },
        { "Pentatonique majeure", "aucune fausse note",         5, { 0, 2, 4, 7, 9 } },
        { "Blues",               "gras, bluesy",                6, { 0, 3, 5, 6, 7, 10 } },
        { "Mineure hongroise",   "dark",                        7, { 0, 2, 3, 6, 7, 8, 11 } },
        { "Double harmonique",   "exotique",                    7, { 0, 1, 4, 5, 7, 8, 11 } },
        { "Hirajoshi",           "hypnotique",                  5, { 0, 2, 3, 7, 8 } },
        { "Chromatique",         "libre, toutes les notes",    12, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 } },
    };
    return table[std::clamp (s, 0, (int) numScales - 1)];
}

inline int scaleSize (int s) noexcept { return scaleInfo (s).size; }

constexpr int floorDiv (int a, int b) noexcept { return a >= 0 ? a / b : -((-a + b - 1) / b); }
constexpr int mod12 (int a) noexcept { return ((a % 12) + 12) % 12; }

/** Intervalle (en demi-tons, depuis la tonique) du degré 'degree' (0 = tonique). Un degré au-delà de la taille
    de la gamme continue à l'octave suivante ; un degré négatif descend. */
inline int degreeInterval (int s, int degree) noexcept
{
    const auto& sc = scaleInfo (s);
    const int oct = floorDiv (degree, sc.size);
    return sc.steps[degree - oct * sc.size] + 12 * oct;
}

/** Masque 12 bits des classes de hauteur de la gamme (bit 0 = C). */
inline uint16_t scaleMask (int s, int rootPc) noexcept
{
    const auto& sc = scaleInfo (s);
    uint16_t m = 0;
    for (int i = 0; i < sc.size; ++i)
        m = (uint16_t) (m | (1u << (unsigned) mod12 (rootPc + sc.steps[i])));
    return m;
}

inline bool inScale (int s, int rootPc, int pitch) noexcept { return ((unsigned) scaleMask (s, rootPc) >> (unsigned) mod12 (pitch)) & 1u; }

/** Indice du degré (0..taille-1) d'une classe de hauteur, ou -1 si elle est hors de la gamme. */
inline int degreeOfPc (int s, int rootPc, int pc) noexcept
{
    const auto& sc = scaleInfo (s);
    const int rel = mod12 (pc - rootPc);
    for (int i = 0; i < sc.size; ++i)
        if (sc.steps[i] == rel)
            return i;
    return -1;
}

inline bool hasInterval (int s, int semis) noexcept
{
    const auto& sc = scaleInfo (s);
    for (int i = 0; i < sc.size; ++i)
        if (sc.steps[i] == semis)
            return true;
    return false;
}

/** Gamme « mineure » : tierce mineure sans tierce majeure. */
inline bool isMinorish (int s) noexcept { return hasInterval (s, 3) && ! hasInterval (s, 4); }

/** Mode parallèle pour les accords empruntés : majeur si la gamme est mineure, mineur naturel sinon. */
inline int parallelScale (int s) noexcept { return isMinorish (s) ? (int) majorScale : (int) naturalMinor; }

//==============================================================================
/** Que faire d'une note hors de la gamme. */
enum OutOfScale { oosNearest = 0, oosUp, oosDown, oosIgnore };

/** Ramène une note dans la gamme. Renvoie -1 si la note est ignorée. Égalité (plus proche) : vers le haut. */
inline int quantize (int s, int rootPc, int note, int policy) noexcept
{
    const auto mask = scaleMask (s, rootPc);
    auto ok = [mask] (int n) { return n >= 0 && n <= 127 && (((unsigned) mask >> (unsigned) mod12 (n)) & 1u); };
    if (ok (note)) return note;

    switch (policy)
    {
        case oosUp:
            for (int d = 1; d < 12; ++d) if (ok (note + d)) return note + d;
            for (int d = 1; d < 12; ++d) if (ok (note - d)) return note - d;   // tout en haut du clavier
            return -1;
        case oosDown:
            for (int d = 1; d < 12; ++d) if (ok (note - d)) return note - d;
            for (int d = 1; d < 12; ++d) if (ok (note + d)) return note + d;
            return -1;
        case oosNearest:
            for (int d = 1; d < 12; ++d)
            {
                if (ok (note + d)) return note + d;
                if (ok (note - d)) return note - d;
            }
            return -1;
        default:
            return -1;
    }
}

//==============================================================================
// Accords
//==============================================================================
enum ChordType
{
    ctPower = 0, ctTriad, ctSus2, ctSus4, ctSix, ctSeventh, ctNinth, ctAdd9, ctEleventh, ctThirteenth,
    ctQuartal, ctCluster, numChordTypes
};

inline const char* chordTypeName (int t) noexcept
{
    static const char* names[numChordTypes] = { "Quinte", "Triade", "Sus2", "Sus4", "6", "7", "9", "add9",
                                                "11", "13", "Quartes", "Cluster" };
    return names[std::clamp (t, 0, (int) numChordTypes - 1)];
}

/** Rôle de chaque note dans l'accord : sert à choisir quelles notes retirer quand on limite le nombre de voix. */
enum Role : uint8_t { rRoot = 0, rThird, rFifth, rSixth, rSeventh, rNinth, rEleventh, rThirteenth, rSus, rDouble, rOther };

constexpr int maxChordTones = 8;

/** Accord en position fondamentale serrée : décalages en demi-tons depuis la fondamentale (0 en premier), croissants. */
struct ChordShape
{
    int8_t  offsets[maxChordTones] {};
    uint8_t roles[maxChordTones] {};
    int count = 0;
    bool orderedRemoval = false;   // quartes, cluster, accord appris : on retire les notes par le haut

    bool add (int offset, Role role) noexcept
    {
        if (count >= maxChordTones) return false;
        for (int i = 0; i < count; ++i)
            if (mod12 (offsets[i]) == mod12 (offset) && role != rDouble)
                return false;   // même classe de hauteur déjà présente
        offsets[count] = (int8_t) offset;
        roles[count] = role;
        ++count;
        return true;
    }

    void sort() noexcept
    {
        for (int i = 1; i < count; ++i)
            for (int j = i; j > 0 && offsets[j] < offsets[j - 1]; --j)
            {
                std::swap (offsets[j], offsets[j - 1]);
                std::swap (roles[j], roles[j - 1]);
            }
    }
};

/**
    Accord construit en empilant les notes de la gamme, sur le degré 'degree' (0 = tonique, peut dépasser la taille).

    - gammes de 7 notes : empilement classique par pas de la gamme (tierces diatoniques) ;
    - autres gammes (pentatoniques, blues, hirajoshi, chromatique) : pour chaque note de l'accord (tierce, quinte...)
      on prend le premier intervalle d'une liste de préférence qui tombe dans la gamme ; sinon la note est omise.
    Dans tous les cas, toutes les notes sont dans la gamme.
*/
inline ChordShape buildScaleChord (int s, int degree, int type) noexcept
{
    ChordShape c;
    const auto& sc = scaleInfo (s);
    const int rootI = degreeInterval (s, degree);
    type = std::clamp (type, 0, (int) numChordTypes - 1);

    if (type == ctCluster)
    {
        for (int k = 0; k < 4; ++k)
            c.add (degreeInterval (s, degree + k) - rootI, k == 0 ? rRoot : rOther);
        c.orderedRemoval = true;
        return c;
    }

    if (sc.size == 7)
    {
        struct Def { int n; int8_t steps[7]; Role roles[7]; };
        static const Def defs[numChordTypes] = {
            { 3, { 0, 4, 7 },             { rRoot, rFifth, rDouble } },
            { 3, { 0, 2, 4 },             { rRoot, rThird, rFifth } },
            { 3, { 0, 1, 4 },             { rRoot, rSus, rFifth } },
            { 3, { 0, 3, 4 },             { rRoot, rSus, rFifth } },
            { 4, { 0, 2, 4, 5 },          { rRoot, rThird, rFifth, rSixth } },
            { 4, { 0, 2, 4, 6 },          { rRoot, rThird, rFifth, rSeventh } },
            { 5, { 0, 2, 4, 6, 8 },       { rRoot, rThird, rFifth, rSeventh, rNinth } },
            { 4, { 0, 2, 4, 8 },          { rRoot, rThird, rFifth, rNinth } },
            { 6, { 0, 2, 4, 6, 8, 10 },   { rRoot, rThird, rFifth, rSeventh, rNinth, rEleventh } },
            { 7, { 0, 2, 4, 6, 8, 10, 12 }, { rRoot, rThird, rFifth, rSeventh, rNinth, rEleventh, rThirteenth } },
            { 4, { 0, 3, 6, 9 },          { rRoot, rOther, rOther, rOther } },
            { 0, {}, {} },
        };
        const auto& d = defs[type];
        for (int k = 0; k < d.n; ++k)
            c.add (degreeInterval (s, degree + d.steps[k]) - rootI, d.roles[k]);
        c.orderedRemoval = type == ctQuartal;
        c.sort();
        return c;
    }

    // --- gammes de taille différente de 7 : listes de préférence
    const int rootPc = rootI;   // relatif à la tonique
    auto pick = [&] (std::initializer_list<int> prefs, int from) -> int
    {
        for (int iv : prefs)
            if (hasInterval (s, mod12 (rootPc + from + iv)))
                return from + iv;
        return -1000;
    };
    auto addPick = [&] (std::initializer_list<int> prefs, Role role)
    {
        const int o = pick (prefs, 0);
        if (o > -1000) c.add (o, role);
    };

    c.add (0, rRoot);
    switch (type)
    {
        case ctPower:
            addPick ({ 7, 6, 8 }, rFifth);
            c.add (12, rDouble);
            break;
        case ctSus2:
            addPick ({ 2, 1 }, rSus);
            addPick ({ 7, 6, 8 }, rFifth);
            break;
        case ctSus4:
            addPick ({ 5, 6 }, rSus);
            addPick ({ 7, 6, 8 }, rFifth);
            break;
        case ctQuartal:
        {
            int from = 0;
            for (int k = 0; k < 3; ++k)
            {
                const int o = pick ({ 5, 6, 4 }, from);
                if (o <= -1000) break;
                c.add (o, rOther);
                from = o;
            }
            c.orderedRemoval = true;
            break;
        }
        default:
        {
            // tierce (ou note suspendue si la gamme n'a pas de tierce sur ce degré), quinte, puis extensions
            const int third = pick ({ 3, 4 }, 0);
            if (third > -1000) c.add (third, rThird);
            else addPick ({ 5, 2 }, rSus);
            addPick ({ 7, 6, 8 }, rFifth);
            if (type == ctSix)        addPick ({ 9, 8 }, rSixth);
            if (type >= ctSeventh && type != ctAdd9) addPick ({ 10, 11, 9 }, rSeventh);
            if (type == ctNinth || type == ctAdd9 || type >= ctEleventh) addPick ({ 14, 13, 15 }, rNinth);
            if (type >= ctEleventh)   addPick ({ 17, 18, 16 }, rEleventh);
            if (type == ctThirteenth) addPick ({ 21, 20, 22 }, rThirteenth);
            break;
        }
    }
    c.sort();
    return c;
}

//==============================================================================
// Formes d'accord fixes (mode « un doigt »)
//==============================================================================
enum { numFixedShapes = 23, fixedLearned = 22 };

inline const char* fixedShapeName (int i) noexcept
{
    static const char* names[numFixedShapes] = {
        "Majeur", "Mineur", "7", "m7", "maj7", "m9", "9", "maj9", "m11", "sus2", "sus4", "add9", "m(add9)",
        "6", "m6", "Quinte", "dim", "aug", "m7b5", "7sus4", "13", "Quartes", "Appris"
    };
    return names[std::clamp (i, 0, numFixedShapes - 1)];
}

/** Rôle d'une note d'un accord quelconque (formes fixes, accord appris), devinée à partir de son intervalle. */
inline Role classifyOffset (int o, bool hasThird, bool hasSeventh) noexcept
{
    if (o == 0) return rRoot;
    const int pc = mod12 (o);
    if (pc == 0) return rDouble;
    switch (pc)
    {
        case 3: case 4: return o < 12 ? rThird : rNinth;
        case 7: return rFifth;
        case 6: return hasThird && ! hasSeventh ? rFifth : rEleventh;
        case 8: return hasSeventh ? rThirteenth : rFifth;
        case 9: return o < 12 && ! hasSeventh ? rSixth : rThirteenth;
        case 10: case 11: return rSeventh;
        case 1: case 2: return o < 12 && ! hasThird ? rSus : rNinth;
        case 5: return o < 12 && ! hasThird ? rSus : rEleventh;
        default: return rOther;
    }
}

inline ChordShape shapeFromIntervals (const int8_t* iv, int n, bool ordered) noexcept
{
    bool third = false, seventh = false;
    for (int i = 0; i < n; ++i)
    {
        third   = third   || iv[i] == 3 || iv[i] == 4;
        seventh = seventh || mod12 (iv[i]) == 10 || mod12 (iv[i]) == 11;
    }
    ChordShape c;
    for (int i = 0; i < n; ++i)
        c.add (iv[i], classifyOffset (iv[i], third, seventh));
    c.orderedRemoval = ordered;
    c.sort();
    return c;
}

inline ChordShape fixedShape (int i) noexcept
{
    struct Def { int n; int8_t iv[6]; };
    static const Def defs[numFixedShapes - 1] = {
        { 3, { 0, 4, 7 } },  { 3, { 0, 3, 7 } },  { 4, { 0, 4, 7, 10 } },  { 4, { 0, 3, 7, 10 } },  { 4, { 0, 4, 7, 11 } },
        { 5, { 0, 3, 7, 10, 14 } },  { 5, { 0, 4, 7, 10, 14 } },  { 5, { 0, 4, 7, 11, 14 } },  { 6, { 0, 3, 7, 10, 14, 17 } },
        { 3, { 0, 2, 7 } },  { 3, { 0, 5, 7 } },  { 4, { 0, 4, 7, 14 } },  { 4, { 0, 3, 7, 14 } },
        { 4, { 0, 4, 7, 9 } },  { 4, { 0, 3, 7, 9 } },  { 3, { 0, 7, 12 } },  { 3, { 0, 3, 6 } },  { 3, { 0, 4, 8 } },
        { 4, { 0, 3, 6, 10 } },  { 4, { 0, 5, 7, 10 } },  { 6, { 0, 4, 7, 10, 14, 21 } },  { 4, { 0, 5, 10, 15 } },
    };
    i = std::clamp (i, 0, numFixedShapes - 2);
    return shapeFromIntervals (defs[i].iv, defs[i].n, i == 21);
}

//==============================================================================
// Noms
//==============================================================================
/** Bémols ou dièses pour nommer les notes, selon l'armure de la tonalité. */
inline bool useFlats (int s, int rootPc) noexcept
{
    const int relMajor = isMinorish (s) ? mod12 (rootPc + 3) : mod12 (rootPc);
    return relMajor == 5 || relMajor == 10 || relMajor == 3 || relMajor == 8 || relMajor == 1 || relMajor == 6;
}

inline const char* pcName (int pc, bool flats) noexcept
{
    static const char* sharps[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    static const char* flat[12]   = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };
    return (flats ? flat : sharps)[mod12 (pc)];
}

/** Nom de note façon Ableton (C3 = 60). */
inline void noteName (int note, char* out, int outSize, bool flats = false) noexcept
{
    std::snprintf (out, (size_t) outSize, "%s%d", pcName (note, flats), floorDiv (note, 12) - 2);
}

/** Qualité d'un accord, déduite de ses intervalles. */
struct Quality
{
    bool minor = false, major = false, dim = false, aug = false, sus = false, power = false;
};

inline Quality qualityOf (uint16_t ivMask) noexcept
{
    auto has = [ivMask] (int i) { return ((ivMask >> i) & 1u) != 0; };
    Quality q;
    const bool maj3 = has (4), min3 = has (3) && ! has (4);
    if (maj3) q.major = true;
    if (min3) q.minor = true;
    if (! maj3 && ! min3) q.sus = true;
    if (min3 && has (6) && ! has (7)) { q.dim = true; q.minor = true; }
    if (maj3 && has (8) && ! has (7)) q.aug = true;
    if (! has (3) && ! has (4) && ! has (2) && ! has (5) && has (7)) q.power = true;
    return q;
}

/**
    Nom d'accord à partir de la fondamentale et des notes (ex. « Am9 », « Fmaj7 », « Bm7b5 », « Gsus4 »).
    Les octaves et l'ordre des notes ne comptent pas.
*/
inline void chordName (int rootPc, const int* notes, int n, bool flats, char* out, int outSize) noexcept
{
    uint16_t m = 0;
    for (int i = 0; i < n; ++i)
        m = (uint16_t) (m | (1u << (unsigned) mod12 (notes[i] - rootPc)));
    m = (uint16_t) (m | 1u);
    auto has = [m] (int i) { return ((m >> i) & 1u) != 0; };

    char q[32] = {};
    auto cat = [&q] (const char* s) { std::strncat (q, s, sizeof (q) - std::strlen (q) - 1); };

    const bool maj3 = has (4), min3 = has (3) && ! maj3;
    const bool sharp9 = has (3) && maj3;
    const bool p5 = has (7);
    const bool b5 = has (6) && ! p5 && min3;
    const bool s5 = has (8) && ! p5 && maj3;
    const bool dim = b5;
    const bool dim7 = dim && has (9) && ! has (10) && ! has (11);
    const bool s7 = has (10), m7 = has (11) && ! s7;
    const bool sev = s7 || m7 || dim7;
    const bool nine = has (2), b9 = has (1);
    const bool third = maj3 || min3;
    const bool eleven = has (5) && third;
    const bool s11 = has (6) && (p5 || ! min3) && ! b5;
    const bool thirteen = has (9) && sev && ! dim7;
    const bool six = has (9) && ! sev;
    const bool b13 = has (8) && (p5 || min3) && ! s5;

    if (! third && ! has (2) && ! has (5) && p5 && ! sev && m == ((1u << 0) | (1u << 7)))
    {
        std::snprintf (out, (size_t) outSize, "%s5", pcName (rootPc, flats));
        return;
    }

    // qualité + septième + extension la plus haute
    if (dim7)               cat ("dim7");
    else if (dim && s7)     cat ("m7b5");
    else if (dim)           cat ("dim");
    else
    {
        if (min3) cat ("m");
        if (s5 && ! sev) cat ("aug");

        const char* ext = thirteen ? "13" : (eleven && sev) ? "11" : (nine && sev) ? "9" : sev ? "7" : nullptr;
        if (m7)
        {
            cat (min3 ? "Maj" : "maj");
            cat (ext);
        }
        else if (ext != nullptr)
            cat (ext);

        if (s5 && sev) cat ("#5");

        if (six) { cat ("6"); if (nine) cat ("/9"); }
        if (! third)
        {
            if (has (5))      cat ("sus4");
            else if (nine)    cat ("sus2");
        }
    }

    // notes ajoutées (add9, add11) puis altérations (b9, #9, #11, b13), toujours entre parenthèses
    char add[24] = {}, alt[24] = {};
    auto listCat = [] (char* list, size_t size, const char* s)
    {
        if (list[0] != 0) std::strncat (list, ",", size - std::strlen (list) - 1);
        std::strncat (list, s, size - std::strlen (list) - 1);
    };
    if (! sev && nine && ! six && third)  listCat (add, sizeof (add), "add9");
    if (! sev && ! third && has (5) && nine) listCat (add, sizeof (add), "add9");
    if (! sev && eleven)                  listCat (add, sizeof (add), "add11");
    if (b9)                               listCat (alt, sizeof (alt), "b9");
    if (sharp9)                           listCat (alt, sizeof (alt), "#9");
    if (s11)                              listCat (alt, sizeof (alt), "#11");
    if (b13)                              listCat (alt, sizeof (alt), sev ? "b13" : "b6");

    if (add[0] != 0)
    {
        const bool paren = q[0] != 0;
        if (paren) cat ("(");
        cat (add);
        if (paren) cat (")");
    }
    if (alt[0] != 0)
    {
        cat ("(");
        cat (alt);
        cat (")");
    }
    std::snprintf (out, (size_t) outSize, "%s%s", pcName (rootPc, flats), q);
}

/** Chiffrage romain du degré : majuscule si majeur, minuscule si mineur, ° si diminué, + si augmenté. */
inline void romanNumeral (int s, int rootPcKey, int chordRootPc, uint16_t chordIvMask, char* out, int outSize) noexcept
{
    static const char* up[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    static const char* lo[7] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };
    const auto q = qualityOf (chordIvMask);
    const bool lower = q.minor || q.dim;
    const char* suffix = q.dim ? "\xc2\xb0" : (q.aug ? "+" : "");

    const int deg = degreeOfPc (s, rootPcKey, chordRootPc);
    if (scaleSize (s) == 7 && deg >= 0)
    {
        std::snprintf (out, (size_t) outSize, "%s%s", (lower ? lo : up)[deg], suffix);
        return;
    }
    // autres gammes (ou accord hors gamme) : chiffrage par intervalle depuis la tonique (bIII, bVII...)
    static const int8_t numeral[12] = { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 };
    static const char* acc[12] = { "", "b", "", "b", "", "", "b", "", "b", "", "b", "" };
    const int iv = mod12 (chordRootPc - rootPcKey);
    std::snprintf (out, (size_t) outSize, "%s%s%s", acc[iv], (lower ? lo : up)[numeral[iv]], suffix);
}
} // namespace Theory
