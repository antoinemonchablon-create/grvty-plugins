// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "PluginEditor.h"

//==============================================================================
// ChoiceControl
//==============================================================================
ChoiceControl::ChoiceControl (juce::AudioProcessorValueTreeState& state, const juce::String& paramID, const juce::String& name)
{
    title.setText (name.toUpperCase(), juce::dontSendNotification);
    title.setFont (Ui::font (8.8f, true, 0.12f));
    title.setColour (juce::Label::textColourId, Ui::dim);
    title.setJustificationType (juce::Justification::centred);
    title.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (title);

    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (paramID)))
        box.addItemList (p->choices, 1);
    addAndMakeVisible (box);
    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramID, box);
}

void ChoiceControl::resized()
{
    auto r = getLocalBounds();
    title.setBounds (r.removeFromTop (12));
    r.removeFromTop (3);
    box.setBounds (r.removeFromTop (20).reduced (2, 0));
}

//==============================================================================
// PillButton / PillSelector
//==============================================================================
void PillButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (1.0f);
    const float corner = r.getHeight() * 0.5f;
    g.setColour (juce::Colours::white.withAlpha (down ? 0.14f : (highlighted ? 0.09f : 0.045f)));
    g.fillRoundedRectangle (r, corner);
    g.setColour (Ui::aqua.withAlpha (highlighted ? 0.9f : 0.5f));
    g.drawRoundedRectangle (r, corner, 1.0f);
    g.setColour (highlighted ? juce::Colours::white : Ui::aqua);
    g.setFont (Ui::font (9.0f, true, 0.12f));
    g.drawText (getButtonText(), r, juce::Justification::centred);
}

