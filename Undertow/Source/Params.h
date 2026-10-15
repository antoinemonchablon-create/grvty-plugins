// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Tous les paramètres d'Undertow.

    RÈGLE ABSOLUE : ces identifiants sont figés depuis la phase 1.
    Ne jamais renommer, supprimer ni changer la plage d'un ID existant :
    les projets Ableton et les presets les référencent. On peut seulement en AJOUTER.
*/
namespace ParamIDs
{
    // --- Voix
    inline constexpr const char* legato        = "legato";
    inline constexpr const char* glide         = "glide";
    inline constexpr const char* velSens       = "velSens";

    // --- Oscillateur A (porteur)
    inline constexpr const char* oscAShape     = "oscAShape";
    inline constexpr const char* oscACoarse    = "oscACoarse";
    inline constexpr const char* oscAFine      = "oscAFine";

    // --- Oscillateur B (modulateur)
    inline constexpr const char* oscBShape     = "oscBShape";
    inline constexpr const char* oscBRatio     = "oscBRatio";
    inline constexpr const char* oscBFree      = "oscBFree";
    inline constexpr const char* oscBFreeRatio = "oscBFreeRatio";
    inline constexpr const char* oscBFine      = "oscBFine";
    inline constexpr const char* oscMix        = "oscMix";

    // --- FM
    inline constexpr const char* fmIndex       = "fmIndex";
    inline constexpr const char* fmFeedback    = "fmFeedback";
    inline constexpr const char* fmMode        = "fmMode";

    // --- Enveloppe Mod (AD)
    inline constexpr const char* modAttack     = "modAttack";
    inline constexpr const char* modDecay      = "modDecay";
    inline constexpr const char* modCurve      = "modCurve";
    inline constexpr const char* modToIndex    = "modToIndex";
    inline constexpr const char* modToCutoff   = "modToCutoff";
    inline constexpr const char* modToPitch    = "modToPitch";

    // --- Filtre
    inline constexpr const char* filterType    = "filterType";
    inline constexpr const char* filterSlope   = "filterSlope";
    inline constexpr const char* cutoff        = "cutoff";
    inline constexpr const char* resonance     = "resonance";
    inline constexpr const char* filterEnv     = "filterEnv";
    inline constexpr const char* filterDrive   = "filterDrive";
    inline constexpr const char* keyTrack      = "keyTrack";

    // --- Enveloppe d'ampli (ADSR)
    inline constexpr const char* ampAttack     = "ampAttack";
    inline constexpr const char* ampDecay      = "ampDecay";
    inline constexpr const char* ampSustain    = "ampSustain";
    inline constexpr const char* ampRelease    = "ampRelease";

    // --- LFO 1 et 2
    inline juce::String lfoShape    (int i) { return "lfo" + juce::String (i + 1) + "Shape"; }
    inline juce::String lfoSync     (int i) { return "lfo" + juce::String (i + 1) + "Sync"; }
    inline juce::String lfoRate     (int i) { return "lfo" + juce::String (i + 1) + "Rate"; }
    inline juce::String lfoSyncRate (int i) { return "lfo" + juce::String (i + 1) + "SyncRate"; }
    inline juce::String lfoDest     (int i) { return "lfo" + juce::String (i + 1) + "Dest"; }
    inline juce::String lfoDepth    (int i) { return "lfo" + juce::String (i + 1) + "Depth"; }
    inline juce::String lfoRetrig   (int i) { return "lfo" + juce::String (i + 1) + "Retrig"; }

    // --- Effets
    inline constexpr const char* fxDrive       = "fxDrive";
    inline constexpr const char* delayTime     = "delayTime";
    inline constexpr const char* delayFeedback = "delayFeedback";
    inline constexpr const char* delayTone     = "delayTone";
    inline constexpr const char* delayMix      = "delayMix";
    inline constexpr const char* reverbSize    = "reverbSize";
    inline constexpr const char* reverbDamp    = "reverbDamp";
    inline constexpr const char* reverbMix     = "reverbMix";

