// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "PluginEditor.h"

using ChordText::u8;

//==============================================================================
// ChoiceControl / PillButton / PillSelector
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

void PillButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = r.getHeight() * 0.5f;
    if (lit)
    {
        g.setColour (accent.withAlpha (0.35f));
        g.fillRoundedRectangle (r, corner);
    }
    g.setColour (juce::Colours::white.withAlpha (down ? 0.14f : (highlighted ? 0.09f : 0.045f)));
    g.fillRoundedRectangle (r, corner);
    g.setColour (accent.withAlpha (highlighted || lit ? 0.9f : (isEnabled() ? 0.5f : 0.2f)));
    g.drawRoundedRectangle (r, corner, 1.0f);
    g.setColour (highlighted || lit ? juce::Colours::white : (isEnabled() ? accent : Ui::dim));
    g.setFont (Ui::font (fontSize, true, 0.1f));
    g.drawFittedText (getButtonText(), r.toNearestInt().reduced (6, 0), juce::Justification::centred, 1, 0.8f);
}

void PillSelector::paint (juce::Graphics& g)
{
    const float h = (float) getHeight() / (float) names.size();
    for (int i = 0; i < names.size(); ++i)
    {
        const auto r = juce::Rectangle<float> (0.0f, (float) i * h, (float) getWidth(), h).reduced (2.0f, 3.0f);
        const bool on = i == current;
        g.setColour (on ? Ui::aqua.withAlpha (0.3f) : juce::Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (on ? Ui::aqua : juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        g.setColour (on ? juce::Colours::white : Ui::dim);
        g.setFont (Ui::font (9.5f, true, 0.16f));
        g.drawText (names[i], r, juce::Justification::centred);
    }
}

void PillSelector::mouseDown (const juce::MouseEvent& e)
{
    const int i = juce::jlimit (0, names.size() - 1, (int) (e.position.y / ((float) getHeight() / (float) names.size())));
    if (i != current) setCurrent (i);
}

//==============================================================================
// VoicingView
//==============================================================================
bool VoicingView::tick()
{
    const auto cs = processor.readChordSettings();
    const auto d = processor.getLastChord();
    const uint32_t s = d.counter * 2654435761u ^ (uint32_t) (cs.voicing.low * 257 + cs.voicing.high * 7) ^ (uint32_t) (cs.voicing.shift + 99) << 16;
    if (s == sig) return false;
    sig = s;
    repaint();
    return true;
}

void VoicingView::paint (juce::Graphics& g)
{
    const auto cs = processor.readChordSettings();
    const auto d = processor.getLastChord();
    const auto r = getLocalBounds().toFloat().reduced (4.0f, 4.0f);
    g.setColour (juce::Colours::white.withAlpha (0.03f));
    g.fillRoundedRectangle (r, 6.0f);

    const int lo = juce::jmin (cs.voicing.low, cs.voicing.high), hi = juce::jmax (cs.voicing.low, cs.voicing.high);
    int minN = lo - 6, maxN = hi + 6;
    for (int i = 0; i < d.count; ++i) { minN = juce::jmin (minN, d.notes[i] - 2); maxN = juce::jmax (maxN, d.notes[i] + 2); }
    auto plot = r.reduced (40.0f, 10.0f).withTrimmedRight (80.0f);
    auto xOf = [&] (int n) { return plot.getX() + plot.getWidth() * (float) (n - minN) / (float) juce::jmax (1, maxN - minN); };

    // plage autorisée
    g.setColour (Ui::aqua.withAlpha (0.08f));
    g.fillRect (juce::Rectangle<float> (xOf (lo), plot.getY(), xOf (hi) - xOf (lo), plot.getHeight()));
    g.setColour (Ui::aqua.withAlpha (0.35f));
    g.drawVerticalLine (juce::roundToInt (xOf (lo)), plot.getY(), plot.getBottom());
    g.drawVerticalLine (juce::roundToInt (xOf (hi)), plot.getY(), plot.getBottom());
    g.setFont (Ui::font (8.0f, true, 0.1f));
    g.setColour (Ui::dim);
    for (int n = (minN / 12) * 12; n <= maxN; n += 12)
        if (n >= minN)
        {
            g.setColour (juce::Colours::white.withAlpha (0.06f));
            g.drawVerticalLine (juce::roundToInt (xOf (n)), plot.getY(), plot.getBottom());
            g.setColour (Ui::dim.withAlpha (0.8f));
            g.drawText (Params::noteName (n), juce::Rectangle<float> (xOf (n) - 15.0f, plot.getBottom() - 11.0f, 30.0f, 11.0f), juce::Justification::centred);
        }

    // notes de l'accord : des gouttes, la basse en corail
    for (int i = 0; i < d.count; ++i)
    {
        const float x = xOf (d.notes[i]);
        const float y = plot.getY() + 6.0f + (float) (i % 3) * 13.0f;
        const auto col = d.notes[i] == d.bass ? Ui::coral : Ui::aqua;
        g.setColour (col.withAlpha (0.25f));
        g.fillEllipse (juce::Rectangle<float> (16.0f, 16.0f).withCentre ({ x, y + 7.0f }));
        g.setColour (col);
        g.fillEllipse (juce::Rectangle<float> (9.0f, 9.0f).withCentre ({ x, y + 7.0f }));
        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (Ui::font (8.0f, true));
        g.drawText (Params::noteName (d.notes[i]), juce::Rectangle<float> (x + 6.0f, y + 1.0f, 34.0f, 12.0f), juce::Justification::centredLeft);
    }

    g.setColour (Ui::dim);
    g.setFont (Ui::font (8.0f, true, 0.18f));
    g.drawText (u8 ("PLAGE  ") + Params::noteName (lo) + " - " + Params::noteName (hi), r.withTrimmedLeft (r.getWidth() - 86.0f).withHeight (14.0f).translated (0.0f, 6.0f),
                juce::Justification::centredLeft);
    g.drawText (juce::String (d.count) + " NOTES", r.withTrimmedLeft (r.getWidth() - 86.0f).withHeight (14.0f).translated (0.0f, 22.0f),
                juce::Justification::centredLeft);
}

//==============================================================================
// LearnBox
//==============================================================================
LearnBox::LearnBox (ConfluenceAudioProcessor& p) : processor (p)
{
    button.setTooltip (u8 ("Joue un accord (touches ensemble) puis rel\xc3\xa2" "che : il devient la forme \xc2\xab Appris \xc2\xbb du mode Accord fixe"));
    button.onClick = [this] { if (processor.isLearning()) processor.cancelLearn(); else processor.startLearn(); };
    addAndMakeVisible (button);
}

void LearnBox::resized()
{
    button.setBounds (getLocalBounds().withHeight (22).withY (16).withWidth (70));
}

bool LearnBox::tick()
{
    const bool learning = processor.isLearning();
    juce::String t;
    const auto& l = processor.getLearned();
    if (learning) t = u8 ("joue l'accord...");
    else if (l.count >= 2)
    {
        int notes[Theory::maxChordTones];
        for (int i = 0; i < l.count; ++i) notes[i] = 48 + l.iv[i];
        char buf[48];
        Theory::chordName (0, notes, l.count, false, buf, 48);
        t = u8 (buf + 1);
        if (t.isEmpty()) t = "maj";
    }
    else t = u8 ("rien d'appris");
    if (t == text && learning == wasLearning) return false;
    text = t;
    wasLearning = learning;
    button.lit = learning;
    button.setButtonText (learning ? u8 ("\xc3\x89" "COUTE") : juce::String ("LEARN"));
    button.repaint();
    repaint();
    return true;
}

void LearnBox::paint (juce::Graphics& g)
{
    g.setColour (Ui::dim);
    g.setFont (Ui::font (8.6f, true, 0.1f));
    g.drawText (u8 ("ACCORD APPRIS"), getLocalBounds().withHeight (12), juce::Justification::centredLeft);
    g.setColour (wasLearning ? Ui::coral : Ui::text);
    g.setFont (Ui::font (11.0f, true));
    g.drawFittedText (text, getLocalBounds().withTrimmedLeft (76).withY (16).withHeight (22), juce::Justification::centredLeft, 1, 0.7f);
    const auto& l = processor.getLearned();
    if (l.count >= 2 && ! wasLearning)
    {
        juce::StringArray iv;
        for (int i = 0; i < l.count; ++i) iv.add (juce::String ((int) l.iv[i]));
        g.setColour (Ui::dim);
        g.setFont (Ui::font (8.5f));
        g.drawText (iv.joinIntoString (" "), getLocalBounds().withY (42).withHeight (12), juce::Justification::centredLeft);
    }
}

//==============================================================================
// Contenu
//==============================================================================
juce::Rectangle<int> ConfluenceContent::cell (int col, int row, int spanCols) const
{
    constexpr int x0 = 292, y0 = 46, cw = 77, ch = 70;
    return { x0 + col * cw, y0 + row * ch, cw * spanCols - 4, ch - 4 };
}

juce::Component* ConfluenceContent::add (int tab, const char* id, const char* name, juce::Rectangle<int> r)
{
    auto* p = state.getParameter (id);
    jassert (p != nullptr);
    juce::Component* c = nullptr;
    if (dynamic_cast<juce::AudioParameterChoice*> (p) != nullptr)
    {
        c = owned.add (new ChoiceControl (state, id, u8 (name)));
        r = r.withHeight (38).withY (r.getY() + 12);
    }
    else if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr)
    {
        c = owned.add (new PillToggle (state, id, Ui::upper (u8 (name)), PillToggle::Icon::wave));
        r = r.withSizeKeepingCentre (r.getWidth() + 14, 26);
    }
    else
    {
        const auto range = p->getNormalisableRange();
        c = owned.add (new Knob (state, id, u8 (name), range.start < 0.0f && range.end > 0.0f));
    }
    // chaque contrôle garde son image : redessiné seulement si sa valeur change ou s'il est survolé
    c->setBufferedToImage (true);
    if (tab < 0) addAndMakeVisible (c);
    else         addChildComponent (c);
    c->setBounds (r);
    placed.push_back ({ c, tab });
    return c;
}

ConfluenceContent::ConfluenceContent (ConfluenceAudioProcessor& p)
    : processor (p), state (p.apvts), presetBar (p), sea (p), whirl (p), pads (p), voicingView (p), learnBox (p)
{
    setLookAndFeel (&look);
    setOpaque (true);
    versionText = "v" JucePlugin_VersionString;
    namespace id = ParamIDs;

    for (juce::Component* c : { (juce::Component*) &presetBar, (juce::Component*) &sea, (juce::Component*) &whirl,
                                (juce::Component*) &pads, (juce::Component*) &tabs, (juce::Component*) &panicButton,
                                (juce::Component*) &analyseButton })
        addAndMakeVisible (c);
    addChildComponent (voicingView);
    addChildComponent (learnBox);
    tabs.onChange = [this] (int t) { showTab (t); };

    // --- colonne de gauche
    add (-1, id::mode,  "Mode",    { 18, 32, 164, 40 });
    add (-1, id::root,  "Tonique", { 18, 76, 62, 40 });
    add (-1, id::scale, "Gamme",   { 82, 76, 100, 40 });
    add (-1, id::style, "Style",   { 18, 120, 164, 40 });

    for (int i = 0; i < 3; ++i)
    {
        auto* b = scalePills.add (new PillButton (""));
        b->fontSize = 8.5f;
        b->onClick = [this, i] { if (pillValid[(size_t) i]) applyScale (pillRoot[(size_t) i], pillScale[(size_t) i]); };
        addAndMakeVisible (b);
    }
    analyseButton.fontSize = 8.0f;
    analyseButton.setTooltip (u8 ("Joue quelques mesures (ou lance le clip) : les 3 gammes les plus compatibles s'affichent, un clic applique"));
    analyseButton.onClick = [this]
    {
        // ANALYSER -> STOP (les résultats restent affichés) -> GAMMES (retour aux gammes du style)
        if (processor.isAnalysing())  processor.stopAnalysis();
        else if (showingDetection)    showingDetection = false;
        else { processor.startAnalysis(); showingDetection = true; lastAnalysisCount = -1; }
        refreshScalePills();
    };
    panicButton.accent = Ui::coral;
    panicButton.fontSize = 8.5f;
    panicButton.setTooltip (u8 ("Coupe toutes les notes (et le sustain)"));
    panicButton.onClick = [this] { processor.requestPanic(); presetBar.flash (u8 ("NOTES COUP\xc3\x89" "ES"), true); };
    infoButton.onClick = [this] { openAbout(); };
    infoButton.setBufferedToImage (true);
    addAndMakeVisible (infoButton);
    pads.onMessage = [this] (const juce::String& m, bool ok) { presetBar.flash (m, ok); };

    // --- CHORD
    add (chordTab, id::chordType,  "Accord",         cell (0, 0));
    add (chordTab, id::outOfScale, "Hors gamme",     cell (1, 0));
    add (chordTab, id::blackKeys,  "Touches noires", cell (2, 0));
    add (chordTab, id::fixedShape, "Accord fixe",    cell (3, 0));
    learnBox.setBounds (cell (4, 0, 2).withTrimmedTop (2));
    placed.push_back ({ &learnBox, chordTab });
    add (chordTab, id::padBase,    "Note pad 1",     cell (6, 0));
    add (chordTab, id::outChannel, "Canal",          cell (0, 1));
    add (chordTab, id::preview,    "Preview",        cell (1, 1));
    add (chordTab, id::previewLevel, "Niveau",       cell (2, 1));

    // --- VOICING
    add (voicingTab, id::inversion, "Renversement",  cell (0, 0));
    add (voicingTab, id::spread,    "\xc3\x89" "cartement", cell (1, 0));
    add (voicingTab, id::maxVoices, "Voix max",      cell (2, 0));
    add (voicingTab, id::bass,      "Basse",         cell (3, 0));
    add (voicingTab, id::lowNote,   "Note basse",    cell (0, 1));
    add (voicingTab, id::highNote,  "Note haute",    cell (1, 1));
    add (voicingTab, id::octave,    "Octave",        cell (2, 1));
    add (voicingTab, id::transpose, "Transpo",       cell (3, 1));
    voicingView.setBounds (cell (4, 0, 5).withHeight (136));
    placed.push_back ({ &voicingView, voicingTab });

    // --- PLAY
    add (playTab, id::strum,         "Strum",       cell (0, 0));
    add (playTab, id::strumDir,      "Sens",        cell (1, 0));
    add (playTab, id::strumSync,     "Sync",        cell (2, 0));
    add (playTab, id::strumSyncRate, "Valeur",      cell (3, 0));
    add (playTab, id::humanTime,     "Human T",     cell (4, 0));
    add (playTab, id::humanVel,      "Human V",     cell (5, 0));
    add (playTab, id::durMode,       "Dur\xc3\xa9" "e", cell (6, 0));
    add (playTab, id::duration,      "Longueur",    cell (7, 0));
    add (playTab, id::latch,         "Latch",       cell (8, 0));
    add (playTab, id::velMode,       "Velocity",    cell (0, 1));
    add (playTab, id::velFixed,      "Vel fixe",    cell (1, 1));
    add (playTab, id::velCurve,      "Courbe",      cell (2, 1));
    add (playTab, id::balBass,       "\xc3\x89" "q. basse",  cell (3, 1));
    add (playTab, id::balMid,        "\xc3\x89" "q. milieu", cell (4, 1));
    add (playTab, id::balTop,        "\xc3\x89" "q. aigu",   cell (5, 1));

    for (juce::Component* c : { (juce::Component*) &presetBar, (juce::Component*) &tabs, (juce::Component*) &panicButton,
                                (juce::Component*) &analyseButton, (juce::Component*) &learnBox })
        c->setBufferedToImage (true);
    for (auto* b : scalePills)
        b->setBufferedToImage (true);

    setSize (designW, designH);
    showTab (chordTab);
    refreshScalePills();
}

ConfluenceContent::~ConfluenceContent()
{
    setLookAndFeel (nullptr);
}

void ConfluenceContent::showTab (int t)
{
    currentTab = t;
    for (auto& pl : placed)
        if (pl.tab >= 0)
            pl.comp->setVisible (pl.tab == t);
    bgDirty = true;
    repaint();
}

void ConfluenceContent::applyScale (int root, int scale)
{
    auto set = [this] (const char* pid, int v)
    {
        if (auto* prm = state.getParameter (pid))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 ((float) v));
            prm->endChangeGesture();
        }
    };
    set (ParamIDs::root, root);
    set (ParamIDs::scale, scale);
    if (showingDetection && ! processor.isAnalysing())
        showingDetection = false;
    presetBar.flash (juce::String (Theory::pcName (root, Theory::useFlats (scale, root))) + " " + Ui::upper (u8 (Theory::scaleInfo (scale).name)), true);
    refreshScalePills();
}

