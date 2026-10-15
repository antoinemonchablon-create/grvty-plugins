// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "AboutPanel.h"
#include "BinaryData.h"

namespace
{
    juce::String u8 (const char* s) { return juce::String::fromUTF8 (s); }

    // couleurs de l'univers « eau » (les mêmes que l'interface des plugins)
    const juce::Colour aqua { 0xff4be3d6 };
    const juce::Colour dim  { 0xff6f8f9b };

    juce::Font makeFont (float size, bool bold, float kerning = 0.0f)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain)).withExtraKerningFactor (kerning);
    }
}

//==============================================================================
InfoButton::InfoButton (const juce::String& productName) : juce::Button ("infos")
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    setTooltip (u8 ("\xc3\x80 propos : licence, code source, confidentialit\xc3\xa9"));
    setTitle (u8 ("\xc3\x80 propos de ") + productName);
}

void InfoButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    const auto r = getLocalBounds().toFloat().reduced (1.5f);
    const float d = juce::jmin (r.getWidth(), r.getHeight());
    const auto c = juce::Rectangle<float> (d, d).withCentre (r.getCentre());
    g.setColour (aqua.withAlpha (down ? 0.3f : (highlighted ? 0.18f : 0.06f)));
    g.fillEllipse (c);
    g.setColour (aqua.withAlpha (highlighted ? 0.9f : 0.55f));
    g.drawEllipse (c, 1.1f);
    // le « i » : un point et une barre
    const auto cx = c.getCentreX(), top = c.getY() + d * 0.24f;
    g.fillEllipse (juce::Rectangle<float> (d * 0.13f, d * 0.13f).withCentre ({ cx, top + d * 0.04f }));
    g.fillRoundedRectangle (juce::Rectangle<float> (d * 0.11f, d * 0.36f).withCentre ({ cx, c.getCentreY() + d * 0.12f }), d * 0.05f);
}

//==============================================================================
AboutPanel::AboutPanel (const juce::String& productName, const juce::String& v, bool withAudioUnit, std::function<void()> closeCallback)
    : name (productName), version (v), onClose (std::move (closeCallback))
{
    setWantsKeyboardFocus (true);
    // le panneau est fixe : une seule image, redessinée seulement quand la souris survole un bouton
    setBufferedToImage (true);

    const auto src = About::sourceUrl();
    hasSource = src.isNotEmpty();

    const auto white = juce::Colours::white;
    paragraphs = {
        { u8 ("\xc2\xa9 2026 GRVTY Records \xe2\x80\x94 logiciel libre"), 11.0f, true, white, 18 },
        { u8 ("Une cr\xc3\xa9" "ation du label et collectif GRVTY Records, d\xc3\xa9velopp\xc3\xa9" "e par Jasperr."), 9.5f, true, aqua, 16 },
        { name + u8 (" est un logiciel libre : vous pouvez l'utiliser, l'\xc3\xa9tudier, le modifier et le redistribuer "
                     "selon les termes de la GNU Affero General Public License, version 3 ou (\xc3\xa0 votre choix) "
                     "toute version ult\xc3\xa9rieure, publi\xc3\xa9" "e par la Free Software Foundation "
                     "(termes additionnels : fichier NOTICE)."), 9.5f, false, white.withAlpha (0.85f), 40 },
        { u8 ("Il est fourni SANS AUCUNE GARANTIE, ni de qualit\xc3\xa9 marchande, ni d'ad\xc3\xa9quation \xc3\xa0 un usage "
              "particulier, dans les limites permises par la loi. Le texte complet de la licence est sur gnu.org "
              "(bouton LICENCE) et dans le fichier LICENSE fourni avec le plugin."), 9.5f, false, white.withAlpha (0.85f), 40 },
        { u8 ("La musique que vous cr\xc3\xa9" "ez avec ") + name + u8 (" vous appartient enti\xc3\xa8rement."), 9.5f, true, aqua, 16 },
        { u8 ("Confidentialit\xc3\xa9 : ") + name + u8 (" ne collecte aucune donn\xc3\xa9" "e et ne se connecte jamais \xc3\xa0 Internet. ")
              + (hasSource ? u8 ("Les boutons LICENCE et CODE SOURCE ouvrent une page dans votre navigateur.")
                           : u8 ("Le bouton LICENCE ouvre une page dans votre navigateur.")), 9.5f, false, white.withAlpha (0.85f), 28 },
        { hasSource ? u8 ("Code source : ") + src
                    : u8 ("Code source : publi\xc3\xa9 avec chaque version (archive ") + About::sourceArchiveName (name, version) + ").",
          9.5f, false, white.withAlpha (0.85f), 16 },
        { u8 ("Construit avec JUCE 8 (GNU AGPL v3) et le SDK VST\xc2\xae 3 de Steinberg (MIT) ; autres composants et licences : "
              "fichier THIRD-PARTY-NOTICES. VST est une marque d\xc3\xa9pos\xc3\xa9" "e de Steinberg Media Technologies GmbH. ")
              + (withAudioUnit ? u8 ("Audio Units, Mac et macOS sont des marques d'Apple Inc. ") : juce::String())
              + u8 ("Ableton et Live sont des marques d'Ableton AG ; ") + name
              + u8 (" n'est ni affili\xc3\xa9 \xc3\xa0 Ableton, ni approuv\xc3\xa9 par Ableton."),
          8.0f, false, dim, 34 },
    };

    // logo « VST Compatible » officiel de Steinberg (règles d'usage du SDK VST 3 : à afficher dans la boîte À propos)
    vstLogo = juce::ImageCache::getFromMemory (BinaryData::VST_Compatible_Logo_Steinberg_with_TM_png,
                                               BinaryData::VST_Compatible_Logo_Steinberg_with_TM_pngSize);
}

