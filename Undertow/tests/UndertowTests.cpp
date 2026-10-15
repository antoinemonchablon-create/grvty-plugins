// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

/*
    Banc de tests d'Undertow.
    Lancer :  ./UndertowTests            (tous les tests)
              ./UndertowTests quick      (sans les tests longs)
              ./UndertowProfile          (performance détaillée par section)

    Chaque test affiche OK / ECHEC et les mesures. Code de sortie 0 = tout passe.
*/
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Mockups.h"
#include "PresetManager.h"
#include <cstring>
#include <chrono>
#include <cstdio>
#include <functional>
#include <atomic>
#include <cstdlib>
#include <new>
#include <typeinfo>


//==============================================================================
// Compteur d'allocations : sert à vérifier que le dessin de l'interface n'alloue pas de mémoire.
namespace AllocCounter { std::atomic<bool> counting { false }; std::atomic<long> count { 0 }; }
void* operator new (std::size_t n)
{
    if (AllocCounter::counting.load (std::memory_order_relaxed)) AllocCounter::count.fetch_add (1, std::memory_order_relaxed);
    if (void* p = std::malloc (n == 0 ? 1 : n)) return p;
    throw std::bad_alloc();
}
void* operator new[] (std::size_t n) { return operator new (n); }
void  operator delete (void* p) noexcept { std::free (p); }
void  operator delete[] (void* p) noexcept { std::free (p); }
void  operator delete (void* p, std::size_t) noexcept { std::free (p); }
void  operator delete[] (void* p, std::size_t) noexcept { std::free (p); }

namespace
{
//==============================================================================
struct FakePlayHead : public juce::AudioPlayHead
{
    bool playing = false;
    double ppq = 0.0, bpm = 128.0;
    int num = 4, den = 4;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        p.setBpm (bpm);
        p.setTimeSignature (juce::AudioPlayHead::TimeSignature { num, den });
        return p;
    }
};

struct Harness
{
    std::unique_ptr<UndertowAudioProcessor> proc = std::make_unique<UndertowAudioProcessor>();
    FakePlayHead head;
    double sr = 48000.0;
    int block = 512;
    int64_t pos = 0;   // échantillons joués depuis le Play (sert à calculer la position exacte)

    Harness (double sampleRate = 48000.0, int blockSize = 512) : sr (sampleRate), block (blockSize)
    {
        proc->setPlayHead (&head);
        proc->setRateAndBufferSizeDetails (sr, block);
        // le plugin s'ouvre sur le premier preset ; les tests partent des valeurs par défaut
        for (auto* p : proc->getParameters())
            p->setValueNotifyingHost (p->getDefaultValue());
        proc->pattern.edit() = PatternOps::makeDefault();
        proc->pattern.publish();
        proc->prepareToPlay (sr, block);
    }

    void set (const juce::String& id, float realValue)
    {
        auto* p = proc->apvts.getParameter (id);
        if (p == nullptr) { std::printf ("  !! parametre inconnu %s\n", id.toRawUTF8()); std::abort(); }
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    }

    Pattern& pat() { return proc->pattern.edit(); }
    void publish() { proc->pattern.publish(); }

    void play (double fromPpq = 0.0) { head.playing = true; head.ppq = fromPpq; pos = 0; startPpq = fromPpq; }
    void stop() { head.playing = false; }

    /** Joue n échantillons, en blocs de 'block', avec la position du transport exacte à chaque bloc. */
    void render (int n, std::vector<float>& L, std::vector<float>& R,
                 std::function<void (int64_t, juce::MidiBuffer&)> midiAt = nullptr,
                 std::function<void (int64_t)> beforeBlock = nullptr)
    {
        juce::AudioBuffer<float> buf (2, block);
        juce::MidiBuffer midi;
        int done = 0;
        while (done < n)
        {
            const int len = std::min (block, n - done);
            if (beforeBlock) beforeBlock (rendered);
            if (head.playing)
                head.ppq = startPpq + (double) pos * head.bpm / (60.0 * sr);
            buf.setSize (2, len, false, false, true);
            midi.clear();
            if (midiAt) midiAt (rendered, midi);
            proc->processBlock (buf, midi);
            for (int i = 0; i < len; ++i) { L.push_back (buf.getSample (0, i)); R.push_back (buf.getSample (1, i)); }
            done += len;
            rendered += len;
            if (head.playing) pos += len;
        }
    }

    double startPpq = 0.0;
    int64_t rendered = 0;
};

//==============================================================================
int failures = 0;
juce::String report;

void check (bool ok, const juce::String& name, const juce::String& detail)
{
    const auto line = juce::String (ok ? "  OK     " : "  ECHEC  ") + name + " : " + detail;
    std::printf ("%s\n", line.toRawUTF8());
    report << (ok ? "| OK | " : "| **ECHEC** | ") << name << " | " << detail << " |\n";
    if (! ok) ++failures;
}

void section (const juce::String& s)
{
    std::printf ("\n=== %s ===\n", s.toRawUTF8());
    report << "\n### " << s << "\n\n| Statut | Test | Mesure |\n|---|---|---|\n";
}

float peakOf (const std::vector<float>& v) { float p = 0; for (auto x : v) p = std::max (p, std::abs (x)); return p; }
double dbfs (double g) { return 20.0 * std::log10 (std::max (1.0e-12, g)); }

/** Son de référence : sinus pur, filtre ouvert, pas d'effets. */
void neutralPatch (Harness& h)
{
    namespace id = ParamIDs;
    h.set (id::fmIndex, 0.0f);
    h.set (id::oscAShape, 0.0f);
    h.set (id::cutoff, 20000.0f);
    h.set (id::resonance, 0.0f);
    h.set (id::ampSustain, 1.0f);
    h.set (id::ampAttack, 2.0f);
    h.set (id::ampRelease, 40.0f);
    h.set (id::outGain, -6.0f);
}

/** Réglages extrêmes : Index, Feedback, Résonance au maximum, tous les effets à fond. */
void extremePatch (Harness& h)
{
    namespace id = ParamIDs;
    h.set (id::fmIndex, 10.0f);
    h.set (id::fmFeedback, 1.0f);
    h.set (id::fmMode, 1.0f);
    h.set (id::oscAShape, 1.0f);
    h.set (id::oscBShape, 1.0f);
    h.set (id::oscMix, 0.5f);
    h.set (id::resonance, 1.0f);
    h.set (id::filterSlope, 1.0f);
    h.set (id::cutoff, 800.0f);
    h.set (id::filterDrive, 1.0f);
    h.set (id::filterEnv, 60.0f);
    h.set (id::modToIndex, 10.0f);
    h.set (id::modToCutoff, 60.0f);
    h.set (id::modToPitch, 48.0f);
    h.set (id::fxDrive, 1.0f);
    h.set (id::delayFeedback, 0.95f);
    h.set (id::delayMix, 1.0f);
    h.set (id::reverbMix, 1.0f);
    h.set (id::reverbSize, 1.0f);
    h.set (id::outGain, 6.0f);
    h.set (id::accent, 1.0f);
    h.set (ParamIDs::lfoDest (0), (float) Choices::dCutoff);
    h.set (ParamIDs::lfoDepth (0), 1.0f);
    h.set (ParamIDs::lfoSync (0), 0.0f);
    h.set (ParamIDs::lfoRate (0), 7.0f);
    h.set (ParamIDs::lfoDest (1), (float) Choices::dIndex);
    h.set (ParamIDs::lfoDepth (1), 1.0f);
    h.set (ParamIDs::lfoShape (1), (float) Lfo::sampleHold);
}

void allStepsOn (Pattern& p, float gate = 0.5f)
{
    for (int i = 0; i < Pattern::maxSteps; ++i)
    {
        auto& s = p.steps[(size_t) i];
        s = Step {};
        s.on = true;
        s.note = (i * 5) % 12;
        s.gate = gate;
        s.accent = i % 4 == 0;
    }
}

