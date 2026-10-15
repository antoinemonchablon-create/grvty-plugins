// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

// Banc de tests de la phase 2 (inclus dans EbbTests.cpp, dans son espace de noms anonyme) :
// courbe perso, Tension, Lissage, multibande, état, édition à la souris.

//==============================================================================
/** Niveau d'une courbe perso, réécrit ici indépendamment (recherche du segment par balayage, courbure en double). */
double refLevel (const Ebb::ShapeData& s, double x)
{
    if (x <= (double) s.pts[0].x) return s.pts[0].y;
    int seg = -1;
    for (int i = 0; i + 1 < s.count; ++i)
        if (x >= (double) s.pts[(size_t) i].x && x < (double) s.pts[(size_t) i + 1].x) seg = i;
    if (seg < 0) return s.pts[(size_t) s.count - 1].y;
    const auto& a = s.pts[(size_t) seg];
    const auto& b = s.pts[(size_t) seg + 1];
    const double t = (x - a.x) / (double) (b.x - a.x);
    double bt = t;
    if (std::abs (a.curve) >= 1.0e-4)
    {
        const double k = std::pow (2.0, -4.0 * (double) a.curve);
        bt = t / (t + (1.0 - t) * k);
    }
    return a.y + (b.y - a.y) * bt;
}

Ebb::ShapeData randomShape (std::mt19937& rng, bool withSteps)
{
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);
    Ebb::ShapeData s;
    const int n = 2 + (int) (uni (rng) * 14.0f);
    std::vector<float> xs { 0.0f, 1.0f };
    for (int i = 0; i < n - 2; ++i) xs.push_back (uni (rng));
    if (withSteps && n > 3) xs.push_back (xs[2]);     // une marche (deux points au même x)
    std::sort (xs.begin(), xs.end());
    for (float x : xs)
        if (s.count < Ebb::ShapeData::maxPoints)
            s.pts[(size_t) s.count++] = { x, uni (rng), uni (rng) * 2.0f - 1.0f };
    Ebb::ShapeMath::sanitize (s);
    return s;
}

void testShapes()
{
    section (u8 ("Courbe perso : points -> table, conversion, texte"));
    std::mt19937 rng (21);
    const int n = Ebb::CurveTables::size;
    std::vector<float> table ((size_t) n + 1);
    double worst = 0.0;
    for (int trial = 0; trial < 300; ++trial)
    {
        const auto s = randomShape (rng, trial % 3 == 0);
        Ebb::ShapeMath::render (s, table.data(), n);
        for (int i = 0; i <= n; ++i)
        {
            const double x = (double) i / n;
            const double ref = i == n ? s.pts[(size_t) s.count - 1].y : refLevel (s, x);
            worst = std::max (worst, std::abs ((1.0 - table[(size_t) i]) - ref));
        }
    }
    check (worst < 2.0e-5, u8 ("300 courbes au hasard (marches et courbures comprises) contre r\xc3\xa9" "f\xc3\xa9rence"), u8 ("\xc3\xa9" "cart max ") + fmt (worst, 7));

    // conversion d'une courbe d'usine en points (pour la retoucher)
    double worstConv = 0.0;
    int maxPts = 0;
    for (int c = 0; c < Ebb::numCurves; ++c)
    {
        const float* fac = Ebb::CurveTables::get().table (c);
        const auto s = Ebb::ShapeMath::fromTable (fac, n);
        maxPts = std::max (maxPts, s.count);
        Ebb::ShapeMath::render (s, table.data(), n);
        for (int i = 0; i <= n; ++i)
        {
            // les marches (gate, rampe) : on ne compare pas à moins de 2/1024 de la marche
            const int i0 = std::max (0, i - 16), i1 = std::min (n, i + 16);
            if (std::abs (fac[i1] - fac[i0]) > 0.2f) continue;
            worstConv = std::max (worstConv, (double) std::abs (table[(size_t) i] - fac[i]));
        }
    }
    check (worstConv < 0.012 && maxPts <= Ebb::ShapeData::maxPoints, u8 ("12 courbes d'usine converties en points"),
           u8 ("\xc3\xa9" "cart max ") + fmt (worstConv, 4) + ", " + juce::String (maxPts) + " points au plus");

    int textBad = 0;
    for (int trial = 0; trial < 100; ++trial)
    {
        const auto s = randomShape (rng, true);
        Ebb::ShapeData back;
        if (! ShapeLibrary::fromText (ShapeLibrary::toText (s), back) || back.count != s.count) { ++textBad; continue; }
        for (int i = 0; i < s.count; ++i)
            if (std::abs (back.pts[(size_t) i].x - s.pts[(size_t) i].x) > 1e-6f || std::abs (back.pts[(size_t) i].y - s.pts[(size_t) i].y) > 1e-6f
                || std::abs (back.pts[(size_t) i].curve - s.pts[(size_t) i].curve) > 1e-6f) { ++textBad; break; }
    }
    check (textBad == 0, u8 ("Texte (\xc3\xa9tat du plugin, fichiers) aller-retour"), "100 courbes, " + juce::String (textBad) + u8 (" diff\xc3\xa9rence(s)"));
}

