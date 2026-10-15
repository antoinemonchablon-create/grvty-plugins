// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once
/*
    Maquettes d'interface de Confluence (avant la phase 3, à choisir) : trois dispositions rendues avec les vrais
    composants (boutons-gouttes, menus, fond d'eau, onglets) et les éléments signature dessinés à la main :
      - le clavier vu sous la surface de l'eau, notes de la gamme éclairées ;
      - les notes de l'accord qui montent comme des bulles ;
      - le nom de l'accord et son degré en grand ;
      - les enchaînements conseillés sur un cercle des quintes en tourbillon ;
      - les 8 pads.
*/
#include "PluginEditor.h"

namespace Mock
{
using namespace Theory;

inline juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

/** Ce que les maquettes montrent : l'accord joué, la gamme, les pads, les suggestions. */
struct Scene
{
    int rootPc = 9, scale = naturalMinor;
    std::vector<int> chord;          // notes jouées (basse comprise)
    int bass = -1;
    juce::String name, degree;
    int chordDegree = 0;
    std::array<int, 3> next { -1, -1, -1 };
    std::array<juce::String, 8> padNames, padDegrees;
    int activePad = 0;
    juce::String styleLabel = "DEEP & HOUSE", presetName = "Warm Current";
};

inline Scene makeScene (ConfluenceAudioProcessor& p)
{
    Scene s;
    auto cs = p.readChordSettings();
    s.rootPc = cs.rootPc;
    s.scale = cs.scale;
    Engine::Resolved r;
    Engine::resolve (cs, p.getPads(), p.getLearned(), 57, {}, r);   // A2 : Am9
    if (r.voiced.bass >= 0) s.chord.push_back (r.voiced.bass);
    for (int i = 0; i < r.voiced.voices.count; ++i) s.chord.push_back (r.voiced.voices.n[i]);
    s.bass = r.voiced.bass;
    char buf[48];
    chordName (r.voiced.rootPc, s.chord.data(), (int) s.chord.size(), useFlats (s.scale, s.rootPc), buf, 48);
    s.name = buf;
    uint16_t mask = 0;
    for (int n : s.chord) mask = (uint16_t) (mask | (1u << (unsigned) mod12 (n - r.voiced.rootPc)));
    romanNumeral (s.scale, s.rootPc, r.chordRootPc, mask, buf, 48);
    s.degree = u8 (buf);
    s.chordDegree = degreeOfPc (s.scale, s.rootPc, r.chordRootPc);
    s.next = Harmony::suggestNext (s.scale, s.chordDegree, Harmony::styleDeep);

    auto padCs = cs;
    padCs.mode = Engine::modePads;
    Voicing::Notes prev;
    for (int i = 0; i < 8; ++i)
    {
        Engine::Resolved pr;
        if (Engine::resolve (padCs, p.getPads(), p.getLearned(), padCs.padBase + i, prev, pr))
        {
            std::vector<int> v;
            if (pr.voiced.bass >= 0) v.push_back (pr.voiced.bass);
            for (int k = 0; k < pr.voiced.voices.count; ++k) v.push_back (pr.voiced.voices.n[k]);
            chordName (pr.voiced.rootPc, v.data(), (int) v.size(), useFlats (s.scale, s.rootPc), buf, 48);
            s.padNames[(size_t) i] = buf;
            uint16_t m = 0;
            for (int n : v) m = (uint16_t) (m | (1u << (unsigned) mod12 (n - pr.voiced.rootPc)));
            romanNumeral (s.scale, s.rootPc, pr.chordRootPc, m, buf, 48);
            s.padDegrees[(size_t) i] = u8 (buf);
        }
    }
    return s;
}

//==============================================================================
// Éléments dessinés
//==============================================================================
inline bool isBlack (int n) { const int pc = mod12 (n); return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }

/** Position horizontale (centre) d'une note sur le clavier dessiné. */
struct KeyboardGeom
{
    juce::Rectangle<float> r;
    int low = 36, high = 84;
    int numWhite() const { int n = 0; for (int k = low; k <= high; ++k) n += isBlack (k) ? 0 : 1; return n; }
    float whiteW() const { return r.getWidth() / (float) numWhite(); }
    float centreX (int note) const
    {
        int w = 0;
        for (int k = low; k < note; ++k) w += isBlack (k) ? 0 : 1;
        return isBlack (note) ? r.getX() + (float) w * whiteW() : r.getX() + ((float) w + 0.5f) * whiteW();
    }
};

/** Clavier vu sous la surface : touches de verre sombre, notes de la gamme éclairées, accord en lumière. */
inline void paintUnderwaterKeyboard (juce::Graphics& g, const KeyboardGeom& kg, const Scene& s, float t)
{
    const auto r = kg.r;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff072630), 0.0f, r.getY(), juce::Colour (0xff020a0e), 0.0f, r.getBottom(), false));
    g.fillRoundedRectangle (r.expanded (4.0f, 4.0f), 8.0f);

    const float ww = kg.whiteW();
    auto isChord = [&s] (int n) { return std::find (s.chord.begin(), s.chord.end(), n) != s.chord.end(); };
    const auto mask = scaleMask (s.scale, s.rootPc);
    auto inSc = [mask] (int n) { return ((mask >> (unsigned) mod12 (n)) & 1u) != 0; };

    // touches blanches
    int w = 0;
    for (int n = kg.low; n <= kg.high; ++n)
    {
        if (isBlack (n)) continue;
        auto k = juce::Rectangle<float> (r.getX() + (float) w * ww + 1.0f, r.getY(), ww - 2.0f, r.getHeight());
        ++w;
        const bool lit = isChord (n), sc = inSc (n);
        if (lit)
        {
            const auto col = n == s.bass ? Ui::coral : Ui::aqua;
            g.setColour (col.withAlpha (0.25f));
            g.fillRoundedRectangle (k.expanded (2.0f, 0.0f), 4.0f);
            g.setGradientFill (juce::ColourGradient (col.withAlpha (0.98f), 0.0f, k.getY(), col.withAlpha (0.55f), 0.0f, k.getBottom(), false));
        }
        else
        {
            const auto top = sc ? juce::Colour (0xff1c4752) : juce::Colour (0xff0e2128);
            g.setGradientFill (juce::ColourGradient (top, 0.0f, k.getY(), top.darker (0.6f), 0.0f, k.getBottom(), false));
        }
        g.fillRoundedRectangle (k, 3.0f);
        if (sc && ! lit)
        {
            g.setColour (Ui::aqua.withAlpha (0.55f));
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ k.getCentreX(), k.getBottom() - 8.0f }));
        }
        if (mod12 (n) == s.rootPc)
        {
            g.setColour (Ui::aqua.withAlpha (0.8f));
            g.drawRoundedRectangle (k.reduced (0.5f), 3.0f, 1.2f);
        }
        if (mod12 (n) == 0)
        {
            g.setColour (Ui::dim.withAlpha (0.8f));
            g.setFont (Ui::font (7.0f, true));
            g.drawText ("C" + juce::String (n / 12 - 2), k.withTrimmedTop (k.getHeight() - 22.0f), juce::Justification::centred);
        }
    }
    // touches noires
    for (int n = kg.low; n <= kg.high; ++n)
    {
        if (! isBlack (n)) continue;
        const float cx = kg.centreX (n);
        auto k = juce::Rectangle<float> (cx - ww * 0.32f, r.getY(), ww * 0.64f, r.getHeight() * 0.6f);
        const bool lit = isChord (n), sc = inSc (n);
        g.setColour (juce::Colour (0xff01070a));
        g.fillRoundedRectangle (k.expanded (1.0f), 2.5f);
        const auto top = lit ? (n == s.bass ? Ui::coral : Ui::aqua) : (sc ? juce::Colour (0xff1f4c57) : juce::Colour (0xff081419));
        g.setGradientFill (juce::ColourGradient (top.withAlpha (lit ? 0.98f : 0.9f), 0.0f, k.getY(), top.withAlpha (lit ? 0.5f : 0.35f), 0.0f, k.getBottom(), false));
        g.fillRoundedRectangle (k, 2.5f);
        if (sc && ! lit)
        {
            g.setColour (Ui::aqua.withAlpha (0.5f));
            g.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ k.getCentreX(), k.getBottom() - 6.0f }));
        }
    }

    // caustiques : la lumière de surface qui danse sur les touches
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (r.toNearestInt());
        for (int i = 0; i < 6; ++i)
        {
            juce::Path c;
            const float fi = (float) i;
            for (float x = r.getX(); x <= r.getRight(); x += 6.0f)
            {
                const float y = r.getY() + r.getHeight() * (0.15f + 0.13f * fi)
                              + 5.0f * std::sin (x * 0.03f + t * 1.3f + fi * 1.7f) + 3.0f * std::sin (x * 0.071f - t * 0.9f + fi);
                if (x == r.getX()) c.startNewSubPath (x, y); else c.lineTo (x, y);
            }
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.strokePath (c, juce::PathStrokeType (2.0f + fi * 0.4f));
        }
        // la surface de l'eau, au-dessus du clavier
        g.setGradientFill (juce::ColourGradient (Ui::aqua.withAlpha (0.18f), 0.0f, r.getY(), Ui::aqua.withAlpha (0.0f), 0.0f, r.getY() + 26.0f, false));
        g.fillRect (r.withHeight (26.0f));
    }
    juce::Path surface;
    for (float x = r.getX() - 4.0f; x <= r.getRight() + 4.0f; x += 4.0f)
    {
        const float y = r.getY() - 6.0f + 2.0f * std::sin (x * 0.05f + t * 2.0f) + 1.2f * std::sin (x * 0.13f - t * 1.4f);
        if (x <= r.getX() - 3.9f) surface.startNewSubPath (x, y); else surface.lineTo (x, y);
    }
    g.setColour (Ui::aqua.withAlpha (0.25f));
    g.strokePath (surface, juce::PathStrokeType (4.0f));
    g.setColour (juce::Colour (0xffd8fffa).withAlpha (0.8f));
    g.strokePath (surface, juce::PathStrokeType (1.2f));
}

