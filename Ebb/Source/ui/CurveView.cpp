// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "CurveView.h"
#include "../PluginProcessor.h"

using namespace Ebb;

namespace { juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); } }

CurveView::CurveView (EbbAudioProcessor& p) : processor (p)
{
    setOpaque (false);
    effTable.resize ((size_t) CurveTables::size + 1);
    effTmp.resize ((size_t) CurveTables::size + 1);
    hintText = u8 ("clic : ajouter un point  \xc2\xb7  glisser : d\xc3\xa9placer  \xc2\xb7  poign\xc3\xa9" "e du milieu : courbure  \xc2\xb7  double-clic : supprimer  \xc2\xb7  Alt : sans aimant");
}

void CurveView::setFrozen (int c, float a, float ph, float g)
{
    frozen = true;
    curve = c;
    custom = false;
    amount = a;
    tension = smooth = 0.0f;
    frozenPhase = ph;
    frozenGain = g;
    phase = ph;
    gain = shownGain = g;
    profileDirty = true;
    repaint();
}

void CurveView::setEditMode (bool on)
{
    if (on == editMode)
        return;
    editMode = on;
    hover = {};
    setMouseCursor (on ? juce::MouseCursor::CrosshairCursor : juce::MouseCursor::NormalCursor);
    profileDirty = underDirty = true;
    repaint();
}

//==============================================================================
juce::Rectangle<float> CurveView::plotArea() const
{
    return getLocalBounds().toFloat().reduced (14.0f, 16.0f).withTrimmedTop (6.0f);
}

juce::Point<float> CurveView::shapeToScreen (float x, float level) const
{
    const auto plot = plotArea();
    return { plot.getX() + x * plot.getWidth(), plot.getBottom() - level * plot.getHeight() };
}

juce::Point<float> CurveView::screenToShape (juce::Point<float> p) const
{
    const auto plot = plotArea();
    return { ShapeMath::clamp01 ((p.x - plot.getX()) / plot.getWidth()), ShapeMath::clamp01 ((plot.getBottom() - p.y) / plot.getHeight()) };
}

float CurveView::snapX (float x, bool free) const
{
    const int g = processor.getGrid();
    if (free || g <= 0) return x;
    return std::round (x * (float) g) / (float) g;
}

float CurveView::snapY (float y, bool free) const
{
    if (free || processor.getGrid() <= 0) return y;
    return std::round (y * 16.0f) / 16.0f;
}

//==============================================================================
bool CurveView::tick()
{
    if (frozen)
        return false;

    const auto s = processor.readSettings();
    const uint32_t sv = processor.getShapeVersion();
    const int g = processor.getGrid();
    if (s.curve != curve || s.custom != custom || std::abs (s.amount - amount) > 0.0005f || s.rate != rate
        || std::abs (s.tension - tension) > 0.0005f || std::abs (s.smooth - smooth) > 0.0005f || sv != shapeVersion)
    {
        curve = s.curve;
        custom = s.custom;
        amount = s.amount;
        rate = s.rate;
        tension = s.tension;
        smooth = s.smooth;
        shapeVersion = sv;
        profileDirty = true;
    }
    if (g != grid)
    {
        grid = g;
        underDirty = true;
    }

    const float newPhase = processor.getPhase();
    const float newGain = processor.getGain();
    const uint32_t trig = processor.getTriggerCount();
    if (trig != lastTriggers)
    {
        lastTriggers = trig;
        ripple = 1.0f;
    }
    ripple = ripple > 0.02f ? ripple * 0.86f : 0.0f;

    shownGain += (newGain - shownGain) * 0.5f;
    const bool moving = profileDirty || underDirty || std::abs (newPhase - phase) > 1.0e-5f || std::abs (shownGain - newGain) > 0.002f
                     || ripple > 0.0f || std::abs (newGain - gain) > 1.0e-5f;
    phase = newPhase;
    gain = newGain;

    if (moving || isMouseOver())
        repaint();
    return moving;
}