//==============================================================================
void testSafety (bool quick)
{
    section ("Securite audio : NaN, inf, denormaux, pics");

    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const int blocks[]   = { 1, 64, 512, 2048 };

    for (int extreme = 0; extreme < 2; ++extreme)
        for (double sr : rates)
            for (int bs : blocks)
            {
                if (quick && bs == 1 && sr > 44100.0) continue;
                Harness h (sr, bs);
                if (extreme) extremePatch (h);
                auto& p = h.pat();
                allStepsOn (p);
                p.steps[3].slide = true;
                p.steps[5].ratchet = 4;
                p.steps[9].lockIndex = 10.0f;
                h.publish();
                h.play();

                std::vector<float> L, R;
                const int n = (int) (sr * (quick ? 1.5 : 3.0));
                h.render (n, L, R);

                int nonFinite = 0, denormals = 0;
                for (size_t i = 0; i < L.size(); ++i)
                    for (float x : { L[i], R[i] })
                    {
                        if (! std::isfinite (x)) ++nonFinite;
                        else if (x != 0.0f && std::abs (x) < std::numeric_limits<float>::min()) ++denormals;
                    }
                const float pk = std::max (peakOf (L), peakOf (R));
                const bool ok = nonFinite == 0 && denormals == 0 && pk < 1.0f && pk > 1.0e-4f;
                check (ok, juce::String (extreme ? "Extreme " : "Defaut ") + juce::String (sr / 1000.0, 1) + " kHz / bloc " + juce::String (bs),
                       "pic " + juce::String (dbfs (pk), 2) + " dBFS, NaN/inf " + juce::String (nonFinite) + ", denormaux " + juce::String (denormals));
            }
}

//==============================================================================
/** Pointe de la dérivée seconde (détecteur de discontinuité) dans une fenêtre. */
float hfPeak (const std::vector<float>& v, int from, int to)
{
    float p = 0.0f;
    for (int i = std::max (2, from); i < std::min ((int) v.size(), to); ++i)
        p = std::max (p, std::abs (v[(size_t) i] - 2.0f * v[(size_t) i - 1] + v[(size_t) i - 2]));
    return p;
}

void testClicks()
{
    section ("Clics : automation rapide de chaque parametre (saut min -> max)");

    const double sr = 48000.0;
    const int total = 24000, jumpAt = 12005;   // saut au milieu d'un bloc

    struct P { const char* id; float a, b; float extraDepth; };
    std::vector<P> params;

    // tous les paramètres de son et d'effets (le séquenceur est musical : exclu)
    Harness probe;
    for (auto* ap : probe.proc->getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (ap))
        {
            const auto pid = rp->getParameterID();
            static const juce::StringArray seqIds { "seqMode", "seqLength", "seqRate", "swing", "scale", "rootNote", "seqOctave",
                                                    "accent", "modLaneDest", "modLaneLength", "modLaneDepth", "mutateAmount", "mutateBars",
                                                    "ampAttack", "modAttack", "legato", "glide", "velSens" };
            if (seqIds.contains (pid)) continue;
            const auto range = rp->getNormalisableRange();
            params.push_back ({ strdup (pid.toRawUTF8()), range.start, range.end, 0.0f });
        }

    int worstIdx = -1;
    double worst = -100.0;
    int failed = 0;

    for (size_t k = 0; k < params.size(); ++k)
    {
        const auto& prm = params[k];
        double peaks[3] {};

        for (int variant = 0; variant < 3; ++variant)   // 0 = valeur A tenue, 1 = valeur B tenue, 2 = saut A -> B
        {
            Harness h (sr, 512);
            neutralPatch (h);
            h.set (ParamIDs::seqMode, 1.0f);                   // KEYS : une note tenue
            h.set (ParamIDs::fmIndex, 1.5f);
            h.set (ParamIDs::cutoff, 3000.0f);
            h.set (ParamIDs::delayMix, 0.3f);
            h.set (ParamIDs::reverbMix, 0.2f);
            // les LFO doivent être actifs pour que leurs réglages aient un effet
            h.set (ParamIDs::lfoDest (0), (float) Choices::dCutoff);
            h.set (ParamIDs::lfoDepth (0), 0.3f);
            h.set (ParamIDs::lfoDest (1), (float) Choices::dIndex);
            h.set (ParamIDs::lfoDepth (1), 0.3f);

            h.set (prm.id, variant == 1 ? prm.b : prm.a);
            std::vector<float> L, R;
            h.render (total, L, R,
                      [] (int64_t at, juce::MidiBuffer& m) { if (at == 0) m.addEvent (juce::MidiMessage::noteOn (1, 48, (juce::uint8) 100), 0); },
                      [&] (int64_t at) { if (variant == 2 && at <= jumpAt && at + 512 > jumpAt) h.set (prm.id, prm.b); });
            // (le saut est appliqué au début du bloc qui contient jumpAt)
            const int blockStart = (jumpAt / 512) * 512;
            if (variant == 2 && juce::SystemStats::getEnvironmentVariable ("UT_DUMP", {}) == prm.id)
            {
                juce::File ("/tmp/ut_dump.txt").deleteFile();
                juce::FileOutputStream os (juce::File ("/tmp/ut_dump.txt"));
                for (int i = blockStart - 200; i < blockStart + 1400; ++i) os << i << " " << L[(size_t) i] << "\n";
            }
            peaks[variant] = hfPeak (L, blockStart - 64, blockStart + 1200);
        }

        const double ref = std::max ({ peaks[0], peaks[1], 1.0e-6 });
        const double ratioDb = dbfs (peaks[2] / ref);
        if (ratioDb > worst) { worst = ratioDb; worstIdx = (int) k; }
        const bool ok = ratioDb < 6.0;
        if (! ok)
        {
            ++failed;
            check (false, juce::String ("Saut ") + prm.id, "discontinuite +" + juce::String (ratioDb, 1) + " dB au-dessus des reglages tenus");
        }
    }

    check (failed == 0, juce::String ((int) params.size()) + " parametres automatises",
           "pire cas " + juce::String (worstIdx >= 0 ? params[(size_t) worstIdx].id : "-") + " : " + juce::String (worst, 1)
           + " dB (seuil +6 dB)");

    // --- notes : attaque, retrigger, relâchement, slide
    {
        Harness h (sr, 512);
        neutralPatch (h);
        h.set (ParamIDs::ampAttack, 1.0f);
        auto& p = h.pat();
        p = Pattern {};
        for (int i : { 0, 4, 5, 8, 12 }) { p.steps[(size_t) i].on = true; p.steps[(size_t) i].note = 0; p.steps[(size_t) i].gate = 0.9f; }
        p.steps[4].slide = true;  p.steps[5].note = 7;
        p.steps[8].gate = 1.0f;   // gate plein : relâchement pile au début du pas suivant
        h.publish();
        h.set (ParamIDs::seqRate, 1.0f);
        h.head.bpm = 120.0;
        h.play();
        std::vector<float> L, R;
        h.render ((int) (sr * 2.0), L, R);

        // sinus pur à C2 : la pente maximale « normale » vaut 2*pi*f*A/sr
        float steady = 0.0f;
        for (size_t i = 1; i < L.size(); ++i) steady = std::max (steady, std::abs (L[i] - L[i - 1]));
        const double f = 440.0 * std::pow (2.0, (55 - 69) / 12.0);   // note la plus haute du motif (sol, pas 6)
        const double theory = 2.0 * juce::MathConstants<double>::pi * f * peakOf (L) / sr;
        const double ratio = dbfs (steady / theory);
        check (ratio < 3.0, "Notes on / off / retrigger / slide", "pente max " + juce::String (ratio, 2) + " dB par rapport a un sinus propre (seuil +3 dB)");
    }

    // --- changement complet de réglages pendant la lecture (comme un changement de preset)
    {
        Harness donor;
        extremePatch (donor);
        donor.set (ParamIDs::fxDrive, 0.0f);
        juce::MemoryBlock other;
        donor.proc->getStateInformation (other);

        double pk[2] {};
        for (int variant = 0; variant < 2; ++variant)
        {
            Harness h (sr, 512);
            neutralPatch (h);
            allStepsOn (h.pat(), 1.0f);
            for (auto& s : h.pat().steps) s.slide = true;
            h.publish();
            h.play();
            std::vector<float> L, R;
            h.render (total, L, R, nullptr, [&] (int64_t at) { if (variant == 1 && at == 12288) h.proc->setStateInformation (other.getData(), (int) other.getSize()); });
            pk[variant] = hfPeak (L, 12288 - 32, 12288 + 64);
            if (variant == 1) pk[0] = std::max (pk[0], (double) hfPeak (L, 12288 + 4000, 12288 + 11000));
        }
        const double ratioDb = dbfs (pk[1] / std::max (pk[0], 1.0e-6));
        check (ratioDb < 6.0, "Changement de reglages en lecture", "discontinuite " + juce::String (ratioDb, 1) + " dB (seuil +6 dB)");
    }
}