/** Les notes de l'accord montent comme des bulles, depuis leur touche jusqu'au nom de l'accord. */
inline void paintBubbles (juce::Graphics& g, const KeyboardGeom& kg, const Scene& s, float topY, float t)
{
    const float bottom = kg.r.getY() - 8.0f;
    for (size_t i = 0; i < s.chord.size(); ++i)
    {
        const int n = s.chord[i];
        const float x0 = kg.centreX (n);
        const bool bass = n == s.bass;
        for (int b = 0; b < 4; ++b)
        {
            const float life = std::fmod (t * 0.18f + (float) b * 0.25f + (float) i * 0.11f, 1.0f);
            const float y = bottom - life * (bottom - topY);
            const float x = x0 + 6.0f * std::sin (life * 9.0f + (float) i) * life;
            const float rad = (b == 0 ? 7.0f : 3.0f + 2.5f * (1.0f - (float) b / 4.0f)) * (0.6f + 0.6f * life);
            const auto col = bass ? Ui::coral : Ui::aqua;
            const float a = std::sin (life * juce::MathConstants<float>::pi);
            g.setColour (col.withAlpha (0.10f * a));
            g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre ({ x, y }));
            g.setColour (col.withAlpha (0.75f * a));
            g.drawEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre ({ x, y }), 1.1f);
            g.setColour (juce::Colours::white.withAlpha (0.6f * a));
            g.fillEllipse (juce::Rectangle<float> (rad * 0.6f, rad * 0.45f).withCentre ({ x - rad * 0.35f, y - rad * 0.4f }));
        }
        // filet de lumière entre la touche et la surface
        g.setGradientFill (juce::ColourGradient ((bass ? Ui::coral : Ui::aqua).withAlpha (0.16f), x0, bottom,
                                                 Ui::aqua.withAlpha (0.0f), x0, topY + (bottom - topY) * 0.4f, false));
        g.fillRect (juce::Rectangle<float> (x0 - 1.0f, topY + (bottom - topY) * 0.4f, 2.0f, (bottom - topY) * 0.6f));
    }
}