//==============================================================================
/** Rendu réel (SYNC, 1 mesure à 60 BPM) contre : table de référence -> lissage -> tension -> Amount -> anti-clic, réécrits ici. */
double renderAgainstReference (Rig& rig, const std::function<double (double)>& refR, float amount)
{
    const double sr = rig.sr;
    const int cycle = (int) (4.0 * sr);
    std::vector<float> out ((size_t) cycle * 2);
    for (int done = 0; done < cycle * 2; done += 512)
        rig.run (std::min (512, cycle * 2 - done), dc, nullptr, {}, out.data() + done);
    const double step = 1.0 / (0.001 * sr);
    double g = out[(size_t) cycle - 1];
    double worst = 0.0;
    for (int i = 0; i < cycle; ++i)
    {
        const double target = 1.0 - amount * refR ((double) i / (double) cycle);
        g += std::max (-step, std::min (step, target - g));
        worst = std::max (worst, std::abs (g - (double) out[(size_t) (cycle + i)]));
    }
    return worst;
}

/** Lissage de référence : 3 passes de moyenne glissante circulaire, calculées naïvement (somme complète à chaque point). */
std::vector<double> refSmooth (const float* in, int n, int h)
{
    std::vector<double> a ((size_t) n), b ((size_t) n);
    for (int i = 0; i < n; ++i) a[(size_t) i] = in[i];
    for (int pass = 0; pass < 3; ++pass)
    {
        for (int i = 0; i < n; ++i)
        {
            double sum = 0.0;
            for (int k = -h; k <= h; ++k) sum += a[(size_t) (((i + k) % n + n) % n)];
            b[(size_t) i] = sum / (2 * h + 1);
        }
        std::swap (a, b);
    }
    return a;
}

