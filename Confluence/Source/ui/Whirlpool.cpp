// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "Whirlpool.h"
#include "../PluginProcessor.h"

bool Whirlpool::tick()
{
    const auto cs = processor.readChordSettings();
    const int st = (int) std::lround (processor.apvts.getRawParameterValue (ParamIDs::style)->load());
    const auto d = processor.getLastChord();

    int cur = -1;
    if (d.count > 0)
        cur = Theory::degreeOfPc (cs.scale, cs.rootPc, d.chordRootPc);
    if (cs.rootPc != root || cs.scale != scale || st != style || cur != current || cs.mode != mode || cs.chordType != type
        || d.counter != lastCounter)
    {
        root = cs.rootPc;
        scale = cs.scale;
        style = st;
        current = cur;
        mode = cs.mode;
        type = cs.chordType;
        lastCounter = d.counter;
        next = { -1, -1, -1 };
        // pas d'enchaînement conseillé en mode Accord fixe (l'accord peut être hors de la gamme)
        if (current >= 0 && mode != Engine::modeFixed)
            next = Harmony::suggestNext (scale, current, style);
        dirty = true;
        return true;
    }
    return false;
}

void Whirlpool::render (float s)
{
    const int w = juce::roundToInt ((float) getWidth() * s), h = juce::roundToInt ((float) getHeight() * s);
    if (! nodes.isValid() || nodes.getWidth() != w || nodes.getHeight() != h)
        nodes = juce::Image (juce::Image::ARGB, w, h, true);
    else
        nodes.clear (nodes.getBounds());

    juce::Graphics g (nodes);
    g.addTransform (juce::AffineTransform::scale (s));
    const auto c = getLocalBounds().toFloat().getCentre();
    const float R = juce::jmin ((float) getWidth(), (float) getHeight()) * 0.5f - 14.0f;
    const auto mask = Theory::scaleMask (scale, root);
    const bool flats = Theory::useFlats (scale, root);
    const int curPc = current >= 0 ? Theory::mod12 (root + Theory::degreeInterval (scale, current)) : -1;
    const int labelType = type == Theory::ctPower ? Theory::ctTriad : Theory::ctSeventh;

    auto angleOfPc = [] (int pc) { for (int k = 0; k < 12; ++k) if (Theory::mod12 (k * 7) == pc) return (float) k * juce::MathConstants<float>::twoPi / 12.0f; return 0.0f; };

    g.setColour (Ui::aqua.withAlpha (0.12f));
    g.drawEllipse (juce::Rectangle<float> (R * 2.0f, R * 2.0f).withCentre (c), 1.0f);

    // courants : de l'accord joué vers les accords conseillés, aspirés par le centre
    for (int j = 2; j >= 0; --j)
    {
        if (next[(size_t) j] < 0 || curPc < 0) continue;
        const int pc = Theory::mod12 (root + Theory::degreeInterval (scale, next[(size_t) j]));
        const auto from = c.getPointOnCircumference (R - 13.0f, angleOfPc (curPc));
        const auto to = c.getPointOnCircumference (R - 13.0f, angleOfPc (pc));
        const auto ctrl = c + ((from + to) * 0.5f - c) * 0.15f;
        juce::Path flow;
        flow.startNewSubPath (from);
        flow.quadraticTo (ctrl, to);
        juce::Path dashed;
        const float dashes[] = { 5.0f, 4.0f };
        juce::PathStrokeType (1.6f - 0.3f * (float) j).createDashedStroke (dashed, flow, dashes, 2);
        g.setColour (Ui::aqua.withAlpha (0.75f - 0.18f * (float) j));
        g.fillPath (dashed);
        g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (to));
    }

    for (int k = 0; k < 12; ++k)
    {
        const int pc = Theory::mod12 (k * 7);
        const float a = (float) k * juce::MathConstants<float>::twoPi / 12.0f;
        const auto p = c.getPointOnCircumference (R, a);
        const bool sc = ((mask >> (unsigned) pc) & 1u) != 0;
        const int deg = Theory::degreeOfPc (scale, root, pc);
        int sugg = -1;
        for (int j = 0; j < 3; ++j) if (next[(size_t) j] >= 0 && deg == next[(size_t) j]) sugg = j;
        const bool isCur = pc == curPc;

        if (sugg >= 0)
        {
            g.setColour (Ui::aqua.withAlpha (0.24f - 0.05f * (float) sugg));
            g.fillEllipse (juce::Rectangle<float> (32.0f, 32.0f).withCentre (p));
        }
        const float d = isCur ? 24.0f : (sc ? 21.0f : 15.0f);
        g.setColour (isCur ? Ui::coral : (sugg >= 0 ? Ui::aqua : (sc ? juce::Colour (0xff1d5966) : juce::Colour (0xff0b1d24))));
        g.fillEllipse (juce::Rectangle<float> (d, d).withCentre (p));
        g.setColour (juce::Colours::white.withAlpha (sc ? 0.35f : 0.08f));
        g.drawEllipse (juce::Rectangle<float> (d, d).withCentre (p), 1.0f);
        g.setColour (sc || isCur ? juce::Colours::white : Ui::dim.withAlpha (0.6f));
        g.setFont (Ui::font (isCur || sugg >= 0 ? 10.0f : 8.5f, true));
        g.drawText (Theory::pcName (pc, flats), juce::Rectangle<float> (d + 10.0f, d).withCentre (p), juce::Justification::centred);

        if ((sugg >= 0 || isCur) && deg >= 0)
        {
            // chiffrage à l'intérieur du cercle
            const auto lp = c.getPointOnCircumference (R - 27.0f, a);
            g.setColour (isCur ? Ui::coral : Ui::aqua);
            g.setFont (Ui::font (9.0f, true));
            g.drawText (ChordText::romanOfDegree (scale, root, deg, labelType), juce::Rectangle<float> (36.0f, 12.0f).withCentre (lp),
                        juce::Justification::centred);
        }
    }
    imgScale = s;
    dirty = false;
}

void Whirlpool::paint (juce::Graphics& g)
{
    const float s = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (dirty || ! nodes.isValid() || std::abs (s - imgScale) > 0.01f)
        render (s);

    const auto c = getLocalBounds().toFloat().getCentre();
    const float R = juce::jmin ((float) getWidth(), (float) getHeight()) * 0.5f - 14.0f;
    const float t = Ui::now();

    // lueur et bras du tourbillon (tournent lentement)
    Ui::setGradient (g, 4, juce::Colour (0xff0a2f3a), c, juce::Colour (0x00020a0e), { c.x, c.y + R * 1.2f }, true);
    g.fillEllipse (juce::Rectangle<float> (R * 2.4f, R * 2.4f).withCentre (c));
    for (int arm = 0; arm < 4; ++arm)
    {
        auto& sp = Ui::scratchPath (6);
        for (int i = 0; i <= 40; ++i)
        {
            const float f = (float) i / 40.0f;
            const float a = (float) arm * juce::MathConstants<float>::halfPi + t * 0.35f + f * 4.2f;
            const auto p = c.getPointOnCircumference (R * (1.02f - 0.88f * f), a);
            if (i == 0) sp.startNewSubPath (p); else sp.lineTo (p);
        }
        g.setColour (Ui::aqua.withAlpha (0.07f));
        g.strokePath (sp, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }
    g.setOpacity (1.0f);   // drawImage prend l'opacité de la dernière couleur (celle des bras, très transparente)
    g.drawImageTransformed (nodes, juce::AffineTransform::scale (1.0f / imgScale));
}
