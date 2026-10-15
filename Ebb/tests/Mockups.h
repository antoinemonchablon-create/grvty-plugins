// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once
/*
    Maquettes d'interface d'Ebb (avant la phase 2, à choisir) : trois dispositions rendues avec les vrais composants
    (boutons-gouttes, menus, fond d'eau, vague de la courbe) et les éléments signature dessinés à la main :
      - la courbe en profil de vague, avec la goutte qui la parcourt ;
      - le niveau de l'eau qui baisse et remonte avec le gain réel ;
      - les 12 courbes en vignettes ;
      - le gros bouton Amount.
*/
#include "PluginEditor.h"

namespace Mock
{
inline juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

constexpr int W = 760, H = 320;

/** État figé montré par les maquettes. */
struct Scene
{
    int curve = Ebb::curveRipple;
    float amount = 0.8f;
    float phase = 0.30f;
    int rate = Ebb::rateQuarter;
    int trigger = 0;
    juce::String bank = "HYPNOTIC", preset = "Steady Pump";
    float gain() const { return 1.0f - amount * Ebb::CurveTables::get().lookup (curve, phase); }
};

//==============================================================================
inline void pill (juce::Graphics& g, juce::Rectangle<float> r, bool on, bool hot = false, juce::Colour accent = Ui::aqua)
{
    const float c = r.getHeight() * 0.5f;
    if (on)
    {
        g.setColour (accent.withAlpha (0.14f));
        g.fillRoundedRectangle (r.expanded (1.5f), c + 1.5f);
        Ui::setVerticalGradient (g, 3, accent.withAlpha (0.38f), r.getY(), Ui::aquaDeep.withAlpha (0.25f), r.getBottom());
    }
    else
        g.setColour (juce::Colours::white.withAlpha (hot ? 0.08f : 0.04f));
    g.fillRoundedRectangle (r, c);
    g.setColour (on ? accent.withAlpha (0.9f) : juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (r, c, 1.0f);
}

inline void label (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> r, float size = 8.6f,
                   juce::Colour col = Ui::dim, juce::Justification j = juce::Justification::centred, float kern = 0.12f)
{
    g.setColour (col);
    g.setFont (Ui::font (size, true, kern));
    g.drawText (text, r, j, false);
}

/** Rangée (ou colonne) de pilules, une seule allumée. */
inline void pillRow (juce::Graphics& g, juce::Rectangle<float> r, const juce::StringArray& names, int selected, bool vertical = false,
                     float gap = 4.0f, float fontSize = 9.0f)
{
    const int n = names.size();
    const float step = (vertical ? r.getHeight() : r.getWidth()) / (float) n;
    for (int i = 0; i < n; ++i)
    {
        auto cell = vertical ? juce::Rectangle<float> (r.getX(), r.getY() + step * (float) i, r.getWidth(), step - gap)
                             : juce::Rectangle<float> (r.getX() + step * (float) i, r.getY(), step - gap, r.getHeight());
        pill (g, cell, i == selected);
        label (g, names[i], cell, fontSize, i == selected ? juce::Colours::white : Ui::text.withAlpha (0.75f), juce::Justification::centred, 0.06f);
    }
}

/** Vignette d'une courbe (profil de vague miniature). */
inline void thumb (juce::Graphics& g, juce::Rectangle<float> r, int curve, bool selected, bool withName)
{
    g.setColour (selected ? Ui::aqua.withAlpha (0.16f) : juce::Colours::white.withAlpha (0.035f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (selected ? Ui::aqua.withAlpha (0.9f) : juce::Colours::white.withAlpha (0.09f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, selected ? 1.3f : 1.0f);

    auto plot = r.reduced (5.0f, 4.0f);
    if (withName) plot.removeFromBottom (11.0f);
    juce::Path p;
    const auto& t = Ebb::CurveTables::get();
    for (int i = 0; i <= 48; ++i)
    {
        const float x = (float) i / 48.0f;
        const float y = plot.getBottom() - (1.0f - t.lookup (curve, x)) * plot.getHeight();
        if (i == 0) p.startNewSubPath (plot.getX(), y); else p.lineTo (plot.getX() + x * plot.getWidth(), y);
    }
    juce::Path fill (p);
    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.lineTo (plot.getX(), plot.getBottom());
    fill.closeSubPath();
    g.setColour (Ui::aqua.withAlpha (selected ? 0.22f : 0.08f));
    g.fillPath (fill);
    g.setColour (selected ? Ui::aqua : Ui::text.withAlpha (0.55f));
    g.strokePath (p, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    if (withName)
        label (g, Ebb::curveName (curve), r.removeFromBottom (13.0f).translated (0.0f, -2.0f), 7.6f,
               selected ? juce::Colours::white : Ui::dim, juce::Justification::centred, 0.08f);
}

/** En-tête commun : logo, barre de presets (aperçu), version. */
inline void header (juce::Graphics& g, const Scene& s, float presetX)
{
    const float t = 1.3f;
    juce::Path logo;
    for (int i = 0; i <= 24; ++i)
    {
        const float xx = 20.0f + (float) i, yy = 20.0f + 3.5f * std::sin ((float) i * 0.42f + t * 2.0f);
        if (i == 0) logo.startNewSubPath (xx, yy); else logo.lineTo (xx, yy);
    }
    g.setColour (Ui::aqua);
    g.strokePath (logo, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colours::white);
    g.setFont (Ui::font (18.0f, true, -0.02f));
    g.drawText ("ebb", 52, 8, 50, 24, juce::Justification::centredLeft);
    label (g, "SIDECHAIN", { 88.0f, 12.0f, 80.0f, 20.0f }, 7.5f, Ui::aqua, juce::Justification::centredLeft, 0.45f);

    // barre de presets (même dessin que Flow Filter / Undertow / Confluence)
    auto bar = juce::Rectangle<float> (presetX, 7.0f, 330.0f, 24.0f);
    auto prev = bar.removeFromLeft (24.0f), dice = bar.removeFromRight (28.0f);
    bar.removeFromRight (4.0f);
    auto folder = bar.removeFromRight (28.0f);
    bar.removeFromRight (4.0f);
    auto save = bar.removeFromRight (46.0f);
    bar.removeFromRight (6.0f);
    auto next = bar.removeFromRight (24.0f);
    for (auto [r, dir] : { std::pair { prev, -1.0f }, std::pair { next, 1.0f } })
    {
        pill (g, r, false);
        juce::Path chev;
        const auto c = r.getCentre();
        chev.startNewSubPath (c.x - 1.5f * dir, c.y - 4.0f);
        chev.lineTo (c.x + 2.0f * dir, c.y);
        chev.lineTo (c.x - 1.5f * dir, c.y + 4.0f);
        g.setColour (Ui::aqua);
        g.strokePath (chev, juce::PathStrokeType (1.5f));
    }
    auto name = bar.reduced (3.0f, 0.0f);
    g.setColour (juce::Colour (0xaa020a0e));
    g.fillRoundedRectangle (name, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawRoundedRectangle (name.reduced (0.5f), 5.0f, 1.0f);
    auto tag = name.reduced (7.0f, 5.0f).removeFromLeft (64.0f);
    g.setColour (juce::Colour (0xff8fa8ff).withAlpha (0.15f));
    g.fillRoundedRectangle (tag, tag.getHeight() * 0.5f);
    label (g, s.bank, tag, 7.5f, juce::Colour (0xff8fa8ff), juce::Justification::centred, 0.2f);
    label (g, s.preset, name.withTrimmedLeft (72.0f).withTrimmedRight (14.0f), 11.0f, juce::Colours::white, juce::Justification::centred, 0.03f);
    pill (g, save, false, false);
    g.setColour (Ui::aqua.withAlpha (0.5f));
    g.drawRoundedRectangle (save, save.getHeight() * 0.5f, 1.0f);
    label (g, "SAVE", save, 9.0f, Ui::aqua, juce::Justification::centred, 0.15f);
    pill (g, folder, false);
    {
        const auto c = folder.getCentre();
        juce::Path f;
        const float w = 12.0f, h = 9.0f, x = c.x - w * 0.5f, y = c.y - h * 0.5f + 0.5f;
        f.startNewSubPath (x, y + h); f.lineTo (x, y); f.lineTo (x + 4.0f, y); f.lineTo (x + 5.5f, y + 1.8f);
        f.lineTo (x + w, y + 1.8f); f.lineTo (x + w, y + h); f.closeSubPath();
        g.setColour (Ui::aqua);
        g.strokePath (f, juce::PathStrokeType (1.3f));
    }
    {
        const auto c = dice.getCentre();
        const float sz = dice.getHeight() - 7.0f;
        g.setColour (Ui::coral.withAlpha (0.18f));
        g.fillRoundedRectangle (juce::Rectangle<float> (sz, sz).withCentre (c), 4.0f);
        g.setColour (Ui::coral.withAlpha (0.8f));
        g.drawRoundedRectangle (juce::Rectangle<float> (sz, sz).withCentre (c), 4.0f, 1.3f);
        g.setColour (juce::Colours::white);
        for (auto [dx, dy] : { std::pair { -1, -1 }, std::pair { 1, -1 }, std::pair { 0, 0 }, std::pair { -1, 1 }, std::pair { 1, 1 } })
            g.fillEllipse (juce::Rectangle<float> (2.6f, 2.6f).withCentre ({ c.x + (float) dx * sz * 0.26f, c.y + (float) dy * sz * 0.26f }));
    }

    label (g, "v" JucePlugin_VersionString, { (float) W - 84.0f, 9.0f, 74.0f, 20.0f }, 8.5f, Ui::dim.withAlpha (0.7f), juce::Justification::centredRight, 0.05f);
}

/** Le niveau de l'eau en tube vertical (maquette C), gradué en dB. */
inline void tideGauge (juce::Graphics& g, juce::Rectangle<float> r, float gain)
{
    Ui::drawGlassPanel (g, r.expanded (6.0f, 8.0f), 10.0f);
    auto tube = r.withTrimmedTop (16.0f).withTrimmedBottom (18.0f).withSizeKeepingCentre (26.0f, r.getHeight() - 34.0f);
    g.setColour (juce::Colour (0xff03090d));
    g.fillRoundedRectangle (tube, 13.0f);
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (tube.reduced (2.0f), 11.0f);
        g.reduceClipRegion (clip);
        Ui::drawLiquid (g, tube.reduced (2.0f), gain, 1.7f, 2.0f, Ui::aqua.withAlpha (0.9f), Ui::aquaDeep.withAlpha (0.45f), 1.6f);
    }
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (tube, 13.0f, 1.0f);
    // graduations : 0, -3, -6, -12, -24 dB (hauteur = gain linéaire)
    for (float db : { 0.0f, -3.0f, -6.0f, -12.0f, -24.0f })
    {
        const float lin = juce::Decibels::decibelsToGain (db);
        const float y = tube.getBottom() - 2.0f - lin * (tube.getHeight() - 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawHorizontalLine (juce::roundToInt (y), tube.getRight() + 2.0f, tube.getRight() + 7.0f);
        label (g, juce::String ((int) db), { tube.getRight() + 8.0f, y - 6.0f, 24.0f, 12.0f }, 7.5f, Ui::dim, juce::Justification::centredLeft, 0.0f);
    }
    label (g, "NIVEAU", r.withHeight (14.0f).translated (0.0f, -2.0f), 8.0f, Ui::dim);
    label (g, juce::String (juce::Decibels::gainToDecibels (gain, -60.0f), 1) + " dB", r.withTrimmedTop (r.getHeight() - 14.0f), 9.0f, juce::Colours::white);
}

/** La courbe enroulée autour du gros bouton (maquette B) : le rayon suit le niveau, la goutte tourne. */
inline void ring (juce::Graphics& g, juce::Point<float> c, float rIn, float rOut, const Scene& s)
{
    const auto& t = Ebb::CurveTables::get();
    juce::Path wave, fill;
    const float a0 = -juce::MathConstants<float>::halfPi;
    for (int i = 0; i <= 240; ++i)
    {
        const float x = (float) i / 240.0f;
        const float gl = 1.0f - s.amount * t.lookup (s.curve, x);
        const float rr = rIn + gl * (rOut - rIn);
        const auto pt = c.getPointOnCircumference (rr, a0 + juce::MathConstants<float>::halfPi + x * juce::MathConstants<float>::twoPi);
        if (i == 0) wave.startNewSubPath (pt); else wave.lineTo (pt);
    }
    wave.closeSubPath();
    fill = wave;
    fill.addEllipse (juce::Rectangle<float> (rIn * 2.0f, rIn * 2.0f).withCentre (c));
    fill.setUsingNonZeroWinding (false);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawEllipse (juce::Rectangle<float> (rOut * 2.0f, rOut * 2.0f).withCentre (c), 1.0f);
    g.drawEllipse (juce::Rectangle<float> (rIn * 2.0f, rIn * 2.0f).withCentre (c), 1.0f);
    for (int k = 0; k < 4; ++k)
    {
        const float a = (float) k * juce::MathConstants<float>::halfPi;
        g.drawLine (juce::Line<float> (c.getPointOnCircumference (rIn, a), c.getPointOnCircumference (rOut + 4.0f, a)), 1.0f);
    }
    Ui::setGradient (g, 6, Ui::aqua.withAlpha (0.32f), c, Ui::aquaDeep.withAlpha (0.05f), c.translated (rOut, 0.0f), true);
    g.fillPath (fill);
    g.setColour (Ui::aqua.withAlpha (0.18f));
    g.strokePath (wave, juce::PathStrokeType (6.0f));
    g.setColour (Ui::aqua);
    g.strokePath (wave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    // la goutte
    const float gl = s.gain();
    const auto pt = c.getPointOnCircumference (rIn + gl * (rOut - rIn), s.phase * juce::MathConstants<float>::twoPi);
    g.setColour (Ui::aqua.withAlpha (0.3f));
    g.fillEllipse (juce::Rectangle<float> (22.0f, 22.0f).withCentre (pt));
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (10.0f, 10.0f).withCentre (pt));
    // le temps (départ de la courbe) : petit repère en haut
    g.setColour (Ui::coral);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre (c.getPointOnCircumference (rOut + 9.0f, 0.0f)));
}

//==============================================================================
/** Toile d'une maquette : fond d'eau, contrôles réels, dessin à la main par-dessus. */
struct Canvas : public juce::Component
{
    explicit Canvas (EbbAudioProcessor& p) : proc (p), curve (p) { setLookAndFeel (&look); setSize (W, H); }
    ~Canvas() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        Ui::drawWaterBackground (g, getLocalBounds().toFloat());
        if (under) under (g);
    }
    void paintOverChildren (juce::Graphics& g) override { if (over) over (g); }

    Knob& knob (const char* pid, const char* name, juce::Rectangle<int> r, float maxDiameter = 46.0f, bool bipolar = false)
    {
        auto* k = knobs.add (new Knob (proc.apvts, pid, u8 (name), bipolar));
        if (maxDiameter > 46.0f) k->setMaxDiameter (maxDiameter);
        addAndMakeVisible (k);
        k->setBounds (r);
        return *k;
    }
    ChoiceControl& choice (const char* pid, const char* name, juce::Rectangle<int> r)
    {
        auto* c = choices.add (new ChoiceControl (proc.apvts, pid, u8 (name)));
        addAndMakeVisible (c);
        c->setBounds (r);
        return *c;
    }

    EbbAudioProcessor& proc;
    LiquidLook look;
    CurveView curve;
    juce::OwnedArray<Knob> knobs;
    juce::OwnedArray<ChoiceControl> choices;
    std::function<void (juce::Graphics&)> under, over;
};

inline juce::StringArray rateShort() { return { "1/16", "1/8", "1/4T", "1/8.", "1/4", "1/2", "1 MES" }; }

//==============================================================================
/** A « Houle » : la vague en grand (deux temps), Amount à droite, tout le reste sur une bande en bas. */
inline void buildA (Canvas& c, const Scene& s)
{
    c.addAndMakeVisible (c.curve);
    c.curve.setBounds (10, 40, 520, 196);
    c.curve.setCycles (2);
    c.curve.setFrozen (s.curve, s.amount, s.phase, s.gain());
    c.knob (ParamIDs::amount, "Amount", { 552, 52, 186, 180 }, 136.0f);
    c.knob (ParamIDs::nudge, "Nudge", { 610, 248, 60, 64 }, 46.0f, true);
    c.knob (ParamIDs::threshold, "Seuil", { 680, 248, 60, 64 });
    c.under = [s] (juce::Graphics& g)
    {
        Ui::drawGlassPanel (g, { 538.0f, 40.0f, 212.0f, 196.0f });
        Ui::drawGlassPanel (g, { 10.0f, 242.0f, 740.0f, 72.0f });
        header (g, s, 300.0f);
        // 12 courbes en vignettes
        label (g, "COURBE", { 20.0f, 246.0f, 60.0f, 12.0f }, 8.0f, Ui::dim, juce::Justification::centredLeft);
        for (int i = 0; i < Ebb::numCurves; ++i)
            thumb (g, { 18.0f + (float) i * 29.0f, 260.0f, 26.0f, 22.0f }, i, i == s.curve, false);
        label (g, Ebb::curveName (s.curve), { 18.0f, 286.0f, 120.0f, 14.0f }, 9.5f, juce::Colours::white, juce::Justification::centredLeft, 0.05f);
        label (g, u8 ("12 courbes : de la plus s\xc3\xa8" "che \xc3\xa0 la plus douce"), { 130.0f, 286.0f, 230.0f, 14.0f }, 8.0f, Ui::dim, juce::Justification::centredLeft, 0.0f);
        // Rate et déclenchement
        label (g, u8 ("DUR\xc3\x89" "E"), { 370.0f, 246.0f, 60.0f, 12.0f }, 8.0f, Ui::dim, juce::Justification::centredLeft);
        pillRow (g, { 368.0f, 260.0f, 232.0f, 20.0f }, rateShort(), s.rate, false, 3.0f, 8.4f);
        label (g, u8 ("D\xc3\x89" "CLENCHEMENT"), { 370.0f, 284.0f, 100.0f, 12.0f }, 8.0f, Ui::dim, juce::Justification::centredLeft);
        pillRow (g, { 468.0f, 284.0f, 132.0f, 18.0f }, { "SYNC", "MIDI", "AUDIO" }, s.trigger, false, 3.0f, 8.4f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawVerticalLine (360, 250.0f, 306.0f);
        g.drawVerticalLine (606, 250.0f, 306.0f);
        label (g, u8 ("SYNC \xc2\xb7 128 BPM \xc2\xb7 2 TEMPS AFFICH\xc3\x89S"), { 300.0f, 44.0f, 222.0f, 14.0f }, 7.6f, Ui::dim.withAlpha (0.9f), juce::Justification::centredRight, 0.15f);
    };
}

/** B « Puits » : la courbe enroulée autour du gros bouton, les courbes à gauche, le rythme à droite. */
inline void buildB (Canvas& c, const Scene& s)
{
    c.knob (ParamIDs::amount, "Amount", { 302, 100, 156, 156 }, 116.0f);
    c.knob (ParamIDs::nudge, "Nudge", { 618, 236, 60, 70 }, 46.0f, true);
    c.knob (ParamIDs::threshold, "Seuil", { 684, 236, 60, 70 });
    c.under = [s] (juce::Graphics& g)
    {
        Ui::drawGlassPanel (g, { 10.0f, 40.0f, 196.0f, 272.0f });
        Ui::drawGlassPanel (g, { 554.0f, 40.0f, 196.0f, 272.0f });
        header (g, s, 300.0f);
        const juce::Point<float> c0 (380.0f, 178.0f);
        // le puits : eau qui monte et descend avec le gain
        {
            juce::Graphics::ScopedSaveState save (g);
            juce::Path clip;
            clip.addEllipse (juce::Rectangle<float> (276.0f, 276.0f).withCentre (c0));
            g.reduceClipRegion (clip);
            g.setColour (juce::Colour (0xc0010608));
            g.fillEllipse (juce::Rectangle<float> (276.0f, 276.0f).withCentre (c0));
            Ui::drawLiquid (g, juce::Rectangle<float> (276.0f, 276.0f).withCentre (c0), s.gain() * 0.92f, 1.1f, 4.0f,
                            Ui::aquaDeep.withAlpha (0.32f), Ui::aquaDeep.withAlpha (0.04f), 3.0f);
        }
        ring (g, c0, 86.0f, 132.0f, s);

        // courbes : grille 3 x 4 avec les noms
        label (g, "COURBE", { 20.0f, 46.0f, 80.0f, 14.0f }, 8.6f, Ui::dim, juce::Justification::centredLeft);
        for (int i = 0; i < Ebb::numCurves; ++i)
            thumb (g, { 18.0f + (float) (i % 3) * 61.0f, 64.0f + (float) (i / 3) * 61.0f, 57.0f, 56.0f }, i, i == s.curve, true);

        // rythme
        label (g, u8 ("DUR\xc3\x89" "E"), { 566.0f, 46.0f, 80.0f, 14.0f }, 8.6f, Ui::dim, juce::Justification::centredLeft);
        pillRow (g, { 564.0f, 64.0f, 84.0f, 112.0f }, { "1/16", "1/8", "1/4 T", "1/8 ." }, s.rate < 4 ? s.rate : -1, true, 4.0f, 9.0f);
        pillRow (g, { 654.0f, 64.0f, 84.0f, 84.0f }, { "1/4", "1/2", "1 MES" }, s.rate - 4, true, 4.0f, 9.0f);
        label (g, u8 ("D\xc3\x89" "CLENCHEMENT"), { 566.0f, 186.0f, 120.0f, 12.0f }, 7.8f, Ui::dim, juce::Justification::centredLeft, 0.06f);
        pillRow (g, { 566.0f, 200.0f, 172.0f, 22.0f }, { "SYNC", "MIDI", "AUDIO" }, s.trigger, false, 4.0f, 9.0f);
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine (230, 566.0f, 738.0f);
    };
}

/** C « Marée » : la jauge de niveau à gauche, la vague au centre avec le choix de courbe dessous, les réglages à droite. */
inline void buildC (Canvas& c, const Scene& s)
{
    c.addAndMakeVisible (c.curve);
    c.curve.setBounds (86, 40, 436, 206);
    c.curve.setFrozen (s.curve, s.amount, s.phase, s.gain());
    c.knob (ParamIDs::amount, "Amount", { 548, 46, 190, 160 }, 118.0f);
    c.choice (ParamIDs::rate, u8 ("Dur\xc3\xa9" "e").toRawUTF8(), { 548, 212, 90, 40 });
    c.knob (ParamIDs::nudge, "Nudge", { 646, 210, 46, 66 }, 40.0f, true);
    c.knob (ParamIDs::threshold, "Seuil", { 696, 210, 46, 66 }, 40.0f);
    c.under = [s] (juce::Graphics& g)
    {
        Ui::drawGlassPanel (g, { 538.0f, 40.0f, 212.0f, 272.0f });
        header (g, s, 300.0f);
        tideGauge (g, { 18.0f, 52.0f, 56.0f, 250.0f }, s.gain());
        // choix de courbe : ‹ nom › + vignettes
        auto strip = juce::Rectangle<float> (86.0f, 252.0f, 436.0f, 60.0f);
        Ui::drawGlassPanel (g, strip, 8.0f);
        for (int i = 0; i < Ebb::numCurves; ++i)
            thumb (g, { 94.0f + (float) i * 35.2f, 258.0f, 32.0f, 26.0f }, i, i == s.curve, false);
        label (g, juce::String (Ebb::curveName (s.curve)).toUpperCase(), { 94.0f, 288.0f, 120.0f, 18.0f }, 10.0f, juce::Colours::white, juce::Justification::centredLeft, 0.18f);
        label (g, u8 ("la pompe classique : chute nette, remont\xc3\xa9" "e rapide puis douce"), { 190.0f, 288.0f, 320.0f, 18.0f }, 8.2f, Ui::dim, juce::Justification::centredLeft, 0.0f);
        label (g, u8 ("D\xc3\x89" "CLENCHEMENT"), { 548.0f, 280.0f, 90.0f, 12.0f }, 7.6f, Ui::dim, juce::Justification::centredLeft, 0.06f);
        pillRow (g, { 548.0f, 292.0f, 194.0f, 16.0f }, { "SYNC", "MIDI", "AUDIO" }, s.trigger, false, 4.0f, 8.4f);
    };
}

//==============================================================================
inline void renderAll (const juce::File& dir)
{
    dir.createDirectory();
    EbbAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    Scene s;
    auto setP = [&p] (const char* pid, float v) { auto* prm = p.apvts.getParameter (pid); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); };
    setP (ParamIDs::amount, s.amount * 100.0f);
    setP (ParamIDs::nudge, 4.0f);

    struct Def { const char* file; void (*build) (Canvas&, const Scene&); };
    const Def defs[] = { { "maquette_A_houle", buildA }, { "maquette_B_puits", buildB }, { "maquette_C_maree", buildC } };
    for (auto& d : defs)
    {
        Canvas c (p);
        d.build (c, s);
        for (float scale : { 1.0f, 2.0f })
        {
            const auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
            const auto f = dir.getChildFile (juce::String (d.file) + (scale > 1.5f ? "@2x.png" : ".png"));
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
        }
        std::printf ("  maquette : %s\n", dir.getChildFile (juce::String (d.file) + "@2x.png").getFullPathName().toRawUTF8());
    }
}
}
