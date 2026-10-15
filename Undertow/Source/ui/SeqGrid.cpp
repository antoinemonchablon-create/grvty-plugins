// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "SeqGrid.h"
#include "../PluginProcessor.h"

namespace
{
    int stepPitch (const Step& s) { return s.note + 12 * s.octave; }
}

SeqGrid::SeqGrid (UndertowAudioProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip ("Clic : pas on/off. Glisser : hauteur. Maj+clic : accent. Alt+clic : slide. Clic droit : ratchet, probabilite, gate, p-locks. "
                "Rangee du bas : piste Mod.");
    crest.preallocateSpace (3 * 90);
}

int SeqGrid::visibleSteps() const
{
    const int len = (int) processor.apvts.getRawParameterValue (ParamIDs::seqLength)->load();
    const int lane = (int) processor.apvts.getRawParameterValue (ParamIDs::modLaneLength)->load();
    return juce::jmax (len, lane) <= 16 ? 16 : 32;
}

juce::Rectangle<float> SeqGrid::notesArea() const
{
    auto r = getLocalBounds().toFloat().reduced (8.0f, 6.0f);
    r.removeFromTop (8.0f);       // place pour la crête de la vague
    r.removeFromBottom (36.0f);   // numéros des temps + piste Mod
    return r;
}

juce::Rectangle<float> SeqGrid::laneArea() const
{
    auto r = getLocalBounds().toFloat().reduced (8.0f, 6.0f);
    return r.removeFromBottom (22.0f);
}

int SeqGrid::stepAt (float x) const
{
    const auto a = notesArea();
    return juce::jlimit (0, visibleSteps() - 1, (int) ((x - a.getX()) / (a.getWidth() / (float) visibleSteps())));
}

juce::String SeqGrid::playedNoteName (const Step& s) const
{
    auto& st = processor.apvts;
    const int root  = (int) st.getRawParameterValue (ParamIDs::rootNote)->load();
    const int oct   = (int) st.getRawParameterValue (ParamIDs::seqOctave)->load();
    const int scale = (int) st.getRawParameterValue (ParamIDs::scale)->load();
    const int note  = juce::jlimit (0, 127, 48 + root + 12 * (s.octave + oct) + Scales::quantize (s.note, scale));
    return juce::MidiMessage::getMidiNoteName (note, true, true, 3);   // convention d'Ableton : C3 = 60
}

void SeqGrid::publish()
{
    processor.pattern.publish();
    repaint();
}

void SeqGrid::setPitchFromY (int step, float y)
{
    const auto a = notesArea();
    const float norm = juce::jlimit (0.0f, 1.0f, (a.getBottom() - y) / a.getHeight());
    int pitch = pitchLow + juce::roundToInt (norm * (float) pitchRange);
    const int scale = (int) processor.apvts.getRawParameterValue (ParamIDs::scale)->load();
    pitch = Scales::quantize (pitch, scale);   // la grille ne propose que des notes de la gamme
    auto& s = processor.pattern.edit().steps[(size_t) step];
    const int oct = (int) std::floor (pitch / 12.0);
    s.octave = juce::jlimit (-2, 2, oct);
    s.note = pitch - 12 * oct;
    s.on = true;
    hintText = "PAS " + juce::String (step + 1) + "  " + playedNoteName (s);
}

void SeqGrid::setLaneFromY (int step, float y)
{
    const auto a = laneArea();
    float v = juce::jlimit (-1.0f, 1.0f, 1.0f - 2.0f * (y - a.getY()) / a.getHeight());
    if (std::abs (v) < 0.06f) v = 0.0f;   // aimant sur zéro
    processor.pattern.edit().modLane[(size_t) step] = v;
    hintText = "MOD " + juce::String (step + 1) + "  " + (v > 0 ? "+" : "") + juce::String (juce::roundToInt (v * 100.0f)) + " %";
}

void SeqGrid::mouseMove (const juce::MouseEvent& e)
{
    const int s = notesArea().expanded (0.0f, 8.0f).contains (e.position) ? stepAt (e.position.x) : -1;
    if (s != hoverStep)
    {
        hoverStep = s;
        if (s >= 0)
        {
            const auto& st = processor.pattern.get().steps[(size_t) s];
            hintText = "PAS " + juce::String (s + 1) + (st.on ? "  " + playedNoteName (st) : juce::String ("  (off)"));
        }
        else hintText.clear();
        repaint();
    }
}

void SeqGrid::mouseExit (const juce::MouseEvent&)
{
    hoverStep = -1;
    if (drag == Drag::none) hintText.clear();
    repaint();
}

