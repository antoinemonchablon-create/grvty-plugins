// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "LiquidUI.h"

namespace About
{
    /** Adresse publique du code source (dépôt), fixée à la compilation (CMake : CF_SOURCE_URL). Vide = pas encore publié. */
    inline juce::String sourceUrl()
    {
       #ifdef CONFLUENCE_SOURCE_URL
        return CONFLUENCE_SOURCE_URL;
       #else
        return {};
       #endif
    }

    inline juce::String licenceUrl() { return "https://www.gnu.org/licenses/agpl-3.0.html"; }
}

//==============================================================================
/** Petit bouton rond « i » (dessiné, sans texte) qui ouvre le panneau À propos. */
class InfoButton : public juce::Button
{
public:
    InfoButton() : juce::Button ("infos")
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
        setTooltip (juce::String::fromUTF8 ("\xc3\x80 propos : licence, code source, confidentialit\xc3\xa9"));
        setTitle (juce::String::fromUTF8 ("\xc3\x80 propos de Confluence"));
    }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

//==============================================================================
/**
    Panneau « À propos » dessiné dans la fenêtre du plugin (comme le panneau Enregistrer) :
    les mentions légales qu'une interface doit afficher sous la licence AGPL (copyright, absence de garantie,
    droit de redistribuer, où lire la licence), le code source, la confidentialité, les composants tiers et les marques.
    Échap, FERMER ou un clic à côté le referment.
*/
class AboutPanel : public juce::Component
{
public:
    AboutPanel (const juce::String& version, std::function<void()> onClose);

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

private:
    struct Paragraph { juce::String text; float size; bool bold; juce::Colour colour; int height; };

    juce::String version;
    std::function<void()> onClose;
    std::vector<Paragraph> paragraphs;
    std::vector<juce::TextLayout> layouts;   // mis en page une fois par taille
    juce::Rectangle<int> panelR, licenceR, sourceR, closeR, logoR;
    juce::Image vstLogo;
    // Steinberg demande au moins 20 x 18,7 mm : 110 x 103 px reste au-dessus même fenêtre réduite à 70 %
    static constexpr int logoW = 110, logoH = 103;
    bool hasSource = false, closed = false;
    int hot = 0;
};