void testCustomTensionSmooth()
{
    section (u8 ("Courbe perso, Tension et Lissage dans le son"));
    const int n = Ebb::CurveTables::size;

    // --- courbe perso
    {
        std::mt19937 rng (5);
        double worst = 0.0;
        for (int trial = 0; trial < 4; ++trial)
        {
            Rig rig (48000.0, 512);
            rig.set (id::rate, (float) Ebb::rateBar);
            rig.set (id::amount, 100.0f);
            rig.ph.bpm = 60.0;
            rig.ph.playing = true;
            // la table elle-même est vérifiée point par point dans « shapes » ; ici : le moteur joue bien la courbe perso
            // reçue de l'interface (échange sans verrou), lue comme sa table, avec l'anti-clic réécrit ici
            const auto shape = randomShape (rng, trial % 2 == 0);
            rig.p.setShape (shape, false, true);
            std::vector<float> tab ((size_t) n + 1);
            Ebb::ShapeMath::render (shape, tab.data(), n);
            worst = std::max (worst, renderAgainstReference (rig, [&] (double x)
            {
                const double pos = x * n;
                const int i = std::min (n - 1, (int) pos);
                return tab[(size_t) i] + (pos - i) * (tab[(size_t) i + 1] - tab[(size_t) i]);
            }, 1.0f));
        }
        check (worst < 2.0e-3, u8 ("Courbe perso jou\xc3\xa9" "e (4 courbes au hasard)"), u8 ("\xc3\xa9" "cart max ") + fmt (worst, 5));
    }

    // --- Tension
    for (float tension : { -100.0f, -50.0f, 50.0f, 100.0f })
    {
        Rig rig (48000.0, 512);
        rig.set (id::rate, (float) Ebb::rateBar);
        rig.set (id::amount, 100.0f);
        rig.set (id::curve, (float) Ebb::curveCurrent);
        rig.set (id::tension, tension);
        rig.ph.bpm = 60.0;
        rig.ph.playing = true;
        const double k = std::pow (2.0, 3.0 * tension / 100.0);
        const double worst = renderAgainstReference (rig, [&] (double x)
        {
            const double r = refCurve (Ebb::curveCurrent, x);
            return r / (r + (1.0 - r) * k);
        }, 1.0f);
        check (worst < 2.0e-3, juce::String ("Tension ") + (tension > 0 ? "+" : "") + juce::String ((int) tension) + " %",
               u8 ("\xc3\xa9" "cart max ") + fmt (worst, 5));
    }

    // tension 0 : rien n'est calculé en plus, le son est identique bit à bit à celui de la phase 1
    {
        Rig a (48000.0, 512), b (48000.0, 512);
        for (auto* rg : { &a, &b }) { rg->ph.playing = true; rg->set (id::curve, (float) Ebb::curveSwell); }
        b.set (id::tension, 30.0f);
        b.set (id::tension, 0.0f);
        std::vector<float> oa (512), ob (512);
        long diff = 0;
        for (int blk = 0; blk < 400; ++blk)
        {
            a.run (512, [] (int, int64_t i) { return std::sin ((float) i * 0.01f); }, nullptr, {}, oa.data());
            b.run (512, [] (int, int64_t i) { return std::sin ((float) i * 0.01f); }, nullptr, {}, ob.data());
            if (blk > 10) for (int i = 0; i < 512; ++i) if (std::memcmp (&oa[(size_t) i], &ob[(size_t) i], 4) != 0) ++diff;
        }
        check (diff == 0, u8 ("Tension remise \xc3\xa0 0 : son identique bit \xc3\xa0 bit"), juce::String (diff) + u8 (" \xc3\xa9" "chantillon(s) diff\xc3\xa9rent(s)"));
    }

    // --- Lissage : table du moteur contre lissage naïf, et rendu
    for (float smooth : { 10.0f, 40.0f, 100.0f })
    {
        Rig rig (48000.0, 512);
        rig.set (id::rate, (float) Ebb::rateBar);
        rig.set (id::amount, 100.0f);
        rig.set (id::curve, (float) Ebb::curveGate);
        rig.set (id::smooth, smooth);
        rig.ph.bpm = 60.0;
        rig.ph.playing = true;
        const int h = (int) std::lround (std::pow (smooth / 100.0, 1.5) * 0.08 * n);
        const auto ref = refSmooth (Ebb::CurveTables::get().table (Ebb::curveGate), n, h);
        const double worst = renderAgainstReference (rig, [&] (double x)
        {
            const double pos = x * n;
            const int i = std::min (n - 1, (int) pos);
            const double f = pos - i;
            return ref[(size_t) i] + f * (ref[(size_t) ((i + 1) % n)] - ref[(size_t) i]);
        }, 1.0f);
        const float* eng = rig.p.getEngine().getActiveTable();
        double worstTab = 0.0;
        for (int i = 0; i < n; ++i) worstTab = std::max (worstTab, std::abs ((double) eng[i] - ref[(size_t) i]));
        check (worstTab < 1.0e-4 && worst < 2.0e-3, juce::String ("Lissage ") + juce::String ((int) smooth) + " % (Gate)",
               u8 ("table : \xc3\xa9" "cart ") + fmt (worstTab, 6) + u8 (" ; son : \xc3\xa9" "cart ") + fmt (worst, 5) + " (largeur " + juce::String (h) + " points)");
    }
}

//==============================================================================
/** Niveau RMS d'un signal sur une fenêtre. */
double rms (const std::vector<float>& v, size_t from, size_t to)
{
    double s = 0.0;
    for (size_t i = from; i < to; ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / (double) std::max<size_t> (1, to - from));
}