//==============================================================================
void CurveView::refreshEffective()
{
    // la même chaîne que le moteur : courbe d'usine ou perso -> lissage -> tension -> Amount
    const auto& tables = CurveTables::get();
    const float* base = (custom && ! frozen) ? processor.getShapeTable() : tables.table (curve);
    const int h = ShapeMath::smoothRadius (smooth, CurveTables::size);
    const float* eff = base;
    if (h > 0)
    {
        ShapeMath::smoothCircular (base, effTable.data(), effTmp.data(), CurveTables::size, h);
        eff = effTable.data();
    }
    const float k = ShapeMath::tensionK (tension);
    const bool useTension = tension > 1.0e-6f || tension < -1.0e-6f;
    const auto plot = plotArea();
    for (int i = 0; i <= 256; ++i)
    {
        const float x01 = (float) i / 256.0f;
        const float local = x01 * (float) cycles - std::floor (x01 * (float) cycles - 1.0e-6f);
        float r = CurveTables::lookupTable (eff, i == 256 ? 1.0f : ShapeMath::clamp01 (local));
        if (useTension) r = ShapeMath::applyTension (r, k);
        const float level = 1.0f - juce::jmax (0.0f, amount) * r;
        effY[(size_t) i] = plot.getBottom() - level * plot.getHeight();
    }
}

void CurveView::rebuildProfile()
{
    refreshEffective();
    const auto plot = plotArea();
    profile.clear();
    for (int i = 0; i <= 256; ++i)
    {
        const float x = plot.getX() + plot.getWidth() * (float) i / 256.0f;
        if (i == 0) profile.startNewSubPath (x, effY[0]);
        else        profile.lineTo (x, effY[(size_t) i]);
    }
    profileFill = profile;
    profileFill.lineTo (plot.getRight(), plot.getBottom());
    profileFill.lineTo (plot.getX(), plot.getBottom());
    profileFill.closeSubPath();

    // la courbe perso à pleine échelle (édition) : échantillonnée finement, marches comprises
    shapePath.clear();
    if (editMode)
    {
        const auto& s = processor.getShape();
        for (int i = 0; i < s.count; ++i)
        {
            const auto& a = s.pts[(size_t) i];
            const auto pa = shapeToScreen (a.x, a.y);
            if (i == 0) shapePath.startNewSubPath (pa);
            else        shapePath.lineTo (pa);
            if (i + 1 < s.count)
            {
                const auto& b = s.pts[(size_t) i + 1];
                const int steps = juce::jlimit (1, 64, (int) ((b.x - a.x) * plot.getWidth() / 3.0f));
                for (int k = 1; k < steps; ++k)
                {
                    const float t = (float) k / (float) steps;
                    shapePath.lineTo (shapeToScreen (a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * ShapeMath::bend (t, a.curve)));
                }
            }
        }
        shapeFill = shapePath;
        shapeFill.lineTo (plot.getRight(), plot.getBottom());
        shapeFill.lineTo (plot.getX(), plot.getBottom());
        shapeFill.closeSubPath();
    }
    profileDirty = false;
    overDirty = true;
}

juce::Point<float> CurveView::pointAt (float ph) const
{
    const auto plot = plotArea();
    const int cycleIdx = cycles > 1 ? (int) (lastTriggers % (uint32_t) cycles) : 0;
    const float x01 = ((float) cycleIdx + juce::jlimit (0.0f, 1.0f, ph)) / (float) cycles;
    const float idx = x01 * 256.0f;
    const int i = juce::jlimit (0, 255, (int) idx);
    const float f = idx - (float) i;
    const float y = effY[(size_t) i] + f * (effY[(size_t) i + 1] - effY[(size_t) i]);
    return { plot.getX() + x01 * plot.getWidth(), y };
}

