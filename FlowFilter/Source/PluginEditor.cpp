// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Flow Filter, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "PluginEditor.h"

//==============================================================================
namespace Ui
{
    const juce::Colour bgTop     { 0xff08202b };
    const juce::Colour bgBottom  { 0xff02080c };
    const juce::Colour aqua      { 0xff4be3d6 };
    const juce::Colour aquaDeep  { 0xff1a7f99 };
    const juce::Colour coral     { 0xffff8a65 };
    const juce::Colour text      { 0xffdcecf1 };
    const juce::Colour dim       { 0xff6f8f9b };
    const juce::Colour ink       { 0xff04121a };

    inline juce::Colour glassFill() { return juce::Colours::white.withAlpha (0.035f); }
    inline juce::Colour glassEdge() { return juce::Colours::white.withAlpha (0.08f); }

    inline juce::Font font (float size, bool bold = false, float kerning = 0.0f)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain))
                   .withExtraKerningFactor (kerning);
    }

    /** Horloge commune de toutes les animations (secondes). */
    inline float now() { return (float) std::fmod (juce::Time::getMillisecondCounterHiRes() * 0.001, 100000.0); }

    constexpr float minDb = -60.0f, maxDb = 6.0f;

    /** Pseudo-aléatoire stable (0..1) pour placer particules et bulles. */
    inline float hash (float n) { const float s = std::sin (n * 12.9898f) * 43758.5453f; return s - std::floor (s); }

    /** Liquide ondulant qui remplit une zone jusqu'à 'level' (0..1). À appeler dans une zone clippée. */
    void drawLiquid (juce::Graphics& g, juce::Rectangle<float> area, float level, float t, float seed,
                     juce::Colour top, juce::Colour bottom, float amp)
    {
        if (level <= 0.001f)
            return;

        const float surfaceY = area.getBottom() - level * area.getHeight();

        for (int layer = 0; layer < 2; ++layer)
        {
            juce::Path p;
            p.startNewSubPath (area.getX() - 2.0f, area.getBottom() + 2.0f);
            const float ph   = t * (layer == 0 ? 3.1f : -2.3f) + seed + (float) layer * 1.7f;
            const float freq = layer == 0 ? 0.21f : 0.29f;

            for (float x = area.getX() - 2.0f; x <= area.getRight() + 2.0f; x += 2.0f)
                p.lineTo (x, surfaceY + amp * std::sin ((x - area.getX()) * freq + ph));

            p.lineTo (area.getRight() + 2.0f, area.getBottom() + 2.0f);
            p.closeSubPath();

            const float alpha = layer == 0 ? 1.0f : 0.45f;
            g.setGradientFill (juce::ColourGradient (top.withMultipliedAlpha (alpha), 0.0f, surfaceY,
                                                     bottom.withMultipliedAlpha (alpha), 0.0f, area.getBottom(), false));
            g.fillPath (p);
        }
    }

    void drawGlassPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 8.0f)
    {
        g.setColour (glassFill());
        g.fillRoundedRectangle (r, corner);
        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.04f), 0.0f, r.getY(),
                                                 juce::Colours::transparentWhite, 0.0f, r.getY() + 40.0f, false));
        g.fillRoundedRectangle (r, corner);
        g.setColour (glassEdge());
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    }
}

//==============================================================================
// LiquidLook
//==============================================================================
LiquidLook::LiquidLook()
{
    setColour (juce::Slider::textBoxTextColourId,       Ui::text);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId,  Ui::aqua.withAlpha (0.35f));
    setColour (juce::Label::textColourId,               Ui::text);
    setColour (juce::ComboBox::textColourId,            Ui::text);
    setColour (juce::ComboBox::arrowColourId,           Ui::aqua);
    setColour (juce::PopupMenu::textColourId,           Ui::text);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, Ui::aqua.withAlpha (0.25f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
    setColour (juce::TextEditor::backgroundColourId,    Ui::ink);
    setColour (juce::TextEditor::textColourId,          Ui::text);
    setColour (juce::TextEditor::highlightColourId,     Ui::aqua.withAlpha (0.35f));
    setColour (juce::TextEditor::outlineColourId,       juce::Colours::transparentBlack);
    setColour (juce::TextEditor::focusedOutlineColourId, Ui::aqua);
    setColour (juce::CaretComponent::caretColourId,     Ui::aqua);
    setColour (juce::TooltipWindow::backgroundColourId, Ui::ink.withAlpha (0.95f));
    setColour (juce::TooltipWindow::textColourId,       Ui::text);
    setColour (juce::TooltipWindow::outlineColourId,    Ui::aqua.withAlpha (0.4f));
    setColour (juce::AlertWindow::backgroundColourId,   Ui::ink);
    setColour (juce::AlertWindow::textColourId,         Ui::text);
    setColour (juce::AlertWindow::outlineColourId,      Ui::aqua.withAlpha (0.5f));
    setColour (juce::TextButton::buttonColourId,        juce::Colour (0xff0d2530));
    setColour (juce::TextButton::textColourOffId,       Ui::text);
    setColour (juce::PopupMenu::headerTextColourId,     Ui::aqua);
}

void LiquidLook::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                   float startAngle, float endAngle, juce::Slider& s)
{
    const float size    = (float) juce::jmin (w, h, 46);
    const float radius  = size * 0.5f - 3.0f;
    const auto  c       = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).getCentre();
    const float angle   = startAngle + pos * (endAngle - startAngle);
    const bool  bipolar = (bool) s.getProperties().getWithDefault ("bipolar", false);
    const bool  active  = s.isMouseOverOrDragging();
    const float t       = Ui::now();
    const float seed    = (float) (reinterpret_cast<juce::pointer_sized_int> (&s) % 997) * 0.37f;

    // --- anneau : piste + valeur lumineuse
    juce::Path track;
    track.addCentredArc (c.x, c.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        juce::Path value;
        value.addCentredArc (c.x, c.y, radius, radius, 0.0f, juce::jmin (from, angle), juce::jmax (from, angle), true);
        g.setColour (Ui::aqua.withAlpha (active ? 0.28f : 0.16f));
        g.strokePath (value, juce::PathStrokeType (7.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (Ui::aqua);
        g.strokePath (value, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- goutte intérieure : liquide dont le niveau suit la valeur
    const float ri = radius - 5.5f;
    const auto inner = juce::Rectangle<float> (ri * 2.0f, ri * 2.0f).withCentre (c);

    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff0d2530), c.x, c.y - ri,
                                             juce::Colour (0xff03090d), c.x, c.y + ri, false));
    g.fillEllipse (inner);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addEllipse (inner);
        g.reduceClipRegion (clip);
        Ui::drawLiquid (g, inner, 0.06f + 0.9f * pos, t, seed,
                        Ui::aqua.withAlpha (0.75f), Ui::aquaDeep.withAlpha (0.35f), active ? 2.2f : 1.2f);
    }

    if (bipolar)   // repère du centre
    {
        g.setColour (juce::Colours::white.withAlpha (0.25f));
        g.drawHorizontalLine ((int) c.y, inner.getX() + 2.0f, inner.getRight() - 2.0f);
    }

    // reflet
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillEllipse (inner.getX() + ri * 0.35f, inner.getY() + ri * 0.22f, ri * 0.55f, ri * 0.28f);
    g.drawEllipse (inner, 1.0f);

    // --- point lumineux au bout de la valeur
    const auto dot = c.getPointOnCircumference (radius, angle);
    g.setColour (Ui::aqua.withAlpha (0.35f));
    g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre (dot));
    g.setColour (juce::Colours::white);
    g.fillEllipse (juce::Rectangle<float> (4.5f, 4.5f).withCentre (dot));
}