/** Les 3 pilules : gammes conseillées pour le style, ou résultats de la détection. */
void ConfluenceContent::refreshScalePills()
{
    const int style = (int) std::lround (state.getRawParameterValue (ParamIDs::style)->load());
    const int root = (int) std::lround (state.getRawParameterValue (ParamIDs::root)->load());
    const int curScale = (int) std::lround (state.getRawParameterValue (ParamIDs::scale)->load());
    lastStyle = style;
    lastRoot = root;

    if (showingDetection)
    {
        const auto ranking = processor.getAnalysisRanking (3);
        lastAnalysisCount = processor.getAnalysisNoteCount();
        pillsTitle = u8 ("D\xc3\x89TECTION (") + juce::String (lastAnalysisCount) + ")";
        for (int i = 0; i < 3; ++i)
        {
            auto* b = scalePills[i];
            pillValid[(size_t) i] = i < (int) ranking.size();
            if (pillValid[(size_t) i])
            {
                const auto& c = ranking[(size_t) i];
                pillRoot[(size_t) i] = c.root;
                pillScale[(size_t) i] = c.scale;
                b->setButtonText (juce::String (Theory::pcName (c.root, Theory::useFlats (c.scale, c.root))) + " " + u8 (Theory::scaleInfo (c.scale).name)
                                  + u8 (" \xc2\xb7 ") + u8 (Theory::scaleInfo (c.scale).mood));
                b->lit = c.root == root && c.scale == curScale;
            }
            else
            {
                b->setButtonText (i == 0 ? u8 ("joue quelques mesures...") : juce::String());
                b->lit = false;
            }
            b->setEnabled (pillValid[(size_t) i]);
            b->repaint();
        }
    }
    else
    {
        pillsTitle = u8 ("GAMMES DU STYLE");
        const auto scales = Harmony::styleScales (style);
        for (int i = 0; i < 3; ++i)
        {
            auto* b = scalePills[i];
            pillValid[(size_t) i] = true;
            pillRoot[(size_t) i] = root;
            pillScale[(size_t) i] = scales[(size_t) i];
            b->setButtonText (u8 (Theory::scaleInfo (scales[(size_t) i]).name) + u8 (" \xc2\xb7 ") + u8 (Theory::scaleInfo (scales[(size_t) i]).mood));
            b->lit = scales[(size_t) i] == curScale;
            b->setEnabled (true);
            b->repaint();
        }
    }
    analyseButton.lit = processor.isAnalysing();
    analyseButton.setButtonText (processor.isAnalysing() ? "STOP" : (showingDetection ? juce::String ("GAMMES") : juce::String ("ANALYSER")));
    analyseButton.repaint();
    repaint (10, 172, 180, 20);

    // le titre des pilules fait partie du calque fixe du fond
    if (overlayDetection != showingDetection || ! overlayImg.isValid() || pillsTitle != lastOverlayTitle)
    {
        overlayDetection = showingDetection;
        lastOverlayTitle = pillsTitle;
        overlayDirty = bgDirty = true;
        repaint (20, 176, 104, 14);
    }
}

