// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

/*
    Banc de tests d'Ebb.
    Lancer :  ./EbbTests            (tous les tests)
              ./EbbTests curves     (un seul groupe : ids, curves, shape, audio, zero, clicks, sync, transport, midi,
                                     kick, sidechain, state, perf, alloc, snap, mockups)

    Chaque test affiche OK / ECHEC et les mesures. Code de sortie 0 = tout passe.
    Le compte rendu est aussi écrit dans test-results.md.
*/
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Mockups.h"
#include <chrono>
#include <cstdio>
#include <functional>
#include <atomic>
#include <cstdlib>
#include <new>
#include <random>
#include <execinfo.h>
#include <unistd.h>
#include <pthread.h>

//==============================================================================
// Compteur d'allocations (repris de Confluence) : vérifie que processBlock et notre code de dessin n'allouent jamais.
namespace AllocCounter
{
    struct Flag
    {
        std::atomic<bool> on { false };
        std::atomic<pthread_t> owner {};
        Flag& operator= (bool v) noexcept { if (v) owner.store (pthread_self()); on.store (v); return *this; }
        bool active() const noexcept { return on.load (std::memory_order_relaxed) && pthread_equal (owner.load (std::memory_order_relaxed), pthread_self()); }
    };
    Flag counting;
    std::atomic<long> count { 0 };
    bool trace = false;
    thread_local bool inHook = false;
    inline void hit()
    {
        if (inHook || ! counting.active()) return;
        count.fetch_add (1, std::memory_order_relaxed);
        if (trace)
        {
            inHook = true;
            void* frames[24];
            const int n = backtrace (frames, 24);
            static const char sep[] = "---- allocation\n";
            (void) ! write (2, sep, sizeof (sep) - 1);
            backtrace_symbols_fd (frames, n, 2);
            inHook = false;
        }
    }
}
extern "C"
{
    void* __libc_malloc (std::size_t);
    void* __libc_calloc (std::size_t, std::size_t);
    void* __libc_realloc (void*, std::size_t);
    void* __libc_memalign (std::size_t, std::size_t);
    void  __libc_free (void*);
    void* malloc (std::size_t n)                    { AllocCounter::hit(); return __libc_malloc (n); }
    void* calloc (std::size_t a, std::size_t b)     { AllocCounter::hit(); return __libc_calloc (a, b); }
    void* realloc (void* p, std::size_t n)          { AllocCounter::hit(); return __libc_realloc (p, n); }
    void* memalign (std::size_t a, std::size_t n)   { AllocCounter::hit(); return __libc_memalign (a, n); }
    void* aligned_alloc (std::size_t a, std::size_t n) { AllocCounter::hit(); return __libc_memalign (a, n); }
    int   posix_memalign (void** out, std::size_t a, std::size_t n)
    {
        AllocCounter::hit();
        *out = __libc_memalign (a, n);
        return *out != nullptr ? 0 : 12;
    }
    void  free (void* p)                            { __libc_free (p); }
}
void* operator new (std::size_t n)
{
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
namespace id = ParamIDs;

//==============================================================================
int failures = 0;
juce::String report;

void check (bool ok, const juce::String& name, const juce::String& detail)
{
    const auto line = juce::String (ok ? "  OK     " : "  ECHEC  ") + name + " : " + detail;
    std::printf ("%s\n", line.toRawUTF8());
    std::fflush (stdout);
    report << (ok ? "| OK | " : "| **ECHEC** | ") << name << " | " << detail << " |\n";
    if (! ok) ++failures;
}

void section (const juce::String& s)
{
    std::printf ("\n=== %s ===\n", s.toRawUTF8());
    report << "\n### " << s << "\n\n| Statut | Test | Mesure |\n|---|---|---|\n";
}

juce::String u8 (const char* s) { return juce::String (juce::CharPointer_UTF8 (s)); }
juce::String fmt (double v, int dec = 2) { return juce::String (v, dec); }

//==============================================================================
/** Courbes de référence, réécrites ici d'après la description (indépendamment de Source/dsp/Curves.h). */
double refCurve (int c, double x)
{
    const double pi = std::acos (-1.0);
    x = std::min (1.0, std::max (0.0, x));
    auto ss = [] (double u) { u = std::min (1.0, std::max (0.0, u)); return 3.0 * u * u - 2.0 * u * u * u; };
    auto expo = [] (double u, double k) { u = std::min (1.0, std::max (0.0, u)); const double e = std::exp (-k); return (std::exp (-k * u) - e) / (1.0 - e); };
    switch (c)
    {
        case 0:  return x < 0.03 ? 1.0 : (x >= 0.18 ? 0.0 : expo ((x - 0.03) / 0.15, 4.0));
        case 1:  return x < 0.05 ? 1.0 : (x >= 0.35 ? 0.0 : expo ((x - 0.05) / 0.30, 3.0));
        case 2:  return x < 0.02 ? 1.0 : (x >= 0.62 ? 0.0 : std::pow (1.0 - (x - 0.02) / 0.60, 2.0));
        case 3:  return x >= 0.70 ? 0.0 : 1.0 - x / 0.70;
        case 4:  return x < 0.08 ? 1.0 : x < 0.16 ? 1.0 - 0.5 * ss ((x - 0.08) / 0.08) : x < 0.5 ? 0.5 : x < 0.62 ? 0.5 - 0.5 * ss ((x - 0.5) / 0.12) : 0.0;
        case 5:
        {
            auto dip = [] (double u) { return u < 0.01 ? 1.0 : (u >= 0.33 ? 0.0 : std::pow (1.0 - (u - 0.01) / 0.32, 2.0)); };
            if (x < 0.5) return dip (x);
            if (x < 0.51) return 0.75 * ss ((x - 0.5) / 0.01);
            return 0.75 * dip (x - 0.5);
        }
        case 6:  return x < 0.45 ? 1.0 : 1.0 - ss ((x - 0.45) / 0.05);
        case 7:  return x < 0.10 ? 1.0 : (x >= 0.70 ? 0.0 : 1.0 - ss ((x - 0.10) / 0.60));
        case 8:  return x >= 0.95 ? 0.0 : 0.5 + 0.5 * std::cos (pi * x / 0.95);
        case 9:  return 0.5 + 0.5 * std::cos (2.0 * pi * x);
        case 10: return 1.0 - x;
        case 11: return x < 0.3 ? 0.0 : (x - 0.3) * (x - 0.3) / 0.49;
        default: return 0.0;
    }
}

//==============================================================================
struct FakePlayHead : public juce::AudioPlayHead
{
    bool playing = false, provideBar = true, provideInfo = true;
    double ppq = 0.0, bpm = 120.0;
    int num = 4, den = 4;
    juce::Optional<PositionInfo> getPosition() const override
    {
        if (! provideInfo) return {};
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        p.setBpm (bpm);
        p.setTimeSignature (juce::AudioPlayHead::TimeSignature { num, den });
        if (provideBar)
        {
            const double bar = 4.0 * num / den;
            p.setPpqPositionOfLastBarStart (std::floor (ppq / bar + 1.0e-9) * bar);
        }
        return p;
    }
};

/** Un processeur prêt à jouer, avec sa tête de lecture et son format de bus. */
struct Rig
{
    EbbAudioProcessor p;
    FakePlayHead ph;
    double sr;
    int block, mainCh, scCh;
    int64_t pos = 0;
    bool autoAdvance = true;
    juce::AudioBuffer<float> buf;
    juce::MidiBuffer midi;

    Rig (double sampleRate, int blockSize, int mainChannels = 2, int sidechainChannels = 2)
        : sr (sampleRate), block (blockSize), mainCh (mainChannels), scCh (sidechainChannels)
    {
        juce::AudioProcessor::BusesLayout l;
        const auto main = mainCh == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
        l.inputBuses.add (main);
        l.inputBuses.add (scCh == 0 ? juce::AudioChannelSet::disabled() : (scCh == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo()));
        l.outputBuses.add (main);
        ok = p.setBusesLayout (l);
        p.setPlayHead (&ph);
        p.setRateAndBufferSizeDetails (sr, block);
        p.prepareToPlay (sr, block);
        buf.setSize (mainCh + scCh, juce::jmax (block, 8192));
    }

    void set (const char* pid, float v)
    {
        auto* prm = p.apvts.getParameter (pid);
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
    }

    /** Traite n échantillons : in(ch, i) pour l'entrée principale, sc(ch, i) pour le sidechain (i = position absolue). */
    void run (int n, const std::function<float (int, int64_t)>& in, const std::function<float (int, int64_t)>& sc,
              const std::vector<int>& noteOffsets = {}, float* outMain0 = nullptr, std::vector<float>* outAll = nullptr)
    {
        buf.setSize (mainCh + scCh, n, false, false, true);
        for (int c = 0; c < mainCh; ++c)
            for (int i = 0; i < n; ++i) buf.setSample (c, i, in ? in (c, pos + i) : 0.0f);
        for (int c = 0; c < scCh; ++c)
            for (int i = 0; i < n; ++i) buf.setSample (mainCh + c, i, sc ? sc (c, pos + i) : 0.0f);
        midi.clear();
        for (int o : noteOffsets) midi.addEvent (juce::MidiMessage::noteOn (1, 36, (juce::uint8) 100), o);
        p.processBlock (buf, midi);
        if (outMain0 != nullptr)
            for (int i = 0; i < n; ++i) outMain0[i] = buf.getSample (0, i);
        if (outAll != nullptr)
            for (int c = 0; c < mainCh; ++c)
                for (int i = 0; i < n; ++i) outAll->push_back (buf.getSample (c, i));
        pos += n;
        if (autoAdvance && ph.playing)
        {
            // comme Ableton : position = échantillons écoulés x tempo (pas d'erreur cumulée), recalée à chaque changement
            // de tempo ou de position imposé par le test
            if (ph.bpm != baseBpm || ph.ppq != lastPpq) { basePpq = ph.ppq; basePos = pos - n; baseBpm = ph.bpm; }
            ph.ppq = basePpq + (double) (pos - basePos) * ph.bpm / (60.0 * sr);
            lastPpq = ph.ppq;
        }
    }
    double basePpq = 0.0, baseBpm = -1.0, lastPpq = -1.0;
    int64_t basePos = 0;

    bool ok = false;
};

float dc (int, int64_t) { return 1.0f; }

std::vector<int64_t> triggers (EbbAudioProcessor& p)
{
    auto& e = p.getEngine();
    std::vector<int64_t> v;
    const int64_t n = e.getTriggerCount();
    for (int64_t i = std::max<int64_t> (0, n - Ebb::PumpEngine::logSize); i < n; ++i) v.push_back (e.getTrigger (i));
    return v;
}

void writePng (const juce::Image& img, const juce::File& f);

#include "Phase2Tests.h"

//==============================================================================
void testIds()
{
    section (u8 ("Param\xc3\xa8tres fig\xc3\xa9s (IDs, plages, choix)"));
    EbbAudioProcessor p;
    struct Exp { const char* pid; float lo, hi, def; int choices; };
    const Exp exp[] = { { "amount", 0, 100, 80, 0 }, { "curve", 0, 11, 2, 12 }, { "rate", 0, 6, 4, 7 },
                        { "trigger", 0, 2, 0, 3 }, { "nudge", -20, 20, 0, 0 }, { "threshold", -60, 0, -20, 0 },
                        // phase 2 (ajoutés après, jamais à la place des premiers)
                        { "custom", 0, 1, 0, 0 }, { "tension", -100, 100, 0, 0 }, { "smooth", 0, 100, 0, 0 },
                        { "band", 0, 2, 0, 3 }, { "crossover", 40, 2000, 150, 0 } };
    int bad = 0;
    juce::StringArray seen;
    for (auto& e : exp)
    {
        auto* prm = dynamic_cast<juce::RangedAudioParameter*> (p.apvts.getParameter (e.pid));
        if (prm == nullptr) { ++bad; continue; }
        seen.add (e.pid);
        const auto range = prm->getNormalisableRange();
        const float def = prm->convertFrom0to1 (prm->getDefaultValue());
        if (std::abs (range.start - e.lo) > 1e-4f || std::abs (range.end - e.hi) > 1e-4f || std::abs (def - e.def) > 1e-3f) ++bad;
        if (e.choices > 0)
            if (auto* c = dynamic_cast<juce::AudioParameterChoice*> (prm); c == nullptr || c->choices.size() != e.choices) ++bad;
    }
    // l'ordre des 6 premiers ne bouge pas (projets Ableton et automation)
    const char* order[] = { "amount", "curve", "rate", "trigger", "nudge", "threshold" };
    for (int i = 0; i < 6; ++i)
        if (auto* prm = dynamic_cast<juce::RangedAudioParameter*> (p.getParameters()[i]); prm == nullptr || prm->getParameterID() != order[i]) ++bad;
    check (bad == 0 && p.getParameters().size() == 11, u8 ("11 param\xc3\xa8tres (6 de la phase 1 \xc3\xa0 leur place + 5 ajout\xc3\xa9s), IDs / plages / d\xc3\xa9" "fauts / choix"),
           seen.joinIntoString (", ") + (bad ? " -- " + juce::String (bad) + " ecart(s)" : juce::String()));
}

void testCurves()
{
    section (u8 ("Courbes : tables contre r\xc3\xa9" "f\xc3\xa9rence calcul\xc3\xa9" "e \xc3\xa0 part"));
    const auto& t = Ebb::CurveTables::get();
    double worstAll = 0.0;
    int outOfRange = 0;
    for (int c = 0; c < Ebb::numCurves; ++c)
    {
        double worst = 0.0;
        for (int i = 0; i <= 20000; ++i)
        {
            const double x = i / 20000.0;
            // les marches (gate, rampe) ne sont pas comparées à moins d'un point de table de la discontinuité
            const double ref = refCurve (c, x);
            const double ref2 = refCurve (c, std::min (1.0, x + 1.0 / 2048.0)), ref0 = refCurve (c, std::max (0.0, x - 1.0 / 2048.0));
            const float v = t.lookup (c, (float) x);
            if (v < -1e-6f || v > 1.0f + 1e-6f) ++outOfRange;
            const double err = std::abs (v - ref);
            if (std::abs (ref2 - ref0) > 0.2) continue;
            worst = std::max (worst, err);
        }
        worstAll = std::max (worstAll, worst);
        check (worst < 2.0e-3, juce::String ("Courbe ") + Ebb::curveName (c), u8 ("\xc3\xa9" "cart max ") + fmt (worst, 6));
    }
    check (outOfRange == 0, u8 ("Toutes les valeurs entre 0 et 1"), juce::String (outOfRange) + " hors plage");
}

/** Rendu réel d'une courbe (SYNC, 1 mesure à 60 BPM = 4 s) comparé à : référence + limiteur de vitesse réécrit ici. */
void testShape()
{
    section (u8 ("Forme mesur\xc3\xa9" "e en sortie = r\xc3\xa9" "f\xc3\xa9rence"));
    const double sr = 48000.0;
    for (int c = 0; c < Ebb::numCurves; ++c)
    {
        Rig rig (sr, 512);
        rig.set (id::curve, (float) c);
        rig.set (id::rate, (float) Ebb::rateBar);
        rig.set (id::amount, 100.0f);
        rig.ph.bpm = 60.0;
        rig.ph.playing = true;
        const int cycle = (int) (4.0 * sr);
        std::vector<float> out ((size_t) cycle * 2);
        for (int done = 0; done < cycle * 2; done += 512)
            rig.run (512, dc, nullptr, {}, out.data() + done);

        // référence : 2e cycle, le limiteur part de la valeur de fin du 1er
        const double step = 1.0 / (0.001 * sr);
        double g = out[(size_t) cycle - 1];
        double worst = 0.0;
        for (int i = 0; i < cycle; ++i)
        {
            const double x = (double) i / (double) cycle;
            const double target = 1.0 - refCurve (c, x);
            g += std::max (-step, std::min (step, target - g));
            worst = std::max (worst, std::abs (g - (double) out[(size_t) (cycle + i)]));
        }
        check (worst < 2.0e-3, juce::String ("Rendu ") + Ebb::curveName (c), u8 ("\xc3\xa9" "cart max ") + fmt (worst, 5));
    }
}

//==============================================================================
/** Audio : formats, tailles de bloc, mono / stéréo, automation au hasard. */
void testAudio()
{
    section (u8 ("Audio : 44,1 / 48 / 96 kHz, blocs 1 / 64 / 512 / 2048, mono et st\xc3\xa9r\xc3\xa9o"));
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const int blocks[] = { 1, 64, 512, 2048 };
    long nanInf = 0, denorm = 0, louder = 0, total = 0;
    double worstStepRatio = 0.0;
    std::mt19937 rng (7);
    for (double sr : rates)
        for (int bs : blocks)
            for (int ch : { 1, 2 })
            {
                Rig rig (sr, bs, ch, ch);
                rig.ph.playing = true;
                rig.ph.bpm = 128.0;
                std::uniform_real_distribution<float> uni (0.0f, 1.0f);
                const int64_t len = (int64_t) (3.0 * sr);
                // entrée : bruit + sinus ; sidechain : kicks
                auto in = [&] (int c, int64_t i) { return 0.4f * std::sin ((float) i * 0.013f + (float) c) + 0.3f * (uni (rng) - 0.5f); };
                auto sc = [&] (int, int64_t i) { const double t = std::fmod ((double) i / sr, 0.46875); return (float) (0.6 * std::exp (-t / 0.1) * std::sin (2.0 * 3.14159265 * 60.0 * t)); };
                std::vector<float> outAll;
                outAll.reserve ((size_t) len * (size_t) ch + 4096);
                std::vector<float> ins;
                ins.reserve ((size_t) len * (size_t) ch + 4096);
                int64_t done = 0;
                int nextChange = 0;
                while (done < len)
                {
                    const int n = (int) std::min<int64_t> (bs, len - done);
                    if (done >= nextChange)
                    {
                        nextChange += (int) (0.11 * sr);
                        rig.set (id::amount, uni (rng) * 100.0f);
                        rig.set (id::curve, (float) (int) (uni (rng) * 12.0f));
                        rig.set (id::rate, (float) (int) (uni (rng) * 7.0f));
                        rig.set (id::trigger, (float) (int) (uni (rng) * 3.0f));
                        rig.set (id::nudge, uni (rng) * 40.0f - 20.0f);
                        rig.set (id::threshold, -40.0f + uni (rng) * 30.0f);
                    }
                    std::vector<int> notes;
                    if (uni (rng) < 0.02f * (float) n / 64.0f) notes.push_back ((int) (uni (rng) * (float) (n - 1)));
                    // on garde l'entrée pour comparer
                    const auto before = outAll.size();
                    rig.run (n, [&] (int c, int64_t i) { const float v = in (c, i); ins.push_back (v); return v; }, sc, notes, nullptr, &outAll);
                    juce::ignoreUnused (before);
                    done += n;
                }
                // ins est rempli canal par canal pour chaque bloc, comme outAll
                for (size_t i = 0; i < outAll.size(); ++i)
                {
                    const float o = outAll[i];
                    ++total;
                    if (! std::isfinite (o)) ++nanInf;
                    else if (o != 0.0f && std::abs (o) < std::numeric_limits<float>::min()) ++denorm;
                    if (std::abs (o) > std::abs (ins[i])) ++louder;
                }
                juce::ignoreUnused (worstStepRatio);
            }
    check (nanInf == 0 && denorm == 0, u8 ("Aucun NaN, inf ni d\xc3\xa9normal"), juce::String (total) + u8 (" \xc3\xa9" "chantillons, 24 configurations"));
    check (louder == 0, u8 ("La sortie ne d\xc3\xa9passe jamais l'entr\xc3\xa9" "e"), juce::String (louder) + u8 (" \xc3\xa9" "chantillon(s) plus fort(s)"));
}

/** Amount à 0 % : sortie identique à l'échantillon près, dans les 3 modes. */
void testZero()
{
    section (u8 ("Amount 0 % = signal intact"));
    long diff = 0, total = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int bs : { 1, 64, 512, 2048 })
            for (int mode = 0; mode < 3; ++mode)
            {
                Rig rig (sr, bs);
                rig.ph.playing = true;
                rig.set (id::amount, 0.0f);
                rig.set (id::trigger, (float) mode);
                const int64_t len = (int64_t) (0.5 * sr);
                std::vector<float> out;
                int64_t done = 0;
                while (done < len)
                {
                    const int n = (int) std::min<int64_t> (bs, len - done);
                    std::vector<int> notes;
                    if (bs > 1 && (done / bs) % 7 == 0) notes.push_back (n / 2);
                    if (bs == 1 && done % 9000 == 0) notes.push_back (0);
                    rig.run (n, [] (int c, int64_t i) { return std::sin ((float) i * 0.01f + (float) c) * 0.7f; },
                             [&] (int, int64_t i) { return (i % 22050) < 2000 ? 0.8f * std::sin ((float) i * 0.008f) : 0.0f; }, notes, nullptr, &out);
                    done += n;
                }
                // comparaison bit à bit avec l'entrée
                size_t k = 0;
                for (int64_t b = 0; b < len; b += bs)
                {
                    const int n = (int) std::min<int64_t> (bs, len - b);
                    for (int c = 0; c < 2; ++c)
                        for (int i = 0; i < n; ++i, ++k)
                        {
                            ++total;
                            const float x = std::sin ((float) (b + i) * 0.01f + (float) c) * 0.7f;
                            if (std::memcmp (&x, &out[k], sizeof (float)) != 0) ++diff;
                        }
                }
            }
    check (diff == 0, u8 ("Identique bit \xc3\xa0 bit (SYNC, MIDI, AUDIO ; 12 configurations)"),
           juce::String (total) + u8 (" \xc3\xa9" "chantillons, ") + juce::String (diff) + u8 (" diff\xc3\xa9rent(s)"));
}

