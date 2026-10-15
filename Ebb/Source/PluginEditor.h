// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "ui/LiquidUI.h"
#include "ui/Controls.h"
#include "ui/CurveView.h"
#include "ui/CurveStrip.h"
#include "ui/AboutPanel.h"

//==============================================================================
/**
    Interface d'Ebb (maquette A « Houle »), dessinée à taille fixe (920 x 400) puis mise à l'échelle :
      - en-tête : logo, état (mode, tempo), version ;
      - la vague en grand (affichage et éditeur de la courbe perso) et sa barre d'outils :
        ÉDITER, grille aimantée, annuler / rétablir, miroir, inverser, effacer ;
      - à droite : le gros bouton Amount, Tension et Lissage ;
      - en bas : les 12 courbes + MA COURBE, mes courbes (charger / enregistrer), durée, déclenchement,
        Nudge, Seuil, multibande (plein / graves / aigus + fréquence).
*/
class EbbContent : public juce::Component
{
public:
    explicit EbbContent (EbbAudioProcessor&);
    ~EbbContent() override;

    static constexpr int designW = 920, designH = 400;

    void paint (juce::Graphics&) override;
    void resized() override;

    /** Appelé à 60 Hz par l'éditeur. Renvoie true si l'interface est active. */
    bool tick();

    void setEditMode (bool on);
    void flash (const juce::String& message, bool ok);

    // banc de tests
    CurveView& getCurveView() { return curveView; }
    CurveStrip& getStrip() { return strip; }
    PillButton& getEditButton() { return editButton; }
    PillButton& getUndoButton() { return undoButton; }
    PillButton& getRedoButton() { return redoButton; }
    PillButton& getMirrorButton() { return mirrorButton; }
    PillButton& getInvertButton() { return invertButton; }
    PillButton& getClearButton() { return clearButton; }
    PillButton& getSaveButton() { return saveButton; }
    PillGroup& getBandPills() { return bandPills; }
    PillGroup& getRatePills() { return ratePills; }
    PillChooser& getGridChooser() { return gridChooser; }
    ShapeSavePanel* getSavePanel() { return savePanel.get(); }
    /** Panneau « À propos » (bouton i en haut à droite) : licence, code source, confidentialité. */
    void openAbout();
    bool isAboutOpen() const noexcept { return about != nullptr; }
    AboutPanel* getAboutPanel() noexcept { return about.get(); }
    juce::Button& getInfoButton() noexcept { return infoButton; }
    bool isBackgroundDirty() const noexcept { return bgDirty; }
    void invalidateBackground() { bgDirty = true; }
    void loadUserShape (const juce::File&);
    void openSavePanel();

private:
    void renderBackground (float scale);
    void renderOverlay (float scale);
    juce::String statusText() const;
    void editShape (const std::function<void (Ebb::ShapeData&)>& fn);
    void showMyCurvesMenu();
    void pickCurve (int index);

    EbbAudioProcessor& processor;
    LiquidLook look;
    CurveView curveView;
    CurveStrip strip;
    Knob amountKnob, tensionKnob, smoothKnob, nudgeKnob, thresholdKnob, crossoverKnob;
    PillGroup ratePills, triggerPills, bandPills;
    PillChooser gridChooser { { juce::String (juce::CharPointer_UTF8 ("\xe2\x80\x94")), "4", "8", "16", "32" } };
    PillButton editButton { juce::String (juce::CharPointer_UTF8 ("\xc3\x89" "DITER")), PillButton::Icon::pencil },
               undoButton { "", PillButton::Icon::undo }, redoButton { "", PillButton::Icon::redo },
               mirrorButton { "MIROIR", PillButton::Icon::mirror }, invertButton { "INVERSER", PillButton::Icon::invert },
               clearButton { "EFFACER", PillButton::Icon::clear },
               myCurvesButton { "MES COURBES", PillButton::Icon::list }, saveButton { "ENREGISTRER", PillButton::Icon::save };
    std::unique_ptr<ShapeSavePanel> savePanel;
    InfoButton infoButton { "Ebb" };
    std::unique_ptr<AboutPanel> about;

    juce::String versionText, status, flashText, curveTitle, curveDesc;
    bool flashOk = true;
    float flashLeft = 0.0f;
    int lastMode = -1, statusKey = -1, lastBand = -1, nameKey = -1, captionKey = -1;
    bool lastCanUndo = false, lastCanRedo = false;

    // fond = eau animée (8 ou 4 images/s, au quart de la résolution) + calque fixe (panneaux et textes)
    juce::Image bgCache, bgLow, bgMid, overlayImg;
    float bgScale = 0.0f, overlayScale = 0.0f;
    bool bgDirty = true, overlayDirty = true;
    double lastBgRefresh = 0.0;
    uint32_t mouseActivityFrames = 0;
    juce::Component* lastHovered = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EbbContent)
};

//==============================================================================
class EbbAudioProcessorEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit EbbAudioProcessorEditor (EbbAudioProcessor&);
    ~EbbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override {}
    void resized() override;

    EbbContent& getContent() { return content; }
    void tickForTests() { timerCallback(); }

private:
    void timerCallback() override;

    /** Déclaré avant le contenu : détruit après lui, il libère alors les caches graphiques si c'était la dernière fenêtre. */
    struct CacheGuard { CacheGuard() { Ui::editorOpened(); } ~CacheGuard() { Ui::editorClosed(); } };

    CacheGuard cacheGuard;
    EbbContent content;
    juce::TooltipWindow tooltips { this, 700 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EbbAudioProcessorEditor)
};