/** Nom de l'accord et degré, en grand. */
inline void paintChordTitle (juce::Graphics& g, juce::Rectangle<float> r, const Scene& s, float nameSize)
{
    g.setColour (Ui::aqua.withAlpha (0.12f));
    g.setFont (Ui::font (nameSize, true, -0.02f));
    g.drawText (s.name, r.translated (0.0f, 3.0f), juce::Justification::centred);
    g.setColour (juce::Colours::white);
    g.drawText (s.name, r, juce::Justification::centred);

    juce::GlyphArrangement ga;
    ga.addLineOfText (Ui::font (nameSize, true, -0.02f), s.name, 0.0f, 0.0f);
    const float nameW = ga.getBoundingBox (0, -1, true).getWidth();
    g.setColour (Ui::aqua);
    g.setFont (Ui::font (nameSize * 0.48f, true));
    g.drawText (s.degree, juce::Rectangle<float> (r.getCentreX() + nameW * 0.5f + 8.0f, r.getY(), 80.0f, r.getHeight() * 0.62f),
                juce::Justification::bottomLeft);

    juce::StringArray notes;
    for (int n : s.chord) { char nn[16]; noteName (n, nn, 16, useFlats (s.scale, s.rootPc)); notes.add (juce::String (nn)); }
    g.setColour (Ui::dim);
    g.setFont (Ui::font (9.5f, true, 0.1f));
    g.drawText (notes.joinIntoString ("   "), r.withTrimmedTop (r.getHeight() * 0.78f).expanded (0.0f, 14.0f), juce::Justification::centred);
}