    // --- Global
    inline constexpr const char* oversampling  = "oversampling";
    inline constexpr const char* outGain       = "outGain";

    // --- Séquenceur (réglages globaux ; les pas eux-mêmes sont dans l'état du plugin)
    inline constexpr const char* seqMode       = "seqMode";
    inline constexpr const char* seqLength     = "seqLength";
    inline constexpr const char* seqRate       = "seqRate";
    inline constexpr const char* swing         = "swing";
    inline constexpr const char* scale         = "scale";
    inline constexpr const char* rootNote      = "rootNote";
    inline constexpr const char* seqOctave     = "seqOctave";
    inline constexpr const char* accent        = "accent";
    inline constexpr const char* modLaneDest   = "modLaneDest";
    inline constexpr const char* modLaneLength = "modLaneLength";
    inline constexpr const char* modLaneDepth  = "modLaneDepth";
    inline constexpr const char* mutateAmount  = "mutateAmount";
    inline constexpr const char* mutateBars    = "mutateBars";
}

//==============================================================================
namespace Choices
{
    inline const juce::StringArray ratios       { "0.5", "1", "1.5", "2", "2.5", "3", "3.5", "4", "5", "6", "7", "8" };
    inline constexpr float ratioValues[]        { 0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f };
    inline constexpr int   numRatios            = 12;

    inline const juce::StringArray fmModes      { "B > A", "A <> B" };
    inline const juce::StringArray filterTypes  { "LP", "BP", "HP" };
    inline const juce::StringArray filterSlopes { "12 dB", "24 dB" };
    inline const juce::StringArray lfoShapes    { "Sine", "Triangle", "Ramp", "Square", "S&H" };

