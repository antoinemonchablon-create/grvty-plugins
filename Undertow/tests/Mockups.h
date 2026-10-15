// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once
/*
    Maquettes d'interface (phase 3, avant le choix) : trois dispositions rendues avec les vrais
    composants (boutons-gouttes, fond d'eau, onglets) et les éléments signature dessinés à la main :
      - colonnes d'eau du séquenceur, vague qui sert de tête de lecture
      - écran central dont les ondes concentriques suivent l'Index FM
*/
#include "PluginEditor.h"

namespace Mock
{
    //==========================================================================
    /** Bouton-goutte « libre » (non relié à un paramètre) pour l'inspecteur de pas. */
    struct FreeKnob : public juce::Component
    {
        FreeKnob (const juce::String& name, double value, double lo, double hi, const juce::String& suffix)
        {
            title.setText (name.toUpperCase(), juce::dontSendNotification);
            title.setFont (Ui::font (8.8f, true, 0.12f));
            title.setColour (juce::Label::textColourId, Ui::dim);
            title.setJustificationType (juce::Justification::centred);
            addAndMakeVisible (title);
            slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
            slider.setRange (lo, hi);
            slider.setValue (value, juce::dontSendNotification);
            slider.setTextValueSuffix (suffix);
            slider.setNumDecimalPlacesToDisplay (hi - lo > 20 ? 0 : 1);
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 10, 14);
            addAndMakeVisible (slider);
        }
        void resized() override
        {
            auto r = getLocalBounds();
            title.setBounds (r.removeFromTop (12));
            slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), 14);
            slider.setBounds (r);
        }
        juce::Label title;
        juce::Slider slider;
    };

    //==========================================================================
    /** Écran FM : forme d'onde liquide + ondes concentriques dont le nombre et l'amplitude suivent l'Index. */
    inline void paintFmScreen (juce::Graphics& g, juce::Rectangle<float> r, float index, float ratio, float t, bool porthole)
    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path shape;
        if (porthole) shape.addEllipse (r);
        else          shape.addRoundedRectangle (r, 10.0f);

        g.setGradientFill (juce::ColourGradient (juce::Colour (0xd0061a22), r.getCentreX(), r.getY(),
                                                 juce::Colour (0xf0010608), r.getCentreX(), r.getBottom(), false));
        g.fillPath (shape);
        g.reduceClipRegion (shape);

        // ondes concentriques : une goutte tombe à chaque note, l'Index règle combien d'anneaux et leur force
        const auto c = r.getCentre().translated (0.0f, porthole ? 0.0f : r.getHeight() * 0.08f);
        const int rings = 3 + (int) (index * 1.2f);
        const float maxR = std::hypot (r.getWidth(), r.getHeight()) * 0.55f;
        for (int i = 0; i < rings; ++i)
        {
            float life = std::fmod (t * 0.35f + (float) i / (float) rings, 1.0f);
            const float rad = life * maxR;
            const float a = (1.0f - life) * (0.10f + 0.05f * index);
            g.setColour (Ui::aqua.withAlpha (juce::jlimit (0.0f, 0.8f, a)));
            g.drawEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 0.9f).withCentre (c), 1.0f + (1.0f - life) * 1.5f);
        }

        // forme d'onde FM (la vraie formule) dessinée comme une surface d'eau
        juce::Path wave;
        const int n = 220;
        const float w = r.getWidth() * (porthole ? 0.78f : 0.9f), x0 = r.getCentreX() - w * 0.5f;
        const float amp = r.getHeight() * (porthole ? 0.16f : 0.2f);
        for (int i = 0; i <= n; ++i)
        {
            const float ph = (float) i / (float) n * 2.0f;   // deux cycles
            const float mod = std::sin (juce::MathConstants<float>::twoPi * ratio * ph + t);
            const float y = std::sin (juce::MathConstants<float>::twoPi * ph + index * mod * 0.6f);
            const float xx = x0 + w * (float) i / (float) n;
            const float yy = c.y - y * amp;
            if (i == 0) wave.startNewSubPath (xx, yy); else wave.lineTo (xx, yy);
        }
        g.setColour (Ui::aqua.withAlpha (0.18f));
        g.strokePath (wave, juce::PathStrokeType (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colour (0xffe8fffc));
        g.strokePath (wave, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // lecture chiffrée
        g.setFont (Ui::font (9.0f, true, 0.12f));
        g.setColour (Ui::dim);
        const auto txt = porthole ? "IDX " + juce::String (index, 1) + "  x" + juce::String (ratio, 2)
                                  : "INDEX " + juce::String (index, 1) + "   RATIO " + juce::String (ratio, 2);
        g.drawText (txt, r.reduced (14.0f, porthole ? r.getHeight() * 0.18f : 8.0f), juce::Justification::centredBottom);

        g.restoreState();
        g.saveState();
        g.setColour (juce::Colours::white.withAlpha (porthole ? 0.14f : 0.07f));
        g.strokePath (shape, juce::PathStrokeType (porthole ? 2.0f : 1.0f));
        if (porthole)   // reflet du hublot
        {
            juce::Path glint;
            glint.addCentredArc (r.getCentreX(), r.getCentreY(), r.getWidth() * 0.44f, r.getHeight() * 0.44f, 0.0f, -2.4f, -1.2f, true);
            g.setColour (juce::Colours::white.withAlpha (0.12f));
            g.strokePath (glint, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
    }

    //==========================================================================
    /** Séquenceur : colonnes d'eau, la tête de lecture est une vague qui soulève les colonnes qu'elle traverse. */
    inline void paintWaterSeq (juce::Graphics& g, juce::Rectangle<float> r, const Pattern& p, int length, float playPos,
                               float t, bool showLane, int laneLen, int selected = -1, int visibleSteps = 32)
    {
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xd0061a22), 0.0f, r.getY(), juce::Colour (0xe0010608), 0.0f, r.getBottom(), false));
        g.fillRoundedRectangle (r, 10.0f);

        auto a = r.reduced (8.0f, 6.0f);
        juce::Rectangle<float> lane;
        if (showLane) lane = a.removeFromBottom (22.0f);
        a.removeFromBottom (12.0f);
        const float colW = a.getWidth() / (float) visibleSteps;

        for (int i = 0; i < visibleSteps; ++i)
        {
            const auto& s = p.steps[(size_t) i];
            auto col = juce::Rectangle<float> (a.getX() + (float) i * colW, a.getY(), colW, a.getHeight()).reduced (2.0f, 0.0f);
            const bool in = i < length;
            g.setColour (juce::Colours::white.withAlpha (in ? ((i / 4) % 2 == 0 ? 0.05f : 0.03f) : 0.012f));
            g.fillRoundedRectangle (col, 3.0f);

            // la vague de lecture soulève l'eau des colonnes qu'elle touche
            const float d = (float) i - playPos;
            const float swell = std::exp (-d * d * 0.8f);

            if (s.on && in)
            {
                const float level = juce::jlimit (0.05f, 1.0f, (float) (s.note + 12 * s.octave + 12) / 36.0f + swell * 0.08f);
                juce::Graphics::ScopedSaveState ss (g);
                juce::Path clip; clip.addRoundedRectangle (col, 3.0f);
                g.reduceClipRegion (clip);
                const auto top = (s.accent ? Ui::coral : Ui::aqua).withAlpha (0.35f + 0.5f * s.probability + 0.15f * swell);
                Ui::drawLiquid (g, col, level, t, (float) i * 1.3f, top, Ui::aquaDeep.withAlpha (0.25f), 1.0f + swell * 2.5f);
                for (int k = 1; k < s.ratchet; ++k)
                {
                    g.setColour (juce::Colours::white.withAlpha (0.6f));
                    g.fillRect (col.getX() + 2.0f, col.getBottom() - 4.0f - (float) k * 4.0f, col.getWidth() - 4.0f, 1.2f);
                }
            }
            if (s.on && s.slide && in)
            {
                g.setColour (Ui::aqua.withAlpha (0.8f));
                g.drawLine (col.getRight() - 2.0f, col.getY() + 10.0f, col.getRight() + 6.0f, col.getY() + 10.0f, 1.5f);
            }
            if (i == selected)
            {
                g.setColour (Ui::coral);
                g.drawRoundedRectangle (col.expanded (1.0f, 1.0f), 3.0f, 1.5f);
            }
            if (i % 4 == 0)
            {
                g.setColour (Ui::dim.withAlpha (0.7f));
                g.setFont (Ui::font (8.0f, true));
                g.drawText (juce::String (i / 4 + 1), juce::Rectangle<float> (col.getX(), a.getBottom() + 1.0f, colW, 9.0f), juce::Justification::centredLeft);
            }
        }

        // crête de la vague de lecture
        juce::Path crest;
        const float cx = a.getX() + (playPos + 0.5f) * colW;
        for (int k = -40; k <= 40; ++k)
        {
            const float xx = cx + (float) k * colW * 0.08f;
            const float yy = a.getY() + 3.0f - 7.0f * std::exp (-(float) (k * k) * 0.004f) + 1.2f * std::sin ((float) k * 0.6f + t * 5.0f);
            if (k == -40) crest.startNewSubPath (xx, yy); else crest.lineTo (xx, yy);
        }
        g.setColour (Ui::aqua.withAlpha (0.25f));
        g.strokePath (crest, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.strokePath (crest, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setGradientFill (juce::ColourGradient (Ui::aqua.withAlpha (0.16f), cx, a.getY(), Ui::aqua.withAlpha (0.0f), cx, a.getBottom(), false));
        g.fillRect (juce::Rectangle<float> (cx - colW * 0.5f, a.getY(), colW, a.getHeight()));

        if (showLane)
        {
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.fillRoundedRectangle (lane, 3.0f);
            g.setColour (Ui::dim.withAlpha (0.8f));
            g.setFont (Ui::font (7.5f, true, 0.1f));
            for (int i = 0; i < visibleSteps; ++i)
            {
                const float v = p.modLane[(size_t) i];
                auto col = juce::Rectangle<float> (lane.getX() + (float) i * colW, lane.getY(), colW, lane.getHeight()).reduced (3.0f, 2.0f);
                const float h = v * col.getHeight() * 0.5f;
                g.setColour (Ui::coral.withAlpha (i < laneLen ? 0.8f : 0.15f));
                g.fillRect (juce::Rectangle<float> (col.getX(), h > 0 ? col.getCentreY() - h : col.getCentreY(), col.getWidth(), std::max (1.0f, std::abs (h))));
            }
        }

        g.setColour (juce::Colours::white.withAlpha (0.07f));
        g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
    }

    //==========================================================================
    /** Barre de presets (même dessin que Flow Filter). */
    inline void paintPresetBar (juce::Graphics& g, juce::Rectangle<int> r, const juce::String& name, const juce::String& bank)
    {
        auto pill = [&g] (juce::Rectangle<int> rr, bool accent)
        {
            const auto f = rr.toFloat().reduced (0.5f);
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.fillRoundedRectangle (f, f.getHeight() * 0.5f);
            g.setColour (accent ? Ui::aqua.withAlpha (0.5f) : juce::Colours::white.withAlpha (0.1f));
            g.drawRoundedRectangle (f, f.getHeight() * 0.5f, 1.0f);
        };
        auto dice = r.removeFromRight (r.getHeight() + 4);
        r.removeFromRight (6);
        auto save = r.removeFromRight (54);
        r.removeFromRight (8);
        auto prev = r.removeFromLeft (r.getHeight());
        auto next = r.removeFromRight (r.getHeight());
        auto nameR = r.reduced (3, 0);

        for (auto [rr, dir] : { std::pair { prev, -1.0f }, std::pair { next, 1.0f } })
        {
            pill (rr, false);
            const auto c = rr.toFloat().getCentre();
            juce::Path chev;
            chev.startNewSubPath (c.x - 1.5f * dir, c.y - 4.0f);
            chev.lineTo (c.x + 2.0f * dir, c.y);
            chev.lineTo (c.x - 1.5f * dir, c.y + 4.0f);
            g.setColour (Ui::aqua);
            g.strokePath (chev, juce::PathStrokeType (1.5f));
        }
        g.setColour (juce::Colour (0xaa020a0e));
        g.fillRoundedRectangle (nameR.toFloat(), 5.0f);
        g.setColour (juce::Colours::white.withAlpha (0.1f));
        g.drawRoundedRectangle (nameR.toFloat().reduced (0.5f), 5.0f, 1.0f);
        auto inner = nameR.reduced (8, 5);
        auto tag = inner.removeFromLeft (juce::roundToInt (Ui::font (8.0f, true, 0.2f).getStringWidthFloat (bank)) + 14).toFloat();
        g.setColour (Ui::coral.withAlpha (0.15f));
        g.fillRoundedRectangle (tag, tag.getHeight() * 0.5f);
        g.setFont (Ui::font (8.0f, true, 0.2f));
        g.setColour (Ui::coral);
        g.drawText (bank, tag, juce::Justification::centred);
        g.setColour (juce::Colours::white);
        g.setFont (Ui::font (11.5f, true, 0.03f));
        g.drawText (name, inner, juce::Justification::centred);

        pill (save, true);
        g.setColour (Ui::aqua);
        g.setFont (Ui::font (9.5f, true, 0.15f));
        g.drawText ("SAVE", save, juce::Justification::centred);

        const auto c = dice.toFloat().getCentre();
        const float s = (float) dice.getHeight() - 8.0f;
        g.setColour (Ui::coral.withAlpha (0.18f));
        g.fillRoundedRectangle (juce::Rectangle<float> (s, s).withCentre (c), 4.0f);
        g.setColour (Ui::coral.withAlpha (0.8f));
        g.drawRoundedRectangle (juce::Rectangle<float> (s, s).withCentre (c), 4.0f, 1.3f);
        g.setColour (juce::Colours::white);
        for (auto [dx, dy] : { std::pair { -1, -1 }, std::pair { 1, -1 }, std::pair { 0, 0 }, std::pair { -1, 1 }, std::pair { 1, 1 } })
            g.fillEllipse (juce::Rectangle<float> (2.8f, 2.8f).withCentre ({ c.x + (float) dx * s * 0.26f, c.y + (float) dy * s * 0.26f }));
    }

    inline void paintLogo (juce::Graphics& g, float t, float x = 20.0f, float y = 22.0f)
    {
        juce::Path w;
        for (int i = 0; i <= 24; ++i)
        {
            const float xx = x + (float) i, yy = y + 3.5f * std::sin ((float) i * 0.42f + t * 2.0f);
            if (i == 0) w.startNewSubPath (xx, yy); else w.lineTo (xx, yy);
        }
        g.setColour (Ui::aqua);
        g.strokePath (w, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white);
        g.setFont (Ui::font (18.0f, true, -0.02f));
        g.drawText ("under", (int) x + 32, (int) y - 12, 60, 24, juce::Justification::centredLeft);
        g.setColour (Ui::aqua);
        g.setFont (Ui::font (9.0f, true, 0.45f));
        g.drawText ("TOW", (int) x + 90, (int) y - 9, 50, 20, juce::Justification::centredLeft);
    }

    inline void paintVersion (juce::Graphics& g, int w)
    {
        g.setColour (Ui::dim.withAlpha (0.7f));
        g.setFont (Ui::font (8.5f, false, 0.05f));
        g.drawText ("v1.0.2", w - 80, 13, 66, 20, juce::Justification::centredRight);
    }

    inline void paintLabel (juce::Graphics& g, const juce::String& txt, juce::Rectangle<int> r, juce::Colour c = Ui::aqua)
    {
        g.setColour (c);
        g.setFont (Ui::font (9.0f, true, 0.2f));
        g.drawText (txt, r, juce::Justification::centredLeft);
    }

    //==========================================================================
    /** Base commune : possède les composants et place les boutons reliés aux paramètres. */
    struct Base : public juce::Component
    {
        Base (UndertowAudioProcessor& p) : proc (p) { setLookAndFeel (&look); }
        ~Base() override { setLookAndFeel (nullptr); }

        void knob (const juce::String& id, const juce::String& name, juce::Rectangle<int> r)
        {
            auto* p = proc.apvts.getParameter (id);
            juce::Component* c = nullptr;
            if (dynamic_cast<juce::AudioParameterChoice*> (p) != nullptr) c = owned.add (new ChoiceControl (proc.apvts, id, name));
            else if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr) { c = owned.add (new PillToggle (proc.apvts, id, name.toUpperCase(), PillToggle::Icon::dot)); r = r.withSizeKeepingCentre (r.getWidth(), 24); }
            else
            {
                const bool bip = p->getNormalisableRange().start < 0.0f && p->getNormalisableRange().end > 0.0f;
                c = owned.add (new Knob (proc.apvts, id, name, bip));
            }
            addAndMakeVisible (c);
            c->setBounds (r);
        }

        void freeKnob (const juce::String& name, double v, double lo, double hi, const juce::String& suffix, juce::Rectangle<int> r)
        {
            auto* c = owned.add (new FreeKnob (name, v, lo, hi, suffix));
            addAndMakeVisible (c);
            c->setBounds (r);
        }

        void tabs (juce::StringArray names, int current, juce::Rectangle<int> r)
        {
            auto* tb = new TabBar (names);
            owned.add (tb);
            tb->setCurrent (current);
            for (int i = 0; i < 40; ++i) tb->animate();
            addAndMakeVisible (tb);
            tb->setBounds (r);
        }

        void meter (juce::Rectangle<int> r)
        {
            auto* m = owned.add (new LiquidMeter (proc.outputLevel));
            addAndMakeVisible (m);
            m->setBounds (r);
        }

        UndertowAudioProcessor& proc;
        LiquidLook look;
        juce::OwnedArray<juce::Component> owned;
        float t = 3.7f, playPos = 6.3f, index = 3.2f, ratio = 3.5f;
    };

    //==========================================================================
    /** A « Lagon » : écran FM à gauche, pages d'onglets à droite, séquenceur en bandeau dessous. Compact, façon device Ableton. */
    struct LayoutA : public Base
    {
        explicit LayoutA (UndertowAudioProcessor& p) : Base (p)
        {
            setSize (900, 430);
            namespace id = ParamIDs;
            tabs ({ "SOUND", "MOD", "FX" }, 0, { 400, 44, 440, 26 });
            const int x0 = 404, y0 = 78, cw = 72, ch = 92;
            const char* ids[]   = { id::oscAShape, id::oscBRatio, id::fmIndex, id::fmFeedback, id::oscMix, id::cutoff,
                                    id::oscBShape, id::fmMode, id::modToIndex, id::modDecay, id::resonance, id::ampDecay };
            const char* names[] = { "Shape A", "Ratio", "Index", "Feedback", "A / B", "Cutoff",
                                    "Shape B", "FM Mode", "Env > Idx", "Env Dec", "Reso", "Decay" };
            for (int i = 0; i < 12; ++i)
                knob (ids[i], names[i], { x0 + (i % 6) * cw, y0 + (i / 6) * ch, cw - 4, ch - 8 });
            meter ({ 874, 78, 10, 176 });

            // réglages du séquenceur : deux lignes compactes à gauche, la grille prend le reste
            int x = 18;
            for (auto [pid, nm, w] : { std::tuple { id::seqMode, "Mode", 58 }, std::tuple { id::seqRate, "Rate", 58 }, std::tuple { id::scale, "Scale", 92 },
                                        std::tuple { id::rootNote, "Root", 52 } })
            {
                knob (pid, nm, { x, 270, w, 44 });
                x += w + 4;
            }
            x = 18;
            for (auto [pid, nm, w] : { std::tuple { id::seqLength, "Steps", 46 }, std::tuple { id::swing, "Swing", 46 }, std::tuple { id::accent, "Accent", 46 },
                                        std::tuple { id::mutateAmount, "Mutate", 46 }, std::tuple { id::modLaneDest, "Lane", 72 } })
            {
                knob (pid, nm, { x, 318, w, 70 });
                x += w + 4;
            }
        }

        void paint (juce::Graphics& g) override
        {
            Ui::drawWaterBackground (g, getLocalBounds().toFloat());
            paintLogo (g, t);
            paintPresetBar (g, { 300, 9, 360, 26 }, "Breakwater", "HARDGROOVE");
            paintVersion (g, getWidth());
            Ui::drawGlassPanel (g, { 396.0f, 44.0f, 494.0f, 216.0f });
            paintFmScreen (g, { 10.0f, 44.0f, 378.0f, 216.0f }, index, ratio, t, false);
            Ui::drawGlassPanel (g, { 10.0f, 266.0f, 880.0f, 156.0f });
            paintWaterSeq (g, { 300.0f, 272.0f, 584.0f, 144.0f }, proc.pattern.get(), 16, playPos, t, true, 13, -1, 16);
            // boutons de génération
            int bx = 22;
            for (auto label : { "EUCLID", "RND", "<", ">" })
            {
                juce::Rectangle<float> b ((float) bx, 390.0f, 20.0f + (float) std::strlen (label) * 5.0f, 20.0f);
                g.setColour (juce::Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (b, 10.0f);
                g.setColour (Ui::aqua.withAlpha (0.5f)); g.drawRoundedRectangle (b, 10.0f, 1.0f);
                g.setFont (Ui::font (8.5f, true, 0.1f)); g.setColour (Ui::aqua); g.drawText (label, b, juce::Justification::centred);
                bx = (int) b.getRight() + 4;
            }
            paintLabel (g, "MAQUETTE A", { 150, 12, 120, 20 }, Ui::coral);
        }
    };

    //==========================================================================
    /** B « Abysse » : le séquenceur au centre en grand, un hublot FM, et un inspecteur de pas en bas. */
    struct LayoutB : public Base
    {
        explicit LayoutB (UndertowAudioProcessor& p) : Base (p)
        {
            setSize (940, 560);
            namespace id = ParamIDs;
            tabs ({ "SOUND", "MOD", "FX" }, 0, { 190, 44, 360, 26 });
            const char* ids[]   = { id::oscBRatio, id::fmIndex, id::fmFeedback, id::oscAShape, id::oscMix, id::cutoff, id::resonance, id::modToIndex, id::ampDecay };
            const char* names[] = { "Ratio", "Index", "Feedback", "Shape A", "A / B", "Cutoff", "Reso", "Env > Idx", "Decay" };
            for (int i = 0; i < 9; ++i)
                knob (ids[i], names[i], { 190 + i * 76, 78, 72, 84 });
            meter ({ 912, 50, 10, 116 });

            int x = 22;
            for (auto [pid, nm, w] : { std::tuple { id::seqMode, "Mode", 60 }, std::tuple { id::seqRate, "Rate", 60 }, std::tuple { id::scale, "Scale", 100 },
                                        std::tuple { id::rootNote, "Root", 54 }, std::tuple { id::seqOctave, "Oct", 46 }, std::tuple { id::seqLength, "Steps", 46 },
                                        std::tuple { id::swing, "Swing", 46 }, std::tuple { id::accent, "Accent", 46 }, std::tuple { id::modLaneDest, "Mod Lane", 76 },
                                        std::tuple { id::modLaneLength, "Mod Steps", 52 }, std::tuple { id::mutateAmount, "Mutate", 46 }, std::tuple { id::mutateBars, "Every", 66 } })
            {
                knob (pid, nm, { x, 178, w, 62 });
                x += w + 4;
            }

            // inspecteur du pas sélectionné
            int ix = 110;
            for (auto [nm, v, lo, hi, suf] : { std::tuple { "Note", 7.0, 0.0, 11.0, "" }, std::tuple { "Octave", 0.0, -2.0, 2.0, "" },
                                               std::tuple { "Velocity", 90.0, 0.0, 100.0, " %" }, std::tuple { "Gate", 40.0, 5.0, 100.0, " %" },
                                               std::tuple { "Proba", 60.0, 0.0, 100.0, " %" }, std::tuple { "Ratchet", 2.0, 1.0, 4.0, "x" },
                                               std::tuple { "Lock Idx", 6.5, 0.0, 10.0, "" }, std::tuple { "Lock Cut", 1200.0, 20.0, 20000.0, " Hz" } })
            {
                freeKnob (nm, v, lo, hi, suf, { ix, 476, 70, 78 });
                ix += 74;
            }
        }

        void paint (juce::Graphics& g) override
        {
            Ui::drawWaterBackground (g, getLocalBounds().toFloat());
            paintLogo (g, t);
            paintPresetBar (g, { 300, 9, 360, 26 }, "Kelp Forest", "HYPNOTIC");
            paintVersion (g, getWidth());
            paintFmScreen (g, { 14.0f, 44.0f, 164.0f, 124.0f }, index, ratio, t, true);
            Ui::drawGlassPanel (g, { 184.0f, 44.0f, 746.0f, 124.0f });
            Ui::drawGlassPanel (g, { 10.0f, 174.0f, 920.0f, 292.0f });
            paintWaterSeq (g, { 16.0f, 244.0f, 908.0f, 216.0f }, proc.pattern.get(), 16, playPos, t, true, 13, 6);
            Ui::drawGlassPanel (g, { 10.0f, 472.0f, 920.0f, 82.0f });
            paintLabel (g, "PAS 7", { 24, 488, 80, 16 }, Ui::coral);
            g.setColour (Ui::dim); g.setFont (Ui::font (8.0f, true, 0.1f));
            g.drawText ("inspecteur", 24, 504, 80, 12, juce::Justification::centredLeft);
            for (auto [lbl, on, xx] : { std::tuple { "ACCENT", true, 720 }, std::tuple { "SLIDE", false, 800 } })
            {
                juce::Rectangle<float> b ((float) xx, 500.0f, 72.0f, 24.0f);
                g.setColour (on ? Ui::aqua.withAlpha (0.3f) : juce::Colours::white.withAlpha (0.05f)); g.fillRoundedRectangle (b, 12.0f);
                g.setColour (on ? Ui::aqua : juce::Colours::white.withAlpha (0.15f)); g.drawRoundedRectangle (b, 12.0f, 1.0f);
                g.setColour (on ? juce::Colours::white : Ui::text); g.setFont (Ui::font (9.5f, true, 0.1f)); g.drawText (lbl, b, juce::Justification::centred);
            }
            paintLabel (g, "MAQUETTE B", { 150, 12, 120, 20 }, Ui::coral);
        }
    };

    //==========================================================================
    /** C « Courant » : très bas et large, trois colonnes (son | séquenceur | onglets). Tient dans la hauteur d'un device Ableton. */
    struct LayoutC : public Base
    {
        explicit LayoutC (UndertowAudioProcessor& p) : Base (p)
        {
            setSize (1060, 300);
            namespace id = ParamIDs;
            // colonne gauche : les 4 réglages FM essentiels
            knob (id::oscBRatio,  "Ratio",    { 150, 44, 62, 84 });
            knob (id::fmIndex,    "Index",    { 212, 44, 62, 84 });
            knob (id::fmFeedback, "Feedback", { 150, 136, 62, 84 });
            knob (id::modToIndex, "Env > Idx",{ 212, 136, 62, 84 });

            tabs ({ "SOUND", "MOD", "FX" }, 1, { 790, 36, 250, 24 });
            const juce::String ids[] = { id::modAttack, id::modDecay, id::modCurve, id::lfoDest (0), id::lfoDepth (0), id::lfoSyncRate (0) };
            const char* names[] = { "Mod Att", "Mod Dec", "Curve", "LFO1 Dest", "Depth", "Rate" };
            for (int i = 0; i < 6; ++i)
                knob (ids[i], names[i], { 792 + (i % 3) * 82, 66 + (i / 3) * 92, 80, 86 });
            meter ({ 1044, 70, 8, 170 });

            int x = 292;
            for (auto [pid, nm, w] : { std::tuple { id::seqMode, "Mode", 58 }, std::tuple { id::seqRate, "Rate", 58 }, std::tuple { id::scale, "Scale", 90 },
                                        std::tuple { id::rootNote, "Root", 52 }, std::tuple { id::seqLength, "Steps", 44 }, std::tuple { id::swing, "Swing", 44 },
                                        std::tuple { id::mutateAmount, "Mutate", 44 }, std::tuple { id::modLaneDest, "Lane", 72 } })
            {
                knob (pid, nm, { x, 230, w, 62 });
                x += w + 2;
            }
        }

        void paint (juce::Graphics& g) override
        {
            Ui::drawWaterBackground (g, getLocalBounds().toFloat());
            paintLogo (g, t, 16.0f, 18.0f);
            paintPresetBar (g, { 340, 5, 360, 24 }, "Crosscurrent", "MENTAL");
            g.setColour (Ui::dim.withAlpha (0.7f)); g.setFont (Ui::font (8.5f, false, 0.05f));
            g.drawText ("v1.0.2", getWidth() - 80, 8, 66, 20, juce::Justification::centredRight);
            Ui::drawGlassPanel (g, { 8.0f, 36.0f, 272.0f, 256.0f });
            paintFmScreen (g, { 14.0f, 48.0f, 132.0f, 132.0f }, index, ratio, t, true);
            Ui::drawGlassPanel (g, { 286.0f, 36.0f, 494.0f, 256.0f });
            paintWaterSeq (g, { 292.0f, 42.0f, 482.0f, 184.0f }, proc.pattern.get(), 16, playPos, t, true, 13, -1, 16);
            Ui::drawGlassPanel (g, { 786.0f, 36.0f, 266.0f, 256.0f });
            paintLabel (g, "MAQUETTE C", { 150, 8, 120, 20 }, Ui::coral);
        }
    };
}