bool ConfluenceContent::tick()
{
    processor.pollMessageThread();
    const bool diceMoving = presetBar.refresh();
    const bool seaAlive = sea.tick();
    const bool whirlChanged = whirl.tick();
    const bool padsAnim = pads.tick();
    learnBox.tick();
    if (currentTab == voicingTab) voicingView.tick();
    if (whirlChanged) whirl.repaint();

    // pilules de gammes : style ou tonique changés, nouvelle note pendant l'analyse
    {
        const int style = (int) std::lround (state.getRawParameterValue (ParamIDs::style)->load());
        const int root = (int) std::lround (state.getRawParameterValue (ParamIDs::root)->load());
        if (style != lastStyle || root != lastRoot
            || (showingDetection && processor.getAnalysisNoteCount() != lastAnalysisCount))
        {
            if (style != lastStyle && ! processor.isAnalysing()) showingDetection = false;
            refreshScalePills();
        }
    }

    const bool mouseInside = isMouseOverOrDragging (true);
    mouseActivityFrames = mouseInside ? 90u : (mouseActivityFrames > 0 ? mouseActivityFrames - 1 : 0u);
    const bool learning = processor.isLearning(), analysing = processor.isAnalysing();
    const bool active = seaAlive || padsAnim || diceMoving || mouseActivityFrames > 0 || learning || analysing;
    activityReason = juce::String (seaAlive ? "mer " : "") + (padsAnim ? "pads " : "") + (diceMoving ? "de " : "")
                   + (mouseActivityFrames > 0 ? "souris " : "") + (learning ? "learn " : "") + (analysing ? "analyse" : "");

    // fond (eau + panneaux) : 8 images/s en activité, 4 au repos ; on redessine alors toute la fenêtre
    const double now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    if (now - lastBgRefresh >= (active ? 0.125 : 0.25))
    {
        lastBgRefresh = now;
        bgDirty = true;
        repaint();
        return active;
    }

    // entre deux : seulement ce qui bouge, à 60 images/s
    if (active)
    {
        if (seaAlive) sea.repaint();
        whirl.repaint();
        repaint (14, 10, 36, 26);   // vague du logo
    }

    // le bouton-goutte sous la souris ondule en continu
    juce::Component* hovered = nullptr;
    if (mouseInside)
        for (auto* c = juce::Desktop::getInstance().getMainMouseSource().getComponentUnderMouse(); c != nullptr && c != this; c = c->getParentComponent())
            if (dynamic_cast<Knob*> (c) != nullptr) { hovered = c; break; }
    if (hovered != nullptr) hovered->repaint();
    if (lastHovered != nullptr && lastHovered != hovered) lastHovered->repaint();
    lastHovered = hovered;
    return active;
}