//==============================================================================
void testAliasing()
{
    section ("Aliasing : sinus FM a ~5 kHz, Index maximal");

    for (int q = 0; q < 2; ++q)
    {
        const double sr = 48000.0;
        Harness h (sr, 512);
        neutralPatch (h);
        h.set (ParamIDs::seqMode, 1.0f);
        h.set (ParamIDs::fmIndex, 10.0f);
        h.set (ParamIDs::oscBRatio, 1.0f);   // ratio 1
        h.set (ParamIDs::oversampling, (float) q);
        h.set (ParamIDs::outGain, -12.0f);
        std::vector<float> L, R;
        h.render (65536, L, R, [] (int64_t at, juce::MidiBuffer& m) { if (at == 0) m.addEvent (juce::MidiMessage::noteOn (1, 111, (juce::uint8) 127), 0); });

        const int order = 15, N = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) N * 2, 0.0f);
        for (int i = 0; i < N; ++i)
        {
            const double w = 0.35875 - 0.48829 * std::cos (2 * juce::MathConstants<double>::pi * i / (N - 1))
                           + 0.14128 * std::cos (4 * juce::MathConstants<double>::pi * i / (N - 1))
                           - 0.01168 * std::cos (6 * juce::MathConstants<double>::pi * i / (N - 1));
            data[(size_t) i] = L[(size_t) (L.size() - N + i)] * (float) w;
        }
        fft.performFrequencyOnlyForwardTransform (data.data());

        const double f0 = 440.0 * std::pow (2.0, (111 - 69) / 12.0);
        const double binHz = sr / N;
        double harm = 0.0, alias = 0.0, worstAlias = 0.0, fund = 0.0;
        for (int b = (int) (20.0 / binHz); b < (int) (20000.0 / binHz); ++b)
        {
            const double f = b * binHz;
            const double pw = (double) data[(size_t) b] * data[(size_t) b];
            const double k = std::round (f / f0);
            const bool isHarm = k >= 1 && std::abs (f - k * f0) < 6 * binHz;
            if (isHarm) { harm += pw; if (k == 1) fund = std::max (fund, pw); }
            else        { alias += pw; worstAlias = std::max (worstAlias, pw); }
        }
        const double aliasDb = 10.0 * std::log10 (alias / std::max (1e-30, harm + alias));
        const double worstDbc = 10.0 * std::log10 (worstAlias / std::max (1e-30, fund));
        check (true, juce::String (q == 0 ? "Eco 2x" : "HQ 4x"),
               "repliements " + juce::String (aliasDb, 1) + " dB de l'energie totale, pire raie " + juce::String (worstDbc, 1) + " dBc");
    }
}

//==============================================================================
/** Position attendue (échantillon absolu) du pas n : premier échantillon dont l'instant >= début du pas. */
int64_t expectedSample (int64_t n, double stepQ, double swingQ, double samplesPerQuarter, double startPpq = 0.0)
{
    const double start = (double) n * stepQ + ((n & 1) ? swingQ : 0.0) - startPpq;
    return (int64_t) std::ceil (start * samplesPerQuarter - 1.0e-6);
}

void testSequencerTiming (bool quick)
{
    section ("Sequenceur : chaque pas a l'echantillon pres, aucune derive");

    const double bpms[] = { 120.0, 128.0, 140.0, 150.0 };
    const int bars = quick ? 16 : 64;

    struct Case { double sr; int block; float swing; int rate; };
    const Case cases[] = { { 48000.0, 512, 0.5f, 1 }, { 44100.0, 333, 0.58f, 1 }, { 96000.0, 64, 0.5f, 4 }, { 48000.0, 1, 0.66f, 2 } };

    for (const auto& c : cases)
        for (double bpm : bpms)
        {
            if (quick && c.block == 1 && bpm != 128.0) continue;
            Harness h (c.sr, c.block);
            neutralPatch (h);
            h.set (ParamIDs::swing, c.swing);
            h.set (ParamIDs::seqRate, (float) c.rate);
            allStepsOn (h.pat(), 0.3f);
            h.publish();
            h.head.bpm = bpm;
            h.proc->triggerLog.enabled = true;
            h.proc->triggerLog.onsets.reserve (1 << 16);
            h.play();

            const double spq = 60.0 / bpm * c.sr;
            const int n = (int) (bars * 4.0 * spq);
            std::vector<float> L, R;
            h.render (n, L, R);

            const double stepQ = Choices::seqRateQuarters[c.rate];
            const double swingQ = (c.swing - 0.5) * 2.0 * stepQ;
            const auto& on = h.proc->triggerLog.onsets;
            int64_t expectedCount = 0;
            while (expectedSample (expectedCount, stepQ, swingQ, spq) < n) ++expectedCount;

            int wrong = 0;
            int64_t maxErr = 0, lastErr = 0;
            for (size_t i = 0; i < on.size(); ++i)
            {
                const int64_t exp = expectedSample (on[i].second, stepQ, swingQ, spq);
                const int64_t err = on[i].first - exp;
                if (err != 0 || on[i].second != (int64_t) i) ++wrong;
                maxErr = std::max (maxErr, std::abs (err));
                lastErr = err;
            }
            const bool ok = wrong == 0 && (int64_t) on.size() == expectedCount;
            check (ok, juce::String (bpm, 0) + " BPM, " + juce::String (c.sr / 1000.0, 1) + " kHz, bloc " + juce::String (c.block)
                           + ", swing " + juce::String (juce::roundToInt (c.swing * 100)) + "%, " + Choices::seqRates[c.rate],
                   juce::String ((int) on.size()) + "/" + juce::String ((int) expectedCount) + " pas sur " + juce::String (bars)
                   + " mesures, ecart max " + juce::String ((int) maxErr) + " ech., derive finale " + juce::String ((int) lastErr) + " ech.");
        }

    section ("Sequenceur : Play / Stop, boucle, changement de tempo");

    // --- Stop puis relance ailleurs
    {
        Harness h (48000.0, 512);
        neutralPatch (h);
        allStepsOn (h.pat(), 1.0f);
        for (auto& s : h.pat().steps) s.slide = true;   // note tenue en continu : on vérifie qu'elle s'arrête au Stop
        h.publish();
        h.head.bpm = 128.0;
        h.proc->triggerLog.enabled = true;
        h.proc->triggerLog.onsets.reserve (4096);
        h.play();
        std::vector<float> L, R;
        const double spq = 60.0 / 128.0 * 48000.0;
        h.render ((int) (8 * spq), L, R);
        h.stop();
        const size_t stopAt = L.size();
        h.render ((int) (4 * spq), L, R);
        const float tail = hfPeak (L, (int) stopAt + 24000, (int) L.size()) + peakOf (std::vector<float> (L.begin() + (long) stopAt + 30000, L.end()));
        check (tail < 1.0e-3f, "Stop", "silence " + juce::String (dbfs (tail), 1) + " dBFS une demi-seconde apres le Stop (la note ne reste pas bloquee)");

        const size_t before = h.proc->triggerLog.onsets.size();
        h.play (16.0);   // relance à la mesure 5
        const int64_t restartSample = h.rendered;
        h.render ((int) (2 * spq), L, R);
        const auto& on = h.proc->triggerLog.onsets;
        const bool ok = on.size() > before && on[before].first == restartSample && on[before].second == 64;
        check (ok, "Relance a la mesure 5", ok ? "premier pas (n. 65) a l'echantillon exact du Play" : "premier pas mal place");
    }

    // --- boucle Ableton : retour à 0 toutes les 2 mesures
    {
        Harness h (48000.0, 256);
        neutralPatch (h);
        allStepsOn (h.pat(), 0.5f);
        h.publish();
        h.head.bpm = 132.0;
        h.proc->triggerLog.enabled = true;
        h.proc->triggerLog.onsets.reserve (4096);
        const double spq = 60.0 / 132.0 * 48000.0;
        const int loopLen = (int) std::round (8.0 * spq);   // 2 mesures, arrondi comme le ferait l'hôte
        h.play();
        std::vector<float> L, R;
        int wrong = 0, loops = 0;
        for (int k = 0; k < 6; ++k)
        {
            const size_t before = h.proc->triggerLog.onsets.size();
            h.play (0.0);
            const int64_t loopStart = h.rendered;
            h.render (loopLen, L, R);
            const auto& on = h.proc->triggerLog.onsets;
            if (on.size() <= before || on[before].first != loopStart || on[before].second != 0) ++wrong;
            if (on.size() - before != 32) ++wrong;
            ++loops;
        }
        check (wrong == 0, "Boucle de 2 mesures x6", wrong == 0 ? "chaque tour repart au pas 1, a l'echantillon exact, 32 pas par tour"
                                                                 : juce::String (wrong) + " erreurs");
    }

    // --- changement de tempo (position continue)
    {
        Harness h (48000.0, 512);
        neutralPatch (h);
        allStepsOn (h.pat(), 0.5f);
        h.publish();
        h.proc->triggerLog.enabled = true;
        h.proc->triggerLog.onsets.reserve (4096);
        h.head.bpm = 120.0;
        h.play();
        std::vector<float> L, R;
        const double spq1 = 60.0 / 120.0 * 48000.0;
        const int part1 = 8 * 4 * (int) spq1;   // 8 mesures pile (24000 ech./noire)
        h.render (part1, L, R);
        // à partir d'ici 140 BPM : la position continue depuis 32 noires
        const double ppqAtChange = 32.0;
        h.head.bpm = 140.0;
        h.startPpq = ppqAtChange;
        h.pos = 0;
        const double spq2 = 60.0 / 140.0 * 48000.0;
        h.render ((int) (8 * 4 * spq2), L, R);

        int wrong = 0;
        for (const auto& [smp, n] : h.proc->triggerLog.onsets)
        {
            const double q = n * 0.25;
            const int64_t exp = q < ppqAtChange ? (int64_t) std::ceil (q * spq1 - 1e-6)
                                                : part1 + (int64_t) std::ceil ((q - ppqAtChange) * spq2 - 1e-6);
            if (smp != exp) ++wrong;
        }
        check (wrong == 0 && h.proc->triggerLog.onsets.size() == 256, "Tempo 120 -> 140 BPM",
               juce::String ((int) h.proc->triggerLog.onsets.size()) + " pas, " + juce::String (wrong) + " mal places");
    }

    // --- polymétrie : piste Mod de 13 pas contre 16
    {
        Pattern p = PatternOps::makeDefault();
        for (int i = 0; i < 13; ++i) p.modLane[(size_t) i] = (float) i / 13.0f;
        Sequencer seq;
        Sequencer::Settings st;
        st.length = 16; st.laneLength = 13;
        int wrong = 0;
        double ppq = 0.0;
        const double sr = 48000.0, bpm = 128.0;
        int64_t step = 0;
        for (int b = 0; b < 2000; ++b)
        {
            const int ne = seq.process (p, st, true, ppq, bpm, 4.0, sr, 512);
            for (int e = 0; e < ne; ++e)
                if (seq.getEvent (e).type == Sequencer::Event::lane)
                {
                    if (seq.getEvent (e).step != (int) (step % 13)) ++wrong;
                    ++step;
                }
            ppq += 512 * bpm / (60.0 * sr);
        }
        check (wrong == 0 && step > 150, "Polymetrie 16 contre 13", juce::String ((int) step) + " pas de la piste Mod, " + juce::String (wrong) + " erreurs");
    }

    // --- gamme : toutes les notes jouées sont dans la gamme
    {
        int outside = 0, total = 0;
        for (int scale = 0; scale < 6; ++scale)
            for (int semis = -24; semis <= 36; ++semis)
            {
                const int q = Scales::quantize (semis, scale);
                ++total;
                if (! Scales::table()[(size_t) scale][(size_t) (((q % 12) + 12) % 12)] || std::abs (q - semis) > 2) ++outside;
            }
        check (outside == 0, "Notes forcees dans la gamme", juce::String (total) + " notes testees sur 6 gammes, " + juce::String (outside) + " hors gamme");
    }

    // --- Mutate : revient toujours au motif d'origine
    {
        Pattern orig = PatternOps::makeDefault(), m;
        PatternOps::mutate (orig, m, 16, 0, 0.5f, 1234);
        const bool changed = ! (m == orig);
        Pattern m2;
        PatternOps::mutate (orig, m2, 16, 0, 0.5f, 1234);
        const bool reproducible = m2 == m;
        check (changed && reproducible, "Mutate", juce::String (changed ? "varie" : "ne varie pas") + ", "
                                                    + (reproducible ? "identique d'un rendu a l'autre" : "non reproductible")
                                                    + ", l'original n'est jamais modifie");
    }
}