void PillSelector::paint (juce::Graphics& g)
{
    const float h = (float) getHeight() / (float) names.size();
    for (int i = 0; i < names.size(); ++i)
    {
        const auto r = juce::Rectangle<float> (0.0f, (float) i * h, (float) getWidth(), h).reduced (2.0f, 2.0f);
        const bool on = i == current;
        g.setColour (on ? Ui::aqua.withAlpha (0.3f) : juce::Colours::white.withAlpha (0.04f));
        g.fillRoundedRectangle (r, r.getHeight() * 0.5f);
        g.setColour (on ? Ui::aqua : juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (r, r.getHeight() * 0.5f, 1.0f);
        g.setColour (on ? juce::Colours::white : Ui::dim);
        g.setFont (Ui::font (9.0f, true, 0.14f));
        g.drawText (names[i], r, juce::Justification::centred);
    }
}

void PillSelector::mouseDown (const juce::MouseEvent& e)
{
    const int i = juce::jlimit (0, names.size() - 1, (int) (e.position.y / ((float) getHeight() / (float) names.size())));
    if (i != current)
    {
        current = i;
        repaint();
        if (onChange) onChange (i);
    }
}

//==============================================================================
// CurveView
//==============================================================================
float CurveView::valueAt (float x) const
{
    auto v = [this] (const juce::String& id) { return state.getRawParameterValue (id)->load(); };

    if (kind == Kind::amp)
    {
        // A, D, S (largeur fixe), R ; temps à l'échelle logarithmique pour rester lisible
        const float a = std::log1p (v (ParamIDs::ampAttack)), d = std::log1p (v (ParamIDs::ampDecay));
        const float r = std::log1p (v (ParamIDs::ampRelease)), s = v (ParamIDs::ampSustain);
        const float hold = 2.0f, total = a + d + hold + r;
        float t = x * total;
        if (t < a) return t / juce::jmax (0.001f, a);
        t -= a;
        if (t < d) return s + (1.0f - s) * std::pow (1.0f - t / d, 2.5f);
        t -= d;
        if (t < hold) return s;
        t -= hold;
        return s * std::pow (juce::jmax (0.0f, 1.0f - t / juce::jmax (0.001f, r)), 2.5f);
    }
    if (kind == Kind::mod)
    {
        const float a = std::log1p (v (ParamIDs::modAttack)), d = std::log1p (v (ParamIDs::modDecay));
        const float curve = v (ParamIDs::modCurve);
        const float total = a + d;
        float t = x * total;
        if (t < a) return t / juce::jmax (0.001f, a);
        t -= a;
        return std::pow (juce::jmax (0.0f, 1.0f - t / juce::jmax (0.001f, d)), std::pow (2.0f, -curve * 3.0f));
    }
    // LFO : deux cycles, ramenés entre 0 et 1
    const int shape = (int) v (ParamIDs::lfoShape (lfoIndex));
    const float ph = std::fmod (x * 2.0f, 1.0f);
    return 0.5f + 0.5f * Lfo::shapeValue (shape, ph, (int64_t) (x * 2.0f), 0xA5u + (uint32_t) lfoIndex * 977u);
}

void CurveView::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (5.0f, 6.0f).withTrimmedTop (12.0f);
    g.setColour (juce::Colours::white.withAlpha (0.03f));
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), 6.0f);

    line.clear();
    fill.clear();
    const int n = 90;
    for (int i = 0; i <= n; ++i)
    {
        const float x = (float) i / (float) n;
        const float y = b.getBottom() - juce::jlimit (0.0f, 1.0f, valueAt (x)) * b.getHeight();
        const float px = b.getX() + x * b.getWidth();
        if (i == 0) { line.startNewSubPath (px, y); fill.startNewSubPath (px, b.getBottom()); }
        line.lineTo (px, y);
        fill.lineTo (px, y);
    }
    fill.lineTo (b.getRight(), b.getBottom());
    fill.closeSubPath();

    const auto col = kind == Kind::lfo ? Ui::coral : Ui::aqua;
    g.setGradientFill (juce::ColourGradient (col.withAlpha (0.28f), 0.0f, b.getY(), col.withAlpha (0.0f), 0.0f, b.getBottom(), false));
    g.fillPath (fill);
    g.setColour (col);
    g.strokePath (line, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (Ui::dim);
    g.setFont (Ui::font (8.0f, true, 0.2f));
    g.drawText (kind == Kind::amp ? "AMP" : (kind == Kind::mod ? "MOD ENV" : (lfoIndex == 0 ? "LFO 1" : "LFO 2")),
                getLocalBounds().reduced (7, 4), juce::Justification::topLeft);
}

//==============================================================================
// Contenu
//==============================================================================
juce::Rectangle<int> UndertowContent::cell (int col, int row, int spanCols) const
{
    constexpr int x0 = 402, y0 = 80, cw = 66, ch = 88;
    return { x0 + col * cw, y0 + row * ch, cw * spanCols, ch - 6 };
}

juce::Component* UndertowContent::add (int page, const juce::String& id, const juce::String& name, juce::Rectangle<int> r, PillToggle::Icon icon)
{
    auto* p = state.getParameter (id);
    jassert (p != nullptr);
    juce::Component* c = nullptr;

    if (dynamic_cast<juce::AudioParameterChoice*> (p) != nullptr)
        c = owned.add (new ChoiceControl (state, id, name));
    else if (dynamic_cast<juce::AudioParameterBool*> (p) != nullptr)
        c = owned.add (new PillToggle (state, id, name.toUpperCase(), icon));
    else
    {
        const auto range = p->getNormalisableRange();
        c = owned.add (new Knob (state, id, name, range.start < 0.0f && range.end > 0.0f));
    }

    // chaque contrôle garde son image : il n'est redessiné que si sa valeur change ou s'il est survolé
    c->setBufferedToImage (true);
    if (page < 0) addAndMakeVisible (c);
    else          addChildComponent (c);
    c->setBounds (r);
    placed.push_back ({ c, page });
    return c;
}

