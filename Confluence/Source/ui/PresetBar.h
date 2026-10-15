// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "LiquidUI.h"

class ConfluenceAudioProcessor;
class PresetManager;

//==============================================================================
/**
    Panneau « Enregistrer le preset » dessiné DANS la fenêtre du plugin (pas de fenêtre séparée :
    dans Ableton sur Mac, une fenêtre de dialogue s'ouvre derrière le plugin et reste invisible).
    Entrée = enregistrer, Échap ou clic à côté = annuler.
*/
class SavePanel : public juce::Component
{
public:
    SavePanel (PresetManager&, const juce::String& suggestedName, std::function<void (bool saved, juce::String name)> onDone);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void parentHierarchyChanged() override;

    void commit();
    void cancel();

private:
    void updateHint();

    PresetManager& presets;
    std::function<void (bool, juce::String)> done;
    juce::TextEditor editor;
    juce::Rectangle<int> panelR, saveR, cancelR;
    bool exists = false, finished = false;
    int hot = 0;
};

//==============================================================================
/**
    Barre de presets (reprise de Flow Filter) :  [‹] [ BANQUE  nom ▾ ] [›]  [SAVE] [dossier] [dé]
      - ‹ › : preset précédent / suivant (usine puis utilisateur)
      - clic sur le nom : menu de tous les presets, rangés par banque
      - SAVE : enregistre les réglages ET les pads sous un nom (panneau dans la fenêtre du plugin)
      - dossier : ouvre le dossier des presets utilisateur (Finder ou Explorateur)
      - dé : réglages au hasard dans des plages musicales (pads regénérés) ; le menu propose aussi des pads au hasard
*/
class PresetBar : public juce::Component
{
public:
    explicit PresetBar (ConfluenceAudioProcessor&);
    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** Appelé par le minuteur de l'éditeur. Renvoie true si quelque chose bouge encore. */
    bool refresh();

    void askSaveName();
    bool isSavePanelOpen() const { return savePanel != nullptr; }
    void closeSavePanel()        { if (savePanel != nullptr) savePanel->cancel(); }

    /** Message court affiché à la place de l'étiquette de banque (2,5 s). */
    void flash (const juce::String& message, bool ok);

private:
    enum class Zone { none, prev, name, next, save, folder, dice };
    Zone zoneAt (juce::Point<int>) const;
    void showMenu();
    void step (int direction);
    void rollDice();
    void openFolder();

    ConfluenceAudioProcessor& processor;
    juce::Rectangle<int> prevR, nameR, nextR, saveR, folderR, diceR;
    std::unique_ptr<SavePanel> savePanel;
    juce::String flashText;
    bool  flashOk = true;
    float flashLeft = 0.0f;
    Zone hovered = Zone::none;
    juce::String shownName, shownBank;
    float diceSpin = 0.0f;
    int   diceFace = 5;
};
