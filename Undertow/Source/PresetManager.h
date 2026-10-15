// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "Params.h"
#include "seq/Pattern.h"
#include "presets/FactoryPresets.h"

//==============================================================================
/**
    Presets d'Undertow (à appeler depuis le thread de l'interface uniquement) :
      - presets d'usine, rangés par banque (HARDGROOVE, HYPNOTIC, MENTAL) : son + motif
      - presets utilisateur en fichiers .utpreset dans ~/Library/Audio/Presets/GRVTY Records/Undertow/
        (anciens presets du dossier Jasperr recopiés une fois)
      - Random : son au hasard dans des plages musicales (le motif est gardé), ou motif au hasard

    Le mode SEQ/KEYS et la qualité Eco/HQ ne sont jamais modifiés par un preset : ce sont tes réglages.
*/
class PresetManager
{
public:
    PresetManager (juce::AudioProcessorValueTreeState& s, PatternExchange& p) : state (s), pattern (p) {}

    static const std::vector<FactoryPreset>& getFactoryPresets() { return getFactoryPresetData(); }
    static int getNumFactoryPresets() { return (int) getFactoryPresets().size(); }

    //==============================================================================
    /** Lit un motif écrit en texte (voir FactoryPresets.h). */
    static Pattern parsePattern (const juce::String& text, const char* lane)
    {
        Pattern p;
        auto tokens = juce::StringArray::fromTokens (text, " \t\n", "");
        tokens.removeEmptyStrings();

        float defGate = 0.5f, defVel = 0.8f;
        int step = 0;
        for (auto tok : tokens)
        {
            if (tok.startsWithChar ('@'))
            {
                if (tok.startsWith ("@g")) defGate = tok.substring (2).getFloatValue() / 100.0f;
                if (tok.startsWith ("@v")) defVel  = tok.substring (2).getFloatValue() / 100.0f;
                continue;
            }
            if (step >= Pattern::maxSteps)
                break;

            auto& s = p.steps[(size_t) step++];
            s = Step {};
            s.gate = defGate;
            s.velocity = defVel;
            if (tok == ".")
                continue;

            s.on = true;
            int i = 0;
            juce::String num;
            if (tok[i] == '-') num << tok[i++];
            while (i < tok.length() && juce::CharacterFunctions::isDigit (tok[i])) num << tok[i++];
            const int semis = num.getIntValue();
            const int oct = (int) std::floor (semis / 12.0);
            s.octave = juce::jlimit (-2, 2, oct);
            s.note = semis - 12 * oct;

            auto readNumber = [&tok, &i]
            {
                juce::String v;
                while (i < tok.length() && (juce::CharacterFunctions::isDigit (tok[i]) || tok[i] == '.' || tok[i] == '-')) v << tok[i++];
                return v.getFloatValue();
            };

            while (i < tok.length())
            {
                const auto c = tok[i++];
                switch (c)
                {
                    case '!': s.accent = true; break;
                    case '~': s.slide = true; break;
                    case 'x': s.ratchet = juce::jlimit (1, 4, (int) readNumber()); break;
                    case '?': s.probability = juce::jlimit (0.0f, 1.0f, readNumber() / 100.0f); break;
                    case 'g': s.gate = juce::jlimit (0.05f, 1.0f, readNumber() / 100.0f); break;
                    case 'v': s.velocity = juce::jlimit (0.0f, 1.0f, readNumber() / 100.0f); break;
                    case 'i': s.lockIndex = juce::jlimit (0.0f, 10.0f, readNumber()); break;
                    case 'c': s.lockCutoff = juce::jlimit (20.0f, 20000.0f, readNumber()); break;
                    default: break;
                }
            }
        }
        // les pas restants (au-delà de la longueur écrite) reprennent les mêmes réglages par défaut
        for (int k = step; k < Pattern::maxSteps; ++k)
        {
            p.steps[(size_t) k].gate = defGate;
            p.steps[(size_t) k].velocity = defVel;
        }

        if (lane != nullptr)
        {
            auto values = juce::StringArray::fromTokens (lane, " ", "");
            values.removeEmptyStrings();
            for (int k = 0; k < juce::jmin (values.size(), Pattern::maxSteps); ++k)
                p.modLane[(size_t) k] = juce::jlimit (-1.0f, 1.0f, values[k].getFloatValue());
        }
        return p;
    }

    //==============================================================================
    void loadFactoryPreset (int index)
    {
        const auto& presets = getFactoryPresets();
        if (! juce::isPositiveAndBelow (index, (int) presets.size()))
            return;

        const auto& preset = presets[(size_t) index];
        resetToDefaults();
        for (const auto& [id, value] : preset.values)
            set (id, value);

        pattern.edit() = parsePattern (preset.pattern, preset.lane);
        pattern.publish();

        currentFactoryIndex = index;
        setCurrentName (preset.name, juce::String (preset.bank) + " - " + preset.type);
    }