/** Aucun saut de gain : entrée continue à 1, la sortie EST le gain. */
void testClicks()
{
    section (u8 ("Anti-clic : variation du gain d'un \xc3\xa9" "chantillon \xc3\xa0 l'autre"));
    std::mt19937 rng (11);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        const double limit = 1.0 / (0.001 * sr);
        double worst = 0.0;
        long events = 0;
        for (int bs : { 1, 64, 512, 2048 })
        {
            Rig rig (sr, bs, 2, 2);
            rig.ph.playing = true;
            rig.ph.bpm = 140.0;
            rig.set (id::amount, 100.0f);
            const int64_t len = (int64_t) (6.0 * sr);
            std::vector<float> out;
            out.reserve ((size_t) len * 2);
            int64_t done = 0;
            int64_t nextEvent = 0;
            while (done < len)
            {
                const int n = (int) std::min<int64_t> (bs, len - done);
                std::vector<int> notes;
                if (done >= nextEvent)
                {
                    ++events;
                    nextEvent += (int64_t) ((0.03 + 0.15 * uni (rng)) * sr);
                    switch ((int) (uni (rng) * 12.0f))
                    {
                        case 8: rig.set (id::tension, uni (rng) * 200.0f - 100.0f); break;                  // Tension
                        case 9: rig.set (id::smooth, uni (rng) * 100.0f); break;                            // Lissage
                        case 10: rig.set (id::custom, uni (rng) < 0.5f ? 0.0f : 1.0f); break;               // courbe perso / usine
                        case 11: { std::mt19937 r2 ((unsigned) events); rig.p.setShape (randomShape (r2, true), false, false); break; }   // nouvelle courbe perso
                        case 0: rig.set (id::curve, (float) (int) (uni (rng) * 12.0f)); break;
                        case 1: rig.set (id::rate, (float) (int) (uni (rng) * 7.0f)); break;
                        case 2: rig.set (id::amount, uni (rng) < 0.5f ? 0.0f : 100.0f); break;   // saut d'Amount
                        case 3: rig.set (id::trigger, (float) (int) (uni (rng) * 3.0f)); break;
                        case 4: rig.ph.playing = ! rig.ph.playing; break;                          // Play / Stop
                        case 5: rig.ph.ppq = std::floor (uni (rng) * 64.0f) * 0.5 + uni (rng); break;  // saut de position
                        case 6: rig.set (id::nudge, uni (rng) * 40.0f - 20.0f); break;
                        default: rig.set (id::trigger, 1.0f); notes.push_back (n > 1 ? (int) (uni (rng) * (float) (n - 1)) : 0); break;  // relance MIDI en plein milieu
                    }
                }
                rig.run (n, dc, [&] (int, int64_t i) { return (i % 20000) < 1500 ? 0.9f * std::sin ((float) i * 0.01f) : 0.0f; }, notes, nullptr, &out);
                done += n;
            }
            // reconstitue le canal 0
            std::vector<float> ch0;
            size_t k = 0;
            for (int64_t b = 0; b < len; b += bs)
            {
                const int n = (int) std::min<int64_t> (bs, len - b);
                for (int i = 0; i < n; ++i) ch0.push_back (out[k + (size_t) i]);
                k += (size_t) n * 2;
            }
            for (size_t i = 1; i < ch0.size(); ++i)
                worst = std::max (worst, (double) std::abs (ch0[i] - ch0[i - 1]));
        }
        check (worst <= limit * 1.0001, juce::String (sr / 1000.0, 1) + u8 (" kHz : saut max du gain"),
               fmt (worst, 5) + " par echantillon (limite " + fmt (limit, 5) + u8 (" = 1 ms pour 0 -> 100 %), ") + juce::String (events)
               + u8 (" changements : courbe, Rate, Amount, mode, Play/Stop, saut de position, Nudge, relance MIDI, Tension, Lissage, courbe perso"));
    }
}

