// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "PadGrid.h"
#include "../PluginProcessor.h"
#include "../PluginEditor.h"

using ChordText::u8;

//==============================================================================
// PadTile
//==============================================================================
PadTile::PadTile (ConfluenceAudioProcessor& p, int i) : processor (p), index (i)
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    shownPad.degree = -1;
}

bool PadTile::tick (bool isActive, uint32_t sig)
{
    bool dirty = false;
    const auto& p = processor.getPads().pads[(size_t) index];
    if (! (p == shownPad) || sig != shownSig)
    {
        shownPad = p;
        shownSig = sig;
        auto cs = processor.readChordSettings();
        cs.mode = Engine::modePads;
        Engine::Resolved r;
        text = Engine::resolve (cs, processor.getPads(), processor.getLearned(), cs.padBase + index, {}, r)
                   ? ChordText::describe (r, cs.scale, cs.rootPc) : ChordText::Described {};
        noteLabel = Params::noteName (cs.padBase + index);
        staticDirty = textDirty = dirty = true;
    }
    if (isActive != active) { active = isActive; staticDirty = dirty = true; }
    const float target = active ? 0.78f : 0.3f;
    const bool moving = std::abs (level - target) > 0.005f;
    if (moving)
    {
        level += (target - level) * 0.18f;
        if (std::abs (level - target) <= 0.005f) { level = target; staticDirty = true; }
        dirty = true;
    }
    if (active) dirty = true;   // ondulation
    if (dirty) repaint();
    return active || moving;
}

void PadTile::drawTexts (juce::Graphics& g)
{
    auto txt = getLocalBounds().toFloat().reduced (1.0f).reduced (7.0f, 4.0f);
    auto top = txt.removeFromTop (11.0f);
    g.setColour (Ui::dim);
    g.setFont (Ui::font (7.5f, true, 0.2f));
    g.drawText (juce::String (index + 1), top, juce::Justification::topLeft);
    g.drawText (noteLabel, top, juce::Justification::topRight);
    g.setColour (juce::Colours::white);
    g.setFont (Ui::font (13.0f, true));
    g.drawFittedText (text.valid ? text.name : juce::String ("--"), txt.removeFromTop (17.0f).toNearestInt(), juce::Justification::centredLeft, 1, 0.7f);
    g.setColour (Ui::aqua);
    g.setFont (Ui::font (9.5f, true));
    g.drawText (text.degree, txt.removeFromTop (12.0f), juce::Justification::centredLeft);
}

void PadTile::drawBody (juce::Graphics& g, float t, bool withText)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool hot = isMouseOver();
    g.setColour (juce::Colour (0xff04121a));
    g.fillRoundedRectangle (r, 7.0f);
    {
        juce::Graphics::ScopedSaveState save (g);
        auto& clip = Ui::scratchPath (4);
        clip.addRoundedRectangle (r, 7.0f);
        g.reduceClipRegion (clip);
        Ui::drawLiquid (g, r, level, t, (float) index * 1.3f,
                        (active ? Ui::aqua : Ui::aquaDeep).withAlpha (active ? 0.65f : 0.42f), Ui::aquaDeep.withAlpha (0.12f), active ? 2.4f : 1.2f);
        if (active)
            for (int k = 0; k < 3; ++k)
            {
                const float life = std::fmod (t * 0.6f + (float) k / 3.0f, 1.0f);
                g.setColour (juce::Colours::white.withAlpha (0.25f * (1.0f - life)));
                g.drawEllipse (juce::Rectangle<float> (r.getWidth() * life * 1.4f, r.getHeight() * life * 0.7f).withCentre (r.getCentre()), 1.0f);
            }
    }
    g.setColour (active ? Ui::aqua : juce::Colours::white.withAlpha (hot ? 0.25f : 0.1f));
    g.drawRoundedRectangle (r.reduced (0.5f), 7.0f, active ? 1.6f : 1.0f);
    if (withText) drawTexts (g);
}

void PadTile::paint (juce::Graphics& g)
{
    const float s = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (std::abs (s - imgScale) > 0.01f) { imgScale = s; staticDirty = textDirty = true; }
    const int w = juce::roundToInt ((float) getWidth() * s), h = juce::roundToInt ((float) getHeight() * s);
    auto prepare = [w, h] (juce::Image& img)
    {
        if (! img.isValid() || img.getWidth() != w || img.getHeight() != h) img = juce::Image (juce::Image::ARGB, w, h, true);
        else img.clear (img.getBounds());
    };

    const bool live = active || std::abs (level - (active ? 0.78f : 0.3f)) > 0.005f;
    if (! live)
    {
        if (staticDirty || ! staticImg.isValid())
        {
            prepare (staticImg);
            juce::Graphics ig (staticImg);
            ig.addTransform (juce::AffineTransform::scale (s));
            drawBody (ig, 0.0f, true);
            staticDirty = false;
        }
        g.drawImageTransformed (staticImg, juce::AffineTransform::scale (1.0f / s));
        return;
    }

    if (textDirty || ! textImg.isValid())
    {
        prepare (textImg);
        juce::Graphics ig (textImg);
        ig.addTransform (juce::AffineTransform::scale (s));
        drawTexts (ig);
        textDirty = false;
    }
    drawBody (g, Ui::now(), false);
    g.setOpacity (1.0f);
    g.drawImageTransformed (textImg, juce::AffineTransform::scale (1.0f / s));
}