juce::String AboutPanel::getAllText() const
{
    juce::String s = name.toUpperCase() + " v" + version + "\n";
    for (const auto& p : paragraphs) s << p.text << "\n";
    s << "LICENCE " << About::licenceUrl() << "\n";
    return s;
}

void AboutPanel::parentHierarchyChanged()
{
    if (isShowing())
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<AboutPanel> (this)]
        {
            if (safe != nullptr && safe->isShowing()) safe->grabKeyboardFocus();
        });
}

void AboutPanel::resized()
{
    // Mise en page : texte à gauche, logo VST à droite, boutons en bas. Si le panneau ne tient pas en hauteur
    // (fenêtre basse, ex. Flow Filter : 292 px), le texte passe sur deux colonnes dans un panneau plus large.
    constexpr int fixedH = 14 + 22 + 10 + 10 + 26 + 18;   // marge, titre, espace, espace, boutons, marge
    const int avail = getHeight() - 12;
    const int n = (int) paragraphs.size();

    auto layoutAll = [this] (int width)
    {
        layouts.clear();
        for (auto& p : paragraphs)
        {
            juce::AttributedString a;
            a.setJustification (juce::Justification::topLeft);
            a.setWordWrap (juce::AttributedString::byWord);
            a.append (p.text, makeFont (p.size, p.bold), p.colour);
            juce::TextLayout l;
            l.createLayout (a, (float) width);
            p.height = (int) std::ceil (l.getHeight()) + 1;
            layouts.push_back (std::move (l));
        }
    };
    auto sumHeights = [this] (int from, int to, int g)
    {
        int h = 0;
        for (int i = from; i < to; ++i) h += paragraphs[(size_t) i].height + g;
        return h;
    };

    int panelW = juce::jmin (640, getWidth() - 24);
    int textW = panelW - 44 - logoW - 18;
    layoutAll (textW);
    gap = 8;
    if (fixedH + sumHeights (0, n, gap) > avail)
        gap = 4;
    int split = n, textH = sumHeights (0, n, gap);

    if (fixedH + textH > avail)
    {
        // deux colonnes : coupure qui équilibre au mieux leurs hauteurs
        panelW = juce::jmin (880, getWidth() - 24);
        textW = (panelW - 44 - logoW - 18 - 18) / 2;
        layoutAll (textW);
        gap = 6;
        textH = 1 << 30;
        for (int k = 1; k < n; ++k)
        {
            const int h = juce::jmax (sumHeights (0, k, gap), sumHeights (k, n, gap));
            if (h < textH) { textH = h; split = k; }
        }
    }

    const int h = fixedH + juce::jmax (textH, logoH);
    panelR = juce::Rectangle<int> (panelW, h).withCentre (getLocalBounds().getCentre());
    auto inner = panelR.reduced (22, 0).withTrimmedTop (14 + 22 + 10).withTrimmedBottom (18);
    auto buttons = inner.removeFromBottom (26);
    inner.removeFromBottom (10);
    logoR = inner.removeFromRight (logoW).withHeight (logoH);
    inner.removeFromRight (18);

    // rectangle de chaque paragraphe
    paraR.clear();
    auto col = split < n ? inner.removeFromLeft (textW) : inner;
    for (int i = 0; i < n; ++i)
    {
        if (i == split)
        {
            inner.removeFromLeft (18);
            col = inner;
        }
        paraR.push_back (col.removeFromTop (paragraphs[(size_t) i].height));
        col.removeFromTop (gap);
    }

    closeR = buttons.removeFromRight (100);
    buttons.removeFromRight (10);
    licenceR = buttons.removeFromLeft (100);
    buttons.removeFromLeft (10);
    sourceR = hasSource ? buttons.removeFromLeft (120) : juce::Rectangle<int>();
}

