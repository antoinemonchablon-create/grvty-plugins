// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "LiquidUI.h"

//==============================================================================
/** Menu déroulant avec son nom au-dessus, relié à un paramètre à choix (repris d'Undertow et de Confluence). */
class ChoiceControl : public juce::Component
{
public:
    ChoiceControl (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& name);
    void resized() override;

private:
    juce::Label title;
    juce::ComboBox box;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

//==============================================================================
/** Bouton pilule (action), avec une icône dessinée facultative. 'lit' : allumé (mode actif). */
class PillButton : public juce::Button
{
public:
    enum class Icon { none, pencil, undo, redo, mirror, invert, clear, save, folder, list };

    explicit PillButton (const juce::String& label, Icon i = Icon::none) : juce::Button (label), icon (i)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

    bool lit = false;
    juce::Colour accent = Ui::aqua;
    float fontSize = 8.6f;
    Icon icon = Icon::none;
};

//==============================================================================
/** Rangée (ou colonne) de pilules dont une seule est allumée, reliée à un paramètre à choix. */
class PillGroup : public juce::Component
{
public:
    PillGroup (juce::RangedAudioParameter&, juce::StringArray labels, bool vertical = false);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    int getSelected() const noexcept { return selected; }
    /** Banc de tests : clique sur la pilule i. */
    void clickIndex (int i);
    float fontSize = 8.4f;

private:
    int indexAt (juce::Point<float>) const;
    juce::Rectangle<float> cell (int i) const;

    juce::StringArray labels;
    bool vertical;
    int selected = 0, hovered = -1;
    juce::ParameterAttachment attachment;
};

//==============================================================================
/** Pilules sans paramètre (réglage de l'interface, ex. la grille de l'éditeur). */
class PillChooser : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit PillChooser (juce::StringArray labels);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;

    void setSelected (int i) { selected = i; repaint(); }
    int getSelected() const noexcept { return selected; }
    std::function<void (int)> onChange;
    float fontSize = 8.2f;

private:
    juce::StringArray labels;
    int selected = 0;
};
