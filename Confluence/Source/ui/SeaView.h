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
    La mer de Confluence : le clavier vu sous la surface de l'eau (notes de la gamme éclairées, notes jouées en lumière),
    les notes de l'accord qui montent comme des bulles, et le nom de l'accord avec son degré qui flotte en surface.

    Le clavier se joue à la souris (la note passe par Confluence, comme une note du clavier MIDI) :
    plus on clique bas sur la touche, plus la velocity est forte.
*/
class SeaView : public juce::Component
{
public:
    explicit SeaView (ConfluenceAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

    /** Appelé à 60 Hz. Renvoie true si quelque chose bouge (notes qui sonnent, bulles, survol). */
    bool tick();

    static constexpr int lowKey = 36, highKey = 84;   // C1 .. C5

    // banc de tests
    int getNumBubbles() const noexcept { int n = 0; for (auto& b : bubbles) n += b.alive ? 1 : 0; return n; }
    juce::Rectangle<float> getKeyboardArea() const noexcept { return keys; }
    int noteAt (juce::Point<float>) const;

private:
    struct Bubble
    {
        bool alive = false;
        float x = 0, y = 0, startY = 0, r = 0, phase = 0, speed = 0, life = 0;
        bool bass = false;
    };

    float keyX (int note) const noexcept;
    float whiteW() const noexcept;
    juce::Rectangle<float> keyRect (int note) const noexcept;
    void renderTitle (float scale);
    void renderWhiteKeys (float scale);
    void spawn (int note, bool bass, float delay);
    void playNote (int note, int velocity);

    ConfluenceAudioProcessor& processor;
    juce::Rectangle<float> keys, titleArea;

    // accord affiché
    uint32_t lastCounter = 0;
    ChordText::Described shown;
    std::array<int, 16> chordNotes {};
    int chordCount = 0, chordBass = -1;
    float titleGlow = 0.0f;

    // touches qui sonnent / tenues (copie de l'affichage)
    std::array<uint32_t, 4> soundingCopy {}, inputCopy {};
    int keyRoot = -1, keyScale = -1;

    std::array<Bubble, 96> bubbles {};
    float spawnClock = 0.0f;
    double lastTime = 0.0;

    juce::Image titleImg, whiteKeysImg, blackKeysImg;
    float imgScale = 0.0f;
    bool titleDirty = true, keysDirty = true;

    int mouseNote = -1;
    bool mouseInside = false;
};