/** Cercle des quintes en tourbillon : la gamme éclairée, l'accord joué au centre, les enchaînements conseillés qui brillent. */
inline void paintWhirlpool (juce::Graphics& g, juce::Rectangle<float> r, const Scene& s, float t, bool labels = true)
{
    const auto c = r.getCentre();
    const float R = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f - 14.0f;

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0a2f3a), c.x, c.y, juce::Colour (0x00020a0e), c.x, c.y + R * 1.25f, true));
    g.fillEllipse (juce::Rectangle<float> (R * 2.5f, R * 2.5f).withCentre (c));

    // bras du tourbillon (spirales qui tournent lentement)
    for (int arm = 0; arm < 5; ++arm)
    {
        juce::Path sp;
        for (int i = 0; i <= 60; ++i)
        {
            const float f = (float) i / 60.0f;
            const float a = (float) arm * juce::MathConstants<float>::twoPi / 5.0f + t * 0.35f + f * 4.2f;
            const float rad = R * (1.05f - 0.9f * f);
            const auto p = c.getPointOnCircumference (rad, a);
            if (i == 0) sp.startNewSubPath (p); else sp.lineTo (p);
        }
        g.setColour (Ui::aqua.withAlpha (0.07f));
        g.strokePath (sp, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setColour (Ui::aqua.withAlpha (0.12f));
    g.drawEllipse (juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c), 1.0f);

    // 12 positions du cercle des quintes (C en haut)
    const auto mask = scaleMask (s.scale, s.rootPc);
    const int curRootPc = mod12 (s.rootPc + degreeInterval (s.scale, juce::jmax (0, s.chordDegree)));
    for (int k = 0; k < 12; ++k)
    {
        const int pc = mod12 (k * 7);
        const float a = (float) k * juce::MathConstants<float>::twoPi / 12.0f;
        const auto p = c.getPointOnCircumference (R, a);
        const bool sc = ((mask >> (unsigned) pc) & 1u) != 0;
        const int deg = degreeOfPc (s.scale, s.rootPc, pc);
        int sugg = -1;
        for (int j = 0; j < 3; ++j) if (s.next[(size_t) j] >= 0 && deg == s.next[(size_t) j]) sugg = j;
        const bool current = pc == curRootPc;

        if (sugg >= 0)
        {
            // courant : de l'accord joué vers l'accord conseillé, aspiré par le centre du tourbillon
            int curK = 0;
            for (int j = 0; j < 12; ++j) if (mod12 (j * 7) == curRootPc) curK = j;
            const auto from = c.getPointOnCircumference (R - 14.0f, (float) curK * juce::MathConstants<float>::twoPi / 12.0f);
            const auto to = c.getPointOnCircumference (R - 14.0f, a);
            const auto ctrl = c + ((from + to) * 0.5f - c) * 0.15f;
            juce::Path flow;
            flow.startNewSubPath (from);
            flow.quadraticTo (ctrl, to);
            g.setColour (Ui::aqua.withAlpha (0.6f - 0.15f * (float) sugg));
            const float dashes[] = { 5.0f, 4.0f };
            juce::Path dashed;
            juce::PathStrokeType (1.6f).createDashedStroke (dashed, flow, dashes, 2);
            g.fillPath (dashed);
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (to));
            g.setColour (Ui::aqua.withAlpha (0.22f));
            g.fillEllipse (juce::Rectangle<float> (34.0f, 34.0f).withCentre (p));
        }
        const float d = current ? 26.0f : (sc ? 22.0f : 16.0f);
        g.setColour (current ? Ui::coral : (sugg >= 0 ? Ui::aqua : (sc ? juce::Colour (0xff1d5966) : juce::Colour (0xff0b1d24))));
        g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (p));
        g.setColour (juce::Colours::white.withAlpha (sc ? 0.35f : 0.08f));
        g.drawEllipse (juce::Rectangle<float> (d, d).withCentre (p), 1.0f);
        g.setColour (sc || current ? juce::Colours::white : Ui::dim.withAlpha (0.6f));
        g.setFont (Ui::font (current || sugg >= 0 ? 10.5f : 9.0f, true));
        g.drawText (pcName (pc, useFlats (s.scale, s.rootPc)), juce::Rectangle<float> (d + 10.0f, d).withCentre (p), juce::Justification::centred);

        if (labels && (sugg >= 0 || current) && deg >= 0)
        {
            auto shape = buildScaleChord (s.scale, deg, ctSeventh);
            uint16_t m = 0;
            for (int i = 0; i < shape.count; ++i) m = (uint16_t) (m | (1u << (unsigned) mod12 (shape.offsets[i])));
            char buf[16];
            romanNumeral (s.scale, s.rootPc, pc, m, buf, 16);
            const auto lp = c.getPointOnCircumference (R + 24.0f, a);
            g.setColour (current ? Ui::coral : Ui::aqua);
            g.setFont (Ui::font (10.0f, true));
            g.drawText (u8 (buf), juce::Rectangle<float> (40.0f, 14.0f).withCentre (lp), juce::Justification::centred);
        }
    }
}

/** Pads : tuiles remplies d'eau, nom de l'accord et degré ; le pad joué ondule. */
inline void paintPads (juce::Graphics& g, juce::Rectangle<float> r, int cols, int rows, const Scene& s, float t, float nameSize = 13.0f)
{
    const float gap = 6.0f;
    const float pw = (r.getWidth() - gap * (float) (cols - 1)) / (float) cols;
    const float ph = (r.getHeight() - gap * (float) (rows - 1)) / (float) rows;
    for (int i = 0; i < cols * rows && i < 8; ++i)
    {
        const int col = i % cols, row = rows - 1 - i / cols;   // pad 1 en bas à gauche, comme Push
        auto p = juce::Rectangle<float> (r.getX() + (float) col * (pw + gap), r.getY() + (float) row * (ph + gap), pw, ph);
        const bool active = i == s.activePad;
        g.setColour (juce::Colour (0xff04121a));
        g.fillRoundedRectangle (p, 7.0f);
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (p.toNearestInt());
            Ui::drawLiquid (g, p, active ? 0.72f : 0.32f + 0.05f * (float) (i % 3), t, (float) i * 1.3f,
                            (active ? Ui::aqua : Ui::aquaDeep).withAlpha (active ? 0.65f : 0.4f), Ui::aquaDeep.withAlpha (0.12f), active ? 2.4f : 1.2f);
            if (active)
                for (int k = 0; k < 3; ++k)
                {
                    const float life = std::fmod (t * 0.5f + (float) k / 3.0f, 1.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.25f * (1.0f - life)));
                    g.drawEllipse (juce::Rectangle<float> (pw * life * 1.4f, ph * life * 0.7f).withCentre (p.getCentre()), 1.0f);
                }
        }
        g.setColour (active ? Ui::aqua : juce::Colours::white.withAlpha (0.1f));
        g.drawRoundedRectangle (p.reduced (0.5f), 7.0f, active ? 1.6f : 1.0f);

        auto txt = p.reduced (8.0f, 5.0f);
        g.setColour (Ui::dim);
        g.setFont (Ui::font (7.5f, true, 0.2f));
        g.drawText (juce::String (i + 1), txt.removeFromTop (10.0f), juce::Justification::topLeft);
        g.setColour (juce::Colours::white);
        g.setFont (Ui::font (nameSize, true));
        g.drawFittedText (s.padNames[(size_t) i], txt.removeFromTop (nameSize + 4.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
        g.setColour (Ui::aqua);
        g.setFont (Ui::font (9.5f, true));
        g.drawText (s.padDegrees[(size_t) i], txt.removeFromTop (12.0f), juce::Justification::centredLeft);
    }
}