//==============================================================================
/** SYNC : chaque début de courbe à l'échantillon attendu, sans dérive sur 64 mesures. */
void testSync()
{
    section (u8 ("SYNC : d\xc3\xa9" "but de chaque courbe \xc3\xa0 l'\xc3\xa9" "chantillon attendu (64 mesures)"));
    for (double bpm : { 120.0, 128.0, 140.0, 150.0 })
    {
        int worst = 0, missing = 0, extra = 0, checked = 0;
        for (double sr : { 44100.0, 48000.0, 96000.0 })
            for (int rate = 0; rate < Ebb::numRates; ++rate)
            {
                const int bs = rate % 3 == 0 ? 512 : (rate % 3 == 1 ? 333 : 64);
                Rig rig (sr, bs);
                rig.ph.bpm = bpm;
                rig.ph.playing = true;
                rig.set (id::rate, (float) rate);
                const double spq = 60.0 * sr / bpm;
                const int64_t len = (int64_t) std::ceil (64 * 4 * spq) + 2;
                for (int64_t d = 0; d < len; d += bs) rig.run ((int) std::min<int64_t> (bs, len - d), dc, nullptr);

                const double cyc = Ebb::rateBeats (rate, 4.0);
                std::vector<int64_t> expected;
                for (int bar = 0; bar <= 64; ++bar)
                    for (int k = 0; k * cyc < 4.0 - 1e-9; ++k)
                    {
                        const double q = bar * 4.0 + k * cyc;
                        const int64_t s = (int64_t) std::ceil (q * spq - 1e-7 * spq);
                        if (s >= 0 && s < len) expected.push_back (s);
                    }
                const auto got = triggers (rig.p);
                // les journaux gardent les 1024 derniers : on compare sur cette fenêtre
                std::vector<int64_t> exp2;
                for (auto e : expected) if (got.empty() || e >= got.front() - 1) exp2.push_back (e);
                size_t i = 0, j = 0;
                while (i < exp2.size() && j < got.size())
                {
                    const int64_t d = got[j] - exp2[i];
                    if (std::abs (d) <= 2) { worst = std::max (worst, (int) std::abs (d)); ++checked; ++i; ++j; }
                    else if (d < 0) { ++extra; ++j; }
                    else { ++missing; ++i; }
                }
                missing += (int) (exp2.size() - i);
                extra += (int) (got.size() - j);
                if (std::getenv ("EB_DEBUG_SYNC") != nullptr)
                    std::printf ("    sr %.0f rate %d bs %d : attendu %d, obtenu %d, premiers obtenus %lld %lld, derniers %lld / attendu %lld\n", sr, rate, bs,
                                 (int) exp2.size(), (int) got.size(), got.size() > 0 ? (long long) got[0] : -1LL, got.size() > 1 ? (long long) got[1] : -1LL,
                                 got.empty() ? -1LL : (long long) got.back(), exp2.empty() ? -1LL : (long long) exp2.back());
            }
        check (worst == 0 && missing == 0 && extra == 0, juce::String ((int) bpm) + " BPM",
               juce::String (checked) + u8 (" d\xc3\xa9" "buts de courbe (7 dur\xc3\xa9" "es x 3 fr\xc3\xa9quences), \xc3\xa9" "cart max ") + juce::String (worst)
               + u8 (" \xc3\xa9" "chantillon, ") + juce::String (missing) + " manquant(s), " + juce::String (extra) + " en trop");
    }

    // Nudge : +10 ms et -7,5 ms
    for (float nudge : { 10.0f, -7.5f })
    {
        const double sr = 48000.0, bpm = 128.0;
        Rig rig (sr, 256);
        rig.ph.bpm = bpm;
        rig.ph.playing = true;
        rig.set (id::nudge, nudge);
        const double spq = 60.0 * sr / bpm;
        const int64_t len = (int64_t) (16 * 4 * spq);
        for (int64_t d = 0; d < len; d += 256) rig.run (256, dc, nullptr);
        const auto got = triggers (rig.p);
        int worst = 0, count = 0;
        for (auto s : got)
        {
            const double beat = std::round (((double) s - nudge * 0.001 * sr) / spq);
            const int64_t expect = (int64_t) std::ceil (beat * spq + nudge * 0.001 * sr - 1e-7 * spq);
            worst = std::max (worst, (int) std::abs (s - expect));
            ++count;
        }
        check (worst <= 1 && count >= 63, juce::String ("Nudge ") + (nudge > 0 ? "+" : "") + juce::String (nudge, 1) + " ms",
               juce::String (count) + u8 (" temps, \xc3\xa9" "cart max ") + juce::String (worst) + u8 (" \xc3\xa9" "chantillon"));
    }
}

