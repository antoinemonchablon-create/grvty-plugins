// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "dsp/Shape.h"

//==============================================================================
/**
    Courbes perso : texte (état du plugin, presets) et fichiers « mes courbes » (.ebbshape) dans
    ~/Library/Audio/Presets/GRVTY Records/Ebb/Courbes/ (anciennes courbes du dossier Jasperr recopiées une fois).
    Thread de l'interface uniquement.
*/
namespace ShapeLibrary
{
    /** "x,y,c;x,y,c;..." (9 chiffres significatifs : chaque nombre revient exactement à l'identique). */
    inline juce::String toText (const Ebb::ShapeData& s)
    {
        auto num = [] (float v) { return juce::String::formatted ("%.9g", (double) v); };
        juce::StringArray parts;
        for (int i = 0; i < s.count; ++i)
        {
            const auto& p = s.pts[(size_t) i];
            parts.add (num (p.x) + "," + num (p.y) + "," + num (p.curve));
        }
        return parts.joinIntoString (";");
    }

    inline bool fromText (const juce::String& text, Ebb::ShapeData& out)
    {
        Ebb::ShapeData s;
        for (auto& token : juce::StringArray::fromTokens (text, ";", ""))
        {
            auto v = juce::StringArray::fromTokens (token, ",", "");
            if (v.size() < 2 || s.count >= Ebb::ShapeData::maxPoints) continue;
            s.pts[(size_t) s.count++] = { v[0].getFloatValue(), v[1].getFloatValue(), v.size() > 2 ? v[2].getFloatValue() : 0.0f };
        }
        if (s.count < 2)
            return false;
        Ebb::ShapeMath::sanitize (s);
        out = s;
        return true;
    }

    inline juce::File folder()
    {
        // emplacement standard des presets sur Mac : ~/Library/Audio/Presets/<fabricant>/<plugin>
        const auto presetsDir = juce::File::getSpecialLocation (juce::File::userHomeDirectory).getChildFile ("Library/Audio/Presets");
        const auto folder = presetsDir.getChildFile ("GRVTY Records/Ebb/Courbes");
        // jusqu'à la version 1.0.2, le fabricant s'appelait « Jasperr » : on recopie une fois les anciennes courbes
        // (l'ancien dossier est laissé tel quel, rien n'est effacé)
        static bool migrated = false;
        if (! migrated)
        {
            migrated = true;
            const auto old = presetsDir.getChildFile ("Jasperr/Ebb/Courbes");
            if (old.isDirectory() && ! folder.exists())
                old.copyDirectoryTo (folder);
        }
        return folder;
    }

    inline juce::String legalName (const juce::String& raw) { return juce::File::createLegalFileName (raw.trim()); }
    inline juce::File fileFor (const juce::String& rawName) { return folder().getChildFile (legalName (rawName) + ".ebbshape"); }

    inline juce::Array<juce::File> list()
    {
        auto files = folder().findChildFiles (juce::File::findFiles, false, "*.ebbshape");
        files.sort();
        return files;
    }

    inline bool save (const juce::String& rawName, const Ebb::ShapeData& s)
    {
        const auto name = legalName (rawName);
        if (name.isEmpty() || ! folder().createDirectory())
            return false;
        juce::XmlElement xml ("EBBSHAPE");
        xml.setAttribute ("version", 1);
        xml.setAttribute ("points", toText (s));
        return xml.writeTo (fileFor (name));
    }

    inline bool load (const juce::File& f, Ebb::ShapeData& out)
    {
        auto xml = juce::XmlDocument::parse (f);
        if (xml == nullptr || ! xml->hasTagName ("EBBSHAPE"))
            return false;
        return fromText (xml->getStringAttribute ("points"), out);
    }
}
