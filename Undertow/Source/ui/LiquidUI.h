// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

//==============================================================================
/**
    Univers visuel « eau » repris de Flow Filter : couleurs, liquide animé, panneaux de verre,
    boutons-gouttes, pilules, onglets, vumètres. Partagé par toute l'interface d'Undertow.
*/
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

    /** Polices mises en cache : une police n'est créée qu'une fois (copier une juce::Font ne fait pas d'allocation).
        Le cache est vidé quand la dernière fenêtre du plugin se ferme (releaseCaches) : une juce::Font statique
        détruite au déchargement du plugin ferait planter l'hôte sur macOS (cache de polices système déjà libéré). */
    struct FontEntry { float size, kerning; bool bold; juce::Font font; };
    inline std::vector<FontEntry>& fontCache()
    {
        static std::vector<FontEntry> cache;
        return cache;
    }

    inline juce::Font font (float size, bool bold = false, float kerning = 0.0f)
    {
        auto& cache = fontCache();
        for (auto& e : cache)
            if (e.bold == bold && std::abs (e.size - size) < 0.01f && std::abs (e.kerning - kerning) < 0.001f)
                return e.font;
        if (cache.capacity() == 0)
            cache.reserve (64);
        cache.push_back ({ size, kerning, bold, juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain))
                                                    .withExtraKerningFactor (kerning) });
        return cache.back().font;
    }

    inline std::array<juce::Path, 8>& scratchPaths()
    {
        static std::array<juce::Path, 8> paths;
        return paths;
    }

    /** Chemins de travail réutilisés d'une image à l'autre (thread de l'interface uniquement) : pas d'allocation en régime établi. */
    inline juce::Path& scratchPath (int slot)
    {
        auto& p = scratchPaths()[(size_t) slot];
        p.clear();
        return p;
    }

    /** Compte les fenêtres ouvertes (thread de l'interface) ; la dernière qui se ferme libère les caches graphiques. */
    inline int& openEditors() { static int n = 0; return n; }
    inline void editorOpened() { ++openEditors(); }
    inline void editorClosed()
    {
        if (--openEditors() > 0)
            return;
        openEditors() = 0;
        std::vector<FontEntry>().swap (fontCache());
        for (auto& p : scratchPaths())
            juce::Path().swapWithPath (p);
    }

    /** Horloge commune de toutes les animations (secondes). */
    inline float now() { return (float) std::fmod (juce::Time::getMillisecondCounterHiRes() * 0.001, 100000.0); }

    constexpr float minDb = -60.0f, maxDb = 6.0f;

    /** Pseudo-aléatoire stable (0..1) pour placer particules et bulles. */
    inline float hash (float n) { const float s = std::sin (n * 12.9898f) * 43758.5453f; return s - std::floor (s); }

    /** Liquide ondulant qui remplit une zone jusqu'à 'level' (0..1). À appeler dans une zone clippée. */
    inline void drawLiquid (juce::Graphics& g, juce::Rectangle<float> area, float level, float t, float seed,
                     juce::Colour top, juce::Colour bottom, float amp)
    {
        if (level <= 0.001f)
            return;

        const float surfaceY = area.getBottom() - level * area.getHeight();

        for (int layer = 0; layer < 2; ++layer)
        {
            auto& p = scratchPath (0);
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

    inline void drawGlassPanel (juce::Graphics& g, juce::Rectangle<float> r, float corner = 8.0f)
    {
        g.setColour (glassFill());
        g.fillRoundedRectangle (r, corner);
        {
            // reflet en haut du panneau : le dégradé ne couvre que les 40 premiers pixels
            juce::Graphics::ScopedSaveState save (g);
            g.reduceClipRegion (r.withHeight (40.0f).toNearestInt());
            g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.04f), 0.0f, r.getY(),
                                                     juce::Colours::transparentWhite, 0.0f, r.getY() + 40.0f, false));
            g.fillRoundedRectangle (r, corner);
        }
        g.setColour (glassEdge());
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    }
}