void LiquidLook::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    const auto r = juce::Rectangle<float> ((float) w, (float) h).reduced (0.5f);
    g.setColour (juce::Colours::white.withAlpha (box.isMouseOver() ? 0.09f : 0.05f));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (Ui::aqua.withAlpha (0.35f));
    g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);

    juce::Path chevron;
    const float ax = (float) w - 11.0f, ay = (float) h * 0.5f;
    chevron.startNewSubPath (ax - 3.0f, ay - 1.5f);
    chevron.lineTo (ax, ay + 1.5f);
    chevron.lineTo (ax + 3.0f, ay - 1.5f);
    g.setColour (Ui::aqua);
    g.strokePath (chevron, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

juce::Font LiquidLook::getComboBoxFont (juce::ComboBox&) { return Ui::font (10.5f, true); }
juce::Font LiquidLook::getPopupMenuFont()                 { return Ui::font (12.5f); }

void LiquidLook::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (8, 0, box.getWidth() - 22, box.getHeight());
    label.setFont (getComboBoxFont (box));
    label.setJustificationType (juce::Justification::centredLeft);
}

void LiquidLook::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (Ui::ink);
    g.setColour (Ui::aqua.withAlpha (0.3f));
    g.drawRect (0, 0, w, h);
}

juce::Label* LiquidLook::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (Ui::font (10.0f));
    l->setJustificationType (juce::Justification::centred);
    l->setColour (juce::Label::textColourId, Ui::text);
    return l;
}

//==============================================================================
// Knob
//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& name,
            bool bipolar, const juce::String& tooltip)
{
    title.setText (name.toUpperCase(), juce::dontSendNotification);
    title.setFont (Ui::font (8.8f, true, 0.12f));
    title.setColour (juce::Label::textColourId, Ui::dim);
    title.setJustificationType (juce::Justification::centred);
    title.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (title);

    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 14);
    slider.setMouseDragSensitivity (200);
    slider.getProperties().set ("bipolar", bipolar);
    slider.setTooltip (tooltip);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (state, paramID, slider);

    if (auto* p = state.getParameter (paramID))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
}

void Knob::resized()
{
    auto r = getLocalBounds();
    title.setBounds (r.removeFromTop (12));
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, r.getWidth(), 14);
    slider.setBounds (r);
}

//==============================================================================
// PillToggle
//==============================================================================
PillToggle::PillToggle (juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
                        const juce::String& text, Icon i)
    : juce::Button (text), icon (i)
{
    setClickingTogglesState (true);
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, paramID, *this);
}

