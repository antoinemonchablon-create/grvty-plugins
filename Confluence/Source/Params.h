// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "theory/Theory.h"
#include "theory/Voicing.h"
#include "engine/Resolver.h"

//==============================================================================
/**
    Tous les paramètres de Confluence.

    RÈGLE ABSOLUE : ces identifiants, leurs plages et la liste de leurs choix sont figés depuis la phase 1.
    Ne jamais renommer, supprimer ni changer la plage d'un ID existant : les projets Ableton,
    l'automation et les presets les référencent. On peut seulement en AJOUTER de nouveaux.

    Les pads (8 accords), l'accord appris et le nom du preset ne sont pas des paramètres :
    ils sont enregistrés dans l'état du plugin (projet Ableton) et dans les presets.
*/
namespace ParamIDs
{
    // --- Accord
    inline constexpr const char* mode          = "mode";
    inline constexpr const char* root          = "root";
    inline constexpr const char* scale         = "scale";
    inline constexpr const char* outOfScale    = "outOfScale";
    inline constexpr const char* chordType     = "chordType";
    inline constexpr const char* blackKeys     = "blackKeys";
    inline constexpr const char* fixedShape    = "fixedShape";
    inline constexpr const char* style         = "style";
    inline constexpr const char* padBase       = "padBase";

    // --- Voicing
    inline constexpr const char* inversion     = "inversion";
    inline constexpr const char* spread        = "spread";
    inline constexpr const char* maxVoices     = "maxVoices";
    inline constexpr const char* lowNote       = "lowNote";
    inline constexpr const char* highNote      = "highNote";
    inline constexpr const char* bass          = "bass";
    inline constexpr const char* octave        = "octave";
    inline constexpr const char* transpose     = "transpose";

    // --- Jeu
    inline constexpr const char* strum         = "strum";
    inline constexpr const char* strumDir      = "strumDir";
    inline constexpr const char* strumSync     = "strumSync";
    inline constexpr const char* strumSyncRate = "strumSyncRate";
    inline constexpr const char* humanTime     = "humanTime";
    inline constexpr const char* humanVel      = "humanVel";
    inline constexpr const char* velMode       = "velMode";
    inline constexpr const char* velFixed      = "velFixed";
    inline constexpr const char* velCurve      = "velCurve";
    inline constexpr const char* balBass       = "balBass";
    inline constexpr const char* balMid        = "balMid";
    inline constexpr const char* balTop        = "balTop";
    inline constexpr const char* durMode       = "durMode";
    inline constexpr const char* duration      = "duration";
    inline constexpr const char* latch         = "latch";
    inline constexpr const char* outChannel    = "outChannel";

    // --- Voix Preview
    inline constexpr const char* preview       = "preview";
    inline constexpr const char* previewLevel  = "previewLevel";
}

namespace Choices
{
    inline juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

