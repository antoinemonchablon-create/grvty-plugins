// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "SeaView.h"
#include "../PluginProcessor.h"

namespace
{
    bool isBlackKey (int n) noexcept { const int pc = ((n % 12) + 12) % 12; return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10; }
    int numWhites (int lo, int hi) noexcept { int n = 0; for (int k = lo; k <= hi; ++k) n += isBlackKey (k) ? 0 : 1; return n; }
}

SeaView::SeaView (ConfluenceAudioProcessor& p) : processor (p)
{
    setOpaque (false);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    lastTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
}

void SeaView::resized()
{
    const auto b = getLocalBounds().toFloat();
    keys = { 12.0f, b.getHeight() - 124.0f, b.getWidth() - 24.0f, 114.0f };
    titleArea = { 0.0f, 8.0f, b.getWidth(), 76.0f };
    titleDirty = keysDirty = true;
}

float SeaView::whiteW() const noexcept { return keys.getWidth() / (float) numWhites (lowKey, highKey); }

float SeaView::keyX (int note) const noexcept
{
    int w = 0;
    for (int k = lowKey; k < note; ++k) w += isBlackKey (k) ? 0 : 1;
    return isBlackKey (note) ? keys.getX() + (float) w * whiteW() : keys.getX() + ((float) w + 0.5f) * whiteW();
}

juce::Rectangle<float> SeaView::keyRect (int note) const noexcept
{
    const float ww = whiteW(), cx = keyX (note);
    if (isBlackKey (note))
        return { cx - ww * 0.32f, keys.getY(), ww * 0.64f, keys.getHeight() * 0.6f };
    return { cx - ww * 0.5f + 1.0f, keys.getY(), ww - 2.0f, keys.getHeight() };
}

int SeaView::noteAt (juce::Point<float> p) const
{
    if (! keys.contains (p)) return -1;
    for (int n = lowKey; n <= highKey; ++n)
        if (isBlackKey (n) && keyRect (n).contains (p)) return n;
    for (int n = lowKey; n <= highKey; ++n)
        if (! isBlackKey (n) && keyRect (n).expanded (1.0f, 0.0f).contains (p)) return n;
    return -1;
}

//==============================================================================
void SeaView::playNote (int note, int velocity)
{
    if (mouseNote >= 0) processor.playFromUi (mouseNote, 0, false);
    mouseNote = note;
    if (note >= 0) processor.playFromUi (note, velocity, false);
}

void SeaView::mouseDown (const juce::MouseEvent& e)
{
    const int n = noteAt (e.position);
    if (n < 0) return;
    const auto r = keyRect (n);
    const int vel = juce::jlimit (30, 127, (int) (40.0f + 87.0f * (e.position.y - r.getY()) / r.getHeight()));
    playNote (n, vel);
}

void SeaView::mouseDrag (const juce::MouseEvent& e)
{
    const int n = noteAt (e.position);
    if (n != mouseNote && n >= 0)
        playNote (n, 96);
}

void SeaView::mouseUp (const juce::MouseEvent&)
{
    playNote (-1, 0);
}

//==============================================================================
void SeaView::spawn (int note, bool bass, float delay)
{
    for (auto& b : bubbles)
        if (! b.alive)
        {
            auto& rng = juce::Random::getSystemRandom();
            b.alive = true;
            b.bass = bass;
            b.x = keyX (note);
            b.startY = keys.getY() - 6.0f + delay * 40.0f;
            b.y = b.startY;
            b.r = 2.5f + 4.5f * rng.nextFloat();
            b.phase = rng.nextFloat() * 6.28f;
            b.speed = 55.0f + 40.0f * rng.nextFloat();
            b.life = 0.0f;
            return;
        }
}