void testBands()
{
    section (u8 ("Multibande : graves ou aigus seulement"));
    const double sr = 48000.0;

    // --- rien ne pompe (Amount 0) : amplitude plate à toutes les fréquences, en graves comme en aigus
    {
        double worstDb = 0.0;
        for (int band : { 1, 2 })
            for (double f : { 30.0, 60.0, 150.0, 300.0, 1000.0, 5000.0, 15000.0 })
            {
                Rig rig (sr, 512, 1, 0);
                rig.set (id::amount, 0.0f);
                rig.set (id::band, (float) band);
                rig.ph.playing = true;
                std::vector<float> out, in;
                for (int b = 0; b < 200; ++b)
                    rig.run (512, [&] (int, int64_t i) { const float v = (float) std::sin (2.0 * 3.14159265358979 * f * (double) i / sr); in.push_back (v); return v; }, nullptr, {}, nullptr, &out);
                const double db = 20.0 * std::log10 (rms (out, 48000, out.size()) / rms (in, 48000, in.size()));
                worstDb = std::max (worstDb, std::abs (db));
            }
        check (worstDb < 0.05, u8 ("Amount 0 % en GRAVES / AIGUS : niveau inchang\xc3\xa9 de 30 Hz \xc3\xa0 15 kHz"), u8 ("\xc3\xa9" "cart max ") + fmt (worstDb, 4) + " dB");
    }

    // --- pendant la baisse (Gate, Amount 100 %, séparation à 150 Hz) : seule la bande choisie disparaît
    struct Case { int band; double f; bool shouldDuck; };
    const Case cases[] = { { 1, 40.0, true }, { 1, 2000.0, false }, { 2, 40.0, false }, { 2, 2000.0, true }, { 0, 40.0, true }, { 0, 2000.0, true } };
    for (auto& cs : cases)
    {
        Rig rig (sr, 512, 2, 0);
        rig.set (id::amount, 100.0f);
        rig.set (id::curve, (float) Ebb::curveGate);
        rig.set (id::rate, (float) Ebb::rateBar);
        rig.set (id::band, (float) cs.band);
        rig.set (id::crossover, 150.0f);
        rig.ph.bpm = 60.0;
        rig.ph.playing = true;
        std::vector<float> out, in;
        const int blocks = (int) (8.0 * sr / 512);
        for (int b = 0; b < blocks; ++b)
            rig.run (512, [&] (int c, int64_t i) { const float v = 0.5f * (float) std::sin (2.0 * 3.14159265358979 * cs.f * (double) i / sr); if (c == 0) in.push_back (v); return v; },
                     nullptr, {}, nullptr, nullptr);
        // on refait un tour pour récupérer le canal 0 dans l'ordre
        juce::ignoreUnused (out);
        // fenêtre : 2e mesure (4 s à 60 BPM), de 0,3 s à 1,5 s (la porte est fermée de 0 à 1,8 s)
        std::vector<float> ch0;
        {
            Rig r2 (sr, 512, 2, 0);
            r2.set (id::amount, 100.0f); r2.set (id::curve, (float) Ebb::curveGate); r2.set (id::rate, (float) Ebb::rateBar);
            r2.set (id::band, (float) cs.band); r2.set (id::crossover, 150.0f); r2.ph.bpm = 60.0; r2.ph.playing = true;
            std::vector<float> o (512);
            for (int b = 0; b < blocks; ++b)
            {
                r2.run (512, [&] (int, int64_t i) { return 0.5f * (float) std::sin (2.0 * 3.14159265358979 * cs.f * (double) i / sr); }, nullptr, {}, o.data());
                ch0.insert (ch0.end(), o.begin(), o.end());
            }
        }
        const size_t a0 = (size_t) (4.3 * sr), a1 = (size_t) (5.5 * sr);
        const double db = 20.0 * std::log10 (std::max (1e-9, rms (ch0, a0, a1)) / rms (in, a0, a1));
        const bool ok = cs.shouldDuck ? db < -30.0 : std::abs (db) < 0.5;
        check (ok, juce::String (cs.band == 0 ? "PLEIN" : cs.band == 1 ? "GRAVES" : "AIGUS") + " : sinus " + juce::String ((int) cs.f) + u8 (" Hz pendant la baisse"),
               fmt (db, 2) + " dB (" + (cs.shouldDuck ? u8 ("doit dispara\xc3\xae" "tre : < -30 dB") : u8 ("doit rester : \xc2\xb1" "0,5 dB")) + ")");
    }

    // --- changements de bande et de fréquence pendant la lecture : aucun clic
    {
        Rig rig (sr, 256, 2, 0);
        rig.set (id::amount, 70.0f);
        rig.ph.playing = true;
        std::mt19937 rng (8);
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);
        std::vector<float> o (256), ch0;
        auto sig = [&] (int, int64_t i) { const double t = (double) i / sr; return (float) (0.4 * std::sin (2.0 * 3.14159265358979 * 60.0 * t) + 0.3 * std::sin (2.0 * 3.14159265358979 * 900.0 * t)); };
        for (int b = 0; b < (int) (6.0 * sr / 256); ++b)
        {
            if (b % 19 == 0) rig.set (id::band, (float) (int) (uni (rng) * 3.0f));
            if (b % 7 == 0) rig.set (id::crossover, 40.0f + uni (rng) * 1500.0f);
            rig.run (256, sig, nullptr, {}, o.data());
            ch0.insert (ch0.end(), o.begin(), o.end());
        }
        // pente maximale du signal d'entrée (somme des deux sinus) : 2 pi f A / sr
        const double maxSlopeIn = 2.0 * 3.14159265358979 * (60.0 * 0.4 + 900.0 * 0.3) / sr;
        double worst = 0.0;
        for (size_t i = 1; i < ch0.size(); ++i) worst = std::max (worst, (double) std::abs (ch0[i] - ch0[i - 1]));
        check (worst < maxSlopeIn * 1.5, u8 ("Bande et fr\xc3\xa9quence chang\xc3\xa9" "es pendant la lecture (6 s)"),
               u8 ("saut max ") + fmt (worst, 4) + u8 (" par \xc3\xa9" "chantillon (pente max du signal : ") + fmt (maxSlopeIn, 4) + ")");
    }
}