void PadTile::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu()) { showMenu(); return; }
    playing = true;
    processor.playFromUi (index, 100, true);
}

void PadTile::mouseUp (const juce::MouseEvent&)
{
    if (! playing) return;
    playing = false;
    processor.playFromUi (index, 0, true);
}

void PadTile::showMenu()
{
    const auto cs = processor.readChordSettings();
    const auto pad = processor.getPads().pads[(size_t) index];
    juce::PopupMenu degrees, types, inversions, octaves;
    for (int d = 0; d < 7; ++d)
        degrees.addItem (100 + d, juce::String (d + 1) + "   " + ChordText::romanOfDegree (cs.scale, cs.rootPc, d, pad.type)
                                  + "   " + Theory::pcName (cs.rootPc + Theory::degreeInterval (cs.scale, d), Theory::useFlats (cs.scale, cs.rootPc)),
                         true, pad.degree == d);
    for (int t = 0; t < Theory::numChordTypes; ++t)
        types.addItem (200 + t, u8 (Theory::chordTypeName (t)), true, pad.type == t);
    const char* inv[] = { "0 (fondamentale)", "1", "2", "3", "Auto (voice leading)" };
    for (int i = 0; i < 5; ++i)
        inversions.addItem (300 + i, inv[i], true, pad.inversion == i);
    for (int o = -2; o <= 2; ++o)
        octaves.addItem (400 + o + 2, (o > 0 ? "+" : "") + juce::String (o), true, pad.octave == o);

    juce::PopupMenu m;
    m.addSectionHeader ("PAD " + juce::String (index + 1) + (text.valid ? "  -  " + text.name : juce::String()));
    m.addSubMenu (u8 ("Degr\xc3\xa9"), degrees);
    m.addSubMenu ("Type d'accord", types);
    m.addSubMenu ("Renversement", inversions);
    m.addSubMenu ("Octave", octaves);

    juce::Component::SafePointer<PadTile> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int res)
    {
        if (safe == nullptr || res <= 0) return;
        auto p = safe->processor.getPads().pads[(size_t) safe->index];
        if (res >= 400)      p.octave = (int8_t) (res - 402);
        else if (res >= 300) p.inversion = (int8_t) (res - 300);
        else if (res >= 200) p.type = (int8_t) (res - 200);
        else                 p.degree = (int8_t) (res - 100);
        safe->processor.setPad (safe->index, p);
    });
}

//==============================================================================
// MidiDragPill
//==============================================================================
void MidiDragPill::resized()
{
    // contour en pointillés calculé une fois par taille (createDashedStroke alloue)
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    juce::Path outline;
    outline.addRoundedRectangle (r, r.getHeight() * 0.5f);
    const float dashes[] = { 4.0f, 3.0f };
    dashedOutline.clear();
    juce::PathStrokeType (1.0f).createDashedStroke (dashedOutline, outline, dashes, 2);
}

void MidiDragPill::paint (juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const bool hot = isMouseOver() || dragging;
    g.setColour (Ui::coral.withAlpha (dragging ? 0.3f : (hot ? 0.18f : 0.08f)));
    g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
    g.setColour (Ui::coral.withAlpha (0.85f));
    g.fillPath (dashedOutline);

    // flèche vers le bas (glisser)
    const float cx = r.getX() + 12.0f, cy = r.getCentreY();
    auto& arrow = Ui::scratchPath (6);
    arrow.startNewSubPath (cx, cy - 4.5f);
    arrow.lineTo (cx, cy + 3.5f);
    arrow.startNewSubPath (cx - 3.0f, cy + 0.5f);
    arrow.lineTo (cx, cy + 3.5f);
    arrow.lineTo (cx + 3.0f, cy + 0.5f);
    g.setColour (hot ? juce::Colours::white : Ui::coral);
    g.strokePath (arrow, juce::PathStrokeType (1.3f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setFont (Ui::font (8.5f, true, 0.12f));
    g.drawText ("MIDI " + juce::String (bars), r.withTrimmedLeft (18.0f), juce::Justification::centred);
}

void MidiDragPill::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging || e.getDistanceFromDragStart() < 5)
        return;
    dragging = true;
    repaint();
    const auto file = juce::File::getSpecialLocation (juce::File::tempDirectory)
                          .getChildFile ("Confluence").getChildFile ("Confluence progression.mid");
    if (processor.exportMidi (file, bars))
        juce::DragAndDropContainer::performExternalDragDropOfFiles ({ file.getFullPathName() }, false, this,
                                                                    [safe = juce::Component::SafePointer<MidiDragPill> (this)]
                                                                    { if (safe != nullptr) { safe->dragging = false; safe->repaint(); } });
}

