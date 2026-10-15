// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include "LiquidUI.h"
#include "../dsp/Shape.h"

class EbbAudioProcessor;

//==============================================================================
/**
    La vague d'Ebb : affichage de la courbe ET éditeur de la courbe perso.

    Affichage (toujours) :
      - la courbe telle qu'on l'entend (Amount, Tension et Lissage compris), en profil de vague ;
      - une goutte qui la parcourt en rythme (position réelle dans la courbe, lue dans le moteur) ;
      - au fond, le niveau de l'eau qui baisse et remonte avec le gain réel ; un rond dans l'eau à chaque départ.

    Édition (bouton ÉDITER) : la courbe perso à pleine échelle (y = niveau du son, en haut = plein volume),
    ses points et la poignée de courbure au milieu de chaque segment ; en fin trait blanc, ce qu'on entend.
      - clic dans le vide : ajoute un point (et le déplace tant qu'on tient le bouton) ;
      - glisser un point : le déplace (entre ses voisins ; les points du bord restent en x = 0 et x = 1) ;
      - glisser une poignée de courbure vers le haut ou le bas : bombe ou creuse le segment ;
      - double-clic (ou clic droit) sur un point : le supprime ; double-clic sur une poignée : segment droit ;
      - grille aimantée (4 à 32 divisions, niveaux par 1/16) ; Alt ou Cmd pendant le geste : sans aimant.
    Chaque geste fait une seule entrée « annuler ».

    Le dessin fixe (fond, repères, profil, points) est mis en image et refait seulement quand il change ;
    à chaque image il ne reste que l'eau, le rond et la goutte.
*/
class CurveView : public juce::Component
{
public:
    explicit CurveView (EbbAudioProcessor&);

    void paint (juce::Graphics&) override;
    void resized() override { profileDirty = underDirty = true; }

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** Appelé à 60 Hz. Renvoie true si quelque chose bouge (et l'a fait redessiner). */
    bool tick();

    void setEditMode (bool on);
    bool isEditMode() const noexcept { return editMode; }

    /** Maquettes : affiche un état figé au lieu de lire le moteur. */
    void setFrozen (int curve, float amount, float phase, float gain);

    /** Nombre de cycles affichés côte à côte (1 par défaut). */
    void setCycles (int n) { cycles = juce::jlimit (1, 4, n); profileDirty = underDirty = true; repaint(); }
    void setShowLabels (bool b) { showLabels = b; overDirty = true; repaint(); }

    /** Texte en haut à droite de la vague (mode, bande...), redessiné seulement quand il change. */
    void setCaption (const juce::String& s) { if (s != caption) { caption = s; overDirty = true; repaint(); } }

    // banc de tests : coordonnées (dans le composant) d'un point de la courbe perso
    juce::Point<float> shapeToScreen (float x, float level) const;
    juce::Rectangle<float> plotArea() const;

private:
    enum class Hit { none, point, handle };
    struct Target { Hit hit = Hit::none; int index = -1; };

    void refreshEffective();
    void rebuildProfile();
    void renderUnder (float scale);
    void renderOver (float scale);
    juce::Point<float> pointAt (float phase) const;
    Target findTarget (juce::Point<float>) const;
    juce::Point<float> screenToShape (juce::Point<float>) const;
    float snapX (float x, bool free) const;
    float snapY (float y, bool free) const;
    void applyEdit (const Ebb::ShapeData&);

    EbbAudioProcessor& processor;

    // ce qui est affiché
    int curve = -1, rate = -1, cycles = 1, grid = -1;
    bool custom = false, editMode = false, frozen = false, showLabels = true;
    float amount = -1.0f, tension = 0.0f, smooth = 0.0f;
    uint32_t shapeVersion = 0;
    juce::String caption;

    // courbe effective (ce qu'on entend) : table lissée + 257 niveaux pour le dessin
    std::vector<float> effTable, effTmp;
    std::array<float, 257> effY {};
    juce::Path profile, profileFill, shapePath, shapeFill;
    bool profileDirty = true;

    // animation
    float phase = -1.0f, gain = 1.0f, shownGain = 1.0f, ripple = 0.0f;
    uint32_t lastTriggers = 0;
    float frozenPhase = 0.3f, frozenGain = 0.5f;

    // édition
    Target hover, drag;
    Ebb::ShapeData dragStart;
    juce::Point<float> dragOrigin;
    float dragCurve0 = 0.0f;
    bool dragging = false;

    const juce::String label100 { "100 %" }, label0 { "0 %" };   // créés une fois : aucune allocation pendant le dessin
    juce::String hintText;

    // calques fixes mis en image
    juce::Image underImg, overImg;
    float underScale = 0.0f, overScale = 0.0f;
    bool underDirty = true, overDirty = true;
};
