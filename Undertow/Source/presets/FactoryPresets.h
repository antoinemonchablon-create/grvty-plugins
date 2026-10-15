// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <vector>
#include <utility>

//==============================================================================
/**
    Presets d'usine d'Undertow : chaque preset contient le son ET le motif.

    Motif : un mot par pas, séparés par des espaces.
      .        pas éteint
      7        note (demi-tons au-dessus de la tonique ; 12 = octave du dessus, -5 = en dessous)
      suffixes : !  accent        ~  slide vers le pas suivant
                 x3 ratchet       ?60  probabilité 60 %
                 g30 gate 30 %    v70  vélocité 70 %
                 i6.5 p-lock Index       c1200 p-lock Cutoff (Hz)
      directives au début : @g30 (gate par défaut), @v80 (vélocité par défaut)
    Piste Mod : une valeur par pas, de -1 à 1.

    Valeurs : valeur réelle du paramètre (index pour les menus, 0/1 pour les interrupteurs).
    Les paramètres non listés reprennent leur valeur par défaut.

    Repères des menus
      oscBRatio   0=0.5 1=1 2=1.5 3=2 4=2.5 5=3 6=3.5 7=4 8=5 9=6 10=7 11=8
      lfoDest     0=Off 1=Pitch 2=Index 3=Ratio 4=Feedback 5=Cutoff 6=Reso 7=ShapeA 8=ShapeB 9=Mix 10=Drive 11=Amp 12=Pan
      lfoShape    0=Sine 1=Tri 2=Ramp 3=Square 4=S&H
      lfoSyncRate 0=1/32 1=1/16T 2=1/16 3=1/8T 4=1/8 5=3/16 6=1/4T 7=1/4 8=5/16 9=7/16 10=3/8 11=1/2 12=3/4 13=1bar 14=2bars 15=4bars 16=8bars 17=16bars
      delayTime   0=1/32 1=1/16T 2=1/16 3=1/8T 4=1/8 5=3/16 6=1/4T 7=1/4 8=3/8 9=1/2
      scale       0=Minor 1=Phrygian 2=Dorian 3=HarmMinor 4=MinorPenta 5=Chromatic
      rootNote    0=C 2=D 4=E 5=F 7=G 9=A
      seqRate     0=1/8 1=1/16 2=1/32 3=1/8T 4=1/16T 5=1/32T
      modLaneDest 0=Off 1=Index 2=Cutoff 3=Ratio
      mutateBars  0=1 1=2 2=4 3=8 4=16 mesures
*/
struct FactoryPreset
{
    const char* name;
    const char* bank;
    const char* type;      // PERC, BASS, SYNTH, FX, BLEEP ou AMBIENT
    double      bpm;       // tempo de référence (rendus, notes) ; le plugin suit toujours le tempo d'Ableton
    std::vector<std::pair<const char*, float>> values;
    const char* pattern;
    const char* lane;
};