/** Play / Stop, boucle Ableton, changement de tempo, mesure en 3/4, pas de tête de lecture. */
void testTransport()
{
    section (u8 ("Transport : Play / Stop, boucle, tempo, signature"));
    const double sr = 48000.0;

    // --- Play au milieu d'un temps : pas de départ avant le temps suivant, puis tout est calé
    {
        Rig rig (sr, 512);
        rig.ph.bpm = 120.0;           // 24 000 échantillons par noire
        rig.ph.ppq = 8.5;
        rig.ph.playing = true;
        for (int b = 0; b < 200; ++b) rig.run (512, dc, nullptr);
        const auto got = triggers (rig.p);
        const bool ok = ! got.empty() && got.front() == 12000 && got.size() > 1 && got[1] == 36000;
        check (ok, u8 ("Play au milieu d'un temps"), u8 ("premier d\xc3\xa9" "part \xc3\xa0 l'\xc3\xa9" "chantillon ") + (got.empty() ? juce::String ("-") : juce::String (got.front())) + " (attendu 12000), puis 36000");
    }

    // --- Stop : la courbe continue au même tempo, sans saut de phase
    {
        Rig rig (sr, 500);
        rig.ph.bpm = 120.0;
        rig.ph.playing = true;
        for (int b = 0; b < 100; ++b) rig.run (500, dc, nullptr);     // 50 000 échantillons = 2,083 noires
        rig.ph.playing = false;
        for (int b = 0; b < 300; ++b) rig.run (500, dc, nullptr);
        const auto got = triggers (rig.p);
        // départs attendus : 24000, 48000 (en lecture), puis en roue libre : 72000, 96000...
        int worst = 0;
        int n = 0;
        for (auto s : got) { const int64_t e = (int64_t) std::llround ((double) s / 24000.0) * 24000; worst = std::max (worst, (int) std::abs (s - e)); ++n; }
        check (worst <= 1 && n >= 6, u8 ("Stop : continue au dernier tempo"), juce::String (n) + u8 (" d\xc3\xa9" "parts, \xc3\xa9" "cart max ") + juce::String (worst) + u8 (" \xc3\xa9" "chantillon"));
    }

    // --- Boucle Ableton : mesures 2 à 4 (ppq 4 -> 12), retour au début de la boucle entre deux blocs
    {
        Rig rig (sr, 480);
        rig.autoAdvance = false;
        rig.ph.bpm = 120.0;
        rig.ph.playing = true;
        double ppq = 4.0;
        std::vector<int64_t> expectedStarts;
        for (int b = 0; b < 1500; ++b)
        {
            rig.ph.ppq = ppq;
            const int64_t blockStart = rig.pos;
            // un début de temps tombe dans ce bloc ? (premier échantillon dont la position atteint un temps entier)
            const double qps = 120.0 / 60.0 / sr;
            for (int i = 0; i < 480; ++i)
            {
                const double q = ppq + i * qps + 1e-7;
                if (q - std::floor (q) < qps) expectedStarts.push_back (blockStart + i);
            }
            rig.run (480, dc, nullptr);
            ppq += 480.0 * 120.0 / 60.0 / sr;
            if (ppq >= 12.0 - 1e-9) ppq = 4.0 + (ppq - 12.0);
        }
        const auto got = triggers (rig.p);
        std::vector<int64_t> exp2;
        for (auto e : expectedStarts) if (e >= got.front()) exp2.push_back (e);
        int mism = 0;
        for (size_t i = 0; i < std::min (exp2.size(), got.size()); ++i) if (exp2[i] != got[i]) ++mism;
        check (mism == 0 && exp2.size() == got.size(), u8 ("Boucle de 2 mesures (ppq 4 \xe2\x86\x92 12)"),
               juce::String ((int) got.size()) + u8 (" d\xc3\xa9" "parts compar\xc3\xa9s, ") + juce::String (mism) + u8 (" d\xc3\xa9" "cal\xc3\xa9(s)"));
    }

    // --- Changement de tempo : 120 BPM pendant 8 mesures puis 140
    {
        Rig rig (sr, 512);
        rig.ph.bpm = 120.0;
        rig.ph.playing = true;
        const int64_t change = 8 * 4 * 24000;
        while (rig.pos < change) rig.run ((int) std::min<int64_t> (512, change - rig.pos), dc, nullptr);
        rig.ph.bpm = 140.0;
        const double spq2 = 60.0 * sr / 140.0;
        const int64_t end = change + (int64_t) (16 * 4 * spq2);
        while (rig.pos < end) rig.run ((int) std::min<int64_t> (512, end - rig.pos), dc, nullptr);
        const auto got = triggers (rig.p);
        int worst = 0, n = 0;
        for (auto s : got)
            if (s >= change)
            {
                const double beat = std::round ((double) (s - change) / spq2);
                const int64_t e = change + (int64_t) std::ceil (beat * spq2 - 1e-7 * spq2);
                worst = std::max (worst, (int) std::abs (s - e));
                ++n;
            }
        check (worst == 0 && n >= 63, u8 ("Tempo 120 \xe2\x86\x92 140 BPM"), juce::String (n) + u8 (" temps apr\xc3\xa8s le changement, \xc3\xa9" "cart max ") + juce::String (worst));
    }

    // --- Mesure à 3/4 : « 1 mesure » = 3 noires ; sans position de mesure fournie par l'hôte
    {
        Rig rig (sr, 512);
        rig.ph.bpm = 120.0;
        rig.ph.num = 3;
        rig.ph.provideBar = false;
        rig.ph.playing = true;
        rig.set (id::rate, (float) Ebb::rateBar);
        for (int b = 0; b < 1000; ++b) rig.run (512, dc, nullptr);
        const auto got = triggers (rig.p);
        int bad = 0;
        for (auto s : got) if (s % 72000 != 0) ++bad;
        check (bad == 0 && got.size() >= 6, u8 ("3/4 : une courbe par mesure de 3 temps"), juce::String ((int) got.size()) + u8 (" d\xc3\xa9" "parts, ") + juce::String (bad) + u8 (" d\xc3\xa9" "cal\xc3\xa9(s)"));
    }

    // --- Aucune tête de lecture (certains hôtes) : roue libre à 120 BPM
    {
        Rig rig (sr, 512);
        rig.ph.provideInfo = false;
        for (int b = 0; b < 400; ++b) rig.run (512, dc, nullptr);
        const auto got = triggers (rig.p);
        int bad = 0;
        for (auto s : got) if (std::abs (s - (int64_t) std::llround ((double) s / 24000.0) * 24000) > 1) ++bad;
        check (bad == 0 && got.size() >= 7, u8 ("Sans tête de lecture : roue libre \xc3\xa0 120 BPM"), juce::String ((int) got.size()) + u8 (" d\xc3\xa9" "parts, ") + juce::String (bad) + u8 (" d\xc3\xa9" "cal\xc3\xa9(s)"));
    }
}

