// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Flow Filter, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "AboutPanel.h"

//==============================================================================
/** Style "liquide" : boutons-gouttes, menus en pilule, textes compacts. */
class LiquidLook : public juce::LookAndFeel_V4
{
public:
    LiquidLook();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

//==============================================================================
/** Bouton rotatif en forme de goutte : nom au-dessus, valeur en dessous. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& name,
          bool bipolar = false, const juce::String& tooltip = {});
    void resized() override;

private:
    juce::Label  title;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
/** Interrupteur en pilule avec icône dessinée, relié à un paramètre booléen. */
class PillToggle : public juce::Button
{
public:
    enum class Icon { sync, reverse, freeze };

    PillToggle (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& text, Icon);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Icon icon;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

//==============================================================================
/** Onglets avec indicateur lumineux qui glisse vers l'onglet actif. */
class TabBar : public juce::Component
{
public:
    explicit TabBar (juce::StringArray names);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void animate();

    std::function<void (int)> onChange;
    int  getCurrent() const noexcept { return current; }
    void setCurrent (int i) { current = i; repaint(); if (onChange) onChange (i); }

private:
    juce::StringArray names;
    int current = 0, hovered = -1;
    float indicator = 0.0f;
};

//==============================================================================
/**
    Barre de presets :  [‹] [ nom du preset ▾ ] [›]  [SAVE]  [dé]
      - ‹ › : preset précédent / suivant (usine puis utilisateur)
      - clic sur le nom : menu de tous les presets
      - SAVE : enregistre les réglages actuels sous un nom
      - dé : tout au hasard (sauf Input et Mix)
*/
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (FlowFilterAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void refresh();   // appelé par le timer de l'éditeur

private:
    enum class Zone { none, prev, name, next, save, dice };
    Zone zoneAt (juce::Point<int>) const;
    void showMenu();
    void step (int direction);
    void askSaveName();
    void rollDice();

    FlowFilterAudioProcessor& processor;
    juce::Rectangle<int> prevR, nameR, nextR, saveR, diceR;
    Zone hovered = Zone::none;
    juce::String shownName;
    float diceSpin = 0.0f;
    int   diceFace = 5;
};

//==============================================================================
/** Vu-mètre en forme de tube rempli de liquide. */
class LiquidMeter : public juce::Component
{
public:
    explicit LiquidMeter (std::atomic<float>& src) : source (src) {}
    void paint (juce::Graphics&) override;

private:
    std::atomic<float>& source;
    float smoothed = 0.0f;
};

//==============================================================================
/**
    Écran central : 64 bandes + surface de la vague calculée sur 256 points.
      - glisser dans le fond : horizontal = Offset, vertical = Gravity
      - glisser les poignées : limites de la zone filtrée (Area)
      - double-clic : remise à zéro
*/
class WaveDisplay : public juce::Component
{
public:
    explicit WaveDisplay (FlowFilterAudioProcessor&);

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;

private:
    enum class DragMode { none, wave, areaLow, areaHigh };
    static constexpr int curvePoints = 256;

    juce::Rectangle<float> plotArea() const;
    float  freqToX (double f) const;
    double xToFreq (float x) const;
    DragMode hitTest (juce::Point<float>) const;
    void setParam (const char* id, float value);

    FlowFilterAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    WaveEngine displayWave;
    DragMode dragMode = DragMode::none;
    float dragStartOffset = 0.0f, dragStartGravity = 0.0f;
};

//==============================================================================
class FlowFilterAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit FlowFilterAudioProcessorEditor (FlowFilterAudioProcessor&);
    ~FlowFilterAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Panneau « À propos » (bouton i en haut à droite) : licence, code source, confidentialité. */
    void openAbout();
    bool isAboutOpen() const noexcept { return about != nullptr; }
    AboutPanel* getAboutPanel() noexcept { return about.get(); }
    juce::Button& getInfoButton() noexcept { return infoButton; }

private:
    enum Tab { mainTab = 0, advancedTab, modTab };

    void timerCallback() override;
    void showTab (int tab);
    void paintBackground (juce::Graphics&);
    Knob* addKnob (juce::OwnedArray<Knob>& page, const juce::String& id, const juce::String& name,
                   const juce::String& tooltip, bool bipolar = false);

    FlowFilterAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& state;
    LiquidLook look;
    juce::TooltipWindow tooltips { this, 600 };

    WaveDisplay display;
    PresetBar presetBar;
    PillToggle syncButton, reverseButton, freezeButton;
    TabBar tabs;

    std::unique_ptr<Knob> inKnob, outKnob, mixKnob;
    LiquidMeter inMeter, outMeter;

    juce::OwnedArray<Knob> mainPage, advancedPage, modPage;
    Knob* rateKnob = nullptr;
    Knob* syncRateKnob = nullptr;
    std::array<juce::ComboBox, FlowFilterAudioProcessor::numMods> modShapeBoxes;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>, FlowFilterAudioProcessor::numMods> modShapeAtt;
    std::array<juce::Label, FlowFilterAudioProcessor::numMods> modTitles;

    juce::Rectangle<int> leftPanel, pagePanel, rightPanel;

    InfoButton infoButton { "Flow Filter" };
    std::unique_ptr<AboutPanel> about;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FlowFilterAudioProcessorEditor)
};
