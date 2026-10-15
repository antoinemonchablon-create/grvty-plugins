// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Flow Filter, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

// Petit banc de tests de Flow Filter : son sans NaN ni dépassement, état sauvegardé / rechargé,
// captures de l'interface et panneau « À propos ». Lancement : ./FlowFilterTests (résultats dans captures/).

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    int failures = 0;
    void check (bool ok, const juce::String& name, const juce::String& detail)
    {
        if (! ok) ++failures;
        std::cout << (ok ? "OK    " : "ECHEC ") << name << " : " << detail << std::endl;
    }
    void writePng (const juce::Image& img, const juce::File& f)
    {
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    const auto dir = juce::File::getCurrentWorkingDirectory().getChildFile ("captures");
    dir.createDirectory();

    std::unique_ptr<FlowFilterAudioProcessor> proc (dynamic_cast<FlowFilterAudioProcessor*> (createPluginFilter()));

    // son : 10 s de bruit à 48 kHz, blocs de 512, chaque preset d'usine
    {
        proc->setPlayConfigDetails (2, 2, 48000.0, 512);
        proc->prepareToPlay (48000.0, 512);
        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        juce::Random rnd (1);
        bool finite = true; float peak = 0.0f;
        for (int pr = 0; pr < PresetManager::getNumFactoryPresets(); ++pr)
        {
            proc->presets.loadFactoryPreset (pr);
            for (int b = 0; b < 940; ++b)
            {
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i) buf.setSample (c, i, (rnd.nextFloat() * 2.0f - 1.0f) * 0.5f);
                proc->processBlock (buf, midi);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 512; ++i)
                    {
                        const float v = buf.getSample (c, i);
                        if (! std::isfinite (v)) finite = false;
                        peak = juce::jmax (peak, std::abs (v));
                    }
            }
        }
        check (finite, "Son", juce::String (PresetManager::getNumFactoryPresets()) + " presets, aucun NaN / inf ; pic "
                                + juce::String (juce::Decibels::gainToDecibels (peak), 1) + " dB");
    }

    // état : sauvegarde puis rechargement dans un second plugin
    {
        juce::MemoryBlock mb;
        proc->presets.loadFactoryPreset (3);
        proc->getStateInformation (mb);
        std::unique_ptr<FlowFilterAudioProcessor> other (dynamic_cast<FlowFilterAudioProcessor*> (createPluginFilter()));
        other->setStateInformation (mb.getData(), (int) mb.getSize());
        int diff = 0;
        for (auto* p : proc->getParameters())
            for (auto* q : other->getParameters())
                if (auto* pp = dynamic_cast<juce::AudioProcessorParameterWithID*> (p))
                    if (auto* qq = dynamic_cast<juce::AudioProcessorParameterWithID*> (q))
                        if (pp->paramID == qq->paramID && std::abs (p->getValue() - q->getValue()) > 1.0e-6f) ++diff;
        check (diff == 0, "Etat", "parametres identiques apres rechargement (" + juce::String (diff) + " difference)");
    }

    // interface : captures 1x / 2x et panneau À propos
    std::unique_ptr<juce::AudioProcessorEditor> edBase (proc->createEditor());
    auto* ed = dynamic_cast<FlowFilterAudioProcessorEditor*> (edBase.get());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);
    for (float s : { 1.0f, 2.0f })
        writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, s), dir.getChildFile ("flowfilter_" + juce::String ((int) s) + "x.png"));
    check (true, "Captures", "captures/flowfilter_1x.png et 2x");

    {
        ed->getInfoButton().triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        auto* about = ed->getAboutPanel();
        writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f), dir.getChildFile ("about_2x.png"));
        const auto text = about != nullptr ? about->getAllText() : juce::String();
        juce::StringArray missing;
        for (auto need : { "GRVTY Records", "Jasperr", "GNU Affero General Public License", "version 3", "SANS AUCUNE GARANTIE", "redistribuer",
                           "LICENSE", "agpl-3.0", "Code source", "ne collecte aucune donn", "THIRD-PARTY-NOTICES",
                           "Steinberg", "Apple", "Ableton AG", "NOTICE", "dans les limites permises par la loi", JucePlugin_VersionString })
            if (! text.contains (juce::String::fromUTF8 (need))) missing.add (need);
        const bool shown = about != nullptr && about->isVisible();
        if (about != nullptr && ! about->hasVstLogo()) missing.add ("logo VST Compatible");
        if (about != nullptr && ! about->fitsInside()) missing.add ("panneau plus grand que la fenetre");
        if (about != nullptr) about->close();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        check (shown && missing.isEmpty() && ! ed->isAboutOpen(), "Panneau A propos (about_2x.png)",
               ! shown ? juce::String ("le panneau ne s'ouvre pas")
                       : ! missing.isEmpty() ? "manque : " + missing.joinIntoString (", ")
                       : ed->isAboutOpen() ? juce::String ("ne se referme pas") : juce::String ("mentions completes ; se referme"));
    }

    edBase.reset();
    std::cout << (failures == 0 ? "TOUS LES TESTS OK" : juce::String (failures) + " ECHEC(S)") << std::endl;
    return failures == 0 ? 0 : 1;
}