//==============================================================================
/** MIDI : la courbe démarre à l'échantillon exact de chaque note. */
void testMidi()
{
    section (u8 ("MIDI : d\xc3\xa9" "part \xc3\xa0 l'\xc3\xa9" "chantillon exact de la note"));
    std::mt19937 rng (3);
    int worst = 0, notes = 0, missing = 0, noDrop = 0;
    for (double sr : { 44100.0, 48000.0, 96000.0 })
        for (int bs : { 1, 64, 512, 2048 })
        {
            Rig rig (sr, bs);
            rig.set (id::trigger, (float) Ebb::triggerMidi);
            rig.set (id::amount, 100.0f);
            rig.set (id::rate, (float) Ebb::rate16th);
            rig.ph.playing = true;
            std::vector<int64_t> sent;
            std::vector<float> ch0;
            const int64_t len = (int64_t) (2.0 * sr);
            std::uniform_real_distribution<float> uni (0.0f, 1.0f);
            int64_t nextNote = (int64_t) (0.01 * sr);
            while (rig.pos < len)
            {
                const int n = (int) std::min<int64_t> (bs, len - rig.pos);
                std::vector<int> offs;
                while (nextNote < rig.pos + n)
                {
                    offs.push_back ((int) (nextNote - rig.pos));
                    sent.push_back (nextNote);
                    nextNote += (int64_t) ((0.12 + 0.2 * uni (rng)) * sr);
                }
                std::vector<float> o ((size_t) n);
                rig.run (n, dc, nullptr, offs, o.data());
                ch0.insert (ch0.end(), o.begin(), o.end());
            }
            const auto got = triggers (rig.p);
            if (got.size() != sent.size()) missing += std::abs ((int) sent.size() - (int) got.size());
            for (size_t i = 0; i < std::min (got.size(), sent.size()); ++i)
            {
                worst = std::max (worst, (int) std::abs (got[i] - sent[i]));
                ++notes;
                // le gain commence à baisser exactement sur la note (il était remonté à 1 avant)
                const auto s = (size_t) sent[i];
                if (s >= 1 && s < ch0.size() && ! (ch0[s] < ch0[s - 1] && ch0[s - 1] > 0.999f)) ++noDrop;
            }
        }
    check (worst == 0 && missing == 0, u8 ("D\xc3\xa9" "part de courbe = note"), juce::String (notes) + u8 (" notes (12 configurations), \xc3\xa9" "cart max ") + juce::String (worst) + u8 (" \xc3\xa9" "chantillon, ") + juce::String (missing) + " manquante(s)");
    check (noDrop == 0, u8 ("Le volume baisse d\xc3\xa8s l'\xc3\xa9" "chantillon de la note"), juce::String (noDrop) + u8 (" note(s) sans baisse imm\xc3\xa9" "diate"));
}

//==============================================================================
// Signaux de test pour le mode AUDIO
struct KickSpec { double amp = 0.5, tau = 0.12; };
float kickAt (double t, const KickSpec& k)
{
    if (t < 0.0) return 0.0f;
    const double env = (1.0 - std::exp (-t / 0.0005)) * std::exp (-t / k.tau);
    const double phi = 2.0 * 3.14159265358979 * (48.0 * t + 112.0 * 0.03 * (1.0 - std::exp (-t / 0.03)));
    return (float) (k.amp * env * std::sin (phi));
}

struct Noise { uint32_t s = 12345; float next() { s = s * 1664525u + 1013904223u; return (float) ((s >> 8) & 0xffff) / 32768.0f - 1.0f; } };

/** Mode AUDIO : délai de réaction, comptage exact, rien sur une basse tenue ou un charley. */
void testKick()
{
    section (u8 ("AUDIO : d\xc3\xa9" "clenchement par le kick du sidechain"));
    struct Case { const char* name; double bpm, sr; KickSpec k; float thr; bool hats; int scCh; };
    const Case cases[] = {
        { "Kick 128 BPM, -6 dB, 48 kHz",                 128.0, 48000.0, { 0.5, 0.12 }, -20.0f, false, 2 },
        { "Kick long (909) 140 BPM, 44,1 kHz",           140.0, 44100.0, { 0.7, 0.35 }, -20.0f, false, 2 },
        { "Kick + charleys 150 BPM, 96 kHz, mono",       150.0, 96000.0, { 0.5, 0.10 }, -20.0f, true,  1 },
        { "Kick faible (-18 dB), seuil -30 dB, 48 kHz",  124.0, 48000.0, { 0.125, 0.12 }, -30.0f, false, 2 },
        { "Kick avec longue queue (encore -6 dB au kick suivant), 128 BPM", 128.0, 48000.0, { 0.9, 0.60 }, -20.0f, false, 2 },
        { "Kick 174 BPM (drum & bass), 48 kHz",          174.0, 48000.0, { 0.5, 0.08 }, -20.0f, false, 2 },
    };
    for (auto& cs : cases)
    {
        Rig rig (cs.sr, 512, 2, cs.scCh);
        rig.set (id::trigger, (float) Ebb::triggerAudio);
        rig.set (id::threshold, cs.thr);
        rig.ph.bpm = cs.bpm;
        rig.ph.playing = true;
        const double beat = 60.0 / cs.bpm;
        const int numKicks = 64;
        const int64_t first = (int64_t) (0.05 * cs.sr);
        std::vector<int64_t> onsets;
        for (int k = 0; k < numKicks; ++k) onsets.push_back (first + (int64_t) std::llround (k * beat * cs.sr));
        Noise noise;
        float hp1 = 0, hp2 = 0;
        const int64_t len = onsets.back() + (int64_t) (0.5 * cs.sr);
        auto sc = [&] (int c, int64_t i)
        {
            const int64_t k = std::min<int64_t> (numKicks - 1, std::max<int64_t> (0, (int64_t) std::floor ((double) (i - first) / (beat * cs.sr) + 1e-9)));
            float v = kickAt ((double) (i - onsets[(size_t) k]) / cs.sr, cs.k) + (k > 0 ? kickAt ((double) (i - onsets[(size_t) k - 1]) / cs.sr, cs.k) : 0.0f);
            if (cs.hats && c == 0)
            {
                const double tt = std::fmod ((double) i / cs.sr, beat / 4.0);
                const float n = noise.next();
                const float h = n - hp1; hp1 = n; const float h2 = h - hp2; hp2 = h;
                v += 0.5f * h2 * (float) std::exp (-tt / 0.03);
            }
            return v;
        };
        for (int64_t d = 0; d < len; d += 512) rig.run ((int) std::min<int64_t> (512, len - d), dc, sc);
        const auto got = triggers (rig.p);
        double worstMs = 0.0, minMs = 1e9;
        int matched = 0;
        for (size_t i = 0; i < std::min (got.size(), onsets.size()); ++i)
        {
            const double ms = (double) (got[i] - onsets[i]) * 1000.0 / cs.sr;
            worstMs = std::max (worstMs, ms);
            minMs = std::min (minMs, ms);
            ++matched;
        }
        const bool ok = (int) got.size() == numKicks && worstMs < 3.0 && minMs >= 0.0;
        check (ok, cs.name, juce::String ((int) got.size()) + " / " + juce::String (numKicks) + u8 (" d\xc3\xa9" "clenchements, d\xc3\xa9lai ")
                                + fmt (minMs, 2) + " - " + fmt (worstMs, 2) + " ms (objectif < 3 ms)");
    }

    // --- Basse tenue et liée, sub-basse (sans kick) : aucun déclenchement une fois la note lancée
    //     (le début d'un son après un silence est une attaque : il compte, on le signale à part)
    struct BassCase { const char* name; double notes[6]; double amp, h2; };
    const BassCase basses[] = {
        { "Basse tenue et li\xc3\xa9" "e (55-82 Hz)",   { 55.0, 65.41, 73.42, 49.0, 58.27, 82.41 }, 0.5, 0.3 },
        { "Sub-basse tenue (31-33 Hz)",                   { 30.87, 32.7, 30.87, 32.7, 30.87, 32.7 }, 0.7, 0.0 },
    };
    for (auto& bc : basses)
    {
        const double sr = 48000.0;
        Rig rig (sr, 512);
        rig.set (id::trigger, (float) Ebb::triggerAudio);
        rig.set (id::threshold, -40.0f);
        rig.ph.playing = true;
        // signal calculé d'avance (phase continue d'une note à l'autre : notes liées), le même sur les deux canaux
        std::vector<float> sig ((size_t) (8.0 * sr) + 1024);
        double ph = 0.0;
        for (size_t i = 0; i < sig.size(); ++i)
        {
            const double t = (double) i / sr;
            ph += 2.0 * 3.14159265358979 * bc.notes[(size_t) ((int64_t) (t / 0.234) % 6)] / sr;
            sig[i] = (float) (bc.amp * std::min (1.0, t / 0.3) * (std::sin (ph) + bc.h2 * std::sin (2.0 * ph)));
        }
        auto bass = [&] (int, int64_t i) { return sig[(size_t) i]; };
        for (int b = 0; b < (int) (8.0 * sr / 512); ++b) rig.run (512, dc, bass);
        int atStart = 0, later = 0;
        for (auto s : triggers (rig.p)) (s < (int64_t) (0.5 * sr) ? atStart : later) += 1;
        check (later == 0, juce::String (u8 (bc.name)) + u8 (" seule, 8 s, seuil -40 dB"),
               juce::String (later) + u8 (" d\xc3\xa9" "clenchement(s) pendant la note ; ") + juce::String (atStart)
               + u8 (" au tout d\xc3\xa9" "but (le son d\xc3\xa9marre apr\xc3\xa8s un silence : c'est une attaque)"));
    }

    // --- Charleys seuls (doubles croches, -6 dB) : aucun déclenchement
    {
        const double sr = 48000.0;
        Rig rig (sr, 512);
        rig.set (id::trigger, (float) Ebb::triggerAudio);
        rig.set (id::threshold, -40.0f);
        rig.ph.playing = true;
        Noise noise;
        float hp1 = 0, hp2 = 0;
        auto hats = [&] (int, int64_t i)
        {
            const double tt = std::fmod ((double) i / sr, 60.0 / 128.0 / 4.0);
            const float n = noise.next();
            const float h = n - hp1; hp1 = n; const float h2 = h - hp2; hp2 = h;
            return 0.5f * h2 * (float) std::exp (-tt / 0.03);
        };
        for (int b = 0; b < (int) (8.0 * sr / 512); ++b) rig.run (512, dc, hats);
        check (rig.p.getEngine().getTriggerCount() == 0, u8 ("Charleys seuls (8 s, -6 dB, seuil -40 dB)"),
               juce::String (rig.p.getEngine().getTriggerCount()) + u8 (" d\xc3\xa9" "clenchement(s)"));
    }
}