//==============================================================================
void CurveView::renderUnder (float scale)
{
    const int w = juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)), h = juce::jmax (1, juce::roundToInt ((float) getHeight() * scale));
    if (! underImg.isValid() || underImg.getWidth() != w || underImg.getHeight() != h)
        underImg = juce::Image (juce::Image::ARGB, w, h, true);
    else
        underImg.clear (underImg.getBounds());

    juce::Graphics g (underImg);
    g.addTransform (juce::AffineTransform::scale (scale));
    const auto b = getLocalBounds().toFloat();
    const auto plot = plotArea();

    Ui::setVerticalGradient (g, 7, juce::Colour (0x60061a22), b.getY(), juce::Colour (0xe0010608), b.getBottom());
    g.fillRoundedRectangle (b, 10.0f);
    g.setColour (editMode ? Ui::aqua.withAlpha (0.35f) : juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (b.reduced (0.5f), 10.0f, 1.0f);

    // repères : grille de l'éditeur (ou quarts de la courbe) et niveaux
    const int divisions = editMode && processor.getGrid() > 0 ? processor.getGrid() : 4;
    for (int c = 0; c < cycles; ++c)
        for (int k = 1; k < divisions; ++k)
        {
            const bool quarter = (k * 4) % divisions == 0;
            g.setColour (juce::Colours::white.withAlpha (quarter ? 0.06f : 0.03f));
            g.drawVerticalLine (juce::roundToInt (plot.getX() + plot.getWidth() * ((float) c + (float) k / (float) divisions) / (float) cycles),
                                plot.getY(), plot.getBottom());
        }
    const int levels = editMode && processor.getGrid() > 0 ? 8 : 2;
    for (int k = 0; k <= levels; ++k)
    {
        g.setColour (juce::Colours::white.withAlpha (k == 0 || k == levels || k * 2 == levels ? 0.06f : 0.025f));
        g.drawHorizontalLine (juce::roundToInt (plot.getY() + plot.getHeight() * (float) k / (float) levels), plot.getX(), plot.getRight());
    }
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    for (int k = 1; k < cycles; ++k)
        g.drawVerticalLine (juce::roundToInt (plot.getX() + plot.getWidth() * (float) k / (float) cycles), plot.getY() - 6.0f, plot.getBottom());

    underScale = scale;
    underDirty = false;
}