void PillToggle::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const bool on = getToggleState();
    auto r = getLocalBounds().toFloat().reduced (2.0f);
    const float corner = r.getHeight() * 0.5f;

    if (on)
    {
        g.setColour (Ui::aqua.withAlpha (0.14f));
        g.fillRoundedRectangle (r.expanded (1.5f), corner + 1.5f);
        g.setGradientFill (juce::ColourGradient (Ui::aqua.withAlpha (0.38f), r.getX(), r.getY(),
                                                 Ui::aquaDeep.withAlpha (0.25f), r.getX(), r.getBottom(), false));
    }
    else
    {
        g.setColour (juce::Colours::white.withAlpha (highlighted ? 0.08f : 0.04f));
    }
    g.fillRoundedRectangle (r, corner);

    g.setColour (on ? Ui::aqua.withAlpha (0.9f) : juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (r, corner, down ? 1.6f : 1.0f);

    // --- icône
    const auto col = on ? juce::Colours::white : Ui::dim;
    const juce::Point<float> ic (r.getX() + corner + 3.0f, r.getCentreY());
    const float s = 4.0f;
    g.setColour (col);
    const juce::PathStrokeType stroke (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    if (icon == Icon::sync)          // horloge
    {
        g.drawEllipse (juce::Rectangle<float> (s * 2.0f, s * 2.0f).withCentre (ic), 1.2f);
        juce::Path hand;
        hand.startNewSubPath (ic.x, ic.y - s * 0.65f);
        hand.lineTo (ic);
        hand.lineTo (ic.x + s * 0.55f, ic.y + s * 0.2f);
        g.strokePath (hand, stroke);
    }
    else if (icon == Icon::reverse)  // flèche circulaire
    {
        juce::Path arc;
        arc.addCentredArc (ic.x, ic.y, s, s, 0.0f, 0.6f, 5.2f, true);
        g.strokePath (arc, stroke);
        const auto tip = ic.getPointOnCircumference (s, 5.2f);
        juce::Path head;
        head.addTriangle (tip.x - 2.2f, tip.y - 2.0f, tip.x + 2.2f, tip.y - 0.4f, tip.x - 0.2f, tip.y + 2.4f);
        g.fillPath (head);
    }
    else                             // flocon
    {
        juce::Path flake;
        for (int k = 0; k < 3; ++k)
        {
            const float a = juce::MathConstants<float>::pi * (float) k / 3.0f;
            flake.startNewSubPath (ic.getPointOnCircumference (s + 0.5f, a));
            flake.lineTo (ic.getPointOnCircumference (s + 0.5f, a + juce::MathConstants<float>::pi));
        }
        g.strokePath (flake, stroke);
    }

    // --- texte
    g.setFont (Ui::font (9.5f, true, 0.1f));
    g.setColour (on ? juce::Colours::white : Ui::text.withAlpha (0.75f));
    g.drawText (getButtonText(), r.withTrimmedLeft (corner * 2.0f + 2.0f).withTrimmedRight (6.0f),
                juce::Justification::centred, false);
}

//==============================================================================
// TabBar
//==============================================================================
TabBar::TabBar (juce::StringArray n) : names (std::move (n))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void TabBar::animate()
{
    const float target = (float) current;
    if (std::abs (indicator - target) > 0.001f)
    {
        indicator += (target - indicator) * 0.3f;
        repaint();
    }
}

void TabBar::paint (juce::Graphics& g)
{
    const float w = (float) getWidth() / (float) names.size();
    const float h = (float) getHeight();

    for (int i = 0; i < names.size(); ++i)
    {
        const bool sel = i == current;
        g.setFont (Ui::font (10.0f, true, 0.18f));
        g.setColour (sel ? juce::Colours::white : (i == hovered ? Ui::text : Ui::dim));
        g.drawText (names[i], juce::Rectangle<float> ((float) i * w, 0.0f, w, h - 4.0f), juce::Justification::centred);
    }

    // indicateur lumineux
    const float x  = indicator * w + w * 0.2f;
    const auto bar = juce::Rectangle<float> (x, h - 3.0f, w * 0.6f, 2.0f);
    g.setColour (Ui::aqua.withAlpha (0.25f));
    g.fillRoundedRectangle (bar.expanded (2.0f, 2.0f), 3.0f);
    g.setColour (Ui::aqua);
    g.fillRoundedRectangle (bar, 1.0f);
}

void TabBar::mouseDown (const juce::MouseEvent& e)
{
    const int i = juce::jlimit (0, names.size() - 1, (int) (e.position.x / ((float) getWidth() / (float) names.size())));
    if (i != current)
    {
        current = i;
        repaint();
        if (onChange) onChange (i);
    }
}

void TabBar::mouseMove (const juce::MouseEvent& e)
{
    const int i = (int) (e.position.x / ((float) getWidth() / (float) names.size()));
    if (i != hovered) { hovered = i; repaint(); }
}

void TabBar::mouseExit (const juce::MouseEvent&) { hovered = -1; repaint(); }

//==============================================================================
// PresetBar
//==============================================================================
PresetBar::PresetBar (FlowFilterAudioProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    shownName = processor.presets.getCurrentName();
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    diceR = r.removeFromRight (r.getHeight() + 4);
    r.removeFromRight (6);
    saveR = r.removeFromRight (54);
    r.removeFromRight (8);
    prevR = r.removeFromLeft (r.getHeight());
    nextR = r.removeFromRight (r.getHeight());
    nameR = r.reduced (3, 0);
}

PresetBar::Zone PresetBar::zoneAt (juce::Point<int> pt) const
{
    if (prevR.contains (pt)) return Zone::prev;
    if (nextR.contains (pt)) return Zone::next;
    if (nameR.contains (pt)) return Zone::name;
    if (saveR.contains (pt)) return Zone::save;
    if (diceR.contains (pt)) return Zone::dice;
    return Zone::none;
}

void PresetBar::refresh()
{
    const auto name = processor.presets.getCurrentName();
    bool dirty = name != shownName;
    shownName = name;

    if (diceSpin > 0.001f) { diceSpin *= 0.82f; dirty = true; }
    if (dirty) repaint();
}

void PresetBar::paint (juce::Graphics& g)
{
    const juce::PathStrokeType stroke (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

    auto pill = [&g] (juce::Rectangle<int> r, bool hot, bool accent)
    {
        const auto f = r.toFloat().reduced (0.5f);
        g.setColour (juce::Colours::white.withAlpha (hot ? 0.09f : 0.045f));
        g.fillRoundedRectangle (f, f.getHeight() * 0.5f);
        g.setColour (accent ? Ui::aqua.withAlpha (hot ? 0.9f : 0.5f) : juce::Colours::white.withAlpha (hot ? 0.2f : 0.1f));
        g.drawRoundedRectangle (f, f.getHeight() * 0.5f, 1.0f);
    };

    // --- flèches précédent / suivant
    for (auto [r, dir, zone] : { std::tuple { prevR, -1.0f, Zone::prev }, std::tuple { nextR, 1.0f, Zone::next } })
    {
        pill (r, hovered == zone, false);
        const auto c = r.toFloat().getCentre();
        juce::Path chev;
        chev.startNewSubPath (c.x - 1.5f * dir, c.y - 4.0f);
        chev.lineTo (c.x + 2.0f * dir, c.y);
        chev.lineTo (c.x - 1.5f * dir, c.y + 4.0f);
        g.setColour (hovered == zone ? juce::Colours::white : Ui::aqua);
        g.strokePath (chev, stroke);
    }

    // --- nom du preset
    {
        const auto f = nameR.toFloat();
        g.setColour (juce::Colour (0xaa020a0e));
        g.fillRoundedRectangle (f, 5.0f);
        g.setColour (hovered == Zone::name ? Ui::aqua.withAlpha (0.7f) : juce::Colours::white.withAlpha (0.1f));
        g.drawRoundedRectangle (f.reduced (0.5f), 5.0f, 1.0f);

        g.setColour (juce::Colours::white);
        g.setFont (Ui::font (11.5f, true, 0.03f));
        g.drawText (shownName, nameR.reduced (12, 0).withTrimmedRight (12), juce::Justification::centred, true);

        juce::Path down;
        const float ax = f.getRight() - 12.0f, ay = f.getCentreY();
        down.startNewSubPath (ax - 3.0f, ay - 1.5f);
        down.lineTo (ax, ay + 1.5f);
        down.lineTo (ax + 3.0f, ay - 1.5f);
        g.setColour (Ui::dim);
        g.strokePath (down, juce::PathStrokeType (1.2f));
    }

    // --- SAVE
    pill (saveR, hovered == Zone::save, true);
    g.setColour (hovered == Zone::save ? juce::Colours::white : Ui::aqua);
    g.setFont (Ui::font (9.5f, true, 0.15f));
    g.drawText ("SAVE", saveR, juce::Justification::centred);

    // --- dé (Random) : tourne quand on clique
    {
        const auto c = diceR.toFloat().getCentre();
        const float s = (float) diceR.getHeight() - 8.0f;
        const auto rot = juce::AffineTransform::rotation (diceSpin * juce::MathConstants<float>::twoPi, c.x, c.y);

        juce::Path body;
        body.addRoundedRectangle (juce::Rectangle<float> (s, s).withCentre (c), 4.0f);
        const bool hot = hovered == Zone::dice;
        g.setColour (Ui::coral.withAlpha (hot ? 0.35f : 0.18f));
        g.fillPath (body, rot);
        g.setColour (Ui::coral.withAlpha (hot ? 1.0f : 0.8f));
        g.strokePath (body, juce::PathStrokeType (1.3f), rot);

        // points du dé
        static const float pips[7][6][2] = {
            {}, { { 0, 0 } }, { { -1, -1 }, { 1, 1 } }, { { -1, -1 }, { 0, 0 }, { 1, 1 } },
            { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } }, { { -1, -1 }, { 1, -1 }, { 0, 0 }, { -1, 1 }, { 1, 1 } },
            { { -1, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 1, 1 } } };
        const float step = s * 0.26f;
        g.setColour (juce::Colours::white);
        for (int i = 0; i < diceFace; ++i)
        {
            auto pt = juce::Point<float> (c.x + pips[diceFace][i][0] * step, c.y + pips[diceFace][i][1] * step).transformedBy (rot);
            g.fillEllipse (juce::Rectangle<float> (2.8f, 2.8f).withCentre (pt));
        }
    }
}

void PresetBar::mouseMove (const juce::MouseEvent& e)
{
    const auto z = zoneAt (e.getPosition());
    if (z != hovered) { hovered = z; repaint(); }
}

void PresetBar::mouseExit (const juce::MouseEvent&) { hovered = Zone::none; repaint(); }

void PresetBar::mouseDown (const juce::MouseEvent& e)
{
    switch (zoneAt (e.getPosition()))
    {
        case Zone::prev: step (-1); break;
        case Zone::next: step (1);  break;
        case Zone::name: showMenu(); break;
        case Zone::save: askSaveName(); break;
        case Zone::dice: rollDice(); break;
        default: break;
    }
}

void PresetBar::rollDice()
{
    processor.presets.randomise();
    diceSpin = 1.0f;
    diceFace = juce::Random::getSystemRandom().nextInt ({ 1, 7 });
    refresh();
}

void PresetBar::step (int direction)
{
    auto& pm = processor.presets;
    const int numFactory = PresetManager::getNumFactoryPresets();
    const auto user = pm.getUserPresets();
    const int total = numFactory + user.size();

    // position actuelle dans la liste (usine puis utilisateur)
    int current = pm.getCurrentFactoryIndex();
    if (current < 0)
        for (int i = 0; i < user.size(); ++i)
            if (user[i].getFileNameWithoutExtension() == pm.getCurrentName())
                current = numFactory + i;

    int next = current < 0 ? (direction > 0 ? 0 : total - 1)
                           : (current + direction + total) % total;

    if (next < numFactory) pm.loadFactoryPreset (next);
    else                   pm.loadUserPreset (user[next - numFactory]);
    refresh();
}

void PresetBar::showMenu()
{
    auto& pm = processor.presets;
    juce::PopupMenu menu;

    menu.addSectionHeader ("PRESETS D'USINE");
    const auto& factory = PresetManager::getFactoryPresets();
    for (int i = 0; i < (int) factory.size(); ++i)
        menu.addItem (1 + i, factory[(size_t) i].name, true, pm.getCurrentFactoryIndex() == i);

    const auto user = pm.getUserPresets();
    menu.addSectionHeader ("MES PRESETS");
    if (user.isEmpty())
        menu.addItem (-1, "(aucun pour l'instant, clique sur SAVE)", false);
    for (int i = 0; i < user.size(); ++i)
        menu.addItem (100 + i, user[i].getFileNameWithoutExtension(), true,
                      pm.getCurrentFactoryIndex() < 0 && user[i].getFileNameWithoutExtension() == pm.getCurrentName());

    menu.addSeparator();
    menu.addItem (1000, "Enregistrer les réglages actuels...");
    menu.addItem (1002, "Random (tout au hasard)");
    menu.addItem (1001, "Ouvrir le dossier des presets");

    juce::Component::SafePointer<PresetBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (nameR.getWidth()),
                        [safe, user] (int result)
                        {
                            if (safe == nullptr || result <= 0) return;
                            auto& manager = safe->processor.presets;
                            if (result < 100)          manager.loadFactoryPreset (result - 1);
                            else if (result < 1000)    manager.loadUserPreset (user[result - 100]);
                            else if (result == 1000)   safe->askSaveName();
                            else if (result == 1001)   PresetManager::getUserFolder().revealToUser();
                            else if (result == 1002)   safe->rollDice();
                            safe->refresh();
                        });
}

