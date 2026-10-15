// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "PluginEditor.h"

namespace
{
    juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }

    const char* curveDescription (int c)
    {
        static const char* d[Ebb::numCurves] = {
            "chute tr\xc3\xa8s courte : ne lib\xc3\xa8re que l'attaque du kick",
            "courte et nerveuse, remont\xc3\xa9" "e rapide",
            "la pompe classique : chute nette, remont\xc3\xa9" "e rapide puis douce",
            "remont\xc3\xa9" "e en ligne droite",
            "palier : retombe \xc3\xa0 mi-hauteur, attend, puis remonte",
            "double pompe : une seconde baisse au milieu",
            "gate net : coup\xc3\xa9 pendant la premi\xc3\xa8re moiti\xc3\xa9",
            "en S, longue et ronde",
            "tr\xc3\xa8s douce, sur presque toute la dur\xc3\xa9" "e",
            "respiration : descend et remonte sans angle",
            "rampe : le niveau remonte sur toute la dur\xc3\xa9" "e",
            "inverse : le niveau baisse en approchant du temps" };
        return d[juce::jlimit (0, Ebb::numCurves - 1, c)];
    }

    juce::RangedAudioParameter& param (EbbAudioProcessor& p, const char* id) { return *p.apvts.getParameter (id); }

    const int gridValues[] = { 0, 4, 8, 16, 32 };
}

