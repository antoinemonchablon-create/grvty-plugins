// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "LiquidUI.h"

// Composants graphiques repris de Flow Filter (même univers visuel), adaptés pour Undertow.
//==============================================================================

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
    auto& track = Ui::scratchPath (2);
    track.addCentredArc (c.x, c.y, radius, radius, 0.0f, startAngle, endAngle, true);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.strokePath (track, juce::PathStrokeType (2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
    if (std::abs (angle - from) > 0.001f)
    {
        auto& value = Ui::scratchPath (3);
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
        auto& clip = Ui::scratchPath (4);
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

    auto& chevron = Ui::scratchPath (2);
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
        auto& hand = Ui::scratchPath (5);
        hand.startNewSubPath (ic.x, ic.y - s * 0.65f);
        hand.lineTo (ic);
        hand.lineTo (ic.x + s * 0.55f, ic.y + s * 0.2f);
        g.strokePath (hand, stroke);
    }
    else if (icon == Icon::reverse)  // flèche circulaire
    {
        auto& arc = Ui::scratchPath (5);
        arc.addCentredArc (ic.x, ic.y, s, s, 0.0f, 0.6f, 5.2f, true);
        g.strokePath (arc, stroke);
        const auto tip = ic.getPointOnCircumference (s, 5.2f);
        auto& head = Ui::scratchPath (6);
        head.addTriangle (tip.x - 2.2f, tip.y - 2.0f, tip.x + 2.2f, tip.y - 0.4f, tip.x - 0.2f, tip.y + 2.4f);
        g.fillPath (head);
    }
    else if (icon == Icon::freeze)   // flocon
    {
        auto& flake = Ui::scratchPath (5);
        for (int k = 0; k < 3; ++k)
        {
            const float a = juce::MathConstants<float>::pi * (float) k / 3.0f;
            flake.startNewSubPath (ic.getPointOnCircumference (s + 0.5f, a));
            flake.lineTo (ic.getPointOnCircumference (s + 0.5f, a + juce::MathConstants<float>::pi));
        }
        g.strokePath (flake, stroke);
    }

    else if (icon == Icon::wave)     // petite vague
    {
        auto& w = Ui::scratchPath (5);
        for (int k = 0; k <= 10; ++k)
        {
            const float x = ic.x - s + (float) k * (s * 0.2f);
            const float y = ic.y + 2.2f * std::sin ((float) k * 0.63f);
            if (k == 0) w.startNewSubPath (x, y); else w.lineTo (x, y);
        }
        g.strokePath (w, stroke);
    }
    else                             // point
    {
        g.fillEllipse (juce::Rectangle<float> (s * 1.2f, s * 1.2f).withCentre (ic));
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
        auto& clip = Ui::scratchPath (4);
        clip.addRoundedRectangle (r.reduced (1.5f), corner - 1.5f);
        g.reduceClipRegion (clip);
        const auto topCol = dB > 0.0f ? Ui::coral : Ui::aqua;
        Ui::drawLiquid (g, r.reduced (1.5f), smoothed, Ui::now(), r.getX(), topCol.withAlpha (0.9f),
                        Ui::aquaDeep.withAlpha (0.4f), 1.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (r, corner, 1.0f);
}