    /** Divisions synchro des LFO (en noires). Les divisions impaires (3/16, 5/16, 7/16) servent la polymétrie. */
    inline const juce::StringArray lfoSyncRates { "1/32", "1/16T", "1/16", "1/8T", "1/8", "3/16", "1/4T", "1/4",
                                                  "5/16", "7/16", "3/8", "1/2", "3/4", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" };
    inline constexpr double lfoSyncQuarters[]   { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0,
                                                  1.25, 1.75, 1.5, 2.0, 3.0, 4.0, 8.0, 16.0, 32.0, 64.0 };

    inline const juce::StringArray lfoDests     { "Off", "Pitch", "FM Index", "Ratio", "Feedback", "Cutoff", "Resonance",
                                                  "Shape A", "Shape B", "Osc Mix", "Drive", "Amp", "Pan" };
    enum LfoDest { dOff = 0, dPitch, dIndex, dRatio, dFeedback, dCutoff, dResonance, dShapeA, dShapeB, dMix, dDrive, dAmp, dPan, numLfoDests };

    inline const juce::StringArray delayTimes   { "1/32", "1/16T", "1/16", "1/8T", "1/8", "3/16", "1/4T", "1/4", "3/8", "1/2" };
    inline constexpr double delayQuarters[]     { 0.125, 1.0 / 6.0, 0.25, 1.0 / 3.0, 0.5, 0.75, 2.0 / 3.0, 1.0, 1.5, 2.0 };

    inline const juce::StringArray oversampling { "Eco 2x", "HQ 4x" };

    inline const juce::StringArray seqModes     { "SEQ", "KEYS" };
    inline const juce::StringArray seqRates     { "1/8", "1/16", "1/32", "1/8T", "1/16T", "1/32T" };
    inline constexpr double seqRateQuarters[]   { 0.5, 0.25, 0.125, 1.0 / 3.0, 1.0 / 6.0, 1.0 / 12.0 };

    inline const juce::StringArray scales       { "Minor", "Phrygian", "Dorian", "Harmonic Minor", "Minor Penta", "Chromatic" };
    inline const juce::StringArray noteNames    { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    inline const juce::StringArray modLaneDests { "Off", "FM Index", "Cutoff", "Ratio" };
    enum ModLaneDest { laneOff = 0, laneIndex, laneCutoff, laneRatio };
    inline const juce::StringArray mutateBars   { "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" };
    inline constexpr int mutateBarValues[]      { 1, 2, 4, 8, 16 };
}

//==============================================================================
namespace Params
{
    inline constexpr int numLfos = 2;

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        using F = AudioParameterFloat;
        using C = AudioParameterChoice;
        using B = AudioParameterBool;
        using I = AudioParameterInt;
        namespace id = ParamIDs;

        std::vector<std::unique_ptr<RangedAudioParameter>> params;
        auto add = [&] (auto p) { params.push_back (std::move (p)); };

        auto fmt = [] (auto fn) { return AudioParameterFloatAttributes().withStringFromValueFunction (fn); };
        auto pct   = fmt ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
        auto bip   = fmt ([] (float v, int) { return (v > 0.0005f ? "+" : "") + String (roundToInt (v * 100.0f)) + " %"; });
        auto db    = fmt ([] (float v, int) { return String (v, 1) + " dB"; });
        auto cents = fmt ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (roundToInt (v)) + " ct"; });
        auto semis = fmt ([] (float v, int) { return (v > 0.05f ? "+" : "") + String (v, 1) + " st"; });
        auto num   = fmt ([] (float v, int) { return String (v, 2); });
        auto hz    = fmt ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " kHz" : String (v, v < 10.0f ? 2 : 0) + " Hz"; });
        auto ms    = fmt ([] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (v, v < 10.0f ? 1 : 0) + " ms"; });

        auto skewed = [] (float lo, float hi, float centre, float step = 0.0f)
        {
            NormalisableRange<float> r (lo, hi, step);
            r.setSkewForCentre (centre);
            return r;
        };
        const NormalisableRange<float> unit (0.0f, 1.0f), bipolar (-1.0f, 1.0f);

        // --- Voix
        add (std::make_unique<B> (ParameterID { id::legato, 1 }, "Legato", true));
        add (std::make_unique<F> (ParameterID { id::glide, 1 },  "Glide", skewed (0.0f, 2000.0f, 150.0f), 0.0f, ms));
        add (std::make_unique<F> (ParameterID { id::velSens, 1 }, "Velocity", unit, 0.5f, pct));

        // --- Oscillateurs
        add (std::make_unique<F> (ParameterID { id::oscAShape, 1 },  "Shape A", unit, 0.0f, pct));
        add (std::make_unique<I> (ParameterID { id::oscACoarse, 1 }, "Coarse A", -24, 24, 0));
        add (std::make_unique<F> (ParameterID { id::oscAFine, 1 },   "Fine A", NormalisableRange<float> (-100.0f, 100.0f), 0.0f, cents));
        add (std::make_unique<F> (ParameterID { id::oscBShape, 1 },  "Shape B", unit, 0.0f, pct));
        add (std::make_unique<C> (ParameterID { id::oscBRatio, 1 },  "Ratio B", Choices::ratios, 1));
        add (std::make_unique<B> (ParameterID { id::oscBFree, 1 },   "Free Ratio", false));
        add (std::make_unique<F> (ParameterID { id::oscBFreeRatio, 1 }, "Free Ratio B", skewed (0.25f, 16.0f, 2.0f, 0.001f), 1.41f, num));
        add (std::make_unique<F> (ParameterID { id::oscBFine, 1 },   "Fine B", NormalisableRange<float> (-100.0f, 100.0f), 0.0f, cents));
        add (std::make_unique<F> (ParameterID { id::oscMix, 1 },     "Osc Mix", unit, 0.0f,
                                  fmt ([] (float v, int) { return "A " + String (roundToInt ((1.0f - v) * 100.0f)) + " / B " + String (roundToInt (v * 100.0f)); })));

        // --- FM
        add (std::make_unique<F> (ParameterID { id::fmIndex, 1 },    "FM Index", skewed (0.0f, 10.0f, 2.5f), 1.5f, num));
        add (std::make_unique<F> (ParameterID { id::fmFeedback, 1 }, "Feedback", unit, 0.0f, pct));
        add (std::make_unique<C> (ParameterID { id::fmMode, 1 },     "FM Mode", Choices::fmModes, 0));

        // --- Enveloppe Mod
        add (std::make_unique<F> (ParameterID { id::modAttack, 1 },  "Mod Attack", skewed (0.1f, 2000.0f, 40.0f), 0.5f, ms));
        add (std::make_unique<F> (ParameterID { id::modDecay, 1 },   "Mod Decay",  skewed (5.0f, 5000.0f, 300.0f), 250.0f, ms));
        add (std::make_unique<F> (ParameterID { id::modCurve, 1 },   "Mod Curve",  bipolar, -0.4f, bip));
        add (std::make_unique<F> (ParameterID { id::modToIndex, 1 }, "Mod > Index", NormalisableRange<float> (-10.0f, 10.0f), 0.0f, num));
        add (std::make_unique<F> (ParameterID { id::modToCutoff, 1 },"Mod > Cutoff", NormalisableRange<float> (-60.0f, 60.0f), 0.0f, semis));
        add (std::make_unique<F> (ParameterID { id::modToPitch, 1 }, "Mod > Pitch", NormalisableRange<float> (-48.0f, 48.0f), 0.0f, semis));

        // --- Filtre
        add (std::make_unique<C> (ParameterID { id::filterType, 1 },  "Filter Type", Choices::filterTypes, 0));
        add (std::make_unique<C> (ParameterID { id::filterSlope, 1 }, "Filter Slope", Choices::filterSlopes, 0));
        add (std::make_unique<F> (ParameterID { id::cutoff, 1 },      "Cutoff", skewed (20.0f, 20000.0f, 800.0f, 0.1f), 20000.0f, hz));
        add (std::make_unique<F> (ParameterID { id::resonance, 1 },   "Resonance", unit, 0.1f, pct));
        add (std::make_unique<F> (ParameterID { id::filterEnv, 1 },   "Filter Env", NormalisableRange<float> (-60.0f, 60.0f), 0.0f, semis));
        add (std::make_unique<F> (ParameterID { id::filterDrive, 1 }, "Filter Drive", unit, 0.0f, pct));
        add (std::make_unique<F> (ParameterID { id::keyTrack, 1 },    "Key Track", unit, 0.0f, pct));

        // --- Ampli
        add (std::make_unique<F> (ParameterID { id::ampAttack, 1 },  "Attack",  skewed (0.1f, 5000.0f, 50.0f), 0.5f, ms));
        add (std::make_unique<F> (ParameterID { id::ampDecay, 1 },   "Decay",   skewed (1.0f, 5000.0f, 300.0f), 300.0f, ms));
        add (std::make_unique<F> (ParameterID { id::ampSustain, 1 }, "Sustain", unit, 0.8f, pct));
        add (std::make_unique<F> (ParameterID { id::ampRelease, 1 }, "Release", skewed (1.0f, 8000.0f, 300.0f), 60.0f, ms));

        // --- LFO
        for (int i = 0; i < numLfos; ++i)
        {
            const String n ("LFO " + String (i + 1));
            add (std::make_unique<C> (ParameterID { id::lfoShape (i), 1 },    n + " Shape", Choices::lfoShapes, 0));
            add (std::make_unique<B> (ParameterID { id::lfoSync (i), 1 },     n + " Sync", true));
            add (std::make_unique<F> (ParameterID { id::lfoRate (i), 1 },     n + " Rate", skewed (0.01f, 30.0f, 2.0f, 0.001f), 1.0f, hz));
            add (std::make_unique<C> (ParameterID { id::lfoSyncRate (i), 1 }, n + " Sync Rate", Choices::lfoSyncRates, 7));
            add (std::make_unique<C> (ParameterID { id::lfoDest (i), 1 },     n + " Dest", Choices::lfoDests, 0));
            add (std::make_unique<F> (ParameterID { id::lfoDepth (i), 1 },    n + " Depth", bipolar, 0.0f, bip));
            add (std::make_unique<B> (ParameterID { id::lfoRetrig (i), 1 },   n + " Retrig", false));
        }

        // --- Effets
        add (std::make_unique<F> (ParameterID { id::fxDrive, 1 },       "Drive", unit, 0.0f, pct));
        add (std::make_unique<C> (ParameterID { id::delayTime, 1 },     "Delay Time", Choices::delayTimes, 5));
        add (std::make_unique<F> (ParameterID { id::delayFeedback, 1 }, "Delay Feedback", NormalisableRange<float> (0.0f, 0.95f), 0.35f, pct));
        add (std::make_unique<F> (ParameterID { id::delayTone, 1 },     "Delay Tone", bipolar, 0.0f,
                                  fmt ([] (float v, int) { return std::abs (v) < 0.01f ? String ("Flat")
                                                                  : (v < 0.0f ? "LP " : "HP ") + String (roundToInt (std::abs (v) * 100.0f)) + " %"; })));
        add (std::make_unique<F> (ParameterID { id::delayMix, 1 },      "Delay Mix", unit, 0.0f, pct));
        add (std::make_unique<F> (ParameterID { id::reverbSize, 1 },    "Reverb Size", unit, 0.4f, pct));
        add (std::make_unique<F> (ParameterID { id::reverbDamp, 1 },    "Reverb Damp", unit, 0.5f, pct));
        add (std::make_unique<F> (ParameterID { id::reverbMix, 1 },     "Reverb Mix", unit, 0.0f, pct));

        // --- Global
        add (std::make_unique<C> (ParameterID { id::oversampling, 1 }, "Quality", Choices::oversampling, 0));
        add (std::make_unique<F> (ParameterID { id::outGain, 1 },      "Volume", NormalisableRange<float> (-36.0f, 6.0f, 0.1f), -6.0f, db));

        // --- Séquenceur
        add (std::make_unique<C> (ParameterID { id::seqMode, 1 },       "Mode", Choices::seqModes, 0));
        add (std::make_unique<I> (ParameterID { id::seqLength, 1 },     "Steps", 1, 32, 16));
        add (std::make_unique<C> (ParameterID { id::seqRate, 1 },       "Step Rate", Choices::seqRates, 1));
        add (std::make_unique<F> (ParameterID { id::swing, 1 },         "Swing", NormalisableRange<float> (0.5f, 0.75f), 0.5f, pct));
        add (std::make_unique<C> (ParameterID { id::scale, 1 },         "Scale", Choices::scales, 0));
        add (std::make_unique<C> (ParameterID { id::rootNote, 1 },      "Root", Choices::noteNames, 0));
        add (std::make_unique<I> (ParameterID { id::seqOctave, 1 },     "Octave", -3, 3, 0));
        add (std::make_unique<F> (ParameterID { id::accent, 1 },        "Accent", unit, 0.5f, pct));
        add (std::make_unique<C> (ParameterID { id::modLaneDest, 1 },   "Mod Lane", Choices::modLaneDests, 0));
        add (std::make_unique<I> (ParameterID { id::modLaneLength, 1 }, "Mod Steps", 1, 32, 16));
        add (std::make_unique<F> (ParameterID { id::modLaneDepth, 1 },  "Mod Lane Depth", unit, 0.5f, pct));
        add (std::make_unique<F> (ParameterID { id::mutateAmount, 1 },  "Mutate", unit, 0.0f, pct));
        add (std::make_unique<C> (ParameterID { id::mutateBars, 1 },    "Mutate Every", Choices::mutateBars, 2));

        return { params.begin(), params.end() };
    }
}