bool SeaView::tick()
{
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const float dt = (float) juce::jlimit (0.0, 0.1, now - lastTime);
    lastTime = now;
    bool changed = false;

    // gamme : le clavier de fond est recalculé quand la tonique ou la gamme change
    const auto cs = processor.readChordSettings();
    if (cs.rootPc != keyRoot || cs.scale != keyScale)
    {
        keyRoot = cs.rootPc;
        keyScale = cs.scale;
        keysDirty = titleDirty = true;
        changed = true;
    }

    // nouvel accord : son nom en surface, et une volée de bulles depuis ses touches
    const auto d = processor.getLastChord();
    if (d.counter != lastCounter)
    {
        lastCounter = d.counter;
        shown = ChordText::describe (d.notes, d.count, d.bass, d.rootPc, d.chordRootPc, cs.scale, cs.rootPc, d.borrowed);
        chordCount = juce::jmin (d.count, (int) chordNotes.size());
        for (int i = 0; i < chordCount; ++i) chordNotes[(size_t) i] = d.notes[i];
        chordBass = d.bass;
        for (int i = 0; i < chordCount; ++i)
            for (int k = 0; k < 3; ++k)
                spawn (chordNotes[(size_t) i], chordNotes[(size_t) i] == chordBass, (float) k * 0.35f + (float) i * 0.05f);
        titleDirty = true;
        titleGlow = 1.0f;
        changed = true;
    }

    // notes qui sonnent : copie pour le dessin, et de nouvelles bulles tant qu'elles sonnent
    bool sounding = false;
    for (int i = 0; i < 4; ++i)
    {
        uint32_t s = 0, in = 0;
        for (int k = 0; k < 32; ++k)
        {
            if (processor.isSounding (i * 32 + k)) s |= 1u << k;
            if (processor.isInputHeld (i * 32 + k)) in |= 1u << k;
        }
        if (s != soundingCopy[(size_t) i] || in != inputCopy[(size_t) i]) changed = true;
        soundingCopy[(size_t) i] = s;
        inputCopy[(size_t) i] = in;
        sounding = sounding || s != 0;
    }
    if (sounding)
    {
        spawnClock += dt;
        if (spawnClock > 0.45f)
        {
            spawnClock = 0.0f;
            for (int n = lowKey; n <= highKey; ++n)
                if ((soundingCopy[(size_t) (n >> 5)] >> (n & 31)) & 1u)
                    spawn (n, n == chordBass, 0.0f);
        }
    }
    const float glowTarget = sounding ? 1.0f : 0.35f;
    if (std::abs (titleGlow - glowTarget) > 0.01f)
    {
        titleGlow += (glowTarget - titleGlow) * juce::jmin (1.0f, dt * 3.0f);
        changed = true;
    }

    // bulles : elles montent, oscillent, grossissent un peu et s'éteignent sous le nom de l'accord
    bool anyBubble = false;
    const float top = titleArea.getBottom() + 6.0f;
    for (auto& b : bubbles)
    {
        if (! b.alive) continue;
        b.life += dt;
        b.y -= b.speed * dt;
        b.x += std::sin (b.life * 3.0f + b.phase) * 8.0f * dt;
        if (b.y < top) b.alive = false;
        anyBubble = anyBubble || b.alive;
    }

    mouseInside = isMouseOverOrDragging();
    return changed || anyBubble || sounding || mouseInside;
}

//==============================================================================
void SeaView::renderTitle (float scale)
{
    const int w = juce::roundToInt (titleArea.getWidth() * scale), h = juce::roundToInt (titleArea.getHeight() * scale);
    if (! titleImg.isValid() || titleImg.getWidth() != w || titleImg.getHeight() != h)
        titleImg = juce::Image (juce::Image::ARGB, w, h, true);
    else
        titleImg.clear (titleImg.getBounds());

    juce::Graphics g (titleImg);
    g.addTransform (juce::AffineTransform::scale (scale));
    const auto r = juce::Rectangle<float> (0.0f, 0.0f, titleArea.getWidth(), titleArea.getHeight());

    if (! shown.valid)
    {
        g.setColour (Ui::dim.withAlpha (0.7f));
        g.setFont (Ui::font (12.0f, true, 0.2f));
        g.drawText (ChordText::u8 ("JOUE UNE NOTE"), r, juce::Justification::centred);
    }
    else
    {
        const auto nameFont = Ui::font (40.0f, true, -0.02f);
        juce::GlyphArrangement ga;
        ga.addLineOfText (nameFont, shown.name, 0.0f, 0.0f);
        const float nameW = ga.getBoundingBox (0, -1, true).getWidth();
        auto nameR = r.withHeight (50.0f);
        g.setColour (Ui::aqua.withAlpha (0.14f));
        g.setFont (nameFont);
        g.drawText (shown.name, nameR.translated (0.0f, 3.0f), juce::Justification::centred);
        g.setColour (juce::Colours::white);
        g.drawText (shown.name, nameR, juce::Justification::centred);
        g.setColour (Ui::aqua);
        g.setFont (Ui::font (20.0f, true));
        g.drawText (shown.degree, juce::Rectangle<float> (r.getCentreX() + nameW * 0.5f + 8.0f, 4.0f, 160.0f, 26.0f),
                    juce::Justification::centredLeft);
        g.setColour (Ui::dim);
        g.setFont (Ui::font (9.5f, true, 0.1f));
        g.drawText (shown.notes, r.withTrimmedTop (52.0f).withHeight (16.0f), juce::Justification::centred);
    }
    titleDirty = false;
}

