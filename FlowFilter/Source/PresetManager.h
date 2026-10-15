// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Flow Filter, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>

//==============================================================================
/**
    Presets de Flow Filter :
      - 10 presets d'usine (aussi visibles dans la liste de presets d'Ableton)
      - presets utilisateur enregistrés en fichiers .ffpreset dans
        ~/Library/Audio/Presets/GRVTY Records/Flow Filter/ (anciens presets du dossier Jasperr recopiés une fois)
      - Random : tire des réglages au hasard, dans des plages qui restent musicales

    Toutes les fonctions sont à appeler depuis le thread de l'interface (message thread).
*/
class PresetManager
{
public:
    struct FactoryPreset
    {
        const char* name;
        std::vector<std::pair<const char*, float>> values;   // id -> valeur réelle (non normalisée)
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState& s) : state (s)
    {
        getUserFolder().createDirectory();
    }

    //==============================================================================
    static const std::vector<FactoryPreset>& getFactoryPresets()
    {
        // Les paramètres non listés reprennent leur valeur par défaut.
        // syncDiv : 0=1/16 1=1/8 2=1/4 3=1/2 4=1 bar 5=2 bars 6=4 bars 7=8 bars 8=16 bars 9=32 bars
        // modXShape : 0=Sine 1=Triangle 2=Ramp 3=Square 4=Random   (mod1=Offset, mod2=Invert, mod3=Gravity)
        static const std::vector<FactoryPreset> presets
        {
            { "Still Water",     { { "rate", 0.08f }, { "multiply", 1.0f }, { "ripple", 0.10f }, { "depth", 12.0f },
                                   { "gravity", 0.50f }, { "analog", 0.30f }, { "width", 0.60f }, { "resonance", 3.0f },
                                   { "release", 250.0f } } },

            { "Tidal Sweep",     { { "sync", 1.0f }, { "syncDiv", 4.0f }, { "multiply", 1.0f }, { "ripple", 0.0f },
                                   { "depth", 36.0f }, { "gravity", 0.55f }, { "resonance", 6.0f }, { "release", 80.0f } } },

            { "Rain on Glass",   { { "outGain", 3.5f }, { "rate", 2.5f }, { "multiply", 6.0f }, { "ripple", 0.80f }, { "depth", 30.0f },
                                   { "gravity", 0.70f }, { "resonance", 9.0f }, { "release", 20.0f } } },

            { "Deep Current",    { { "outGain", -5.0f }, { "rate", 0.12f }, { "multiply", 0.75f }, { "ripple", 0.25f }, { "depth", 42.0f },
                                   { "gravity", 0.60f }, { "areaLow", 40.0f }, { "areaHigh", 800.0f }, { "areaSlope", 1.0f },
                                   { "analog", 0.50f }, { "resonance", 5.0f }, { "release", 180.0f } } },

            { "Hypnotic Pulse",  { { "outGain", 8.0f }, { "sync", 1.0f }, { "syncDiv", 2.0f }, { "scan", 1.0f }, { "multiply", 2.0f },
                                   { "ripple", 0.15f }, { "depth", 40.0f }, { "gravity", 0.80f }, { "resonance", 5.0f },
                                   { "release", 25.0f } } },

            { "Riptide",         { { "outGain", 5.5f }, { "rate", 1.2f }, { "reverse", 1.0f }, { "multiply", 3.0f }, { "ripple", 0.60f },
                                   { "invert", -1.0f }, { "depth", 45.0f }, { "gravity", 0.55f }, { "resonance", 6.5f },
                                   { "mod1Shape", 0.0f }, { "mod1Rate", 0.3f }, { "mod1Depth", 0.5f } } },

            { "Surface Shimmer", { { "outGain", -2.5f }, { "rate", 1.5f }, { "multiply", 5.0f }, { "ripple", 0.50f }, { "depth", 30.0f },
                                   { "areaLow", 3000.0f }, { "areaHigh", 20000.0f }, { "areaSlope", 0.8f },
                                   { "resonance", 10.0f }, { "width", 1.0f }, { "release", 40.0f } } },

            { "Whirlpool",       { { "outGain", 2.5f }, { "rate", 0.6f }, { "multiply", 4.0f }, { "ripple", 0.40f }, { "gravity", 0.45f },
                                   { "depth", 38.0f }, { "resonance", 7.0f },
                                   { "mod1Shape", 4.0f }, { "mod1Rate", 0.3f }, { "mod1Depth", 0.6f },
                                   { "mod2Shape", 0.0f }, { "mod2Rate", 0.15f }, { "mod2Depth", 0.5f } } },

            { "Frozen Lake",     { { "outGain", 8.0f }, { "freeze", 1.0f }, { "offset", 0.35f }, { "multiply", 2.5f }, { "ripple", 0.30f },
                                   { "depth", 48.0f }, { "gravity", 0.70f }, { "resonance", 14.0f }, { "release", 300.0f } } },

            { "Monsoon",         { { "outGain", 7.5f }, { "rate", 3.5f }, { "multiply", 8.0f }, { "ripple", 1.0f }, { "depth", 54.0f },
                                   { "gravity", 0.75f }, { "analog", 1.0f }, { "resonance", 12.0f }, { "release", 10.0f },
                                   { "mod1Shape", 4.0f }, { "mod1Rate", 2.0f }, { "mod1Depth", 0.7f },
                                   { "mod2Shape", 3.0f }, { "mod2Rate", 0.5f }, { "mod2Depth", 0.4f },
                                   { "mod3Shape", 1.0f }, { "mod3Rate", 0.8f }, { "mod3Depth", 0.5f } } },
        };
        return presets;
    }