//==============================================================================
EbbContent::EbbContent (EbbAudioProcessor& p)
    : processor (p), curveView (p), strip (p),
      amountKnob (p.apvts, ParamIDs::amount, "Amount", false, u8 ("Profondeur de la baisse de volume (0 % : le son passe intact)")),
      tensionKnob (p.apvts, ParamIDs::tension, "Tension", true, u8 ("+ : plus s\xc3\xa8" "che (le son revient plus vite)   \xe2\x80\x93 : plus molle (il reste bas plus longtemps)")),
      smoothKnob (p.apvts, ParamIDs::smooth, "Lissage", false, u8 ("Arrondit la courbe (les angles et les marches)")),
      nudgeKnob (p.apvts, ParamIDs::nudge, "Nudge", true, u8 ("SYNC : d\xc3\xa9" "cale la courbe (+ = plus tard) pour un kick qui n'attaque pas pile sur le temps")),
      thresholdKnob (p.apvts, ParamIDs::threshold, "Seuil", false, u8 ("AUDIO : niveau des graves du sidechain qui relance la courbe")),
      crossoverKnob (p.apvts, ParamIDs::crossover, u8 ("Fr\xc3\xa9q"), false, u8 ("Fr\xc3\xa9quence de s\xc3\xa9paration graves / aigus")),
      ratePills (param (p, ParamIDs::rate), { "1/16", "1/8", "1/4T", "1/8.", "1/4", "1/2", "1 MES" }),
      triggerPills (param (p, ParamIDs::trigger), { "SYNC", "MIDI", "AUDIO" }),
      bandPills (param (p, ParamIDs::band), { "PLEIN", "GRAVES", "AIGUS" }, true)
{
    setLookAndFeel (&look);
    setOpaque (true);
    versionText = "v" JucePlugin_VersionString;

    amountKnob.setMaxDiameter (136.0f);
    for (juce::Component* c : { (juce::Component*) &curveView, (juce::Component*) &strip, (juce::Component*) &amountKnob,
                                (juce::Component*) &tensionKnob, (juce::Component*) &smoothKnob, (juce::Component*) &nudgeKnob,
                                (juce::Component*) &thresholdKnob, (juce::Component*) &crossoverKnob, (juce::Component*) &ratePills,
                                (juce::Component*) &triggerPills, (juce::Component*) &bandPills, (juce::Component*) &gridChooser,
                                (juce::Component*) &editButton, (juce::Component*) &undoButton, (juce::Component*) &redoButton,
                                (juce::Component*) &mirrorButton, (juce::Component*) &invertButton, (juce::Component*) &clearButton,
                                (juce::Component*) &myCurvesButton, (juce::Component*) &saveButton })
        addAndMakeVisible (c);

    // chaque contrôle garde son image : redessiné seulement si sa valeur change ou s'il est survolé
    for (juce::Component* c : { (juce::Component*) &strip, (juce::Component*) &amountKnob, (juce::Component*) &tensionKnob,
                                (juce::Component*) &smoothKnob, (juce::Component*) &nudgeKnob, (juce::Component*) &thresholdKnob,
                                (juce::Component*) &crossoverKnob, (juce::Component*) &ratePills, (juce::Component*) &triggerPills,
                                (juce::Component*) &bandPills, (juce::Component*) &gridChooser, (juce::Component*) &editButton,
                                (juce::Component*) &undoButton, (juce::Component*) &redoButton, (juce::Component*) &mirrorButton,
                                (juce::Component*) &invertButton, (juce::Component*) &clearButton, (juce::Component*) &myCurvesButton,
                                (juce::Component*) &saveButton })
        c->setBufferedToImage (true);

    for (auto* b : { &undoButton, &redoButton })
        b->fontSize = 9.0f;
    for (auto* b : { &myCurvesButton, &saveButton })
        b->accent = Ui::coral;

    editButton.setTooltip (u8 ("Dessiner sa courbe : clic = ajouter un point, glisser = d\xc3\xa9placer, poign\xc3\xa9" "e du milieu = courbure, double-clic = supprimer"));
    undoButton.setTooltip ("Annuler");
    redoButton.setTooltip (u8 ("R\xc3\xa9tablir"));
    mirrorButton.setTooltip (u8 ("Retourne la courbe dans le temps (la fin devient le d\xc3\xa9" "but)"));
    invertButton.setTooltip (u8 ("Retourne la courbe de haut en bas"));
    clearButton.setTooltip (u8 ("Repart d'une pompe simple"));
    myCurvesButton.setTooltip (u8 ("Charger une de tes courbes, ou copier une courbe d'usine pour la retoucher"));
    saveButton.setTooltip (u8 ("Enregistrer ta courbe dans Biblioth\xc3\xa8que / Audio / Presets / GRVTY Records / Ebb / Courbes"));
    gridChooser.setTooltip (u8 ("Grille aimant\xc3\xa9" "e de l'\xc3\xa9" "diteur (divisions de la courbe). Alt pendant le geste : sans aimant"));

    editButton.onClick = [this] { setEditMode (! curveView.isEditMode()); };
    undoButton.onClick = [this] { processor.undoShape(); };
    redoButton.onClick = [this] { processor.redoShape(); };
    mirrorButton.onClick = [this]
    {
        editShape ([] (Ebb::ShapeData& s)
        {
            Ebb::ShapeData m;
            m.count = s.count;
            for (int i = 0; i < s.count; ++i)
            {
                const auto& src = s.pts[(size_t) (s.count - 1 - i)];
                // le segment qui partait du point précédent est maintenant parcouru à l'envers : courbure opposée
                const float c = i + 1 < s.count ? -s.pts[(size_t) (s.count - 2 - i)].curve : 0.0f;
                m.pts[(size_t) i] = { 1.0f - src.x, src.y, c };
            }
            s = m;
        });
    };
    invertButton.onClick = [this]
    {
        editShape ([] (Ebb::ShapeData& s) { for (int i = 0; i < s.count; ++i) s.pts[(size_t) i].y = 1.0f - s.pts[(size_t) i].y; });
    };
    clearButton.onClick = [this] { editShape ([] (Ebb::ShapeData& s) { s = Ebb::ShapeMath::defaultShape(); }); };
    myCurvesButton.onClick = [this] { showMyCurvesMenu(); };
    saveButton.onClick = [this] { openSavePanel(); };

    // bouton « i » : panneau À propos (licence, code source, confidentialité, composants tiers)
    infoButton.onClick = [this] { openAbout(); };
    addAndMakeVisible (infoButton);
    strip.onPick = [this] (int i) { pickCurve (i); };

    int gi = 0;
    for (int i = 0; i < 5; ++i) if (gridValues[i] == processor.getGrid()) gi = i;
    gridChooser.setSelected (gi);
    gridChooser.onChange = [this] (int i) { processor.setGrid (gridValues[juce::jlimit (0, 4, i)]); };

    status = statusText();
    setSize (designW, designH);
}

