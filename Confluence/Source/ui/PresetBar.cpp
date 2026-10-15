// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "PresetBar.h"
#include "../PluginProcessor.h"

namespace { juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); } }

PresetBar::PresetBar (ConfluenceAudioProcessor& p) : processor (p)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    shownName = processor.presets.getCurrentName();
    shownBank = processor.presets.getCurrentBank();
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    diceR = r.removeFromRight (r.getHeight() + 4);
    r.removeFromRight (4);
    folderR = r.removeFromRight (r.getHeight() + 4);
    r.removeFromRight (4);
    saveR = r.removeFromRight (50);
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
    if (folderR.contains (pt)) return Zone::folder;
    if (diceR.contains (pt)) return Zone::dice;
    return Zone::none;
}

bool PresetBar::refresh()
{
    auto& pm = processor.presets;
    bool dirty = false;
    if (pm.getCurrentName() != shownName || pm.getCurrentBank() != shownBank)
    {
        shownName = pm.getCurrentName();
        shownBank = pm.getCurrentBank();
        dirty = true;
    }
    if (diceSpin > 0.001f) { diceSpin *= 0.85f; dirty = true; }
    if (flashLeft > 0.0f)
    {
        flashLeft -= 1.0f / 60.0f;
        if (flashLeft <= 0.0f) { flashLeft = 0.0f; flashText.clear(); dirty = true; }
    }
    if (dirty) repaint();
    return diceSpin > 0.001f || flashLeft > 0.0f;
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

    for (auto [r, dir, zone] : { std::tuple { prevR, -1.0f, Zone::prev }, std::tuple { nextR, 1.0f, Zone::next } })
    {
        pill (r, hovered == zone, false);
        const auto c = r.toFloat().getCentre();
        auto& chev = Ui::scratchPath (5);
        chev.startNewSubPath (c.x - 1.5f * dir, c.y - 4.0f);
        chev.lineTo (c.x + 2.0f * dir, c.y);
        chev.lineTo (c.x - 1.5f * dir, c.y + 4.0f);
        g.setColour (hovered == zone ? juce::Colours::white : Ui::aqua);
        g.strokePath (chev, stroke);
    }

    // --- nom du preset, précédé de l'étiquette de banque
    {
        const auto f = nameR.toFloat();
        g.setColour (juce::Colour (0xaa020a0e));
        g.fillRoundedRectangle (f, 5.0f);
        g.setColour (hovered == Zone::name ? Ui::aqua.withAlpha (0.7f) : juce::Colours::white.withAlpha (0.1f));
        g.drawRoundedRectangle (f.reduced (0.5f), 5.0f, 1.0f);

        auto inner = nameR.reduced (8, 5);
        const auto tagText = flashText.isNotEmpty() ? flashText : shownBank;
        if (tagText.isNotEmpty())
        {
            const auto tagFont = Ui::font (8.0f, true, 0.2f);
            juce::GlyphArrangement ga;
            ga.addLineOfText (tagFont, tagText, 0.0f, 0.0f);
            auto tag = inner.removeFromLeft (juce::roundToInt (ga.getBoundingBox (0, -1, true).getWidth()) + 14).toFloat();
            const auto col = flashText.isNotEmpty() ? (flashOk ? Ui::aqua : Ui::coral)
                           : shownBank == "USER" ? Ui::aqua : (shownBank == "RANDOM" ? Ui::text
                           : shownBank.startsWith ("DEEP") ? Ui::aqua.interpolatedWith (Ui::text, 0.3f)
                           : shownBank.startsWith ("HYPNOTIC") ? juce::Colour (0xff8fa8ff) : Ui::coral);
            g.setColour (col.withAlpha (flashText.isNotEmpty() ? 0.3f : 0.15f));
            g.fillRoundedRectangle (tag, tag.getHeight() * 0.5f);
            g.setFont (tagFont);
            g.setColour (col);
            g.drawText (tagText, tag, juce::Justification::centred);
        }

        g.setColour (juce::Colours::white);
        g.setFont (Ui::font (11.5f, true, 0.03f));
        g.drawText (shownName, inner.withTrimmedRight (10), juce::Justification::centred, true);

        auto& down = Ui::scratchPath (5);
        const float ax = f.getRight() - 12.0f, ay = f.getCentreY();
        down.startNewSubPath (ax - 3.0f, ay - 1.5f);
        down.lineTo (ax, ay + 1.5f);
        down.lineTo (ax + 3.0f, ay - 1.5f);
        g.setColour (Ui::dim);
        g.strokePath (down, juce::PathStrokeType (1.2f));
    }

    pill (saveR, hovered == Zone::save, true);
    g.setColour (hovered == Zone::save ? juce::Colours::white : Ui::aqua);
    g.setFont (Ui::font (9.5f, true, 0.15f));
    g.drawText ("SAVE", saveR, juce::Justification::centred);

    // --- dossier des presets utilisateur
    {
        const bool hot = hovered == Zone::folder;
        pill (folderR, hot, false);
        const auto c = folderR.toFloat().getCentre();
        auto& f = Ui::scratchPath (6);
        const float w = 12.0f, h = 9.0f, x = c.x - w * 0.5f, y = c.y - h * 0.5f + 0.5f;
        f.startNewSubPath (x, y + h);
        f.lineTo (x, y);
        f.lineTo (x + 4.0f, y);
        f.lineTo (x + 5.5f, y + 1.8f);
        f.lineTo (x + w, y + 1.8f);
        f.lineTo (x + w, y + h);
        f.closeSubPath();
        g.setColour (hot ? juce::Colours::white : Ui::aqua);
        g.strokePath (f, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // --- dé (Random) : tourne quand on clique
    {
        const auto c = diceR.toFloat().getCentre();
        const float s = (float) diceR.getHeight() - 8.0f;
        const auto rot = juce::AffineTransform::rotation (diceSpin * juce::MathConstants<float>::twoPi, c.x, c.y);

        auto& body = Ui::scratchPath (6);
        body.addRoundedRectangle (juce::Rectangle<float> (s, s).withCentre (c), 4.0f);
        const bool hot = hovered == Zone::dice;
        g.setColour (Ui::coral.withAlpha (hot ? 0.35f : 0.18f));
        g.fillPath (body, rot);
        g.setColour (Ui::coral.withAlpha (hot ? 1.0f : 0.8f));
        g.strokePath (body, juce::PathStrokeType (1.3f), rot);

        static const float pips[7][6][2] = {
            {}, { { 0, 0 } }, { { -1, -1 }, { 1, 1 } }, { { -1, -1 }, { 0, 0 }, { 1, 1 } },
            { { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } }, { { -1, -1 }, { 1, -1 }, { 0, 0 }, { -1, 1 }, { 1, 1 } },
            { { -1, -1 }, { 1, -1 }, { -1, 0 }, { 1, 0 }, { -1, 1 }, { 1, 1 } } };
        const float stepPx = s * 0.26f;
        g.setColour (juce::Colours::white);
        for (int i = 0; i < diceFace; ++i)
        {
            auto pt = juce::Point<float> (c.x + pips[diceFace][i][0] * stepPx, c.y + pips[diceFace][i][1] * stepPx).transformedBy (rot);
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
        case Zone::folder: openFolder(); break;
        case Zone::dice: rollDice(); break;
        case Zone::none:
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

    int current = pm.getCurrentFactoryIndex();
    if (current < 0)
        for (int i = 0; i < user.size(); ++i)
            if (user[i].getFileNameWithoutExtension() == pm.getCurrentName())
                current = numFactory + i;

    const int next = current < 0 ? (direction > 0 ? 0 : total - 1) : (current + direction + total) % total;
    if (next < numFactory) pm.loadFactoryPreset (next);
    else                   pm.loadUserPreset (user[next - numFactory]);
    refresh();
}

void PresetBar::flash (const juce::String& message, bool ok)
{
    flashText = message;
    flashOk = ok;
    flashLeft = 2.5f;
    repaint();
}

void PresetBar::openFolder()
{
    const auto folder = PresetManager::getUserFolder();
    // startAsProcess ouvre le dossier lui-même (Finder ou Explorateur) et le passe au premier plan
    // (revealToUser se contentait de le sélectionner dans son dossier parent).
    const bool ok = folder.createDirectory() && folder.startAsProcess();
    if (! ok)
        flash ("DOSSIER ?", false);
}

void PresetBar::showMenu()
{
    auto& pm = processor.presets;
    juce::PopupMenu menu;

    // presets d'usine : un sous-menu par banque (le menu reste court même avec beaucoup de presets)
    const auto& factory = PresetManager::getFactoryPresets();
    const int currentIndex = pm.getCurrentFactoryIndex();
    for (int i = 0; i < (int) factory.size();)
    {
        const juce::String bank (factory[(size_t) i].bank);
        juce::PopupMenu sub;
        bool containsCurrent = false;
        const int firstOfBank = i;
        for (; i < (int) factory.size() && bank == factory[(size_t) i].bank; ++i)
        {
            sub.addItem (1 + i, juce::String (factory[(size_t) i].name), true, currentIndex == i);
            containsCurrent = containsCurrent || currentIndex == i;
        }
        menu.addSubMenu (bank + "   (" + juce::String (i - firstOfBank) + " presets, " + juce::String ((int) factory[(size_t) firstOfBank].bpm) + " BPM)",
                         sub, true, nullptr, containsCurrent);
    }

    const auto user = pm.getUserPresets();
    juce::PopupMenu userMenu;
    bool userCurrent = false;
    if (user.isEmpty())
        userMenu.addItem (-1, "(aucun pour l'instant : clique sur SAVE)", false);
    for (int i = 0; i < user.size(); ++i)
    {
        const bool isCurrent = currentIndex < 0 && user[i].getFileNameWithoutExtension() == pm.getCurrentName();
        userMenu.addItem (2000 + i, user[i].getFileNameWithoutExtension(), true, isCurrent);
        userCurrent = userCurrent || isCurrent;
    }
    menu.addSeparator();
    menu.addSubMenu ("MES PRESETS   (" + juce::String (user.size()) + ")", userMenu, true, nullptr, userCurrent);

    menu.addSeparator();
    menu.addItem (1000, u8 ("Enregistrer les r\xc3\xa9glages et les pads..."));
    menu.addItem (1001, "Ouvrir le dossier des presets");
    menu.addItem (1002, u8 ("R\xc3\xa9glages au hasard (d\xc3\xa9)"));
    menu.addItem (1003, u8 ("Pads au hasard (r\xc3\xa9glages gard\xc3\xa9s)"));

    juce::Component::SafePointer<PresetBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMinimumWidth (nameR.getWidth()),
                        [safe, user] (int result)
                        {
                            if (safe == nullptr || result <= 0) return;
                            auto& manager = safe->processor.presets;
                            if (result < 1000)         manager.loadFactoryPreset (result - 1);
                            else if (result >= 2000)   manager.loadUserPreset (user[result - 2000]);
                            else if (result == 1000)   safe->askSaveName();
                            else if (result == 1001)   safe->openFolder();
                            else if (result == 1002)   safe->rollDice();
                            else if (result == 1003)   manager.randomisePads();
                            safe->refresh();
                        });
}

void PresetBar::askSaveName()
{
    auto* parent = getParentComponent();
    if (parent == nullptr || savePanel != nullptr)
        return;

    const auto current = processor.presets.getCurrentName();
    const bool factory = processor.presets.getCurrentFactoryIndex() >= 0;
    const auto suggested = current.startsWith ("Random") || current == "Init" ? juce::String ("Mon preset")
                         : factory ? current + " perso" : current;

    juce::Component::SafePointer<PresetBar> safe (this);
    savePanel = std::make_unique<SavePanel> (processor.presets, suggested, [safe] (bool saved, juce::String name)
    {
        if (safe == nullptr)
            return;
        if (saved)
            safe->flash (u8 ("ENREGISTR\xc3\x89"), true);
        else if (name.isNotEmpty())
            safe->flash ("ERREUR", false);
        safe->refresh();
        // on détruit le panneau après la fin de son propre appel
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->savePanel.reset(); });
    });
    parent->addAndMakeVisible (*savePanel);
    savePanel->setBounds (parent->getLocalBounds());
}

//==============================================================================
SavePanel::SavePanel (PresetManager& pm, const juce::String& suggestedName, std::function<void (bool, juce::String)> onDone)
    : presets (pm), done (std::move (onDone))
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

void SavePanel::parentHierarchyChanged()
{
    if (isShowing())
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<SavePanel> (this)]
        {
            if (safe != nullptr && safe->isShowing())
            {
                safe->editor.grabKeyboardFocus();
                safe->editor.selectAll();
            }
        });
}