void PresetBar::askSaveName()
{
    auto* w = new juce::AlertWindow ("Enregistrer le preset", "Nom du preset :", juce::MessageBoxIconType::NoIcon, this);
    const auto current = processor.presets.getCurrentName();
    w->addTextEditor ("name", current.startsWith ("Random") ? juce::String() : current);
    w->addButton ("Enregistrer", 1, juce::KeyPress (juce::KeyPress::returnKey));
    w->addButton ("Annuler", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    juce::Component::SafePointer<PresetBar> safe (this);
    w->enterModalState (true, juce::ModalCallbackFunction::create ([safe, w] (int result)
    {
        if (result == 1 && safe != nullptr)
        {
            safe->processor.presets.saveUserPreset (w->getTextEditorContents ("name"));
            safe->refresh();
        }
    }), true);
}

//==============================================================================
// LiquidMeter
//==============================================================================
void LiquidMeter::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (0.5f);
    const float corner = r.getWidth() * 0.5f;

    const float level = source.load();
    const float dB    = juce::Decibels::gainToDecibels (level, -60.0f);
    const float norm  = juce::jlimit (0.0f, 1.0f, juce::jmap (dB, -60.0f, 6.0f, 0.0f, 1.0f));
    smoothed += (norm - smoothed) * (norm > smoothed ? 0.6f : 0.15f);

    g.setColour (juce::Colour (0xff03090d));
    g.fillRoundedRectangle (r, corner);

    {
        juce::Graphics::ScopedSaveState save (g);
        juce::Path clip;
        clip.addRoundedRectangle (r.reduced (1.5f), corner - 1.5f);
        g.reduceClipRegion (clip);
        const auto topCol = dB > 0.0f ? Ui::coral : Ui::aqua;
        Ui::drawLiquid (g, r.reduced (1.5f), smoothed, Ui::now(), r.getX(), topCol.withAlpha (0.9f),
                        Ui::aquaDeep.withAlpha (0.4f), 1.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}

//==============================================================================
// WaveDisplay
//==============================================================================
WaveDisplay::WaveDisplay (FlowFilterAudioProcessor& p) : processor (p), state (p.apvts)
{
    setMouseCursor (juce::MouseCursor::UpDownLeftRightResizeCursor);
}

juce::Rectangle<float> WaveDisplay::plotArea() const
{
    auto r = getLocalBounds().toFloat().reduced (10.0f, 12.0f);
    r.removeFromBottom (8.0f);   // place pour les fréquences
    return r;
}

float WaveDisplay::freqToX (double f) const
{
    const auto p = plotArea();
    const double frac = juce::jlimit (0.0, 1.0, std::log (f / FilterBank::lowestHz)
                                                / std::log (FilterBank::highestHz / FilterBank::lowestHz));
    return p.getX() + (float) frac * p.getWidth();
}

double WaveDisplay::xToFreq (float x) const
{
    const auto p = plotArea();
    const double frac = (double) ((x - p.getX()) / p.getWidth());
    if (frac <= 0.0) return 20.0;      // tout à gauche -> zone ouverte vers le grave
    if (frac >= 1.0) return 20000.0;   // tout à droite -> zone ouverte vers l'aigu
    return FilterBank::lowestHz * std::pow (FilterBank::highestHz / FilterBank::lowestHz, frac);
}

WaveDisplay::DragMode WaveDisplay::hitTest (juce::Point<float> pt) const
{
    const float xl = freqToX (state.getRawParameterValue (ParamIDs::areaLow)->load());
    const float xh = freqToX (state.getRawParameterValue (ParamIDs::areaHigh)->load());
    if (std::abs (pt.x - xl) < 7.0f) return DragMode::areaLow;
    if (std::abs (pt.x - xh) < 7.0f) return DragMode::areaHigh;
    return DragMode::wave;
}

void WaveDisplay::setParam (const char* id, float value)
{
    if (auto* p = state.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (value));
}

void WaveDisplay::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (hitTest (e.position) == DragMode::wave ? juce::MouseCursor::UpDownLeftRightResizeCursor
                                                          : juce::MouseCursor::LeftRightResizeCursor);
}

void WaveDisplay::mouseDown (const juce::MouseEvent& e)
{
    dragMode = hitTest (e.position);
    dragStartOffset  = state.getRawParameterValue (ParamIDs::offset)->load();
    dragStartGravity = state.getRawParameterValue (ParamIDs::gravity)->load();

    auto begin = [this] (const char* id) { if (auto* p = state.getParameter (id)) p->beginChangeGesture(); };
    if (dragMode == DragMode::wave)     { begin (ParamIDs::offset); begin (ParamIDs::gravity); }
    if (dragMode == DragMode::areaLow)  begin (ParamIDs::areaLow);
    if (dragMode == DragMode::areaHigh) begin (ParamIDs::areaHigh);
}

void WaveDisplay::mouseDrag (const juce::MouseEvent& e)
{
    const auto p = plotArea();

    if (dragMode == DragMode::wave)
    {
        float off = dragStartOffset + (float) e.getDistanceFromDragStartX() / p.getWidth();
        off -= std::floor (off);   // la vague est cyclique
        setParam (ParamIDs::offset, off);
        setParam (ParamIDs::gravity, juce::jlimit (0.0f, 1.0f, dragStartGravity - (float) e.getDistanceFromDragStartY() / p.getHeight()));
    }
    else if (dragMode == DragMode::areaLow)
    {
        const float hi = state.getRawParameterValue (ParamIDs::areaHigh)->load();
        setParam (ParamIDs::areaLow, (float) juce::jmin ((double) hi, xToFreq (e.position.x)));
    }
    else if (dragMode == DragMode::areaHigh)
    {
        const float lo = state.getRawParameterValue (ParamIDs::areaLow)->load();
        setParam (ParamIDs::areaHigh, (float) juce::jmax ((double) lo, xToFreq (e.position.x)));
    }
}

void WaveDisplay::mouseUp (const juce::MouseEvent&)
{
    auto end = [this] (const char* id) { if (auto* p = state.getParameter (id)) p->endChangeGesture(); };
    if (dragMode == DragMode::wave)     { end (ParamIDs::offset); end (ParamIDs::gravity); }
    if (dragMode == DragMode::areaLow)  end (ParamIDs::areaLow);
    if (dragMode == DragMode::areaHigh) end (ParamIDs::areaHigh);
    dragMode = DragMode::none;
}

void WaveDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const auto mode = hitTest (e.position);
    if (mode == DragMode::areaLow)       setParam (ParamIDs::areaLow, 20.0f);
    else if (mode == DragMode::areaHigh) setParam (ParamIDs::areaHigh, 20000.0f);
    else { setParam (ParamIDs::offset, 0.0f); setParam (ParamIDs::gravity, 0.5f); }
}

