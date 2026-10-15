// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "FmScreen.h"
#include "../PluginProcessor.h"

FmScreen::FmScreen (UndertowAudioProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
    setTooltip ("Signal reel qui sort du plugin (ONDE : 2 cycles de la note jouee, MESURE : la derniere mesure). "
                "En fond : la forme theorique. Glisser : vertical = Index FM, horizontal = Ratio. Double-clic : Index par defaut.");
    wave.preallocateSpace (3 * 300);
    body.preallocateSpace (3 * 520);
    live.preallocateSpace (3 * 300);
    lastCounter = processor.noteCounter.load();
}

float FmScreen::getNorm (const char* id) const
{
    if (auto* p = processor.apvts.getParameter (id)) return p->getValue();
    return 0.0f;
}

void FmScreen::setParamNormalised (const char* id, float norm)
{
    if (auto* p = processor.apvts.getParameter (id))
        p->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, norm));
}

bool FmScreen::tick()
{
    const float t = Ui::now();
    const uint32_t c = processor.noteCounter.load (std::memory_order_relaxed);
    if (c != lastCounter)
    {
        lastCounter = c;
        const int note = processor.displayNote.load (std::memory_order_relaxed);
        const float index = processor.displayIndex.load (std::memory_order_relaxed);
        auto& d = drops[(size_t) nextDrop];
        nextDrop = (nextDrop + 1) % (int) drops.size();
        d.born = t;
        d.x = 0.12f + 0.76f * juce::jlimit (0.0f, 1.0f, (float) (note - 24) / 72.0f);
        d.strength = 0.35f + 0.65f * processor.lastNoteVelocity.load (std::memory_order_relaxed);
        d.rings = juce::jlimit (1, 8, 1 + juce::roundToInt (index * 0.9f));
        d.accent = processor.lastNoteAccent.load (std::memory_order_relaxed);
    }

    const float env = processor.displayAmpEnv.load (std::memory_order_relaxed);
    envSmoothed += (env - envSmoothed) * (env > envSmoothed ? 0.6f : 0.12f);

    bool alive = false;
    for (const auto& d : drops)
        if (t - d.born < 2.4f) alive = true;
    return alive || envSmoothed > 0.01f;
}