void MidiDragPill::mouseUp (const juce::MouseEvent& e)
{
    if (dragging) { dragging = false; repaint(); return; }
    if (e.getDistanceFromDragStart() < 5) showMenu();
}

void MidiDragPill::showMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("PROGRESSION DES 8 PADS");
    m.addItem (1, u8 ("Glisser : clip de 4 mesures"), true, bars == 4);
    m.addItem (2, u8 ("Glisser : clip de 8 mesures"), true, bars == 8);
    m.addSeparator();
    m.addItem (3, u8 ("Exporter en .mid (4 mesures)"));
    m.addItem (4, u8 ("Exporter en .mid (8 mesures)"));
    m.addItem (5, "Ouvrir le dossier Musique / Confluence");

    juce::Component::SafePointer<MidiDragPill> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int res)
    {
        if (safe == nullptr || res <= 0) return;
        auto& p = safe->processor;
        if (res == 1 || res == 2) { safe->bars = res == 1 ? 4 : 8; safe->repaint(); return; }
        if (res == 5)
        {
            const auto folder = ConfluenceAudioProcessor::getExportFolder();
            if (! (folder.createDirectory() && folder.startAsProcess()) && safe->onMessage) safe->onMessage ("DOSSIER ?", false);
            return;
        }
        const int exportBars = res == 3 ? 4 : 8;
        const auto cs = p.readChordSettings();
        const auto name = "Confluence " + juce::String (Theory::pcName (cs.rootPc, Theory::useFlats (cs.scale, cs.rootPc))) + " "
                        + u8 (Theory::scaleInfo (cs.scale).name) + " - " + p.presets.getCurrentName() + " - " + juce::String (exportBars) + " mesures - "
                        + juce::Time::getCurrentTime().formatted ("%Y-%m-%d %H%M%S") + ".mid";
        const auto f = ConfluenceAudioProcessor::getExportFolder().getChildFile (juce::File::createLegalFileName (name));
        const bool ok = p.exportMidi (f, exportBars);
        if (safe->onMessage) safe->onMessage (ok ? u8 ("EXPORT\xc3\x89") : juce::String ("ERREUR"), ok);
    });
}

//==============================================================================
// PadGrid
//==============================================================================
PadGrid::PadGrid (ConfluenceAudioProcessor& p) : processor (p), midi (p)
{
    for (int i = 0; i < Engine::numPads; ++i)
        addAndMakeVisible (tiles.add (new PadTile (processor, i)));

    auto* gen = new PillButton (u8 ("G\xc3\x89N\xc3\x89RER"));
    gen->setTooltip (u8 ("Remplit les 8 pads avec une progression dans la gamme, selon le style"));
    gen->onClick = [this]
    {
        processor.presets.randomisePads();
        if (onMessage) onMessage (u8 ("PADS G\xc3\x89N\xc3\x89R\xc3\x89S"), true);
    };
    generate.reset (gen);
    generate->setBufferedToImage (true);
    addAndMakeVisible (*generate);
    addAndMakeVisible (midi);
    midi.setTooltip (u8 ("Glisse vers une piste d'Ableton : clip MIDI de la progression des pads. Clic : export .mid"));
    midi.onMessage = [this] (const juce::String& m, bool ok) { if (onMessage) onMessage (m, ok); };
}

void PadGrid::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (24);
    head.removeFromLeft (44);   // titre PADS (dessiné dans le fond)
    generate->setBounds (head.removeFromLeft (76).reduced (0, 2));
    head.removeFromLeft (4);
    midi.setBounds (head.removeFromLeft (76).reduced (0, 2));
    r.removeFromTop (6);

    const int gap = 6;
    const int pw = (r.getWidth() - gap) / 2, ph = (r.getHeight() - 3 * gap) / 4;
    for (int i = 0; i < tiles.size(); ++i)
    {
        const int col = i % 2, row = 3 - i / 2;   // pad 1 en bas à gauche
        tiles[i]->setBounds (r.getX() + col * (pw + gap), r.getY() + row * (ph + gap), pw, ph);
    }
}

bool PadGrid::tick()
{
    const auto cs = processor.readChordSettings();
    const uint32_t sig = (uint32_t) (cs.rootPc + 12 * cs.scale + 400 * cs.voicing.spread + 4000 * cs.voicing.maxVoices
                                     + 40000 * cs.voicing.bassOctaves) ^ ((uint32_t) (cs.voicing.shift + 64) << 20)
                         ^ ((uint32_t) cs.padBase << 26) ^ ((uint32_t) (cs.voicing.low * 131 + cs.voicing.high) << 8);
    const int active = processor.activePad.load (std::memory_order_relaxed);
    bool anim = false;
    for (int i = 0; i < tiles.size(); ++i)
        anim = tiles[i]->tick (i == active, sig) || anim;
    return anim;
}
