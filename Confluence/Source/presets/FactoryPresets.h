// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <vector>
#include <utility>

//==============================================================================
/**
    Presets d'usine de Confluence. Chaque preset contient les réglages, les 8 pads et, si besoin, l'accord appris.

    Pads : 8 jetons « degré/type[/renversement][/octave] »
        degré        1 à 7 (degré de la gamme)
        type         5 (quinte), t (triade), s2, s4, 6, 7, 9, a9, 11, 13, q (quartes), c (cluster)
        renversement 0 à 3, ou a (Auto, par défaut)
        octave       -2 à 2 (0 par défaut)

    Démo : la phrase de notes simples jouée pour les rendus (.mid et WAV de 4 mesures), « note:début:durée » en noires.
    Les réglages non cités gardent leur valeur par défaut. Les presets ne touchent jamais au canal de sortie,
    à la voix Preview ni à la note de départ des pads (ce sont tes réglages de routage et de contrôleur).
*/
struct FactoryPreset
{
    const char* name;
    const char* bank;
    int bpm;
    const char* description;
    std::vector<std::pair<const char*, float>> values;
    const char* pads;
    const char* learned;   // intervalles de l'accord appris, ou nullptr
    const char* demo;
};

inline const std::vector<FactoryPreset>& getFactoryPresetData()
{
    // valeurs : indices des choix (voir Params.h) ou valeurs réelles
    //   mode 0 Gamme, 1 Touches blanches, 2 Accord fixe, 3 Pads
    //   scale 0 majeur, 1 mineur naturel, 2 dorien, 3 phrygien, 4 phrygien dominant, 5 mineur harmonique...
    //   chordType 0 quinte, 1 triade, 2 sus2, 3 sus4, 4 6, 5 7, 6 9, 7 add9, 8 11, 9 13, 10 quartes, 11 cluster
    //   inversion 4 = Auto ; spread 0 serré, 1 ouvert, 2 drop 2, 3 drop 3, 4 large ; bass 0 off, 1 -1 oct, 2 -2 oct
    //   strumDir 0 haut, 1 bas, 2 alterné, 3 aléatoire ; velMode 0 suit, 1 fixe, 2 courbe ; durMode 0 tenue, 1 fixe
    //   duration 0 1/32, 1 1/16T, 2 1/16, 3 1/16., 4 1/8T, 5 1/8, 6 1/8., 7 1/4, 8 1/2, 9 1/1
    static const std::vector<FactoryPreset> presets = {
        //======================================================================
        // DEEP & HOUSE (118-124 BPM : chaud, jazzy, 7e et 9e)
        //======================================================================
        { "Warm Current", "DEEP & HOUSE", 122, "m9 en dorien, voicing ouvert, strum 15 ms vers le haut",
          { { "mode", 0 }, { "root", 2 }, { "scale", 2 }, { "chordType", 6 }, { "style", 0 },
            { "inversion", 4 }, { "spread", 1 }, { "maxVoices", 5 }, { "lowNote", 48 }, { "highNote", 86 },
            { "strum", 15.0f }, { "strumDir", 0 }, { "humanTime", 4.0f }, { "humanVel", 6.0f }, { "balTop", 1.1f } },
          "1/9 4/9 7/7 5/7 1/9 3/7 4/9 1/11", nullptr,
          "D3:0:2 G2:2:2 C3:4:2 A2:6:2 F2:8:2 G2:10:2 D3:12:4" },

        { "Lagoon Keys", "DEEP & HOUSE", 120, "maj7 et m7 en majeur, voice leading Auto, basse a l'octave",
          { { "mode", 0 }, { "root", 5 }, { "scale", 0 }, { "chordType", 5 }, { "style", 0 },
            { "inversion", 4 }, { "spread", 0 }, { "maxVoices", 4 }, { "bass", 1 }, { "lowNote", 52 }, { "highNote", 79 },
            { "humanTime", 3.0f }, { "humanVel", 8.0f }, { "balBass", 0.85f } },
          "2/7 5/7 1/7 6/7 4/7 3/7 2/9 5/7", nullptr,
          "G2:0:4 C3:4:4 F2:8:4 D3:12:4" },

        //======================================================================
        // HYPNOTIC & DUB (126-134 BPM : accords dub, suspendus, sombres)
        //======================================================================
        { "Dub Chamber", "HYPNOTIC & DUB", 128, "m7 dub serre dans le medium, courts, pense pour un delay et une reverb",
          { { "mode", 0 }, { "root", 0 }, { "scale", 1 }, { "chordType", 5 }, { "style", 1 },
            { "inversion", 4 }, { "spread", 0 }, { "maxVoices", 4 }, { "lowNote", 55 }, { "highNote", 74 },
            { "durMode", 1 }, { "duration", 6 }, { "velMode", 1 }, { "velFixed", 96 }, { "humanTime", 2.0f } },
          "1/7 1/7 6/7 7/7 1/7 4/7 1/9 7/7", nullptr,
          "C3:0.5:0.5 C3:1.5:0.5 C3:2.75:0.5 C3:3.5:0.5 C3:4.5:0.5 C3:5.5:0.5 C3:6.75:0.5 C3:7.5:0.5 "
          "G#2:8.5:0.5 G#2:9.5:0.5 G#2:10.75:0.5 A#2:11.5:0.5 C3:12.5:0.5 C3:13.5:0.5 C3:14.75:0.5 A#2:15.5:0.5" },

        { "Fog Bank", "HYPNOTIC & DUB", 130, "sus2 et sus4 en mineur naturel (touches noires = sus4), entre majeur et mineur",
          { { "mode", 1 }, { "root", 9 }, { "scale", 1 }, { "chordType", 2 }, { "blackKeys", 0 }, { "style", 1 },
            { "inversion", 4 }, { "spread", 1 }, { "maxVoices", 4 }, { "bass", 1 }, { "lowNote", 50 }, { "highNote", 84 },
            { "strum", 30.0f }, { "strumDir", 0 }, { "humanTime", 6.0f }, { "humanVel", 5.0f } },
          "1/s2 1/s4 6/s2 7/s4 1/s2 4/s2 3/s4 7/s2", nullptr,
          "C3:0:4 C#3:4:2 C3:6:2 A3:8:4 B3:12:2 A#3:14:2" },

        //======================================================================
        // TECH HOUSE & STABS (124-130 BPM : courts, punchy, groovy)
        //======================================================================
        { "Splash Stab", "TECH HOUSE & STABS", 126, "m7 serre, duree fixe 1/16, velocity accentuee",
          { { "mode", 0 }, { "root", 5 }, { "scale", 1 }, { "chordType", 5 }, { "style", 2 },
            { "inversion", 4 }, { "spread", 0 }, { "maxVoices", 4 }, { "lowNote", 53 }, { "highNote", 77 },
            { "durMode", 1 }, { "duration", 2 }, { "velMode", 2 }, { "velCurve", 0.5f }, { "balTop", 1.15f }, { "humanTime", 2.0f } },
          "1/7 1/7 4/7 5/7 1/7 7/7 6/7 7/7", nullptr,
          "F2:0.5:0.25 F2:1.5:0.25 F2:2.5:0.25 F2:3.5:0.25 F2:3.75:0.25 F2:4.5:0.25 F2:5.5:0.25 F2:6.5:0.25 F2:7.5:0.25 "
          "C#3:8.5:0.25 C#3:9.5:0.25 C#3:10.5:0.25 C#3:11.5:0.25 D#3:12.5:0.25 D#3:13.5:0.25 D#3:14.5:0.25 D#3:15.5:0.25 D#3:15.75:0.25" },

        { "Rave Tide", "TECH HOUSE & STABS", 128, "accord fixe mineur 7 facon stab rave, duree 1/8",
          { { "mode", 2 }, { "root", 9 }, { "scale", 1 }, { "fixedShape", 3 }, { "style", 2 },
            { "inversion", 0 }, { "spread", 0 }, { "maxVoices", 4 }, { "lowNote", 48 }, { "highNote", 84 },
            { "durMode", 1 }, { "duration", 5 }, { "velMode", 1 }, { "velFixed", 110 } },
          "1/7 6/7 4/7 5/7 1/7 6/7 3/7 7/7", nullptr,
          "A2:0:0.5 A2:0.75:0.5 A2:1.5:0.5 A2:2.5:0.5 A2:3:0.5 A2:4:0.5 A2:4.75:0.5 A2:5.5:0.5 A2:6.5:0.5 A2:7:0.5 "
          "F2:8:0.5 F2:8.75:0.5 F2:9.5:0.5 F2:10.5:0.5 G2:12:0.5 G2:12.75:0.5 G2:13.5:0.5 G2:14.5:0.5 G2:15:0.5" },
    };
    return presets;
}