EbbContent::~EbbContent()
{
    setLookAndFeel (nullptr);
}

//==============================================================================
void EbbContent::setEditMode (bool on)
{
    if (on && ! processor.readSettings().custom)
        processor.copyFactoryToShape (processor.readSettings().curve, true);   // on part de ce qu'on entend
    curveView.setEditMode (on);
    editButton.lit = on;
    editButton.setButtonText (on ? juce::String ("OK") : u8 ("\xc3\x89" "DITER"));
    editButton.repaint();
}

void EbbContent::editShape (const std::function<void (Ebb::ShapeData&)>& fn)
{
    auto s = processor.readSettings().custom ? processor.getShape()
                                             : Ebb::ShapeMath::fromTable (Ebb::CurveTables::get().table (processor.readSettings().curve), Ebb::CurveTables::size);
    fn (s);
    processor.setShape (s, true, true);
}

void EbbContent::pickCurve (int index)
{
    if (index == CurveStrip::numCells - 1)
    {
        processor.setParam (ParamIDs::custom, 1.0f);
        return;
    }
    if (curveView.isEditMode())
    {
        processor.copyFactoryToShape (index, true);   // en édition : la courbe d'usine devient le point de départ
        flash (u8 ("COPI\xc3\x89" "E : ") + Ui::upper (Ebb::curveName (index)), true);
        return;
    }
    processor.setParam (ParamIDs::curve, (float) index);
    processor.setParam (ParamIDs::custom, 0.0f);
}

void EbbContent::loadUserShape (const juce::File& f)
{
    Ebb::ShapeData s;
    if (ShapeLibrary::load (f, s))
    {
        processor.setShape (s, true, true);
        flash (Ui::upper (f.getFileNameWithoutExtension()), true);
    }
    else
        flash ("FICHIER ILLISIBLE", false);
}

void EbbContent::showMyCurvesMenu()
{
    juce::PopupMenu menu;
    const auto files = ShapeLibrary::list();
    menu.addSectionHeader ("MES COURBES");
    if (files.isEmpty())
        menu.addItem (-1, u8 ("(aucune pour l'instant : dessine, puis ENREGISTRER)"), false);
    for (int i = 0; i < files.size(); ++i)
        menu.addItem (100 + i, files[i].getFileNameWithoutExtension());

    juce::PopupMenu factory;
    for (int i = 0; i < Ebb::numCurves; ++i)
        factory.addItem (1000 + i, Ebb::curveName (i));
    menu.addSeparator();
    menu.addSubMenu (u8 ("Copier une courbe d'usine pour la retoucher"), factory);
    menu.addItem (2, u8 ("Repartir d'une pompe simple"));
    menu.addItem (3, "Ouvrir le dossier de mes courbes");

    juce::Component::SafePointer<EbbContent> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&myCurvesButton).withMinimumWidth (220),
                        [safe, files] (int result)
                        {
                            if (safe == nullptr || result <= 0) return;
                            auto& self = *safe;
                            if (result >= 1000)      { self.processor.copyFactoryToShape (result - 1000, true); self.setEditMode (true); }
                            else if (result >= 100)  self.loadUserShape (files[result - 100]);
                            else if (result == 2)    self.processor.setShape (Ebb::ShapeMath::defaultShape(), true, true);
                            else if (result == 3)
                            {
                                const auto folder = ShapeLibrary::folder();
                                if (! (folder.createDirectory() && folder.startAsProcess()))
                                    self.flash ("DOSSIER ?", false);
                            }
                        });
}