void AboutPanel::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xc0010508));

    const auto p = panelR.toFloat();
    g.setColour (juce::Colour (0xf40a1d26));
    g.fillRoundedRectangle (p, 10.0f);
    g.setColour (aqua.withAlpha (0.45f));
    g.drawRoundedRectangle (p.reduced (0.5f), 10.0f, 1.2f);

    const auto titleR = panelR.reduced (22, 0).withTrimmedTop (14).withHeight (22);
    g.setColour (aqua);
    g.setFont (makeFont (11.0f, true, 0.25f));
    g.drawText (name.toUpperCase(), titleR, juce::Justification::centredLeft);
    g.setColour (dim);
    g.setFont (makeFont (9.0f, false, 0.05f));
    g.drawText ("v" + version, titleR, juce::Justification::centredRight);

    for (size_t i = 0; i < layouts.size() && i < paraR.size(); ++i)
        layouts[i].draw (g, paraR[i].toFloat());

    if (vstLogo.isValid())
    {
        g.setOpacity (1.0f);
        g.drawImage (vstLogo, logoR.toFloat(), juce::RectanglePlacement::centred);
    }

    auto button = [&g] (juce::Rectangle<int> b, const juce::String& text, bool primary, bool isHot)
    {
        if (b.isEmpty()) return;
        const auto f = b.toFloat().reduced (0.5f);
        g.setColour (primary ? aqua.withAlpha (isHot ? 0.45f : 0.3f) : juce::Colours::white.withAlpha (isHot ? 0.1f : 0.05f));
        g.fillRoundedRectangle (f, f.getHeight() * 0.5f);
        g.setColour (primary ? aqua : juce::Colours::white.withAlpha (0.25f));
        g.drawRoundedRectangle (f, f.getHeight() * 0.5f, 1.0f);
        g.setColour (juce::Colours::white);
        g.setFont (makeFont (9.5f, true, 0.15f));
        g.drawText (text, b, juce::Justification::centred);
    };
    button (licenceR, "LICENCE", false, hot == 1);
    button (sourceR, "CODE SOURCE", false, hot == 2);
    button (closeR, "FERMER", true, hot == 3);
}

void AboutPanel::mouseMove (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    const int h = licenceR.contains (pt) ? 1 : sourceR.contains (pt) ? 2 : closeR.contains (pt) ? 3 : 0;
    if (h != hot) { hot = h; repaint(); }
}

void AboutPanel::mouseExit (const juce::MouseEvent&)
{
    if (hot != 0) { hot = 0; repaint(); }
}

void AboutPanel::mouseDown (const juce::MouseEvent& e)
{
    const auto pt = e.getPosition();
    if (licenceR.contains (pt))                     juce::URL (About::licenceUrl()).launchInDefaultBrowser();
    else if (hasSource && sourceR.contains (pt))    juce::URL (About::sourceUrl()).launchInDefaultBrowser();
    else if (closeR.contains (pt) || ! panelR.contains (pt)) close();
}

bool AboutPanel::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey || k == juce::KeyPress::returnKey) { close(); return true; }
    return false;
}

void AboutPanel::close()
{
    if (closed) return;
    closed = true;
    setVisible (false);
    if (onClose) onClose();
}