void SavePanel::resized()
{
    panelR = juce::Rectangle<int> (340, 128).withCentre (getLocalBounds().getCentre()).withY (70);
    auto r = panelR.reduced (18, 14);
    r.removeFromTop (22);
    editor.setBounds (r.removeFromTop (30));
    r.removeFromTop (22);
    auto buttons = r.removeFromTop (26);
    cancelR = buttons.removeFromLeft (110);
    saveR   = buttons.removeFromRight (130);
}

void SavePanel::updateHint()
{
    const auto name = PresetManager::toPresetName (editor.getText());
    exists = name.isNotEmpty() && PresetManager::getUserPresetFile (name).existsAsFile();
    repaint();
}

void SavePanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xb0010508));

    const auto p = panelR.toFloat();
    g.setColour (juce::Colour (0xf20a1d26));
    g.fillRoundedRectangle (p, 10.0f);
    g.setColour (Ui::aqua.withAlpha (0.45f));
    g.drawRoundedRectangle (p.reduced (0.5f), 10.0f, 1.2f);

    g.setColour (Ui::aqua);
    g.setFont (Ui::font (10.0f, true, 0.2f));
    g.drawText (u8 ("ENREGISTRER LE PRESET  (R\xc3\x89GLAGES + PADS)"), panelR.reduced (18, 0).withHeight (36).withY (panelR.getY() + 6),
                juce::Justification::centredLeft);

    // chemin du fichier / avertissement si le nom existe déjà
    const auto hintArea = juce::Rectangle<int> (panelR.getX() + 18, editor.getBottom() + 3, panelR.getWidth() - 36, 16);
    g.setFont (Ui::font (9.0f));
    if (exists)
    {
        g.setColour (Ui::coral);
        g.drawText (u8 ("Ce nom existe d\xc3\xa9j\xc3\xa0 : le preset sera remplac\xc3\xa9."), hintArea, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (Ui::dim);
        g.drawText ("Dossier : " + PresetManager::getUserFolderDisplayName(), hintArea, juce::Justification::centredLeft);
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

void SavePanel::mouseMove (const juce::MouseEvent& e)
{
    const int h = cancelR.contains (e.getPosition()) ? 1 : saveR.contains (e.getPosition()) ? 2 : 0;
    if (h != hot) { hot = h; repaint(); }
}

void SavePanel::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    if (saveR.contains (pt))         commit();
    else if (cancelR.contains (pt))  cancel();
    else if (! panelR.contains (pt)) cancel();
}

void SavePanel::commit()
{
    if (finished)
        return;
    const auto name = PresetManager::toPresetName (editor.getText());
    if (name.isEmpty())
    {
        editor.grabKeyboardFocus();
        return;
    }
    finished = true;
    const bool ok = presets.saveUserPreset (name);
    setVisible (false);
    if (done) done (ok, name);
}

void SavePanel::cancel()
{
    if (finished)
        return;
    finished = true;
    setVisible (false);
    if (done) done (false, {});
}