/** Barre de presets (même dessin que Flow Filter / Undertow), en image fixe. */
inline void paintPresetBar (juce::Graphics& g, juce::Rectangle<float> r, const Scene& s)
{
    auto pill = [&g] (juce::Rectangle<float> f, bool accent)
    {
        g.setColour (juce::Colours::white.withAlpha (0.045f));
        g.fillRoundedRectangle (f, f.getHeight() * 0.5f);
        g.setColour (accent ? Ui::aqua.withAlpha (0.5f) : juce::Colours::white.withAlpha (0.1f));
        g.drawRoundedRectangle (f, f.getHeight() * 0.5f, 1.0f);
    };
    auto prev = r.removeFromLeft (r.getHeight());
    auto dice = r.removeFromRight (r.getHeight() + 4.0f);
    r.removeFromRight (6.0f);
    auto save = r.removeFromRight (50.0f);
    r.removeFromRight (8.0f);
    auto next = r.removeFromRight (r.getHeight());
    pill (prev, false);
    pill (next, false);
    for (auto [b, dir] : { std::pair { prev, -1.0f }, std::pair { next, 1.0f } })
    {
        juce::Path ch;
        const auto c = b.getCentre();
        ch.startNewSubPath (c.x - 1.5f * dir, c.y - 4.0f);
        ch.lineTo (c.x + 2.0f * dir, c.y);
        ch.lineTo (c.x - 1.5f * dir, c.y + 4.0f);
        g.setColour (Ui::aqua);
        g.strokePath (ch, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    auto name = r.reduced (4.0f, 0.0f);
    g.setColour (juce::Colour (0xaa020a0e));
    g.fillRoundedRectangle (name, 5.0f);
    g.setColour (juce::Colours::white.withAlpha (0.1f));
    g.drawRoundedRectangle (name.reduced (0.5f), 5.0f, 1.0f);
    auto inner = name.reduced (8.0f, 5.0f);
    const auto tagFont = Ui::font (8.0f, true, 0.2f);
    juce::GlyphArrangement ga;
    ga.addLineOfText (tagFont, s.styleLabel, 0.0f, 0.0f);
    auto tag = inner.removeFromLeft (ga.getBoundingBox (0, -1, true).getWidth() + 14.0f);
    g.setColour (Ui::coral.withAlpha (0.15f));
    g.fillRoundedRectangle (tag, tag.getHeight() * 0.5f);
    g.setColour (Ui::coral);
    g.setFont (tagFont);
    g.drawText (s.styleLabel, tag, juce::Justification::centred);
    g.setColour (juce::Colours::white);
    g.setFont (Ui::font (11.5f, true, 0.03f));
    g.drawText (s.presetName, inner, juce::Justification::centred);
    pill (save, true);
    g.setColour (Ui::aqua);
    g.setFont (Ui::font (9.5f, true, 0.15f));
    g.drawText ("SAVE", save, juce::Justification::centred);
    const float d = dice.getHeight() - 8.0f;
    auto body = juce::Rectangle<float> (d, d).withCentre (dice.getCentre());
    g.setColour (Ui::coral.withAlpha (0.18f));
    g.fillRoundedRectangle (body, 4.0f);
    g.setColour (Ui::coral.withAlpha (0.8f));
    g.drawRoundedRectangle (body, 4.0f, 1.3f);
    g.setColour (juce::Colours::white);
    for (auto [px, py] : { std::pair { -1, -1 }, std::pair { 1, -1 }, std::pair { 0, 0 }, std::pair { -1, 1 }, std::pair { 1, 1 } })
        g.fillEllipse (juce::Rectangle<float> (2.8f, 2.8f).withCentre ({ body.getCentreX() + (float) px * d * 0.26f, body.getCentreY() + (float) py * d * 0.26f }));
}

inline void paintLogo (juce::Graphics& g, float x, float y, float t)
{
    juce::Path logo;
    for (int i = 0; i <= 24; ++i)
    {
        const float xx = x + (float) i, yy = y + 3.5f * std::sin ((float) i * 0.42f + t * 2.0f);
        if (i == 0) logo.startNewSubPath (xx, yy); else logo.lineTo (xx, yy);
    }
    g.setColour (Ui::aqua);
    g.strokePath (logo, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colours::white);
    g.setFont (Ui::font (18.0f, true, -0.02f));
    g.drawText ("conflu", (int) x + 32, (int) y - 12, 70, 24, juce::Justification::centredLeft);
    g.setColour (Ui::aqua);
    g.setFont (Ui::font (9.0f, true, 0.45f));
    g.drawText ("ENCE", (int) x + 88, (int) y - 9, 50, 20, juce::Justification::centredLeft);
}

inline void paintMockLabel (juce::Graphics& g, const juce::String& text, float x, float y)
{
    g.setColour (Ui::coral);
    g.setFont (Ui::font (9.0f, true, 0.4f));
    g.drawText (text, (int) x, (int) y, 220, 20, juce::Justification::centredLeft);
}

//==============================================================================
/** Base commune : les contrôles réels (reliés aux paramètres) des 3 onglets. */
struct MockBase : public juce::Component
{
    explicit MockBase (ConfluenceAudioProcessor& p) : proc (p), scene (makeScene (p))
    {
        setLookAndFeel (&look);
        setOpaque (true);
    }
    ~MockBase() override { setLookAndFeel (nullptr); }

    juce::Component* add (const char* id, const char* name, juce::Rectangle<int> r)
    {
        auto* prm = proc.apvts.getParameter (id);
        juce::Component* c = nullptr;
        if (dynamic_cast<juce::AudioParameterChoice*> (prm) != nullptr)
        {
            c = owned.add (new ChoiceControl (proc.apvts, id, u8 (name)));
            r = r.withHeight (38).withY (r.getY() + 16);
        }
        else if (dynamic_cast<juce::AudioParameterBool*> (prm) != nullptr)
        {
            c = owned.add (new PillToggle (proc.apvts, id, Ui::upper (u8 (name)), PillToggle::Icon::wave));
            r = r.withSizeKeepingCentre (r.getWidth() + 6, 26);
        }
        else
        {
            const auto range = prm->getNormalisableRange();
            c = owned.add (new Knob (proc.apvts, id, u8 (name), range.start < 0.0f && range.end > 0.0f));
        }
        addAndMakeVisible (c);
        c->setBounds (r);
        return c;
    }

    void paintBackground (juce::Graphics& g)
    {
        Ui::drawWaterBackground (g, getLocalBounds().toFloat());
    }

    ConfluenceAudioProcessor& proc;
    Scene scene;
    LiquidLook look;
    juce::OwnedArray<juce::Component> owned;
    TabBar tabs { { "CHORD", "VOICING", "PLAY" } };
    float t = 2.7f;
};

//==============================================================================
/**
    A « Lagon » : la disposition d'Undertow.
    Haut : écran de l'accord (bulles qui montent du clavier, nom en grand, petit tourbillon) | onglets CHORD / VOICING / PLAY.
    Bas : clavier sous l'eau (3 octaves) + 8 pads en 4 x 2.
*/
struct LayoutA : public MockBase
{
    explicit LayoutA (ConfluenceAudioProcessor& p) : MockBase (p)
    {
        setSize (900, 520);
        addAndMakeVisible (tabs);
        tabs.setBounds (544, 50, 340, 26);
        constexpr int x0 = 548, y0 = 86, cw = 84, ch = 92;
        auto cell = [&] (int col, int row) { return juce::Rectangle<int> (x0 + col * cw, y0 + row * ch, cw - 4, ch - 10); };
        add (ParamIDs::mode, "Mode", cell (0, 0));
        add (ParamIDs::root, "Tonique", cell (1, 0));
        add (ParamIDs::scale, "Gamme", cell (2, 0).withWidth (2 * cw - 4));
        add (ParamIDs::chordType, "Accord", cell (0, 1));
        add (ParamIDs::inversion, "Renversement", cell (1, 1));
        add (ParamIDs::strum, "Strum", cell (2, 1));
        add (ParamIDs::latch, "Latch", cell (3, 1));
    }

    void paint (juce::Graphics& g) override
    {
        paintBackground (g);
        Ui::drawGlassPanel (g, { 538.0f, 44.0f, 352.0f, 236.0f });
        Ui::drawGlassPanel (g, { 10.0f, 290.0f, 880.0f, 220.0f });
        paintLogo (g, 20.0f, 22.0f, t);
        paintMockLabel (g, "MAQUETTE A", 160.0f, 12.0f);
        paintPresetBar (g, { 270.0f, 9.0f, 410.0f, 26.0f }, scene);
        g.setColour (Ui::dim.withAlpha (0.7f));
        g.setFont (Ui::font (8.5f, false, 0.05f));
        g.drawText ("v1.0.2", 820, 13, 66, 20, juce::Justification::centredRight);

        // écran de l'accord : la mer au-dessus du clavier, sur la même largeur
        const auto screen = juce::Rectangle<float> (10.0f, 44.0f, 520.0f, 236.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0xd0061a22), 0.0f, screen.getY(), juce::Colour (0xf0010608), 0.0f, screen.getBottom(), false));
        g.fillRoundedRectangle (screen, 10.0f);
        paintWhirlpool (g, { 404.0f, 50.0f, 122.0f, 122.0f }, scene, t, false);
        g.setColour (Ui::dim);
        g.setFont (Ui::font (8.5f, true, 0.2f));
        g.drawText (u8 ("APR\xc3\x88S : ") + suggestionText(), 22, 58, 220, 14, juce::Justification::centredLeft);

        KeyboardGeom kg;
        kg.r = { 24.0f, 330.0f, 492.0f, 166.0f };
        kg.low = 36; kg.high = 83;
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (juce::Rectangle<int> (10, 160, 520, 162));
            paintBubbles (g, kg, scene, 170.0f, t);
        }
        paintChordTitle (g, { 30.0f, 92.0f, 360.0f, 74.0f }, scene, 50.0f);
        paintUnderwaterKeyboard (g, kg, scene, t);
        g.setColour (Ui::dim);
        g.setFont (Ui::font (8.5f, true, 0.2f));
        g.drawText ("PADS  (C1 - G1)", 546, 298, 200, 14, juce::Justification::centredLeft);
        paintPads (g, { 546.0f, 318.0f, 332.0f, 180.0f }, 4, 2, scene, t);
    }

    juce::String suggestionText() const
    {
        juce::StringArray a;
        for (int d : scene.next)
            if (d >= 0)
            {
                auto shape = buildScaleChord (scene.scale, d, ctSeventh);
                uint16_t m = 0;
                for (int i = 0; i < shape.count; ++i) m = (uint16_t) (m | (1u << (unsigned) mod12 (shape.offsets[i])));
                char buf[16];
                romanNumeral (scene.scale, scene.rootPc, mod12 (scene.rootPc + degreeInterval (scene.scale, d)), m, buf, 16);
                a.add (u8 (buf));
            }
        return a.joinIntoString ("  ");
    }
};

