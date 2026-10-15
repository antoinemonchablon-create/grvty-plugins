// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>

/*
    Panneau « À propos » et son bouton « i », communs aux plugins GRVTY Records (même fichier dans Flow Filter,
    Undertow et Ebb ; Confluence a sa propre version). Le panneau ne dépend que de JUCE : il a ses propres couleurs
    et ses propres polices, détruites avec lui.
*/
namespace About
{
    /** Adresse publique du code source (dépôt), fixée à la compilation (CMake : <PLUGIN>_SOURCE_URL). Vide = pas encore publié. */
    inline juce::String sourceUrl()
    {
       #ifdef GRVTY_SOURCE_URL
        return GRVTY_SOURCE_URL;
       #else
        return {};
       #endif
    }

    inline juce::String licenceUrl() { return "https://www.gnu.org/licenses/agpl-3.0.html"; }

    /** Nom de l'archive du code source publiée avec chaque version, ex. « Flow-Filter-1.0.4-source.zip ». */
    inline juce::String sourceArchiveName (const juce::String& productName, const juce::String& version)
    {
        return productName.replaceCharacter (' ', '-') + "-" + version + "-source.zip";
    }
}

//==============================================================================
/** Petit bouton rond « i » (dessiné, sans texte) qui ouvre le panneau À propos. */
class InfoButton : public juce::Button
{
public:
    explicit InfoButton (const juce::String& productName);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

//==============================================================================
/**
    Panneau « À propos » dessiné dans la fenêtre du plugin : les mentions qu'une interface doit afficher sous la
    licence AGPL (copyright, absence de garantie, droit de redistribuer, où lire la licence), le code source,
    la confidentialité, les composants tiers et les marques. Échap, FERMER ou un clic à côté le referment.
*/
class AboutPanel : public juce::Component
{
public:
    /** productName : « Flow Filter » ; version : « 1.0.4 » (JucePlugin_VersionString) ; withAudioUnit : le plugin existe aussi en AU. */
    AboutPanel (const juce::String& productName, const juce::String& version, bool withAudioUnit, std::function<void()> onClose);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    bool keyPressed (const juce::KeyPress&) override;
    void parentHierarchyChanged() override;

    void close();

    /** Tout le texte affiché (banc de tests : vérifie que les mentions obligatoires sont là). */
    juce::String getAllText() const;
    bool hasVstLogo() const noexcept { return vstLogo.isValid(); }
    /** Le panneau tient-il entièrement dans la fenêtre ? (banc de tests) */
    bool fitsInside() const noexcept { return getLocalBounds().contains (panelR); }

private:
    struct Paragraph { juce::String text; float size; bool bold; juce::Colour colour; int height; };

    juce::String name, version;
    std::function<void()> onClose;
    std::vector<Paragraph> paragraphs;
    std::vector<juce::TextLayout> layouts;   // mis en page une fois par taille
    std::vector<juce::Rectangle<int>> paraR;   // place de chaque paragraphe
    juce::Rectangle<int> panelR, licenceR, sourceR, closeR, logoR;
    juce::Image vstLogo;
    // Steinberg demande au moins 20 x 18,7 mm : 110 x 103 px reste au-dessus même fenêtre réduite à 70 %
    static constexpr int logoW = 110, logoH = 103;
    int gap = 8;
    bool hasSource = false, closed = false;
    int hot = 0;
};
