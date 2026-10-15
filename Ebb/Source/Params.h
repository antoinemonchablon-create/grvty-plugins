// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "dsp/Curves.h"

//==============================================================================
/**
    Tous les paramètres d'Ebb.

    RÈGLE ABSOLUE : ces identifiants, leurs plages et la liste de leurs choix sont figés depuis la phase 1.
    Ne jamais renommer, supprimer ni changer la plage d'un ID existant : les projets Ableton,
    l'automation et les presets les référencent. On peut seulement en AJOUTER de nouveaux.
*/
namespace ParamIDs
{
    inline constexpr const char* amount    = "amount";     // profondeur de la baisse, 0-100 %
    inline constexpr const char* curve     = "curve";      // 12 courbes
    inline constexpr const char* rate      = "rate";       // durée de la courbe (1/16 ... 1 mesure)
    inline constexpr const char* trigger   = "trigger";    // SYNC / MIDI / AUDIO
    inline constexpr const char* nudge     = "nudge";      // décalage en mode SYNC, -20 à +20 ms
    inline constexpr const char* threshold = "threshold";  // seuil du déclenchement AUDIO, -60 à 0 dB

    // --- ajoutés en phase 2 (courbe perso, Tension, Lissage, multibande)
    inline constexpr const char* custom    = "custom";     // la courbe perso (dessinée) remplace la courbe d'usine
    inline constexpr const char* tension   = "tension";    // -100 (molle) à +100 % (sèche)
    inline constexpr const char* smooth    = "smooth";     // lissage 0-100 %
    inline constexpr const char* band      = "band";       // Plein / Graves / Aigus
    inline constexpr const char* crossover = "crossover";  // fréquence de séparation, 40-2000 Hz
}

namespace Params
{
    inline juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

    inline juce::StringArray curveNames()
    {
        juce::StringArray a;
        for (int i = 0; i < Ebb::numCurves; ++i) a.add (Ebb::curveName (i));
        return a;
    }

    inline juce::StringArray rateNames()
    {
        juce::StringArray a;
        for (int i = 0; i < Ebb::numRates; ++i) a.add (u8 (Ebb::rateName (i)));
        return a;
    }

    inline juce::StringArray triggerNames() { return { "SYNC", "MIDI", "AUDIO" }; }
    inline juce::StringArray bandNames()    { return { "Plein", "Graves", "Aigus" }; }

    enum Band { bandFull = 0, bandLow, bandHigh };

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        AudioProcessorValueTreeState::ParameterLayout layout;
        auto pid = [] (const char* id) { return ParameterID { id, 1 }; };

        layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::amount), "Amount",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 80.0f,
            AudioParameterFloatAttributes().withLabel ("%")
                .withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " %"; })));

        layout.add (std::make_unique<AudioParameterChoice> (pid (ParamIDs::curve), "Curve", curveNames(), Ebb::curveRipple));
        layout.add (std::make_unique<AudioParameterChoice> (pid (ParamIDs::rate), "Rate", rateNames(), Ebb::rateQuarter));
        layout.add (std::make_unique<AudioParameterChoice> (pid (ParamIDs::trigger), "Trigger", triggerNames(), 0));

        layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::nudge), "Nudge",
            NormalisableRange<float> (-20.0f, 20.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("ms")
                .withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 1) + " ms"; })));

        layout.add (std::make_unique<AudioParameterFloat> (pid (ParamIDs::threshold), "Threshold",
            NormalisableRange<float> (-60.0f, 0.0f, 0.1f), -20.0f,
            AudioParameterFloatAttributes().withLabel ("dB")
                .withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; })));

        // --- phase 2 : versionHint 2 (nouveaux paramètres ajoutés après les 6 premiers, jamais à leur place)
        auto pid2 = [] (const char* id) { return ParameterID { id, 2 }; };

        layout.add (std::make_unique<AudioParameterBool> (pid2 (ParamIDs::custom), "Custom Curve", false));

        layout.add (std::make_unique<AudioParameterFloat> (pid2 (ParamIDs::tension), "Tension",
            NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("%")
                .withStringFromValueFunction ([] (float v, int) { return (v > 0.04f ? "+" : "") + String (v, 0) + " %"; })));

        layout.add (std::make_unique<AudioParameterFloat> (pid2 (ParamIDs::smooth), "Smooth",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 0.0f,
            AudioParameterFloatAttributes().withLabel ("%")
                .withStringFromValueFunction ([] (float v, int) { return String (v, 0) + " %"; })));

        layout.add (std::make_unique<AudioParameterChoice> (pid2 (ParamIDs::band), "Band", bandNames(), bandFull));

        NormalisableRange<float> xr (40.0f, 2000.0f, 1.0f);
        xr.setSkewForCentre (250.0f);
        layout.add (std::make_unique<AudioParameterFloat> (pid2 (ParamIDs::crossover), "Crossover", xr, 150.0f,
            AudioParameterFloatAttributes().withLabel ("Hz")
                .withStringFromValueFunction ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (juce::roundToInt (v)) + " Hz"; })));

        return layout;
    }
}