//==============================================================================
/**
    B « Tourbillon » : le cercle des quintes au centre de tout.
    Gauche : mode, gamme, gammes du style, Learn / Analyser. Centre : tourbillon avec l'accord dans l'œil.
    Droite : onglets CHORD / VOICING / PLAY. Bas : rangée de 8 pads, puis le clavier sur toute la largeur.
*/
struct LayoutB : public MockBase
{
    explicit LayoutB (ConfluenceAudioProcessor& p) : MockBase (p)
    {
        setSize (900, 560);
        add (ParamIDs::mode, "Mode", { 18, 50, 120, 60 });
        add (ParamIDs::root, "Tonique", { 140, 50, 70, 60 });
        add (ParamIDs::scale, "Gamme", { 18, 100, 192, 60 });
        add (ParamIDs::outOfScale, "Hors gamme", { 18, 150, 192, 60 });
        addAndMakeVisible (tabs);
        tabs.setBounds (626, 50, 262, 26);
        constexpr int x0 = 630, y0 = 86;
        auto cell = [&] (int col, int row) { return juce::Rectangle<int> (x0 + col * 86, y0 + row * 86, 82, 78); };
        add (ParamIDs::chordType, "Accord", cell (0, 0));
        add (ParamIDs::inversion, "Renversement", cell (1, 0));
        add (ParamIDs::spread, "\xc3\x89" "cartement", cell (2, 0));
        add (ParamIDs::maxVoices, "Voix", cell (0, 1));
        add (ParamIDs::strum, "Strum", cell (1, 1));
        add (ParamIDs::octave, "Octave", cell (2, 1));
    }