void SeqGrid::mouseDown (const juce::MouseEvent& e)
{
    const int step = stepAt (e.position.x);
    dragMoved = false;

    if (laneArea().expanded (0.0f, 3.0f).contains (e.position))
    {
        drag = Drag::lane;
        setLaneFromY (step, e.position.y);
        publish();
        return;
    }

    if (e.mods.isPopupMenu()) { showStepMenu (step); return; }

    auto& s = processor.pattern.edit().steps[(size_t) step];
    if (e.mods.isShiftDown()) { s.accent = ! s.accent; s.on = true; publish(); return; }
    if (e.mods.isAltDown())   { s.slide = ! s.slide;   s.on = true; publish(); return; }

    drag = Drag::pitch;
    dragStep = step;
}

void SeqGrid::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::lane)
    {
        setLaneFromY (stepAt (e.position.x), e.position.y);
        publish();
    }
    else if (drag == Drag::pitch && e.getDistanceFromDragStart() > 2)
    {
        dragMoved = true;
        setPitchFromY (dragStep, e.position.y);
        publish();
    }
}

void SeqGrid::mouseUp (const juce::MouseEvent&)
{
    if (drag == Drag::pitch && ! dragMoved && dragStep >= 0)
    {
        auto& s = processor.pattern.edit().steps[(size_t) dragStep];
        s.on = ! s.on;
        publish();
    }
    drag = Drag::none;
    dragStep = -1;
}

void SeqGrid::showStepMenu (int step)
{
    const auto s = processor.pattern.get().steps[(size_t) step];
    juce::PopupMenu m, ratchet, prob, gate, vel;

    m.addSectionHeader ("PAS " + juce::String (step + 1) + (s.on ? "  -  " + playedNoteName (s) : juce::String()));
    m.addItem (1, "Accent  (Maj + clic)", true, s.accent);
    m.addItem (2, "Slide vers le pas suivant  (Alt + clic)", true, s.slide);

    for (int r = 1; r <= 4; ++r) ratchet.addItem (10 + r, juce::String (r) + "x", true, s.ratchet == r);
    m.addSubMenu ("Ratchet", ratchet);

    const int probs[] = { 100, 75, 60, 50, 40, 25, 10 };
    for (int i = 0; i < 7; ++i) prob.addItem (20 + i, juce::String (probs[i]) + " %", true, juce::roundToInt (s.probability * 100.0f) == probs[i]);
    m.addSubMenu ("Probabilite", prob);

    const int gates[] = { 10, 25, 50, 75, 100 };
    for (int i = 0; i < 5; ++i) gate.addItem (30 + i, juce::String (gates[i]) + " %", true, juce::roundToInt (s.gate * 100.0f) == gates[i]);
    m.addSubMenu ("Gate", gate);

    const int vels[] = { 100, 80, 60, 45, 30 };
    for (int i = 0; i < 5; ++i) vel.addItem (50 + i, juce::String (vels[i]) + " %", true, juce::roundToInt (s.velocity * 100.0f) == vels[i]);
    m.addSubMenu ("Velocite", vel);

    m.addSeparator();
    auto& st = processor.apvts;
    const float idx = st.getRawParameterValue (ParamIDs::fmIndex)->load();
    const float cut = st.getRawParameterValue (ParamIDs::cutoff)->load();
    m.addItem (40, "P-lock Index = " + juce::String (idx, 2) + "  (valeur actuelle du bouton)");
    m.addItem (41, "Effacer le p-lock Index", s.hasLockIndex());
    m.addItem (42, "P-lock Cutoff = " + juce::String (juce::roundToInt (cut)) + " Hz  (valeur actuelle du bouton)");
    m.addItem (43, "Effacer le p-lock Cutoff", s.hasLockCutoff());
    m.addSeparator();
    m.addItem (60, "Copier ce pas sur tous les pas allumes (sauf la note)");

    juce::Component::SafePointer<SeqGrid> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe, step, idx, cut] (int res)
    {
        if (safe == nullptr || res <= 0) return;
        auto& pat = safe->processor.pattern.edit();
        auto& p = pat.steps[(size_t) step];
        const int probValues[] = { 100, 75, 60, 50, 40, 25, 10 };
        const int gateValues[] = { 10, 25, 50, 75, 100 };
        const int velValues[]  = { 100, 80, 60, 45, 30 };
        const float nan = std::numeric_limits<float>::quiet_NaN();
        if (res == 1) { p.accent = ! p.accent; p.on = true; }
        else if (res == 2) { p.slide = ! p.slide; p.on = true; }
        else if (res > 10 && res <= 14) p.ratchet = res - 10;
        else if (res >= 20 && res < 27) p.probability = (float) probValues[res - 20] / 100.0f;
        else if (res >= 30 && res < 35) p.gate = (float) gateValues[res - 30] / 100.0f;
        else if (res >= 50 && res < 55) p.velocity = (float) velValues[res - 50] / 100.0f;
        else if (res == 40) p.lockIndex = idx;
        else if (res == 41) p.lockIndex = nan;
        else if (res == 42) p.lockCutoff = cut;
        else if (res == 43) p.lockCutoff = nan;
        else if (res == 60)
        {
            const auto src = p;
            for (auto& o : pat.steps)
                if (o.on)
                {
                    const int note = o.note, oct = o.octave;
                    o = src;
                    o.note = note; o.octave = oct;
                }
        }
        safe->publish();
    });
}