void CurveView::renderOver (float scale)
{
    const int w = juce::jmax (1, juce::roundToInt ((float) getWidth() * scale)), h = juce::jmax (1, juce::roundToInt ((float) getHeight() * scale));
    if (! overImg.isValid() || overImg.getWidth() != w || overImg.getHeight() != h)
        overImg = juce::Image (juce::Image::ARGB, w, h, true);
    else
        overImg.clear (overImg.getBounds());

    juce::Graphics g (overImg);
    g.addTransform (juce::AffineTransform::scale (scale));
    const auto plot = plotArea();
    const juce::PathStrokeType thick (6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    const juce::PathStrokeType line (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    if (! editMode)
    {
        Ui::setVerticalGradient (g, 6, Ui::aqua.withAlpha (0.30f), plot.getY(), Ui::aquaDeep.withAlpha (0.06f), plot.getBottom());
        g.fillPath (profileFill);
        g.setColour (Ui::aqua.withAlpha (0.18f));
        g.strokePath (profile, thick);
        g.setColour (Ui::aqua);
        g.strokePath (profile, line);
    }
    else
    {
        // la courbe perso à pleine échelle
        Ui::setVerticalGradient (g, 6, Ui::aqua.withAlpha (0.22f), plot.getY(), Ui::aquaDeep.withAlpha (0.04f), plot.getBottom());
        g.fillPath (shapeFill);
        g.setColour (Ui::aqua.withAlpha (0.16f));
        g.strokePath (shapePath, thick);
        g.setColour (Ui::aqua);
        g.strokePath (shapePath, line);

        // ce qu'on entend (Amount, Tension, Lissage)
        g.setColour (juce::Colours::white.withAlpha (0.45f));
        g.strokePath (profile, juce::PathStrokeType (1.1f));

        // poignées de courbure (milieu de chaque segment assez large)
        const auto& s = processor.getShape();
        for (int i = 0; i + 1 < s.count; ++i)
        {
            const auto& a = s.pts[(size_t) i];
            const auto& b = s.pts[(size_t) i + 1];
            if ((b.x - a.x) * plot.getWidth() < 14.0f || std::abs (b.y - a.y) < 0.004f) continue;
            const auto c = shapeToScreen ((a.x + b.x) * 0.5f, a.y + (b.y - a.y) * ShapeMath::bend (0.5f, a.curve));
            const bool hot = (hover.hit == Hit::handle && hover.index == i) || (drag.hit == Hit::handle && drag.index == i);
            auto& d = Ui::scratchPath (2);
            const float r = hot ? 5.5f : 4.0f;
            d.startNewSubPath (c.x, c.y - r);
            d.lineTo (c.x + r, c.y);
            d.lineTo (c.x, c.y + r);
            d.lineTo (c.x - r, c.y);
            d.closeSubPath();
            g.setColour (Ui::ink.withAlpha (0.9f));
            g.fillPath (d);
            g.setColour (hot ? Ui::coral : Ui::aqua.withAlpha (0.7f));
            g.strokePath (d, juce::PathStrokeType (1.2f));
        }

        // points
        for (int i = 0; i < s.count; ++i)
        {
            const auto& p = s.pts[(size_t) i];
            const auto c = shapeToScreen (p.x, p.y);
            const bool hot = (hover.hit == Hit::point && hover.index == i) || (drag.hit == Hit::point && drag.index == i);
            const bool edge = i == 0 || i == s.count - 1;
            const float r = hot ? 6.5f : 5.0f;
            g.setColour (Ui::aqua.withAlpha (hot ? 0.35f : 0.18f));
            g.fillEllipse (juce::Rectangle<float> (r * 3.0f, r * 3.0f).withCentre (c));
            g.setColour (hot ? juce::Colours::white : Ui::text);
            if (edge) g.fillRect (juce::Rectangle<float> (r * 1.7f, r * 1.7f).withCentre (c));
            else      g.fillEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c));
            g.setColour (Ui::aquaDeep);
            if (edge) g.drawRect (juce::Rectangle<float> (r * 1.7f, r * 1.7f).withCentre (c), 1.2f);
            else      g.drawEllipse (juce::Rectangle<float> (r * 2.0f, r * 2.0f).withCentre (c), 1.2f);
        }

        g.setColour (Ui::dim.withAlpha (0.85f));
        g.setFont (Ui::font (7.6f, true, 0.04f));
        g.drawText (hintText, juce::Rectangle<float> (plot.getX(), plot.getBottom() + 1.0f, plot.getWidth(), 13.0f), juce::Justification::centred, true);
    }

    if (showLabels)
    {
        g.setColour (Ui::dim.withAlpha (0.8f));
        g.setFont (Ui::font (8.0f, true, 0.15f));
        g.drawText (label100, juce::Rectangle<float> (plot.getX() + 4.0f, plot.getY() - 13.0f, 40.0f, 11.0f), juce::Justification::centredLeft, false);
        if (! editMode)
            g.drawText (label0, juce::Rectangle<float> (plot.getX() + 4.0f, plot.getBottom() - 12.0f, 40.0f, 11.0f), juce::Justification::centredLeft, false);
    }
    if (caption.isNotEmpty())
    {
        g.setColour (editMode ? Ui::aqua : Ui::dim.withAlpha (0.9f));
        g.setFont (Ui::font (7.8f, true, 0.15f));
        g.drawText (caption, juce::Rectangle<float> (plot.getX(), plot.getY() - 14.0f, plot.getWidth() - 4.0f, 12.0f), juce::Justification::centredRight, true);
    }

    overScale = scale;
    overDirty = false;
}