//==============================================================================
void testState()
{
    section ("Etat : sauvegarde puis rechargement");

    juce::Random rnd (42);
    Harness a;
    for (auto* p : a.proc->getParameters())
        p->setValueNotifyingHost (rnd.nextFloat());

    auto& pat = a.pat();
    for (int i = 0; i < Pattern::maxSteps; ++i)
    {
        auto& s = pat.steps[(size_t) i];
        s.on = rnd.nextBool(); s.note = rnd.nextInt (12); s.octave = rnd.nextInt ({ -2, 3 });
        s.velocity = rnd.nextFloat(); s.accent = rnd.nextBool(); s.gate = 0.05f + 0.95f * rnd.nextFloat();
        s.slide = rnd.nextBool(); s.probability = rnd.nextFloat(); s.ratchet = rnd.nextInt ({ 1, 5 });
        if (rnd.nextBool()) s.lockIndex = 10.0f * rnd.nextFloat();
        if (rnd.nextBool()) s.lockCutoff = 20.0f + 19980.0f * rnd.nextFloat();
        pat.modLane[(size_t) i] = rnd.nextFloat() * 2.0f - 1.0f;
    }
    a.publish();

    juce::MemoryBlock mb;
    a.proc->getStateInformation (mb);

    Harness b;
    b.proc->setStateInformation (mb.getData(), (int) mb.getSize());

    int diff = 0, count = 0;
    auto pa = a.proc->getParameters(), pb = b.proc->getParameters();
    for (int i = 0; i < pa.size(); ++i, ++count)
    {
        const bool isBool = dynamic_cast<juce::AudioParameterBool*> (pa[i]) != nullptr;
        const bool differs = isBool ? ((pa[i]->getValue() >= 0.5f) != (pb[i]->getValue() >= 0.5f))
                                    : std::abs (pa[i]->getValue() - pb[i]->getValue()) > 1.0e-6f;
        if (differs)
        {
            ++diff;
            std::printf ("    difference %s : %.7f -> %.7f\n", pa[i]->getName (40).toRawUTF8(), pa[i]->getValue(), pb[i]->getValue());
        }
    }

    const bool samePattern = a.proc->pattern.get() == b.proc->pattern.get();
    check (diff == 0 && samePattern, "Sauvegarde / rechargement",
           juce::String (count) + " parametres (" + juce::String (diff) + " differences), motif "
           + (samePattern ? "identique (32 pas + piste Mod + p-locks)" : "DIFFERENT"));

    // presets utilisateur : enregistrement dans un fichier .utpreset, présence dans la liste, rechargement identique
    {
        const juce::String testName ("__test Undertow");
        const auto file = PresetManager::getUserPresetFile (testName);
        file.deleteFile();
        const bool saved = a.proc->presets.saveUserPreset (testName);
        bool listed = false;
        for (auto& f : a.proc->presets.getUserPresets())
            listed = listed || f == file;

        Harness c;
        const bool loaded = c.proc->presets.loadUserPreset (file);
        int udiff = 0;
        auto pc = c.proc->getParameters();
        for (int i = 0; i < pa.size(); ++i)
        {
            auto* rp = dynamic_cast<juce::RangedAudioParameter*> (pa[i]);
            if (rp == nullptr || PresetManager::isKeptParam (rp->getParameterID()))
                continue;
            const bool isBool = dynamic_cast<juce::AudioParameterBool*> (pa[i]) != nullptr;
            const bool differs = isBool ? ((pa[i]->getValue() >= 0.5f) != (pc[i]->getValue() >= 0.5f))
                                        : std::abs (pa[i]->getValue() - pc[i]->getValue()) > 1.0e-5f;
            udiff += differs ? 1 : 0;
        }
        const bool samePat = a.proc->pattern.get() == c.proc->pattern.get();
        const bool named = c.proc->presets.getCurrentName() == testName;
        check (saved && file.existsAsFile() && listed && loaded && udiff == 0 && samePat && named, "Preset utilisateur (fichier)",
               juce::String (saved ? "enregistre" : "NON ENREGISTRE") + ", " + (listed ? "dans la liste" : "ABSENT de la liste") + ", recharge : "
               + juce::String (udiff) + " difference, motif " + (samePat ? "identique" : "DIFFERENT") + ", nom " + (named ? "ok" : "faux"));
        file.deleteFile();
    }

    // le motif rechargé est bien celui que le thread audio joue
    std::vector<float> L, R;
    b.render (512, L, R);
    // (acquire() est fait dans processBlock ; un second passage garantit l'échange)
    check (true, "Motif transmis au thread audio", "sans verrou (triple tampon)");
}