UndertowContent::UndertowContent (UndertowAudioProcessor& p)
    : processor (p), state (p.apvts), presetBar (p), fmScreen (p), grid (p),
      tabs ({ "OSC", "FILTER", "MOD", "FX" }), meter (p.outputLevel)
{
    setLookAndFeel (&look);
    setOpaque (true);
    namespace id = ParamIDs;
    versionText = "v" JucePlugin_VersionString;

    addAndMakeVisible (presetBar);
    addAndMakeVisible (fmScreen);
    addAndMakeVisible (grid);
    addAndMakeVisible (tabs);
    addAndMakeVisible (meter);
    tabs.onChange = [this] (int t) { showTab (t); };

    // bouton « i » : panneau À propos (licence, code source, confidentialité, composants tiers)
    infoButton.onClick = [this] { openAbout(); };
    addAndMakeVisible (infoButton);

    // --- OSC : oscillateurs et FM
    add (oscTab, id::oscAShape,  "Shape A",  cell (0, 0));
    add (oscTab, id::oscACoarse, "Coarse",   cell (1, 0));
    add (oscTab, id::oscAFine,   "Fine A",   cell (2, 0));
    add (oscTab, id::oscBShape,  "Shape B",  cell (3, 0));
    add (oscTab, id::oscBRatio,  "Ratio",    cell (4, 0).withHeight (38));
    add (oscTab, id::oscBFree,   "Free",     cell (4, 0).withTrimmedTop (44).withHeight (24).reduced (3, 0), PillToggle::Icon::wave);
    add (oscTab, id::oscBFreeRatio, "Free Ratio", cell (5, 0));
    add (oscTab, id::oscBFine,   "Fine B",   cell (6, 0));
    add (oscTab, id::oscMix,     "A / B",    cell (0, 1));
    add (oscTab, id::fmIndex,    "Index",    cell (1, 1));
    add (oscTab, id::fmFeedback, "Feedback", cell (2, 1));
    add (oscTab, id::fmMode,     "FM Mode",  cell (3, 1).withHeight (38));
    add (oscTab, id::glide,      "Glide",    cell (4, 1));
    add (oscTab, id::legato,     "Legato",   cell (5, 1).withTrimmedTop (28).withHeight (24).expanded (6, 0), PillToggle::Icon::wave);
    add (oscTab, id::velSens,    "Velocity", cell (6, 1));

    // --- FILTER : filtre et ampli
    add (filterTab, id::filterType,  "Filter",  cell (0, 0).withHeight (38));
    add (filterTab, id::filterSlope, "Slope",   cell (0, 0).withTrimmedTop (42).withHeight (38));
    add (filterTab, id::cutoff,      "Cutoff",  cell (1, 0));
    add (filterTab, id::resonance,   "Reso",    cell (2, 0));
    add (filterTab, id::filterEnv,   "Env",     cell (3, 0));
    add (filterTab, id::filterDrive, "Drive",   cell (4, 0));
    add (filterTab, id::keyTrack,    "Key Trk", cell (5, 0));
    add (filterTab, id::ampAttack,   "Attack",  cell (0, 1));
    add (filterTab, id::ampDecay,    "Decay",   cell (1, 1));
    add (filterTab, id::ampSustain,  "Sustain", cell (2, 1));
    add (filterTab, id::ampRelease,  "Release", cell (3, 1));
    ampCurve.setBounds (cell (4, 1, 3).reduced (4, 6));
    addChildComponent (ampCurve);
    placed.push_back ({ &ampCurve, filterTab });

    // --- MOD : enveloppe Mod + 2 LFO (un seul affiché à la fois)
    add (modTab, id::modAttack,   "Mod Att",  cell (0, 0));
    add (modTab, id::modDecay,    "Mod Dec",  cell (1, 0));
    add (modTab, id::modCurve,    "Curve",    cell (2, 0));
    add (modTab, id::modToIndex,  "> Index",  cell (3, 0));
    add (modTab, id::modToCutoff, "> Cutoff", cell (4, 0));
    add (modTab, id::modToPitch,  "> Pitch",  cell (5, 0));
    modCurve.setBounds (cell (6, 0).reduced (2, 8));
    addChildComponent (modCurve);
    placed.push_back ({ &modCurve, modTab });

    lfoSelector.setBounds (cell (0, 1).reduced (4, 14));
    addChildComponent (lfoSelector);
    placed.push_back ({ &lfoSelector, modTab });
    lfoSelector.onChange = [this] (int) { updateLfoVisibility(); };

    for (int i = 0; i < Params::numLfos; ++i)
    {
        auto addLfo = [&] (juce::Component* c) { placed.back().lfo = i; return c; };
        addLfo (add (modTab, id::lfoShape (i), "Shape", cell (1, 1).withHeight (38)));
        addLfo (add (modTab, id::lfoSync (i),   "Sync",   cell (2, 1).withTrimmedTop (8).withHeight (24).reduced (1, 0), PillToggle::Icon::sync));
        addLfo (add (modTab, id::lfoRetrig (i), "Retrig", cell (2, 1).withTrimmedTop (40).withHeight (24).expanded (6, 0), PillToggle::Icon::reverse));
        lfoRateKnob[(size_t) i]    = addLfo (add (modTab, id::lfoRate (i),     "Rate", cell (3, 1)));
        lfoSyncRateBox[(size_t) i] = addLfo (add (modTab, id::lfoSyncRate (i), "Rate", cell (3, 1).withHeight (38)));
        addLfo (add (modTab, id::lfoDest (i),  "Dest",  cell (4, 1).withHeight (38)));
        addLfo (add (modTab, id::lfoDepth (i), "Depth", cell (5, 1)));
    }
    lfoCurve.setBounds (cell (6, 1).reduced (2, 8));
    addChildComponent (lfoCurve);
    placed.push_back ({ &lfoCurve, modTab });

    // --- FX
    add (fxTab, id::delayTime,     "Delay",     cell (0, 0).withHeight (38));
    add (fxTab, id::delayFeedback, "Feedback",  cell (1, 0));
    add (fxTab, id::delayTone,     "Tone",      cell (2, 0));
    add (fxTab, id::delayMix,      "Dly Mix",   cell (3, 0));
    add (fxTab, id::reverbSize,    "Size",      cell (4, 0));
    add (fxTab, id::reverbDamp,    "Damp",      cell (5, 0));
    add (fxTab, id::reverbMix,     "Rev Mix",   cell (6, 0));
    add (fxTab, id::fxDrive,       "Drive",     cell (0, 1));
    add (fxTab, id::oversampling,  "Quality",   cell (1, 1).withHeight (38));
    add (fxTab, id::outGain,       "Volume",    cell (2, 1));

    // --- séquenceur (toujours visible) : trois lignes à gauche, la grille à droite
    {
        int x = 16;
        auto row = [&] (int y, int h, std::initializer_list<std::tuple<const char*, const char*, int>> items)
        {
            x = 16;
            for (auto [pid, nm, w] : items)
            {
                add (-1, pid, nm, { x, y, w, h });
                x += w + 2;
            }
        };
        row (274, 38, { { id::seqMode, "Mode", 60 }, { id::seqRate, "Rate", 60 }, { id::scale, "Scale", 98 }, { id::rootNote, "Root", 56 } });
        row (318, 70, { { id::seqOctave, "Oct", 44 }, { id::seqLength, "Steps", 44 }, { id::swing, "Swing", 44 },
                        { id::accent, "Accent", 44 }, { id::mutateAmount, "Mutate", 44 } });
        add (-1, id::mutateBars, "Every", { x, 318, 58, 38 });
        row (392, 66, { { id::modLaneDest, "Mod Lane", 66 }, { id::modLaneLength, "Lane Stp", 46 }, { id::modLaneDepth, "Lane Amt", 46 } });
    }

    for (auto* b : { &euclidButton, &randomButton, &leftButton, &rightButton })
    {
        b->setBufferedToImage (true);
        addAndMakeVisible (b);
    }
    for (juce::Component* c : { (juce::Component*) &presetBar, (juce::Component*) &tabs, (juce::Component*) &lfoSelector,
                                (juce::Component*) &ampCurve, (juce::Component*) &modCurve, (juce::Component*) &lfoCurve })
        c->setBufferedToImage (true);
    euclidButton.setBounds (180, 400, 60, 24);
    randomButton.setBounds (242, 400, 52, 24);
    leftButton.setBounds   (180, 428, 56, 24);
    rightButton.setBounds  (238, 428, 56, 24);

    euclidButton.setTooltip ("Rythme euclidien sur la longueur du motif (les notes sont gardees)");
    randomButton.setTooltip ("Notes au hasard dans la gamme (le rythme est garde)");
    leftButton.setTooltip ("Decaler le motif d'un pas vers la gauche");
    rightButton.setTooltip ("Decaler le motif d'un pas vers la droite");

    euclidButton.onClick = [this] { showEuclidMenu(); };
    randomButton.onClick = [this]
    {
        const int len = (int) state.getRawParameterValue (ParamIDs::seqLength)->load();
        const int sc  = (int) state.getRawParameterValue (ParamIDs::scale)->load();
        PatternOps::randomNotes (processor.pattern.edit(), len, sc, (uint32_t) juce::Random::getSystemRandom().nextInt());
        processor.pattern.publish();
    };
    auto shift = [this] (int dir)
    {
        PatternOps::shift (processor.pattern.edit(), (int) state.getRawParameterValue (ParamIDs::seqLength)->load(), dir);
        processor.pattern.publish();
    };
    leftButton.onClick  = [shift] { shift (-1); };
    rightButton.onClick = [shift] { shift (1); };

    setSize (designW, designH);
    showTab (oscTab);
}