void WaveDisplay::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const float t = Ui::now();

    // --- écran : verre sombre, légèrement transparent sur le fond animé
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xd0061a22), 0.0f, bounds.getY(),
                                             juce::Colour (0xe0010608), 0.0f, bounds.getBottom(), false));
    g.fillRoundedRectangle (bounds, 10.0f);

    const auto p = plotArea();
    auto dbToY = [&p] (float dB)
    {
        const float n = juce::jlimit (0.0f, 1.0f, juce::jmap (dB, Ui::minDb, Ui::maxDb, 0.0f, 1.0f));
        return p.getBottom() - n * p.getHeight();
    };

    // --- grille discrète
    g.setColour (juce::Colours::white.withAlpha (0.045f));
    for (float dB : { 0.0f, -20.0f, -40.0f })
        g.drawHorizontalLine ((int) dbToY (dB), p.getX(), p.getRight());
    for (double f : { 100.0, 1000.0, 10000.0 })
        g.drawVerticalLine ((int) freqToX (f), p.getY(), p.getBottom());

    g.setFont (Ui::font (8.5f, true, 0.1f));
    g.setColour (Ui::dim.withAlpha (0.6f));
    const std::pair<double, const char*> labels[] = { { 100.0, "100" }, { 1000.0, "1K" }, { 10000.0, "10K" } };
    for (const auto& [f, label] : labels)
        g.drawText (label, juce::Rectangle<float> (freqToX (f) - 20.0f, p.getBottom() + 2.0f, 40.0f, 10.0f), juce::Justification::centred);

    // --- bulles qui remontent (vitesse liée au niveau de sortie)
    const float energy = juce::jlimit (0.0f, 1.0f, processor.outputLevel.load() * 2.0f);
    for (int i = 0; i < 18; ++i)
    {
        const float fi    = (float) i;
        const float speed = 0.04f + 0.06f * Ui::hash (fi + 3.1f) + energy * 0.08f;
        const float life  = std::fmod (t * speed + Ui::hash (fi + 7.7f), 1.0f);
        const float bx    = p.getX() + p.getWidth() * Ui::hash (fi) + 4.0f * std::sin (t * 1.3f + fi);
        const float by    = p.getBottom() - life * p.getHeight();
        const float rad   = 0.8f + 1.8f * Ui::hash (fi + 1.3f);
        g.setColour (Ui::aqua.withAlpha (0.18f * std::sin (life * juce::MathConstants<float>::pi)));
        g.drawEllipse (bx - rad, by - rad, rad * 2.0f, rad * 2.0f, 0.8f);
    }

    // --- 64 bandes : fines colonnes, gauche = aqua, droite = corail
    const int numBands = processor.displayNumBands.load();
    const float colW = juce::jmax (1.2f, p.getWidth() / (float) numBands * 0.42f);

    for (int b = 0; b < numBands; ++b)
    {
        const float gain = processor.displayBands[(size_t) b].load (std::memory_order_relaxed);
        const float y    = dbToY (juce::Decibels::gainToDecibels (gain, -100.0f));
        const float x    = freqToX (FlowFilterAudioProcessor::bandFrequency (b, numBands));
        const auto  col  = (b % 2 == 0) ? Ui::aqua : Ui::coral;

        juce::Rectangle<float> bar (x - colW * 0.5f, y, colW, p.getBottom() - y);
        g.setGradientFill (juce::ColourGradient (col.withAlpha (0.55f), 0.0f, y, col.withAlpha (0.02f), 0.0f, p.getBottom(), false));
        g.fillRect (bar);
        g.setColour (col);
        g.fillRect (bar.withHeight (1.5f));
    }

    // --- surface de la vague : 256 points calculés avec le même moteur que l'audio
    const float depthDb = state.getRawParameterValue (ParamIDs::depth)->load();
    displayWave.setState (processor.displayPhase.load (std::memory_order_relaxed), processor.getDisplayWaveParams());

    juce::Path surface;
    for (int i = 0; i < curvePoints; ++i)
    {
        const float frac = (float) i / (float) (curvePoints - 1);
        const float a    = displayWave.valueAt (frac);
        const float x    = p.getX() + frac * p.getWidth();
        const float y    = dbToY (-depthDb * (1.0f - a));
        if (i == 0) surface.startNewSubPath (x, y);
        else        surface.lineTo (x, y);
    }

    juce::Path body (surface);
    body.lineTo (p.getRight(), p.getBottom());
    body.lineTo (p.getX(), p.getBottom());
    body.closeSubPath();
    g.setGradientFill (juce::ColourGradient (Ui::aqua.withAlpha (0.30f), 0.0f, p.getY(),
                                             Ui::aquaDeep.withAlpha (0.02f), 0.0f, p.getBottom(), false));
    g.fillPath (body);

    g.setColour (Ui::aqua.withAlpha (0.18f));
    g.strokePath (surface, juce::PathStrokeType (5.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xffe8fffc));
    g.strokePath (surface, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // --- zone filtrée (Area) : extérieur voilé + poignées
    const float lo = state.getRawParameterValue (ParamIDs::areaLow)->load();
    const float hi = state.getRawParameterValue (ParamIDs::areaHigh)->load();
    const float xl = freqToX (lo), xh = freqToX (hi);

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    if (xl > p.getX())     g.fillRect (juce::Rectangle<float> (p.getX() - 4.0f, p.getY(), xl - p.getX() + 4.0f, p.getHeight()));
    if (xh < p.getRight()) g.fillRect (juce::Rectangle<float> (xh, p.getY(), p.getRight() - xh + 4.0f, p.getHeight()));

    for (float x : { xl, xh })
    {
        g.setColour (Ui::coral.withAlpha (0.25f));
        g.fillRect (juce::Rectangle<float> (x - 1.5f, p.getY(), 3.0f, p.getHeight()));
        g.setColour (Ui::coral);
        g.drawLine (x, p.getY(), x, p.getBottom(), 1.0f);
        g.fillEllipse (juce::Rectangle<float> (7.0f, 7.0f).withCentre ({ x, p.getY() }));
    }

    // --- infos pendant le glisser
    if (dragMode == DragMode::wave)
    {
        const auto txt = "OFFSET " + juce::String (juce::roundToInt (state.getRawParameterValue (ParamIDs::offset)->load() * 100.0f)) + "%   "
                       + "GRAVITY " + juce::String (juce::roundToInt (state.getRawParameterValue (ParamIDs::gravity)->load() * 100.0f)) + "%";
        g.setColour (Ui::text);
        g.setFont (Ui::font (9.5f, true, 0.1f));
        g.drawText (txt, p.reduced (6.0f, 4.0f), juce::Justification::topRight);
    }
    else if (dragMode != DragMode::none)
    {
        const float f = dragMode == DragMode::areaLow ? lo : hi;
        const auto txt = f >= 1000.0f ? juce::String (f / 1000.0f, 1) + " kHz" : juce::String (juce::roundToInt (f)) + " Hz";
        g.setColour (Ui::coral);
        g.setFont (Ui::font (9.5f, true, 0.1f));
        g.drawText (txt, p.reduced (6.0f, 4.0f), juce::Justification::topRight);
    }

    // liseré
    g.setColour (juce::Colours::white.withAlpha (0.07f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);
}

//==============================================================================
// Editor
//==============================================================================
static constexpr int editorW = 892, editorH = 292;

Knob* FlowFilterAudioProcessorEditor::addKnob (juce::OwnedArray<Knob>& page, const juce::String& id, const juce::String& name,
                                               const juce::String& tooltip, bool bipolar)
{
    auto* k = page.add (new Knob (state, id, name, bipolar, tooltip));
    addChildComponent (k);
    return k;
}

FlowFilterAudioProcessorEditor::FlowFilterAudioProcessorEditor (FlowFilterAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), state (p.apvts), display (p), presetBar (p),
      syncButton    (p.apvts, ParamIDs::sync,    "SYNC",   PillToggle::Icon::sync),
      reverseButton (p.apvts, ParamIDs::reverse, "REV",    PillToggle::Icon::reverse),
      freezeButton  (p.apvts, ParamIDs::freeze,  "FREEZE", PillToggle::Icon::freeze),
      tabs ({ "MAIN", "ADVANCED", "MOD" }),
      inMeter (p.inputLevel), outMeter (p.outputLevel)
{
    setLookAndFeel (&look);
    namespace id = ParamIDs;

    addAndMakeVisible (display);
    addAndMakeVisible (presetBar);

    syncButton.setTooltip ("Vitesse libre (Hz) ou calée sur le tempo du projet");
    reverseButton.setTooltip ("Inverse le sens de déplacement de la vague");
    freezeButton.setTooltip ("Fige la vague à sa position actuelle");
    addAndMakeVisible (syncButton);
    addAndMakeVisible (reverseButton);
    addAndMakeVisible (freezeButton);

    tabs.onChange = [this] (int t) { showTab (t); };
    addAndMakeVisible (tabs);

    // bouton « i » : panneau À propos (licence, code source, confidentialité, composants tiers)
    infoButton.onClick = [this] { openAbout(); };
    addAndMakeVisible (infoButton);

    // --- entrée / sortie
    inKnob  = std::make_unique<Knob> (state, id::inGain,  "Input",  false, "Niveau avant le banc de filtres");
    outKnob = std::make_unique<Knob> (state, id::outGain, "Output", false, "Niveau de sortie");
    mixKnob = std::make_unique<Knob> (state, id::mix,     "Mix",    false, "Mélange signal sec / filtré");
    addAndMakeVisible (*inKnob);
    addAndMakeVisible (*outKnob);
    addAndMakeVisible (*mixKnob);
    addAndMakeVisible (inMeter);
    addAndMakeVisible (outMeter);

    // --- MAIN : la vague
    rateKnob     = addKnob (mainPage, id::rate,     "Rate",     "Vitesse de la vague (Hz)");
    syncRateKnob = addKnob (mainPage, id::syncDiv,  "Rate",     "Durée d'un cycle, calée sur le tempo");
    addKnob (mainPage, id::multiply, "Multiply", "Nombre de vagues sur le spectre");
    addKnob (mainPage, id::ripple,   "Ripple",   "Contre-vague : rend le mouvement irrégulier, comme de l'eau");
    addKnob (mainPage, id::scan,     "Scan",     "0 = la vague se déplace, 100 % = elle ondule sur place");
    addKnob (mainPage, id::offset,   "Offset",   "Décale la vague (Rate à 0 = contrôle manuel)");
    addKnob (mainPage, id::invert,   "Invert",   "Profondeur signée : négatif = vague inversée", true);
    addKnob (mainPage, id::gravity,  "Gravity",  "Tire la vague vers le bas ou la fait flotter");
    addKnob (mainPage, id::depth,    "Depth",    "Atténuation maximale des bandes (dB)");

    // --- ADVANCED : filtres et stéréo
    addKnob (advancedPage, id::areaLow,   "Area Lo",   "Début de la zone modulée (aussi à la souris sur l'écran)");
    addKnob (advancedPage, id::areaHigh,  "Area Hi",   "Fin de la zone modulée");
    addKnob (advancedPage, id::areaGain,  "Area Gain", "Niveau fixe des bandes hors zone");
    addKnob (advancedPage, id::areaSlope, "Slope",     "Transition douce aux bords de la zone (octaves)");
    addKnob (advancedPage, id::release,   "Release",   "Temps de glissement des bandes");
    addKnob (advancedPage, id::width,     "Width",     "Largeur stéréo (0 = mono)");
    addKnob (advancedPage, id::resonance, "Reso",      "Finesse des 64 filtres");
    addKnob (advancedPage, id::analog,    "Analog",    "Dérive analogique des bandes + saturation douce");

    // --- MOD : un modulateur par cible
    const char* targets[] = { "OFFSET", "INVERT", "GRAVITY" };
    for (int i = 0; i < FlowFilterAudioProcessor::numMods; ++i)
    {
        auto& t = modTitles[(size_t) i];
        t.setText (targets[i], juce::dontSendNotification);
        t.setFont (Ui::font (9.5f, true, 0.18f));
        t.setColour (juce::Label::textColourId, Ui::aqua);
        t.setJustificationType (juce::Justification::centred);
        addChildComponent (t);

        auto& box = modShapeBoxes[(size_t) i];
        box.addItemList ({ "Sine", "Triangle", "Ramp", "Square", "Random" }, 1);
        box.setTooltip ("Forme du modulateur");
        addChildComponent (box);
        modShapeAtt[(size_t) i] = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, id::modShape (i), box);

        addKnob (modPage, id::modRate (i),  "Rate",  "Vitesse du modulateur");
        addKnob (modPage, id::modDepth (i), "Depth", "Quantité de modulation");
    }

    setSize (editorW, editorH);
    showTab (mainTab);
    startTimerHz (30);
}