void ConfluenceContent::renderOverlay (float scale)
{
    const int w = juce::roundToInt ((float) designW * scale), h = juce::roundToInt ((float) designH * scale);
    if (! overlayImg.isValid() || overlayImg.getWidth() != w || overlayImg.getHeight() != h)
        overlayImg = juce::Image (juce::Image::ARGB, w, h, true);
    else
        overlayImg.clear (overlayImg.getBounds());

    juce::Graphics g (overlayImg);
    g.addTransform (juce::AffineTransform::scale (scale));
    Ui::drawGlassPanel (g, { 10.0f, 40.0f, 180.0f, 420.0f });
    Ui::drawGlassPanel (g, { 196.0f, 40.0f, 794.0f, 146.0f });
    Ui::drawGlassPanel (g, { 766.0f, 192.0f, 224.0f, 268.0f });
    const auto seaR = sea.getBounds().toFloat();
    Ui::setVerticalGradient (g, 7, juce::Colour (0x40061a22), seaR.getY(), juce::Colour (0xd0010608), seaR.getBottom());
    g.fillRoundedRectangle (seaR, 10.0f);
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (seaR.reduced (0.5f), 10.0f, 1.0f);

    g.setColour (juce::Colours::white);
    g.setFont (Ui::font (18.0f, true, -0.02f));
    g.drawText ("confluence", 52, 8, 110, 24, juce::Justification::centredLeft);
    g.setColour (Ui::aqua);
    g.setFont (Ui::font (7.5f, true, 0.45f));
    g.drawText ("CHORDS", 142, 8, 80, 14, juce::Justification::centredLeft);
    // le label : Confluence est une création GRVTY Records
    g.setColour (Ui::dim);
    g.setFont (Ui::font (6.5f, true, 0.3f));
    g.drawText ("GRVTY RECORDS", 142, 20, 120, 12, juce::Justification::centredLeft);

    // numéro de version : permet de vérifier d'un coup d'œil qu'Ableton charge la dernière compilation
    g.setColour (Ui::dim.withAlpha (0.7f));
    g.setFont (Ui::font (8.5f, false, 0.05f));
    g.drawText (versionText, 908, 9, 54, 20, juce::Justification::centredRight);

    g.setColour (Ui::dim);
    g.setFont (Ui::font (8.0f, true, 0.2f));
    g.drawText (u8 ("ENCHA\xc3\x8eNEMENTS"), 10, 442, 180, 14, juce::Justification::centred);
    g.setFont (Ui::font (8.5f, true, 0.2f));
    g.drawText ("PADS", 776, 197, 40, 18, juce::Justification::centredLeft);

    // titre des pilules de gammes (gammes du style ou détection)
    g.setColour (overlayDetection ? Ui::coral : Ui::dim);
    g.setFont (Ui::font (8.0f, true, 0.16f));
    g.drawText (pillsTitle, 20, 176, 104, 14, juce::Justification::centredLeft);

    // séparation onglets / réglages
    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawVerticalLine (286, 50.0f, 176.0f);

    overlayScale = scale;
    overlayDirty = false;
}