UndertowContent::~UndertowContent()
{
    setLookAndFeel (nullptr);
}

void UndertowContent::showEuclidMenu()
{
    const int len = (int) state.getRawParameterValue (ParamIDs::seqLength)->load();
    juce::PopupMenu m;
    m.addSectionHeader ("EUCLIDIEN SUR " + juce::String (len) + " PAS");
    for (int f = 1; f < len; ++f)
    {
        juce::PopupMenu rot;
        for (int r = 0; r < juce::jmin (len, 8); ++r)
            rot.addItem (f * 100 + r + 1, r == 0 ? juce::String ("sans decalage") : "decale de " + juce::String (r));
        m.addSubMenu (juce::String (f) + " coups", rot);
    }

    juce::Component::SafePointer<UndertowContent> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&euclidButton), [safe, len] (int res)
    {
        if (safe == nullptr || res <= 0) return;
        PatternOps::euclidean (safe->processor.pattern.edit(), len, res / 100, res % 100 - 1);
        safe->processor.pattern.publish();
    });
}

void UndertowContent::showTab (int t)
{
    currentTab = t;
    for (auto& pl : placed)
        if (pl.page >= 0)
            pl.comp->setVisible (pl.page == t);
    updateLfoVisibility();
    repaint();
}

void UndertowContent::updateLfoVisibility()
{
    const int sel = lfoSelector.getCurrent();
    lfoCurve.lfoIndex = sel;
    for (auto& pl : placed)
        if (pl.lfo >= 0)
            pl.comp->setVisible (currentTab == modTab && pl.lfo == sel);

    if (currentTab == modTab)
        for (int i = 0; i < Params::numLfos; ++i)
        {
            const bool synced = state.getRawParameterValue (ParamIDs::lfoSync (i))->load() > 0.5f;
            lfoRateKnob[(size_t) i]->setVisible (i == sel && ! synced);
            lfoSyncRateBox[(size_t) i]->setVisible (i == sel && synced);
        }
}

