// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "ui/AboutPanel.h"
#include "PluginProcessor.h"
#include "ui/LiquidUI.h"
#include "ui/PresetBar.h"
#include "ui/SeaView.h"
#include "ui/Whirlpool.h"
#include "ui/PadGrid.h"

//==============================================================================
/** Menu déroulant avec son nom au-dessus, relié à un paramètre à choix (repris d'Undertow). */
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

/** Bouton pilule à texte (action). 'lit' : allumé (Learn en cours, analyse en cours...). */
class PillButton : public juce::Button
{
public:
    explicit PillButton (const juce::String& label) : juce::Button (label) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
    bool lit = false;
    juce::Colour accent = Ui::aqua;
    float fontSize = 9.0f;
};

/** Onglets en pilules empilées (CHORD / VOICING / PLAY). */
class PillSelector : public juce::Component
{
public:
    explicit PillSelector (juce::StringArray options) : names (std::move (options)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void (int)> onChange;
    int getCurrent() const noexcept { return current; }
    void setCurrent (int i) { current = i; repaint(); if (onChange) onChange (i); }

private:
    juce::StringArray names;
    int current = 0;
};

/** Onglet VOICING : l'accord joué sur une échelle verticale, entre la note la plus basse et la plus haute autorisées. */
class VoicingView : public juce::Component
{
public:
    explicit VoicingView (ConfluenceAudioProcessor& p) : processor (p) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    bool tick();

private:
    ConfluenceAudioProcessor& processor;
    uint32_t sig = 0;
};

/** LEARN + forme apprise. */
class LearnBox : public juce::Component
{
public:
    explicit LearnBox (ConfluenceAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    bool tick();

private:
    ConfluenceAudioProcessor& processor;
    PillButton button { "LEARN" };
    juce::String text;
    bool wasLearning = false;
};

//==============================================================================
/**
    Interface de Confluence (maquette C « Courant »), dessinée à taille fixe (1000 x 470) puis mise à l'échelle :
      - en-tête : logo, presets (banques, SAVE, dossier, dé), PANIC, version ;
      - colonne de gauche : mode, tonique, gamme, style, gammes conseillées / détection, tourbillon des enchaînements ;
      - bande du haut : onglets CHORD / VOICING / PLAY, réglages sur deux rangées ;
      - au centre : la mer, le clavier sous l'eau (jouable), les bulles et le nom de l'accord ;
      - à droite : les 8 pads (jouables, clic droit pour les régler), GÉNÉRER et la zone MIDI.
*/
class ConfluenceContent : public juce::Component
{
public:
    explicit ConfluenceContent (ConfluenceAudioProcessor&);
    ~ConfluenceContent() override;

    static constexpr int designW = 1000, designH = 470;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Appelé à 60 Hz par l'éditeur. Renvoie true si l'interface est active. */
    bool tick();
    void setTab (int t) { tabs.setCurrent (t); }

    // banc de tests
    PresetBar& getPresetBar() { return presetBar; }
    SeaView& getSea() { return sea; }
    juce::String getActivityReason() const { return activityReason; }
    void invalidateBackground() { bgDirty = true; }
    bool isBackgroundDirty() const noexcept { return bgDirty; }
    juce::Rectangle<int> getSeaBounds() const { return sea.getBounds(); }
    juce::Rectangle<int> getWhirlBounds() const { return whirl.getBounds(); }
    juce::Rectangle<int> getPadsBounds() const { return pads.getBounds(); }

    /** Panneau « À propos » (licence, code source, confidentialité, composants tiers). */
    void openAbout();
    bool isAboutOpen() const noexcept { return about != nullptr; }
    AboutPanel* getAboutPanel() noexcept { return about.get(); }
    juce::Button& getInfoButton() noexcept { return infoButton; }

private:
    enum Tab { chordTab = 0, voicingTab, playTab };

    juce::Component* add (int tab, const char* id, const char* name, juce::Rectangle<int> r);
    juce::Rectangle<int> cell (int col, int row, int spanCols = 1) const;
    void showTab (int t);
    void renderBackground (float scale);
    void upscaleWater();
    void renderOverlay (float scale);
    void refreshScalePills();
    void applyScale (int root, int scale);

    ConfluenceAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    LiquidLook look;

    PresetBar presetBar;
    SeaView sea;
    Whirlpool whirl;
    PadGrid pads;
    VoicingView voicingView;
    LearnBox learnBox;
    PillSelector tabs { { "CHORD", "VOICING", "PLAY" } };
    PillButton panicButton { "PANIC" }, analyseButton { "ANALYSER" };
    InfoButton infoButton;
    std::unique_ptr<AboutPanel> about;
    juce::OwnedArray<PillButton> scalePills;
    std::array<int, 3> pillRoot {}, pillScale {};
    std::array<bool, 3> pillValid {};
    bool showingDetection = false;
    int lastStyle = -1, lastRoot = -1, lastAnalysisCount = -1;

    struct Placed { juce::Component* comp; int tab; };
    juce::OwnedArray<juce::Component> owned;
    std::vector<Placed> placed;

    juce::String versionText, pillsTitle, lastOverlayTitle;
    int currentTab = 0;
    uint32_t mouseActivityFrames = 0;

    // fond = eau animée (8 ou 4 images/s, au quart de la résolution) + calque fixe (panneaux et textes, redessiné
    // seulement quand il change) : aucun texte n'est remis en page pendant l'animation
    juce::Image bgCache, bgLow, overlayImg;
    std::vector<int> upX0;          // agrandissement de l'eau (Linux) : colonne source et poids par colonne
    std::vector<uint16_t> upWx, upRows;
    float bgScale = 0.0f, overlayScale = 0.0f;
    bool bgDirty = true, overlayDirty = true;
    bool overlayDetection = false;
    double lastBgRefresh = 0.0;
    juce::Component* lastHovered = nullptr;
    juce::String activityReason;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfluenceContent)
};

//==============================================================================
class ConfluenceAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::DragAndDropContainer,
                                       private juce::Timer
{
public:
    explicit ConfluenceAudioProcessorEditor (ConfluenceAudioProcessor&);
    ~ConfluenceAudioProcessorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    ConfluenceContent& getContent() { return content; }
    void tickForTests() { timerCallback(); }

private:
    void timerCallback() override;

    /** Déclaré avant le contenu : détruit après lui, il libère alors les caches graphiques si c'était la dernière fenêtre. */
    struct CacheGuard { CacheGuard() { Ui::editorOpened(); } ~CacheGuard() { Ui::editorClosed(); } };

    CacheGuard cacheGuard;
    ConfluenceContent content;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConfluenceAudioProcessorEditor)
};
