// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "CurveStrip.h"
#include "../PluginProcessor.h"

namespace { juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); } }

CurveStrip::CurveStrip (EbbAudioProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

juce::Rectangle<float> CurveStrip::cellBounds (int i) const
{
    const float gap = 3.0f;
    const float extra = 10.0f;   // MA COURBE un peu plus large
    const float w = ((float) getWidth() - gap * (float) (numCells - 1) - extra) / (float) numCells;
    const float x = (float) i * (w + gap) + (i == numCells - 1 ? 0.0f : 0.0f);
    return { x, 0.0f, i == numCells - 1 ? w + extra : w, (float) getHeight() };
}

int CurveStrip::indexAt (juce::Point<float> p) const
{
    for (int i = 0; i < numCells; ++i)
        if (cellBounds (i).expanded (1.5f, 0.0f).contains (p))
            return i;
    return -1;
}

bool CurveStrip::tick()
{
    const auto s = processor.readSettings();
    const int sel = s.custom ? numCells - 1 : juce::jlimit (0, Ebb::numCurves - 1, s.curve);
    const uint32_t sv = processor.getShapeVersion();
    if (sel != selected || sv != shapeVersion)
    {
        selected = sel;
        shapeVersion = sv;
        repaint();
        return true;
    }
    return false;
}

void CurveStrip::drawThumb (juce::Graphics& g, juce::Rectangle<float> r, const float* table, bool selected, bool hot, bool custom)
{
    const auto accent = custom ? Ui::coral : Ui::aqua;
    g.setColour (selected ? accent.withAlpha (0.16f) : juce::Colours::white.withAlpha (hot ? 0.07f : 0.035f));
    g.fillRoundedRectangle (r, 5.0f);
    g.setColour (selected ? accent.withAlpha (0.95f) : juce::Colours::white.withAlpha (hot ? 0.22f : 0.09f));
    g.drawRoundedRectangle (r.reduced (0.5f), 5.0f, selected ? 1.3f : 1.0f);

    const auto plot = r.reduced (4.0f, 4.0f);
    auto& p = Ui::scratchPath (6);
    for (int i = 0; i <= 40; ++i)
    {
        const float x = (float) i / 40.0f;
        const float y = plot.getBottom() - (1.0f - Ebb::CurveTables::lookupTable (table, x)) * plot.getHeight();
        if (i == 0) p.startNewSubPath (plot.getX(), y); else p.lineTo (plot.getX() + x * plot.getWidth(), y);
    }
    g.setColour (selected ? accent : Ui::text.withAlpha (hot ? 0.8f : 0.5f));
    g.strokePath (p, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void CurveStrip::paint (juce::Graphics& g)
{
    const auto& tables = Ebb::CurveTables::get();
    for (int i = 0; i < numCells; ++i)
    {
        const bool custom = i == numCells - 1;
        drawThumb (g, cellBounds (i), custom ? processor.getShapeTable() : tables.table (i), i == selected, i == hovered, custom);
    }
    // repère « perso » sur la dernière vignette
    const auto c = cellBounds (numCells - 1);
    g.setColour (Ui::coral);
    g.fillEllipse (juce::Rectangle<float> (4.0f, 4.0f).withCentre ({ c.getRight() - 5.0f, c.getY() + 5.0f }));
}

void CurveStrip::mouseDown (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i >= 0 && onPick) onPick (i);
}

void CurveStrip::mouseMove (const juce::MouseEvent& e)
{
    const int i = indexAt (e.position);
    if (i != hovered)
    {
        hovered = i;
        setTooltip (i < 0 ? juce::String() : (i == numCells - 1 ? u8 ("Ma courbe (dessin\xc3\xa9" "e dans l'\xc3\xa9" "diteur)") : juce::String (Ebb::curveName (i))));
        repaint();
    }
}

void CurveStrip::mouseExit (const juce::MouseEvent&) { hovered = -1; repaint(); }

//==============================================================================
// ShapeSavePanel
//==============================================================================
ShapeSavePanel::ShapeSavePanel (const Ebb::ShapeData& s, const juce::String& suggestedName, std::function<void (bool, juce::String)> onDone)
    : shape (s), done (std::move (onDone))
{
    setWantsKeyboardFocus (true);
    editor.setFont (Ui::font (14.0f, true));
    editor.setJustification (juce::Justification::centredLeft);
    editor.setIndents (10, 6);
    editor.setColour (juce::TextEditor::backgroundColourId, juce::Colour (0xff04121a));
    editor.setColour (juce::TextEditor::textColourId, juce::Colours::white);
    editor.setColour (juce::TextEditor::highlightColourId, Ui::aqua.withAlpha (0.35f));
    editor.setColour (juce::TextEditor::highlightedTextColourId, juce::Colours::white);
    editor.setColour (juce::CaretComponent::caretColourId, Ui::aqua);
    editor.setColour (juce::TextEditor::outlineColourId, Ui::aqua.withAlpha (0.4f));
    editor.setColour (juce::TextEditor::focusedOutlineColourId, Ui::aqua);
    editor.setInputRestrictions (60);
    editor.setText (suggestedName, false);
    editor.onReturnKey = [this] { commit(); };
    editor.onEscapeKey = [this] { cancel(); };
    editor.onTextChange = [this] { updateHint(); };
    addAndMakeVisible (editor);
    updateHint();
}

void ShapeSavePanel::parentHierarchyChanged()
{
    if (isShowing())
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ShapeSavePanel> (this)]
        {
            if (safe != nullptr && safe->isShowing())
            {
                safe->editor.grabKeyboardFocus();
                safe->editor.selectAll();
            }
        });
}