/** Sidechain absent, mono ou stéréo ; entrée principale mono ou stéréo. */
void testSidechainLayouts()
{
    section (u8 ("Formats de bus : sidechain absent / mono / st\xc3\xa9r\xc3\xa9o"));
    int okCount = 0, total = 0;
    juce::StringArray details;
    for (int mainCh : { 1, 2 })
        for (int scCh : { 0, 1, 2 })
        {
            ++total;
            Rig rig (48000.0, 512, mainCh, scCh);
            rig.set (id::trigger, (float) Ebb::triggerAudio);
            rig.ph.playing = true;
            KickSpec k;
            auto sc = [&] (int, int64_t i) { return kickAt ((double) (i % 24000) / 48000.0, k); };
            std::vector<float> out;
            for (int b = 0; b < 200; ++b) rig.run (512, dc, sc, {}, nullptr, &out);
            const auto n = rig.p.getEngine().getTriggerCount();
            bool finite = true;
            for (float v : out) finite = finite && std::isfinite (v) && v <= 1.0f && v >= 0.0f;
            const bool expectTriggers = scCh > 0;
            const bool ok = rig.ok && finite && (expectTriggers ? n >= 4 : n == 0) && rig.p.isSidechainConnected() == (scCh > 0);
            if (ok) ++okCount;
            details.add (juce::String (mainCh == 1 ? "mono" : "stereo") + "/" + (scCh == 0 ? "sans" : scCh == 1 ? "mono" : "stereo") + "=" + juce::String (n));
        }
    check (okCount == total, u8 ("6 formats accept\xc3\xa9s, d\xc3\xa9" "clenchements seulement avec sidechain"), details.joinIntoString (", "));

    EbbAudioProcessor p;
    juce::AudioProcessor::BusesLayout bad;
    bad.inputBuses.add (juce::AudioChannelSet::stereo());
    bad.inputBuses.add (juce::AudioChannelSet::disabled());
    bad.outputBuses.add (juce::AudioChannelSet::mono());
    juce::AudioProcessor::BusesLayout surround;
    surround.inputBuses.add (juce::AudioChannelSet::create5point1());
    surround.inputBuses.add (juce::AudioChannelSet::stereo());
    surround.outputBuses.add (juce::AudioChannelSet::create5point1());
    check (! p.checkBusesLayoutSupported (bad) && ! p.checkBusesLayoutSupported (surround), u8 ("Formats refus\xc3\xa9s (entr\xc3\xa9" "e st\xc3\xa9r\xc3\xa9o / sortie mono, 5.1)"), "OK");
}

//==============================================================================
void testState()
{
    section (u8 ("Sauvegarde / rechargement"));
    std::mt19937 rng (5);
    int bad = 0;
    for (int trial = 0; trial < 50; ++trial)
    {
        EbbAudioProcessor a, b;
        std::uniform_real_distribution<float> uni (0.0f, 1.0f);
        for (auto* prm : a.getParameters())
            prm->setValueNotifyingHost (uni (rng));
        juce::MemoryBlock mb;
        a.getStateInformation (mb);
        b.setStateInformation (mb.getData(), (int) mb.getSize());
        auto legal = [] (juce::AudioProcessorParameter* q)
        {
            auto* rq = dynamic_cast<juce::RangedAudioParameter*> (q);
            return rq != nullptr ? rq->convertTo0to1 (rq->convertFrom0to1 (q->getValue())) : q->getValue();
        };
        for (int i = 0; i < a.getParameters().size(); ++i)
            if (std::abs (legal (a.getParameters()[i]) - legal (b.getParameters()[i])) > 1.0e-6f) ++bad;
    }
    check (bad == 0, u8 ("50 \xc3\xa9tats au hasard, 11 param\xc3\xa8tres"), juce::String (bad) + u8 (" diff\xc3\xa9rence(s)"));
}

//==============================================================================
void testPerformance()
{
    section ("Performance (bloc de 512 a 48 kHz, stereo, mode AUDIO avec kicks)");
    auto measure = [] (bool everything, double& mean, double& p99, double& worst)
    {
        Rig rig (48000.0, 512);
        rig.set (id::trigger, (float) Ebb::triggerAudio);
        if (everything)
        {
            // tout allumé : courbe perso, Tension, Lissage, multibande
            std::mt19937 rng (3);
            rig.p.setShape (randomShape (rng, false), false, true);
            rig.set (id::tension, 40.0f);
            rig.set (id::smooth, 30.0f);
            rig.set (id::band, 1.0f);
        }
        rig.ph.playing = true;
        KickSpec k;
        juce::AudioBuffer<float> buf (4, 512);
        juce::MidiBuffer midi;
        std::vector<double> us;
        int64_t pos = 0;
        for (int b = 0; b < 20000; ++b)
        {
            for (int i = 0; i < 512; ++i)
            {
                const float x = 0.3f * std::sin ((float) (pos + i) * 0.01f);
                buf.setSample (0, i, x); buf.setSample (1, i, x);
                const float kk = kickAt ((double) ((pos + i) % 22500) / 48000.0, k);
                buf.setSample (2, i, kk); buf.setSample (3, i, kk);
            }
            const auto t0 = std::chrono::steady_clock::now();
            rig.p.processBlock (buf, midi);
            const auto t1 = std::chrono::steady_clock::now();
            us.push_back (std::chrono::duration<double, std::micro> (t1 - t0).count());
            pos += 512;
            rig.ph.ppq += 512.0 * 120.0 / 60.0 / 48000.0;
        }
        std::vector<double> sorted (us.begin() + 1000, us.end());
        std::sort (sorted.begin(), sorted.end());
        mean = 0.0;
        for (double v : sorted) mean += v;
        mean /= (double) sorted.size();
        p99 = sorted[(size_t) (sorted.size() * 0.99)];
        worst = sorted.back();
    };
    const double blockUs = 512.0 / 48000.0 * 1.0e6;
    for (bool everything : { false, true })
    {
        double mean, p99, worst;
        measure (everything, mean, p99, worst);
        const juce::String what = everything ? u8 ("tout allum\xc3\xa9 (courbe perso, Tension, Lissage, GRAVES)") : juce::String ("courbe d'usine, pleine bande");
        check (mean / blockUs * 100.0 < 0.3, u8 ("CPU moyen, ") + what, fmt (mean, 2) + u8 (" \xc2\xb5s par bloc = ") + fmt (mean / blockUs * 100.0, 3) + u8 (" % d'un c\xc5\x93ur (objectif < 0,3 %)"));
        check (true, u8 ("99e centile / pire bloc, ") + what, fmt (p99, 2) + u8 (" \xc2\xb5s / ") + fmt (worst, 1) + u8 (" \xc2\xb5s (le pire inclut les interruptions de la machine de test)"));
    }
}