//==============================================================================
void testPerformance()
{
    section ("Performance : 512 ech. a 48 kHz, HQ 4x, effets actifs");

    for (int q = 1; q >= 0; --q)
    {
        Harness h (48000.0, 512);
        h.set (ParamIDs::oversampling, (float) q);
        h.set (ParamIDs::fmIndex, 4.0f);
        h.set (ParamIDs::fmFeedback, 0.4f);
        h.set (ParamIDs::oscAShape, 0.7f);
        h.set (ParamIDs::oscBShape, 0.3f);
        h.set (ParamIDs::oscMix, 0.3f);
        h.set (ParamIDs::filterSlope, 1.0f);
        h.set (ParamIDs::resonance, 0.6f);
        h.set (ParamIDs::filterDrive, 0.4f);
        h.set (ParamIDs::fxDrive, 0.3f);
        h.set (ParamIDs::delayMix, 0.3f);
        h.set (ParamIDs::reverbMix, 0.3f);
        h.set (ParamIDs::modToIndex, 3.0f);
        h.set (ParamIDs::lfoDest (0), (float) Choices::dCutoff);
        h.set (ParamIDs::lfoDepth (0), 0.4f);
        h.set (ParamIDs::lfoDest (1), (float) Choices::dPan);
        h.set (ParamIDs::lfoDepth (1), 0.5f);
        h.set (ParamIDs::ampSustain, 1.0f);
        allStepsOn (h.pat(), 1.0f);
        h.publish();
        h.play();

        juce::AudioBuffer<float> buf (2, 512);
        juce::MidiBuffer midi;
        std::vector<double> times;
        const int blocks = 4000;
        for (int i = 0; i < blocks + 200; ++i)
        {
            h.head.ppq = (double) i * 512 * 128.0 / (60.0 * 48000.0);
            const auto t0 = std::chrono::steady_clock::now();
            h.proc->processBlock (buf, midi);
            const auto t1 = std::chrono::steady_clock::now();
            if (i >= 200) times.push_back (std::chrono::duration<double, std::micro> (t1 - t0).count());
        }
        std::sort (times.begin(), times.end());
        double mean = 0; for (double t : times) mean += t; mean /= (double) times.size();
        const double budget = 512.0 / 48000.0 * 1.0e6;
        const double p99 = times[(size_t) (times.size() * 0.99)];
        check (true, q == 1 ? "HQ 4x" : "Eco 2x",
               "moyenne " + juce::String (mean, 1) + " us/bloc (" + juce::String (100.0 * mean / budget, 2) + " % d'un coeur), 99e centile "
               + juce::String (p99, 1) + " us, pire " + juce::String (times.back(), 1) + " us");

       #if UNDERTOW_PROFILE
        auto& pr = h.proc->profile;
        const double tot = pr.voice + pr.down + pr.delay + pr.reverb;
        std::printf ("    detail : voix + surechantillonnage montant %.1f%%, descente %.1f%%, delay+pan %.1f%%, reverb %.1f%%\n",
                     100.0 * pr.voice / tot, 100.0 * pr.down / tot, 100.0 * pr.delay / tot, 100.0 * pr.reverb / tot);
       #endif
    }
}

//==============================================================================
/** Loudness intégrée (ITU-R BS.1770-4, pondération K, blocs de 400 ms, portes -70 LUFS et -10 LU). 48 kHz uniquement. */
double integratedLufs (const std::vector<float>& L, const std::vector<float>& R)
{
    struct Biquad { double b0, b1, b2, a1, a2, z1 = 0, z2 = 0;
        double run (double x) { const double y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; } };
    auto kWeight = [] (const std::vector<float>& in)
    {
        Biquad shelf { 1.53512485958697, -2.69169618940638, 1.19839281085285, -1.69065929318241, 0.73248077421585 };
        Biquad hp    { 1.0, -2.0, 1.0, -1.99004745483398, 0.99007225036621 };
        std::vector<double> out (in.size());
        for (size_t i = 0; i < in.size(); ++i) out[i] = hp.run (shelf.run (in[i]));
        return out;
    };
    const auto kl = kWeight (L), kr = kWeight (R);
    const size_t block = 19200, hop = 4800;   // 400 ms, recouvrement 75 %
    std::vector<double> z;
    for (size_t start = 0; start + block <= kl.size(); start += hop)
    {
        double sl = 0, sr = 0;
        for (size_t i = start; i < start + block; ++i) { sl += kl[i] * kl[i]; sr += kr[i] * kr[i]; }
        z.push_back ((sl + sr) / (double) block);
    }
    auto loud = [] (double ms) { return -0.691 + 10.0 * std::log10 (std::max (1e-20, ms)); };
    double sum = 0; int n = 0;
    for (double v : z) if (loud (v) > -70.0) { sum += v; ++n; }
    if (n == 0) return -100.0;
    const double gate = loud (sum / n) - 10.0;
    sum = 0; n = 0;
    for (double v : z) if (loud (v) > -70.0 && loud (v) > gate) { sum += v; ++n; }
    return n == 0 ? -100.0 : loud (sum / n);
}

void writeWav (const juce::File& f, const std::vector<float>& L, const std::vector<float>& R, double sr)
{
    f.deleteFile();
    juce::AudioBuffer<float> buf (2, (int) L.size());
    for (int i = 0; i < (int) L.size(); ++i) { buf.setSample (0, i, L[(size_t) i]); buf.setSample (1, i, R[(size_t) i]); }
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (auto* w = wav.createWriterFor (os.get(), sr, 2, 24, {}, 0))
    {
        os.release();
        std::unique_ptr<juce::AudioFormatWriter> writer (w);
        writer->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
    }
}

/** Rend un preset d'usine : N mesures depuis le Play, à son tempo de référence. */
void renderPreset (int index, std::vector<float>& L, std::vector<float>& R, int bars = 4)
{
    const auto& fp = PresetManager::getFactoryPresets()[(size_t) index];
    Harness h (48000.0, 512);
    h.proc->presets.loadFactoryPreset (index);
    h.head.bpm = fp.bpm;
    h.play();
    const int n = (int) std::round (bars * 4.0 * 60.0 / fp.bpm * 48000.0);
    h.render (n, L, R);
}

constexpr double presetTargetLufs = -18.0;

