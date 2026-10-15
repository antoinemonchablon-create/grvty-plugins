// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "LiquidUI.h"
#include "ChordText.h"

class ConfluenceAudioProcessor;

//==============================================================================
/**
    Un pad : tuile remplie d'eau avec le nom de l'accord et son degré.
      - clic : joue l'accord du pad (relâcher = arrêter), quel que soit le mode ;
      - clic droit : menu pour choisir le degré, le type, le renversement et l'octave.
    Le pad joué se remplit et ondule.
*/
class PadTile : public juce::Component
{
public:
    PadTile (ConfluenceAudioProcessor&, int index);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { staticDirty = true; repaint(); }
    void mouseExit (const juce::MouseEvent&) override { staticDirty = true; repaint(); }

    /** Renvoie true si le pad doit être redessiné (animation, texte changé). */
    bool tick (bool active, uint32_t settingsSig);
    const ChordText::Described& getText() const noexcept { return text; }

private:
    void showMenu();
    void drawBody (juce::Graphics&, float t, bool withText);
    void drawTexts (juce::Graphics&);
    ConfluenceAudioProcessor& processor;
    const int index;
    ChordText::Described text;
    juce::String noteLabel;
    Engine::Pad shownPad;
    uint32_t shownSig = 0xffffffffu;
    bool active = false, playing = false;
    float level = 0.3f;

    // pad au repos : une seule image ; pad joué : l'eau est dessinée en direct, les textes viennent d'une image
    juce::Image staticImg, textImg;
    float imgScale = 0.0f;
    bool staticDirty = true, textDirty = true;
};

//==============================================================================
/** Zone MIDI : glisser vers une piste d'Ableton (clip de la progression des pads) ; clic = menu d'export. */
class MidiDragPill : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit MidiDragPill (ConfluenceAudioProcessor& p) : processor (p)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        setBufferedToImage (true);
    }
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
    std::function<void (const juce::String&, bool)> onMessage;
    int bars = 4;

private:
    void showMenu();
    ConfluenceAudioProcessor& processor;
    bool dragging = false;
    juce::Path dashedOutline;
};

//==============================================================================
/** Les 8 pads en 2 x 4 (pad 1 en bas à gauche, comme sur Push), avec GÉNÉRER et la zone MIDI. */
class PadGrid : public juce::Component
{
public:
    explicit PadGrid (ConfluenceAudioProcessor&);
    void resized() override;

    /** Appelé à 60 Hz : redessine les pads qui bougent. Renvoie true si l'un d'eux est animé. */
    bool tick();

    juce::Component& getGenerateButton() { return *generate; }
    std::function<void (const juce::String&, bool)> onMessage;

private:
    ConfluenceAudioProcessor& processor;
    juce::OwnedArray<PadTile> tiles;
    std::unique_ptr<juce::Button> generate;
    MidiDragPill midi;
};