void ConfluenceContent::upscaleWater()
{
    // interpolation bilinéaire en deux passes (lignes source agrandies en largeur, puis mélange vertical),
    // en virgule fixe sur 8 bits : même résultat que la qualité « medium » de JUCE, sans passer par le moteur de rendu
    const juce::Image::BitmapData src (bgLow, juce::Image::BitmapData::readOnly);
    juce::Image::BitmapData dst (bgCache, juce::Image::BitmapData::writeOnly);
    const int sw = src.width, sh = src.height, dw = dst.width, dh = dst.height, ps = src.pixelStride;
    if (sw < 2 || sh < 2 || ps != dst.pixelStride)
        return;
    const int rowLen = dw * ps;
    if ((int) upX0.size() != dw || (int) upRows.size() != sh * rowLen)   // tables refaites seulement si la taille change
    {
        upX0.resize ((size_t) dw);
        upWx.resize ((size_t) dw);
        upRows.resize ((size_t) (sh * rowLen));
        for (int x = 0; x < dw; ++x)
        {
            const float fx = juce::jlimit (0.0f, (float) (sw - 1), ((float) x + 0.5f) * (float) sw / (float) dw - 0.5f);
            const int x0 = juce::jmin ((int) fx, sw - 2);
            upX0[(size_t) x] = x0;
            upWx[(size_t) x] = (uint16_t) juce::jlimit (0, 256, juce::roundToInt ((fx - (float) x0) * 256.0f));
        }
    }
    for (int y = 0; y < sh; ++y)
    {
        const uint8_t* r = src.getLinePointer (y);
        uint16_t* h = upRows.data() + (size_t) (y * rowLen);
        for (int x = 0; x < dw; ++x)
        {
            const int xa = upX0[(size_t) x] * ps, wx = upWx[(size_t) x];
            for (int c = 0; c < ps; ++c)
                h[x * ps + c] = (uint16_t) (r[xa + c] * (256 - wx) + r[xa + ps + c] * wx);
        }
    }
    for (int y = 0; y < dh; ++y)
    {
        const float fy = juce::jlimit (0.0f, (float) (sh - 1), ((float) y + 0.5f) * (float) sh / (float) dh - 0.5f);
        const int y0 = juce::jmin ((int) fy, sh - 2);
        const uint32_t wy = (uint32_t) juce::jlimit (0, 256, juce::roundToInt ((fy - (float) y0) * 256.0f)), iwy = 256u - wy;
        const uint16_t* h0 = upRows.data() + (size_t) (y0 * rowLen);
        const uint16_t* h1 = h0 + rowLen;
        uint8_t* out = dst.getLinePointer (y);
        for (int i = 0; i < rowLen; ++i)
            out[i] = (uint8_t) ((h0[i] * iwy + h1[i] * wy + 32768u) >> 16);
    }
}