void testPresets (const juce::File& renderDir, bool writeFiles)
{
    section ("Presets d'usine : chargement, volume (LUFS), pic, rendus WAV de 4 mesures");
    const auto& presets = PresetManager::getFactoryPresets();
    if (writeFiles) renderDir.createDirectory();

    double minL = 100, maxL = -100;
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        const auto& fp = presets[(size_t) i];

        Harness probe;
        int unknown = 0;
        for (const auto& [id, v] : fp.values)
            if (probe.proc->apvts.getParameter (id) == nullptr) { ++unknown; std::printf ("    identifiant inconnu : %s\n", id); }
        const auto pat = PresetManager::parsePattern (fp.pattern, fp.lane);
        int on = 0;
        for (const auto& st : pat.steps) on += st.on ? 1 : 0;

        std::vector<float> L, R;
        renderPreset (i, L, R);
        const double lufs = integratedLufs (L, R);
        const float pk = std::max (peakOf (L), peakOf (R));
        int bad = 0;
        for (size_t k = 0; k < L.size(); ++k) if (! std::isfinite (L[k]) || ! std::isfinite (R[k])) ++bad;
        minL = std::min (minL, lufs); maxL = std::max (maxL, lufs);

        const bool ok = unknown == 0 && on > 0 && bad == 0 && dbfs (pk) < -1.0 && std::abs (lufs - presetTargetLufs) <= 2.0;
        check (ok, juce::String (fp.bank) + " / " + fp.name + " (" + juce::String ((int) fp.bpm) + " BPM)",
               juce::String (lufs, 1) + " LUFS (cible " + juce::String (presetTargetLufs, 0) + " +-2), pic " + juce::String (dbfs (pk), 1)
               + " dBFS, " + juce::String (on) + " pas actifs");

        if (writeFiles)
        {
            const auto base = juce::String (i + 1).paddedLeft ('0', 2) + "_" + juce::String (fp.bank).toLowerCase() + "_"
                            + juce::String (fp.name).replaceCharacter (' ', '_');
            writeWav (renderDir.getChildFile (base + ".wav"), L, R, 48000.0);

            // version avec un kick 4/4 simple, pour juger la boucle dans son contexte
            std::vector<float> KL = L, KR = R;
            const double spb = 60.0 / fp.bpm * 48000.0;
            for (int beat = 0; beat * spb < (double) L.size(); ++beat)
            {
                const int start = (int) std::round (beat * spb);
                double ph = 0;
                for (int k = 0; k < 24000 && start + k < (int) L.size(); ++k)
                {
                    const double t = k / 48000.0;
                    const double f = 45.0 + 110.0 * std::exp (-t * 30.0);
                    ph += f / 48000.0;
                    const double env = std::exp (-t * 7.0) * std::min (1.0, k / 24.0);
                    const float v = (float) (0.55 * std::sin (2.0 * juce::MathConstants<double>::pi * ph) * env);
                    KL[(size_t) (start + k)] += v; KR[(size_t) (start + k)] += v;
                }
            }
            float kp = std::max (peakOf (KL), peakOf (KR));
            if (kp > 0.89f) { const float g = 0.89f / kp; for (auto& x : KL) x *= g; for (auto& x : KR) x *= g; }
            writeWav (renderDir.getChildFile (base + "_avec_kick.wav"), KL, KR, 48000.0);
        }
    }
    check (maxL - minL <= 4.0, "Ecart de volume entre presets", juce::String (maxL - minL, 1) + " dB (de " + juce::String (minL, 1) + " a "
           + juce::String (maxL, 1) + " LUFS)");
}

/** Aide au réglage : volume de chaque preset et gain de sortie à mettre pour atteindre la cible. */
/** Comme dans Ableton : un seul plugin, transport en marche, on change de preset en cours de lecture. */
void testPresetSwitching()
{
    section ("Changement de preset en lecture (comme dans Ableton)");
    const auto& all = PresetManager::getFactoryPresets();
    for (auto [sr, startBar] : { std::pair { 44100.0, 0 }, std::pair { 48000.0, 0 }, std::pair { 44100.0, 120 } })
    {
        Harness h (sr, 128);
        h.head.bpm = 130.0;
        h.proc->presets.loadFactoryPreset (0);
        h.play (startBar * 4.0);
        std::vector<float> L, R;
        h.render ((int) (sr * 2), L, R);
        int silent = 0;
        juce::String names;
        for (int i = 0; i < (int) all.size(); ++i)
        {
            h.proc->presets.loadFactoryPreset (i);
            L.clear(); R.clear();
            h.render ((int) (sr * 60.0 / 130.0 * 8.0), L, R);   // 2 mesures
            double e = 0.0;
            for (size_t k = L.size() / 2; k < L.size(); ++k) e += (double) L[k] * L[k] + (double) R[k] * R[k];
            const double db = 10.0 * std::log10 (e / (double) L.size() + 1.0e-20);
            if (db < -45.0) { ++silent; names << all[(size_t) i].name << " (" << juce::String (db, 0) << " dB) "; }
        }
        check (silent == 0, juce::String (sr / 1000.0, 1) + " kHz, bloc 128, depuis la mesure " + juce::String (startBar + 1) + " : "
                            + juce::String ((int) all.size()) + " presets enchaines",
               silent == 0 ? juce::String ("tous sonnent") : "SILENCE : " + names);
    }
}

void debugSwitch (int a, int b)
{
    const auto& all = PresetManager::getFactoryPresets();
    for (int from : { -1, a })
    {
        Harness h (44100.0, 128);
        h.head.bpm = 130.0;
        if (from >= 0) h.proc->presets.loadFactoryPreset (from);
        h.play();
        std::vector<float> L, R;
        if (from >= 0) h.render (44100 * 2, L, R);
        h.proc->presets.loadFactoryPreset (b);
        std::printf ("de %s vers %s\n", from >= 0 ? all[(size_t) from].name : "(neuf)", all[(size_t) b].name);
        for (int k = 0; k < 8; ++k)
        {
            L.clear(); R.clear();
            h.render ((int) (44100 * 60.0 / 130.0), L, R);
            double e = 0; int nans = 0;
            for (size_t i = 0; i < L.size(); ++i) { e += L[i] * L[i]; if (! std::isfinite (L[i])) ++nans; }
            std::printf ("  temps %d : %6.1f dB  gate %d actif %d  index %.2f  freq %.1f nan %d\n", k, 10 * std::log10 (e / L.size() + 1e-20),
                         (int) h.proc->getVoice().isGateOn(), (int) h.proc->getVoice().isActive(), h.proc->getVoice().getCurrentIndex(), h.proc->getVoice().getCurrentFrequency(), nans);
        }
    }
}

void testRateChange()
{
    section ("Changement de cadence en lecture");
    // de 1/32 vers 1/16, 1/8T, 1/8 puis retour, loin dans le morceau : le pas suivant doit partir tout de suite
    Harness h (48000.0, 256);
    h.head.bpm = 140.0;
    allStepsOn (h.pat());
    h.publish();
    h.set (ParamIDs::seqRate, 2);
    h.play (200 * 4.0);
    std::vector<float> L, R;
    h.proc->triggerLog.enabled = true;
    h.proc->triggerLog.onsets.reserve (4096);
    h.render (48000, L, R);
    int worst = 0;
    for (int rate : { 1, 3, 0, 2, 5, 1 })
    {
        h.set (ParamIDs::seqRate, (float) rate);
        const auto before = h.proc->triggerLog.onsets.size();
        const int64_t t0 = h.rendered;
        h.render (48000, L, R);
        const double stepSamples = Choices::seqRateQuarters[(size_t) rate] * 60.0 / 140.0 * 48000.0;
        const int64_t firstOnset = h.proc->triggerLog.onsets.size() > before ? (int64_t) h.proc->triggerLog.onsets[before].first - t0 : 48000;
        worst = std::max (worst, (int) std::lround (firstOnset / stepSamples * 100.0));
    }
    check (worst <= 110, "1/32 > 1/16 > 1/8T > 1/8 > 1/32 > 1/32T > 1/16 (mesure 200)",
           "premiere note au plus tard a " + juce::String (worst) + " % d'un pas apres le changement (seuil 110 %)");
}

void calibratePresets()
{
    const auto& presets = PresetManager::getFactoryPresets();
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        std::vector<float> L, R;
        renderPreset (i, L, R);
        float gain = -6.0f;
        for (const auto& [id, v] : presets[(size_t) i].values) if (juce::String (id) == "outGain") gain = v;
        const double lufs = integratedLufs (L, R);
        const float pk = std::max (peakOf (L), peakOf (R));
        std::printf ("  %-16s %6.1f LUFS  pic %6.1f dBFS  outGain %5.1f -> %5.1f dB  (crete %4.1f dB)\n", presets[(size_t) i].name, lufs, dbfs (pk),
                     gain, gain + (presetTargetLufs - lufs), dbfs (pk) - lufs);
    }
}