//==============================================================================
void testStateV2()
{
    section (u8 ("\xc3\x89" "tat phase 2 : courbe perso, grille, compatibilit\xc3\xa9 phase 1"));
    std::mt19937 rng (13);
    int bad = 0;
    for (int trial = 0; trial < 30; ++trial)
    {
        EbbAudioProcessor a, b;
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);
        for (auto* prm : a.getParameters()) prm->setValueNotifyingHost (uni (rng));
        a.setShape (randomShape (rng, true), false, false);
        a.setGrid (16);
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        // valeur « légale » (un interrupteur réglé à 0,97 vaut 1)
        auto legal = [] (juce::AudioProcessorParameter* q)
        {
            auto* rq = dynamic_cast<juce::RangedAudioParameter*> (q);
            return rq != nullptr ? rq->convertTo0to1 (rq->convertFrom0to1 (q->getValue())) : q->getValue();
        };
        for (int i = 0; i < a.getParameters().size(); ++i)
            if (std::abs (legal (a.getParameters()[i]) - legal (b.getParameters()[i])) > 1.0e-6f)
            {
                ++bad;
                if (std::getenv ("EB_DEBUG_STATE") != nullptr)
                    std::printf ("    param %s : %f / %f\n", a.getParameters()[i]->getName (40).toRawUTF8(), a.getParameters()[i]->getValue(), b.getParameters()[i]->getValue());
            }
        const auto& sa = a.getShape();
        const auto& sb = b.getShape();
        if (sa.count != sb.count) ++bad;
        else for (int i = 0; i < sa.count; ++i)
            if (std::abs (sa.pts[(size_t) i].x - sb.pts[(size_t) i].x) > 1e-6f || std::abs (sa.pts[(size_t) i].y - sb.pts[(size_t) i].y) > 1e-6f) ++bad;
        if (b.getGrid() != 16) ++bad;
        for (int i = 0; i <= Ebb::CurveTables::size; i += 97)
            if (std::abs (a.getShapeTable()[i] - b.getShapeTable()[i]) > 1e-6f) ++bad;
    }
    check (bad == 0, u8 ("30 \xc3\xa9tats au hasard : 11 param\xc3\xa8tres, points, table, grille"), juce::String (bad) + u8 (" diff\xc3\xa9rence(s)"));

    // un projet enregistré avec la phase 1 (6 paramètres, pas de courbe perso)
    const char* phase1 = "<EBB><PARAM id=\"amount\" value=\"55.0\"/><PARAM id=\"curve\" value=\"6.0\"/><PARAM id=\"rate\" value=\"1.0\"/>"
                         "<PARAM id=\"trigger\" value=\"0.0\"/><PARAM id=\"nudge\" value=\"3.0\"/><PARAM id=\"threshold\" value=\"-25.0\"/></EBB>";
    EbbAudioProcessor p;
    juce::MemoryBlock mb;
    juce::AudioProcessor::copyXmlToBinary (*juce::parseXML (phase1), mb);
    p.setStateInformation (mb.getData(), (int) mb.getSize());
    const auto s = p.readSettings();
    const bool ok = std::abs (s.amount - 0.55f) < 1e-4f && s.curve == 6 && s.rate == 1 && std::abs (s.nudgeMs - 3.0f) < 1e-4f
                    && ! s.custom && std::abs (s.tension) < 1e-6f && std::abs (s.smooth) < 1e-6f && p.getBand() == 0
                    && std::abs (p.getCrossover() - 150.0f) < 0.01f && p.getShape().count >= 2;
    check (ok, u8 ("Projet de la phase 1 : r\xc3\xa9glages retrouv\xc3\xa9s, nouveaux r\xc3\xa9glages \xc3\xa0 leur valeur par d\xc3\xa9" "faut"),
           u8 ("Amount ") + fmt (s.amount * 100.0f, 1) + " %, courbe " + juce::String (s.curve) + u8 (", bande ") + juce::String (p.getBand()));
}