void ConfluenceContent::renderBackground (float scale)
{
    const int w = juce::roundToInt ((float) designW * scale), h = juce::roundToInt ((float) designH * scale);
    if (! bgCache.isValid() || bgCache.getWidth() != w || bgCache.getHeight() != h)
        bgCache = juce::Image (juce::Image::RGB, w, h, false);
    if (overlayDirty || ! overlayImg.isValid() || std::abs (scale - overlayScale) > 0.01f)
        renderOverlay (scale);

    // l'eau (dégradés et courants très doux) est calculée au quart de la résolution puis agrandie ;
    // les particules sont dessinées ensuite à pleine résolution pour rester nettes
    const float lowScale = scale * 0.25f;
    const int lw = juce::roundToInt ((float) designW * lowScale), lh = juce::roundToInt ((float) designH * lowScale);
    if (! bgLow.isValid() || bgLow.getWidth() != lw || bgLow.getHeight() != lh)
        bgLow = juce::Image (juce::Image::RGB, lw, lh, false);
    {
        juce::Graphics lg (bgLow);
        lg.addTransform (juce::AffineTransform::scale (lowScale));
        Ui::drawWaterBackground (lg, getLocalBounds().toFloat(), false);
    }

   #if JUCE_LINUX
    // Linux (moteur de rendu logiciel) : agrandissement bilinéaire direct, 4 à 5 fois plus rapide que
    // drawImageTransformed ; sur Mac et Windows, CoreGraphics et Direct2D le font eux-mêmes très vite
    upscaleWater();
    juce::Graphics g (bgCache);
   #else
    juce::Graphics g (bgCache);
    g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
    g.drawImageTransformed (bgLow, juce::AffineTransform::scale ((float) w / (float) lw, (float) h / (float) lh));
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
   #endif
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

void ConfluenceContent::paint (juce::Graphics& g)
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

void ConfluenceContent::openAbout()
{
    if (about != nullptr)
        return;
    juce::Component::SafePointer<ConfluenceContent> safe (this);
    about = std::make_unique<AboutPanel> (versionText, [safe]
    {
        // détruit après la fin de son propre appel
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->about.reset(); });
    });
    addAndMakeVisible (*about);
    about->setBounds (getLocalBounds());
}