FlowFilterAudioProcessorEditor::~FlowFilterAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void FlowFilterAudioProcessorEditor::showTab (int tab)
{
    const bool synced = state.getRawParameterValue (ParamIDs::sync)->load() > 0.5f;

    for (auto* k : mainPage)      k->setVisible (tab == mainTab);
    for (auto* k : advancedPage)  k->setVisible (tab == advancedTab);
    for (auto* k : modPage)       k->setVisible (tab == modTab);
    for (auto& b : modShapeBoxes) b.setVisible (tab == modTab);
    for (auto& l : modTitles)     l.setVisible (tab == modTab);

    if (tab == mainTab)
    {
        rateKnob->setVisible (! synced);
        syncRateKnob->setVisible (synced);
    }
    repaint();
}

void FlowFilterAudioProcessorEditor::timerCallback()
{
    if (tabs.getCurrent() == mainTab)
    {
        const bool synced = state.getRawParameterValue (ParamIDs::sync)->load() > 0.5f;
        if (rateKnob->isVisible() == synced)
        {
            rateKnob->setVisible (! synced);
            syncRateKnob->setVisible (synced);
        }
    }

    tabs.animate();
    presetBar.refresh();
    repaint();   // fond animé + liquides des boutons + écran
}

void FlowFilterAudioProcessorEditor::paintBackground (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const float t = Ui::now();

    // profondeur
    g.setGradientFill (juce::ColourGradient (Ui::bgTop, 0.0f, 0.0f, Ui::bgBottom, 0.0f, b.getBottom(), false));
    g.fillAll();

    // lumière qui dérive lentement en surface
    const float lx = b.getWidth() * (0.5f + 0.35f * std::sin (t * 0.07f));
    g.setGradientFill (juce::ColourGradient (Ui::aqua.withAlpha (0.10f), lx, -40.0f,
                                             Ui::aqua.withAlpha (0.0f), lx + 320.0f, 140.0f, true));
    g.fillRect (b);

    // courants : couches de vagues lentes et translucides
    for (int layer = 0; layer < 4; ++layer)
    {
        const float fl    = (float) layer;
        const float baseY = b.getHeight() * (0.38f + 0.15f * fl);
        const float amp   = 10.0f + 6.0f * fl;
        const float speed = 0.18f + 0.07f * fl;
        const float freq  = 0.0065f + 0.0022f * fl;

        juce::Path current;
        current.startNewSubPath (0.0f, b.getBottom());
        for (float x = 0.0f; x <= b.getWidth() + 8.0f; x += 8.0f)
        {
            const float y = baseY + amp * std::sin (x * freq + t * speed + fl * 1.9f)
                                  + amp * 0.4f * std::sin (x * freq * 2.7f - t * speed * 1.6f + fl);
            current.lineTo (x, y);
        }
        current.lineTo (b.getWidth() + 8.0f, b.getBottom());
        current.closeSubPath();

        g.setGradientFill (juce::ColourGradient (Ui::aqua.withAlpha (0.045f), 0.0f, baseY - amp,
                                                 Ui::aquaDeep.withAlpha (0.0f), 0.0f, b.getBottom(), false));
        g.fillPath (current);
    }

    // particules en suspension
    for (int i = 0; i < 40; ++i)
    {
        const float fi   = (float) i;
        const float life = std::fmod (t * (0.012f + 0.02f * Ui::hash (fi + 2.0f)) + Ui::hash (fi + 9.0f), 1.0f);
        const float px   = b.getWidth() * Ui::hash (fi) + 10.0f * std::sin (t * 0.4f + fi);
        const float py   = b.getBottom() - life * (b.getHeight() + 20.0f);
        const float r    = 0.6f + 1.2f * Ui::hash (fi + 4.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f * std::sin (life * juce::MathConstants<float>::pi)));
        g.fillEllipse (px - r, py - r, r * 2.0f, r * 2.0f);
    }
}