void SeaView::renderWhiteKeys (float scale)
{
    const int w = juce::roundToInt ((float) getWidth() * scale), h = juce::roundToInt ((float) getHeight() * scale);
    if (! whiteKeysImg.isValid() || whiteKeysImg.getWidth() != w || whiteKeysImg.getHeight() != h)
        whiteKeysImg = juce::Image (juce::Image::ARGB, w, h, true);
    else
        whiteKeysImg.clear (whiteKeysImg.getBounds());

    juce::Graphics g (whiteKeysImg);
    g.addTransform (juce::AffineTransform::scale (scale));
    Ui::setVerticalGradient (g, 5, juce::Colour (0xff072630), keys.getY(), juce::Colour (0xff020a0e), keys.getBottom());
    g.fillRoundedRectangle (keys.expanded (4.0f, 4.0f), 8.0f);

    const auto mask = Theory::scaleMask (keyScale, keyRoot);
    for (int n = lowKey; n <= highKey; ++n)
    {
        if (isBlackKey (n)) continue;
        const auto k = keyRect (n);
        const bool sc = ((mask >> (unsigned) Theory::mod12 (n)) & 1u) != 0;
        const auto top = sc ? juce::Colour (0xff1c4752) : juce::Colour (0xff0e2128);
        Ui::setVerticalGradient (g, 5, top, k.getY(), top.darker (0.6f), k.getBottom());
        g.fillRoundedRectangle (k, 3.0f);
        if (sc)
        {
            g.setColour (Ui::aqua.withAlpha (0.55f));
            g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ k.getCentreX(), k.getBottom() - 8.0f }));
        }
        if (Theory::mod12 (n) == keyRoot)
        {
            g.setColour (Ui::aqua.withAlpha (0.75f));
            g.drawRoundedRectangle (k.reduced (0.5f), 3.0f, 1.2f);
        }
        if (Theory::mod12 (n) == 0)
        {
            g.setColour (Ui::dim.withAlpha (0.85f));
            g.setFont (Ui::font (7.0f, true));
            g.drawText ("C" + juce::String (n / 12 - 2), k.withTrimmedTop (k.getHeight() - 24.0f), juce::Justification::centred);
        }
    }

    // touches noires éteintes : un calque à part, posé par-dessus les touches blanches allumées
    if (! blackKeysImg.isValid() || blackKeysImg.getWidth() != w || blackKeysImg.getHeight() != h)
        blackKeysImg = juce::Image (juce::Image::ARGB, w, h, true);
    else
        blackKeysImg.clear (blackKeysImg.getBounds());
    juce::Graphics bg (blackKeysImg);
    bg.addTransform (juce::AffineTransform::scale (scale));
    for (int n = lowKey; n <= highKey; ++n)
    {
        if (! isBlackKey (n)) continue;
        const auto k = keyRect (n);
        const bool sc = ((mask >> (unsigned) Theory::mod12 (n)) & 1u) != 0;
        bg.setColour (juce::Colour (0xff01070a));
        bg.fillRoundedRectangle (k.expanded (1.0f), 2.5f);
        const auto top = sc ? juce::Colour (0xff1f4c57) : juce::Colour (0xff081419);
        Ui::setVerticalGradient (bg, 5, top.withAlpha (0.9f), k.getY(), top.withAlpha (0.35f), k.getBottom());
        bg.fillRoundedRectangle (k, 2.5f);
        if (sc)
        {
            bg.setColour (Ui::aqua.withAlpha (0.5f));
            bg.fillEllipse (juce::Rectangle<float> (3.0f, 3.0f).withCentre ({ k.getCentreX(), k.getBottom() - 6.0f }));
        }
    }
    keysDirty = false;
}