void ShapeSavePanel::resized()
{
    panelR = juce::Rectangle<int> (340, 128).withCentre (getLocalBounds().getCentre()).withY (80);
    auto r = panelR.reduced (18, 14);
    r.removeFromTop (22);
    editor.setBounds (r.removeFromTop (30));
    r.removeFromTop (22);
    auto buttons = r.removeFromTop (26);
    cancelR = buttons.removeFromLeft (110);
    saveR   = buttons.removeFromRight (130);
}

void ShapeSavePanel::updateHint()
{
    const auto name = ShapeLibrary::legalName (editor.getText());
    exists = name.isNotEmpty() && ShapeLibrary::fileFor (name).existsAsFile();
    repaint();
}

void ShapeSavePanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xb0010508));

    const auto p = panelR.toFloat();
    g.setColour (juce::Colour (0xf20a1d26));
    g.fillRoundedRectangle (p, 10.0f);
    g.setColour (Ui::aqua.withAlpha (0.45f));
    g.drawRoundedRectangle (p.reduced (0.5f), 10.0f, 1.2f);

    g.setColour (Ui::aqua);
    g.setFont (Ui::font (10.0f, true, 0.2f));
    g.drawText ("ENREGISTRER MA COURBE", panelR.reduced (18, 0).withHeight (36).withY (panelR.getY() + 6), juce::Justification::centredLeft);

    const auto hintArea = juce::Rectangle<int> (panelR.getX() + 18, editor.getBottom() + 3, panelR.getWidth() - 36, 16);
    g.setFont (Ui::font (9.0f));
    if (exists)
    {
        g.setColour (Ui::coral);
        g.drawText (u8 ("Ce nom existe d\xc3\xa9j\xc3\xa0 : la courbe sera remplac\xc3\xa9" "e."), hintArea, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (Ui::dim);
        g.drawText (u8 ("Dossier : Biblioth\xc3\xa8que / Audio / Presets / GRVTY Records / Ebb / Courbes"), hintArea, juce::Justification::centredLeft);
    }

    auto button = [&g] (juce::Rectangle<int> r, const juce::String& text, bool primary, bool isHot)
    {
        const auto f = r.toFloat().reduced (0.5f);
        g.setColour (primary ? Ui::aqua.withAlpha (isHot ? 0.45f : 0.3f) : juce::Colours::white.withAlpha (isHot ? 0.1f : 0.05f));
        g.fillRoundedRectangle (f, f.getHeight() * 0.5f);
        g.setColour (primary ? Ui::aqua : juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (f, f.getHeight() * 0.5f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (Ui::font (10.0f, true, 0.15f));
        g.drawText (text, r, juce::Justification::centred);
    };
    button (cancelR, "ANNULER", false, hot == 1);
    button (saveR, exists ? "REMPLACER" : "ENREGISTRER", true, hot == 2);
}

void ShapeSavePanel::mouseMove (const juce::MouseEvent& e)
{
    const int h = cancelR.contains (e.getPosition()) ? 1 : saveR.contains (e.getPosition()) ? 2 : 0;
    if (h != hot) { hot = h; repaint(); }
}

void ShapeSavePanel::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    if (saveR.contains (pt))         commit();
    else if (cancelR.contains (pt))  cancel();
    else if (! panelR.contains (pt)) cancel();
}

void ShapeSavePanel::commit()
{
    if (finished)
        return;
    const auto name = ShapeLibrary::legalName (editor.getText());
    if (name.isEmpty())
    {
        editor.grabKeyboardFocus();
        return;
    }
    finished = true;
    const bool ok = ShapeLibrary::save (name, shape);
    setVisible (false);
    if (done) done (ok, name);
}

void ShapeSavePanel::cancel()
{
    if (finished)
        return;
    finished = true;
    setVisible (false);
    if (done) done (false, {});
}