void FlowFilterAudioProcessorEditor::paint (juce::Graphics& g)
{
    paintBackground (g);

    // panneaux de verre
    Ui::drawGlassPanel (g, leftPanel.toFloat());
    Ui::drawGlassPanel (g, pagePanel.toFloat());
    Ui::drawGlassPanel (g, rightPanel.toFloat());

    // logo : petite vague animée + "flow FILTER"
    const float lx = 20.0f, ly = 22.0f;
    juce::Path logoWave;
    for (int i = 0; i <= 24; ++i)
    {
        const float x = lx + (float) i;
        const float y = ly + 3.5f * std::sin ((float) i * 0.42f + Ui::now() * 2.0f);
        if (i == 0) logoWave.startNewSubPath (x, y); else logoWave.lineTo (x, y);
    }
    g.setColour (Ui::aqua);
    g.strokePath (logoWave, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (juce::Colours::white);
    g.setFont (Ui::font (18.0f, true, -0.02f));
    g.drawText ("flow", 52, 10, 50, 24, juce::Justification::centredLeft);
    g.setColour (Ui::aqua);
    g.setFont (Ui::font (9.0f, true, 0.45f));
    g.drawText ("FILTER", 96, 8, 70, 16, juce::Justification::centredLeft);
    // le label : Flow Filter est une création GRVTY Records
    g.setColour (Ui::dim);
    g.setFont (Ui::font (6.5f, true, 0.3f));
    g.drawText ("GRVTY RECORDS", 96, 22, 120, 12, juce::Justification::centredLeft);

    // numéro de version : permet de vérifier d'un coup d'œil qu'Ableton charge bien la dernière compilation
    g.setColour (Ui::dim.withAlpha (0.7f));
    g.setFont (Ui::font (8.5f, false, 0.05f));
    g.drawText ("v" JucePlugin_VersionString, editorW - 104, 13, 66, 20, juce::Justification::centredRight);
}

void FlowFilterAudioProcessorEditor::openAbout()
{
    if (about != nullptr)
        return;
    juce::Component::SafePointer<FlowFilterAudioProcessorEditor> safe (this);
    about = std::make_unique<AboutPanel> ("Flow Filter", JucePlugin_VersionString, true, [safe]
    {
        // fermé depuis un de ses propres clics : on le détruit juste après
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->about.reset(); });
    });
    addAndMakeVisible (*about);
    about->setBounds (getLocalBounds());
}