void SeaView::paint (juce::Graphics& g)
{
    const float scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (std::abs (scale - imgScale) > 0.01f) { imgScale = scale; titleDirty = keysDirty = true; }
    if (keysDirty || ! whiteKeysImg.isValid()) renderWhiteKeys (scale);
    if (titleDirty || ! titleImg.isValid()) renderTitle (scale);

    const float t = Ui::now();
    auto isOn = [this] (int n) { return ((soundingCopy[(size_t) (n >> 5)] >> (n & 31)) & 1u) != 0; };
    auto isHeld = [this] (int n) { return ((inputCopy[(size_t) (n >> 5)] >> (n & 31)) & 1u) != 0; };

    // bulles (derrière le clavier, devant le fond)
    for (const auto& b : bubbles)
    {
        if (! b.alive) continue;
        const float span = juce::jmax (1.0f, b.startY - titleArea.getBottom());
        const float f = juce::jlimit (0.0f, 1.0f, (b.startY - b.y) / span);
        const float a = std::sin (juce::jlimit (0.0f, 1.0f, f) * juce::MathConstants<float>::pi) * 0.9f + 0.1f * (1.0f - f);
        const float rad = b.r * (0.7f + 0.6f * f);
        const auto col = b.bass ? Ui::coral : Ui::aqua;
        const auto e = juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre ({ b.x, b.y });
        g.setColour (col.withAlpha (0.10f * a));
        g.fillEllipse (e);
        g.setColour (col.withAlpha (0.75f * a));
        g.drawEllipse (e, 1.1f);
        g.setColour (juce::Colours::white.withAlpha (0.55f * a));
        g.fillEllipse (juce::Rectangle<float> (rad * 0.6f, rad * 0.45f).withCentre ({ b.x - rad * 0.35f, b.y - rad * 0.4f }));
    }

    // nom de l'accord en surface (plus lumineux tant que l'accord sonne)
    g.setOpacity (juce::jlimit (0.3f, 1.0f, titleGlow));
    g.drawImageTransformed (titleImg, juce::AffineTransform::scale (1.0f / imgScale).translated (titleArea.getX(), titleArea.getY()));
    g.setOpacity (1.0f);

    // clavier : touches blanches (image), lumières des notes jouées, touches noires
    g.drawImageTransformed (whiteKeysImg, juce::AffineTransform::scale (1.0f / imgScale));
    for (int n = lowKey; n <= highKey; ++n)
    {
        if (isBlackKey (n) || ! isOn (n)) continue;
        const auto k = keyRect (n);
        const auto col = n == chordBass ? Ui::coral : Ui::aqua;
        g.setColour (col.withAlpha (0.25f));
        g.fillRoundedRectangle (k.expanded (2.0f, 0.0f), 4.0f);
        Ui::setVerticalGradient (g, 6, col.withAlpha (0.98f), k.getY(), col.withAlpha (0.55f), k.getBottom());
        g.fillRoundedRectangle (k, 3.0f);
    }
    g.setOpacity (1.0f);
    g.drawImageTransformed (blackKeysImg, juce::AffineTransform::scale (1.0f / imgScale));
    for (int n = lowKey; n <= highKey; ++n)
    {
        if (! isBlackKey (n) || ! isOn (n)) continue;
        const auto k = keyRect (n);
        const auto top = n == chordBass ? Ui::coral : Ui::aqua;
        g.setColour (juce::Colour (0xff01070a));
        g.fillRoundedRectangle (k.expanded (1.0f), 2.5f);
        Ui::setVerticalGradient (g, 6, top.withAlpha (0.98f), k.getY(), top.withAlpha (0.5f), k.getBottom());
        g.fillRoundedRectangle (k, 2.5f);
    }
    // touche tenue (note jouée au clavier MIDI ou à la souris) : un point blanc
    for (int n = lowKey; n <= highKey; ++n)
        if (isHeld (n))
        {
            const auto k = keyRect (n);
            g.setColour (juce::Colours::white.withAlpha (0.9f));
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ k.getCentreX(), k.getBottom() - (isBlackKey (n) ? 14.0f : 20.0f) }));
        }

    // caustiques : la lumière de surface qui danse sur les touches
    {
        juce::Graphics::ScopedSaveState save (g);
        g.reduceClipRegion (keys.toNearestInt());
        for (int i = 0; i < 5; ++i)
        {
            auto& c = Ui::scratchPath (6);
            const float fi = (float) i;
            const int steps = (int) (keys.getWidth() / 8.0f);
            for (int k = 0; k <= steps; ++k)
            {
                const float x = keys.getX() + 8.0f * (float) k;
                const float y = keys.getY() + keys.getHeight() * (0.15f + 0.16f * fi)
                              + 5.0f * std::sin (x * 0.03f + t * 1.3f + fi * 1.7f) + 3.0f * std::sin (x * 0.071f - t * 0.9f + fi);
                if (k == 0) c.startNewSubPath (x, y); else c.lineTo (x, y);
            }
            g.setColour (juce::Colours::white.withAlpha (0.045f));
            g.strokePath (c, juce::PathStrokeType (2.0f + fi * 0.4f));
        }
    }

    // surface de l'eau au-dessus du clavier
    auto& surface = Ui::scratchPath (7);
    const int surfSteps = (int) ((keys.getWidth() + 8.0f) / 4.0f);
    for (int k = 0; k <= surfSteps; ++k)
    {
        const float x = keys.getX() - 4.0f + 4.0f * (float) k;
        const float y = keys.getY() - 6.0f + 2.0f * std::sin (x * 0.05f + t * 2.0f) + 1.2f * std::sin (x * 0.13f - t * 1.4f);
        if (k == 0) surface.startNewSubPath (x, y); else surface.lineTo (x, y);
    }
    g.setColour (Ui::aqua.withAlpha (0.25f));
    g.strokePath (surface, juce::PathStrokeType (4.0f));
    g.setColour (juce::Colour (0xffd8fffa).withAlpha (0.8f));
    g.strokePath (surface, juce::PathStrokeType (1.2f));
}