void CurveView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto plot = plotArea();
    if (profileDirty)
        rebuildProfile();

    const float scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (underDirty || ! underImg.isValid() || std::abs (scale - underScale) > 0.01f) renderUnder (scale);
    if (overDirty || ! overImg.isValid() || std::abs (scale - overScale) > 0.01f)    renderOver (scale);

    g.drawImageTransformed (underImg, juce::AffineTransform::scale (1.0f / underScale));

    // --- niveau réel de l'eau : suit le gain appliqué au son
    {
        // découpe rectangulaire (bien moins chère qu'un chemin arrondi) : à 4 px du bord, on reste dans les coins arrondis
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (b.reduced (4.0f).toNearestInt());
        const auto water = juce::Rectangle<float> (b.getX(), plot.getY(), b.getWidth(), b.getBottom() - plot.getY());
        const float level = juce::jlimit (0.0f, 1.0f, (shownGain * plot.getHeight() + (b.getBottom() - plot.getBottom())) / water.getHeight());
        Ui::drawLiquid (g, water, level, Ui::now(), 3.0f, Ui::aquaDeep.withAlpha (editMode ? 0.18f : 0.30f), Ui::aquaDeep.withAlpha (0.05f), 2.0f);
    }

    g.drawImageTransformed (overImg, juce::AffineTransform::scale (1.0f / overScale));

    // --- rond dans l'eau au départ de la courbe
    const int cycleIdx = cycles > 1 ? (int) (lastTriggers % (uint32_t) cycles) : 0;
    if (ripple > 0.0f)
    {
        const float x0 = plot.getX() + plot.getWidth() * (float) cycleIdx / (float) cycles;
        const float rr = 6.0f + (1.0f - ripple) * 34.0f;
        g.setColour (Ui::aqua.withAlpha (0.55f * ripple));
        g.drawEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 0.7f).withCentre ({ x0, plot.getY() }), 1.4f);
    }

    // --- la goutte qui parcourt la courbe (celle qu'on entend)
    const float ph = phase >= 0.0f ? phase : 1.0f;
    const auto pt = pointAt (ph);
    const float s = editMode ? 5.5f : 7.0f;
    g.setColour (Ui::aqua.withAlpha (0.25f));
    g.fillEllipse (juce::Rectangle<float> (s * 3.4f, s * 3.4f).withCentre (pt));
    auto& drop = Ui::scratchPath (5);
    drop.startNewSubPath (pt.x, pt.y - s * 1.9f);
    drop.cubicTo (pt.x + s * 0.25f, pt.y - s * 1.1f, pt.x + s, pt.y - s * 0.45f, pt.x + s, pt.y + s * 0.15f);
    drop.cubicTo (pt.x + s, pt.y + s * 0.75f, pt.x + s * 0.5f, pt.y + s, pt.x, pt.y + s);
    drop.cubicTo (pt.x - s * 0.5f, pt.y + s, pt.x - s, pt.y + s * 0.75f, pt.x - s, pt.y + s * 0.15f);
    drop.cubicTo (pt.x - s, pt.y - s * 0.45f, pt.x - s * 0.25f, pt.y - s * 1.1f, pt.x, pt.y - s * 1.9f);
    drop.closeSubPath();
    Ui::setVerticalGradient (g, 5, juce::Colours::white, pt.y - s * 1.9f, Ui::aqua, pt.y + s);
    g.fillPath (drop);
    g.setColour (juce::Colours::white.withAlpha (0.8f));
    g.fillEllipse (pt.x - s * 0.45f, pt.y - s * 0.2f, s * 0.35f, s * 0.35f);
}

//==============================================================================
// Édition
//==============================================================================
CurveView::Target CurveView::findTarget (juce::Point<float> p) const
{
    if (! editMode) return {};
    const auto& s = processor.getShape();
    const auto plot = plotArea();
    // points d'abord (le plus proche dans un rayon de 9 px)
    Target best;
    float bestD = 9.0f;
    for (int i = 0; i < s.count; ++i)
    {
        const float d = p.getDistanceFrom (shapeToScreen (s.pts[(size_t) i].x, s.pts[(size_t) i].y));
        if (d < bestD) { bestD = d; best = { Hit::point, i }; }
    }
    if (best.hit != Hit::none) return best;
    bestD = 8.0f;
    for (int i = 0; i + 1 < s.count; ++i)
    {
        const auto& a = s.pts[(size_t) i];
        const auto& b = s.pts[(size_t) i + 1];
        if ((b.x - a.x) * plot.getWidth() < 14.0f || std::abs (b.y - a.y) < 0.004f) continue;
        const float d = p.getDistanceFrom (shapeToScreen ((a.x + b.x) * 0.5f, a.y + (b.y - a.y) * ShapeMath::bend (0.5f, a.curve)));
        if (d < bestD) { bestD = d; best = { Hit::handle, i }; }
    }
    return best;
}

void CurveView::applyEdit (const ShapeData& s)
{
    processor.setShape (s, true, true);
    profileDirty = true;
    repaint();
}