void testAlloc()
{
    section (u8 ("Allocations dans processBlock"));
    Rig rig (48000.0, 512);
    rig.ph.playing = true;
    KickSpec k;
    std::mt19937 rng (9);
    std::uniform_real_distribution<float> uni (0.0f, 1.0f);
    juce::AudioBuffer<float> buf (4, 512);
    juce::MidiBuffer midi;
    midi.ensureSize (4096);
    long allocs = 0;
    for (int b = 0; b < 3000; ++b)
    {
        for (int i = 0; i < 512; ++i)
        {
            buf.setSample (0, i, 0.2f); buf.setSample (1, i, -0.2f);
            const float kk = kickAt ((double) ((b * 512 + i) % 22500) / 48000.0, k);
            buf.setSample (2, i, kk); buf.setSample (3, i, kk);
        }
        midi.clear();
        if (b % 3 == 0) midi.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 90), (int) (uni (rng) * 511.0f));
        if (b % 50 == 0) rig.set (id::trigger, (float) ((b / 50) % 3));
        if (b % 70 == 0) rig.set (id::curve, (float) ((b / 70) % 12));
        if (b % 90 == 0) rig.set (id::amount, uni (rng) * 100.0f);
        if (b % 40 == 0) rig.set (id::band, (float) ((b / 40) % 3));
        if (b % 33 == 0) rig.set (id::crossover, 40.0f + uni (rng) * 1900.0f);
        if (b % 25 == 0) rig.set (id::tension, uni (rng) * 200.0f - 100.0f);
        if (b % 29 == 0) rig.set (id::smooth, uni (rng) * 100.0f);
        if (b % 60 == 0) rig.set (id::custom, (float) ((b / 60) % 2));
        if (b % 45 == 0) rig.p.setShape (randomShape (rng, true), false, false);   // l'interface publie une nouvelle courbe (hors comptage)
        const bool bypass = (b % 400) > 380;
        AllocCounter::count = 0;
        AllocCounter::counting = true;
        if (bypass) rig.p.processBlockBypassed (buf, midi);
        else        rig.p.processBlock (buf, midi);
        AllocCounter::counting = false;
        allocs += AllocCounter::count.load();
        rig.ph.ppq += 512.0 * 120.0 / 60.0 / 48000.0;
        if (b % 300 == 299) rig.ph.playing = ! rig.ph.playing;
    }
    check (allocs == 0, u8 ("3 000 blocs (3 modes, MIDI, courbes, courbe perso, Tension, Lissage, bandes, Amount, bypass, Play/Stop)"), juce::String (allocs) + " allocation(s)");
}

//==============================================================================
void writePng (const juce::Image& img, const juce::File& f)
{
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat().writeImageToStream (img, os);
}

/** Contexte graphique qui ne dessine rien : isole les allocations de notre code (texte, chemins, chaînes)
    de celles du moteur de rendu (tables de bords, dégradés). */
struct NullContext : public juce::LowLevelGraphicsContext
{
    juce::Font font { juce::FontOptions{} };
    juce::Rectangle<int> clip { 0, 0, 4000, 4000 };
    bool isVectorDevice() const override { return false; }
    void setOrigin (juce::Point<int>) override {}
    void addTransform (const juce::AffineTransform&) override {}
    float getPhysicalPixelScaleFactor() const override { return 1.0f; }
    bool clipToRectangle (const juce::Rectangle<int>&) override { return true; }
    bool clipToRectangleList (const juce::RectangleList<int>&) override { return true; }
    void excludeClipRectangle (const juce::Rectangle<int>&) override {}
    void clipToPath (const juce::Path&, const juce::AffineTransform&) override {}
    void clipToImageAlpha (const juce::Image&, const juce::AffineTransform&) override {}
    bool clipRegionIntersects (const juce::Rectangle<int>&) override { return true; }
    juce::Rectangle<int> getClipBounds() const override { return clip; }
    bool isClipEmpty() const override { return false; }
    void saveState() override {}
    void restoreState() override {}
    void beginTransparencyLayer (float) override {}
    void endTransparencyLayer() override {}
    void setFill (const juce::FillType&) override {}
    void setOpacity (float) override {}
    void setInterpolationQuality (juce::Graphics::ResamplingQuality) override {}
    void fillRect (const juce::Rectangle<int>&, bool) override {}
    void fillRect (const juce::Rectangle<float>&) override {}
    void fillRectList (const juce::RectangleList<float>&) override {}
    void fillPath (const juce::Path&, const juce::AffineTransform&) override {}
    void drawRect (const juce::Rectangle<float>&, float) override {}
    void strokePath (const juce::Path&, const juce::PathStrokeType&, const juce::AffineTransform&) override {}
    void drawImage (const juce::Image&, const juce::AffineTransform&) override {}
    void drawLine (const juce::Line<float>&) override {}
    void drawLineWithThickness (const juce::Line<float>&, float) override {}
    void setFont (const juce::Font& f) override { font = f; }
    const juce::Font& getFont() override { return font; }
    void drawGlyphs (juce::Span<const uint16_t>, juce::Span<const juce::Point<float>>, const juce::AffineTransform&) override {}
    std::unique_ptr<juce::ImageType> getPreferredImageTypeForTemporaryImages() const override { return std::make_unique<juce::SoftwareImageType>(); }
    void drawRoundedRectangle (const juce::Rectangle<float>&, float, float) override {}
    void fillRoundedRectangle (const juce::Rectangle<float>&, float) override {}
    void drawEllipse (const juce::Rectangle<float>&, float) override {}
    void fillEllipse (const juce::Rectangle<float>&) override {}
    uint64_t getFrameId() const override { return 0; }
};

/** Dessine un composant et ses enfants comme le ferait l'hôte entre deux rafraîchissements du fond :
    les composants mis en cache (setBufferedToImage) ne sont pas redessinés, seule leur image l'est. */
void paintLive (juce::Component& c, juce::Graphics& g)
{
    if (! c.isVisible() || c.getCachedComponentImage() != nullptr) return;
    c.paint (g);
    for (auto* child : c.getChildren())
    {
        juce::Graphics::ScopedSaveState save (g);
        g.setOrigin (child->getPosition());
        paintLive (*child, g);
    }
    c.paintOverChildren (g);
}