void EbbContent::openSavePanel()
{
    if (savePanel != nullptr)
        return;
    const auto shape = processor.readSettings().custom ? processor.getShape()
                     : Ebb::ShapeMath::fromTable (Ebb::CurveTables::get().table (processor.readSettings().curve), Ebb::CurveTables::size);
    juce::Component::SafePointer<EbbContent> safe (this);
    savePanel = std::make_unique<ShapeSavePanel> (shape, u8 ("Ma courbe"), [safe] (bool saved, juce::String name)
    {
        if (safe == nullptr)
            return;
        if (saved)               safe->flash (u8 ("ENREGISTR\xc3\x89" "E : ") + Ui::upper (name), true);
        else if (name.isNotEmpty()) safe->flash ("ERREUR", false);
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->savePanel.reset(); });
    });
    addAndMakeVisible (*savePanel);
    savePanel->setBounds (getLocalBounds());
}

void EbbContent::flash (const juce::String& message, bool ok)
{
    flashText = message;
    flashOk = ok;
    flashLeft = 2.5f;
    overlayDirty = bgDirty = true;
    repaint();
}

juce::String EbbContent::statusText() const
{
    const auto s = processor.readSettings();
    if (s.trigger == Ebb::triggerAudio)
        return processor.isSidechainConnected() ? u8 ("AUDIO  \xc2\xb7  KICK SUR LE SIDECHAIN")
                                                : u8 ("AUDIO  \xc2\xb7  CHOISIS LA PISTE DU KICK DANS SIDECHAIN");
    if (s.trigger == Ebb::triggerMidi)
        return u8 ("MIDI  \xc2\xb7  CHAQUE NOTE RELANCE LA COURBE");
    return u8 ("SYNC  \xc2\xb7  ") + juce::String (juce::roundToInt (processor.getBpm())) + " BPM";
}

//==============================================================================
bool EbbContent::tick()
{
    const bool curveMoving = curveView.tick();
    strip.tick();
    const auto s = processor.readSettings();

    // le réglage inutile dans le mode actuel est grisé
    const int band = processor.getBand();
    if (s.trigger != lastMode || band != lastBand)
    {
        lastMode = s.trigger;
        lastBand = band;
        nudgeKnob.setAlpha (s.trigger == Ebb::triggerSync ? 1.0f : 0.35f);
        thresholdKnob.setAlpha (s.trigger == Ebb::triggerAudio ? 1.0f : 0.35f);
        crossoverKnob.setAlpha (band != Params::bandFull ? 1.0f : 0.35f);
    }
    // légende de la vague : la bande qui pompe (recalculée seulement quand elle change)
    const int capKey = band * 100000 + juce::roundToInt (processor.getCrossover()) * 2 + (curveView.isEditMode() ? 1 : 0);
    if (capKey != captionKey)
    {
        captionKey = capKey;
        const int hz = juce::roundToInt (processor.getCrossover());
        const juce::String hzText = hz >= 1000 ? juce::String (hz / 1000.0f, 1) + " kHz" : juce::String (hz) + " Hz";
        curveView.setCaption (band == Params::bandLow ? u8 ("SEULS LES GRAVES POMPENT  \xc2\xb7  < ") + hzText
                              : band == Params::bandHigh ? u8 ("SEULS LES AIGUS POMPENT  \xc2\xb7  > ") + hzText
                              : (curveView.isEditMode() ? u8 ("\xc3\x89" "DITION DE MA COURBE") : juce::String()));
    }

    if (processor.canUndo() != lastCanUndo || processor.canRedo() != lastCanRedo)
    {
        lastCanUndo = processor.canUndo();
        lastCanRedo = processor.canRedo();
        undoButton.setEnabled (lastCanUndo);
        redoButton.setEnabled (lastCanRedo);
    }

    // nom de la courbe (calque fixe) : seulement quand il change
    const int nk = s.custom ? 1000 : s.curve;
    if (nk != nameKey)
    {
        nameKey = nk;
        curveTitle = s.custom ? juce::String ("MA COURBE") : Ui::upper (Ebb::curveName (s.curve));
        curveDesc = s.custom ? u8 ("dessin\xc3\xa9" "e dans l'\xc3\xa9" "diteur (\xc3\x89" "DITER)") : u8 (curveDescription (s.curve));
        overlayDirty = bgDirty = true;
        repaint();
    }

    // texte d'état : recalculé seulement quand le mode, le sidechain ou le tempo changent
    const int key = s.trigger * 100000 + (processor.isSidechainConnected() ? 50000 : 0) + juce::roundToInt (processor.getBpm() * 10.0);
    if (key != statusKey)
    {
        statusKey = key;
        if (auto st = statusText(); st != status)
        {
            status = st;
            overlayDirty = bgDirty = true;
            repaint();
        }
    }
    if (flashLeft > 0.0f)
    {
        flashLeft -= 1.0f / 60.0f;
        if (flashLeft <= 0.0f)
        {
            flashLeft = 0.0f;
            flashText.clear();
            overlayDirty = bgDirty = true;
            repaint();
        }
    }

    const bool mouseInside = isMouseOverOrDragging (true);
    mouseActivityFrames = mouseInside ? 90u : (mouseActivityFrames > 0 ? mouseActivityFrames - 1 : 0u);
    const bool active = curveMoving || mouseActivityFrames > 0;

    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (now - lastBgRefresh >= (active ? 0.125 : 0.25))
    {
        lastBgRefresh = now;
        bgDirty = true;
        repaint();
        return active;
    }
    if (active)
        repaint (14, 10, 36, 26);   // vague du logo

    // la goutte du bouton sous la souris ondule en continu
    juce::Component* hovered = nullptr;
    if (mouseInside)
        for (auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse(); c != nullptr && c != this; c = c->getParentComponent())
            if (dynamic_cast<Knob*> (c) != nullptr) { hovered = c; break; }
    if (hovered != nullptr) hovered->repaint();
    if (lastHovered != nullptr && lastHovered != hovered) lastHovered->repaint();
    lastHovered = hovered;
    return active;
}