void FlowFilterAudioProcessorEditor::resized()
{
    const int top = 74, bottom = editorH - 10;

    leftPanel  = { 10,  top, 64,  bottom - top };
    pagePanel  = { 510, top, 300, bottom - top };
    rightPanel = { 818, top, 64,  bottom - top };

    // --- ligne 1 : presets (centrés), bouton « i » à droite du numéro de version
    presetBar.setBounds (editorW / 2 - 200, 9, 400, 26);
    infoButton.setBounds (editorW - 32, 13, 20, 20);
    if (about != nullptr) about->setBounds (getLocalBounds());

    // --- ligne 2 : Sync / Rev / Freeze au-dessus de l'écran, onglets au-dessus des pages
    syncButton.setBounds    (82,  43, 74, 22);
    reverseButton.setBounds (160, 43, 62, 22);
    freezeButton.setBounds  (226, 43, 86, 22);
    tabs.setBounds (pagePanel.getX(), 41, pagePanel.getWidth(), 26);

    // --- colonne entrée
    inKnob->setBounds (leftPanel.getX() + 2, top + 8, 60, 78);
    inMeter.setBounds (leftPanel.getCentreX() - 5, top + 96, 10, bottom - top - 106);

    // --- écran
    display.setBounds (82, top, 420, bottom - top);

    // --- pages (grille 4 x 2)
    const int px = pagePanel.getX() + 4, py = top + 8, cellW = 73, cellH = 100;
    auto cell = [&] (int col, int row) { return juce::Rectangle<int> (px + col * cellW, py + row * cellH, cellW, cellH - 8); };

    // MAIN : [Rate, Multiply, Ripple, Scan] [Offset, Invert, Gravity, Depth]
    rateKnob->setBounds (cell (0, 0));
    syncRateKnob->setBounds (cell (0, 0));
    for (int i = 2; i < mainPage.size(); ++i)
    {
        const int slot = i - 1;
        mainPage[i]->setBounds (cell (slot % 4, slot / 4));
    }

    // ADVANCED : [Area Lo, Area Hi, Area Gain, Slope] [Release, Width, Reso, Analog]
    for (int i = 0; i < advancedPage.size(); ++i)
        advancedPage[i]->setBounds (cell (i % 4, i / 4));

    // MOD : 3 colonnes (titre, forme, Rate + Depth)
    const int colW = 98;
    for (int i = 0; i < FlowFilterAudioProcessor::numMods; ++i)
    {
        const int x = pagePanel.getX() + 2 + i * colW;
        modTitles[(size_t) i].setBounds (x, py + 2, colW, 14);
        modShapeBoxes[(size_t) i].setBounds (x + 10, py + 22, colW - 20, 20);
        modPage[i * 2]    ->setBounds (x,            py + 60, colW / 2, 96);
        modPage[i * 2 + 1]->setBounds (x + colW / 2, py + 60, colW / 2, 96);
    }

    // --- colonne sortie
    outKnob->setBounds (rightPanel.getX() + 2, top + 8,  60, 78);
    mixKnob->setBounds (rightPanel.getX() + 2, top + 90, 60, 78);
    outMeter.setBounds (rightPanel.getCentreX() - 5, top + 176, 10, bottom - top - 186);
}