//==============================================================================
/** Événement souris pour piloter l'éditeur dans le banc de tests. */
juce::MouseEvent mouseAt (juce::Component& c, juce::Point<float> pos, juce::ModifierKeys mods = {}, int clicks = 1, juce::Point<float> down = {})
{
    auto source = juce::Desktop::getInstance().getMainMouseSource();
    const auto now = juce::Time::getCurrentTime();
    return juce::MouseEvent (source, pos, mods, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, &c, &c, now, down.isOrigin() ? pos : down, now, clicks, false);
}

void testEditor (const juce::File& dir)
{
    section (u8 ("\xc3\x89" "diteur de courbe (souris, outils, mes courbes)"));
    EbbAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    std::unique_ptr<EbbAudioProcessorEditor> ed (dynamic_cast<EbbAudioProcessorEditor*> (p.createEditor()));
    auto& content = ed->getContent();
    auto& cv = content.getCurveView();
    for (int i = 0; i < 3; ++i) ed->tickForTests();

    // ÉDITER depuis une courbe d'usine : on part de ce qu'on entend
    p.setParam (ParamIDs::curve, (float) Ebb::curveRipple);
    content.getEditButton().onClick();
    ed->tickForTests();
    const bool startOk = cv.isEditMode() && p.readSettings().custom && p.getShape().count >= 3;
    check (startOk, u8 ("\xc3\x89" "DITER : la courbe Ripple devient le point de d\xc3\xa9part"), juce::String (p.getShape().count) + " points");

    // ajouter un point (grille 8 : aimanté sur 1/8 en x et 1/16 en niveau), le glisser, relâcher
    // (sur la pompe simple de départ : 3 points, la place de glisser jusqu'à 0,375)
    p.setShape (Ebb::ShapeMath::defaultShape(), true, true);
    p.setGrid (8);
    const int before = p.getShape().count;
    const auto undoBefore = p.canUndo();
    juce::ignoreUnused (undoBefore);
    const auto clickPos = cv.shapeToScreen (0.27f, 0.52f);
    cv.mouseDown (mouseAt (cv, clickPos));
    bool added = p.getShape().count == before + 1;
    int idx = -1;
    for (int i = 0; i < p.getShape().count; ++i)
        if (std::abs (p.getShape().pts[(size_t) i].x - 0.25f) < 1e-6f && std::abs (p.getShape().pts[(size_t) i].y - 0.5f) < 1e-6f) idx = i;
    cv.mouseDrag (mouseAt (cv, cv.shapeToScreen (0.40f, 0.77f), {}, 1, clickPos));
    const bool moved = idx >= 0 && std::abs (p.getShape().pts[(size_t) idx].x - 0.375f) < 1e-6f && std::abs (p.getShape().pts[(size_t) idx].y - 0.75f) < 1e-6f;
    cv.mouseUp (mouseAt (cv, cv.shapeToScreen (0.40f, 0.77f)));
    check (added && moved, u8 ("Clic : point ajout\xc3\xa9 et aimant\xc3\xa9 (0,25 ; 50 %), gliss\xc3\xa9 (0,375 ; 75 %)"),
           idx >= 0 ? "x " + fmt (p.getShape().pts[(size_t) idx].x, 4) + ", niveau " + fmt (p.getShape().pts[(size_t) idx].y, 4) : juce::String ("point introuvable"));

    // Alt : sans aimant
    {
        const auto from = cv.shapeToScreen (p.getShape().pts[(size_t) idx].x, p.getShape().pts[(size_t) idx].y);
        cv.mouseDown (mouseAt (cv, from, juce::ModifierKeys::altModifier));
        cv.mouseDrag (mouseAt (cv, cv.shapeToScreen (0.333f, 0.71f), juce::ModifierKeys::altModifier, 1, from));
        cv.mouseUp (mouseAt (cv, cv.shapeToScreen (0.333f, 0.71f)));
        const auto& pt = p.getShape().pts[(size_t) idx];
        check (std::abs (pt.x - 0.333f) < 0.003f && std::abs (pt.y - 0.71f) < 0.005f, u8 ("Alt : d\xc3\xa9placement libre (sans aimant)"),
               "x " + fmt (pt.x, 4) + ", niveau " + fmt (pt.y, 4));
    }

    // annuler / rétablir : un geste = une entrée
    {
        const auto after = p.getShape();
        p.undoShape();                     // annule le déplacement libre
        p.undoShape();                     // annule l'ajout + le glisser (un seul geste)
        const bool backToStart = p.getShape().count == before;
        p.redoShape();
        p.redoShape();
        check (backToStart && p.getShape() == after, u8 ("Annuler / r\xc3\xa9tablir (un geste = une \xc3\xa9tape)"),
               juce::String (before) + u8 (" points apr\xc3\xa8s 2 annulations, ") + juce::String (p.getShape().count) + u8 (" apr\xc3\xa8s 2 r\xc3\xa9tablissements"));
    }

    // poignée de courbure : on la monte, le segment se bombe vers le haut
    {
        auto s = p.getShape();
        int seg = -1;
        for (int i = 0; i + 1 < s.count; ++i)
            if (s.pts[(size_t) i + 1].y - s.pts[(size_t) i].y > 0.2f && s.pts[(size_t) i + 1].x - s.pts[(size_t) i].x > 0.05f) { seg = i; break; }
        if (seg >= 0)
        {
            s.pts[(size_t) seg].curve = 0.0f;
            p.setShape (s, true, true);
            const auto& a = p.getShape().pts[(size_t) seg];
            const auto& b = p.getShape().pts[(size_t) seg + 1];
            const auto h = cv.shapeToScreen ((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f);
            cv.mouseDown (mouseAt (cv, h));
            cv.mouseDrag (mouseAt (cv, h.translated (0.0f, -45.0f), {}, 1, h));
            cv.mouseUp (mouseAt (cv, h.translated (0.0f, -45.0f)));
            const float c = p.getShape().pts[(size_t) seg].curve;
            const float mid = (float) refLevel (p.getShape(), (a.x + b.x) * 0.5);
            check (c > 0.4f && mid > (a.y + b.y) * 0.5f + 0.05f, u8 ("Poign\xc3\xa9" "e de courbure tir\xc3\xa9" "e vers le haut : segment bomb\xc3\xa9"),
                   "courbure " + fmt (c, 2) + ", niveau au milieu " + fmt (mid, 3));
        }
        else
            check (false, u8 ("Poign\xc3\xa9" "e de courbure"), u8 ("aucun segment montant trouv\xc3\xa9"));
    }

    // double-clic sur un point : supprimé
    {
        const int n0 = p.getShape().count;
        const auto& pt = p.getShape().pts[(size_t) idx];
        const auto at = cv.shapeToScreen (pt.x, pt.y);
        cv.mouseDoubleClick (mouseAt (cv, at, {}, 2));
        check (p.getShape().count == n0 - 1, u8 ("Double-clic : point supprim\xc3\xa9"), juce::String (n0) + " -> " + juce::String (p.getShape().count) + " points");
    }

    // outils : miroir x2 et inverser x2 reviennent au départ ; effacer = pompe simple
    {
        const auto ref = p.getShape();
        content.getMirrorButton().onClick();
        const bool changed = p.getShape() != ref;
        content.getMirrorButton().onClick();
        double worst = 0.0;
        for (int i = 0; i <= 200; ++i)
            worst = std::max (worst, std::abs (refLevel (p.getShape(), i / 200.0) - refLevel (ref, i / 200.0)));
        content.getInvertButton().onClick();
        const float y0 = p.getShape().pts[0].y;
        content.getInvertButton().onClick();
        double worst2 = 0.0;
        for (int i = 0; i <= 200; ++i)
            worst2 = std::max (worst2, std::abs (refLevel (p.getShape(), i / 200.0) - refLevel (ref, i / 200.0)));
        check (changed && worst < 1e-5 && worst2 < 1e-5 && std::abs (y0 - (1.0f - ref.pts[0].y)) < 1e-6f,
               u8 ("MIROIR et INVERSER (deux fois = la courbe de d\xc3\xa9part)"), u8 ("\xc3\xa9" "carts ") + fmt (worst, 7) + " / " + fmt (worst2, 7));

        // miroir : la courbe est bien lue à l'envers
        content.getMirrorButton().onClick();
        double worst3 = 0.0;
        for (int i = 1; i < 200; ++i)
            worst3 = std::max (worst3, std::abs (refLevel (p.getShape(), i / 200.0) - refLevel (ref, 1.0 - i / 200.0)));
        content.getMirrorButton().onClick();
        check (worst3 < 1e-4, u8 ("MIROIR : niveau(x) = niveau d'origine(1 - x)"), u8 ("\xc3\xa9" "cart ") + fmt (worst3, 6));
    }

    // vignettes : en édition, copie de la courbe d'usine ; hors édition, choix de la courbe d'usine
    {
        content.getStrip().onPick (Ebb::curveGate);
        const bool copied = p.readSettings().custom && p.getShape().count >= 3;
        content.setEditMode (false);
        content.getStrip().onPick (Ebb::curveBreath);
        const auto s = p.readSettings();
        content.getStrip().onPick (CurveStrip::numCells - 1);
        check (copied && ! s.custom && s.curve == Ebb::curveBreath && p.readSettings().custom,
               u8 ("Vignettes : copie en \xc3\xa9" "dition, choix hors \xc3\xa9" "dition, MA COURBE"), "OK");
    }

    // mes courbes : enregistrer puis recharger
    {
        const auto file = ShapeLibrary::fileFor ("Test banc Ebb");
        file.deleteFile();
        const auto saved = p.getShape();
        content.openSavePanel();
        auto* panel = content.getSavePanel();
        if (panel != nullptr)
        {
            panel->getEditor().setText ("Test banc Ebb", false);
            panel->commit();
        }
        const bool exists = file.existsAsFile();
        p.setShape (Ebb::ShapeMath::defaultShape(), true, true);
        content.loadUserShape (file);
        const bool same = p.getShape() == saved || [&]
        {
            double w = 0.0;
            for (int i = 0; i <= 200; ++i) w = std::max (w, std::abs (refLevel (p.getShape(), i / 200.0) - refLevel (saved, i / 200.0)));
            return w < 1e-5;
        }();
        check (panel != nullptr && exists && same, u8 ("ENREGISTRER puis MES COURBES : la courbe revient \xc3\xa0 l'identique"), file.getFullPathName());
        file.deleteFile();
    }

    // pilules de bande
    content.getBandPills().clickIndex (1);
    check (p.getBand() == 1, u8 ("Pilule GRAVES"), "bande " + juce::String (p.getBand()));
    content.getBandPills().clickIndex (0);

    // captures : affichage et édition d'une courbe perso
    {
        dir.createDirectory();
        FakePlayHead ph;
        ph.playing = true;
        ph.bpm = 126.0;
        p.setPlayHead (&ph);
        juce::AudioBuffer<float> buf (4, 512);
        juce::MidiBuffer midi;
        auto pump = [&] (int blocks)
        {
            for (int b = 0; b < blocks; ++b)
            {
                for (int c = 0; c < 4; ++c) for (int i = 0; i < 512; ++i) buf.setSample (c, i, c < 2 ? 0.5f : 0.0f);
                p.processBlock (buf, midi);
                ph.ppq += 512.0 * ph.bpm / 60.0 / 48000.0;
            }
        };
        // une courbe perso « hypnotique » : chute, palier creusé, remontée bombée, petite respiration
        Ebb::ShapeData s;
        s.pts[0] = { 0.0f, 0.0f, 0.6f };
        s.pts[1] = { 0.22f, 0.55f, -0.3f };
        s.pts[2] = { 0.40f, 0.62f, 0.7f };
        s.pts[3] = { 0.62f, 1.0f, 0.0f };
        s.pts[4] = { 0.80f, 0.86f, -0.4f };
        s.pts[5] = { 1.0f, 1.0f, 0.0f };
        s.count = 6;
        p.setShape (s, true, true);
        p.setParam (ParamIDs::amount, 85.0f);
        content.flash ({}, true);   // pas de message dans l'en-tête sur les captures
        p.setGrid (8);
        content.getGridChooser().setSelected (2);
        content.setEditMode (true);
        pump (20);
        for (int i = 0; i < 8; ++i) ed->tickForTests();
        writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f), dir.getChildFile ("ebb_edition@2x.png"));
        content.setEditMode (false);
        p.setParam (ParamIDs::band, 1.0f);
        p.setParam (ParamIDs::tension, 35.0f);
        p.setParam (ParamIDs::smooth, 20.0f);
        pump (25);
        for (int i = 0; i < 8; ++i) ed->tickForTests();
        writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f), dir.getChildFile ("ebb_graves@2x.png"));
        writePng (ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f), dir.getChildFile ("ebb_graves@1x.png"));
        check (true, "Captures", dir.getChildFile ("ebb_edition@2x.png").getFullPathName() + ", ebb_graves@1x/@2x");
        p.setPlayHead (nullptr);
    }
}