void FmScreen::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float t = Ui::now();

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xd0061a22), 0.0f, bounds.getY(),
                                             juce::Colour (0xf0010608), 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 10.0f);

    auto& st = processor.apvts;
    const float index    = processor.displayIndex.load (std::memory_order_relaxed);
    const float ratio    = processor.displayRatio.load (std::memory_order_relaxed);
    const float shapeA   = st.getRawParameterValue (ParamIDs::oscAShape)->load();
    const float shapeB   = st.getRawParameterValue (ParamIDs::oscBShape)->load();
    const float mix      = st.getRawParameterValue (ParamIDs::oscMix)->load();
    const float fb       = st.getRawParameterValue (ParamIDs::fmFeedback)->load();
    const bool  cross    = st.getRawParameterValue (ParamIDs::fmMode)->load() > 0.5f;

    const auto area   = bounds.reduced (14.0f, 16.0f).withTrimmedBottom (14.0f);
    const float surfY = area.getCentreY() + area.getHeight() * 0.06f;

    // --- gouttes : ondes concentriques à plat sur l'eau (ellipses aplaties = perspective)
    for (const auto& d : drops)
    {
        const float age = t - d.born;
        if (age < 0.0f || age > 2.4f) continue;
        const auto col = d.accent ? Ui::coral : Ui::aqua;
        const float cx = area.getX() + d.x * area.getWidth();
        for (int k = 0; k < d.rings; ++k)
        {
            const float life = (age - (float) k * 0.11f) / 1.7f;
            if (life <= 0.0f || life >= 1.0f) continue;
            const float rad = 6.0f + life * area.getWidth() * 0.42f;
            const float a = (1.0f - life) * (1.0f - life) * d.strength * (0.55f - 0.04f * (float) k);
            g.setColour (col.withAlpha (juce::jlimit (0.0f, 0.9f, a)));
            g.drawEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 0.55f).withCentre ({ cx, surfY + 18.0f }), 1.0f + 1.4f * (1.0f - life));
        }
        // la goutte elle-même, qui tombe puis disparaît
        if (age < 0.18f)
        {
            const float y = area.getY() + (surfY - area.getY()) * (age / 0.18f);
            g.setColour (col.withAlpha (0.9f));
            g.fillEllipse (juce::Rectangle<float> (4.0f, 6.0f).withCentre ({ cx, y }));
        }
    }

    // --- forme théorique (calculée depuis les réglages) : en fantôme, derrière le signal réel
    auto shapeOsc = [] (float ph, float sh)
    {
        ph -= std::floor (ph);
        const float sn = std::sin (juce::MathConstants<float>::twoPi * ph);
        float tri = 1.0f - 4.0f * std::abs (ph + 0.25f - std::floor (ph + 0.25f) - 0.5f);
        if (sh < 0.5f) { const float u = sh * 2.0f; return sn * (1.0f - u) + tri * u; }
        const float u = sh * 2.0f - 1.0f, k = 2.0f + 10.0f * u;
        return tri * (1.0f - u) + u * std::tanh (k * sn) / std::tanh (k);
    };

    const float halfH = area.getHeight() * 0.36f;
    {
        const int n = 120;
        const float amp = halfH * 0.8f;
        const float idxCyc = index / juce::MathConstants<float>::twoPi;
        const float fbCyc = fb * fb * 0.9f;
        float prevB = 0.0f, prevA = 0.0f;
        wave.clear();
        for (int i = 0; i <= n; ++i)
        {
            const float pa = (float) i / (float) n * 2.0f;
            const float b  = shapeOsc (pa * ratio + fbCyc * prevB + (cross ? idxCyc * prevA : 0.0f), shapeB);
            const float a  = shapeOsc (pa + idxCyc * b, shapeA);
            prevB = b; prevA = a;
            const float y  = a + (b - a) * mix;
            const float x  = area.getX() + area.getWidth() * (float) i / (float) n;
            if (i == 0) wave.startNewSubPath (x, surfY - y * amp); else wave.lineTo (x, surfY - y * amp);
        }
        g.setColour (Ui::aqua.withAlpha (viewMode == 0 ? 0.16f : 0.0f));
        if (viewMode == 0)
            g.strokePath (wave, juce::PathStrokeType (1.2f));
    }

    // --- signal réel qui sort du plugin
    const double sr = processor.displaySampleRate.load (std::memory_order_relaxed);
    live.clear();
    body.clear();
    float peak = 0.0f;
    const int pts = 180;

    if (viewMode == 0)
    {
        // ONDE : 2 cycles de la note jouée, calés sur un passage par zéro montant (l'image ne défile pas)
        const uint32_t w = processor.scopeWrite.load (std::memory_order_acquire);
        const float freq = juce::jlimit (20.0f, 8000.0f, processor.displayFreq.load (std::memory_order_relaxed));
        const float period = (float) sr / freq;
        const int window = juce::jlimit (48, 4096, juce::roundToInt (period * 2.0f));
        auto at = [this] (uint32_t i) { return processor.scope[(size_t) (i & (UndertowAudioProcessor::scopeSize - 1))].load (std::memory_order_relaxed); };

        uint32_t start = w - (uint32_t) window - 2u;
        const int searchLen = juce::jmin (UndertowAudioProcessor::scopeSize - window - 8, juce::roundToInt (period * 1.5f) + 2);
        for (int k = 0; k < searchLen; ++k)
        {
            const uint32_t i = w - (uint32_t) window - 2u - (uint32_t) k;
            if (at (i - 1) < 0.0f && at (i) >= 0.0f) { start = i; break; }
        }

        for (int k = 0; k < window; ++k) peak = juce::jmax (peak, std::abs (at (start + (uint32_t) k)));
        const float target = 0.92f / juce::jmax (0.02f, peak);
        scopeGain += (target - scopeGain) * (target < scopeGain ? 0.5f : 0.08f);

        for (int k = 0; k <= pts; ++k)
        {
            const float pos = (float) k / (float) pts * (float) window;
            const uint32_t i0 = start + (uint32_t) pos;
            const float fr = pos - std::floor (pos);
            const float v = at (i0) + fr * (at (i0 + 1) - at (i0));
            const float x = area.getX() + area.getWidth() * (float) k / (float) pts;
            const float y = surfY - juce::jlimit (-1.1f, 1.1f, v * scopeGain) * halfH;
            if (k == 0) { live.startNewSubPath (x, y); body.startNewSubPath (x, area.getBottom() + 14.0f); }
            live.lineTo (x, y);
            body.lineTo (x, y);
        }
    }
    else
    {
        // MESURE : enveloppe de la dernière mesure (min / max de chaque paquet de 64 échantillons)
        const uint32_t ew = processor.envWrite.load (std::memory_order_acquire);
        const double bpm = juce::jmax (20.0, processor.displayBpm.load (std::memory_order_relaxed));
        const int chunks = juce::jlimit (32, UndertowAudioProcessor::envSize - 1,
                                         (int) std::round (4.0 * 60.0 / bpm * sr / UndertowAudioProcessor::envChunk));
        auto mn = [this] (uint32_t i) { return processor.envMin[(size_t) (i & (UndertowAudioProcessor::envSize - 1))].load (std::memory_order_relaxed); };
        auto mx = [this] (uint32_t i) { return processor.envMax[(size_t) (i & (UndertowAudioProcessor::envSize - 1))].load (std::memory_order_relaxed); };
        const uint32_t first = ew - (uint32_t) chunks;
        for (int c = 0; c < chunks; ++c) peak = juce::jmax (peak, juce::jmax (std::abs (mn (first + (uint32_t) c)), std::abs (mx (first + (uint32_t) c))));
        scopeGain = 0.92f / juce::jmax (0.02f, peak);   // la fenêtre couvre une mesure entière : échelle directe, stable

        // haut (max) de gauche à droite, puis bas (min) de droite à gauche : une forme pleine
        for (int k = 0; k <= pts; ++k)
        {
            const int c0 = k * chunks / (pts + 1), c1 = juce::jmax (c0 + 1, (k + 1) * chunks / (pts + 1));
            float v = -1.0f;
            for (int c = c0; c < c1; ++c) v = juce::jmax (v, mx (first + (uint32_t) c));
            const float x = area.getX() + area.getWidth() * (float) k / (float) pts;
            const float y = surfY - juce::jlimit (-1.1f, 1.1f, v * scopeGain) * halfH;
            if (k == 0) body.startNewSubPath (x, y); else body.lineTo (x, y);
            if (k == 0) live.startNewSubPath (x, y); else live.lineTo (x, y);
        }
        for (int k = pts; k >= 0; --k)
        {
            const int c0 = k * chunks / (pts + 1), c1 = juce::jmax (c0 + 1, (k + 1) * chunks / (pts + 1));
            float v = 1.0f;
            for (int c = c0; c < c1; ++c) v = juce::jmin (v, mn (first + (uint32_t) c));
            body.lineTo (area.getX() + area.getWidth() * (float) k / (float) pts, surfY - juce::jlimit (-1.1f, 1.1f, v * scopeGain) * halfH);
        }
        body.closeSubPath();

        // repères des 4 temps
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        for (int b = 1; b < 4; ++b)
            g.drawVerticalLine (juce::roundToInt (area.getX() + area.getWidth() * (float) b / 4.0f), area.getY() + 6.0f, area.getBottom());
    }

    const bool silent = peak < 1.0e-4f;
    if (viewMode == 0)
    {
        body.lineTo (area.getRight(), area.getBottom() + 14.0f);
        body.closeSubPath();
        g.setGradientFill (juce::ColourGradient (Ui::aqua.withAlpha (silent ? 0.04f : 0.14f), 0.0f, surfY - halfH,
                                                 Ui::aquaDeep.withAlpha (0.0f), 0.0f, bounds.getBottom(), false));
        g.fillPath (body);
    }
    else
    {
        g.setGradientFill (juce::ColourGradient (Ui::aqua.withAlpha (0.45f), 0.0f, surfY - halfH,
                                                 Ui::aquaDeep.withAlpha (0.25f), 0.0f, surfY + halfH, false));
        g.fillPath (body);
    }
    // jointures droites (moins coûteuses que les arrondis, identiques à l'œil à cette densité de points)
    g.setColour (Ui::aqua.withAlpha (silent ? 0.08f : 0.2f));
    g.strokePath (live, juce::PathStrokeType (4.0f, juce::PathStrokeType::beveled));
    g.setColour (juce::Colour (0xffe8fffc).withAlpha (silent ? 0.35f : 1.0f));
    g.strokePath (live, juce::PathStrokeType (1.5f, juce::PathStrokeType::beveled));

    // --- lecture chiffrée (texte refait seulement quand la valeur affichée change)
    const int key = juce::roundToInt (index * 10.0f) * 100000 + juce::roundToInt (ratio * 100.0f) * 2 + (cross ? 1 : 0);
    if (key != infoKey)
    {
        infoKey = key;
        infoText = "INDEX " + juce::String (index, 1) + "    RATIO " + juce::String (ratio, 2) + (cross ? "    A <> B" : "    B > A");
    }
    g.setFont (Ui::font (9.0f, true, 0.14f));
    g.setColour (dragging ? Ui::text : Ui::dim);
    g.drawText (infoText, bounds.reduced (12.0f, 8.0f), juce::Justification::centredBottom);

    // --- en haut à gauche : LIVE + note jouée et niveau de crête
    const int note = processor.displayNote.load (std::memory_order_relaxed);
    const int lvl = silent ? -1000 : juce::roundToInt (juce::Decibels::gainToDecibels (peak));
    if (note != noteKey || lvl != levelKey)
    {
        noteKey = note; levelKey = lvl;
        noteText = (note >= 0 ? juce::MidiMessage::getMidiNoteName (note, true, true, 3) + "  " : juce::String())
                 + (silent ? juce::String ("silence") : juce::String (lvl) + " dBFS");
    }
    g.setColour (silent ? Ui::dim : Ui::coral);
    g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ bounds.getX() + 17.0f, bounds.getY() + 15.0f }));
    g.setColour (Ui::aqua.withAlpha (0.75f));
    g.setFont (Ui::font (8.5f, true, 0.3f));
    g.drawText ("LIVE", juce::Rectangle<float> (bounds.getX() + 25.0f, bounds.getY() + 8.0f, 40.0f, 14.0f), juce::Justification::centredLeft);
    g.setColour (Ui::dim);
    g.setFont (Ui::font (8.5f, true, 0.1f));
    g.drawText (noteText, juce::Rectangle<float> (bounds.getX() + 58.0f, bounds.getY() + 8.0f, 140.0f, 14.0f), juce::Justification::centredLeft);

    // --- choix de la vue : ONDE / MESURE
    for (int m = 0; m < 2; ++m)
    {
        const auto r = viewPill (m);
        const bool on = m == viewMode;
        g.setColour (on ? Ui::aqua.withAlpha (0.28f) : juce::Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (on ? Ui::aqua : juce::Colours::white.withAlpha (0.14f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        g.setColour (on ? juce::Colours::white : Ui::dim);
        g.setFont (Ui::font (8.0f, true, 0.14f));
        g.drawText (m == 0 ? "ONDE" : "MESURE", r, juce::Justification::centred);
    }

    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
}

juce::Rectangle<float> FmScreen::viewPill (int mode) const
{
    const float w = mode == 0 ? 44.0f : 54.0f;
    const float x = (float) getWidth() - 12.0f - 54.0f - (mode == 0 ? 48.0f : 0.0f);
    return { x, 7.0f, w, 17.0f };
}

void FmScreen::mouseDown (const juce::MouseEvent& e)
{
    for (int m = 0; m < 2; ++m)
        if (viewPill (m).expanded (2.0f).contains (e.position))
        {
            viewMode = m;
            scopeGain = 1.0f;
            repaint();
            return;
        }

    dragging = true;
    dragIndex0 = getNorm (ParamIDs::fmIndex);
    const bool freeRatio = processor.apvts.getRawParameterValue (ParamIDs::oscBFree)->load() > 0.5f;
    dragRatio0 = getNorm (freeRatio ? ParamIDs::oscBFreeRatio : ParamIDs::oscBRatio);
    for (auto* id : { ParamIDs::fmIndex, ParamIDs::oscBRatio, ParamIDs::oscBFreeRatio })
        if (auto* p = processor.apvts.getParameter (id)) p->beginChangeGesture();
}

void FmScreen::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging) return;
    const bool freeRatio = processor.apvts.getRawParameterValue (ParamIDs::oscBFree)->load() > 0.5f;
    setParamNormalised (ParamIDs::fmIndex, dragIndex0 - (float) e.getDistanceFromDragStartY() / ((float) getHeight() * 0.9f));
    setParamNormalised (freeRatio ? ParamIDs::oscBFreeRatio : ParamIDs::oscBRatio,
                        dragRatio0 + (float) e.getDistanceFromDragStartX() / ((float) getWidth() * 0.9f));
}

void FmScreen::mouseUp (const juce::MouseEvent&)
{
    if (! dragging) return;
    dragging = false;
    for (auto* id : { ParamIDs::fmIndex, ParamIDs::oscBRatio, ParamIDs::oscBFreeRatio })
        if (auto* p = processor.apvts.getParameter (id)) p->endChangeGesture();
}

void FmScreen::mouseDoubleClick (const juce::MouseEvent&)
{
    if (auto* p = processor.apvts.getParameter (ParamIDs::fmIndex))
        p->setValueNotifyingHost (p->getDefaultValue());
}