    static int getNumFactoryPresets() { return (int) getFactoryPresets().size(); }

    //==============================================================================
    void loadFactoryPreset (int index)
    {
        const auto& presets = getFactoryPresets();
        if (! juce::isPositiveAndBelow (index, (int) presets.size()))
            return;

        const auto& preset = presets[(size_t) index];

        // tout revient par défaut (sauf Input et Mix), y compris Output :
        // chaque preset d'usine fixe son propre Output pour que les niveaux restent proches
        for (auto* p : state.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! isKeptParam (rp->getParameterID()))
                    setParam (rp, rp->convertFrom0to1 (rp->getDefaultValue()));

        for (const auto& [id, value] : preset.values)
            if (auto* rp = state.getParameter (id))
                setParam (rp, value);

        currentFactoryIndex = index;
        setCurrentName (preset.name);
    }

    //==============================================================================
    static juce::File getUserFolder()
    {
        // emplacement standard des presets sur Mac : ~/Library/Audio/Presets/<fabricant>/<plugin>
        const auto presets = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Audio/Presets");
        const auto folder = presets.getChildFile ("GRVTY Records/Flow Filter");
        // jusqu'à la version 1.0.3, le fabricant s'appelait « Jasperr » : on recopie une fois les anciens presets
        // (l'ancien dossier est laissé tel quel, rien n'est effacé)
        static bool migrated = false;
        if (! migrated)
        {
            migrated = true;
            const auto old = presets.getChildFile ("Jasperr/Flow Filter");
            if (old.isDirectory() && ! folder.exists())
                old.copyDirectoryTo (folder);
        }
        return folder;
    }

    juce::Array<juce::File> getUserPresets() const
    {
        auto files = getUserFolder().findChildFiles (juce::File::findFiles, false, "*.ffpreset");
        files.sort();
        return files;
    }

    bool saveUserPreset (const juce::String& rawName)
    {
        const auto name = juce::File::createLegalFileName (rawName.trim());
        if (name.isEmpty())
            return false;

        setCurrentName (name);
        auto xml = state.copyState().createXml();
        if (xml == nullptr)
            return false;

        getUserFolder().createDirectory();
        const bool ok = xml->writeTo (getUserFolder().getChildFile (name + ".ffpreset"));
        currentFactoryIndex = -1;
        return ok;
    }

    bool loadUserPreset (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName (state.state.getType()))
            return false;