bool UndertowContent::tick()
{
    tabs.animate();
    const bool diceMoving = presetBar.refresh();
    const bool screenAlive = fmScreen.tick();
    grid.tick();

    if (currentTab == modTab)
    {
        const int sel = lfoSelector.getCurrent();
        const bool synced = state.getRawParameterValue (ParamIDs::lfoSync (sel))->load() > 0.5f;
        if (lfoRateKnob[(size_t) sel]->isVisible() == synced)
            updateLfoVisibility();
    }

    // courbes d'enveloppe / LFO : redessinées seulement quand un de leurs réglages a bougé
    {
        auto sig = [this] (std::initializer_list<juce::String> ids)
        {
            float v = 0.0f, k = 1.0f;
            for (const auto& pid : ids) { v += state.getRawParameterValue (pid)->load() * k; k *= 1.618f; }
            return v;
        };
        const int sel = lfoSelector.getCurrent();
        const float a = sig ({ ParamIDs::ampAttack, ParamIDs::ampDecay, ParamIDs::ampSustain, ParamIDs::ampRelease });
        const float m = sig ({ ParamIDs::modAttack, ParamIDs::modDecay, ParamIDs::modCurve });
        const float l = sig ({ ParamIDs::lfoShape (sel) }) + (float) sel * 100.0f;
        if (a != curveSig[0]) { curveSig[0] = a; ampCurve.repaint(); }
        if (m != curveSig[1]) { curveSig[1] = m; modCurve.repaint(); }
        if (l != curveSig[2]) { curveSig[2] = l; lfoCurve.repaint(); }
    }

    const bool mouseInside = isMouseOverOrDragging (true);
    mouseActivityFrames = mouseInside ? 90u : (mouseActivityFrames > 0 ? mouseActivityFrames - 1 : 0u);
    const bool playing = processor.transportPlaying.load (std::memory_order_relaxed);
    const bool active = playing || screenAlive || diceMoving || mouseActivityFrames > 0;
    activityReason = juce::String (playing ? "lecture " : "") + (screenAlive ? "ondes " : "") + (diceMoving ? "de " : "")
                   + (mouseActivityFrames > 0 ? "souris" : "");

    // fond : 8 images/s en activité, 4 au repos (les courants sont lents) ; on redessine alors toute la fenêtre
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
        if (playing || screenAlive) fmScreen.repaint();
        if (playing) grid.repaint();
        meter.repaint();
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

void UndertowContent::renderBackground (float scale)
{
    const int w = juce::roundToInt ((float) designW * scale), h = juce::roundToInt ((float) designH * scale);
    if (! bgCache.isValid() || bgCache.getWidth() != w || bgCache.getHeight() != h)
        bgCache = juce::Image (juce::Image::RGB, w, h, false);

    // l'eau (dégradés et courants très doux) est calculée à mi-résolution puis agrandie : aucune différence visible,
    // quatre fois moins de pixels à calculer
    const float lowScale = scale * 0.5f;
    const int lw = juce::roundToInt ((float) designW * lowScale), lh = juce::roundToInt ((float) designH * lowScale);
    if (! bgLow.isValid() || bgLow.getWidth() != lw || bgLow.getHeight() != lh)
        bgLow = juce::Image (juce::Image::RGB, lw, lh, false);
    {
        juce::Graphics lg (bgLow);
        lg.addTransform (juce::AffineTransform::scale (lowScale));
        Ui::drawWaterBackground (lg, getLocalBounds().toFloat());
    }

    juce::Graphics g (bgCache);
    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);   // dégradés très doux : l'agrandissement simple suffit
    g.drawImageTransformed (bgLow, juce::AffineTransform::scale ((float) w / (float) lw, (float) h / (float) lh));
    g.addTransform (juce::AffineTransform::scale (scale));
    Ui::drawGlassPanel (g, pagePanel.toFloat());
    Ui::drawGlassPanel (g, seqPanel.toFloat());

    g.setColour (juce::Colours::white);
    g.setFont (Ui::font (18.0f, true, -0.02f));
    g.drawText ("under", 52, 10, 60, 24, juce::Justification::centredLeft);
    g.setColour (Ui::aqua);
    g.setFont (Ui::font (9.0f, true, 0.45f));
    g.drawText ("TOW", 110, 8, 50, 16, juce::Justification::centredLeft);
    // le label : Undertow est une création GRVTY Records
    g.setColour (Ui::dim);
    g.setFont (Ui::font (6.5f, true, 0.3f));
    g.drawText ("GRVTY RECORDS", 110, 22, 120, 12, juce::Justification::centredLeft);

    // numéro de version : permet de vérifier d'un coup d'œil qu'Ableton charge la dernière compilation
    g.setColour (Ui::dim.withAlpha (0.7f));
    g.setFont (Ui::font (8.5f, false, 0.05f));
    g.drawText (versionText, designW - 104, 13, 66, 20, juce::Justification::centredRight);

    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawVerticalLine (seqLeft.getRight() + 3, (float) seqPanel.getY() + 10.0f, (float) seqPanel.getBottom() - 10.0f);

    bgScale = scale;
    bgDirty = false;
}