void SeqGrid::tick()
{
    if (processor.transportPlaying.load (std::memory_order_relaxed)
        && processor.apvts.getRawParameterValue (ParamIDs::seqMode)->load() < 0.5f)
    {
        const int rate = (int) processor.apvts.getRawParameterValue (ParamIDs::seqRate)->load();
        const int len  = (int) processor.apvts.getRawParameterValue (ParamIDs::seqLength)->load();
        const double pos = processor.displayPpq.load (std::memory_order_relaxed) / Choices::seqRateQuarters[juce::jlimit (0, 5, rate)];
        playPos = (float) std::fmod (std::max (0.0, pos), (double) juce::jmax (1, len));
    }
    else
    {
        playPos = -1.0f;
    }
}

void SeqGrid::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xd0061a22), 0.0f, bounds.getY(),
                                             juce::Colour (0xe0010608), 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 10.0f);

    auto& st = processor.apvts;
    const auto& pat    = processor.pattern.get();
    const int length   = (int) st.getRawParameterValue (ParamIDs::seqLength)->load();
    const int laneLen  = (int) st.getRawParameterValue (ParamIDs::modLaneLength)->load();
    const bool laneOn  = st.getRawParameterValue (ParamIDs::modLaneDest)->load() > 0.5f;
    const int  vis     = visibleSteps();
    const float t      = Ui::now();

    const auto a = notesArea();
    const float colW = a.getWidth() / (float) vis;
    const bool playing = playPos >= 0.0f;
    const int  current = playing ? (int) playPos : -1;

    for (int i = 0; i < vis; ++i)
    {
        const auto& s = pat.steps[(size_t) i];
        const bool in = i < length;
        auto col = juce::Rectangle<float> (a.getX() + (float) i * colW, a.getY(), colW, a.getHeight()).reduced (colW > 20.0f ? 2.5f : 1.5f, 0.0f);

        g.setColour (juce::Colours::white.withAlpha (in ? ((i / 4) % 2 == 0 ? 0.055f : 0.03f) : 0.012f)
                         .withMultipliedAlpha (i == hoverStep ? 1.8f : 1.0f));
        g.fillRoundedRectangle (col, 3.0f);

        float swell = 0.0f;
        if (playing)
        {
            const float d = ((float) i + 0.5f) - playPos;
            swell = std::exp (-d * d * 1.2f);
        }

        if (s.on && in)
        {
            const float level = juce::jlimit (0.05f, 1.0f, (float) (stepPitch (s) - pitchLow) / (float) pitchRange + swell * 0.07f);
            juce::Graphics::ScopedSaveState save (g);
            auto& clip = Ui::scratchPath (7);
            clip.addRoundedRectangle (col, 3.0f);
            g.reduceClipRegion (clip);
            const float alpha = 0.3f + 0.55f * s.probability * (0.6f + 0.4f * s.velocity) + 0.15f * swell;
            const auto top = (s.accent ? Ui::coral : Ui::aqua).withAlpha (juce::jmin (1.0f, alpha));
            Ui::drawLiquid (g, col, level, t, (float) i * 1.3f, top, Ui::aquaDeep.withAlpha (0.25f), 1.0f + swell * 2.5f);

            for (int k = 1; k < s.ratchet; ++k)
            {
                g.setColour (juce::Colours::white.withAlpha (0.65f));
                g.fillRect (col.getX() + 2.0f, col.getBottom() - 4.0f - (float) k * 4.0f, col.getWidth() - 4.0f, 1.2f);
            }
            if (s.hasLockIndex() || s.hasLockCutoff())
            {
                const auto c = juce::Point<float> (col.getCentreX(), col.getY() + 7.0f);
                auto& d = Ui::scratchPath (6);
                d.addTriangle (c.x - 3.0f, c.y, c.x, c.y - 3.0f, c.x + 3.0f, c.y);
                d.addTriangle (c.x - 3.0f, c.y, c.x, c.y + 3.0f, c.x + 3.0f, c.y);
                g.setColour (s.hasLockIndex() ? Ui::coral : juce::Colours::white);
                g.fillPath (d);
            }
            if (s.probability < 0.999f)   // pointillé : la note ne joue pas toujours
            {
                g.setColour (juce::Colours::white.withAlpha (0.5f));
                for (float y = col.getY() + 3.0f; y < col.getBottom() - 3.0f; y += 6.0f)
                    g.fillRect (col.getRight() - 2.5f, y, 1.0f, 3.0f);
            }
        }

        if (s.on && s.slide && in)
        {
            g.setColour (Ui::aqua.withAlpha (0.85f));
            const float y = col.getY() + 12.0f;
            g.drawLine (col.getRight() - 3.0f, y, col.getRight() + (colW - col.getWidth()) + 3.0f, y, 1.6f);
        }

        if (i == current)
        {
            g.setColour (juce::Colours::white.withAlpha (0.55f));
            g.drawRoundedRectangle (col, 3.0f, 1.0f);
        }

        if (i % 4 == 0)
        {
            g.setColour (Ui::dim.withAlpha (0.75f));
            g.setFont (Ui::font (8.0f, true));
            static const char* const beatLabels[] { "1", "2", "3", "4", "5", "6", "7", "8" };
            g.drawText (beatLabels[(i / 4) & 7], juce::Rectangle<float> (col.getX(), a.getBottom() + 2.0f, colW * 2.0f, 9.0f),
                        juce::Justification::centredLeft);
        }
    }

    // --- vague de lecture : une crête qui glisse au-dessus des colonnes
    if (playing)
    {
        juce::Graphics::ScopedSaveState clipCrest (g);
        g.reduceClipRegion (a.expanded (2.0f, 14.0f).toNearestInt());
        const float cx = a.getX() + playPos * colW;
        crest.clear();
        const int pts = 60;
        for (int k = -pts / 2; k <= pts / 2; ++k)
        {
            const float xx = cx + (float) k * colW * 0.09f;
            const float u = (float) k / (float) (pts / 2);
            const float yy = a.getY() - 1.0f - 7.0f * std::exp (-u * u * 5.0f) + 1.0f * std::sin ((float) k * 0.7f + t * 6.0f);
            if (k == -pts / 2) crest.startNewSubPath (xx, yy); else crest.lineTo (xx, yy);
        }
        g.setGradientFill (juce::ColourGradient (Ui::aqua.withAlpha (0.14f), cx, a.getY(), Ui::aqua.withAlpha (0.0f), cx, a.getBottom(), false));
        g.fillRect (juce::Rectangle<float> (cx - colW * 0.6f, a.getY(), colW * 1.2f, a.getHeight()));
        g.setColour (Ui::aqua.withAlpha (0.3f));
        g.strokePath (crest, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (juce::Colours::white.withAlpha (0.92f));
        g.strokePath (crest, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- piste Mod (polymétrie : sa propre longueur)
    const auto la = laneArea();
    g.setColour (juce::Colours::white.withAlpha (0.04f));
    g.fillRoundedRectangle (la, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine ((int) la.getCentreY(), la.getX(), la.getRight());
    const int laneCur = playing ? processor.getCurrentLaneStep() : -1;
    for (int i = 0; i < vis; ++i)
    {
        const float v = pat.modLane[(size_t) i];
        auto col = juce::Rectangle<float> (la.getX() + (float) i * colW, la.getY(), colW, la.getHeight()).reduced (colW > 20.0f ? 3.0f : 2.0f, 2.0f);
        const float h = v * col.getHeight() * 0.5f;
        const bool in = i < laneLen;
        const auto c = (i == laneCur ? juce::Colours::white : Ui::coral).withAlpha (in ? (laneOn ? 0.85f : 0.35f) : 0.12f);
        g.setColour (c);
        g.fillRect (juce::Rectangle<float> (col.getX(), h > 0 ? col.getCentreY() - h : col.getCentreY(), col.getWidth(), std::max (1.0f, std::abs (h))));
    }

    // --- info du pas survolé / modifié
    if (hintText.isNotEmpty())
    {
        g.setFont (Ui::font (9.0f, true, 0.12f));
        g.setColour (Ui::text);
        g.drawText (hintText, bounds.reduced (12.0f, 6.0f), juce::Justification::topRight);
    }

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
}