inline const std::vector<FactoryPreset>& getFactoryPresetData()
{
    static const std::vector<FactoryPreset> presets
    {
        //======================================================================
        // HARDGROOVE : percussif, rebondissant, tribal, métallique (135 BPM) — 13 presets
        //======================================================================
        { "Breakwater", "HARDGROOVE", "SYNTH", 135.0,
          { { "oscAShape", 0.08f }, { "oscBRatio", 6 }, { "fmIndex", 0.8f }, { "fmFeedback", 0.12f },
            { "modAttack", 0.3f }, { "modDecay", 75.0f }, { "modCurve", -0.6f }, { "modToIndex", 6.5f }, { "modToCutoff", 18.0f },
            { "filterType", 0 }, { "cutoff", 3200.0f }, { "resonance", 0.22f }, { "keyTrack", 0.4f },
            { "ampAttack", 0.3f }, { "ampDecay", 170.0f }, { "ampSustain", 0.0f }, { "ampRelease", 35.0f },
            { "delayTime", 5 }, { "delayFeedback", 0.32f }, { "delayTone", 0.35f }, { "delayMix", 0.16f },
            { "reverbSize", 0.45f }, { "reverbMix", 0.08f },
            { "scale", 0 }, { "rootNote", 5 }, { "seqOctave", 0 }, { "seqLength", 16 }, { "swing", 0.54f }, { "accent", 0.7f },
            { "outGain", -5.5f } },
          "@g35 . . 0! . . 0 . 0 . . 0! . 7 . 0 3", nullptr },

        { "Rolling Surf", "HARDGROOVE", "BASS", 135.0,
          { { "oscBRatio", 1 }, { "fmIndex", 0.6f }, { "fmFeedback", 0.22f },
            { "modAttack", 0.5f }, { "modDecay", 140.0f }, { "modCurve", -0.3f }, { "modToIndex", 3.5f },
            { "filterType", 0 }, { "cutoff", 700.0f }, { "resonance", 0.3f }, { "filterEnv", 26.0f }, { "filterDrive", 0.25f }, { "keyTrack", 0.3f },
            { "ampAttack", 0.5f }, { "ampDecay", 170.0f }, { "ampSustain", 0.35f }, { "ampRelease", 40.0f },
            { "glide", 70.0f }, { "legato", 1 },
            { "fxDrive", 0.2f },
            { "scale", 0 }, { "rootNote", 5 }, { "seqOctave", -2 }, { "seqLength", 16 }, { "swing", 0.55f }, { "accent", 0.6f },
            { "outGain", -8.6f } },
          "@g55 . 0 12! 0 . 0 12~ 0 . 0 12! 0 . 3 12~ 0", nullptr },

        { "Steel Tide", "HARDGROOVE", "PERC", 135.0,
          { { "oscBFree", 1 }, { "oscBFreeRatio", 1.41f }, { "fmIndex", 2.2f }, { "fmFeedback", 0.3f },
            { "modAttack", 0.2f }, { "modDecay", 45.0f }, { "modCurve", -0.8f }, { "modToIndex", 4.5f },
            { "filterType", 2 }, { "cutoff", 320.0f }, { "resonance", 0.15f },
            { "ampAttack", 0.2f }, { "ampDecay", 90.0f }, { "ampSustain", 0.0f }, { "ampRelease", 45.0f },
            { "fxDrive", 0.4f }, { "delayTime", 2 }, { "delayFeedback", 0.25f }, { "delayTone", 0.5f }, { "delayMix", 0.12f },
            { "reverbSize", 0.5f }, { "reverbMix", 0.14f },
            { "scale", 0 }, { "rootNote", 7 }, { "seqOctave", 1 }, { "seqLength", 12 }, { "swing", 0.52f }, { "accent", 0.8f },
            { "modLaneDest", 1 }, { "modLaneLength", 16 }, { "modLaneDepth", 0.35f },
            { "outGain", -4.3f } },
          "@g25 0! . 0 0 . 0 7 . 0! . 0 10",
          "0.6 -0.2 0.1 -0.5 0.9 -0.1 0.3 -0.7 0.4 0 -0.4 0.8 -0.3 0.2 -0.6 0.5" },

        { "Tribal Current", "HARDGROOVE", "PERC", 135.0,
          { { "oscBRatio", 1 }, { "fmIndex", 0.4f }, { "fmFeedback", 0.05f },
            { "modAttack", 0.2f }, { "modDecay", 55.0f }, { "modCurve", -0.7f }, { "modToIndex", 1.5f }, { "modToPitch", 16.0f },
            { "filterType", 0 }, { "cutoff", 3000.0f }, { "resonance", 0.1f },
            { "ampAttack", 0.2f }, { "ampDecay", 280.0f }, { "ampSustain", 0.0f }, { "ampRelease", 60.0f },
            { "fxDrive", 0.3f }, { "reverbSize", 0.35f }, { "reverbMix", 0.12f },
            { "scale", 0 }, { "rootNote", 9 }, { "seqOctave", -1 }, { "seqLength", 16 }, { "swing", 0.56f }, { "accent", 0.6f },
            { "outGain", -6.6f } },
          "@g60 0! . 7 . 3 0 . 10x2 . 0 3 . 7! . 0x3 3", nullptr },

        { "Steel Conga", "HARDGROOVE", "PERC", 135.0,
          { { "oscBRatio", 2 }, { "fmIndex", 0.6f },
            { "modAttack", 0.2f }, { "modDecay", 25.0f }, { "modCurve", -0.8f }, { "modToIndex", 3.0f }, { "modToPitch", 7.0f },
            { "filterType", 0 }, { "cutoff", 5000.0f },
            { "ampAttack", 0.2f }, { "ampDecay", 200.0f }, { "ampSustain", 0.0f }, { "ampRelease", 50.0f },
            { "reverbSize", 0.3f }, { "reverbMix", 0.1f },
            { "scale", 0 }, { "rootNote", 2 }, { "seqOctave", 0 }, { "seqLength", 16 }, { "swing", 0.57f }, { "accent", 0.5f },
            { "mutateAmount", 0.15f }, { "mutateBars", 1 },
            { "outGain", -5.8f } },
          "@g50 . 7 . 12 7 . 12i3.5! 7 . 7 12 . 7i3.5! . 12 7", nullptr },

        { "Iron Bell", "HARDGROOVE", "PERC", 135.0,
          { { "oscAShape", 1.0f }, { "oscBShape", 1.0f }, { "oscMix", 0.5f }, { "fmIndex", 0.0f }, { "fxDrive", 0.35f },
            { "oscBFree", 1 }, { "oscBFreeRatio", 1.48f },
            { "filterType", 1 }, { "cutoff", 900.0f }, { "resonance", 0.35f },
            { "ampAttack", 0.2f }, { "ampDecay", 220.0f }, { "ampSustain", 0.0f }, { "ampRelease", 40.0f },
            { "delayTime", 5 }, { "delayFeedback", 0.3f }, { "delayTone", 0.3f }, { "delayMix", 0.1f },
            { "scale", 5 }, { "rootNote", 0 }, { "seqOctave", 2 }, { "seqLength", 16 }, { "swing", 0.55f }, { "accent", 0.5f },
            { "outGain", -2.2f } },
          "@g30 . . 0 . . . 0 . . 0 . . 0 . 0x2v60 .", nullptr },

        { "Ride Spray", "HARDGROOVE", "PERC", 135.0,
          { { "oscBFree", 1 }, { "oscBFreeRatio", 3.73f }, { "fmIndex", 5.0f }, { "fmFeedback", 0.55f },
            { "modAttack", 0.2f }, { "modDecay", 30.0f }, { "modCurve", -0.8f }, { "modToIndex", 3.0f },
            { "filterType", 2 }, { "cutoff", 5500.0f }, { "resonance", 0.1f },
            { "ampAttack", 0.2f }, { "ampDecay", 240.0f }, { "ampSustain", 0.0f }, { "ampRelease", 80.0f },
            { "velSens", 0.6f }, { "fxDrive", 0.3f },
            { "lfo2Shape", 4 }, { "lfo2Sync", 1 }, { "lfo2SyncRate", 2 }, { "lfo2Dest", 12 }, { "lfo2Depth", 0.45f },
            { "reverbSize", 0.4f }, { "reverbMix", 0.1f },
            { "scale", 5 }, { "rootNote", 0 }, { "seqOctave", 2 }, { "seqLength", 16 }, { "swing", 0.55f }, { "accent", 0.5f },
            { "outGain", -1.7f } },
          "@g40 . 0v45?50 0! 0v45 . 0v45?50 0! 0v45?60 . 0v45?50 0! 0v45 . 0v45?50 0! 0v50", nullptr },

        { "Rimshot Rattle", "HARDGROOVE", "PERC", 135.0,
          { { "oscBFree", 1 }, { "oscBFreeRatio", 2.27f }, { "fmIndex", 1.5f },
            { "modAttack", 0.1f }, { "modDecay", 18.0f }, { "modCurve", -0.8f }, { "modToIndex", 5.0f },
            { "filterType", 1 }, { "cutoff", 2200.0f }, { "resonance", 0.3f },
            { "ampAttack", 0.1f }, { "ampDecay", 70.0f }, { "ampSustain", 0.0f }, { "ampRelease", 30.0f },
            { "fxDrive", 0.35f },
            { "delayTime", 1 }, { "delayFeedback", 0.2f }, { "delayMix", 0.08f },
            { "reverbSize", 0.3f }, { "reverbMix", 0.12f },
            { "scale", 5 }, { "rootNote", 0 }, { "seqOctave", 1 }, { "seqLength", 16 }, { "swing", 0.58f }, { "accent", 0.7f },
            { "mutateAmount", 0.25f }, { "mutateBars", 1 },
            { "outGain", -1.6f } },
          "@g40 0! . 0?60 0 . 0x2 . 0?70 0! . 0?50 0x3 . 0 0?40 .", nullptr },

        { "Rave Hoover", "HARDGROOVE", "SYNTH", 135.0,
          { { "oscAShape", 0.5f }, { "oscBShape", 0.5f }, { "oscMix", 0.45f }, { "oscBFree", 1 }, { "oscBFreeRatio", 1.012f },
            { "fmIndex", 0.7f }, { "fmFeedback", 0.25f },
            { "modAttack", 0.5f }, { "modDecay", 220.0f }, { "modCurve", -0.3f }, { "modToPitch", 7.0f }, { "modToIndex", 1.5f },
            { "glide", 90.0f }, { "legato", 1 },
            { "filterType", 0 }, { "cutoff", 3500.0f }, { "resonance", 0.2f }, { "filterDrive", 0.3f },
            { "ampAttack", 2.0f }, { "ampDecay", 400.0f }, { "ampSustain", 0.5f }, { "ampRelease", 180.0f },
            { "lfo1Shape", 0 }, { "lfo1Sync", 1 }, { "lfo1SyncRate", 4 }, { "lfo1Dest", 1 }, { "lfo1Depth", 0.012f },
            { "delayTime", 5 }, { "delayFeedback", 0.35f }, { "delayTone", -0.1f }, { "delayMix", 0.15f },
            { "reverbSize", 0.6f }, { "reverbMix", 0.2f },
            { "scale", 0 }, { "rootNote", 4 }, { "seqOctave", 0 }, { "seqLength", 16 }, { "accent", 0.5f },
            { "outGain", -7.5f } },
          "@g70 0! . . . . . 0 . . . 3~ 0 . . 7 .", nullptr },

        { "Groundswell", "HARDGROOVE", "BASS", 135.0,
          { { "oscBRatio", 1 }, { "fmIndex", 1.0f }, { "fmFeedback", 0.15f },
            { "modAttack", 0.3f }, { "modDecay", 130.0f }, { "modCurve", -0.4f }, { "modToIndex", 3.0f },
            { "filterType", 0 }, { "cutoff", 550.0f }, { "resonance", 0.25f }, { "filterEnv", 20.0f }, { "keyTrack", 0.4f },
            { "ampAttack", 0.3f }, { "ampDecay", 200.0f }, { "ampSustain", 0.2f }, { "ampRelease", 40.0f },
            { "velSens", 0.9f }, { "fxDrive", 0.25f },
            { "scale", 0 }, { "rootNote", 7 }, { "seqOctave", -2 }, { "seqLength", 16 }, { "swing", 0.55f }, { "accent", 0.6f },
            { "outGain", -4.2f } },
          "@g55 0! . 0 0v45?60 . 0 . 0v45?60 0! . 0 0v45?60 . 7 . 0v45?60", nullptr },

        { "Shoreline Stab", "HARDGROOVE", "SYNTH", 135.0,
          { { "oscAShape", 0.35f }, { "oscBShape", 0.35f }, { "oscBRatio", 2 }, { "oscMix", 0.45f }, { "fmIndex", 0.15f },
            { "modAttack", 0.3f }, { "modDecay", 80.0f }, { "modToCutoff", 24.0f }, { "fxDrive", 0.3f },
            { "filterType", 0 }, { "cutoff", 1200.0f }, { "resonance", 0.25f },
            { "ampAttack", 0.3f }, { "ampDecay", 210.0f }, { "ampSustain", 0.0f }, { "ampRelease", 50.0f },
            { "delayTime", 5 }, { "delayFeedback", 0.4f }, { "delayTone", 0.4f }, { "delayMix", 0.22f },
            { "reverbSize", 0.5f }, { "reverbMix", 0.12f },
            { "scale", 0 }, { "rootNote", 5 }, { "seqOctave", 0 }, { "seqLength", 16 }, { "swing", 0.58f }, { "accent", 0.6f },
            { "outGain", 0.1f } },
          "@g30 . . 0! . . 0 . . 0 . . 0! . . 0 .", nullptr },

        { "Wave Hammer", "HARDGROOVE", "BASS", 135.0,
          { { "oscBRatio", 3 }, { "fmIndex", 1.8f }, { "fmFeedback", 0.2f },
            { "modAttack", 0.3f }, { "modDecay", 80.0f }, { "modToIndex", 2.0f },
            { "lfo1Shape", 0 }, { "lfo1Sync", 1 }, { "lfo1SyncRate", 5 }, { "lfo1Dest", 2 }, { "lfo1Depth", 0.2f },
            { "filterType", 0 }, { "cutoff", 1500.0f }, { "resonance", 0.35f },
            { "ampAttack", 0.3f }, { "ampDecay", 180.0f }, { "ampSustain", 0.25f }, { "ampRelease", 40.0f },
            { "fxDrive", 0.55f },
            { "scale", 0 }, { "rootNote", 0 }, { "seqOctave", -1 }, { "seqLength", 16 }, { "accent", 0.6f },
            { "outGain", -7.6f } },
          "@g40 0 0 0! 0 0 0 0! 0 0 0 0! 0 0 3 0! 0", nullptr },

        { "Flare Zap", "HARDGROOVE", "FX", 135.0,
          { { "oscBRatio", 1 }, { "fmIndex", 1.5f }, { "fmFeedback", 0.15f },
            { "modAttack", 0.2f }, { "modDecay", 110.0f }, { "modCurve", -0.7f }, { "modToPitch", 48.0f }, { "modToIndex", 3.0f },
            { "filterType", 0 }, { "cutoff", 8000.0f },
            { "ampAttack", 0.2f }, { "ampDecay", 300.0f }, { "ampSustain", 0.0f }, { "ampRelease", 80.0f },
            { "lfo2Shape", 0 }, { "lfo2Sync", 1 }, { "lfo2SyncRate", 13 }, { "lfo2Dest", 12 }, { "lfo2Depth", 0.6f },
            { "delayTime", 4 }, { "delayFeedback", 0.55f }, { "delayTone", 0.2f }, { "delayMix", 0.3f },
            { "reverbSize", 0.6f }, { "reverbMix", 0.18f },
            { "scale", 0 }, { "rootNote", 0 }, { "seqOctave", 1 }, { "seqLength", 16 }, { "accent", 0.6f },
            { "outGain", -4.2f } },
          "@g40 0! . . . . . . . . . . . 7 . . 5?50", nullptr },


        //======================================================================
        // HYPNOTIC : boucles longues, profondes, qui évoluent lentement (130 BPM) — 13 presets
        //======================================================================
        { "Abyssal Loop", "HYPNOTIC", "SYNTH", 130.0,
          { { "oscBRatio", 3 }, { "fmIndex", 2.6f }, { "fmFeedback", 0.1f },
            { "modAttack", 0.5f }, { "modDecay", 180.0f }, { "modCurve", -0.4f }, { "modToIndex", 1.8f },
            { "filterType", 0 }, { "cutoff", 3000.0f }, { "resonance", 0.25f }, { "keyTrack", 0.3f },
            { "ampAttack", 1.0f }, { "ampDecay", 240.0f }, { "ampSustain", 0.15f }, { "ampRelease", 140.0f },
            { "lfo1Shape", 2 }, { "lfo1Sync", 1 }, { "lfo1SyncRate", 16 }, { "lfo1Dest", 2 }, { "lfo1Depth", 0.22f },
            { "delayTime", 5 }, { "delayFeedback", 0.45f }, { "delayTone", -0.3f }, { "delayMix", 0.22f },
            { "reverbSize", 0.6f }, { "reverbMix", 0.2f },
            { "scale", 0 }, { "rootNote", 9 }, { "seqOctave", 0 }, { "seqLength", 16 }, { "swing", 0.5f }, { "accent", 0.5f },
            { "outGain", -7.5f } },
          "@g45 0! . 0 7 . 0 3 . 0 . 10 . 0 7 . 3", nullptr },

        { "Slow Tide", "HYPNOTIC", "SYNTH", 130.0,
          { { "oscBRatio", 3 }, { "fmIndex", 1.1f },
            { "modAttack", 2.0f }, { "modDecay", 420.0f }, { "modCurve", -0.2f }, { "modToIndex", 1.4f },
            { "filterType", 0 }, { "cutoff", 1100.0f }, { "resonance", 0.32f },
            { "ampAttack", 6.0f }, { "ampDecay", 650.0f }, { "ampSustain", 0.25f }, { "ampRelease", 420.0f },
            { "lfo1Shape", 0 }, { "lfo1Sync", 1 }, { "lfo1SyncRate", 15 }, { "lfo1Dest", 5 }, { "lfo1Depth", 0.35f },
            { "delayTime", 5 }, { "delayFeedback", 0.5f }, { "delayTone", -0.4f }, { "delayMix", 0.2f },
            { "reverbSize", 0.8f }, { "reverbDamp", 0.6f }, { "reverbMix", 0.32f },
            { "scale", 0 }, { "rootNote", 9 }, { "seqOctave", 0 }, { "seqLength", 16 }, { "accent", 0.3f },
            { "outGain", -7.6f } },
          "@g80 0 . . 7 . . 0 . . 3 . . 0 . 10 .", nullptr },

        { "Deep Pressure", "HYPNOTIC", "BASS", 130.0,
          { { "oscBRatio", 1 }, { "fmIndex", 0.8f }, { "fmFeedback", 0.05f },
            { "modAttack", 0.5f }, { "modDecay", 120.0f }, { "modCurve", -0.4f }, { "modToIndex", 2.5f },
            { "filterType", 0 }, { "cutoff", 380.0f }, { "resonance", 0.2f }, { "filterEnv", 18.0f }, { "filterDrive", 0.2f }, { "keyTrack", 0.5f },
            { "ampAttack", 1.0f }, { "ampDecay", 320.0f }, { "ampSustain", 0.4f }, { "ampRelease", 90.0f },
            { "reverbSize", 0.4f }, { "reverbMix", 0.05f },
            { "scale", 1 }, { "rootNote", 4 }, { "seqOctave", -2 }, { "seqLength", 16 }, { "accent", 0.5f },
            { "outGain", -5.0f } },
          "@g70 0! . . . . . . 1 . . 0 . . . . .", nullptr },

        { "Kelp Forest", "HYPNOTIC", "SYNTH", 130.0,
          { { "oscBRatio", 5 }, { "fmIndex", 1.4f },
            { "modAttack", 0.5f }, { "modDecay", 160.0f }, { "modToIndex", 1.5f },
            { "filterType", 0 }, { "cutoff", 2600.0f }, { "resonance", 0.2f }, { "keyTrack", 0.3f },
            { "ampAttack", 0.5f }, { "ampDecay", 300.0f }, { "ampSustain", 0.08f }, { "ampRelease", 160.0f },
            { "modLaneDest", 1 }, { "modLaneLength", 13 }, { "modLaneDepth", 0.3f },
            { "delayTime", 5 }, { "delayFeedback", 0.5f }, { "delayTone", -0.2f }, { "delayMix", 0.25f },
            { "reverbSize", 0.6f }, { "reverbMix", 0.2f },
            { "scale", 0 }, { "rootNote", 9 }, { "seqOctave", 0 }, { "seqLength", 16 }, { "accent", 0.4f },
            { "outGain", -7.0f } },
          "@g45 0 . 7 0 . 3 . 0 10 . 0 . 7 . 3 .",
          "0.8 0.2 -0.4 0.5 -0.1 0.9 -0.6 0.3 0 -0.3 0.6 -0.8 0.4" },

        { "Night Swim", "HYPNOTIC", "SYNTH", 130.0,
          { { "oscBRatio", 6 }, { "fmIndex", 1.6f },
            { "modAttack", 1.0f }, { "modDecay", 500.0f }, { "modCurve", -0.4f }, { "modToIndex", 2.0f },
            { "filterType", 0 }, { "cutoff", 6000.0f },
            { "ampAttack", 1.5f }, { "ampDecay", 1200.0f }, { "ampSustain", 0.0f }, { "ampRelease", 800.0f },
            { "delayTime", 6 }, { "delayFeedback", 0.45f }, { "delayTone", -0.2f }, { "delayMix", 0.2f },
            { "reverbSize", 0.85f }, { "reverbDamp", 0.5f }, { "reverbMix", 0.4f },
            { "scale", 0 }, { "rootNote", 9 }, { "seqOctave", 1 }, { "seqLength", 16 }, { "seqRate", 0 }, { "accent", 0.3f },
            { "outGain", -9.0f } },
          "@g90 0 . . 7 . . 12 . . . 3 . . . . 10", nullptr },

        { "Dark Lagoon", "HYPNOTIC", "BASS", 130.0,
          { { "oscBRatio", 3 }, { "fmIndex", 1.0f }, { "fmFeedback", 0.1f },
            { "modAttack", 0.3f }, { "modDecay", 140.0f }, { "modToIndex", 1.5f },
            { "filterType", 0 }, { "filterSlope", 1 }, { "cutoff", 700.0f }, { "resonance", 0.5f }, { "filterEnv", 12.0f },
            { "ampAttack", 0.5f }, { "ampDecay", 240.0f }, { "ampSustain", 0.2f }, { "ampRelease", 60.0f },
            { "delayTime", 5 }, { "delayFeedback", 0.35f }, { "delayTone", -0.3f }, { "delayMix", 0.12f },
            { "scale", 3 }, { "rootNote", 2 }, { "seqOctave", -1 }, { "seqLength", 16 }, { "accent", 0.5f },
            { "outGain", -8.8f } },
          "@g55 0c400 3 8c1800 0 3c900 8 0 3c2600 8 0c600 3 8 0c1500 3 8 0c3000", nullptr },

        { "Pendulum", "HYPNOTIC", "SYNTH", 130.0,
          { { "oscBRatio", 3 }, { "fmIndex", 1.2f },
            { "modAttack", 0.5f }, { "modDecay", 200.0f }, { "modToIndex", 1.2f },
            { "filterType", 0 }, { "cutoff", 1800.0f }, { "resonance", 0.25f },
            { "ampAttack", 0.5f }, { "ampDecay", 260.0f }, { "ampSustain", 0.1f }, { "ampRelease", 120.0f },
            { "mutateAmount", 0.15f }, { "mutateBars", 2 },
            { "delayTime", 5 }, { "delayFeedback", 0.45f }, { "delayTone", -0.3f }, { "delayMix", 0.2f },
            { "reverbSize", 0.6f }, { "reverbMix", 0.15f },
            { "scale", 0 }, { "rootNote", 4 }, { "seqOctave", 0 }, { "seqLength", 16 }, { "accent", 0.4f },
            { "outGain", -5.4f } },
          "@g45 0! . 0 7 . 0 . 3 0 . 10 . 0 7 . .", nullptr },

        { "Bioluminescence", "HYPNOTIC", "FX", 130.0,
          { { "oscBRatio", 7 }, { "fmIndex", 0.9f },
            { "modAttack", 0.5f }, { "modDecay", 300.0f }, { "modToIndex", 1.0f },
            { "filterType", 0 }, { "cutoff", 9000.0f },
            { "ampAttack", 1.0f }, { "ampDecay", 500.0f }, { "ampSustain", 0.0f }, { "ampRelease", 300.0f },
            { "lfo2Shape", 4 }, { "lfo2Sync", 1 }, { "lfo2SyncRate", 7 }, { "lfo2Dest", 12 }, { "lfo2Depth", 0.5f },
            { "delayTime", 3 }, { "delayFeedback", 0.6f }, { "delayTone", 0.2f }, { "delayMix", 0.35f },
            { "reverbSize", 0.7f }, { "reverbMix", 0.3f },
            { "scale", 4 }, { "rootNote", 9 }, { "seqOctave", 1 }, { "seqLength", 16 }, { "accent", 0.3f },
            { "outGain", -6.4f } },
          "@g50 12?40 . . 19?40 . . 15?40 . 24?40 . . 12?40 . 22?40 . .", nullptr },

        { "Low Tide Drone", "HYPNOTIC", "FX", 130.0,
          { { "oscBRatio", 2 }, { "fmIndex", 2.0f }, { "fmFeedback", 0.15f },
            { "filterType", 0 }, { "cutoff", 1400.0f }, { "resonance", 0.2f },
            { "ampAttack", 400.0f }, { "ampDecay", 1000.0f }, { "ampSustain", 1.0f }, { "ampRelease", 1500.0f },
            { "legato", 1 },
            { "lfo1Shape", 4 }, { "lfo1Sync", 1 }, { "lfo1SyncRate", 13 }, { "lfo1Dest", 2 }, { "lfo1Depth", 0.2f },
            { "lfo2Shape", 0 }, { "lfo2Sync", 1 }, { "lfo2SyncRate", 15 }, { "lfo2Dest", 5 }, { "lfo2Depth", 0.15f },
            { "delayTime", 7 }, { "delayFeedback", 0.4f }, { "delayTone", -0.4f }, { "delayMix", 0.12f },
            { "reverbSize", 0.8f }, { "reverbMix", 0.35f },
            { "scale", 0 }, { "rootNote", 9 }, { "seqOctave", -1 }, { "seqLength", 16 }, { "accent", 0.0f },
            { "outGain", -14.3f } },
          "@g100 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~", nullptr },

        { "Sonar", "HYPNOTIC", "FX", 130.0,
          { { "oscBRatio", 4 }, { "fmIndex", 1.2f },
            { "modAttack", 0.2f }, { "modDecay", 40.0f }, { "modToIndex", 3.0f }, { "fxDrive", 0.35f },
            { "filterType", 1 }, { "cutoff", 1500.0f }, { "resonance", 0.45f },
            { "ampAttack", 0.3f }, { "ampDecay", 220.0f }, { "ampSustain", 0.0f }, { "ampRelease", 60.0f },
            { "delayTime", 5 }, { "delayFeedback", 0.72f }, { "delayTone", -0.4f }, { "delayMix", 0.55f },
            { "reverbSize", 0.7f }, { "reverbMix", 0.25f },
            { "scale", 0 }, { "rootNote", 9 }, { "seqOctave", 1 }, { "seqLength", 16 }, { "accent", 0.4f },
            { "outGain", 0.6f } },
          "@g40 0! . . . . . . . . . 7?50 . . . . .", nullptr },

        { "Dub Basin", "HYPNOTIC", "SYNTH", 130.0,
          { { "oscAShape", 0.35f }, { "oscBShape", 0.35f }, { "oscBFree", 1 }, { "oscBFreeRatio", 1.498f }, { "oscMix", 0.45f },
            { "fmIndex", 0.2f },
            { "modAttack", 0.5f }, { "modDecay", 120.0f }, { "modToCutoff", 18.0f },
            { "filterType", 0 }, { "cutoff", 800.0f }, { "resonance", 0.35f }, { "filterEnv", 10.0f }, { "filterDrive", 0.35f },
            { "ampAttack", 1.0f }, { "ampDecay", 450.0f }, { "ampSustain", 0.0f }, { "ampRelease", 120.0f },
            { "lfo1Shape", 0 }, { "lfo1Sync", 1 }, { "lfo1SyncRate", 15 }, { "lfo1Dest", 5 }, { "lfo1Depth", 0.12f },
            { "delayTime", 5 }, { "delayFeedback", 0.65f }, { "delayTone", -0.5f }, { "delayMix", 0.35f },
            { "reverbSize", 0.75f }, { "reverbMix", 0.28f },
            { "scale", 0 }, { "rootNote", 0 }, { "seqOctave", 0 }, { "seqLength", 16 }, { "accent", 0.4f },
            { "outGain", -4.1f } },
          "@g40 . . 0 . . . 0 . . . 0 . . . 0 .", nullptr },

        { "Deep Rotation", "HYPNOTIC", "BASS", 130.0,
          { { "oscBRatio", 1 }, { "fmIndex", 0.6f },
            { "modAttack", 0.3f }, { "modDecay", 90.0f }, { "modToIndex", 1.5f },
            { "filterType", 0 }, { "cutoff", 280.0f }, { "resonance", 0.25f }, { "filterEnv", 16.0f }, { "keyTrack", 0.4f }, { "filterDrive", 0.3f }, { "fxDrive", 0.2f },
            { "ampAttack", 0.5f }, { "ampDecay", 150.0f }, { "ampSustain", 0.0f }, { "ampRelease", 40.0f },
            { "lfo1Shape", 0 }, { "lfo1Sync", 1 }, { "lfo1SyncRate", 14 }, { "lfo1Dest", 5 }, { "lfo1Depth", 0.1f },
            { "scale", 0 }, { "rootNote", 9 }, { "seqOctave", -2 }, { "seqLength", 16 }, { "accent", 0.5f },
            { "outGain", -3.9f } },
          "@g50 . 0v70 0! 0v60 . 0v70 0 0v55 . 0v70 0! 0v60 . 0v70 3 0v55", nullptr },

        { "Sea Foam", "HYPNOTIC", "PERC", 130.0,
          { { "oscBFree", 1 }, { "oscBFreeRatio", 5.1f }, { "fmIndex", 4.0f }, { "fmFeedback", 0.9f },
            { "filterType", 2 }, { "cutoff", 7000.0f }, { "resonance", 0.1f },
            { "ampAttack", 0.5f }, { "ampDecay", 95.0f }, { "ampSustain", 0.0f }, { "ampRelease", 40.0f },
            { "velSens", 0.6f }, { "fxDrive", 0.35f },
            { "lfo2Shape", 0 }, { "lfo2Sync", 1 }, { "lfo2SyncRate", 13 }, { "lfo2Dest", 12 }, { "lfo2Depth", 0.3f },
            { "reverbSize", 0.5f }, { "reverbMix", 0.15f },
            { "scale", 5 }, { "rootNote", 0 }, { "seqOctave", 2 }, { "seqLength", 16 }, { "swing", 0.54f }, { "accent", 0.4f },
            { "outGain", -1.7f } },
          "@g40 0v35 0v55 0v100 0v55 0v35 0v55 0v100 0v55 0v35 0v55 0v100 0v55 0v35 0v55 0v100 0v70", nullptr },


        //======================================================================
        // MENTAL : acide, agressif, strident, psychédélique (142 BPM)
        //======================================================================
        { "Maelstrom", "MENTAL", "SYNTH", 142.0,
          { { "oscBRatio", 2 }, { "fmIndex", 3.0f }, { "fmFeedback", 0.62f },
            { "modAttack", 0.3f }, { "modDecay", 90.0f }, { "modCurve", -0.5f }, { "modToIndex", 2.0f },
            { "filterType", 0 }, { "cutoff", 4500.0f }, { "resonance", 0.4f },
            { "ampAttack", 0.5f }, { "ampDecay", 180.0f }, { "ampSustain", 0.45f }, { "ampRelease", 50.0f },
            { "lfo1Shape", 0 }, { "lfo1Sync", 1 }, { "lfo1SyncRate", 1 }, { "lfo1Dest", 2 }, { "lfo1Depth", 0.28f },
            { "lfo2Shape", 4 }, { "lfo2Sync", 1 }, { "lfo2SyncRate", 4 }, { "lfo2Dest", 5 }, { "lfo2Depth", 0.2f },
            { "fxDrive", 0.3f },
            { "delayTime", 2 }, { "delayFeedback", 0.3f }, { "delayTone", 0.3f }, { "delayMix", 0.1f },
            { "reverbMix", 0.08f },
            { "scale", 0 }, { "rootNote", 2 }, { "seqOctave", 0 }, { "seqLength", 16 }, { "accent", 0.6f },
            { "outGain", -10.1f } },
          "@g60 0 0 0! 0 7 0 0 12 0 0 0! 3 0 0 10 0", nullptr },

        { "Vortex", "MENTAL", "SYNTH", 142.0,
          { { "oscBFree", 1 }, { "oscBFreeRatio", 2.0f }, { "fmIndex", 3.4f }, { "fmFeedback", 0.2f },
            { "modAttack", 0.5f }, { "modDecay", 160.0f }, { "modToIndex", 1.5f },
            { "filterType", 0 }, { "filterSlope", 1 }, { "cutoff", 1600.0f }, { "resonance", 0.78f }, { "filterEnv", 20.0f },
            { "ampAttack", 0.5f }, { "ampDecay", 240.0f }, { "ampSustain", 0.55f }, { "ampRelease", 70.0f },
            { "lfo1Shape", 1 }, { "lfo1Sync", 1 }, { "lfo1SyncRate", 14 }, { "lfo1Dest", 3 }, { "lfo1Depth", 0.35f },
            { "delayTime", 5 }, { "delayFeedback", 0.35f }, { "delayMix", 0.14f },
            { "reverbSize", 0.5f }, { "reverbMix", 0.1f },
            { "scale", 0 }, { "rootNote", 2 }, { "seqOctave", -1 }, { "seqLength", 16 }, { "accent", 0.6f },
            { "outGain", -10.0f } },
          "@g55 0! . 0 0 . 0 0 . 0! . 0 0 . 0 0 .", nullptr },

        { "Crosscurrent", "MENTAL", "BASS", 142.0,
          { { "oscAShape", 0.55f }, { "oscBRatio", 1 }, { "fmIndex", 1.2f }, { "fmFeedback", 0.35f },
            { "modAttack", 0.3f }, { "modDecay", 210.0f }, { "modCurve", -0.5f }, { "modToCutoff", 38.0f },
            { "filterType", 0 }, { "filterSlope", 1 }, { "cutoff", 300.0f }, { "resonance", 0.85f }, { "filterEnv", 10.0f },
            { "filterDrive", 0.4f },
            { "ampAttack", 0.3f }, { "ampDecay", 380.0f }, { "ampSustain", 0.7f }, { "ampRelease", 30.0f },
            { "fxDrive", 0.25f },
            { "delayTime", 5 }, { "delayFeedback", 0.4f }, { "delayTone", 0.2f }, { "delayMix", 0.12f },
            { "scale", 0 }, { "rootNote", 2 }, { "seqOctave", -1 }, { "seqLength", 16 }, { "accent", 0.85f },
            { "outGain", -11.3f } },
          "@g50 0 0~ 12 0! . 0 3~ 5 0 . 10! 0~ 12 . 0! 7", nullptr },

        //======================================================================
        // MENTAL (suite) : synthés, bleeps et roulements de percussions (142 BPM)
        //======================================================================
        { "Pressure Drop", "MENTAL", "BASS", 142.0,
          { { "oscAShape", 0.3f }, { "oscBRatio", 1 }, { "fmIndex", 1.8f }, { "fmFeedback", 0.3f }, { "modAttack", 0.2f },
            { "modDecay", 130 }, { "modCurve", -0.6f }, { "modToIndex", 3.0f }, { "modToPitch", 24 }, { "filterType", 0 },
            { "cutoff", 2400 }, { "resonance", 0.3f }, { "filterDrive", 0.4f }, { "ampAttack", 0.3f }, { "ampDecay", 240 },
            { "ampSustain", 0.25f }, { "ampRelease", 40 }, { "fxDrive", 0.6f }, { "reverbSize", 0.4f }, { "reverbMix", 0.06f },
            { "scale", 1 }, { "rootNote", 4 }, { "seqOctave", -1 }, { "seqLength", 16 }, { "accent", 0.7f }, { "outGain", -6.1f } },
          "@g45 0! . . 0 . 0 1 . 0! . . 0 . 0 . 1", nullptr },

        { "Siren Surge", "MENTAL", "FX", 142.0,
          { { "fmMode", 1 }, { "oscBRatio", 3 }, { "oscMix", 0.3f }, { "fmIndex", 2.2f }, { "fmFeedback", 0.25f }, { "legato", 1 },
            { "glide", 900 }, { "filterType", 0 }, { "cutoff", 5000 }, { "resonance", 0.35f }, { "ampAttack", 20 },
            { "ampDecay", 600 }, { "ampSustain", 0.9f }, { "ampRelease", 200 }, { "lfo1Shape", 2 }, { "lfo1Sync", 1 },
            { "lfo1SyncRate", 13 }, { "lfo1Dest", 1 }, { "lfo1Depth", 0.25f }, { "lfo2Shape", 0 }, { "lfo2Sync", 1 },
            { "lfo2SyncRate", 2 }, { "lfo2Dest", 2 }, { "lfo2Depth", 0.15f }, { "fxDrive", 0.3f }, { "delayTime", 4 },
            { "delayFeedback", 0.45f }, { "delayTone", 0.2f }, { "delayMix", 0.2f }, { "reverbSize", 0.6f }, { "reverbMix", 0.15f },
            { "scale", 0 }, { "rootNote", 2 }, { "seqOctave", 1 }, { "seqLength", 16 }, { "seqRate", 0 }, { "accent", 0.3f },
            { "outGain", -13.1f } },
          "@g100 0~ 0~ 0~ 0~ 7~ 7~ 7~ 7~ 12~ 12~ 12~ 12~ 10~ 10~ 7~ 7", nullptr },

        { "Cavitation", "MENTAL", "SYNTH", 142.0,
          { { "oscBFree", 1 }, { "oscBFreeRatio", 2.37f }, { "fmIndex", 2.6f }, { "fmFeedback", 0.35f }, { "modAttack", 0.2f },
            { "modDecay", 70 }, { "modCurve", -0.6f }, { "modToIndex", 3.5f }, { "filterType", 0 }, { "cutoff", 3800 },
            { "resonance", 0.45f }, { "filterEnv", 12 }, { "ampAttack", 0.3f }, { "ampDecay", 150 }, { "ampSustain", 0.0f },
            { "ampRelease", 40 }, { "fxDrive", 0.35f }, { "delayTime", 5 }, { "delayFeedback", 0.35f }, { "delayMix", 0.12f },
            { "reverbMix", 0.06f }, { "scale", 1 }, { "rootNote", 2 }, { "seqOctave", 0 }, { "seqLength", 16 }, { "accent", 0.7f },
            { "mutateAmount", 0.2f }, { "mutateBars", 0 }, { "outGain", -6.9f } },
          "@g50 0! 0 . 1 0 . 7 0! . 0 3 . 0! . 1 0", nullptr },

        { "Electric Eel", "MENTAL", "SYNTH", 142.0,
          { { "oscBRatio", 3 }, { "fmIndex", 1.0f }, { "fmFeedback", 0.2f }, { "modAttack", 0.2f }, { "modDecay", 45 },
            { "modToIndex", 2.0f }, { "filterType", 0 }, { "cutoff", 4200 }, { "resonance", 0.5f }, { "ampAttack", 0.2f },
            { "ampDecay", 90 }, { "ampSustain", 0.0f }, { "ampRelease", 25 }, { "fxDrive", 0.3f }, { "delayTime", 2 },
            { "delayFeedback", 0.3f }, { "delayMix", 0.1f }, { "scale", 0 }, { "rootNote", 2 }, { "seqOctave", 0 },
            { "seqLength", 16 }, { "accent", 0.6f }, { "outGain", -9.2f } },
          "@g60 0!i1 0i3 0x4i6 0i2 12i4 0i1 0x4i8 0i2 0!i5 0i1 3x4i7 0i3 0i2 0x4i9 7i4 0i1", nullptr },

        { "Storm Surge", "MENTAL", "PERC", 142.0,
          { { "oscBFree", 1 }, { "oscBFreeRatio", 3.1f }, { "fmIndex", 3.0f }, { "fmFeedback", 0.45f }, { "modAttack", 0.1f },
            { "modDecay", 25 }, { "modToIndex", 3.0f }, { "filterType", 1 }, { "cutoff", 2600 }, { "resonance", 0.4f },
            { "ampAttack", 0.1f }, { "ampDecay", 60 }, { "ampSustain", 0.0f }, { "ampRelease", 20 }, { "fxDrive", 0.55f },
            { "delayTime", 2 }, { "delayFeedback", 0.4f }, { "delayTone", 0.3f }, { "delayMix", 0.22f }, { "reverbMix", 0.08f },
            { "scale", 4 }, { "rootNote", 2 }, { "seqOctave", 1 }, { "seqLength", 32 }, { "seqRate", 2 }, { "accent", 0.6f },
            { "outGain", -4.2f } },
          "@g50 0! 0?40 0?60 . 7?50 0 . 0?30 0! . 0?50 3?40 0 0?70 . 0?30 0! 0?40 . 0?60 10?50 0 0?50 . 0! 0?60 0?40 . 7 0?50 0?30 12?40", nullptr },

        { "Whirl", "MENTAL", "SYNTH", 142.0,
          { { "oscBRatio", 5 }, { "fmIndex", 1.6f }, { "fmFeedback", 0.2f }, { "modAttack", 0.3f }, { "modDecay", 100 },
            { "modToIndex", 2.0f }, { "filterType", 0 }, { "cutoff", 2200 }, { "resonance", 0.6f }, { "filterEnv", 18 },
            { "ampAttack", 0.3f }, { "ampDecay", 200 }, { "ampSustain", 0.2f }, { "ampRelease", 40 }, { "lfo1Shape", 2 },
            { "lfo1Sync", 1 }, { "lfo1SyncRate", 8 }, { "lfo1Dest", 5 }, { "lfo1Depth", 0.25f }, { "modLaneDest", 1 },
            { "modLaneLength", 7 }, { "modLaneDepth", 0.5f }, { "fxDrive", 0.25f }, { "delayTime", 5 }, { "delayFeedback", 0.35f },
            { "delayMix", 0.12f }, { "scale", 2 }, { "rootNote", 2 }, { "seqOctave", 0 }, { "seqLength", 16 }, { "accent", 0.6f },
            { "outGain", -11.7f } },
          "@g55 0! 0 7 0 3 0 0! 10 0 0 7! 0 5 0 3 0", "0.6 -0.3 0.2 0.9 -0.6 0.1 -0.2" },

        { "Rapture of the Deep", "MENTAL", "FX", 142.0,
          { { "oscBRatio", 1 }, { "fmIndex", 8.0f }, { "fmFeedback", 0.3f }, { "legato", 1 }, { "glide", 300 }, { "filterType", 0 },
            { "cutoff", 900 }, { "resonance", 0.3f }, { "ampAttack", 300 }, { "ampDecay", 1000 }, { "ampSustain", 0.9f },
            { "ampRelease", 1500 }, { "lfo1Shape", 0 }, { "lfo1Sync", 1 }, { "lfo1SyncRate", 16 }, { "lfo1Dest", 1 },
            { "lfo1Depth", 0.05f }, { "lfo2Shape", 1 }, { "lfo2Sync", 1 }, { "lfo2SyncRate", 14 }, { "lfo2Dest", 5 },
            { "lfo2Depth", 0.12f }, { "delayTime", 7 }, { "delayFeedback", 0.5f }, { "delayTone", -0.5f }, { "delayMix", 0.15f },
            { "reverbSize", 0.9f }, { "reverbDamp", 0.8f }, { "reverbMix", 0.45f }, { "scale", 1 }, { "rootNote", 4 },
            { "seqOctave", -1 }, { "seqLength", 16 }, { "seqRate", 0 }, { "accent", 0.0f }, { "outGain", -16.8f } },
          "@g100 0~ 0~ 0~ 0~ 0~ 0~ 0~ 1~ 1~ 1~ 1~ 1~ 0~ 0~ 0~ 0", nullptr },

        { "Acid Vein", "MENTAL", "BASS", 142.0,
          { { "oscAShape", 0.85f }, { "oscBRatio", 1 }, { "fmIndex", 0.6f }, { "fmFeedback", 0.1f }, { "modAttack", 0.2f },
            { "modDecay", 180 }, { "modCurve", -0.6f }, { "modToCutoff", 30 }, { "filterType", 0 }, { "filterSlope", 1 },
            { "cutoff", 420 }, { "resonance", 0.88f }, { "filterEnv", 8 }, { "filterDrive", 0.5f }, { "ampAttack", 0.3f },
            { "ampDecay", 300 }, { "ampSustain", 0.6f }, { "ampRelease", 25 }, { "glide", 60 }, { "fxDrive", 0.3f },
            { "lfo1Shape", 1 }, { "lfo1Sync", 1 }, { "lfo1SyncRate", 15 }, { "lfo1Dest", 5 }, { "lfo1Depth", 0.15f },
            { "delayTime", 5 }, { "delayFeedback", 0.35f }, { "delayTone", 0.2f }, { "delayMix", 0.1f }, { "scale", 0 },
            { "rootNote", 9 }, { "seqOctave", -1 }, { "seqLength", 16 }, { "accent", 0.9f }, { "outGain", -12.9f } },
          "@g55 0! 0 12~ 0 . 0! 3~ 5 0 . 12! 0~ 10 0 7! 0", nullptr },

        { "Bleep Signal", "MENTAL", "BLEEP", 142.0,
          { { "oscBRatio", 3 }, { "fmIndex", 0.3f }, { "modAttack", 0.1f }, { "modDecay", 20 }, { "modToIndex", 1.2f },
            { "filterType", 0 }, { "cutoff", 12000 }, { "ampAttack", 0.5f }, { "ampDecay", 140.0f }, { "ampSustain", 0.0f },
            { "ampRelease", 30 }, { "delayTime", 5 }, { "delayFeedback", 0.45f }, { "delayTone", -0.2f }, { "delayMix", 0.38f },
            { "reverbSize", 0.5f }, { "reverbMix", 0.2f }, { "scale", 4 }, { "rootNote", 9 }, { "seqOctave", 2 },
            { "seqLength", 16 }, { "accent", 0.4f }, { "fxDrive", 0.2f }, { "outGain", -5.5f } },
          "@g40 0! . . 7 . . 12 . . 0 . . 10 . 7 .", nullptr },

        { "Pulse Code", "MENTAL", "BLEEP", 142.0,
          { { "oscAShape", 0.55f }, { "oscBRatio", 1 }, { "fmIndex", 0.5f }, { "modAttack", 0.1f }, { "modDecay", 30 },
            { "modToIndex", 1.5f }, { "filterType", 0 }, { "cutoff", 5000 }, { "resonance", 0.2f }, { "ampAttack", 0.3f },
            { "ampDecay", 100.0f }, { "ampSustain", 0.0f }, { "ampRelease", 20 }, { "delayTime", 2 }, { "delayFeedback", 0.25f },
            { "delayMix", 0.25f }, { "reverbMix", 0.15f }, { "scale", 0 }, { "rootNote", 2 }, { "seqOctave", 2 },
            { "seqLength", 16 }, { "accent", 0.5f }, { "fxDrive", 0.25f }, { "outGain", -1.8f } },
          "@g30 0! 0 . 0 0 . . 0 7! . 0 0 . 12 . 0?60", nullptr },

        { "Echo Probe", "MENTAL", "BLEEP", 142.0,
          { { "oscBRatio", 7 }, { "fmIndex", 0.8f }, { "modAttack", 0.1f }, { "modDecay", 35 }, { "modToIndex", 2.0f },
            { "filterType", 1 }, { "cutoff", 2500 }, { "resonance", 0.5f }, { "ampAttack", 0.3f }, { "ampDecay", 170.0f },
            { "ampSustain", 0.0f }, { "ampRelease", 40 }, { "delayTime", 3 }, { "delayFeedback", 0.7f }, { "delayTone", -0.3f },
            { "delayMix", 0.45f }, { "reverbSize", 0.7f }, { "reverbMix", 0.3f }, { "scale", 1 }, { "rootNote", 4 },
            { "seqOctave", 2 }, { "seqLength", 16 }, { "accent", 0.4f }, { "fxDrive", 0.25f }, { "outGain", -1.7f } },
          "@g40 0! . . . . . 7?70 . . . . . 1 . . .", nullptr },

        { "Telemetry", "MENTAL", "BLEEP", 142.0,
          { { "oscBRatio", 3 }, { "fmIndex", 0.6f }, { "modAttack", 0.1f }, { "modDecay", 25 }, { "modToIndex", 1.0f },
            { "glide", 20 }, { "filterType", 0 }, { "cutoff", 7000 }, { "resonance", 0.25f }, { "ampAttack", 0.3f },
            { "ampDecay", 110.0f }, { "ampSustain", 0.0f }, { "ampRelease", 25 }, { "modLaneDest", 2 }, { "modLaneLength", 11 },
            { "modLaneDepth", 0.4f }, { "delayTime", 5 }, { "delayFeedback", 0.35f }, { "delayMix", 0.28f }, { "reverbMix", 0.16f },
            { "scale", 4 }, { "rootNote", 7 }, { "seqOctave", 1 }, { "seqLength", 32 }, { "accent", 0.5f }, { "fxDrive", 0.2f }, { "outGain", -6.0f } },
          "@g35 0! 7 12 . 3 . 10 7 0 . 12 15 . 7 3 . 0! 7 12 . 5 . 10 7 0 . 15 12 . 10 7 3", "0.5 -0.3 0.1 0.8 -0.6 0.2 -0.1 0.6 -0.4 0.3 -0.8" },

        { "Snare Roll Tide", "MENTAL", "PERC", 142.0,
          { { "oscBFree", 1 }, { "oscBFreeRatio", 2.83f }, { "fmIndex", 6.0f }, { "fmFeedback", 0.85f }, { "modAttack", 0.1f },
            { "modDecay", 40 }, { "modToPitch", 7 }, { "filterType", 1 }, { "cutoff", 2800 }, { "resonance", 0.15f },
            { "ampAttack", 0.1f }, { "ampDecay", 120 }, { "ampSustain", 0.0f }, { "ampRelease", 50 }, { "velSens", 0.8f },
            { "fxDrive", 0.35f }, { "reverbSize", 0.5f }, { "reverbMix", 0.15f }, { "scale", 5 }, { "rootNote", 0 },
            { "seqOctave", 0 }, { "seqLength", 32 }, { "accent", 0.6f }, { "outGain", -1.2f } },
          "@g60 . . . . 0v60 . . . . . . . 0v60 . . . . . 0v50 . 0v55 . 0v60 . 0x2v60 0x2v65 0x2v70 0x2v75 0x3v80 0x3v85 0x4v90 0x4!v100", nullptr },

        { "Tom Cascade", "MENTAL", "PERC", 142.0,
          { { "oscBRatio", 1 }, { "fmIndex", 0.5f }, { "fmFeedback", 0.05f }, { "modAttack", 0.2f }, { "modDecay", 60 },
            { "modCurve", -0.7f }, { "modToIndex", 1.5f }, { "modToPitch", 14 }, { "filterType", 0 }, { "cutoff", 3500 },
            { "resonance", 0.1f }, { "ampAttack", 0.2f }, { "ampDecay", 260 }, { "ampSustain", 0.0f }, { "ampRelease", 60 },
            { "fxDrive", 0.3f }, { "reverbSize", 0.45f }, { "reverbMix", 0.14f }, { "scale", 0 }, { "rootNote", 9 },
            { "seqOctave", -1 }, { "seqLength", 16 }, { "swing", 0.54f }, { "accent", 0.6f }, { "outGain", -6.7f } },
          "@g60 0! . . 0 . . 0 . . . 12x2 10x2 7x2 5x2 3x3 0x3!", nullptr },

        { "Drum Avalanche", "MENTAL", "PERC", 142.0,
          { { "oscBFree", 1 }, { "oscBFreeRatio", 1.41f }, { "fmIndex", 3.5f }, { "fmFeedback", 0.4f }, { "modAttack", 0.1f },
            { "modDecay", 25 }, { "modToIndex", 3.0f }, { "modToPitch", 5 }, { "filterType", 2 }, { "cutoff", 900 },
            { "resonance", 0.2f }, { "ampAttack", 0.1f }, { "ampDecay", 110.0f }, { "ampSustain", 0.0f }, { "ampRelease", 30 },
            { "velSens", 0.8f }, { "fxDrive", 0.6f }, { "reverbSize", 0.4f }, { "reverbMix", 0.18f }, { "scale", 5 },
            { "rootNote", 0 }, { "seqOctave", 0 }, { "seqLength", 32 }, { "seqRate", 2 }, { "accent", 0.6f }, { "outGain", -0.7f } },
          "@g50 0! . . . 0v50?50 . 0v55 . 0! . . 0v50?40 0v55 . 0v60?60 . 0! . 0v55 0v58?70 0v60 0v65?70 0v68 0v70 0v75 0v78 0v80 0v84 0v88 0v92 0v96 0!v100", nullptr },

        { "Gallop Roll", "MENTAL", "PERC", 142.0,
          { { "oscBRatio", 2 }, { "fmIndex", 0.9f }, { "fmFeedback", 0.15f }, { "modAttack", 0.2f }, { "modDecay", 40 },
            { "modToIndex", 2.0f }, { "modToPitch", 10 }, { "filterType", 0 }, { "cutoff", 4000 }, { "resonance", 0.15f },
            { "ampAttack", 0.2f }, { "ampDecay", 150 }, { "ampSustain", 0.0f }, { "ampRelease", 40 }, { "fxDrive", 0.3f },
            { "reverbSize", 0.4f }, { "reverbMix", 0.1f }, { "scale", 4 }, { "rootNote", 2 }, { "seqOctave", -1 },
            { "seqLength", 24 }, { "seqRate", 4 }, { "accent", 0.65f }, { "outGain", -7.1f } },
          "@g55 0! 0v60 7v70 . 0 3v60 0! . 7 0v60 . 3 0! 0v60 7v70 . 10 7v60 0! 3v70 0v75 7v80 3v85 0!v95", nullptr },

        //======================================================================
        // ABYSS : ambient très profond (124 BPM)
        //======================================================================
        { "Hadal Zone", "ABYSS", "AMBIENT", 124.0,
          { { "oscBRatio", 0 }, { "oscMix", 0.15f }, { "fmIndex", 1.6f }, { "fmFeedback", 0.1f }, { "legato", 1 }, { "glide", 1500 },
            { "filterType", 0 }, { "cutoff", 1200.0f }, { "resonance", 0.25f }, { "keyTrack", 0.3f }, { "ampAttack", 1500 },
            { "ampDecay", 2000 }, { "ampSustain", 1.0f }, { "ampRelease", 4000 }, { "lfo1Shape", 0 }, { "lfo1Sync", 1 },
            { "lfo1SyncRate", 16 }, { "lfo1Dest", 2 }, { "lfo1Depth", 0.12f }, { "lfo2Shape", 0 }, { "lfo2Sync", 1 },
            { "lfo2SyncRate", 15 }, { "lfo2Dest", 5 }, { "lfo2Depth", 0.12f }, { "delayTime", 9 }, { "delayFeedback", 0.55f },
            { "delayTone", -0.6f }, { "delayMix", 0.2f }, { "reverbSize", 0.95f }, { "reverbDamp", 0.75f }, { "reverbMix", 0.55f },
            { "scale", 0 }, { "rootNote", 2 }, { "seqOctave", -1 }, { "seqLength", 16 }, { "seqRate", 0 }, { "accent", 0.0f },
            { "outGain", -14.5f } },
          "@g100 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ -2~ -2~ -2~ -2~ 3~ 3~ 0~ 0~", nullptr },

        { "Abyss Cathedral", "ABYSS", "AMBIENT", 124.0,
          { { "oscBFree", 1 }, { "oscBFreeRatio", 3.51f }, { "fmIndex", 0.9f }, { "modAttack", 5 }, { "modDecay", 1500 },
            { "modCurve", -0.3f }, { "modToIndex", 2.0f }, { "filterType", 0 }, { "cutoff", 4500 }, { "ampAttack", 5 },
            { "ampDecay", 3000 }, { "ampSustain", 0.0f }, { "ampRelease", 3000 }, { "delayTime", 8 }, { "delayFeedback", 0.55f },
            { "delayTone", -0.5f }, { "delayMix", 0.3f }, { "reverbSize", 1.0f }, { "reverbDamp", 0.6f }, { "reverbMix", 0.6f },
            { "scale", 0 }, { "rootNote", 2 }, { "seqOctave", 0 }, { "seqLength", 16 }, { "seqRate", 0 }, { "accent", 0.0f },
            { "outGain", -14.9f } },
          "@g90 0 . . . 7 . . . . . 3 . . 10?60 . .", nullptr },

        { "Trench Breath", "ABYSS", "AMBIENT", 124.0,
          { { "oscAShape", 0.2f }, { "oscBRatio", 1 }, { "fmIndex", 1.5f }, { "fmFeedback", 0.55f }, { "legato", 1 },
            { "glide", 800 }, { "filterType", 1 }, { "cutoff", 600 }, { "resonance", 0.55f }, { "ampAttack", 2000 },
            { "ampDecay", 2000 }, { "ampSustain", 1.0f }, { "ampRelease", 3000 }, { "lfo1Shape", 0 }, { "lfo1Sync", 1 },
            { "lfo1SyncRate", 15 }, { "lfo1Dest", 5 }, { "lfo1Depth", 0.35f }, { "lfo2Shape", 4 }, { "lfo2Sync", 1 },
            { "lfo2SyncRate", 13 }, { "lfo2Dest", 2 }, { "lfo2Depth", 0.1f }, { "delayTime", 9 }, { "delayFeedback", 0.5f },
            { "delayTone", -0.5f }, { "delayMix", 0.2f }, { "reverbSize", 0.9f }, { "reverbDamp", 0.7f }, { "reverbMix", 0.5f },
            { "scale", 1 }, { "rootNote", 4 }, { "seqOctave", -1 }, { "seqLength", 16 }, { "seqRate", 0 }, { "accent", 0.0f },
            { "outGain", -6.3f } },
          "@g100 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~ 0~", nullptr },

        { "Whale Fall", "ABYSS", "AMBIENT", 124.0,
          { { "oscBRatio", 1 }, { "fmIndex", 2.0f }, { "fmFeedback", 0.15f }, { "legato", 1 }, { "glide", 600 }, { "filterType", 0 },
            { "cutoff", 1100 }, { "resonance", 0.3f }, { "ampAttack", 400 }, { "ampDecay", 2500 }, { "ampSustain", 0.6f },
            { "ampRelease", 2500 }, { "lfo1Shape", 0 }, { "lfo1Sync", 1 }, { "lfo1SyncRate", 14 }, { "lfo1Dest", 1 },
            { "lfo1Depth", 0.08f }, { "delayTime", 7 }, { "delayFeedback", 0.6f }, { "delayTone", -0.5f }, { "delayMix", 0.3f },
            { "reverbSize", 0.9f }, { "reverbDamp", 0.7f }, { "reverbMix", 0.45f }, { "scale", 1 }, { "rootNote", 2 },
            { "seqOctave", -1 }, { "seqLength", 16 }, { "seqRate", 0 }, { "accent", 0.0f }, { "outGain", -7.1f } },
          "@g85 0 . . . . . 7~ 5 . . . . 1 . . .", nullptr },
    };
    return presets;
}