        auto loaded = juce::ValueTree::fromXml (*xml);

        // on applique paramètre par paramètre (l'hôte voit les changements ; Input et Mix ne bougent pas)
        for (auto* p : state.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                if (isKeptParam (rp->getParameterID()))
                    continue;
                auto child = loaded.getChildWithProperty ("id", rp->getParameterID());
                const float v = child.isValid() ? (float) child.getProperty ("value")
                                                : rp->convertFrom0to1 (rp->getDefaultValue());
                setParam (rp, v);
            }

        currentFactoryIndex = -1;
        setCurrentName (file.getFileNameWithoutExtension());
        return true;
    }

    //==============================================================================
    /** Tire tous les réglages au hasard (sauf Input et Mix), dans des plages musicales. Output revient à 0 dB. */
    void randomise()
    {
        auto& r = random;
        auto set   = [this] (const char* id, float v) { if (auto* p = state.getParameter (id)) setParam (p, v); };
        auto range = [&r] (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };
        auto chance = [&r] (float probability) { return r.nextFloat() < probability; };

        const bool synced = chance (0.35f);
        set ("sync",     synced ? 1.0f : 0.0f);
        set ("syncDiv",  (float) r.nextInt ({ 2, 8 }));
        set ("rate",     0.02f + 2.0f * std::pow (r.nextFloat(), 2.0f));   // plutôt lent, parfois rapide
        set ("reverse",  chance (0.3f) ? 1.0f : 0.0f);
        set ("freeze",   0.0f);
        set ("outGain",  0.0f);

        set ("multiply", 0.5f + 5.5f * std::pow (r.nextFloat(), 1.5f));
        set ("ripple",   range (0.0f, 0.9f));
        set ("scan",     chance (0.3f) ? range (0.5f, 1.0f) : range (0.0f, 0.4f));
        set ("offset",   r.nextFloat());
        set ("invert",   chance (0.2f) ? range (-1.0f, -0.4f) : range (0.4f, 1.0f));
        set ("gravity",  range (0.25f, 0.85f));

        set ("depth",     range (18.0f, 48.0f));
        set ("resonance", range (2.0f, 12.0f));
        set ("areaLow",   chance (0.7f) ? 20.0f : range (40.0f, 400.0f));
        set ("areaHigh",  chance (0.7f) ? 20000.0f : range (2000.0f, 16000.0f));
        set ("areaGain",  range (-24.0f, 0.0f));
        set ("areaSlope", range (0.0f, 1.5f));
        set ("release",   range (20.0f, 400.0f));
        set ("width",     range (0.5f, 1.0f));
        set ("analog",    chance (0.5f) ? range (0.0f, 0.6f) : 0.0f);

        for (int i = 1; i <= 3; ++i)
        {
            const juce::String n (i);
            set (("mod" + n + "Shape").toRawUTF8(), (float) r.nextInt (5));
            set (("mod" + n + "Rate").toRawUTF8(),  0.05f + 2.0f * std::pow (r.nextFloat(), 2.0f));
            set (("mod" + n + "Depth").toRawUTF8(), chance (0.5f) ? 0.0f : range (0.1f, 0.6f));
        }

        currentFactoryIndex = -1;
        setCurrentName ("Random #" + juce::String (++randomCount));
    }

    //==============================================================================
    juce::String getCurrentName() const { return state.state.getProperty ("presetName", "Init").toString(); }
    int getCurrentFactoryIndex() const  { return currentFactoryIndex; }

private:
    /** Réglages que les presets ne touchent jamais : le niveau d'entrée et le dosage sec/filtré restent les tiens. */
    static bool isKeptParam (const juce::String& id) { return id == "inGain" || id == "mix"; }

    static void setParam (juce::RangedAudioParameter* p, float realValue)
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }

    void setCurrentName (const juce::String& name) { state.state.setProperty ("presetName", name, nullptr); }

    juce::AudioProcessorValueTreeState& state;
    juce::Random random;
    int currentFactoryIndex = -1;
    int randomCount = 0;
};