    void paint (juce::Graphics& g) override
    {
        paintBackground (g);
        Ui::drawGlassPanel (g, { 10.0f, 44.0f, 210.0f, 240.0f });
        Ui::drawGlassPanel (g, { 620.0f, 44.0f, 270.0f, 240.0f });
        Ui::drawGlassPanel (g, { 10.0f, 292.0f, 880.0f, 258.0f });
        paintLogo (g, 20.0f, 22.0f, t);
        paintMockLabel (g, "MAQUETTE B", 160.0f, 12.0f);
        paintPresetBar (g, { 270.0f, 9.0f, 410.0f, 26.0f }, scene);
        g.setColour (Ui::dim.withAlpha (0.7f));
        g.setFont (Ui::font (8.5f, false, 0.05f));
        g.drawText ("v1.0.2", 820, 13, 66, 20, juce::Justification::centredRight);

        // gammes du style
        g.setColour (Ui::dim);
        g.setFont (Ui::font (8.5f, true, 0.2f));
        g.drawText (u8 ("GAMMES CONSEILL\xc3\x89" "ES"), 20, 212, 190, 14, juce::Justification::centredLeft);
        const auto scales = Harmony::styleScales (Harmony::styleDeep);
        for (int i = 0; i < 3; ++i)
        {
            auto pr = juce::Rectangle<float> (18.0f, 230.0f + (float) i * 17.0f, 192.0f, 15.0f);
            g.setColour (i == 1 ? Ui::aqua.withAlpha (0.25f) : juce::Colours::white.withAlpha (0.04f));
            g.fillRoundedRectangle (pr, 7.5f);
            g.setColour (i == 1 ? juce::Colours::white : Ui::text.withAlpha (0.8f));
            g.setFont (Ui::font (8.5f, true));
            g.drawText (u8 (scaleInfo (scales[(size_t) i]).name) + u8 (" \xc2\xb7 ") + u8 (scaleInfo (scales[(size_t) i]).mood),
                        pr.reduced (8.0f, 0.0f), juce::Justification::centredLeft);
        }

        // tourbillon au centre, l'accord dans l'œil
        const auto wr = juce::Rectangle<float> (230.0f, 40.0f, 380.0f, 250.0f);
        paintWhirlpool (g, wr.withSizeKeepingCentre (250.0f, 250.0f), scene, t, true);
        g.setColour (juce::Colour (0xe0020a0e));
        g.fillEllipse (juce::Rectangle<float> (128.0f, 128.0f).withCentre (wr.getCentre()));
        g.setColour (Ui::aqua.withAlpha (0.3f));
        g.drawEllipse (juce::Rectangle<float> (128.0f, 128.0f).withCentre (wr.getCentre()), 1.2f);
        paintChordTitle (g, juce::Rectangle<float> (120.0f, 50.0f).withCentre (wr.getCentre().translated (-10.0f, -6.0f)), scene, 30.0f);

        // pads en rangée, puis le clavier
        g.setColour (Ui::dim);
        g.setFont (Ui::font (8.5f, true, 0.2f));
        g.drawText ("PADS", 22, 298, 100, 14, juce::Justification::centredLeft);
        paintPads (g, { 22.0f, 314.0f, 856.0f, 58.0f }, 8, 1, scene, t, 12.0f);
        KeyboardGeom kg;
        kg.r = { 22.0f, 404.0f, 856.0f, 134.0f };
        kg.low = 36; kg.high = 84;
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (juce::Rectangle<int> (10, 376, 880, 30));
            paintBubbles (g, kg, scene, 376.0f, t);
        }
        paintUnderwaterKeyboard (g, kg, scene, t);
    }
};