    //==============================================================================
    static juce::File getUserFolder()
    {
        // emplacement standard des presets sur Mac : ~/Library/Audio/Presets/<fabricant>/<plugin>
        const auto presets = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Audio/Presets");
        const auto folder = presets.getChildFile ("GRVTY Records/Undertow");
        // jusqu'à la version 1.0.5, le fabricant s'appelait « Jasperr » : on recopie une fois les anciens presets
        // (l'ancien dossier est laissé tel quel, rien n'est effacé)
        static bool migrated = false;
        if (! migrated)
        {
            migrated = true;
            const auto old = presets.getChildFile ("Jasperr/Undertow");
            if (old.isDirectory() && ! folder.exists())
                old.copyDirectoryTo (folder);
        }
        return folder;
    }

    static juce::String toPresetName (const juce::String& raw) { return juce::File::createLegalFileName (raw.trim()); }
    static juce::File getUserPresetFile (const juce::String& raw) { return getUserFolder().getChildFile (toPresetName (raw) + ".utpreset"); }

    juce::Array<juce::File> getUserPresets() const
    {
        auto files = getUserFolder().findChildFiles (juce::File::findFiles, false, "*.utpreset");
        files.sort();
        return files;
    }

    bool saveUserPreset (const juce::String& rawName)
    {
        const auto name = toPresetName (rawName);
        if (name.isEmpty())
            return false;

        setCurrentName (name, "USER");
        auto tree = state.copyState();
        if (auto old = tree.getChildWithName ("PATTERN"); old.isValid())
            tree.removeChild (old, nullptr);
        tree.appendChild (PatternIO::toValueTree (pattern.get()), nullptr);

        auto xml = tree.createXml();
        if (xml == nullptr)
            return false;

        if (! getUserFolder().createDirectory())
            return false;
        currentFactoryIndex = -1;
        return xml->writeTo (getUserPresetFile (name));
    }

    bool loadUserPreset (const juce::File& file)
    {
        auto xml = juce::XmlDocument::parse (file);
        if (xml == nullptr || ! xml->hasTagName (state.state.getType()))
            return false;

        auto loaded = juce::ValueTree::fromXml (*xml);
        for (auto* p : state.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                if (isKeptParam (rp->getParameterID()))
                    continue;
                auto child = loaded.getChildWithProperty ("id", rp->getParameterID());
                setParam (rp, child.isValid() ? (float) child.getProperty ("value")
                                              : rp->convertFrom0to1 (rp->getDefaultValue()));
            }

        if (auto pat = loaded.getChildWithName ("PATTERN"); pat.isValid())
        {
            pattern.edit() = PatternIO::fromValueTree (pat);
            pattern.publish();
        }

        currentFactoryIndex = -1;
        setCurrentName (file.getFileNameWithoutExtension(), "USER");
        return true;
    }

