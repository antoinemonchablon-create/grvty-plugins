// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/LiquidUI.h"
#include "ui/PresetBar.h"
#include "ui/FmScreen.h"
#include "ui/SeqGrid.h"
#include "ui/AboutPanel.h"

//==============================================================================
/** Menu déroulant avec son nom au-dessus, relié à un paramètre à choix. */
class ChoiceControl : public juce::Component
{
public:
    ChoiceControl (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& name);
    void resized() override;
    juce::ComboBox& getBox() { return box; }

private:
    juce::Label title;
    juce::ComboBox box;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
};

//==============================================================================
/** Bouton pilule à texte (action, pas d'état). */
class PillButton : public juce::Button
{
public:
    explicit PillButton (const juce::String& text) : juce::Button (text) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;
};

//==============================================================================
/** Sélecteur vertical en pilules (ex. LFO 1 / LFO 2). */
class PillSelector : public juce::Component
{
public:
    explicit PillSelector (juce::StringArray options) : names (std::move (options)) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    std::function<void (int)> onChange;
    int getCurrent() const noexcept { return current; }

private:
    juce::StringArray names;
    int current = 0;
};

//==============================================================================
/** Petite courbe liquide : enveloppe d'ampli, enveloppe Mod ou forme du LFO affiché. */
class CurveView : public juce::Component
{
public:
    enum class Kind { amp, mod, lfo };
    CurveView (juce::AudioProcessorValueTreeState& s, Kind k) : state (s), kind (k) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;
    int lfoIndex = 0;

private:
    float valueAt (float x) const;
    juce::AudioProcessorValueTreeState& state;
    Kind kind;
    juce::Path line, fill;
};

//==============================================================================
/**
    Contenu de la fenêtre, dessiné à taille fixe (900 x 470) puis mis à l'échelle :
    la fenêtre est redimensionnable sans rien déformer.
*/
class UndertowContent : public juce::Component
{
public:
    PresetBar& getPresetBar() { return presetBar; }
    /** Panneau « À propos » (bouton i en haut à droite) : licence, code source, confidentialité. */
    void openAbout();
    bool isAboutOpen() const noexcept { return about != nullptr; }
    AboutPanel* getAboutPanel() noexcept { return about.get(); }
    juce::Button& getInfoButton() noexcept { return infoButton; }
    explicit UndertowContent (UndertowAudioProcessor&);
    ~UndertowContent() override;

    static constexpr int designW = 900, designH = 470;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Appelé à 60 Hz par l'éditeur. Renvoie true si l'interface est « active » (lecture, souris, animation). */
    bool tick();
    void setTab (int t) { tabs.setCurrent (t); }

    /** Pour le banc de tests : pourquoi l'interface est-elle active ? */
    juce::String getActivityReason() const { return activityReason; }
    void invalidateBackground() { bgDirty = true; }
    void setScopeView (int m) { fmScreen.setViewMode (m); }
    juce::Rectangle<int> getFmBounds() const { return fmScreen.getBounds(); }
    juce::Rectangle<int> getGridBounds() const { return grid.getBounds(); }
    juce::Rectangle<int> getMeterBounds() const { return meter.getBounds(); }

private:
    enum Tab { oscTab = 0, filterTab, modTab, fxTab, numTabs };

    juce::Component* add (int page, const juce::String& id, const juce::String& name, juce::Rectangle<int> r, PillToggle::Icon icon = PillToggle::Icon::dot);
    juce::Rectangle<int> cell (int col, int row, int spanCols = 1) const;
    void showTab (int t);
    void updateLfoVisibility();
    void showEuclidMenu();

    UndertowAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& state;
    LiquidLook look;

    PresetBar presetBar;
    FmScreen fmScreen;
    SeqGrid grid;
    TabBar tabs;
    LiquidMeter meter;
    PillSelector lfoSelector { { "LFO 1", "LFO 2" } };
    CurveView ampCurve { state, CurveView::Kind::amp }, modCurve { state, CurveView::Kind::mod }, lfoCurve { state, CurveView::Kind::lfo };
    PillButton euclidButton { "EUCLID" }, randomButton { "RND" }, leftButton { "<" }, rightButton { ">" };

    struct Placed { juce::Component* comp; int page; int lfo = -1; };
    juce::OwnedArray<juce::Component> owned;
    std::vector<Placed> placed;
    std::array<juce::Component*, Params::numLfos> lfoRateKnob {}, lfoSyncRateBox {};

    void renderBackground (float scale);

    juce::Rectangle<int> pagePanel, seqPanel, seqLeft;
    juce::String versionText;
    InfoButton infoButton { "Undertow" };
    std::unique_ptr<AboutPanel> about;
    int currentTab = 0;
    uint32_t mouseActivityFrames = 0;

    // Fond (eau animée + panneaux + textes fixes) calculé dans une image à la résolution de l'écran,
    // rafraîchie 8 fois par seconde (4 au repos) : chaque image ne fait alors qu'une copie.
    juce::Image bgCache, bgLow;
    float bgScale = 0.0f;
    bool  bgDirty = true;
    double lastBgRefresh = 0.0;
    juce::Component* lastHovered = nullptr;
    std::array<float, 3> curveSig { -1.0f, -1.0f, -1.0f };
    juce::String activityReason;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndertowContent)
};

//==============================================================================
class UndertowAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit UndertowAudioProcessorEditor (UndertowAudioProcessor&);
    ~UndertowAudioProcessorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    UndertowContent& getContent() { return content; }

private:
    void timerCallback() override;

    /** Déclaré avant le contenu : détruit après lui, il libère alors les caches graphiques si c'était la dernière fenêtre. */
    struct CacheGuard { CacheGuard(); ~CacheGuard(); };

    UndertowAudioProcessor& processorRef;
    CacheGuard cacheGuard;
    UndertowContent content;
    juce::TooltipWindow tooltips { this, 700 };
    int frame = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndertowAudioProcessorEditor)
};