/** Captures de l'interface (1x et 2x), temps de dessin, allocations pendant le dessin, activité au repos. */
void snapshots (const juce::File& dir)
{
    section (u8 ("Interface provisoire : captures, dessin, repos"));
    dir.createDirectory();
    EbbAudioProcessor p;
    p.setRateAndBufferSizeDetails (48000.0, 512);
    p.prepareToPlay (48000.0, 512);
    FakePlayHead ph;
    ph.playing = true;
    ph.bpm = 128.0;
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
    std::unique_ptr<EbbAudioProcessorEditor> ed (dynamic_cast<EbbAudioProcessorEditor*> (p.createEditor()));
    ed->setSize (EbbContent::designW, EbbContent::designH);
    pump (30);
    for (int i = 0; i < 10; ++i) ed->tickForTests();
    for (float scale : { 1.0f, 2.0f })
    {
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
        writePng (img, dir.getChildFile (juce::String ("ebb_provisoire@") + juce::String ((int) scale) + "x.png"));
    }
    check (true, "Captures", dir.getChildFile ("ebb_provisoire@1x.png").getFullPathName() + " et @2x");

    // panneau « À propos » : les mentions qu'une interface doit afficher sous la licence AGPL
    {
        auto& ct = ed->getContent();
        ct.getInfoButton().triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        auto* about = ct.getAboutPanel();
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        writePng (img, dir.getChildFile ("about_2x.png"));
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

    // temps de dessin (même méthode que Confluence) :
    //   image pleine : fond recalculé + toute la fenêtre (8 fois par seconde)
    //   image courante : la courbe, l'eau et la goutte, le logo (les autres images)
    auto& content = ed->getContent();
    juce::RectangleList<int> partial;
    partial.add (content.getCurveView().getBounds());
    partial.add (juce::Rectangle<int> (14, 10, 36, 26));
    auto measure = [&] (bool full, float scale, double& meanMs, double& p95)
    {
        juce::Image target (juce::Image::ARGB, juce::roundToInt ((float) ed->getWidth() * scale), juce::roundToInt ((float) ed->getHeight() * scale), true);
        std::vector<double> times;
        for (int i = 0; i < 70; ++i)
        {
            pump (2);
            ed->tickForTests();
            if (full) content.invalidateBackground();
            juce::Graphics g (target);
            g.addTransform (juce::AffineTransform::scale (scale));
            if (! full) g.reduceClipRegion (partial);
            const auto t0 = std::chrono::steady_clock::now();
            ed->paintEntireComponent (g, true);
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
            if (i >= 10) times.push_back (ms);
        }
        std::sort (times.begin(), times.end());
        meanMs = 0; for (auto t : times) meanMs += t; meanMs /= (double) times.size();
        p95 = times[(size_t) (times.size() * 0.95)];
    };
    for (float scale : { 1.0f, 2.0f })
    {
        double fm, fp, pm, pp;
        measure (true, scale, fm, fp);
        measure (false, scale, pm, pp);
        const juce::String sc = juce::String ((int) scale) + "x";
        // objectif vérifié à 1x comme pour Confluence ; à 2x le rendu logiciel de Linux est donné pour information
        // (sur Mac, les images passent par CoreGraphics)
        const bool judged = scale < 1.5f;
        const juce::String info = judged ? juce::String (" (objectif < 16 ms)") : u8 (" (information : rendu logiciel Linux, 8 images pleines par seconde au plus)");
        check (! judged || fp < 16.0, u8 ("Image pleine \xc3\xa0 ") + sc + u8 (" (fond recalcul\xc3\xa9)"),
               "moyenne " + fmt (fm) + " ms, 95e centile " + fmt (fp) + " ms" + info);
        check (! judged || pp < 16.0, u8 ("Image courante \xc3\xa0 ") + sc + u8 (" (vague, eau, goutte, logo)"),
               "moyenne " + fmt (pm) + " ms, 95e centile " + fmt (pp) + " ms" + info);
    }

    // notre code seul (contexte qui ne dessine rien) pendant l'animation : aucune allocation.
    // Les allocations du moteur de rendu de JUCE (tables de bords, dégradés) ne sont pas comptées.
    {
        NullContext nc;
        juce::Image target (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
        long allocs = 0;
        int frames = 0;
        for (int i = 0; i < 100; ++i)
        {
            pump (2);
            ed->tickForTests();
            if (content.isBackgroundDirty()) { juce::Graphics g (target); content.paint (g); }
            juce::Graphics g (nc);
            AllocCounter::count = 0;
            AllocCounter::counting = i >= 20;
            paintLive (content, g);
            AllocCounter::counting = false;
            if (i >= 20) { allocs += AllocCounter::count.load(); ++frames; }
        }
        check (allocs == 0, u8 ("Aucune allocation dans paint() pendant l'animation (notre code)"),
               juce::String (allocs) + " allocation(s) sur " + juce::String (frames) + " images");
    }

    // au repos (mode AUDIO, aucun kick, souris dehors) : l'interface ne redessine presque rien
    p.apvts.getParameter (id::trigger)->setValueNotifyingHost (1.0f);   // AUDIO
    ph.playing = false;
    pump (200);
    for (int i = 0; i < 60; ++i) { pump (1); content.tick(); }
    int activeTicks = 0;
    for (int i = 0; i < 120; ++i) { pump (1); if (content.tick()) ++activeTicks; }
    check (activeTicks == 0, u8 ("Au repos (AUDIO sans kick)"), juce::String (activeTicks) + u8 (" image(s) active(s) sur 120 (fond rafra\xc3\xae" "chi 4 fois/s seulement)"));
}

/** Profil du dessin à 2x : fond, courbe, boutons (pour savoir où passe le temps). */
void uiProfile()
{
    EbbAudioProcessor p;
    p.prepareToPlay (48000.0, 512);
    std::unique_ptr<EbbAudioProcessorEditor> ed (dynamic_cast<EbbAudioProcessorEditor*> (p.createEditor()));
    auto& content = ed->getContent();
    for (int i = 0; i < 5; ++i) ed->tickForTests();
    juce::Image target (juce::Image::ARGB, EbbContent::designW * 2, EbbContent::designH * 2, true);
    auto timeIt = [&] (const char* name, const std::function<void (juce::Graphics&)>& fn)
    {
        std::vector<double> t;
        for (int i = 0; i < 40; ++i)
        {
            juce::Graphics g (target);
            g.addTransform (juce::AffineTransform::scale (2.0f));
            const auto t0 = std::chrono::steady_clock::now();
            fn (g);
            t.push_back (std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count());
        }
        std::sort (t.begin(), t.end());
        std::printf ("  %-40s median %.2f ms  p95 %.2f ms\n", name, t[t.size() / 2], t[(size_t) (t.size() * 0.95)]);
    };
    timeIt ("fond recalcule (content.paint)", [&] (juce::Graphics& g) { content.invalidateBackground(); content.paint (g); });
    timeIt ("fond en cache (content.paint)", [&] (juce::Graphics& g) { content.paint (g); });
    timeIt ("courbe (paintEntireComponent)", [&] (juce::Graphics& g) { g.setOrigin (content.getCurveView().getPosition()); content.getCurveView().paintEntireComponent (g, false); });
    {
        juce::Image low (juce::Image::RGB, 380, 150, true), big (juce::Image::RGB, 1520, 600, true), ov (juce::Image::ARGB, 1520, 600, true);
        { juce::Graphics g (ov); g.setColour (juce::Colours::white.withAlpha (0.1f)); g.fillRect (100, 100, 300, 300); }
        timeIt ("agrandissement eau x4 (moyenne qualite)", [&] (juce::Graphics&) { juce::Graphics g (big); g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality); g.drawImageTransformed (low, juce::AffineTransform::scale (4.0f)); });
        timeIt ("agrandissement eau x4 (basse qualite)", [&] (juce::Graphics&) { juce::Graphics g (big); g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality); g.drawImageTransformed (low, juce::AffineTransform::scale (4.0f)); });
        timeIt ("calque fixe ARGB pose", [&] (juce::Graphics&) { juce::Graphics g (big); g.drawImageAt (ov, 0, 0); });
        timeIt ("eau quart de resolution", [&] (juce::Graphics&) { juce::Graphics g (low); g.addTransform (juce::AffineTransform::scale (0.5f)); Ui::drawWaterBackground (g, { 0, 0, 760, 300 }, false); });
        timeIt ("particules", [&] (juce::Graphics&) { juce::Graphics g (big); g.addTransform (juce::AffineTransform::scale (2.0f)); Ui::drawParticles (g, { 0, 0, 760, 300 }); });
    }
    timeIt ("fenetre entiere, fond en cache", [&] (juce::Graphics& g) { content.paintEntireComponent (g, false); });
}

} // namespace

//==============================================================================
int main (int argc, char** argv)
{
    if (const char* t = std::getenv ("EB_ALLOC_TRACE")) AllocCounter::trace = juce::String (t) == "1";
    { void* warm[2]; (void) backtrace (warm, 2); }
    juce::ScopedJuceInitialiser_GUI init;
    const juce::String only = argc > 1 ? juce::String (argv[1]) : juce::String();
    auto run = [&] (const char* name) { return only.isEmpty() || only == name; };

    if (run ("ids"))       testIds();
    if (run ("curves"))    testCurves();
    if (run ("shape"))     testShape();
    if (run ("audio"))     testAudio();
    if (run ("zero"))      testZero();
    if (run ("clicks"))    testClicks();
    if (run ("sync"))      testSync();
    if (run ("transport")) testTransport();
    if (run ("midi"))      testMidi();
    if (run ("kick"))      testKick();
    if (run ("sidechain")) testSidechainLayouts();
    if (run ("state"))     testState();
    if (run ("perf"))      testPerformance();
    if (run ("alloc"))     testAlloc();
    if (run ("shapes"))    testShapes();
    if (run ("custom"))    testCustomTensionSmooth();
    if (run ("bands"))     testBands();
    if (run ("state2"))    testStateV2();
    if (run ("edit"))      testEditor (juce::File::getCurrentWorkingDirectory().getChildFile ("captures"));
    if (run ("snap"))      snapshots (juce::File::getCurrentWorkingDirectory().getChildFile ("captures"));
    if (only == "uiprof")  uiProfile();
    if (only == "mockups") Mock::renderAll (juce::File::getCurrentWorkingDirectory().getChildFile ("maquettes"));

    std::printf ("\n%s\n", failures == 0 ? "TOUS LES TESTS PASSENT" : ("ECHECS : " + juce::String (failures)).toRawUTF8());
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile ("test-results.md");
    out.replaceWithText ("# Resultats du banc de tests d'Ebb\n" + report);
    return failures == 0 ? 0 : 1;
}