    //==============================================================================
    /** Son au hasard, dans des plages qui restent musicales. Le motif et les réglages du séquenceur ne bougent pas. */
    void randomiseSound()
    {
        auto& r = random;
        auto range  = [&r] (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };
        auto logR   = [&r] (float lo, float hi) { return lo * std::pow (hi / lo, r.nextFloat()); };
        auto chance = [&r] (float p) { return r.nextFloat() < p; };
        namespace id = ParamIDs;

        set (id::oscAShape, chance (0.7f) ? range (0.0f, 0.3f) : range (0.3f, 1.0f));
        set (id::oscACoarse, 0.0f);
        set (id::oscAFine, 0.0f);
        set (id::oscBShape, chance (0.75f) ? 0.0f : range (0.0f, 0.5f));
        static const int musicalRatios[] = { 1, 1, 3, 3, 5, 6, 0, 2, 7 };
        set (id::oscBRatio, (float) musicalRatios[r.nextInt (9)]);
        const bool freeRatio = chance (0.15f);
        set (id::oscBFree, freeRatio ? 1.0f : 0.0f);
        set (id::oscBFreeRatio, logR (1.2f, 4.0f));
        set (id::oscBFine, 0.0f);
        set (id::oscMix, chance (0.7f) ? 0.0f : range (0.0f, 0.3f));
        set (id::fmIndex, logR (0.3f, 4.0f));
        set (id::fmFeedback, std::pow (r.nextFloat(), 2.0f) * 0.55f);
        set (id::fmMode, chance (0.15f) ? 1.0f : 0.0f);

        set (id::modAttack, 0.5f);
        set (id::modDecay, logR (30.0f, 600.0f));
        set (id::modCurve, range (-0.8f, 0.2f));
        set (id::modToIndex, range (0.0f, 6.0f));
        set (id::modToCutoff, chance (0.5f) ? range (0.0f, 30.0f) : 0.0f);
        set (id::modToPitch, chance (0.1f) ? range (12.0f, 36.0f) : 0.0f);

        const float ft = r.nextFloat();
        set (id::filterType, ft < 0.75f ? 0.0f : (ft < 0.9f ? 1.0f : 2.0f));
        set (id::filterSlope, chance (0.4f) ? 1.0f : 0.0f);
        set (id::cutoff, ft < 0.9f ? logR (400.0f, 8000.0f) : logR (80.0f, 600.0f));
        set (id::resonance, range (0.0f, 0.7f));
        set (id::filterEnv, range (0.0f, 36.0f));
        set (id::filterDrive, range (0.0f, 0.5f));
        set (id::keyTrack, range (0.0f, 0.5f));

        set (id::ampAttack, chance (0.8f) ? range (0.2f, 2.0f) : logR (2.0f, 30.0f));
        set (id::ampDecay, logR (60.0f, 800.0f));
        set (id::ampSustain, chance (0.4f) ? 0.0f : range (0.0f, 0.6f));
        set (id::ampRelease, logR (20.0f, 400.0f));

        for (int i = 0; i < Params::numLfos; ++i)
        {
            const bool on = chance (i == 0 ? 0.5f : 0.3f);
            static const int dests[] = { Choices::dIndex, Choices::dIndex, Choices::dCutoff, Choices::dCutoff, Choices::dFeedback, Choices::dPan };
            static const int rates[] = { 2, 4, 5, 7, 11, 13, 14, 15 };
            set (id::lfoDest (i), on ? (float) dests[r.nextInt (6)] : 0.0f);
            set (id::lfoShape (i), (float) r.nextInt (5));
            set (id::lfoSync (i), 1.0f);
            set (id::lfoSyncRate (i), (float) rates[r.nextInt (8)]);
            set (id::lfoDepth (i), range (0.1f, 0.35f));
            set (id::lfoRetrig (i), 0.0f);
        }

        set (id::fxDrive, chance (0.5f) ? range (0.0f, 0.3f) : 0.0f);
        static const int delays[] = { 2, 4, 5, 6 };
        set (id::delayTime, (float) delays[r.nextInt (4)]);
        set (id::delayFeedback, range (0.2f, 0.5f));
        set (id::delayTone, range (-0.3f, 0.3f));
        set (id::delayMix, chance (0.5f) ? range (0.05f, 0.25f) : 0.0f);
        set (id::reverbSize, range (0.3f, 0.8f));
        set (id::reverbMix, range (0.0f, 0.2f));
        set (id::outGain, -10.0f);

        currentFactoryIndex = -1;
        setCurrentName ("Random #" + juce::String (++randomCount), "RANDOM");
    }

    /** Motif au hasard : rythme euclidien + notes dans la gamme. */
    void randomisePattern()
    {
        auto& r = random;
        const int len = (int) state.getRawParameterValue (ParamIDs::seqLength)->load();
        const int scale = (int) state.getRawParameterValue (ParamIDs::scale)->load();
        auto& p = pattern.edit();
        PatternOps::euclidean (p, len, juce::jlimit (2, len, (int) std::round (len * (0.35f + 0.35f * r.nextFloat()))), r.nextInt (4));
        PatternOps::randomNotes (p, len, scale, (uint32_t) r.nextInt());
        for (int i = 0; i < len; ++i)
        {
            auto& s = p.steps[(size_t) i];
            s.accent = r.nextFloat() < 0.2f;
            s.slide = r.nextFloat() < 0.1f;
            s.ratchet = r.nextFloat() < 0.06f ? 2 : 1;
            s.probability = r.nextFloat() < 0.12f ? 0.6f : 1.0f;
        }
        pattern.publish();
    }

    //==============================================================================
    juce::String getCurrentName() const { return state.state.getProperty ("presetName", "Init").toString(); }
    juce::String getCurrentBank() const { return state.state.getProperty ("presetBank", "").toString(); }
    int getCurrentFactoryIndex() const  { return currentFactoryIndex; }

    /** Réglages que les presets ne touchent jamais. */
    static bool isKeptParam (const juce::String& id) { return id == ParamIDs::seqMode || id == ParamIDs::oversampling; }

private:
    void resetToDefaults()
    {
        for (auto* p : state.processor.getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
                if (! isKeptParam (rp->getParameterID()))
                    setParam (rp, rp->convertFrom0to1 (rp->getDefaultValue()));
    }

    void set (const juce::String& id, float realValue)
    {
        if (auto* p = state.getParameter (id))
            setParam (p, realValue);
        else
            jassertfalse;   // identifiant inconnu dans un preset
    }

    static void setParam (juce::RangedAudioParameter* p, float realValue)
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
        p->endChangeGesture();
    }

    void setCurrentName (const juce::String& name, const juce::String& bank)
    {
        state.state.setProperty ("presetName", name, nullptr);
        state.state.setProperty ("presetBank", bank, nullptr);
    }

    juce::AudioProcessorValueTreeState& state;
    PatternExchange& pattern;
    juce::Random random;
    int currentFactoryIndex = -1;
    int randomCount = 0;
};
