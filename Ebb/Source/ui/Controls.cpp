// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "Controls.h"

//==============================================================================
// ChoiceControl
//==============================================================================
ChoiceControl::ChoiceControl (juce::AudioProcessorValueTreeState& st, const juce::String& paramID, const juce::String& name)
{
    title.setText (Ui::upper (name), juce::dontSendNotification);
    title.setFont (Ui::font (8.6f, true, 0.1f));
    title.setColour (juce::Label::textColourId, Ui::dim);
    title.setJustificationType (juce::Justification::centred);
    title.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (title);

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (st.getParameter (paramID)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (st, paramID, box);
}

void ChoiceControl::resized()
{
    auto r = getLocalBounds();
    title.setBounds (r.removeFromTop (12));
    r.removeFromTop (3);
    box.setBounds (r.removeFromTop (20).reduced (2, 0));
}

//==============================================================================
// PillButton
//==============================================================================
namespace
{
    void drawIcon (juce::Graphics& g, PillButton::Icon icon, juce::Point<float> c, float s)
    {
        using Icon = PillButton::Icon;
        const juce::PathStrokeType stroke (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        auto& p = Ui::scratchPath (5);
        switch (icon)
        {
            case Icon::pencil:
                p.startNewSubPath (c.x - s, c.y + s);
                p.lineTo (c.x - s + 1.2f, c.y + s - 3.0f);
                p.lineTo (c.x + s - 1.5f, c.y - s + 0.5f);
                p.lineTo (c.x + s, c.y - s + 2.0f);
                p.lineTo (c.x - s + 3.0f, c.y + s - 1.2f);
                p.closeSubPath();
                g.strokePath (p, stroke);
                break;
            case Icon::undo:
            case Icon::redo:
            {
                const float dir = icon == Icon::undo ? -1.0f : 1.0f;
                p.addCentredArc (c.x, c.y + 1.0f, s, s * 0.8f, 0.0f, dir * 1.2f, -dir * 1.6f, true);
                g.strokePath (p, stroke);
                auto& h = Ui::scratchPath (6);
                const juce::Point<float> tip (c.x + dir * s * 0.93f, c.y + 1.0f - s * 0.28f);
                h.addTriangle (tip.x + dir * 2.0f, tip.y - 2.6f, tip.x + dir * 2.4f, tip.y + 2.0f, tip.x - dir * 1.8f, tip.y + 0.2f);
                g.fillPath (h);
                break;
            }
            case Icon::mirror:
                p.startNewSubPath (c.x, c.y - s);
                p.lineTo (c.x, c.y + s);
                g.strokePath (p, juce::PathStrokeType (1.0f));
                p.clear();
                p.addTriangle (c.x - 1.5f, c.y - s * 0.6f, c.x - 1.5f, c.y + s * 0.6f, c.x - s - 1.0f, c.y + s * 0.6f);
                p.addTriangle (c.x + 1.5f, c.y - s * 0.6f, c.x + 1.5f, c.y + s * 0.6f, c.x + s + 1.0f, c.y + s * 0.6f);
                g.strokePath (p, stroke);
                break;
            case Icon::invert:
                p.startNewSubPath (c.x - s, c.y);
                p.lineTo (c.x + s, c.y);
                g.strokePath (p, juce::PathStrokeType (1.0f));
                p.clear();
                p.addTriangle (c.x - s * 0.6f, c.y - 1.5f, c.x + s * 0.6f, c.y - 1.5f, c.x + s * 0.6f, c.y - s - 1.0f);
                p.addTriangle (c.x - s * 0.6f, c.y + 1.5f, c.x + s * 0.6f, c.y + 1.5f, c.x + s * 0.6f, c.y + s + 1.0f);
                g.strokePath (p, stroke);
                break;
            case Icon::clear:
                p.startNewSubPath (c.x - s * 0.7f, c.y - s * 0.7f);
                p.lineTo (c.x + s * 0.7f, c.y + s * 0.7f);
                p.startNewSubPath (c.x + s * 0.7f, c.y - s * 0.7f);
                p.lineTo (c.x - s * 0.7f, c.y + s * 0.7f);
                g.strokePath (p, stroke);
                break;
            case Icon::save:
                p.addRoundedRectangle (juce::Rectangle<float> (s * 2.0f, s * 2.0f).withCentre (c), 1.5f);
                p.addRectangle (juce::Rectangle<float> (s, s * 0.6f).withCentre ({ c.x, c.y - s * 0.6f }));
                g.strokePath (p, juce::PathStrokeType (1.2f));
                break;
            case Icon::folder:
            {
                const float w = s * 2.2f, h = s * 1.7f, x = c.x - w * 0.5f, y = c.y - h * 0.5f;
                p.startNewSubPath (x, y + h);
                p.lineTo (x, y);
                p.lineTo (x + w * 0.35f, y);
                p.lineTo (x + w * 0.45f, y + h * 0.2f);
                p.lineTo (x + w, y + h * 0.2f);
                p.lineTo (x + w, y + h);
                p.closeSubPath();
                g.strokePath (p, stroke);
                break;
            }
            case Icon::list:
                for (int k = -1; k <= 1; ++k)
                {
                    p.startNewSubPath (c.x - s, c.y + (float) k * s * 0.7f);
                    p.lineTo (c.x + s, c.y + (float) k * s * 0.7f);
                }
                g.strokePath (p, stroke);
                break;
            case Icon::none:
            default:
                break;
        }
    }
}

void PillButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = r.getHeight() * 0.5f;
    const bool enabled = isEnabled();
    if (lit)
    {
        g.setColour (accent.withAlpha (0.14f));
        g.fillRoundedRectangle (r.expanded (1.0f), corner + 1.0f);
        Ui::setVerticalGradient (g, 3, accent.withAlpha (0.38f), r.getY(), Ui::aquaDeep.withAlpha (0.25f), r.getBottom());
        g.fillRoundedRectangle (r, corner);
    }
    g.setColour (juce::Colours::white.withAlpha (down ? 0.14f : (highlighted && enabled ? 0.09f : 0.045f)));
    g.fillRoundedRectangle (r, corner);
    g.setColour (accent.withAlpha (lit ? 0.95f : (highlighted && enabled ? 0.9f : (enabled ? 0.45f : 0.15f))));
    g.drawRoundedRectangle (r, corner, 1.0f);

    const auto col = lit || (highlighted && enabled) ? juce::Colours::white : (enabled ? accent : Ui::dim.withAlpha (0.5f));
    g.setColour (col);
    auto textArea = r.toNearestInt().reduced (6, 0);
    if (icon != Icon::none)
    {
        const bool iconOnly = getButtonText().isEmpty();
        const float iconX = iconOnly ? r.getCentreX() : r.getX() + corner + 4.0f;
        drawIcon (g, icon, { iconX, r.getCentreY() }, 4.2f);
        if (! iconOnly) textArea = textArea.withTrimmedLeft (juce::roundToInt (corner + 6.0f));
    }
    if (getButtonText().isNotEmpty())
    {
        g.setFont (Ui::font (fontSize, true, 0.1f));
        g.drawFittedText (getButtonText(), textArea, juce::Justification::centred, 1, 0.8f);
    }
}

//==============================================================================
// PillGroup
//==============================================================================
PillGroup::PillGroup (juce::RangedAudioParameter& p, juce::StringArray l, bool v)
    : labels (std::move (l)), vertical (v),
      attachment (p, [this] (float value) { selected = juce::roundToInt (value); repaint(); })
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

juce::Rectangle<float> PillGroup::cell (int i) const
{
    const int n = labels.size();
    const auto b = getLocalBounds().toFloat();
    const float gap = 3.0f;
    if (vertical)
    {
        const float h = (b.getHeight() - gap * (float) (n - 1)) / (float) n;
        return { b.getX(), b.getY() + (float) i * (h + gap), b.getWidth(), h };
    }
    const float w = (b.getWidth() - gap * (float) (n - 1)) / (float) n;
    return { b.getX() + (float) i * (w + gap), b.getY(), w, b.getHeight() };
}

int PillGroup::indexAt (juce::Point<float> p) const
{
    for (int i = 0; i < labels.size(); ++i)
        if (cell (i).expanded (1.5f).contains (p))
            return i;
    return -1;
}

void PillGroup::paint (juce::Graphics& g)
{
    for (int i = 0; i < labels.size(); ++i)
    {
        const auto r = cell (i).reduced (0.5f);
        const bool on = i == selected, hot = i == hovered;
        const float c = juce::jmin (r.getHeight(), r.getWidth()) * 0.5f;
        if (on)
        {
            g.setColour (Ui::aqua.withAlpha (0.14f));
            g.fillRoundedRectangle (r.expanded (1.0f), c + 1.0f);
            Ui::setVerticalGradient (g, 3, Ui::aqua.withAlpha (0.38f), r.getY(), Ui::aquaDeep.withAlpha (0.25f), r.getBottom());
        }
        else
            g.setColour (juce::Colours::white.withAlpha (hot ? 0.08f : 0.04f));
        g.fillRoundedRectangle (r, c);
        g.setColour (on ? Ui::aqua.withAlpha (0.9f) : juce::Colours::white.withAlpha (hot ? 0.22f : 0.12f));
        g.drawRoundedRectangle (r, c, 1.0f);
        g.setColour (on ? juce::Colours::white : Ui::text.withAlpha (hot ? 0.95f : 0.72f));
        g.setFont (Ui::font (fontSize, true, 0.06f));
        g.drawFittedText (labels[i], r.toNearestInt().reduced (2, 0), juce::Justification::centred, 1, 0.75f);
    }
}

void PillGroup::clickIndex (int i)
{
    if (i < 0 || i >= labels.size())
        return;
    attachment.setValueAsCompleteGesture ((float) i);
}

void PillGroup::mouseDown (const juce::MouseEvent& e) { clickIndex (indexAt (e.position)); }

void PillGroup::mouseMove (const juce::MouseEvent& e)
{
    const int h = indexAt (e.position);
    if (h != hovered) { hovered = h; repaint(); }
}

void PillGroup::mouseExit (const juce::MouseEvent&) { hovered = -1; repaint(); }

//==============================================================================
// PillChooser
//==============================================================================
PillChooser::PillChooser (juce::StringArray l) : labels (std::move (l))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void PillChooser::paint (juce::Graphics& g)
{
    const int n = labels.size();
    const float gap = 2.0f;
    const float w = ((float) getWidth() - gap * (float) (n - 1)) / (float) n;
    for (int i = 0; i < n; ++i)
    {
        const auto r = juce::Rectangle<float> ((float) i * (w + gap), 0.0f, w, (float) getHeight()).reduced (0.5f);
        const bool on = i == selected;
        g.setColour (on ? Ui::aqua.withAlpha (0.3f) : juce::Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (on ? Ui::aqua : juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        g.setColour (on ? juce::Colours::white : Ui::text.withAlpha (0.72f));
        g.setFont (Ui::font (fontSize, true, 0.04f));
        g.drawFittedText (labels[i], r.toNearestInt(), juce::Justification::centred, 1, 0.7f);
    }
}

void PillChooser::mouseDown (const juce::MouseEvent& e)
{
    const int i = juce::jlimit (0, labels.size() - 1, (int) (e.position.x / ((float) getWidth() / (float) labels.size())));
    selected = i;
    repaint();
    if (onChange) onChange (i);
}