void UndertowContent::paint (juce::Graphics& g)
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
        const float xx = 20.0f + (float) i, yy = 22.0f + 3.5f * std::sin ((float) i * 0.42f + t * 2.0f);
        if (i == 0) logo.startNewSubPath (xx, yy); else logo.lineTo (xx, yy);
    }
    g.setColour (Ui::aqua);
    g.strokePath (logo, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void UndertowContent::resized()
{
    bgDirty = true;
    pagePanel = { 396, 44, 494, 216 };
    seqPanel  = { 10, 266, 880, 196 };
    seqLeft   = { 10, 266, 290, 196 };

    presetBar.setBounds (designW / 2 - 200, 9, 400, 26);
    fmScreen.setBounds (10, 44, 378, 216);
    tabs.setBounds (pagePanel.getX() + 8, pagePanel.getY() + 4, pagePanel.getWidth() - 40, 26);
    meter.setBounds (pagePanel.getRight() - 18, pagePanel.getY() + 36, 10, pagePanel.getHeight() - 46);
    grid.setBounds (308, 272, 576, 184);
    infoButton.setBounds (designW - 32, 13, 20, 20);
    if (about != nullptr) about->setBounds (getLocalBounds());
}

void UndertowContent::openAbout()
{
    if (about != nullptr)
        return;
    juce::Component::SafePointer<UndertowContent> safe (this);
    about = std::make_unique<AboutPanel> ("Undertow", JucePlugin_VersionString, true, [safe]
    {
        // fermé depuis un de ses propres clics : on le détruit juste après
        juce::MessageManager::callAsync ([safe] { if (safe != nullptr) safe->about.reset(); });
    });
    addAndMakeVisible (*about);
    about->setBounds (getLocalBounds());
}

//==============================================================================
// Éditeur : met le contenu à l'échelle de la fenêtre
//==============================================================================
UndertowAudioProcessorEditor::UndertowAudioProcessorEditor (UndertowAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p), content (p)
{
    addAndMakeVisible (content);
    setResizable (true, true);
    setResizeLimits (UndertowContent::designW * 7 / 10, UndertowContent::designH * 7 / 10,
                     UndertowContent::designW * 2, UndertowContent::designH * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) UndertowContent::designW / (double) UndertowContent::designH);
    setSize (UndertowContent::designW, UndertowContent::designH);
    startTimerHz (60);
}

UndertowAudioProcessorEditor::~UndertowAudioProcessorEditor()
{
    stopTimer();
}

UndertowAudioProcessorEditor::CacheGuard::CacheGuard()  { Ui::editorOpened(); }
UndertowAudioProcessorEditor::CacheGuard::~CacheGuard() { Ui::editorClosed(); }

void UndertowAudioProcessorEditor::resized()
{
    const float scale = (float) getWidth() / (float) UndertowContent::designW;
    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0, UndertowContent::designW, UndertowContent::designH);
}

void UndertowAudioProcessorEditor::timerCallback()
{
    // le contenu décide lui-même quoi redessiner (voir UndertowContent::tick)
    content.tick();
}