//==============================================================================
/**
    C « Courant » : une bande large et basse, comme un device d'Ableton.
    Gauche : mode et gamme. Centre : clavier sous l'eau, les bulles montent vers le nom de l'accord qui flotte en surface.
    Droite : 8 pads en 2 x 4. Rangée du haut : réglages de l'onglet choisi, sur une ligne.
*/
struct LayoutC : public MockBase
{
    explicit LayoutC (ConfluenceAudioProcessor& p) : MockBase (p)
    {
        setSize (1000, 400);
        addAndMakeVisible (tabs);
        tabs.setBounds (196, 46, 300, 24);
        add (ParamIDs::mode, "Mode", { 18, 44, 166, 60 });
        add (ParamIDs::root, "Tonique", { 18, 94, 70, 60 });
        add (ParamIDs::scale, "Gamme", { 90, 94, 94, 60 });
        add (ParamIDs::style, "Style", { 18, 144, 166, 60 });
        constexpr int x0 = 506, y0 = 44;
        int x = x0;
        for (auto [pid, name] : { std::pair { ParamIDs::chordType, "Accord" }, std::pair { ParamIDs::inversion, "Renv." },
                                  std::pair { ParamIDs::spread, "\xc3\x89" "cart." }, std::pair { ParamIDs::maxVoices, "Voix" },
                                  std::pair { ParamIDs::bass, "Basse" }, std::pair { ParamIDs::strum, "Strum" } })
        {
            add (pid, name, { x, y0, 78, 66 });
            x += 80;
        }
    }

    void paint (juce::Graphics& g) override
    {
        paintBackground (g);
        Ui::drawGlassPanel (g, { 10.0f, 40.0f, 180.0f, 350.0f });
        Ui::drawGlassPanel (g, { 196.0f, 40.0f, 794.0f, 74.0f });
        Ui::drawGlassPanel (g, { 760.0f, 120.0f, 230.0f, 270.0f });
        paintLogo (g, 20.0f, 20.0f, t);
        paintMockLabel (g, "MAQUETTE C", 160.0f, 10.0f);
        paintPresetBar (g, { 330.0f, 8.0f, 410.0f, 24.0f }, scene);
        g.setColour (Ui::dim.withAlpha (0.7f));
        g.setFont (Ui::font (8.5f, false, 0.05f));
        g.drawText ("v1.0.2", 920, 11, 66, 20, juce::Justification::centredRight);

        // petit tourbillon sous les réglages de gamme
        paintWhirlpool (g, { 22.0f, 210.0f, 156.0f, 156.0f }, scene, t, false);
        g.setColour (Ui::dim);
        g.setFont (Ui::font (8.0f, true, 0.2f));
        g.drawText (u8 ("ENCHA\xc3\x8eNEMENTS"), 22, 368, 156, 14, juce::Justification::centred);

        // le nom de l'accord flotte à la surface, le clavier est au fond
        const auto sea = juce::Rectangle<float> (196.0f, 120.0f, 558.0f, 270.0f);
        g.setGradientFill (juce::ColourGradient (juce::Colour (0x40061a22), 0.0f, sea.getY(), juce::Colour (0xd0010608), 0.0f, sea.getBottom(), false));
        g.fillRoundedRectangle (sea, 10.0f);
        KeyboardGeom kg;
        kg.r = { 210.0f, 262.0f, 530.0f, 116.0f };
        kg.low = 36; kg.high = 84;
        {
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (sea.toNearestInt());
            paintBubbles (g, kg, scene, 214.0f, t);
        }
        paintChordTitle (g, { 300.0f, 136.0f, 350.0f, 60.0f }, scene, 42.0f);
        paintUnderwaterKeyboard (g, kg, scene, t);

        g.setColour (Ui::dim);
        g.setFont (Ui::font (8.5f, true, 0.2f));
        g.drawText ("PADS", 772, 126, 100, 14, juce::Justification::centredLeft);
        paintPads (g, { 772.0f, 144.0f, 206.0f, 236.0f }, 2, 4, scene, t, 12.0f);
    }
};
} // namespace Mock