namespace Ui
{
    /** Fond animé de Flow Filter : profondeur, lumière de surface qui dérive, courants lents, particules. */
    inline void drawWaterBackground (juce::Graphics& g, juce::Rectangle<float> b)
    {
        const float t = now();

        g.setGradientFill (juce::ColourGradient (bgTop, 0.0f, 0.0f, bgBottom, 0.0f, b.getBottom(), false));
        g.fillAll();

        const float lx = b.getWidth() * (0.5f + 0.35f * std::sin (t * 0.07f));
        g.setGradientFill (juce::ColourGradient (aqua.withAlpha (0.10f), lx, -40.0f,
                                                 aqua.withAlpha (0.0f), lx + 320.0f, 140.0f, true));
        g.fillRect (b);

        for (int layer = 0; layer < 4; ++layer)
        {
            const float fl    = (float) layer;
            const float baseY = b.getHeight() * (0.38f + 0.15f * fl);
            const float amp   = 10.0f + 6.0f * fl;
            const float speed = 0.18f + 0.07f * fl;
            const float freq  = 0.0065f + 0.0022f * fl;

            auto& current = scratchPath (1);
            current.startNewSubPath (0.0f, b.getBottom());
            for (float x = 0.0f; x <= b.getWidth() + 8.0f; x += 8.0f)
            {
                const float y = baseY + amp * std::sin (x * freq + t * speed + fl * 1.9f)
                                      + amp * 0.4f * std::sin (x * freq * 2.7f - t * speed * 1.6f + fl);
                current.lineTo (x, y);
            }
            current.lineTo (b.getWidth() + 8.0f, b.getBottom());
            current.closeSubPath();

            g.setGradientFill (juce::ColourGradient (aqua.withAlpha (0.045f), 0.0f, baseY - amp,
                                                     aquaDeep.withAlpha (0.0f), 0.0f, b.getBottom(), false));
            g.fillPath (current);
        }

        for (int i = 0; i < 40; ++i)
        {
            const float fi   = (float) i;
            const float life = std::fmod (t * (0.012f + 0.02f * hash (fi + 2.0f)) + hash (fi + 9.0f), 1.0f);
            const float px   = b.getWidth() * hash (fi) + 10.0f * std::sin (t * 0.4f + fi);
            const float py   = b.getBottom() - life * (b.getHeight() + 20.0f);
            const float r    = 0.6f + 1.2f * hash (fi + 4.0f);
            g.setColour (juce::Colours::white.withAlpha (0.10f * std::sin (life * juce::MathConstants<float>::pi)));
            g.fillEllipse (px - r, py - r, r * 2.0f, r * 2.0f);
        }
    }
}

//==============================================================================
/** Style "liquide" : boutons-gouttes, menus en pilule, textes compacts. */
class LiquidLook : public juce::LookAndFeel_V4
{
public:
    LiquidLook();

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
};

//==============================================================================
/** Bouton rotatif en forme de goutte : nom au-dessus, valeur en dessous. */
class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& name,
          bool bipolar = false, const juce::String& tooltip = {});
    void resized() override;

private:
    juce::Label  title;
    juce::Slider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

//==============================================================================
/** Interrupteur en pilule avec icône dessinée, relié à un paramètre booléen. */
class PillToggle : public juce::Button
{
public:
    enum class Icon { sync, reverse, freeze, wave, dot };

    PillToggle (juce::AudioProcessorValueTreeState&, const juce::String& paramID, const juce::String& text, Icon);
    void paintButton (juce::Graphics&, bool highlighted, bool down) override;

private:
    Icon icon;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
};

//==============================================================================
/** Onglets avec indicateur lumineux qui glisse vers l'onglet actif. */
class TabBar : public juce::Component
{
public:
    explicit TabBar (juce::StringArray names);
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void animate();

    std::function<void (int)> onChange;
    int  getCurrent() const noexcept { return current; }
    void setCurrent (int i) { current = i; repaint(); if (onChange) onChange (i); }

private:
    juce::StringArray names;
    int current = 0, hovered = -1;
    float indicator = 0.0f;
};

//==============================================================================
/** Vu-mètre en forme de tube rempli de liquide. */
class LiquidMeter : public juce::Component
{
public:
    explicit LiquidMeter (std::atomic<float>& src) : source (src) {}
    void paint (juce::Graphics&) override;

private:
    std::atomic<float>& source;
    float smoothed = 0.0f;
};