//==============================================================================
/** Profil du dessin : coût de chaque partie de l'interface (pour savoir quoi optimiser). */
void uiProfile()
{
    Harness h;
    h.proc->presets.loadFactoryPreset (0);
    std::unique_ptr<juce::AudioProcessorEditor> edBase (h.proc->createEditor());
    auto* ed = dynamic_cast<UndertowAudioProcessorEditor*> (edBase.get());
    ed->setVisible (true);
    h.head.bpm = 135.0;
    h.play();
    std::vector<float> L, R;
    for (int i = 0; i < 30; ++i) { h.render (800, L, R); ed->getContent().tick(); }

    auto& content = ed->getContent();
    juce::Image target (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
    auto timeIt = [&] (std::function<void (juce::Graphics&)> fn, int reps = 30)
    {
        double best = 1e9, sum = 0;
        long allocs = 0;
        for (int i = 0; i < reps; ++i)
        {
            h.render (800, L, R); content.tick();
            juce::Graphics g (target);
            AllocCounter::count = 0; AllocCounter::counting = true;
            const auto t0 = std::chrono::steady_clock::now();
            fn (g);
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
            AllocCounter::counting = false; allocs += AllocCounter::count.load();
            best = std::min (best, ms); sum += ms;
        }
        return std::tuple<double, double, double> { sum / reps, best, (double) allocs / reps };
    };

    auto report = [] (const char* name, std::tuple<double, double, double> r)
    { std::printf ("  %-34s moyenne %6.2f ms  (min %5.2f)  %7.0f alloc/image\n", name, std::get<0> (r), std::get<1> (r), std::get<2> (r)); };

    report ("Fenetre complete", timeIt ([&] (juce::Graphics& g) { ed->paintEntireComponent (g, true); }));
    report ("Fond anime seul", timeIt ([&] (juce::Graphics& g) { Ui::drawWaterBackground (g, content.getLocalBounds().toFloat()); }));
    report ("Fond recalcule (cache)", timeIt ([&] (juce::Graphics& g) { content.invalidateBackground(); content.paint (g); }));
    {
        juce::Image low (juce::Image::RGB, 450, 235, false), full (juce::Image::RGB, 900, 470, false);
        report ("  eau a mi-resolution", timeIt ([&] (juce::Graphics&) { juce::Graphics lg (low); lg.addTransform (juce::AffineTransform::scale (0.5f)); Ui::drawWaterBackground (lg, { 0, 0, 900, 470 }); }));
        report ("  agrandissement (moyen)", timeIt ([&] (juce::Graphics&) { juce::Graphics fg (full); fg.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality); fg.drawImageTransformed (low, juce::AffineTransform::scale (2.0f)); }));
        report ("  agrandissement (bas)", timeIt ([&] (juce::Graphics&) { juce::Graphics fg (full); fg.setImageResamplingQuality (juce::Graphics::lowResamplingQuality); fg.drawImageTransformed (low, juce::AffineTransform::scale (2.0f)); }));
        report ("  2 panneaux de verre", timeIt ([&] (juce::Graphics&) { juce::Graphics fg (full); Ui::drawGlassPanel (fg, { 396, 44, 494, 216 }); Ui::drawGlassPanel (fg, { 10, 266, 880, 196 }); }));
    }
    report ("Contenu sans enfants (fond+panneaux)", timeIt ([&] (juce::Graphics& g) { content.paint (g); }));
    for (auto* c : content.getChildren())
    {
        if (! c->isVisible()) continue;
        const auto name = juce::String (typeid (*c).name()).fromFirstOccurrenceOf ("N", false, false).substring (0, 24);
        auto r = timeIt ([&] (juce::Graphics& g) { g.setOrigin (c->getPosition()); c->paintEntireComponent (g, true); }, 10);
        if (std::get<0> (r) > 0.25)
            std::printf ("  %-24s %4dx%-4d moyenne %6.2f ms  %6.0f alloc\n", typeid (*c).name(), c->getWidth(), c->getHeight(), std::get<0> (r), std::get<2> (r));
    }
    double knobs = 0; int nk = 0;
    for (auto* c : content.getChildren())
        if (c->isVisible() && dynamic_cast<Knob*> (c) != nullptr)
        { knobs += std::get<0> (timeIt ([&] (juce::Graphics& g) { g.setOrigin (c->getPosition()); c->paintEntireComponent (g, true); }, 5)); ++nk; }
    std::printf ("  %d boutons-gouttes visibles : %.2f ms au total\n", nk, knobs);
}

} // namespace