//==============================================================================
void EbbContent::renderOverlay (float scale)
{
    const int w = juce::roundToInt ((float) designW * scale), h = juce::roundToInt ((float) designH * scale);
    if (! overlayImg.isValid() || overlayImg.getWidth() != w || overlayImg.getHeight() != h)
        overlayImg = juce::Image (juce::Image::ARGB, w, h, true);
    else
        overlayImg.clear (overlayImg.getBounds());

    juce::Graphics g (overlayImg);
    g.addTransform (juce::AffineTransform::scale (scale));
    Ui::drawGlassPanel (g, { 638.0f, 40.0f, 272.0f, 248.0f });
    Ui::drawGlassPanel (g, { 10.0f, 296.0f, 900.0f, 96.0f });

    // en-tête
    g.setColour (juce::Colours::white);
    g.setFont (Ui::font (18.0f, true, -0.02f));
    g.drawText ("ebb", 52, 8, 50, 24, juce::Justification::centredLeft);
    g.setColour (Ui::aqua);
    g.setFont (Ui::font (7.5f, true, 0.45f));
    g.drawText ("SIDECHAIN", 88, 8, 80, 14, juce::Justification::centredLeft);
    // le label : Ebb est une création GRVTY Records
    g.setColour (Ui::dim);
    g.setFont (Ui::font (6.5f, true, 0.3f));
    g.drawText ("GRVTY RECORDS", 88, 20, 90, 12, juce::Justification::centredLeft);

    g.setFont (Ui::font (8.0f, true, 0.18f));
    if (flashText.isNotEmpty())
    {
        g.setColour (flashOk ? Ui::aqua : Ui::coral);
        g.drawText (flashText, 180, 10, 460, 20, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (Ui::dim);
        g.drawText (status, 180, 10, 460, 20, juce::Justification::centredLeft);
    }

    // numéro de version : permet de vérifier d'un coup d'œil qu'Ableton charge la dernière compilation
    g.setColour (Ui::dim.withAlpha (0.7f));
    g.setFont (Ui::font (8.5f, false, 0.05f));
    g.drawText (versionText, designW - 108, 9, 74, 20, juce::Justification::centredRight);

    // barre d'outils
    g.setColour (Ui::dim);
    g.setFont (Ui::font (7.8f, true, 0.14f));
    g.drawText ("GRILLE", 100, 266, 40, 22, juce::Justification::centredLeft);

    // titres de la bande du bas
    auto title = [&g] (const juce::String& t, int x, int y, int wd)
    {
        g.setColour (Ui::dim);
        g.setFont (Ui::font (8.0f, true, 0.14f));
        g.drawText (t, x, y, wd, 12, juce::Justification::centredLeft);
    };
    title ("COURBE", 20, 300, 80);
    title (u8 ("DUR\xc3\x89" "E"), 432, 300, 80);
    title (u8 ("D\xc3\x89" "CLENCHEMENT"), 432, 342, 120);
    title ("MULTIBANDE", 798, 300, 80);

    // nom et description de la courbe
    g.setColour (nameKey == 1000 ? Ui::coral : juce::Colours::white);
    g.setFont (Ui::font (10.5f, true, 0.12f));
    g.drawText (curveTitle, 20, 344, 250, 16, juce::Justification::centredLeft);
    g.setColour (Ui::dim);
    g.setFont (Ui::font (8.4f));
    g.drawFittedText (curveDesc, 20, 361, 256, 24, juce::Justification::topLeft, 2, 0.9f);

    // séparations
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (int x : { 424, 662, 790 })
        g.drawVerticalLine (x, 304.0f, 384.0f);

    overlayScale = scale;
    overlayDirty = false;
}

void EbbContent::renderBackground (float scale)
{
    const int w = juce::roundToInt ((float) designW * scale), h = juce::roundToInt ((float) designH * scale);
    if (! bgCache.isValid() || bgCache.getWidth() != w || bgCache.getHeight() != h)
        bgCache = juce::Image (juce::Image::RGB, w, h, false);
    if (overlayDirty || ! overlayImg.isValid() || std::abs (scale - overlayScale) > 0.01f)
        renderOverlay (scale);

    const float lowScale = scale * 0.25f;
    const int lw = juce::roundToInt ((float) designW * lowScale), lh = juce::roundToInt ((float) designH * lowScale);
    if (! bgLow.isValid() || bgLow.getWidth() != lw || bgLow.getHeight() != lh)
        bgLow = juce::Image (juce::Image::RGB, lw, lh, false);
    {
        juce::Graphics lg (bgLow);
        lg.addTransform (juce::AffineTransform::scale (lowScale));
        Ui::drawWaterBackground (lg, getLocalBounds().toFloat(), false);
    }

    // agrandissement en deux temps : quart de résolution -> taille logique (lissé), puis -> taille physique
    // (pixels doublés) ; l'eau est floue de toute façon, et c'est 3 fois moins cher qu'un seul agrandissement lissé
    juce::Graphics g (bgCache);
    if (scale > 1.01f)
    {
        if (! bgMid.isValid() || bgMid.getWidth() != designW || bgMid.getHeight() != designH)
            bgMid = juce::Image (juce::Image::RGB, designW, designH, false);
        {
            juce::Graphics mg (bgMid);
            mg.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
            mg.drawImageTransformed (bgLow, juce::AffineTransform::scale ((float) designW / (float) lw, (float) designH / (float) lh));
        }
        g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
        g.drawImageTransformed (bgMid, juce::AffineTransform::scale ((float) w / (float) designW, (float) h / (float) designH));
    }
    else
    {
        g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
        g.drawImageTransformed (bgLow, juce::AffineTransform::scale ((float) w / (float) lw, (float) h / (float) lh));
    }
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    {
        juce::Graphics::ScopedSaveState save (g);
        g.addTransform (juce::AffineTransform::scale (scale));
        Ui::drawParticles (g, getLocalBounds().toFloat());
    }
    g.setOpacity (1.0f);
    g.drawImageAt (overlayImg, 0, 0);

    bgScale = scale;
    bgDirty = false;
}

void EbbContent::paint (juce::Graphics& g)
{
    const float scale = juce::jmax (1.0f, g.getInternalContext().getPhysicalPixelScaleFactor());
    if (bgDirty || ! bgCache.isValid() || std::abs (scale - bgScale) > 0.01f)
        renderBackground (scale);
    g.drawImageTransformed (bgCache, juce::AffineTransform::scale (1.0f / bgScale));

    // vague du logo (animée)
    const float t = Ui::now();
    auto& logo = Ui::scratchPath (5);
    for (int i = 0; i <= 24; ++i)
    {
        const float xx = 20.0f + (float) i, yy = 20.0f + 3.5f * std::sin ((float) i * 0.42f + t * 2.0f);
        if (i == 0) logo.startNewSubPath (xx, yy); else logo.lineTo (xx, yy);
    }
    g.setColour (Ui::aqua);
    g.strokePath (logo, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void EbbContent::resized()
{
    bgDirty = overlayDirty = true;
    curveView.setBounds (10, 40, 620, 222);

    // barre d'outils sous la vague
    editButton.setBounds (10, 266, 84, 22);
    gridChooser.setBounds (142, 267, 150, 20);
    undoButton.setBounds (302, 266, 30, 22);
    redoButton.setBounds (336, 266, 30, 22);
    mirrorButton.setBounds (376, 266, 78, 22);
    invertButton.setBounds (458, 266, 86, 22);
    clearButton.setBounds (548, 266, 82, 22);

    // panneau de droite
    amountKnob.setBounds (676, 44, 196, 158);
    tensionKnob.setBounds (664, 212, 96, 74);
    smoothKnob.setBounds (788, 212, 96, 74);

    // bande du bas
    strip.setBounds (18, 314, 396, 26);
    myCurvesButton.setBounds (286, 344, 128, 20);
    saveButton.setBounds (286, 368, 128, 20);
    ratePills.setBounds (430, 314, 224, 20);
    triggerPills.setBounds (430, 356, 180, 20);
    nudgeKnob.setBounds (668, 300, 56, 86);
    thresholdKnob.setBounds (726, 300, 56, 86);
    bandPills.setBounds (796, 314, 56, 70);
    crossoverKnob.setBounds (852, 300, 56, 86);
    if (savePanel != nullptr) savePanel->setBounds (getLocalBounds());
    infoButton.setBounds (designW - 30, 9, 20, 20);
    if (about != nullptr) about->setBounds (getLocalBounds());
}

void EbbContent::openAbout()
{
    if (about != nullptr)
        return;
    juce::Component::SafePointer<EbbContent> safe (this);
    about = std::make_unique<AboutPanel> ("Ebb", JucePlugin_VersionString, true, [safe]
    {
        // fermé depuis un de ses propres clics : on le détruit juste après
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->about.reset(); });
    });
    addAndMakeVisible (*about);
    about->setBounds (getLocalBounds());
}

//==============================================================================
EbbAudioProcessorEditor::EbbAudioProcessorEditor (EbbAudioProcessor& p)
    : AudioProcessorEditor (&p), content (p)
{
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (EbbContent::designW * 7 / 10, EbbContent::designH * 7 / 10,
                     EbbContent::designW * 2, EbbContent::designH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) EbbContent::designW / (double) EbbContent::designH);
    setSize (EbbContent::designW, EbbContent::designH);
    startTimerHz (60);
}

EbbAudioProcessorEditor::~EbbAudioProcessorEditor()
{
    stopTimer();
}

void EbbAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) EbbContent::designW;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, EbbContent::designW, EbbContent::designH);
}

void EbbAudioProcessorEditor::timerCallback()
{
    content.tick();
}

juce::AudioProcessorEditor* EbbAudioProcessor::createEditor()
{
    return new EbbAudioProcessorEditor (*this);
}