void CurveView::mouseDown (const juce::MouseEvent& e)
{
    if (! editMode || frozen) return;
    const bool free = e.mods.isAltDown() || e.mods.isCommandDown();
    auto s = processor.getShape();
    auto t = findTarget (e.position);

    if (e.mods.isPopupMenu())
    {
        // clic droit sur un point : on le supprime (pas les points du bord)
        if (t.hit == Hit::point && t.index > 0 && t.index < s.count - 1)
        {
            for (int i = t.index; i + 1 < s.count; ++i) s.pts[(size_t) i] = s.pts[(size_t) i + 1];
            --s.count;
            applyEdit (s);
        }
        return;
    }

    if (t.hit == Hit::none)
    {
        if (! plotArea().expanded (4.0f).contains (e.position) || s.count >= ShapeData::maxPoints)
            return;
        // nouveau point, inséré à sa place ; il garde la courbure du segment qu'il coupe
        const auto q = screenToShape (e.position);
        const float x = juce::jlimit (0.0005f, 0.9995f, snapX (q.x, free));
        const float y = snapY (q.y, free);
        int at = 1;
        while (at < s.count && s.pts[(size_t) at].x <= x) ++at;
        for (int i = s.count; i > at; --i) s.pts[(size_t) i] = s.pts[(size_t) i - 1];
        s.pts[(size_t) at] = { x, y, s.pts[(size_t) at - 1].curve };
        ++s.count;
        processor.beginShapeGesture();
        applyEdit (s);
        t = { Hit::point, at };
    }
    else
        processor.beginShapeGesture();

    drag = t;
    dragging = true;
    dragStart = processor.getShape();
    dragOrigin = e.position;
    dragCurve0 = t.hit == Hit::handle ? dragStart.pts[(size_t) t.index].curve : 0.0f;
    overDirty = true;
    repaint();
}

void CurveView::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging || ! editMode) return;
    const bool free = e.mods.isAltDown() || e.mods.isCommandDown();
    auto s = dragStart;
    if (drag.hit == Hit::point && drag.index >= 0 && drag.index < s.count)
    {
        const auto q = screenToShape (e.position);
        auto& p = s.pts[(size_t) drag.index];
        const bool first = drag.index == 0, last = drag.index == s.count - 1;
        if (! first && ! last)
        {
            const float lo = s.pts[(size_t) drag.index - 1].x, hi = s.pts[(size_t) drag.index + 1].x;
            p.x = juce::jlimit (lo, hi, snapX (q.x, free));
        }
        p.y = snapY (q.y, free);
    }
    else if (drag.hit == Hit::handle && drag.index >= 0 && drag.index + 1 < s.count)
    {
        const auto& a = s.pts[(size_t) drag.index];
        const auto& b = s.pts[(size_t) drag.index + 1];
        const float dir = b.y >= a.y ? 1.0f : -1.0f;      // vers le haut = le segment se bombe vers le haut
        const float dy = (dragOrigin.y - e.position.y) / 90.0f;
        s.pts[(size_t) drag.index].curve = juce::jlimit (-1.0f, 1.0f, dragCurve0 + dir * dy);
    }
    applyEdit (s);
}

void CurveView::mouseUp (const juce::MouseEvent&)
{
    if (dragging)
        processor.endShapeGesture();
    dragging = false;
    drag = {};
    overDirty = true;
    repaint();
}

void CurveView::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (! editMode || frozen) return;
    auto s = processor.getShape();
    const auto t = findTarget (e.position);
    if (t.hit == Hit::point && t.index > 0 && t.index < s.count - 1)
    {
        for (int i = t.index; i + 1 < s.count; ++i) s.pts[(size_t) i] = s.pts[(size_t) i + 1];
        --s.count;
        applyEdit (s);
    }
    else if (t.hit == Hit::handle)
    {
        s.pts[(size_t) t.index].curve = 0.0f;
        applyEdit (s);
    }
}

void CurveView::mouseMove (const juce::MouseEvent& e)
{
    if (! editMode) return;
    const auto t = findTarget (e.position);
    if (t.hit != hover.hit || t.index != hover.index)
    {
        hover = t;
        setMouseCursor (t.hit == Hit::point ? juce::MouseCursor::DraggingHandCursor
                        : t.hit == Hit::handle ? juce::MouseCursor::UpDownResizeCursor : juce::MouseCursor::CrosshairCursor);
        overDirty = true;
        repaint();
    }
}

void CurveView::mouseExit (const juce::MouseEvent&)
{
    if (hover.hit != Hit::none)
    {
        hover = {};
        overDirty = true;
        repaint();
    }
}