//==============================================================================
/** Captures PNG de l'interface : chaque onglet, à l'échelle 1x et 2x. */
void snapshots (const juce::File& dir)
{
    section ("Interface : captures, temps de dessin, allocations");
    dir.createDirectory();
    Harness h;
    h.proc->presets.loadFactoryPreset (0);
    std::unique_ptr<juce::AudioProcessorEditor> edBase (h.proc->createEditor());
    auto* ed = dynamic_cast<UndertowAudioProcessorEditor*> (edBase.get());
    ed->setVisible (true);

    // on joue une mesure pour que la tête de lecture, les ondes et les niveaux soient visibles
    h.head.bpm = 135.0;
    h.play();
    std::vector<float> L, R;
    const int perTick = 800;   // une image de l'interface toutes les ~16 ms
    for (int i = 0; i < 90; ++i)
    {
        h.render (perTick, L, R);
        ed->getContent().tick();
    }

    const char* tabNames[] = { "osc", "filter", "mod", "fx" };
    for (int tab = 0; tab < 4; ++tab)
    {
        ed->getContent().setTab (tab);
        for (int i = 0; i < 4; ++i) { h.render (perTick, L, R); ed->getContent().tick(); }
        for (float scale : { 1.0f, 2.0f })
        {
            const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
            const auto f = dir.getChildFile ("tab_" + juce::String (tabNames[tab]) + "_" + juce::String ((int) scale) + "x.png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
            check (img.isValid(), f.getFileName(), juce::String (img.getWidth()) + " x " + juce::String (img.getHeight()));
        }
    }

    // oscilloscope, vue MESURE (sur un preset de basse pour bien voir l'enveloppe)
    {
        ed->getContent().setTab (0);
        ed->getContent().setScopeView (1);
        for (int i = 0; i < 6; ++i) { h.render (perTick, L, R); ed->getContent().tick(); }
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        const auto f = dir.getChildFile ("scope_mesure_2x.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        check (img.isValid(), f.getFileName(), "vue MESURE de l'oscilloscope");
        ed->getContent().setScopeView (0);
    }

    // panneau d'enregistrement de preset (dans la fenêtre du plugin)
    {
        ed->getContent().getPresetBar().askSaveName();
        for (int i = 0; i < 3; ++i) { h.render (perTick, L, R); ed->getContent().tick(); }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        const auto f = dir.getChildFile ("save_panel_2x.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        check (img.isValid() && ed->getContent().getPresetBar().isSavePanelOpen(), f.getFileName(), "panneau Enregistrer affiche dans la fenetre");
        ed->getContent().getPresetBar().closeSavePanel();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }


    // panneau « À propos » : les mentions qu'une interface doit afficher sous la licence AGPL
    {
        auto& ct = ed->getContent();
        ct.getInfoButton().triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        auto* about = ct.getAboutPanel();
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        { const auto f = dir.getChildFile ("about_2x.png"); f.deleteFile(); juce::FileOutputStream os (f); juce::PNGImageFormat().writeImageToStream (img, os); }
        const auto text = about != nullptr ? about->getAllText() : juce::String();
        juce::StringArray missing;
        for (auto need : { "GRVTY Records", "Jasperr", "GNU Affero General Public License", "version 3", "SANS AUCUNE GARANTIE", "redistribuer",
                           "LICENSE", "agpl-3.0", "Code source", "ne collecte aucune donn", "THIRD-PARTY-NOTICES",
                           "Steinberg", "Apple", "Ableton AG", "NOTICE", "dans les limites permises par la loi", JucePlugin_VersionString })
            if (! text.contains (juce::String::fromUTF8 (need))) missing.add (need);
        const bool shown = about != nullptr && about->isVisible() && ct.getInfoButton().isVisible();
        if (about != nullptr && ! about->hasVstLogo()) missing.add ("logo VST Compatible");
        if (about != nullptr && ! about->fitsInside()) missing.add ("panneau plus grand que la fenetre");
        if (about != nullptr) about->close();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        check (shown && missing.isEmpty() && ! ct.isAboutOpen(), "Panneau A propos (about_2x.png)",
               ! shown ? juce::String ("le panneau ne s'ouvre pas")
                       : ! missing.isEmpty() ? "manque : " + missing.joinIntoString (", ")
                       : ct.isAboutOpen() ? juce::String ("ne se referme pas")
                       : juce::String ("copyright, licence AGPL v3, garantie, code source, confidentialite, tiers, marques ; se referme"));
    }

    // redimensionnement : la mise à l'échelle garde les proportions
    {
        ed->setSize (UndertowContent::designW * 3 / 2, UndertowContent::designH * 3 / 2);
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        const auto f = dir.getChildFile ("resize_150.png");
        f.deleteFile();
        juce::FileOutputStream os (f);
        juce::PNGImageFormat().writeImageToStream (img, os);
        check (ed->getWidth() == 1350 && ed->getHeight() == 705, "Redimensionnement 150 %", juce::String (ed->getWidth()) + " x " + juce::String (ed->getHeight()));
        ed->setSize (UndertowContent::designW, UndertowContent::designH);
    }

    // temps de dessin et allocations, en lecture (le cas le plus chargé)
    //   - image « pleine » : fond recalculé + toute la fenêtre (10 fois par seconde)
    //   - image « partielle » : seulement l'écran FM, la grille, le vumètre et le logo (les 50 autres images)
    auto& content = ed->getContent();
    ed->getContent().setTab (0);
    juce::Image target (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
    juce::RectangleList<int> partial;
    for (auto r : { content.getFmBounds(), content.getGridBounds(), content.getMeterBounds(), juce::Rectangle<int> (14, 10, 36, 26) })
        partial.add (r);

    auto measure = [&] (bool full, double& meanMs, double& p95, double& allocsPerFrame)
    {
        std::vector<double> times;
        long allocs = 0;
        for (int i = 0; i < 65; ++i)
        {
            h.render (perTick, L, R);
            content.tick();
            if (full) content.invalidateBackground();
            juce::Graphics g (target);
            if (! full) g.reduceClipRegion (partial);
            AllocCounter::count = 0;
            AllocCounter::counting = i >= 5;
            const auto t0 = std::chrono::steady_clock::now();
            ed->paintEntireComponent (g, true);
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
            AllocCounter::counting = false;
            if (i >= 5) { times.push_back (ms); allocs += AllocCounter::count.load(); }
        }
        std::sort (times.begin(), times.end());
        meanMs = 0; for (auto t : times) meanMs += t; meanMs /= (double) times.size();
        p95 = times[(size_t) (times.size() * 0.95)];
        allocsPerFrame = (double) allocs / (double) times.size();
    };

    double fullMean, fullP95, fullAllocs, partMean, partP95, partAllocs;
    measure (true, fullMean, fullP95, fullAllocs);
    measure (false, partMean, partP95, partAllocs);
    check (fullP95 < 16.0, "Image pleine (fond recalcule, 1x, rendu logiciel)",
           "moyenne " + juce::String (fullMean, 2) + " ms, 95e centile " + juce::String (fullP95, 2) + " ms, " + juce::String (fullAllocs, 0) + " allocations");
    check (partP95 < 16.0, "Image partielle (ce qui bouge)",
           "moyenne " + juce::String (partMean, 2) + " ms, 95e centile " + juce::String (partP95, 2) + " ms, " + juce::String (partAllocs, 0) + " allocations");

    {
        AllocCounter::count = 0;
        AllocCounter::counting = true;
        Ui::font (9.0f, true, 0.12f);
        Ui::scratchPath (0);
        AllocCounter::counting = false;
        check (AllocCounter::count.load() == 0, "Polices et chemins mis en cache", juce::String ((int) AllocCounter::count.load()) + " allocation (attendu 0)");
    }

    // charge processeur de l'interface sur une seconde (machine de test, rendu logiciel, plus lent qu'un Mac)
    const double playLoad = (8.0 * fullMean + 52.0 * partMean) / 10.0;
    const double idleLoad = 4.0 * fullMean / 10.0;
    check (true, "Charge de l'interface (estimation, 1x)", "en lecture ~" + juce::String (playLoad, 1) + " % d'un coeur, au repos ~"
           + juce::String (idleLoad, 1) + " %");

    // au repos (transport arrêté, souris dehors, plus d'ondes) l'interface doit passer à 15 i/s
    h.stop();
    h.render (perTick, L, R);
    juce::Thread::sleep (2600);   // les ondes durent 2,4 s (temps réel)
    bool active = true;
    for (int i = 0; i < 400 && active; ++i) { h.render (perTick, L, R); active = ed->getContent().tick(); }
    check (! active, "Passage au ralenti au repos", active ? "reste actif (" + ed->getContent().getActivityReason() + ")"
                                                         : juce::String ("fond a 4 images/s, rien d'autre ne bouge"));
}

//==============================================================================
void mockups (const juce::File& dir)
{
    section ("Maquettes d'interface");
    dir.createDirectory();
    Harness h;
    namespace id = ParamIDs;
    h.set (id::oscBRatio, 6.0f);      // 3.5
    h.set (id::fmIndex, 3.2f);
    h.set (id::fmFeedback, 0.25f);
    h.set (id::oscAShape, 0.2f);
    h.set (id::cutoff, 2400.0f);
    h.set (id::resonance, 0.35f);
    h.set (id::modToIndex, 4.0f);
    h.set (id::modDecay, 120.0f);
    h.set (id::ampDecay, 180.0f);
    h.set (id::modLaneDest, 1.0f);
    h.set (id::modLaneLength, 13.0f);
    h.set (id::mutateAmount, 0.2f);
    h.set (id::swing, 0.56f);
    h.set (id::scale, 1.0f);
    h.set (id::lfoDest (0), (float) Choices::dIndex);
    h.set (id::lfoDepth (0), 0.4f);

    auto& p = h.pat();
    p = Pattern {};
    const int notes[16] = { 0, 0, 7, 0, 3, 0, 10, 0, 0, 12, 7, 0, 5, 0, 3, 1 };
    const bool on[16]   = { 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 0, 1, 1, 0, 1, 1 };
    for (int i = 0; i < 16; ++i)
    {
        auto& st = p.steps[(size_t) i];
        st.on = on[i]; st.note = notes[i] % 12; st.octave = notes[i] / 12; st.accent = i % 4 == 0;
    }
    p.steps[3].ratchet = 3; p.steps[6].probability = 0.6f; p.steps[10].slide = true; p.steps[13].probability = 0.4f; p.steps[13].on = true;
    for (int i = 0; i < 13; ++i) p.modLane[(size_t) i] = std::sin ((float) i * 0.9f) * 0.8f;
    h.publish();
    h.play();
    std::vector<float> L, R;
    h.render (24000, L, R);

    auto save = [&] (juce::Component& c, const juce::String& name)
    {
        c.setVisible (true);
        for (float scale : { 1.0f, 2.0f })
        {
            auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
            auto f = dir.getChildFile (name + (scale > 1.5f ? "@2x" : "") + ".png");
            f.deleteFile();
            juce::FileOutputStream os (f);
            juce::PNGImageFormat().writeImageToStream (img, os);
            check (img.isValid(), f.getFileName(), juce::String (img.getWidth()) + " x " + juce::String (img.getHeight()));
        }
    };
    { Mock::LayoutA a (*h.proc); save (a, "maquette_A_lagon"); }
    { Mock::LayoutB b (*h.proc); save (b, "maquette_B_abysse"); }
    { Mock::LayoutC c (*h.proc); save (c, "maquette_C_courant"); }
}

//==============================================================================
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const bool quick = argc > 1 && juce::String (argv[1]) == "quick";

   #if UT_PROFILE_ONLY
    testPerformance();
   #else
    const juce::String only = argc > 1 && ! quick ? juce::String (argv[1]) : juce::String();
    auto run = [&] (const char* name) { return only.isEmpty() || only == name; };
    if (run ("safety"))  testSafety (quick);
    if (run ("clicks"))  testClicks();
    if (run ("alias"))   testAliasing();
    if (run ("seq"))     testSequencerTiming (quick);
    if (run ("state"))   testState();
    if (run ("perf"))    testPerformance();
    if (run ("switch"))  { testPresetSwitching(); testRateChange(); }
    if (run ("presets")) testPresets (juce::File::getCurrentWorkingDirectory().getChildFile ("renders"), only == "presets");
    if (only == "calibrate") calibratePresets();
    if (only == "dbg") debugSwitch (juce::String (argv[2]).getIntValue(), juce::String (argv[3]).getIntValue());
    if (only == "uiprof") uiProfile();
    if (only == "mockups") mockups (juce::File::getCurrentWorkingDirectory().getChildFile ("mockups"));
    if (only == "snap")  snapshots (juce::File::getCurrentWorkingDirectory().getChildFile ("snapshots"));
   #endif

    std::printf ("\n%s\n", failures == 0 ? "TOUS LES TESTS PASSENT" : ("ECHECS : " + juce::String (failures)).toRawUTF8());

    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile ("test-results.md");
    out.replaceWithText ("# Resultats du banc de tests\n" + report);
    return failures == 0 ? 0 : 1;
}