    inline juce::StringArray modes()      { return { u8 ("Gamme"), u8 ("Touches blanches"), u8 ("Accord fixe"), u8 ("Pads") }; }
    inline juce::StringArray roots()      { return { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" }; }
    inline juce::StringArray scales()
    {
        juce::StringArray a;
        for (int i = 0; i < Theory::numScales; ++i) a.add (u8 (Theory::scaleInfo (i).name));
        return a;
    }
    inline juce::StringArray outOfScale() { return { u8 ("Plus proche"), u8 ("Au-dessus"), u8 ("En dessous"), u8 ("Ignorer") }; }
    inline juce::StringArray chordTypes()
    {
        juce::StringArray a;
        for (int i = 0; i < Theory::numChordTypes; ++i) a.add (u8 (Theory::chordTypeName (i)));
        return a;
    }
    inline juce::StringArray blackKeys()  { return { "Sus4", "Sus2", u8 ("9e"), u8 ("Mode parall\xc3\xa8le"), u8 ("Muettes") }; }
    inline juce::StringArray fixedShapes()
    {
        juce::StringArray a;
        for (int i = 0; i < Theory::numFixedShapes; ++i) a.add (u8 (Theory::fixedShapeName (i)));
        return a;
    }
    inline juce::StringArray styles()     { return { "Deep / House", "Hypnotic", "Tech House", "Mental" }; }
    inline juce::StringArray inversions() { return { "0", "1", "2", "3", "Auto" }; }
    inline juce::StringArray spreads()
    {
        juce::StringArray a;
        for (int i = 0; i < Voicing::numSpreads; ++i) a.add (u8 (Voicing::spreadName (i)));
        return a;
    }
    inline juce::StringArray bass()       { return { "Off", "-1 oct", "-2 oct" }; }
    inline juce::StringArray strumDirs()  { return { "Haut", "Bas", u8 ("Altern\xc3\xa9"), u8 ("Al\xc3\xa9" "atoire") }; }
    inline juce::StringArray strumRates() { return { "1/128", "1/64T", "1/64", "1/32T", "1/32", "1/16T", "1/16" }; }
    inline juce::StringArray velModes()   { return { "Touche", "Fixe", "Courbe" }; }
    inline juce::StringArray durModes()   { return { "Tenue", "Fixe" }; }
    inline juce::StringArray durations()  { return { "1/32", "1/16T", "1/16", "1/16.", "1/8T", "1/8", "1/8.", "1/4", "1/2", "1/1" }; }

    /** Durées en noires. */
    inline double strumRateBeats (int i)
    {
        static const double v[] = { 1.0 / 32.0, 1.0 / 24.0, 1.0 / 16.0, 1.0 / 12.0, 1.0 / 8.0, 1.0 / 6.0, 1.0 / 4.0 };
        return v[juce::jlimit (0, 6, i)];
    }
    inline double durationBeats (int i)
    {
        static const double v[] = { 0.125, 1.0 / 6.0, 0.25, 0.375, 1.0 / 3.0, 0.5, 0.75, 1.0, 2.0, 4.0 };
        return v[juce::jlimit (0, 9, i)];
    }
}

namespace Params
{
    /** Nom d'une note façon Ableton (C3 = 60). */
    inline juce::String noteName (int n) { return juce::MidiMessage::getMidiNoteName (n, true, true, 3); }

    /** Lit un nom de note (« C3 », « F#1 », « Bb-1 ») ou un numéro. */
    inline int parseNote (const juce::String& text)
    {
        auto t = text.trim();
        if (t.containsOnly ("0123456789")) return t.getIntValue();
        static const int base[] = { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
        const auto c = juce::CharacterFunctions::toUpperCase (t[0]);
        if (c < 'A' || c > 'G') return t.getIntValue();
        int pc = base[c - 'A'];
        int i = 1;
        if (t[i] == '#') { ++pc; ++i; }
        else if (t[i] == 'b' || t[i] == 'B') { --pc; ++i; }
        return juce::jlimit (0, 127, pc + 12 * (t.substring (i).getIntValue() + 2));
    }

    inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
    {
        using namespace juce;
        namespace id = ParamIDs;
        using C = AudioParameterChoice;
        using I = AudioParameterInt;
        using F = AudioParameterFloat;
        using B = AudioParameterBool;
        auto pid = [] (const char* s) { return ParameterID { s, 1 }; };
        auto u8 = Choices::u8;

        auto noteAttr = AudioParameterIntAttributes()
                            .withStringFromValueFunction ([] (int v, int) { return noteName (v); })
                            .withValueFromStringFunction ([] (const String& s) { return parseNote (s); });
        auto msAttr  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, v < 10.0f ? 1 : 0) + " ms"; });
        auto pctAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
        auto velAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String::charToString (0xb1) + String (roundToInt (v)); });
        auto curveAttr = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2); });
        auto dbAttr  = AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + " dB"; });
        auto signedInt = AudioParameterIntAttributes().withStringFromValueFunction ([] (int v, int) { return v > 0 ? "+" + String (v) : String (v); });

        AudioProcessorValueTreeState::ParameterLayout l;

        // --- Accord
        l.add (std::make_unique<C> (pid (id::mode),       "Mode",            Choices::modes(), 0));
        l.add (std::make_unique<C> (pid (id::root),       "Tonique",         Choices::roots(), 9));
        l.add (std::make_unique<C> (pid (id::scale),      "Gamme",           Choices::scales(), (int) Theory::naturalMinor));
        l.add (std::make_unique<C> (pid (id::outOfScale), "Hors gamme",      Choices::outOfScale(), 0));
        l.add (std::make_unique<C> (pid (id::chordType),  "Type d'accord",   Choices::chordTypes(), (int) Theory::ctSeventh));
        l.add (std::make_unique<C> (pid (id::blackKeys),  "Touches noires",  Choices::blackKeys(), 0));
        l.add (std::make_unique<C> (pid (id::fixedShape), "Accord fixe",     Choices::fixedShapes(), 5));
        l.add (std::make_unique<C> (pid (id::style),      "Style",           Choices::styles(), 1));
        l.add (std::make_unique<I> (pid (id::padBase),    u8 ("Pads : 1re note"), 0, 120, 36, noteAttr));

        // --- Voicing
        l.add (std::make_unique<C> (pid (id::inversion),  "Renversement",    Choices::inversions(), 4));
        l.add (std::make_unique<C> (pid (id::spread),     u8 ("\xc3\x89" "cartement"), Choices::spreads(), 0));
        l.add (std::make_unique<I> (pid (id::maxVoices),  "Voix max",        2, 8, 5));
        l.add (std::make_unique<I> (pid (id::lowNote),    "Note la plus basse", 0, 127, 45, noteAttr));
        l.add (std::make_unique<I> (pid (id::highNote),   "Note la plus haute", 0, 127, 88, noteAttr));
        l.add (std::make_unique<C> (pid (id::bass),       "Basse",           Choices::bass(), 0));
        l.add (std::make_unique<I> (pid (id::octave),     "Octave",          -3, 3, 0, signedInt));
        l.add (std::make_unique<I> (pid (id::transpose),  "Transposition",   -12, 12, 0, signedInt));

        // --- Jeu
        l.add (std::make_unique<F> (pid (id::strum),      "Strum",           NormalisableRange<float> (0.0f, 200.0f, 0.1f, 0.45f), 0.0f, msAttr));
        l.add (std::make_unique<C> (pid (id::strumDir),   "Strum : sens",    Choices::strumDirs(), 0));
        l.add (std::make_unique<B> (pid (id::strumSync),  "Strum : synchro", false));
        l.add (std::make_unique<C> (pid (id::strumSyncRate), "Strum : valeur", Choices::strumRates(), 4));
        l.add (std::make_unique<F> (pid (id::humanTime),  "Humanize : timing", NormalisableRange<float> (0.0f, 40.0f, 0.1f, 0.6f), 0.0f, msAttr));
        l.add (std::make_unique<F> (pid (id::humanVel),   "Humanize : velocity", NormalisableRange<float> (0.0f, 40.0f, 1.0f), 0.0f, velAttr));
        l.add (std::make_unique<C> (pid (id::velMode),    "Velocity : mode", Choices::velModes(), 0));
        l.add (std::make_unique<I> (pid (id::velFixed),   "Velocity fixe",   1, 127, 100));
        l.add (std::make_unique<F> (pid (id::velCurve),   "Velocity : courbe", NormalisableRange<float> (-1.0f, 1.0f, 0.01f), 0.0f, curveAttr));
        l.add (std::make_unique<F> (pid (id::balBass),    u8 ("\xc3\x89" "quilibre basse"), NormalisableRange<float> (0.25f, 1.5f, 0.01f), 1.0f, pctAttr));
        l.add (std::make_unique<F> (pid (id::balMid),     u8 ("\xc3\x89" "quilibre milieu"), NormalisableRange<float> (0.25f, 1.5f, 0.01f), 1.0f, pctAttr));
        l.add (std::make_unique<F> (pid (id::balTop),     u8 ("\xc3\x89" "quilibre aigu"), NormalisableRange<float> (0.25f, 1.5f, 0.01f), 1.0f, pctAttr));
        l.add (std::make_unique<C> (pid (id::durMode),    u8 ("Dur\xc3\xa9" "e : mode"), Choices::durModes(), 0));
        l.add (std::make_unique<C> (pid (id::duration),   u8 ("Dur\xc3\xa9" "e"), Choices::durations(), 2));
        l.add (std::make_unique<B> (pid (id::latch),      "Latch",           false));
        l.add (std::make_unique<I> (pid (id::outChannel), "Canal de sortie", 1, 16, 1));

        // --- Voix Preview (désactivée par défaut : le plugin ne sort que du MIDI)
        l.add (std::make_unique<B> (pid (id::preview),    "Preview",         false));
        l.add (std::make_unique<F> (pid (id::previewLevel), "Preview : niveau", NormalisableRange<float> (-36.0f, 0.0f, 0.1f), -12.0f, dbAttr));
        return l;
    }
}
