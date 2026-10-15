// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "engine/Resolver.h"
#include "presets/FactoryPresets.h"

class ConfluenceAudioProcessor;

//==============================================================================
/**
    Presets de Confluence (thread de l'interface uniquement) :
      - presets d'usine rangés par banque (DEEP & HOUSE, HYPNOTIC & DUB, TECH HOUSE & STABS) :
        réglages + 8 pads (+ accord appris) ;
      - presets utilisateur (.cfpreset) : Mac ~/Library/Audio/Presets/GRVTY Records/Confluence/
        (les anciens presets de « Jasperr/Confluence » y sont recopiés), Windows Documents/GRVTY Records/Confluence/Presets ;
      - dé : réglages au hasard dans des plages musicales, et pads regénérés.

    Un preset ne touche jamais au canal de sortie, à la voix Preview ni à la note de départ des pads.
    Changer de preset coupe toutes les notes (All Notes Off).
*/
class PresetManager
{
public:
    explicit PresetManager (ConfluenceAudioProcessor& p) : proc (p) {}

    static const std::vector<FactoryPreset>& getFactoryPresets() { return getFactoryPresetData(); }
    static int getNumFactoryPresets() { return (int) getFactoryPresets().size(); }

    void loadFactoryPreset (int index);

    static juce::File getUserFolder();
    /** Le dossier des presets tel qu'on le décrit à l'utilisateur (Finder / Explorateur). */
    static juce::String getUserFolderDisplayName();
    static juce::String toPresetName (const juce::String& raw) { return juce::File::createLegalFileName (raw.trim()); }
    static juce::File getUserPresetFile (const juce::String& raw) { return getUserFolder().getChildFile (toPresetName (raw) + ".cfpreset"); }
    juce::Array<juce::File> getUserPresets() const;
    bool saveUserPreset (const juce::String& rawName);
    bool loadUserPreset (const juce::File&);

    /** Dé : réglages au hasard dans des plages musicales (tonique et mode gardés), pads regénérés selon le style. */
    void randomise();
    /** Seulement les pads, au hasard selon le style et la gamme. */
    void randomisePads();

    juce::String getCurrentName() const;
    juce::String getCurrentBank() const;
    int getCurrentFactoryIndex() const noexcept { return currentFactoryIndex; }

    /** Réglages que les presets ne touchent jamais. */
    static bool isKeptParam (const juce::String& id);

    /** Lit les 8 pads écrits en texte (voir FactoryPresets.h). */
    static Engine::PadSet parsePads (const char* text);
    static Engine::LearnedShape parseLearned (const char* text);

private:
    void resetToDefaults();
    void set (const juce::String& id, float realValue);
    void setCurrentName (const juce::String& name, const juce::String& bank);

    ConfluenceAudioProcessor& proc;
    juce::Random random;
    int currentFactoryIndex = -1;
    int randomCount = 0;
};
