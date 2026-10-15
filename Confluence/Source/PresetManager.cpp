// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "PresetManager.h"
#include "PluginProcessor.h"

//==============================================================================
bool PresetManager::isKeptParam (const juce::String& id)
{
    return id == ParamIDs::outChannel || id == ParamIDs::preview || id == ParamIDs::previewLevel || id == ParamIDs::padBase;
}

static void setParamValue (juce::RangedAudioParameter* p, float realValue)
{
    p->beginChangeGesture();
    p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    p->endChangeGesture();
}

void PresetManager::set (const juce::String& id, float realValue)
{
    if (auto* p = proc.apvts.getParameter (id))
        setParamValue (p, realValue);
    else
        jassertfalse;   // identifiant inconnu dans un preset
}

void PresetManager::resetToDefaults()
{
    for (auto* p : proc.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            if (! isKeptParam (rp->getParameterID()))
                setParamValue (rp, rp->convertFrom0to1 (rp->getDefaultValue()));
}

void PresetManager::setCurrentName (const juce::String& name, const juce::String& bank)
{
    proc.apvts.state.setProperty ("presetName", name, nullptr);
    proc.apvts.state.setProperty ("presetBank", bank, nullptr);
}

juce::String PresetManager::getCurrentName() const { return proc.apvts.state.getProperty ("presetName", "Init").toString(); }
juce::String PresetManager::getCurrentBank() const { return proc.apvts.state.getProperty ("presetBank", "").toString(); }

//==============================================================================
Engine::PadSet PresetManager::parsePads (const char* text)
{
    Engine::PadSet ps;
    if (text == nullptr) return ps;
    auto tokens = juce::StringArray::fromTokens (text, " ", "");
    tokens.removeEmptyStrings();
    static const char* typeCodes[Theory::numChordTypes] = { "5", "t", "s2", "s4", "6", "7", "9", "a9", "11", "13", "q", "c" };
    for (int i = 0; i < juce::jmin (Engine::numPads, tokens.size()); ++i)
    {
        auto parts = juce::StringArray::fromTokens (tokens[i], "/", "");
        auto& p = ps.pads[(size_t) i];
        p.degree = (int8_t) juce::jlimit (0, 11, parts[0].getIntValue() - 1);
        p.type = (int8_t) Theory::ctSeventh;
        for (int t = 0; t < Theory::numChordTypes; ++t)
            if (parts[1] == typeCodes[t]) p.type = (int8_t) t;
        p.inversion = (int8_t) (parts.size() > 2 && parts[2] != "a" ? juce::jlimit (0, 3, parts[2].getIntValue()) : Voicing::inversionAuto);
        p.octave = (int8_t) (parts.size() > 3 ? juce::jlimit (-2, 2, parts[3].getIntValue()) : 0);
    }
    return ps;
}

Engine::LearnedShape PresetManager::parseLearned (const char* text)
{
    Engine::LearnedShape l;
    if (text == nullptr) return l;
    auto tokens = juce::StringArray::fromTokens (text, " ", "");
    tokens.removeEmptyStrings();
    for (int i = 0; i < juce::jmin ((int) Theory::maxChordTones, tokens.size()); ++i)
        l.iv[l.count++] = (int8_t) juce::jlimit (0, 127, tokens[i].getIntValue());
    return l;
}

void PresetManager::loadFactoryPreset (int index)
{
    const auto& presets = getFactoryPresets();
    if (! juce::isPositiveAndBelow (index, (int) presets.size()))
        return;
    const auto& preset = presets[(size_t) index];
    resetToDefaults();
    for (const auto& [id, value] : preset.values)
        set (id, value);
    proc.setPads (parsePads (preset.pads));
    if (preset.learned != nullptr)
        proc.setLearned (parseLearned (preset.learned));
    currentFactoryIndex = index;
    setCurrentName (preset.name, preset.bank);
    proc.notifyPresetChange();
}

//==============================================================================
juce::File PresetManager::getUserFolder()
{
   #if JUCE_MAC
    // emplacement standard des presets sur Mac : ~/Library/Audio/Presets/<fabricant>/<plugin>
    const auto presets = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Audio/Presets");
    const auto folder = presets.getChildFile ("GRVTY Records/Confluence");
    // avant la version 1.0.4, le fabricant s'appelait « Jasperr » : on recopie une fois les anciens presets
    // (l'ancien dossier est laissé tel quel, rien n'est effacé)
    static bool migrated = false;
    if (! migrated)
    {
        migrated = true;
        const auto old = presets.getChildFile ("Jasperr/Confluence");
        if (old.isDirectory() && ! folder.exists())
            old.copyDirectoryTo (folder);
    }
    return folder;
   #else
    // Windows et Linux : Documents/GRVTY Records/Confluence/Presets
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("GRVTY Records/Confluence/Presets");
   #endif
}

juce::String PresetManager::getUserFolderDisplayName()
{
   #if JUCE_MAC
    return juce::String::fromUTF8 ("Biblioth\xc3\xa8que / Audio / Presets / GRVTY Records / Confluence");
   #else
    return "Documents / GRVTY Records / Confluence / Presets";
   #endif
}

juce::Array<juce::File> PresetManager::getUserPresets() const
{
    auto files = getUserFolder().findChildFiles (juce::File::findFiles, false, "*.cfpreset");
    files.sort();
    return files;
}

bool PresetManager::saveUserPreset (const juce::String& rawName)
{
    const auto name = toPresetName (rawName);
    if (name.isEmpty())
        return false;
    setCurrentName (name, "USER");
    auto xml = proc.createStateTree().createXml();
    if (xml == nullptr || ! getUserFolder().createDirectory())
        return false;
    currentFactoryIndex = -1;
    return xml->writeTo (getUserPresetFile (name));
}

bool PresetManager::loadUserPreset (const juce::File& file)
{
    auto xml = juce::XmlDocument::parse (file);
    if (xml == nullptr || ! xml->hasTagName (proc.apvts.state.getType()))
        return false;
    proc.applyStateTree (juce::ValueTree::fromXml (*xml), true);
    currentFactoryIndex = -1;
    setCurrentName (file.getFileNameWithoutExtension(), "USER");
    proc.notifyPresetChange();
    return true;
}

//==============================================================================
void PresetManager::randomise()
{
    auto& r = random;
    auto range  = [&r] (float lo, float hi) { return lo + (hi - lo) * r.nextFloat(); };
    auto chance = [&r] (float p) { return r.nextFloat() < p; };
    auto pick   = [&r] (std::initializer_list<int> v) { return *(v.begin() + r.nextInt ((int) v.size())); };
    namespace id = ParamIDs;

    const int style = (int) std::lround (proc.apvts.getRawParameterValue (id::style)->load());
    const auto scales = Harmony::styleScales (style);
    set (id::scale, (float) scales[(size_t) r.nextInt (3)]);

    switch (style)
    {
        case Harmony::styleDeep:      set (id::chordType, (float) pick ({ Theory::ctSeventh, Theory::ctNinth, Theory::ctNinth, Theory::ctEleventh, Theory::ctAdd9 })); break;
        case Harmony::styleHypnotic:  set (id::chordType, (float) pick ({ Theory::ctSus2, Theory::ctSus4, Theory::ctSeventh, Theory::ctNinth, Theory::ctTriad })); break;
        case Harmony::styleTechHouse: set (id::chordType, (float) pick ({ Theory::ctSeventh, Theory::ctSeventh, Theory::ctNinth, Theory::ctTriad })); break;
        default:                      set (id::chordType, (float) pick ({ Theory::ctTriad, Theory::ctQuartal, Theory::ctCluster, Theory::ctSus2, Theory::ctPower })); break;
    }
    set (id::inversion, chance (0.75f) ? (float) Voicing::inversionAuto : (float) r.nextInt (3));
    set (id::spread, (float) pick ({ Voicing::spClose, Voicing::spClose, Voicing::spOpen, Voicing::spDrop2, Voicing::spWide }));
    set (id::maxVoices, (float) (4 + r.nextInt (3)));
    set (id::bass, chance (0.4f) ? 1.0f : 0.0f);
    const int low = 45 + r.nextInt (10);
    set (id::lowNote, (float) low);
    set (id::highNote, (float) (low + 26 + r.nextInt (10)));

    const bool stabs = style == Harmony::styleTechHouse || (style == Harmony::styleHypnotic && chance (0.4f));
    set (id::durMode, stabs ? 1.0f : 0.0f);
    set (id::duration, (float) pick ({ 2, 3, 5, 6 }));
    set (id::strum, stabs ? 0.0f : (chance (0.6f) ? range (6.0f, 45.0f) : 0.0f));
    set (id::strumDir, (float) pick ({ 0, 0, 1, 2 }));
    set (id::humanTime, range (0.0f, 8.0f));
    set (id::humanVel, range (0.0f, 10.0f));
    set (id::velMode, stabs ? 2.0f : 0.0f);
    set (id::velCurve, stabs ? range (0.2f, 0.6f) : 0.0f);

    proc.setPads (Harmony::generatePads (style, (int) std::lround (proc.apvts.getRawParameterValue (id::scale)->load()),
                                         (uint32_t) r.nextInt() | 1u));
    currentFactoryIndex = -1;
    setCurrentName ("Random #" + juce::String (++randomCount), "RANDOM");
    proc.notifyPresetChange();
}

void PresetManager::randomisePads()
{
    proc.generatePads ((uint32_t) random.nextInt() | 1u);
}