void ConfluenceContent::resized()
{
    bgDirty = overlayDirty = true;
    presetBar.setBounds (300, 6, 420, 26);
    panicButton.setBounds (848, 9, 56, 20);
    infoButton.setBounds (966, 9, 20, 20);
    if (about != nullptr) about->setBounds (getLocalBounds());
    analyseButton.setBounds (122, 174, 62, 18);
    for (int i = 0; i < 3; ++i)
        scalePills[i]->setBounds (16, 195 + i * 21, 168, 20);
    whirl.setBounds (12, 262, 176, 180);
    tabs.setBounds (204, 52, 76, 124);
    sea.setBounds (196, 192, 564, 268);
    pads.setBounds (774, 196, 208, 258);
}

//==============================================================================
// Éditeur : met le contenu à l'échelle de la fenêtre
//==============================================================================
ConfluenceAudioProcessorEditor::ConfluenceAudioProcessorEditor (ConfluenceAudioProcessor& p)
    : AudioProcessorEditor (&p), content (p)
{
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (ConfluenceContent::designW * 7 / 10, ConfluenceContent::designH * 7 / 10,
                     ConfluenceContent::designW * 2, ConfluenceContent::designH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) ConfluenceContent::designW / (double) ConfluenceContent::designH);
    setSize (ConfluenceContent::designW, ConfluenceContent::designH);
    startTimerHz (60);
}

ConfluenceAudioProcessorEditor::~ConfluenceAudioProcessorEditor()
{
    stopTimer();
}

void ConfluenceAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) ConfluenceContent::designW;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, ConfluenceContent::designW, ConfluenceContent::designH);
}

void ConfluenceAudioProcessorEditor::timerCallback()
{
    content.tick();
}

//==============================================================================
juce::AudioProcessorEditor* ConfluenceAudioProcessor::createEditor()
{
    return new ConfluenceAudioProcessorEditor (*this);
}
