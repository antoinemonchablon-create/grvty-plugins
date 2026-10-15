// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

/*
    Banc de tests de Confluence.
    Lancer :  ./ConfluenceTests            (tous les tests)
              ./ConfluenceTests theory     (un seul groupe : theory, names, scale, voicing, detect, modes, midi,
                                            offs, fuzz, state, hostile, perf, presets, snap)
              ./ConfluenceTests uiprof     (coût de dessin de chaque partie de l'interface)
              ./ConfluenceTests mockups    (maquettes d'interface)
              ./ConfluenceTests quick      (fuzz réduit)

    Chaque test affiche OK / ECHEC et les mesures. Code de sortie 0 = tout passe.
    Le compte rendu est aussi écrit dans test-results.md.
*/
#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Mockups.h"
#include <chrono>
#include <cstdio>
#include <deque>
#include <functional>
#include <atomic>
#include <cstdlib>
#include <new>
#include <set>
#include <map>
#include <iterator>
#include <execinfo.h>
#include <unistd.h>
#include <pthread.h>

//==============================================================================
// Compteur d'allocations : sert à vérifier que processBlock (et notre code de dessin) n'alloue jamais de mémoire.
// On intercepte malloc / calloc / realloc / memalign de la glibc (les tableaux de JUCE passent par malloc, pas par new)
// et new / new[]. CF_ALLOC_TRACE=1 affiche la pile d'appels de chaque allocation comptée.
namespace AllocCounter
{
    /** Activé depuis un thread : seules les allocations de CE thread sont comptées
        (le thread des Timers de JUCE poste des messages pendant les mesures, ce n'est pas notre code). */
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
using namespace Theory;
namespace id = ParamIDs;

//==============================================================================
int failures = 0;
juce::String report;
bool quickMode = false;

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

//==============================================================================
struct FakePlayHead : public juce::AudioPlayHead
{
    bool playing = false;
    double ppq = 0.0, bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setIsPlaying (playing);
        p.setPpqPosition (ppq);
        p.setBpm (bpm);
        p.setTimeSignature (juce::AudioPlayHead::TimeSignature { 4, 4 });
        return p;
    }
};

struct Ev
{
    int64_t t = 0;
    uint8_t d[3] {};
    int n = 3;
    int status() const { return d[0] & 0xF0; }
    int channel() const { return (d[0] & 0x0F) + 1; }
    bool isOn() const { return n == 3 && status() == 0x90 && d[2] > 0; }
    bool isOff() const { return n == 3 && (status() == 0x80 || (status() == 0x90 && d[2] == 0)); }
};

/** Simule un synthé qui reçoit la sortie de Confluence : détecte notes doublées, coupures orphelines, notes restées. */
struct SynthSim
{
    bool on[16][128] {};
    long ons = 0, offs = 0, doubleOn = 0, orphanOff = 0, badData = 0;

    void feed (const Ev& e)
    {
        if (e.isOn())
        {
            if (e.d[1] > 127 || e.d[2] > 127) ++badData;
            auto& s = on[e.channel() - 1][e.d[1] & 127];
            if (s) ++doubleOn;
            s = true;
            ++ons;
        }
        else if (e.isOff())
        {
            auto& s = on[e.channel() - 1][e.d[1] & 127];
            if (! s) ++orphanOff;
            s = false;
            ++offs;
        }
    }
    int sounding() const
    {
        int n = 0;
        for (auto& ch : on) for (bool b : ch) n += b ? 1 : 0;
        return n;
    }
};

struct Harness
{
    std::unique_ptr<ConfluenceAudioProcessor> proc = std::make_unique<ConfluenceAudioProcessor>();
    FakePlayHead head;
    double sr = 48000.0;
    int block = 512;
    int64_t rendered = 0;
    std::deque<Ev> input;
    std::vector<Ev> output;
    long formatErrors = 0;
    juce::MidiBuffer midi;
    juce::AudioBuffer<float> audio;
    bool bypass = false;
    std::function<void (int64_t)> beforeBlock;
    std::function<void (const juce::AudioBuffer<float>&, int64_t)> afterBlock;

    Harness (double sampleRate = 48000.0, int blockSize = 512) : sr (sampleRate), block (blockSize)
    {
        proc->setPlayHead (&head);
        proc->setRateAndBufferSizeDetails (sr, block);
        for (auto* p : proc->getParameters())
            p->setValueNotifyingHost (p->getDefaultValue());
        proc->prepareToPlay (sr, block);
        midi.ensureSize (65536);      // comme un hôte qui réutilise son tampon MIDI
        audio.setSize (2, juce::jmax (block, 4096));
    }

    void set (const juce::String& pidStr, float realValue)
    {
        auto* p = proc->apvts.getParameter (pidStr);
        if (p == nullptr) { std::printf ("  !! parametre inconnu %s\n", pidStr.toRawUTF8()); std::abort(); }
        p->setValueNotifyingHost (p->convertTo0to1 (realValue));
    }

    void schedule (int64_t t, uint8_t a, uint8_t b, uint8_t c)
    {
        Ev e;
        e.t = t; e.d[0] = a; e.d[1] = b; e.d[2] = c;
        auto it = std::upper_bound (input.begin(), input.end(), e, [] (const Ev& x, const Ev& y) { return x.t < y.t; });
        input.insert (it, e);
    }
    void noteOn (int64_t t, int note, int vel = 100, int ch = 1)  { schedule (t, (uint8_t) (0x90 | (ch - 1)), (uint8_t) note, (uint8_t) vel); }
    void noteOff (int64_t t, int note, int ch = 1)                { schedule (t, (uint8_t) (0x80 | (ch - 1)), (uint8_t) note, 0); }

    /** Joue jusqu'à l'instant absolu 'until'. */
    void runUntil (int64_t until)
    {
        while (rendered < until)
        {
            const int len = (int) std::min<int64_t> (block, until - rendered);
            if (beforeBlock) beforeBlock (rendered);
            midi.clear();
            while (! input.empty() && input.front().t < rendered + len)
            {
                const auto& e = input.front();
                midi.addEvent (e.d, e.n, (int) juce::jmax ((int64_t) 0, e.t - rendered));
                input.pop_front();
            }
            audio.setSize (2, len, false, false, true);
            if (head.playing) head.ppq += (double) len / sr * head.bpm / 60.0;
            if (bypass) proc->processBlockBypassed (audio, midi);
            else        proc->processBlock (audio, midi);
            if (afterBlock) afterBlock (audio, rendered);

            int last = 0;
            for (const auto m : midi)
            {
                if (m.samplePosition < 0 || m.samplePosition >= len || m.samplePosition < last) ++formatErrors;
                last = m.samplePosition;
                Ev e;
                e.t = rendered + m.samplePosition;
                e.n = juce::jmin (3, m.numBytes);
                for (int i = 0; i < e.n; ++i) e.d[i] = m.data[i];
                if (e.n == 3 && (e.status() == 0x90 || e.status() == 0x80) && (e.d[1] > 127 || e.d[2] > 127)) ++formatErrors;
                output.push_back (e);
            }
            rendered += len;
        }
    }
    void run (int64_t samples) { runUntil (rendered + samples); }

    std::vector<Ev> onsIn (int64_t from, int64_t to) const
    {
        std::vector<Ev> v;
        for (const auto& e : output) if (e.isOn() && e.t >= from && e.t < to) v.push_back (e);
        return v;
    }
    SynthSim sim() const
    {
        SynthSim s;
        for (const auto& e : output) s.feed (e);
        return s;
    }
};

/** Réglages neutres : renversement 0, serré, 8 voix, plage complète, pas de basse. */
Engine::ChordSettings neutral (int mode, int root, int scale, int type)
{
    Engine::ChordSettings cs;
    cs.mode = mode; cs.rootPc = root; cs.scale = scale; cs.chordType = type;
    cs.voicing.inversion = 0; cs.voicing.spread = 0; cs.voicing.maxVoices = 8;
    cs.voicing.low = 0; cs.voicing.high = 127; cs.voicing.bassOctaves = 0; cs.voicing.shift = 0;
    return cs;
}

std::vector<int> voicesOf (const Engine::Resolved& r)
{
    std::vector<int> v;
    if (r.voiced.bass >= 0) v.push_back (r.voiced.bass);
    for (int i = 0; i < r.voiced.voices.count; ++i) v.push_back (r.voiced.voices.n[i]);
    return v;
}

juce::String listNotes (const std::vector<int>& v)
{
    juce::StringArray s;
    for (int n : v) s.add (Params::noteName (n));
    return s.joinIntoString (" ");
}

//==============================================================================
// 1. Théorie : table de référence calculée indépendamment
//==============================================================================
/** Implémentation de référence, écrite séparément : liste de toutes les hauteurs de la gamme, puis indices. */
std::vector<int> referenceOffsets (int s, int degree, int type)
{
    // hauteurs de la gamme (tonique = 0) sur 12 octaves de part et d'autre
    std::vector<int> pitches;
    const uint16_t mask = scaleMask (s, 0);
    for (int p = -144; p <= 144; ++p)
        if ((mask >> (((p % 12) + 12) % 12)) & 1) pitches.push_back (p);
    const int size = __builtin_popcount (mask);
    // indice de la tonique (hauteur 0) dans la liste
    const int zero = (int) (std::find (pitches.begin(), pitches.end(), 0) - pitches.begin());
    const int rootIndex = zero + degree;
    const int root = pitches[(size_t) rootIndex];
    auto stepUp = [&] (int k) { return pitches[(size_t) (rootIndex + k)] - root; };
    auto inSc = [&] (int semis) { return ((mask >> (((root + semis) % 12 + 12) % 12)) & 1) != 0; };

    std::vector<int> v;

    if (type == ctCluster)
    {
        for (int k = 0; k < 4; ++k) v.push_back (stepUp (k));
        return v;
    }
    if (size == 7)
    {
        static const std::vector<std::vector<int>> steps = {
            { 0, 4, 7 }, { 0, 2, 4 }, { 0, 1, 4 }, { 0, 3, 4 }, { 0, 2, 4, 5 }, { 0, 2, 4, 6 }, { 0, 2, 4, 6, 8 },
            { 0, 2, 4, 8 }, { 0, 2, 4, 6, 8, 10 }, { 0, 2, 4, 6, 8, 10, 12 }, { 0, 3, 6, 9 } };
        for (int k : steps[(size_t) type]) v.push_back (stepUp (k));
        std::sort (v.begin(), v.end());
        return v;
    }
    // gammes de 5, 6 ou 12 notes : premier intervalle de chaque liste qui tombe dans la gamme
    auto first = [&] (std::vector<int> prefs, int from) { for (int p : prefs) if (inSc (from + p)) return from + p; return -999; };
    auto put = [&] (int o) { if (o == -999) return; for (int x : v) if (((x - o) % 12 + 12) % 12 == 0) return; v.push_back (o); };
    v.push_back (0);
    if (type == ctPower) { put (first ({ 7, 6, 8 }, 0)); v.push_back (12); }
    else if (type == ctSus2) { put (first ({ 2, 1 }, 0)); put (first ({ 7, 6, 8 }, 0)); }
    else if (type == ctSus4) { put (first ({ 5, 6 }, 0)); put (first ({ 7, 6, 8 }, 0)); }
    else if (type == ctQuartal)
    {
        int from = 0;
        for (int k = 0; k < 3; ++k) { const int o = first ({ 5, 6, 4 }, from); if (o == -999) break; put (o); from = o; }
    }
    else
    {
        const int third = first ({ 3, 4 }, 0);
        if (third != -999) put (third); else put (first ({ 5, 2 }, 0));
        put (first ({ 7, 6, 8 }, 0));
        if (type == ctSix) put (first ({ 9, 8 }, 0));
        if (type >= ctSeventh && type != ctAdd9) put (first ({ 10, 11, 9 }, 0));
        if (type == ctNinth || type == ctAdd9 || type >= ctEleventh) put (first ({ 14, 13, 15 }, 0));
        if (type >= ctEleventh) put (first ({ 17, 18, 16 }, 0));
        if (type == ctThirteenth) put (first ({ 21, 20, 22 }, 0));
    }
    std::sort (v.begin(), v.end());
    return v;
}

void testTheoryTable()
{
    section (u8 ("Th\xc3\xa9orie : accords contre une table de r\xc3\xa9" "f\xc3\xa9rence calcul\xc3\xa9" "e ind\xc3\xa9pendamment"));
    long combos = 0, mismatches = 0;
    juce::String firstBad;
    for (int s = 0; s < numScales; ++s)
        for (int deg = -3; deg < scaleSize (s) + 3; ++deg)
            for (int type = 0; type < numChordTypes; ++type)
            {
                const auto shape = buildScaleChord (s, deg, type);
                std::vector<int> got;
                for (int i = 0; i < shape.count; ++i) got.push_back (shape.offsets[i]);
                const auto ref = referenceOffsets (s, deg, type);
                ++combos;
                if (got != ref)
                {
                    if (mismatches++ == 0)
                    {
                        juce::StringArray a, b;
                        for (int x : got) a.add (juce::String (x));
                        for (int x : ref) b.add (juce::String (x));
                        firstBad = u8 (scaleInfo (s).name) + " degre " + juce::String (deg) + " " + chordTypeName (type)
                                 + " : obtenu [" + a.joinIntoString (" ") + "] attendu [" + b.joinIntoString (" ") + "]";
                    }
                }
            }
    check (mismatches == 0, u8 ("Formes d'accords (17 gammes x degr\xc3\xa9s x 12 types)"),
           juce::String (combos) + " formes, " + juce::String (mismatches) + " differences" + (firstBad.isEmpty() ? juce::String() : " ; " + firstBad));

    // chemin complet (note jouée -> accord) : 17 gammes x 12 toniques x toutes les notes de la gamme x 12 types
    long played = 0, bad = 0;
    juce::String bad1;
    for (int s = 0; s < numScales; ++s)
        for (int root = 0; root < 12; ++root)
            for (int type = 0; type < numChordTypes; ++type)
                for (int note = 24; note < 100; ++note)
                {
                    if (! inScale (s, root, note)) continue;
                    auto cs = neutral (Engine::modeScale, root, s, type);
                    Engine::Resolved r;
                    if (! Engine::resolve (cs, {}, {}, note, {}, r)) { ++bad; continue; }
                    const int deg = degreeOfPc (s, root, note);
                    auto ref = referenceOffsets (s, deg, type);
                    std::vector<int> expect;
                    for (int o : ref) expect.push_back (note + o);
                    ++played;
                    if (voicesOf (r) != expect)
                    {
                        if (bad++ == 0) bad1 = Params::noteName (note) + " " + u8 (scaleInfo (s).name) + " : " + listNotes (voicesOf (r)) + " au lieu de " + listNotes (expect);
                    }
                }
    check (bad == 0, u8 ("Note jou\xc3\xa9" "e -> accord (mode Gamme, voicing neutre)"),
           juce::String (played) + " accords, " + juce::String (bad) + " differences" + (bad1.isEmpty() ? juce::String() : " ; " + bad1));
}

//==============================================================================
// 2. Accords connus : notes et noms écrits à la main
//==============================================================================
void testKnownChords()
{
    section (u8 ("Th\xc3\xa9orie : accords connus (notes et noms \xc3\xa9" "crits \xc3\xa0 la main)"));
    struct Known { int root, scale, degree, type; const char* notes; const char* name; const char* roman; };
    const Known table[] = {
        { 0, majorScale, 0, ctSeventh, "C E G B", "Cmaj7", "I" },
        { 0, majorScale, 1, ctSeventh, "D F A C", "Dm7", "ii" },
        { 0, majorScale, 4, ctSeventh, "G B D F", "G7", "V" },
        { 0, majorScale, 6, ctSeventh, "B D F A", "Bm7b5", "vii\xc2\xb0" },
        { 0, majorScale, 0, ctTriad, "C E G", "C", "I" },
        { 0, majorScale, 0, ctAdd9, "C E G D", "Cadd9", "I" },
        { 0, majorScale, 0, ctSix, "C E G A", "C6", "I" },
        { 9, naturalMinor, 0, ctNinth, "A C E G B", "Am9", "i" },
        { 9, naturalMinor, 0, ctTriad, "A C E", "Am", "i" },
        { 9, naturalMinor, 5, ctSeventh, "F A C E", "Fmaj7", "VI" },
        { 9, naturalMinor, 6, ctSeventh, "G B D F", "G7", "VII" },
        { 9, naturalMinor, 0, ctSus2, "A B E", "Asus2", "I" },
        { 9, naturalMinor, 0, ctSus4, "A D E", "Asus4", "I" },
        { 9, naturalMinor, 0, ctPower, "A E A", "A5", "I" },
        { 9, naturalMinor, 0, ctAdd9, "A C E B", "Am(add9)", "i" },
        { 9, naturalMinor, 0, ctEleventh, "A C E G B D", "Am11", "i" },
        { 9, naturalMinor, 0, ctThirteenth, "A C E G B D F", "Am11(b13)", "i" },
        { 9, naturalMinor, 0, ctSix, "A C E F", "Am(b6)", "i" },
        { 2, dorian, 0, ctNinth, "D F A C E", "Dm9", "i" },
        { 2, dorian, 3, ctSeventh, "G B D F", "G7", "IV" },
        { 2, dorian, 0, ctSix, "D F A B", "Dm6", "i" },
        { 2, dorian, 0, ctThirteenth, "D F A C E G B", "Dm13", "i" },
        { 4, phrygian, 0, ctTriad, "E G B", "Em", "i" },
        { 4, phrygian, 1, ctTriad, "F A C", "F", "II" },
        { 4, phrygianDominant, 0, ctSeventh, "E G# B D", "E7", "I" },
        { 9, harmonicMinor, 4, ctSeventh, "E G# B D", "E7", "V" },
        { 9, harmonicMinor, 0, ctSeventh, "A C E G#", "AmMaj7", "i" },
        { 0, melodicMinor, 2, ctTriad, "Eb G B", "Ebaug", "III+" },
        { 5, lydian, 0, ctSeventh, "F A C E", "Fmaj7", "I" },
        { 5, lydian, 0, ctEleventh, "F A C E G B", "Fmaj9(#11)", "I" },
        { 7, mixolydian, 0, ctSeventh, "G B D F", "G7", "I" },
        { 11, locrian, 0, ctTriad, "B D F", "Bdim", "i\xc2\xb0" },
        { 0, minorPenta, 0, ctTriad, "C Eb G", "Cm", "i" },
        { 0, minorPenta, 2, ctTriad, "F Bb C", "Fsus4", "IV" },
        { 0, majorPenta, 0, ctTriad, "C E G", "C", "I" },
        { 9, blues, 0, ctSeventh, "A C E G", "Am7", "i" },
        { 0, chromatic, 0, ctTriad, "C Eb G", "Cm", "i" },
        { 0, minorPenta, 0, ctQuartal, "C F Bb Eb", "Cm11", "i" },
    };

    auto pcOf = [] (const juce::String& n)
    {
        static const int base[] = { 9, 11, 0, 2, 4, 5, 7 };
        int pc = base[n[0] - 'A'];
        if (n.length() > 1) pc += n[1] == '#' ? 1 : (n[1] == 'b' ? -1 : 0);
        return mod12 (pc);
    };

    int ok = 0;
    for (const auto& k : table)
    {
        const auto shape = buildScaleChord (k.scale, k.degree, k.type);
        const int rootPc = mod12 (k.root + degreeInterval (k.scale, k.degree));
        std::vector<int> gotPc, wantPc;
        int notes[16];
        for (int i = 0; i < shape.count; ++i) { gotPc.push_back (mod12 (rootPc + shape.offsets[i])); notes[i] = 48 + rootPc + shape.offsets[i]; }
        for (auto& t : juce::StringArray::fromTokens (k.notes, " ", "")) wantPc.push_back (pcOf (t));

        char name[48], roman[16];
        chordName (rootPc, notes, shape.count, useFlats (k.scale, k.root), name, 48);
        uint16_t mask = 0;
        for (int i = 0; i < shape.count; ++i) mask = (uint16_t) (mask | (1u << mod12 (shape.offsets[i])));
        romanNumeral (k.scale, k.root, rootPc, mask, roman, 16);

        const bool good = gotPc == wantPc && juce::String (name) == k.name && u8 (roman) == u8 (k.roman);
        if (good) ++ok;
        else
        {
            juce::StringArray g;
            for (int pc : gotPc) g.add (pcName (pc, useFlats (k.scale, k.root)));
            check (false, juce::String (k.name), "obtenu " + g.joinIntoString (" ") + " = " + juce::String (name) + " (" + u8 (roman) + "), attendu "
                                                  + juce::String (k.notes) + " = " + juce::String (k.name) + " (" + u8 (k.roman) + ")");
        }
    }
    check (ok == (int) std::size (table), u8 ("Accords connus"), juce::String (ok) + " / " + juce::String ((int) std::size (table)) + u8 (" corrects (notes, nom, degr\xc3\xa9)"));

    // formes fixes
    struct Fixed { int shape; const char* name; };
    const Fixed fixed[] = { { 0, "C" }, { 1, "Cm" }, { 2, "C7" }, { 3, "Cm7" }, { 4, "Cmaj7" }, { 5, "Cm9" }, { 6, "C9" }, { 7, "Cmaj9" },
                            { 8, "Cm11" }, { 9, "Csus2" }, { 10, "Csus4" }, { 11, "Cadd9" }, { 12, "Cm(add9)" }, { 13, "C6" },
                            { 14, "Cm6" }, { 15, "C5" }, { 16, "Cdim" }, { 17, "Caug" }, { 18, "Cm7b5" }, { 19, "C7sus4" }, { 20, "C13" } };
    int fok = 0;
    juce::String fbad;
    for (const auto& f : fixed)
    {
        const auto shape = fixedShape (f.shape);
        int notes[16];
        for (int i = 0; i < shape.count; ++i) notes[i] = 60 + shape.offsets[i];
        char name[48];
        chordName (0, notes, shape.count, false, name, 48);
        if (juce::String (name) == f.name) ++fok;
        else fbad << fixedShapeName (f.shape) << " -> " << name << "  ";
    }
    check (fok == (int) std::size (fixed), "Formes fixes (mode Accord fixe)", juce::String (fok) + " / " + juce::String ((int) std::size (fixed)) + " noms corrects " + fbad);
}

//==============================================================================
// 3. Aucune note hors de la gamme
//==============================================================================
void testInScale()
{
    section ("Aucune note hors de la gamme (modes Gamme, Touches blanches, Pads)");
    juce::Random rnd (42);
    long chords = 0, outside = 0, empty = 0;
    juce::String firstBad;

    for (int s = 0; s < numScales; ++s)
        for (int root = 0; root < 12; ++root)
            for (int mode : { (int) Engine::modeScale, (int) Engine::modeWhiteKeys, (int) Engine::modePads })
                for (int type = 0; type < numChordTypes; ++type)
                {
                    Engine::ChordSettings cs;
                    cs.mode = mode; cs.rootPc = root; cs.scale = s; cs.chordType = type;
                    cs.outOfScale = rnd.nextInt (3);                       // plus proche, au-dessus, en dessous
                    cs.blackKeys = rnd.nextInt (3);                        // sus4, sus2, 9e (le mode parallèle sort de la gamme par définition)
                    cs.padBase = 36;
                    cs.voicing.inversion = rnd.nextInt (5);
                    cs.voicing.spread = rnd.nextInt (5);
                    cs.voicing.maxVoices = 2 + rnd.nextInt (7);
                    cs.voicing.bassOctaves = rnd.nextInt (3);
                    cs.voicing.low = 20 + rnd.nextInt (30);
                    cs.voicing.high = cs.voicing.low + 14 + rnd.nextInt (50);
                    cs.voicing.shift = 12 * (rnd.nextInt (5) - 2);         // octave (transposition 0 : elle change de tonalité)
                    Engine::PadSet pads = Harmony::generatePads (rnd.nextInt (4), s, (uint32_t) rnd.nextInt() | 1u);
                    for (auto& p : pads.pads) { p.type = (int8_t) rnd.nextInt (numChordTypes); p.degree = (int8_t) rnd.nextInt (7); p.octave = (int8_t) (rnd.nextInt (5) - 2); }

                    Voicing::Notes prev;
                    for (int note = 0; note < 128; ++note)
                    {
                        Engine::Resolved r;
                        if (! Engine::resolve (cs, pads, {}, note, prev, r)) { if (mode != Engine::modePads) ++empty; continue; }
                        prev = r.voiced.voices;
                        ++chords;
                        for (int x : voicesOf (r))
                            if (! inScale (s, root, x) || x < 0 || x > 127)
                            {
                                if (outside++ == 0)
                                    firstBad = Params::noteName (note) + " " + u8 (scaleInfo (s).name) + " " + pcName (root, false) + " -> " + listNotes (voicesOf (r));
                                break;
                            }
                    }
                }
    check (outside == 0, "Notes dans la gamme", juce::String (chords) + " accords (17 gammes x 12 toniques x 3 modes x 12 types x 128 notes, voicings au hasard), "
                                                + juce::String (outside) + " hors gamme" + (firstBad.isEmpty() ? juce::String() : " ; " + firstBad));
    check (true, "Notes sans accord", juce::String (empty) + u8 (" (plage trop \xc3\xa9troite pour la classe de hauteur, hors des notes MIDI)"));
}

//==============================================================================
// 4. Voicing
//==============================================================================
void testVoicing()
{
    section ("Voicing : renversements, ecartements, voix, plage, basse");
    using namespace Voicing;
    const auto cmaj7 = fixedShape (4);   // C E G B
    auto place1 = [&] (int inv, int spread)
    {
        Settings s; s.inversion = inv; s.spread = spread; s.low = 0; s.high = 127;
        auto r = voice (cmaj7, 60, s, {});
        std::vector<int> v (r.voices.n, r.voices.n + r.voices.count);
        return v;
    };
    check (place1 (0, spClose) == std::vector<int> { 60, 64, 67, 71 }, u8 ("Serr\xc3\xa9, renversement 0"), listNotes (place1 (0, spClose)));
    check (place1 (1, spClose) == std::vector<int> { 64, 67, 71, 72 }, "Renversement 1", listNotes (place1 (1, spClose)));
    check (place1 (2, spClose) == std::vector<int> { 67, 71, 72, 76 }, "Renversement 2", listNotes (place1 (2, spClose)));
    check (place1 (3, spClose) == std::vector<int> { 71, 72, 76, 79 }, "Renversement 3", listNotes (place1 (3, spClose)));
    check (place1 (0, spOpen)  == std::vector<int> { 60, 67, 76, 83 }, "Ouvert", listNotes (place1 (0, spOpen)));
    check (place1 (0, spDrop2) == std::vector<int> { 55, 60, 64, 71 }, "Drop 2", listNotes (place1 (0, spDrop2)));
    check (place1 (0, spDrop3) == std::vector<int> { 52, 60, 67, 71 }, "Drop 3", listNotes (place1 (0, spDrop3)));
    check (place1 (0, spWide)  == std::vector<int> { 48, 67, 76, 83 }, "Large (2 octaves)", listNotes (place1 (0, spWide)));

    // limite de voix : la quinte part d'abord, puis la fondamentale si la basse la joue
    {
        Settings s; s.maxVoices = 4;
        auto r = voice (fixedShape (5), 60, s, {});          // Cm9 : C Eb G Bb D -> sans la quinte
        std::vector<int> v (r.voices.n, r.voices.n + r.voices.count);
        check (v == std::vector<int> { 60, 63, 70, 74 }, "Voix max 4 sur Cm9 (sans quinte)", listNotes (v));
        s.maxVoices = 3; s.bassOctaves = 1;
        r = voice (fixedShape (5), 60, s, {});
        std::vector<int> w (r.voices.n, r.voices.n + r.voices.count);
        check (w == std::vector<int> { 63, 70, 74 } && r.bass == 48, "Voix max 3 + basse (la basse joue la fondamentale)",
               listNotes (w) + " + basse " + Params::noteName (r.bass));
    }

    // propriétés sur 200 000 accords au hasard
    juce::Random rnd (7);
    long n = 0, outOfRange = 0, dup = 0, tooMany = 0, bassBad = 0;
    for (int i = 0; i < 200000; ++i)
    {
        Settings s;
        s.inversion = rnd.nextInt (5); s.spread = rnd.nextInt (5); s.maxVoices = 2 + rnd.nextInt (7);
        s.low = rnd.nextInt (90); s.high = s.low + 12 + rnd.nextInt (40); s.bassOctaves = rnd.nextInt (3);
        s.shift = 12 * (rnd.nextInt (7) - 3) + rnd.nextInt (25) - 12;
        const auto shape = rnd.nextBool() ? buildScaleChord (rnd.nextInt (numScales), rnd.nextInt (9) - 1, rnd.nextInt (numChordTypes))
                                          : fixedShape (rnd.nextInt (numFixedShapes - 1));
        Notes prev;
        for (int k = 0; k < rnd.nextInt (4); ++k) prev.push (40 + rnd.nextInt (50));
        prev.sortUnique();
        const auto r = voice (shape, rnd.nextInt (128), s, prev);
        if (! r.valid()) continue;
        ++n;
        if (r.voices.count > s.maxVoices) ++tooMany;
        for (int k = 0; k < r.voices.count; ++k)
        {
            if (r.voices.n[k] < std::min (s.low, 127) || r.voices.n[k] > std::min (s.high, 127)) { ++outOfRange; break; }
            if (k > 0 && r.voices.n[k] <= r.voices.n[k - 1]) { ++dup; break; }
        }
        if (r.bass >= 0 && (r.bass >= r.voices.n[0] || mod12 (r.bass) != r.rootPc)) ++bassBad;
    }
    check (outOfRange == 0 && dup == 0 && tooMany == 0 && bassBad == 0, "200 000 voicings au hasard",
           juce::String (n) + " accords : " + juce::String (outOfRange) + " hors plage, " + juce::String (dup) + " doublons, "
           + juce::String (tooMany) + " trop de voix, " + juce::String (bassBad) + " basses incorrectes");

    // voice leading Auto : moins de mouvement qu'avec un renversement fixe
    auto progressionMovement = [] (int inversion, int scale, std::vector<int> degrees, int type)
    {
        Engine::ChordSettings cs = neutral (Engine::modeScale, 0, scale, type);
        cs.voicing.inversion = inversion;
        cs.voicing.low = 36; cs.voicing.high = 96;
        Voicing::Notes prev;
        double total = 0.0;
        int changes = 0, voices = 0;
        for (int rep = 0; rep < 2; ++rep)
            for (int d : degrees)
            {
                Engine::Resolved r;
                Engine::resolve (cs, {}, {}, 60 + degreeInterval (scale, d), prev, r);
                if (prev.count > 0) { total += movement (r.voiced.voices, prev); ++changes; voices += r.voiced.voices.count; }
                prev = r.voiced.voices;
            }
        return total / juce::jmax (1, voices);   // demi-tons par voix et par changement
    };
    struct Prog { const char* name; int scale; std::vector<int> deg; int type; };
    const Prog progs[] = {
        { "I vi IV V (majeur, 7e)", majorScale, { 0, 5, 3, 4 }, ctSeventh },
        { "ii V I vi (majeur, 9e)", majorScale, { 1, 4, 0, 5 }, ctNinth },
        { "i VI III VII (mineur, triades)", naturalMinor, { 0, 5, 2, 6 }, ctTriad },
        { "i iv v i (dorien, 7e)", dorian, { 0, 3, 4, 0 }, ctSeventh },
        { "i bII i bVII (phrygien, triades)", phrygian, { 0, 1, 0, 6 }, ctTriad },
    };
    bool allBetter = true;
    juce::String detail;
    for (const auto& p : progs)
    {
        const double fixed0 = progressionMovement (0, p.scale, p.deg, p.type);
        const double autoM  = progressionMovement (Voicing::inversionAuto, p.scale, p.deg, p.type);
        allBetter = allBetter && autoM < fixed0;
        detail << p.name << " : " << juce::String (autoM, 2) << " contre " << juce::String (fixed0, 2) << " ; ";
    }
    check (allBetter, u8 ("Voice leading Auto (demi-tons par voix et par changement, Auto contre renversement 0)"), detail);
}

//==============================================================================
// 5. Détection de la gamme
//==============================================================================
void testDetection()
{
    section (u8 ("D\xc3\xa9tection de la gamme (m\xc3\xa9lodies de test connues, la bonne gamme dans le top 3)"));
    struct Melody { juce::String name; int root, scale; std::vector<int> notes; std::vector<int> lengths; };
    // notes MIDI ; durée en double-croches (1 = 1/16)
    const Melody melodies[] = {
        { "La mineur (naturel)", 9, naturalMinor, { 57, 60, 64, 67, 65, 64, 62, 60, 59, 57, 64, 57, 53, 55, 57 }, { 4, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 4, 2, 2, 8 } },
        { "Do majeur", 0, majorScale, { 60, 64, 67, 72, 71, 69, 67, 65, 64, 62, 60, 55, 60, 64, 62, 60 }, { 4, 2, 2, 4, 2, 2, 2, 2, 2, 2, 4, 2, 2, 2, 2, 8 } },
        { u8 ("R\xc3\xa9 dorien"), 2, dorian, { 50, 53, 57, 59, 60, 57, 59, 53, 50, 62, 60, 59, 57, 55, 53, 50 }, { 4, 2, 2, 4, 2, 2, 4, 2, 4, 2, 2, 4, 2, 2, 2, 8 } },
        { "Mi phrygien", 4, phrygian, { 52, 53, 52, 55, 53, 52, 59, 57, 55, 53, 52, 48, 52, 53, 52 }, { 4, 2, 4, 2, 2, 4, 2, 2, 2, 2, 4, 2, 2, 2, 8 } },
        { "Sol mixolydien", 7, mixolydian, { 55, 59, 62, 65, 64, 62, 60, 59, 57, 55, 65, 62, 55, 59, 55 }, { 4, 2, 2, 4, 2, 2, 2, 2, 2, 4, 4, 2, 4, 2, 8 } },
        { "Fa# mineur", 6, naturalMinor, { 54, 57, 61, 64, 62, 61, 59, 57, 56, 54, 61, 54, 50, 52, 54 }, { 4, 2, 2, 2, 2, 2, 2, 2, 2, 4, 2, 4, 2, 2, 8 } },
        { "Do mineur harmonique", 0, harmonicMinor, { 48, 51, 55, 59, 60, 56, 55, 51, 50, 59, 48, 55, 56, 59, 60, 48 }, { 4, 2, 2, 2, 4, 2, 2, 2, 2, 2, 4, 2, 2, 2, 4, 8 } },
        { "La pentatonique mineure", 9, minorPenta, { 57, 60, 62, 64, 67, 64, 62, 60, 57, 67, 64, 57, 60, 57 }, { 4, 2, 2, 2, 4, 2, 2, 2, 4, 2, 2, 4, 2, 8 } },
        { "Mi phrygien dominant", 4, phrygianDominant, { 52, 53, 56, 57, 59, 57, 56, 53, 52, 60, 59, 56, 53, 52, 62, 52 }, { 4, 2, 2, 2, 4, 2, 2, 2, 4, 2, 2, 2, 2, 4, 2, 8 } },
        { "Fa lydien", 5, lydian, { 53, 57, 60, 59, 57, 55, 53, 64, 60, 59, 57, 53, 59, 60, 53 }, { 4, 2, 2, 4, 2, 2, 4, 2, 2, 4, 2, 4, 2, 2, 8 } },
    };

    int ok = 0;
    for (const auto& m : melodies)
    {
        std::array<float, 12> w {}, b {};
        for (size_t i = 0; i < m.notes.size(); ++i)
        {
            const int pc = m.notes[i] % 12;
            w[(size_t) pc] += 1.0f + 0.5f * (float) m.lengths[i] * 0.125f;   // même pondération que le plugin (1 + 0,5 x secondes à 120 BPM)
            b[(size_t) pc] += 1.0f;                                         // mélodie seule : chaque note est la plus grave
        }
        const auto rank = Harmony::rankScales (w, b, 3);
        int pos = -1;
        for (int i = 0; i < (int) rank.size(); ++i) if (rank[(size_t) i].root == m.root && rank[(size_t) i].scale == m.scale) pos = i;
        juce::StringArray top;
        for (auto& c : rank) top.add (juce::String (pcName (c.root, false)) + " " + u8 (scaleInfo (c.scale).name));
        if (pos >= 0) ++ok;
        check (pos >= 0, m.name, pos >= 0 ? "rang " + juce::String (pos + 1) + " (" + top.joinIntoString (", ") + ")"
                                          : "absente du top 3 : " + top.joinIntoString (", "));
    }

    // de bout en bout : la mélodie jouée dans le plugin pendant l'analyse
    {
        Harness h;
        h.proc->startAnalysis();
        const auto& m = melodies[2];   // ré dorien
        int64_t t = 1000;
        for (size_t i = 0; i < m.notes.size(); ++i)
        {
            const int64_t len = (int64_t) m.lengths[i] * 6000;   // double-croche à 120 BPM, 48 kHz
            h.noteOn (t, m.notes[i]);
            h.noteOff (t + len - 100, m.notes[i]);
            t += len;
        }
        h.runUntil (t + 1000);
        const auto rank = h.proc->getAnalysisRanking (3);
        const bool good = ! rank.empty() && rank[0].root == 2 && rank[0].scale == dorian;
        check (good && h.proc->getAnalysisNoteCount() == (int) m.notes.size(), u8 ("Analyse dans le plugin (r\xc3\xa9 dorien jou\xc3\xa9)"),
               rank.empty() ? juce::String ("aucun resultat") : juce::String ("1er : ") + pcName (rank[0].root, false) + " " + u8 (scaleInfo (rank[0].scale).name)
               + ", " + juce::String (h.proc->getAnalysisNoteCount()) + " notes");
    }

    // enchaînements proposés : quelques évidences
    {
        auto has = [] (std::array<int, 3> a, int d) { return a[0] == d || a[1] == d || a[2] == d; };
        const bool v1 = has (Harmony::suggestNext (majorScale, 4, Harmony::styleDeep), 0);          // V -> I
        const bool ii = has (Harmony::suggestNext (majorScale, 1, Harmony::styleDeep), 4);          // ii -> V
        const bool m1 = has (Harmony::suggestNext (naturalMinor, 0, Harmony::styleHypnotic), 6) || has (Harmony::suggestNext (naturalMinor, 0, Harmony::styleHypnotic), 3);
        const bool ph = has (Harmony::suggestNext (phrygian, 0, Harmony::styleMental), 1);     // i -> bII en phrygien
        check (v1 && ii && m1 && ph, u8 ("Enchainements propos\xc3\xa9s"), juce::String ("V->I ") + (v1 ? "oui" : "non") + ", ii->V " + (ii ? "oui" : "non")
               + ", i->VII/iv " + (m1 ? "oui" : "non") + ", phrygien i->bII " + (ph ? "oui" : "non"));
    }
}

//==============================================================================
// 5b. Modes de jeu
//==============================================================================
std::string chordNameOf (const Engine::Resolved& r, int scale, int root)
{
    const auto v = voicesOf (r);
    char buf[48];
    chordName (r.voiced.rootPc, v.data(), (int) v.size(), useFlats (scale, root), buf, 48);
    return buf;
}

void testModes()
{
    section ("Modes de jeu : Gamme (hors gamme), Touches blanches, Accord fixe, Pads");
    // hors gamme : do majeur, on joue C#3
    {
        juce::String got;
        bool ok = true;
        const char* expect[] = { "Dm7", "Dm7", "Cmaj7", "" };
        for (int policy = 0; policy < 4; ++policy)
        {
            auto cs = neutral (Engine::modeScale, 0, majorScale, ctSeventh);
            cs.outOfScale = policy;
            Engine::Resolved r;
            const bool played = Engine::resolve (cs, {}, {}, 61, {}, r);
            const auto name = played ? chordNameOf (r, majorScale, 0) : std::string();
            ok = ok && name == expect[policy];
            got << Choices::outOfScale()[policy] << " -> " << (played ? juce::String (name) : juce::String ("rien")) << " ; ";
        }
        check (ok, u8 ("Note hors gamme (C#3 en do majeur)"), got);
    }
    // touches blanches en fa# mineur : Do..Si = i ii° III iv v VI VII, quelle que soit la tonique
    {
        const char* expect[7] = { "F#m7", "G#m7b5", "Amaj7", "Bm7", "C#m7", "Dmaj7", "E7" };
        const int whites[7] = { 60, 62, 64, 65, 67, 69, 71 };
        bool ok = true;
        juce::String got;
        for (int i = 0; i < 7; ++i)
        {
            auto cs = neutral (Engine::modeWhiteKeys, 6, naturalMinor, ctSeventh);
            Engine::Resolved r;
            Engine::resolve (cs, {}, {}, whites[i], {}, r);
            const auto name = chordNameOf (r, naturalMinor, 6);
            ok = ok && name == expect[i];
            got << Params::noteName (whites[i]) << "=" << juce::String (name) << " ";
        }
        check (ok, u8 ("Touches blanches en F# mineur"), got);

        // touches noires : variantes du degré de la touche blanche en dessous
        juce::String got2;
        bool ok2 = true;
        const std::pair<int, const char*> variants[] = { { Engine::bkSus4, "F#sus4" }, { Engine::bkSus2, "F#sus2" }, { Engine::bkNinth, "F#m9" },
                                                         { Engine::bkBorrowed, "F#maj7" } };
        for (auto [bk, name] : variants)
        {
            auto cs = neutral (Engine::modeWhiteKeys, 6, naturalMinor, ctSeventh);
            cs.blackKeys = bk;
            Engine::Resolved r;
            Engine::resolve (cs, {}, {}, 61, {}, r);
            const auto n = chordNameOf (r, naturalMinor, 6);
            ok2 = ok2 && n == name && (bk != Engine::bkBorrowed || r.borrowed);
            got2 << Choices::blackKeys()[bk] << "=" << juce::String (n) << " ";
        }
        auto cs = neutral (Engine::modeWhiteKeys, 6, naturalMinor, ctSeventh);
        cs.blackKeys = Engine::bkMute;
        Engine::Resolved r;
        const bool muted = ! Engine::resolve (cs, {}, {}, 61, {}, r);
        check (ok2 && muted, u8 ("Touches noires (C#3 = variante du degr\xc3\xa9 i)"), got2 + (muted ? "Muettes=rien" : "Muettes=JOUE"));
    }
    // accord fixe : la forme suit la note jouée, chromatiquement
    {
        auto cs = neutral (Engine::modeFixed, 0, majorScale, ctSeventh);
        cs.fixedShape = 5;   // m9
        juce::String got;
        bool ok = true;
        for (int note : { 57, 58, 61, 66 })
        {
            Engine::Resolved r;
            Engine::resolve (cs, {}, {}, note, {}, r);
            const auto v = voicesOf (r);
            ok = ok && v == std::vector<int> { note, note + 3, note + 7, note + 10, note + 14 };
            got << juce::String (chordNameOf (r, majorScale, 0)) << " ";
        }
        check (ok, u8 ("Accord fixe m9 sur A, A#, C#, F#"), got);
    }
    // pads : 8 notes consécutives depuis la note de départ, les autres ne jouent rien
    {
        auto cs = neutral (Engine::modePads, 9, naturalMinor, ctSeventh);
        cs.padBase = 36;
        Engine::PadSet pads;
        for (int i = 0; i < 8; ++i) { pads.pads[(size_t) i].degree = (int8_t) (i % 7); pads.pads[(size_t) i].type = ctSeventh; pads.pads[(size_t) i].inversion = 0; }
        int played = 0;
        bool ok = true;
        for (int note = 30; note < 50; ++note)
        {
            Engine::Resolved r;
            const bool p = Engine::resolve (cs, pads, {}, note, {}, r);
            if (p) { ++played; ok = ok && r.padIndex == note - 36; }
        }
        Engine::Resolved r0;
        Engine::resolve (cs, pads, {}, 36, {}, r0);
        check (played == 8 && ok && chordNameOf (r0, naturalMinor, 9) == "Am7", "Pads (C1 a G1)",
               juce::String (played) + " notes sur 20 jouent un pad ; pad 1 = " + juce::String (chordNameOf (r0, naturalMinor, 9)));
    }
}

//==============================================================================
// 6. MIDI : format, latence, strum, canal, durée fixe, latch
//==============================================================================
void setNeutral (Harness& h)
{
    h.set (id::mode, 0); h.set (id::root, 0); h.set (id::scale, majorScale); h.set (id::chordType, ctSeventh);
    h.set (id::inversion, 0); h.set (id::spread, 0); h.set (id::maxVoices, 8); h.set (id::lowNote, 0); h.set (id::highNote, 127);
    h.set (id::bass, 0); h.set (id::strum, 0); h.set (id::humanTime, 0); h.set (id::humanVel, 0);
}

void testMidiBasics()
{
    section ("MIDI : latence nulle, strum, canal de sortie, bornes");
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const int blocks[] = { 1, 64, 512, 2048 };

    // latence nulle : toutes les notes de l'accord au même échantillon que la note jouée
    {
        long chords = 0, late = 0;
        for (double sr : rates)
            for (int bs : blocks)
            {
                Harness h (sr, bs);
                setNeutral (h);
                juce::Random rnd (3);
                int64_t t = 17;
                std::vector<int64_t> times;
                for (int k = 0; k < 40; ++k)
                {
                    h.noteOn (t, 48 + rnd.nextInt (24));
                    times.push_back (t);
                    t += 300 + rnd.nextInt (5000);
                    h.noteOff (t - 150, 0);   // relâchement inoffensif (note jamais jouée)
                }
                h.runUntil (t + 1000);
                for (auto tt : times)
                {
                    const auto ons = h.onsIn (tt, tt + 1);
                    ++chords;
                    if (ons.size() != 4) ++late;
                }
            }
        check (late == 0, u8 ("Latence 0 (strum et humanize \xc3\xa0 0)"), juce::String (chords) + u8 (" accords sur 3 fr\xc3\xa9quences x 4 tailles de bloc : ")
                                                         + juce::String (late) + u8 (" accords dont une note n'est pas au m\xc3\xaame \xc3\xa9" "chantillon"));
    }

    // strum : écart mesuré = réglage, à 1 échantillon près, y compris d'un bloc à l'autre
    {
        long notes = 0, off = 0;
        double worst = 0.0;
        for (double sr : rates)
            for (int bs : blocks)
                for (int sync = 0; sync < 2; ++sync)
                {
                    Harness h (sr, bs);
                    setNeutral (h);
                    h.set (id::chordType, ctThirteenth);
                    h.set (id::strum, 37.3f);
                    h.set (id::strumSync, (float) sync);
                    h.set (id::strumSyncRate, 4);    // 1/32
                    h.head.bpm = 128.0;
                    const double step = sync ? 0.125 * 60.0 / 128.0 * sr : 0.0373 * sr;
                    int64_t t = 333;
                    std::vector<int64_t> starts;
                    for (int k = 0; k < 6; ++k) { h.noteOn (t, 60 + 2 * k); h.noteOff (t + (int64_t) (step * 9), 60 + 2 * k); starts.push_back (t); t += (int64_t) (step * 10) + 977; }
                    h.runUntil (t + 1000);
                    for (auto s0 : starts)
                    {
                        auto ons = h.onsIn (s0, s0 + (int64_t) (step * 8));
                        std::sort (ons.begin(), ons.end(), [] (const Ev& a, const Ev& b) { return a.d[1] < b.d[1]; });
                        for (size_t k = 0; k < ons.size(); ++k)
                        {
                            const double expect = (double) s0 + (double) k * step;
                            const double err = std::abs ((double) ons[k].t - expect);
                            worst = std::max (worst, err);
                            ++notes;
                            if (err > 1.0) ++off;
                        }
                    }
                }
        check (off == 0, u8 ("Strum (37,3 ms et 1/32 \xc3\xa0 128 BPM)"), juce::String (notes) + u8 (" notes, \xc3\xa9" "cart maximal ")
                                                     + juce::String (worst, 2) + u8 (" \xc3\xa9" "chantillon, ") + juce::String (off) + " hors tolerance");
    }

    // canal de sortie, bornes, CC recopiés
    {
        Harness h (48000.0, 256);
        setNeutral (h);
        h.set (id::outChannel, 7);
        h.set (id::octave, 3);
        h.set (id::highNote, 127);
        h.noteOn (100, 120, 90, 3);
        h.schedule (200, 0xB2, 74, 55);          // CC74 canal 3
        h.schedule (300, 0xE2, 0, 80);           // pitch bend canal 3
        h.schedule (310, 0xB2, 64, 127);         // sustain
        h.noteOff (5000, 120, 3);
        h.noteOn (6000, 2, 90, 1);
        h.noteOff (9000, 2, 1);
        h.runUntil (12000);
        bool channelOk = true, rangeOk = true, ccOk = false, pbOk = false, susOk = false;
        for (const auto& e : h.output)
        {
            if (e.n == 3 && e.status() >= 0x80 && e.status() < 0xF0 && e.channel() != 7) channelOk = false;
            if ((e.isOn() || e.isOff()) && e.d[1] > 127) rangeOk = false;
            if (e.status() == 0xB0 && e.d[1] == 74 && e.d[2] == 55) ccOk = true;
            if (e.status() == 0xE0 && e.d[2] == 80) pbOk = true;
            if (e.status() == 0xB0 && e.d[1] == 64 && e.d[2] == 127) susOk = true;
        }
        check (channelOk, u8 ("Canal de sortie respect\xc3\xa9 (canal 7, entr\xc3\xa9" "e sur 1 et 3)"), channelOk ? "toutes les notes et CC sur le canal 7" : "evenement sur un autre canal");
        check (rangeOk && h.formatErrors == 0, "Notes 0-127, horodatages dans le bloc et dans l'ordre",
               juce::String (h.output.size()) + " evenements, " + juce::String (h.formatErrors) + " erreurs (notes jouees a C8+3 oct. et C-2)");
        check (ccOk && pbOk && susOk, u8 ("CC, pitch bend, sustain transmis"), juce::String ("CC74 ") + (ccOk ? "oui" : "non") + ", pitch bend " + (pbOk ? "oui" : "non")
               + ", sustain " + (susOk ? "oui" : "non"));
        check (h.sim().sounding() == 0 && h.sim().doubleOn == 0, "Pas de note restante", juce::String (h.sim().sounding()) + " note(s) restante(s)");
    }

    // durée fixe 1/16 à 120 BPM = 6000 échantillons à 48 kHz
    {
        Harness h (48000.0, 333);
        setNeutral (h);
        h.set (id::durMode, 1);
        h.set (id::duration, 2);
        h.noteOn (1000, 60);
        h.noteOff (1200, 60);        // touche relâchée tout de suite : l'accord dure quand même 1/16
        h.noteOn (20000, 62);
        h.noteOff (90000, 62);       // touche tenue longtemps : l'accord s'arrête après 1/16
        h.runUntil (100000);
        std::vector<int64_t> lengths;
        std::map<int, int64_t> onAt;
        for (const auto& e : h.output)
        {
            if (e.isOn()) onAt[e.d[1]] = e.t;
            else if (e.isOff() && onAt.count (e.d[1])) lengths.push_back (e.t - onAt[e.d[1]]);
        }
        bool ok = lengths.size() == 8;
        for (auto l : lengths) ok = ok && std::abs ((double) l - 6000.0) <= 1.0;
        check (ok && h.sim().sounding() == 0, u8 ("Dur\xc3\xa9" "e fixe 1/16 (touche courte et touche longue)"),
               juce::String ((int) lengths.size()) + u8 (" notes, dur\xc3\xa9" "es ") + (lengths.empty() ? juce::String ("-") : juce::String (lengths.front()) + ".." + juce::String (lengths.back()))
               + u8 (" \xc3\xa9" "chantillons (attendu 6000)"));
    }
}

//==============================================================================
// 7. Le note-off relâche exactement son accord ; Latch
//==============================================================================
void testNoteOffs()
{
    section ("Note-off : exactement les notes de son accord, Latch");
    {
        Harness h (48000.0, 128);
        setNeutral (h);
        h.noteOn (100, 57);            // Am7 en do majeur : A C E G
        h.runUntil (1000);
        // tout change pendant que la touche est tenue
        h.set (id::mode, Engine::modeFixed); h.set (id::fixedShape, 6); h.set (id::octave, 2); h.set (id::inversion, 2);
        h.set (id::outChannel, 5); h.set (id::maxVoices, 3); h.set (id::bass, 2); h.set (id::scale, phrygian);
        h.noteOn (2000, 62);           // un autre accord, sur le canal 5
        h.runUntil (3000);
        const auto before = h.output.size();
        h.noteOff (4000, 57);
        h.runUntil (5000);
        std::set<int> offNotes;
        int offChannel = 0;
        for (size_t i = before; i < h.output.size(); ++i)
            if (h.output[i].isOff()) { offNotes.insert (h.output[i].d[1]); offChannel = h.output[i].channel(); }
        const bool exact = offNotes == std::set<int> { 57, 60, 64, 67 } && offChannel == 1;
        const int stillOn = h.sim().sounding();
        h.noteOff (6000, 62);
        h.runUntil (7000);
        check (exact && stillOn > 0 && h.sim().sounding() == 0, u8 ("Rel\xc3\xa2" "chement apr\xc3\xa8s changement de tous les r\xc3\xa9glages"),
               juce::String ("coupe ") + juce::String ((int) offNotes.size()) + " notes (A C E G, canal 1) ; " + juce::String (stillOn) + " notes du 2e accord restent jusqu'a son relachement");
    }
    {
        // deux accords qui partagent des notes : la note commune n'est coupée qu'au dernier relâchement
        Harness h (48000.0, 64);
        setNeutral (h);
        h.set (id::chordType, ctTriad);
        h.noteOn (100, 60);            // C E G
        h.noteOn (500, 64);            // Em : E G B (E et G en commun)
        h.noteOff (1000, 60);          // C coupé, E et G restent
        h.runUntil (2000);
        auto s = h.sim();
        const bool shared = s.on[0][64] && s.on[0][67] && s.on[0][71] && ! s.on[0][60];
        h.noteOff (2500, 64);
        h.runUntil (3000);
        check (shared && h.sim().sounding() == 0 && h.sim().doubleOn == 0 && h.sim().orphanOff == 0, u8 ("Notes partag\xc3\xa9" "es entre deux accords"),
               shared ? u8 ("E et G restent tant que Em est tenu, relanc\xc3\xa9" "es proprement") : juce::String ("note commune coupee trop tot"));
    }
    {
        Harness h (48000.0, 256);
        setNeutral (h);
        h.set (id::latch, 1);
        h.noteOn (100, 60);
        h.noteOff (2000, 60);
        h.runUntil (5000);
        const int held = h.sim().sounding();
        h.noteOn (6000, 65);           // la note suivante remplace l'accord tenu
        h.runUntil (7000);
        auto s = h.sim();
        const bool replaced = ! s.on[0][60] && s.on[0][65];
        h.noteOff (8000, 65);
        h.runUntil (9000);
        const int stillLatched = h.sim().sounding();
        h.set (id::latch, 0);          // Latch coupé : l'accord tenu s'arrête
        h.run (512);
        check (held == 4 && replaced && stillLatched == 4 && h.sim().sounding() == 0, "Latch",
               juce::String (held) + " notes tenues apres relachement, remplacees a la note suivante, coupees quand le Latch s'eteint");
    }
}

//==============================================================================
// 8. All Notes Off : Stop, bypass, preset, désactivation, Panic
//==============================================================================
void testAllNotesOff()
{
    section ("All Notes Off : Stop, bypass, changement de preset, desactivation, Panic");
    auto scenario = [] (const char* name, std::function<void (Harness&)> action)
    {
        Harness h (48000.0, 256);
        setNeutral (h);
        h.set (id::strum, 150.0f);           // une partie des notes est encore en attente au moment de la coupure
        h.set (id::chordType, ctThirteenth);
        h.head.playing = true;
        h.noteOn (100, 60);
        h.noteOn (200, 67);
        h.runUntil (14000);                  // ~290 ms : les premières notes sonnent, d'autres pas encore
        const int before = h.sim().sounding();
        action (h);
        h.run (256);
        const int64_t cut = h.rendered;
        h.run (48000);                       // plus rien ne doit partir ensuite
        long after = 0;
        for (const auto& e : h.output) if (e.isOn() && e.t >= cut) ++after;
        auto s = h.sim();
        check (before > 0 && s.sounding() == 0 && after == 0 && s.doubleOn == 0, name,
               juce::String (before) + " notes sonnaient, " + juce::String (s.sounding()) + " restent, " + juce::String (after) + " notes en attente parties ensuite");
    };
    scenario ("Stop du transport", [] (Harness& h) { h.head.playing = false; });
    scenario ("Bypass", [] (Harness& h) { h.bypass = true; });
    scenario ("Changement de preset", [] (Harness& h) { h.proc->notifyPresetChange(); });
    scenario ("Rechargement d'etat (projet)", [] (Harness& h) { juce::MemoryBlock mb; h.proc->getStateInformation (mb); h.proc->setStateInformation (mb.getData(), (int) mb.getSize()); });
    scenario ("Desactivation du plugin", [] (Harness& h) { h.proc->releaseResources(); h.proc->prepareToPlay (h.sr, h.block); });
    scenario ("Bouton Panic", [] (Harness& h) { h.proc->requestPanic(); });

    // Panic envoie aussi sustain 0 et CC123 ; retour du bypass : les notes passées telles quelles sont coupées
    {
        Harness h (48000.0, 256);
        setNeutral (h);
        h.proc->requestPanic();
        h.run (256);
        bool cc123 = false, sus0 = false;
        for (const auto& e : h.output) { if (e.status() == 0xB0 && e.d[1] == 123) cc123 = true; if (e.status() == 0xB0 && e.d[1] == 64 && e.d[2] == 0) sus0 = true; }
        h.bypass = true;
        h.noteOn (h.rendered + 10, 60);
        h.run (2000);
        const int rawOn = h.sim().sounding();
        h.bypass = false;
        h.run (256);
        check (cc123 && sus0 && rawOn == 1 && h.sim().sounding() == 0, "Panic (CC123 + sustain 0) et bypass (note telle quelle)",
               juce::String ("CC123 ") + (cc123 ? "oui" : "non") + ", sustain 0 " + (sus0 ? "oui" : "non") + ", bypass : " + juce::String (rawOn)
               + " note jouee telle quelle, coupee a la sortie du bypass");
    }
}

//==============================================================================
// 9. Notes bloquées : fuzz
//==============================================================================
void testFuzz()
{
    section (u8 ("Notes bloqu\xc3\xa9" "es : \xc3\xa9v\xc3\xa9nements au hasard"));
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const int blocks[] = { 1, 64, 512, 2048 };
    const int eventsPerConfig = quickMode ? 10000 : 100000;
    long totalEvents = 0, totalOns = 0;
    bool allOk = true;

    for (double sr : rates)
        for (int bs : blocks)
        {
            Harness h (sr, bs);
            juce::Random rnd ((int64_t) (sr + bs));
            std::vector<std::pair<int, int>> keys;   // touches tenues (canal, note)
            const int64_t spacing = bs == 1 ? 12 : 60;
            int64_t t = 0;
            juce::MemoryBlock savedState;
            h.proc->getStateInformation (savedState);

            // actions sur le plugin, entre deux blocs (comme l'interface et l'hôte le feraient)
            std::vector<std::function<void()>> actions;
            h.beforeBlock = [&] (int64_t)
            {
                for (auto& a : actions) a();
                actions.clear();
            };

            for (int i = 0; i < eventsPerConfig; ++i)
            {
                t += 1 + rnd.nextInt ((int) spacing * 2);
                const int r = rnd.nextInt (1000);
                if (r < 380 || keys.empty())
                {
                    const int ch = 1 + rnd.nextInt (2), note = 30 + rnd.nextInt (60);
                    h.noteOn (t, note, 1 + rnd.nextInt (127), ch);
                    keys.push_back ({ ch, note });
                }
                else if (r < 760)
                {
                    const int k = rnd.nextInt ((int) keys.size());
                    h.noteOff (t, keys[(size_t) k].second, keys[(size_t) k].first);
                    keys.erase (keys.begin() + k);
                }
                else if (r < 790) h.noteOff (t, rnd.nextInt (128), 1 + rnd.nextInt (2));      // relâchement d'une note jamais jouée
                else if (r < 800) h.schedule (t, 0xA0, (uint8_t) (30 + rnd.nextInt (60)), (uint8_t) rnd.nextInt (128));
                else if (r < 810) h.schedule (t, 0xB0, 64, rnd.nextBool() ? 127 : 0);
                else
                {
                    // changement de réglage, preset, transport, bypass, désactivation, Panic, Learn, analyse
                    h.runUntil (t);
                    const int what = rnd.nextInt (24);
                    switch (what)
                    {
                        case 0:  h.set (id::mode, (float) rnd.nextInt (4)); break;
                        case 1:  h.set (id::strum, rnd.nextFloat() * 200.0f); break;
                        case 2:  h.set (id::durMode, (float) rnd.nextInt (2)); break;
                        case 3:  h.set (id::duration, (float) rnd.nextInt (10)); break;
                        case 4:  h.set (id::latch, (float) rnd.nextInt (2)); break;
                        case 5:  h.set (id::outChannel, (float) (1 + rnd.nextInt (16))); break;
                        case 6:  h.set (id::inversion, (float) rnd.nextInt (5)); h.set (id::spread, (float) rnd.nextInt (5)); break;
                        case 7:  h.set (id::maxVoices, (float) (2 + rnd.nextInt (7))); h.set (id::bass, (float) rnd.nextInt (3)); break;
                        case 8:  h.set (id::octave, (float) (rnd.nextInt (7) - 3)); h.set (id::transpose, (float) (rnd.nextInt (25) - 12)); break;
                        case 9:  h.set (id::humanTime, rnd.nextFloat() * 40.0f); h.set (id::humanVel, rnd.nextFloat() * 40.0f); break;
                        case 10: h.set (id::strumDir, (float) rnd.nextInt (4)); h.set (id::strumSync, (float) rnd.nextInt (2)); break;
                        case 11: h.set (id::scale, (float) rnd.nextInt (numScales)); h.set (id::root, (float) rnd.nextInt (12)); break;
                        case 12: h.set (id::chordType, (float) rnd.nextInt (numChordTypes)); h.set (id::fixedShape, (float) rnd.nextInt (numFixedShapes)); break;
                        case 13: actions.push_back ([&h] { h.proc->notifyPresetChange(); }); break;
                        case 14: actions.push_back ([&h, &savedState] { h.proc->setStateInformation (savedState.getData(), (int) savedState.getSize()); }); break;
                        case 15: h.head.playing = ! h.head.playing; break;
                        case 16: h.bypass = ! h.bypass; break;
                        case 17: actions.push_back ([&h] { h.proc->releaseResources(); h.proc->prepareToPlay (h.sr, h.block); }); break;
                        case 18: actions.push_back ([&h] { h.proc->requestPanic(); }); break;
                        case 19: actions.push_back ([&h, &rnd] { if (rnd.nextBool()) h.proc->startLearn(); else h.proc->cancelLearn(); h.proc->pollMessageThread(); }); break;
                        case 20: actions.push_back ([&h, &rnd] { if (rnd.nextBool()) h.proc->startAnalysis(); else h.proc->stopAnalysis(); }); break;
                        case 21: actions.push_back ([&h, &rnd] { h.proc->generatePads ((uint32_t) rnd.nextInt() | 1u); }); break;
                        case 22: h.set (id::padBase, (float) (30 + rnd.nextInt (40))); break;
                        default: h.set (id::velMode, (float) rnd.nextInt (3)); h.set (id::preview, (float) rnd.nextInt (2)); break;
                    }
                }
            }
            h.runUntil (t + 1);

            // fin : toutes les touches relâchées, Latch coupé, on laisse finir les durées fixes et les strums
            for (auto& k : keys) h.noteOff (h.rendered + 5, k.second, k.first);
            h.run (64);
            h.set (id::latch, 0);
            h.run ((int64_t) (sr * 6.0));

            const auto s = h.sim();
            const bool ok = s.sounding() == 0 && s.doubleOn == 0 && s.badData == 0 && h.formatErrors == 0
                            && h.proc->getEngine().numSounding() == 0;
            allOk = allOk && ok;
            totalEvents += eventsPerConfig;
            totalOns += s.ons;
            check (ok, juce::String (sr / 1000.0, 1) + " kHz, blocs de " + juce::String (bs),
                   juce::String (eventsPerConfig) + u8 (" \xc3\xa9v\xc3\xa9nements, ") + juce::String (s.ons) + " note-on / " + juce::String (s.offs) + " note-off ; restantes "
                   + juce::String (s.sounding()) + ", note-on en double " + juce::String (s.doubleOn) + ", note-off orphelins " + juce::String (s.orphanOff)
                   + ", erreurs de format " + juce::String (h.formatErrors));
        }
    check (allOk, "Total", juce::String (totalEvents) + u8 (" \xc3\xa9v\xc3\xa9nements au hasard, ") + juce::String (totalOns) + u8 (" notes jou\xc3\xa9" "es, aucune note bloqu\xc3\xa9" "e"));
}

//==============================================================================
// 10. État, Learn, export MIDI
//==============================================================================
void testState()
{
    section (u8 ("\xc3\x89tat : sauvegarde / rechargement, Learn, export MIDI"));
    {
        Harness a;
        juce::Random rnd (99);
        for (auto* p : a.proc->getParameters())
            p->setValueNotifyingHost (rnd.nextFloat());
        Engine::PadSet ps = Harmony::generatePads (3, phrygian, 1234);
        ps.pads[2].octave = -2; ps.pads[5].inversion = 1;
        a.proc->setPads (ps);
        Engine::LearnedShape l; l.count = 5; l.iv[0] = 0; l.iv[1] = 3; l.iv[2] = 10; l.iv[3] = 14; l.iv[4] = 19;
        a.proc->setLearned (l);
        juce::MemoryBlock mb;
        a.proc->getStateInformation (mb);

        Harness b;
        b.proc->setStateInformation (mb.getData(), (int) mb.getSize());
        int diff = 0;
        auto pa = a.proc->getParameters(), pb = b.proc->getParameters();
        juce::String which;
        for (int i = 0; i < pa.size(); ++i)
            if (auto* ba = dynamic_cast<juce::AudioParameterBool*> (pa[i]); ba != nullptr
                    ? ba->get() != dynamic_cast<juce::AudioParameterBool*> (pb[i])->get()
                    : std::abs (pa[i]->getValue() - pb[i]->getValue()) > 1.0e-4f)
            {
                ++diff;
                which << pa[i]->getName (40) << " " << juce::String (pa[i]->getValue(), 4) << " / " << juce::String (pb[i]->getValue(), 4) << " ; ";
            }
        const bool padsOk = b.proc->getPads() == ps, learnedOk = b.proc->getLearned() == l;
        check (diff == 0 && padsOk && learnedOk, u8 ("Sauvegarde puis rechargement"),
               juce::String (pa.size()) + u8 (" param\xc3\xa8tres (") + juce::String (diff) + u8 (" diff\xc3\xa9rences), 8 pads ") + (padsOk ? "identiques" : "DIFFERENTS")
               + u8 (", accord appris ") + (learnedOk ? "identique" : "DIFFERENT") + (which.isEmpty() ? juce::String() : " : " + which));
    }
    {
        Harness h;
        setNeutral (h);
        h.proc->startLearn();
        h.run (512);
        for (int n : { 48, 52, 55, 58 }) h.noteOn (h.rendered + 100 + n, n);
        h.run (4000);
        const int heard = h.sim().sounding();   // pendant l'apprentissage, on entend les notes jouées
        for (int n : { 48, 52, 55, 58 }) h.noteOff (h.rendered + 100 + n, n);
        h.run (4000);
        h.proc->pollMessageThread();
        const auto& l = h.proc->getLearned();
        const bool shapeOk = l.count == 4 && l.iv[0] == 0 && l.iv[1] == 4 && l.iv[2] == 7 && l.iv[3] == 10;
        const bool modeOk = (int) h.proc->apvts.getRawParameterValue (id::mode)->load() == Engine::modeFixed
                         && (int) h.proc->apvts.getRawParameterValue (id::fixedShape)->load() == fixedLearned;
        h.run (64);
        const auto mark = h.rendered + 50;
        h.noteOn (mark, 62);       // D -> D F# A C
        h.run (2000);
        std::set<int> got;
        for (const auto& e : h.onsIn (mark, mark + 1)) got.insert (e.d[1]);
        check (shapeOk && modeOk && heard == 4 && got == std::set<int> { 62, 66, 69, 72 } && ! h.proc->isLearning(), "Learn",
               u8 ("C E G Bb jou\xc3\xa9s ensemble -> forme 0 4 7 10, mode Accord fixe / Appris ; D jou\xc3\xa9 ensuite -> ") + listNotes ({ got.begin(), got.end() }));
    }
    {
        Harness h;
        setNeutral (h);
        h.set (id::scale, naturalMinor); h.set (id::root, 9); h.set (id::inversion, 4);
        h.proc->setPads (Harmony::generatePads (Harmony::styleDeep, naturalMinor, 5));
        auto dir = juce::File::getCurrentWorkingDirectory().getChildFile ("renders");
        dir.createDirectory();
        bool allOk = true;
        juce::String detail;
        for (int bars : { 4, 8 })
        {
            const auto f = dir.getChildFile ("export_" + juce::String (bars) + "mesures.mid");
            const bool written = h.proc->exportMidi (f, bars);
            juce::MidiFile mf;
            juce::FileInputStream in (f);
            const bool read = in.openedOk() && mf.readFrom (in);
            // accords attendus : les 8 pads joués un par un dans le plugin
            std::vector<std::set<int>> expected;
            {
                Harness p;
                setNeutral (p);
                p.set (id::scale, naturalMinor); p.set (id::root, 9); p.set (id::inversion, 4); p.set (id::mode, Engine::modePads);
                p.proc->setPads (h.proc->getPads());
                const int base = (int) p.proc->apvts.getRawParameterValue (id::padBase)->load();
                for (int i = 0; i < 8; ++i)
                {
                    const int64_t at = 1000 + i * 4000;
                    p.noteOn (at, base + i);
                    p.noteOff (at + 3000, base + i);
                    p.runUntil (at + 3500);
                    std::set<int> c;
                    for (const auto& e : p.onsIn (at, at + 1)) c.insert (e.d[1]);
                    expected.push_back (c);
                }
            }
            // regroupe les notes par instant de départ
            std::map<int, std::set<int>> chords;
            double lastEnd = 0.0;
            if (read && mf.getNumTracks() > 0)
            {
                const auto* tr = mf.getTrack (0);
                for (const auto* ev : *tr)
                {
                    if (ev->message.isNoteOn()) chords[(int) ev->message.getTimeStamp()].insert (ev->message.getNoteNumber());
                    if (ev->message.isNoteOff()) lastEnd = std::max (lastEnd, ev->message.getTimeStamp());
                }
            }
            bool same = (int) chords.size() == 8;
            int k = 0;
            const int slot = bars == 8 ? 4 * 960 : 2 * 960;
            for (auto& [time, notes] : chords)
            {
                same = same && time == k * slot && k < (int) expected.size() && notes == expected[(size_t) k];
                ++k;
            }
            const bool lengthOk = std::abs (lastEnd - bars * 4.0 * 960.0) < 1.0;
            allOk = allOk && written && read && same && lengthOk;
            detail << bars << " mesures : " << (int) chords.size() << " accords " << (same ? "identiques au plugin" : "DIFFERENTS")
                   << ", fin a " << juce::String (lastEnd / 960.0 / 4.0, 2) << " mes. ; ";
        }
        check (allOk, "Export MIDI (.mid relu)", detail);
    }
}

//==============================================================================
// 11. Performance et allocations
//==============================================================================
void testPerformance()
{
    section ("Performance et allocations (bloc de 512 a 48 kHz)");
    auto scenario = [] (Harness& h, bool preview)
    {
        h.set (id::maxVoices, 8); h.set (id::chordType, ctThirteenth); h.set (id::bass, 1);
        h.set (id::strum, 25.0f); h.set (id::humanTime, 15.0f); h.set (id::humanVel, 12.0f); h.set (id::inversion, 4);
        h.set (id::preview, preview ? 1.0f : 0.0f);
        h.head.playing = true;
        h.head.bpm = 128.0;
    };
    auto feed = [] (Harness& h, juce::Random& rnd)
    {
        // un accord toutes les double-croches environ, relâché un peu plus tard
        const int64_t t = h.rendered + rnd.nextInt (512);
        const int note = 48 + rnd.nextInt (24);
        h.noteOn (t, note, 100);
        h.noteOff (t + 2000 + rnd.nextInt (8000), note);
    };

    for (bool preview : { false, true })
    {
        Harness h (48000.0, 512);
        scenario (h, preview);
        juce::Random rnd (5);
        for (int i = 0; i < 200; ++i) { feed (h, rnd); h.run (512); }   // chauffe
        h.audio.setSize (2, 512, false, false, true);

        std::vector<double> times;
        const int blocks = 20000;
        for (int i = 0; i < blocks; ++i)
        {
            if (i % 3 == 0) feed (h, rnd);
            // même chose que runUntil, chronométré
            h.midi.clear();
            while (! h.input.empty() && h.input.front().t < h.rendered + 512)
            {
                h.midi.addEvent (h.input.front().d, 3, (int) (h.input.front().t - h.rendered));
                h.input.pop_front();
            }
            const auto t0 = std::chrono::steady_clock::now();
            h.proc->processBlock (h.audio, h.midi);
            times.push_back (std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now() - t0).count());
            h.rendered += 512;
        }
        std::sort (times.begin(), times.end());
        double mean = 0; for (double x : times) mean += x; mean /= (double) times.size();
        const double worst = times.back(), p99 = times[(size_t) (times.size() * 0.99)];
        const double budget = 512.0 / 48000.0 * 1.0e6;
        const double pct = 100.0 * mean / budget;
        if (! preview)
            check (pct < 0.5, "processBlock (MIDI seul : 8 voix + basse, strum, humanize, voice leading)",
                   u8 ("moyenne ") + juce::String (mean, 2) + u8 (" \xc2\xb5s (") + juce::String (pct, 3) + u8 (" % d'un c\xc5\x93ur), 99e centile ")
                   + juce::String (p99, 2) + u8 (" \xc2\xb5s, pire ") + juce::String (worst, 1) + u8 (" \xc2\xb5s ; objectif < 0,5 %"));
        else
            check (true, "processBlock avec la voix Preview (information)",
                   u8 ("moyenne ") + juce::String (mean, 2) + u8 (" \xc2\xb5s (") + juce::String (pct, 2) + u8 (" % d'un c\xc5\x93ur), pire ") + juce::String (worst, 1) + u8 (" \xc2\xb5s"));
    }

    // allocations : aucune dans processBlock (notes, strum, humanize, preview, Panic, bypass)
    {
        Harness h (48000.0, 512);
        scenario (h, true);
        juce::Random rnd (11);
        for (int i = 0; i < 300; ++i) { feed (h, rnd); h.run (512); }
        h.audio.setSize (2, 512, false, false, true);
        long allocs = 0;
        for (int i = 0; i < 3000; ++i)
        {
            if (i % 2 == 0) feed (h, rnd);
            if (i % 500 == 250) h.proc->requestPanic();
            h.bypass = (i % 700) > 650;
            h.midi.clear();
            while (! h.input.empty() && h.input.front().t < h.rendered + 512)
            {
                h.midi.addEvent (h.input.front().d, 3, (int) (h.input.front().t - h.rendered));
                h.input.pop_front();
            }
            AllocCounter::count = 0;
            AllocCounter::counting = true;
            if (h.bypass) h.proc->processBlockBypassed (h.audio, h.midi);
            else          h.proc->processBlock (h.audio, h.midi);
            AllocCounter::counting = false;
            allocs += AllocCounter::count.load();
            h.rendered += 512;
        }
        check (allocs == 0, "Aucune allocation dans processBlock", juce::String (allocs) + " allocation(s) sur 3000 blocs (accords, strum, humanize, Preview, Panic, bypass)");
    }
}

//==============================================================================
// 12. Interface : captures de chaque onglet, temps de dessin, allocations, repos
//==============================================================================
void savePng (const juce::Image& img, const juce::File& f)
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

void snapshots (const juce::File& dir)
{
    section ("Interface : captures, temps de dessin, allocations, repos");
    dir.createDirectory();
    Harness h;
    h.proc->presets.loadFactoryPreset (0);          // Warm Current : D dorien, m9
    h.run (512);
    std::unique_ptr<juce::AudioProcessorEditor> edBase (h.proc->createEditor());
    auto* ed = dynamic_cast<ConfluenceAudioProcessorEditor*> (edBase.get());
    ed->setVisible (true);
    auto& content = ed->getContent();
    const int perTick = 800;   // une image toutes les ~16 ms

    // un accord tenu : les bulles montent, le nom flotte, le tourbillon montre les enchaînements
    h.noteOn (h.rendered + 10, 50);
    for (int i = 0; i < 70; ++i) { h.run (perTick); ed->tickForTests(); }

    const char* tabNames[] = { "chord", "voicing", "play" };
    for (int tab = 0; tab < 3; ++tab)
    {
        content.setTab (tab);
        for (int i = 0; i < 4; ++i) { h.run (perTick); ed->tickForTests(); }
        for (float scale : { 1.0f, 2.0f })
        {
            const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, scale);
            const auto f = dir.getChildFile ("tab_" + juce::String (tabNames[tab]) + "_" + juce::String ((int) scale) + "x.png");
            savePng (img, f);
            check (img.isValid(), f.getFileName(), juce::String (img.getWidth()) + " x " + juce::String (img.getHeight()));
        }
    }

    // pad joué à la souris (en mode Gamme : le pad joue quand même son accord)
    {
        content.setTab (0);
        h.noteOff (h.rendered + 10, 50);
        h.proc->playFromUi (2, 100, true);
        for (int i = 0; i < 30; ++i) { h.run (perTick); ed->tickForTests(); }
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        savePng (img, dir.getChildFile ("pad_joue_2x.png"));
        const bool sounding = h.proc->anySounding() && h.proc->activePad.load() == 2;
        h.proc->playFromUi (2, 0, true);
        for (int i = 0; i < 4; ++i) { h.run (perTick); ed->tickForTests(); }
        check (img.isValid() && sounding && ! h.proc->anySounding(), "Pad 3 joue a la souris", "l'accord du pad sonne pendant le clic, s'arrete au relachement");
    }

    // clavier joué à la souris : la note passe par Confluence
    {
        auto& sea = content.getSea();
        const auto k = sea.getKeyboardArea();
        juce::Point<float> pt;
        int note = -1;
        for (float x = k.getX() + 2.0f; x < k.getRight(); x += 2.0f)
            if (sea.noteAt ({ x, k.getBottom() - 10.0f }) == 57) { pt = { x, k.getBottom() - 10.0f }; note = 57; break; }
        h.proc->playFromUi (note, 100, false);
        for (int i = 0; i < 6; ++i) { h.run (perTick); ed->tickForTests(); }
        const bool on = h.proc->isInputHeld (57) && h.proc->anySounding();
        h.proc->playFromUi (note, 0, false);
        for (int i = 0; i < 4; ++i) { h.run (perTick); ed->tickForTests(); }
        check (note == 57 && on && ! h.proc->anySounding(), "Clavier de l'interface (A2 clique)", "la touche joue l'accord par Confluence, puis le relache");
    }

    // panneau d'enregistrement de preset, dans la fenêtre du plugin
    {
        content.getPresetBar().askSaveName();
        for (int i = 0; i < 3; ++i) { h.run (perTick); ed->tickForTests(); }
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        savePng (img, dir.getChildFile ("save_panel_2x.png"));
        check (img.isValid() && content.getPresetBar().isSavePanelOpen(), "save_panel_2x.png", "panneau Enregistrer affiche dans la fenetre");
        content.getPresetBar().closeSavePanel();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
    }

    // panneau « À propos » : les mentions qu'une interface doit afficher sous la licence AGPL
    {
        content.getInfoButton().triggerClick();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        auto* about = content.getAboutPanel();
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        savePng (img, dir.getChildFile ("about_2x.png"));
        const auto text = about != nullptr ? about->getAllText() : juce::String();
        juce::StringArray missing;
        for (auto need : { "GRVTY Records", "Jasperr", "GNU Affero General Public License", "version 3", "SANS AUCUNE GARANTIE", "redistribuer",
                           "LICENSE", "agpl-3.0", "Code source", "ne collecte aucune donn", "THIRD-PARTY-NOTICES",
                           "Steinberg", "Ableton AG", "NOTICE", "dans les limites permises par la loi", JucePlugin_VersionString })
            if (! text.contains (juce::String::fromUTF8 (need))) missing.add (need);
        const bool shown = about != nullptr && about->isVisible() && content.getInfoButton().isVisible();
        if (about != nullptr && ! about->hasVstLogo()) missing.add ("logo VST Compatible");
        if (about != nullptr) about->close();
        juce::MessageManager::getInstance()->runDispatchLoopUntil (50);
        check (shown && missing.isEmpty() && ! content.isAboutOpen(), u8 ("Panneau \xc2\xab \xc3\x80 propos \xc2\xbb (about_2x.png)"),
               ! shown ? juce::String ("le panneau ne s'ouvre pas")
                       : ! missing.isEmpty() ? "manque : " + missing.joinIntoString (", ")
                       : content.isAboutOpen() ? juce::String ("ne se referme pas")
                       : u8 ("copyright, licence AGPL v3, absence de garantie, droit de redistribuer, o\xc3\xb9 lire la licence, "
                             "code source, confidentialit\xc3\xa9, composants tiers, marques ; se referme"));
    }

    // redimensionnement : les proportions sont gardées
    {
        ed->setSize (ConfluenceContent::designW * 3 / 2, ConfluenceContent::designH * 3 / 2);
        const auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
        savePng (img, dir.getChildFile ("resize_150.png"));
        check (ed->getWidth() == 1500 && ed->getHeight() == 705, "Redimensionnement 150 %", juce::String (ed->getWidth()) + " x " + juce::String (ed->getHeight()));
        ed->setSize (ConfluenceContent::designW, ConfluenceContent::designH);
    }

    // temps de dessin et allocations pendant le jeu (le cas le plus chargé) :
    //   image pleine : fond recalculé + toute la fenêtre (8 fois par seconde)
    //   image partielle : la mer, le tourbillon, le logo (les autres images)
    content.setTab (0);
    juce::Random rnd (4);
    int held = -1;
    juce::Image target (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
    juce::RectangleList<int> partial;
    for (auto r : { content.getSeaBounds(), content.getWhirlBounds(), juce::Rectangle<int> (14, 10, 36, 26) })
        partial.add (r);

    auto measure = [&] (bool full, double& meanMs, double& p95, double& allocsPerFrame)
    {
        std::vector<double> times;
        long allocs = 0;
        for (int i = 0; i < 70; ++i)
        {
            if (i % 12 == 0)
            {
                if (held >= 0) h.noteOff (h.rendered + 5, held);
                held = 45 + rnd.nextInt (20);
                h.noteOn (h.rendered + 10, held);
            }
            h.run (perTick);
            ed->tickForTests();
            if (full) content.invalidateBackground();
            juce::Graphics g (target);
            if (! full) g.reduceClipRegion (partial);
            AllocCounter::count = 0;
            AllocCounter::counting = i >= 10;
            const auto t0 = std::chrono::steady_clock::now();
            ed->paintEntireComponent (g, true);
            const double ms = std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
            AllocCounter::counting = false;
            if (i >= 10) { times.push_back (ms); allocs += AllocCounter::count.load(); }
        }
        std::sort (times.begin(), times.end());
        meanMs = 0; for (auto t : times) meanMs += t; meanMs /= (double) times.size();
        p95 = times[(size_t) (times.size() * 0.95)];
        allocsPerFrame = (double) allocs / (double) times.size();
    };

    double fullMean, fullP95, fullAllocs, partMean, partP95, partAllocs;
    measure (true, fullMean, fullP95, fullAllocs);
    measure (false, partMean, partP95, partAllocs);
    // sur les machines partagées de la compilation automatique (CF_CI=1), le temps mesuré dépend des voisins :
    // le seuil y est doublé ; l'objectif de 16 ms reste celui du banc de tests de référence
    const bool sharedMachine = std::getenv ("CF_CI") != nullptr;
    const double budget = sharedMachine ? 32.0 : 16.0;
    const juce::String budgetNote = sharedMachine ? u8 (" ; machine partag\xc3\xa9" "e : seuil 32 ms") : juce::String();
    check (fullP95 < budget, "Image pleine (fond recalcule, 1x, rendu logiciel)",
           "moyenne " + juce::String (fullMean, 2) + " ms, 95e centile " + juce::String (fullP95, 2) + " ms (" + juce::String (fullAllocs, 0) + u8 (" allocations du moteur de rendu)") + budgetNote);
    check (partP95 < budget, "Image partielle (mer, bulles, tourbillon)",
           "moyenne " + juce::String (partMean, 2) + " ms, 95e centile " + juce::String (partP95, 2) + " ms (" + juce::String (partAllocs, 0) + u8 (" allocations du moteur de rendu)") + budgetNote);
    {
        // notre code seul (contexte qui ne dessine rien) pendant l'animation : accord qui sonne, bulles, pad joué,
        // tourbillon, logo. Les allocations du moteur de rendu de JUCE (tables de bords, dégradés) ne sont pas comptées.
        // Quand un nouvel accord arrive, son nom et le tourbillon sont remis en page une fois (JUCE alloue pour mettre
        // un texte en page) puis gardés en image : ces images-là sont comptées à part.
        NullContext nc;
        long steadyAllocs = 0, changeAllocs = 0;
        int steadyFrames = 0, changeFrames = 0;
        h.proc->playFromUi (h.proc->readChordSettings().padBase + 2, 100, true);   // un pad animé
        for (int i = 0; i < 100; ++i)
        {
            if (i % 12 == 0)
            {
                if (held >= 0) h.noteOff (h.rendered + 5, held);
                held = 45 + rnd.nextInt (20);
                h.noteOn (h.rendered + 10, held);
            }
            const auto before = h.proc->getLastChord().counter;
            h.run (perTick);
            const bool chordChanged = h.proc->getLastChord().counter != before;
            ed->tickForTests();
            if (content.isBackgroundDirty())   // fond (8 images/s) : rendu à part, dans sa propre image
            {
                juce::Graphics g (target);
                content.paint (g);
            }
            juce::Graphics g (nc);
            AllocCounter::count = 0;
            AllocCounter::counting = i >= 20;
            AllocCounter::trace = i >= 20 && ! chordChanged && juce::String (std::getenv ("CF_ALLOC_TRACE")) == "paint";
            paintLive (content, g);
            AllocCounter::trace = false;
            AllocCounter::counting = false;
            if (i < 20) continue;
            if (chordChanged) { changeAllocs += AllocCounter::count.load(); ++changeFrames; }
            else              { steadyAllocs += AllocCounter::count.load(); ++steadyFrames; }
        }
        h.proc->playFromUi (h.proc->readChordSettings().padBase + 2, 0, true);
        h.run (perTick * 4);
        check (steadyAllocs == 0 && steadyFrames > 50, u8 ("Aucune allocation dans paint() pendant l'animation (notre code)"),
               juce::String (steadyAllocs) + u8 (" allocation sur ") + juce::String (steadyFrames) + u8 (" images d'animation ; ")
                   + juce::String (juce::roundToInt ((double) changeAllocs / juce::jmax (1, changeFrames))) + u8 (" par image o\xc3\xb9 un nouvel accord s'affiche (")
                   + juce::String (changeFrames) + u8 (" images : nom et tourbillon remis en page une fois)"));
    }
    {
        Ui::font (9.0f, true, 0.12f);    // première demande : la police est créée une fois
        AllocCounter::count = 0;
        AllocCounter::counting = true;
        Ui::font (9.0f, true, 0.12f);
        Ui::scratchPath (0);
        AllocCounter::counting = false;
        check (AllocCounter::count.load() == 0, "Polices et chemins mis en cache", juce::String ((int) AllocCounter::count.load()) + " allocation (attendu 0)");
    }
    const double playLoad = (8.0 * fullMean + 52.0 * partMean) / 10.0;
    const double idleLoad = 4.0 * fullMean / 10.0;
    check (true, "Charge de l'interface (estimation, 1x)", "en jeu ~" + juce::String (playLoad, 1) + " % d'un coeur, au repos ~" + juce::String (idleLoad, 1) + " %");

    // au repos (plus de note, bulles éteintes, souris dehors) : seul le fond bouge, 4 fois par seconde
    if (held >= 0) h.noteOff (h.rendered + 5, held);
    h.run (perTick);
    bool active = true;
    for (int i = 0; i < 600 && active; ++i) { h.run (perTick); juce::Thread::sleep (8); active = content.tick(); }
    check (! active, "Passage au ralenti au repos", active ? "reste actif (" + content.getActivityReason() + ")"
                                                         : juce::String ("fond a 4 images/s, rien d'autre ne bouge"));
}

/** Profil du dessin : coût de chaque partie de l'interface (pour savoir quoi optimiser). */

void uiProfile()
{
    {
        // détail du recalcul du fond (8 fois par seconde en jeu)
        auto ms = [] (std::function<void()> fn)
        {
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < 20; ++i) fn();
            return std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count() / 20.0;
        };
        const int W = ConfluenceContent::designW, H = ConfluenceContent::designH;
        juce::Image low (juce::Image::RGB, W / 4, H / 4, false), full (juce::Image::RGB, W, H, false), over (juce::Image::ARGB, W, H, true);
        { juce::Graphics g (over); Ui::drawGlassPanel (g, { 10.0f, 40.0f, 180.0f, 420.0f }); }
        const double tWater = ms ([&] { juce::Graphics g (low); g.addTransform (juce::AffineTransform::scale (0.25f)); Ui::drawWaterBackground (g, { 0.0f, 0.0f, (float) W, (float) H }, false); });
        const double tUp = ms ([&] { juce::Graphics g (full); g.setImageResamplingQuality (juce::Graphics::mediumResamplingQuality);
                                     g.drawImageTransformed (low, juce::AffineTransform::scale ((float) W / (float) low.getWidth(), (float) H / (float) low.getHeight())); });
        const double tPart = ms ([&] { juce::Graphics g (full); Ui::drawParticles (g, { 0.0f, 0.0f, (float) W, (float) H }); });
        const double tOver = ms ([&] { juce::Graphics g (full); g.drawImageAt (over, 0, 0); });
        std::printf ("  Fond : eau 1/4 %.2f ms, agrandissement %.2f ms, particules %.2f ms, calque fixe %.2f ms\n", tWater, tUp, tPart, tOver);
    }
    Harness h;
    h.proc->presets.loadFactoryPreset (0);
    std::unique_ptr<juce::AudioProcessorEditor> edBase (h.proc->createEditor());
    auto* ed = dynamic_cast<ConfluenceAudioProcessorEditor*> (edBase.get());
    ed->setVisible (true);
    auto& content = ed->getContent();
    h.noteOn (h.rendered + 10, 50);
    for (int i = 0; i < 40; ++i) { h.run (800); ed->tickForTests(); }
    juce::Image target (juce::Image::ARGB, ed->getWidth(), ed->getHeight(), true);
    auto timeIt = [&] (std::function<void (juce::Graphics&)> fn, int reps = 20)
    {
        double sum = 0; long allocs = 0;
        for (int i = 0; i < reps; ++i)
        {
            h.run (800); ed->tickForTests();
            juce::Graphics g (target);
            AllocCounter::count = 0; AllocCounter::counting = true;
            const auto t0 = std::chrono::steady_clock::now();
            fn (g);
            sum += std::chrono::duration<double, std::milli> (std::chrono::steady_clock::now() - t0).count();
            AllocCounter::counting = false; allocs += AllocCounter::count.load();
        }
        return std::pair<double, double> { sum / reps, (double) allocs / reps };
    };
    auto rep = [] (const char* n, std::pair<double, double> r) { std::printf ("  %-36s %6.2f ms  %7.0f alloc\n", n, r.first, r.second); };
    {
        // allocations de notre code seul (contexte qui ne dessine rien), image animée après image animée
        NullContext nc;
        long allocs = 0;
        for (int i = 0; i < 20; ++i)
        {
            h.run (800); ed->tickForTests();
            juce::Graphics g (nc);
            AllocCounter::count = 0; AllocCounter::counting = true;
            content.paint (g);
            for (auto* c : content.getChildren())
                if (c->isVisible() && c->getCachedComponentImage() == nullptr) { g.setOrigin (c->getPosition()); c->paintEntireComponent (g, true); g.setOrigin (-c->getPosition()); }
            AllocCounter::counting = false; allocs += AllocCounter::count.load();
        }
        std::printf ("  %-36s %7.1f alloc par image\n", "Notre code seul (sans rendu)", (double) allocs / 20.0);
        for (auto* c : content.getChildren())
        {
            if (! c->isVisible() || c->getCachedComponentImage() != nullptr) continue;
            const char* only = std::getenv ("CF_ALLOC_TRACE");
            AllocCounter::trace = only != nullptr && juce::String (typeid (*c).name()).contains (only);
            long a = 0;
            for (int i = 0; i < 10; ++i)
            {
                h.run (800); ed->tickForTests();
                juce::Graphics g (nc);
                AllocCounter::count = 0; AllocCounter::counting = true;
                c->paintEntireComponent (g, true);
                AllocCounter::counting = false; a += AllocCounter::count.load();
            }
            AllocCounter::trace = false;
            if (a > 0) std::printf ("    %-34s %7.1f alloc\n", typeid (*c).name(), (double) a / 10.0);
        }
    }
    rep ("Fenetre complete", timeIt ([&] (juce::Graphics& g) { ed->paintEntireComponent (g, true); }));
    rep ("Fond recalcule + contenu seul", timeIt ([&] (juce::Graphics& g) { content.invalidateBackground(); content.paint (g); }));
    rep ("Contenu seul (fond en cache)", timeIt ([&] (juce::Graphics& g) { content.paint (g); }));
    for (auto* c : content.getChildren())
    {
        if (! c->isVisible()) continue;
        auto r = timeIt ([&] (juce::Graphics& g) { g.setOrigin (c->getPosition()); c->paintEntireComponent (g, true); }, 10);
        if (r.first > 0.15)
            std::printf ("  %-36s %6.2f ms  %7.0f alloc  (%d x %d)\n", typeid (*c).name(), r.first, r.second, c->getWidth(), c->getHeight());
    }
}

//==============================================================================
// 13. Maquettes d'interface (à choisir avant la phase 3)
//==============================================================================
void mockups (const juce::File& dir)
{
    section ("Maquettes d'interface");
    dir.createDirectory();
    Harness h;
    h.set (id::root, 9); h.set (id::scale, naturalMinor); h.set (id::chordType, ctNinth); h.set (id::maxVoices, 5);
    h.set (id::bass, 1); h.set (id::inversion, 4); h.set (id::style, 0); h.set (id::strum, 12.0f); h.set (id::humanTime, 6.0f);
    h.set (id::spread, 1); h.set (id::lowNote, 52); h.set (id::highNote, 80);
    h.proc->setPads (Harmony::generatePads (Harmony::styleDeep, naturalMinor, 11));

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
    { Mock::LayoutB b (*h.proc); save (b, "maquette_B_tourbillon"); }
    { Mock::LayoutC c (*h.proc); save (c, "maquette_C_courant"); }
}
} // namespace


//==============================================================================
// Presets : rendus de 4 mesures (.mid de la sortie MIDI + WAV avec la voix Preview)
//==============================================================================
/** Loudness intégrée ITU-R BS.1770-4 (pondération K, portes -70 LUFS et -10 LU), coefficients pour 48 kHz. */
double integratedLufs (const juce::AudioBuffer<float>& b)
{
    const int n = b.getNumSamples();
    std::vector<double> sq ((size_t) n, 0.0);
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        double x1 = 0, x2 = 0, s1 = 0, s2 = 0, y1 = 0, y2 = 0;
        const auto* d = b.getReadPointer (ch);
        for (int i = 0; i < n; ++i)
        {
            const double x = d[i];
            const double s = 1.53512485958697 * x - 2.69169618940638 * x1 + 1.19839281085285 * x2 + 1.69065929318241 * s1 - 0.73248077421585 * s2;
            x2 = x1; x1 = x;
            const double y = s - 2.0 * s1 + s2 + 1.99004745483398 * y1 - 0.99007225036621 * y2;
            s2 = s1; s1 = s;
            y2 = y1; y1 = y;
            sq[(size_t) i] += y * y;
        }
    }
    const int blockLen = 19200, hop = 4800;   // 400 ms, recouvrement 75 %
    std::vector<double> z;
    for (int start = 0; start + blockLen <= n; start += hop)
    {
        double acc = 0.0;
        for (int i = start; i < start + blockLen; ++i) acc += sq[(size_t) i];
        z.push_back (acc / blockLen);
    }
    auto loud = [] (double v) { return -0.691 + 10.0 * std::log10 (std::max (v, 1.0e-12)); };
    double sum = 0.0; int cnt = 0;
    for (double v : z) if (loud (v) > -70.0) { sum += v; ++cnt; }
    if (cnt == 0) return -100.0;
    const double rel = loud (sum / cnt) - 10.0;
    sum = 0.0; cnt = 0;
    for (double v : z) if (loud (v) > -70.0 && loud (v) > rel) { sum += v; ++cnt; }
    return cnt == 0 ? -100.0 : loud (sum / cnt);
}

struct DemoNote { int note; double start, len; };

std::vector<DemoNote> parseDemo (const char* text)
{
    std::vector<DemoNote> v;
    for (const auto& tok : juce::StringArray::fromTokens (juce::String::fromUTF8 (text), " ", ""))
    {
        auto parts = juce::StringArray::fromTokens (tok, ":", "");
        if (parts.size() != 3) continue;
        const int note = Params::parseNote (parts[0]);
        if (note >= 0) v.push_back ({ note, parts[1].getDoubleValue(), parts[2].getDoubleValue() });
    }
    return v;
}

void presetRenders (const juce::File& dir)
{
    section (u8 ("Presets d'usine : rendus de 4 mesures (.mid sortie MIDI + WAV voix Preview), pic < -1 dBFS"));
    dir.createDirectory();
    const auto& data = getFactoryPresetData();
    juce::String table = "\n| Preset | Banque | BPM | Notes jouees | Notes MIDI sorties | Preview : pic / LUFS | WAV : pic / LUFS |\n|---|---|---|---|---|---|---|\n";
    for (size_t i = 0; i < data.size(); ++i)
    {
        const auto& fp = data[i];
        const double sr = 48000.0;
        Harness h (sr, 512);
        h.proc->presets.loadFactoryPreset ((int) i);
        h.proc->pollMessageThread();
        h.set (id::preview, 1.0f);   // voix Preview (le preset n'y touche pas : on l'active pour le rendu)
        h.head.bpm = fp.bpm;
        h.head.playing = true;
        h.head.ppq = 0.0;
        const double spb = sr * 60.0 / fp.bpm;
        auto at = [spb] (double beats) { return (int64_t) std::llround (beats * spb); };

        // la phrase est jouée deux fois ; on garde la seconde : la queue de la 4e mesure retombe sur la 1re,
        // le WAV boucle donc sans coupure comme un clip d'Ableton
        const auto demo = parseDemo (fp.demo);
        for (int pass = 0; pass < 2; ++pass)
            for (const auto& d : demo)
            {
                const double s0 = d.start + 16.0 * pass;
                h.noteOn (at (s0), d.note, 100);
                h.noteOff (at (s0 + d.len) - 1, d.note);
            }

        const int64_t from = at (16.0), to = at (32.0);
        juce::AudioBuffer<float> wav (2, (int) (to - from));
        wav.clear();
        h.afterBlock = [&] (const juce::AudioBuffer<float>& a, int64_t pos)
        {
            for (int k = 0; k < a.getNumSamples(); ++k)
            {
                const int64_t t = pos + k;
                if (t < from || t >= to) continue;
                for (int ch = 0; ch < 2; ++ch) wav.setSample (ch, (int) (t - from), a.getSample (juce::jmin (ch, a.getNumChannels() - 1), k));
            }
        };
        h.runUntil (to);
        h.proc->requestPanic();
        h.run (2048);

        // --- .mid : notes sorties pendant la seconde passe, à 960 ticks par noire
        juce::MidiMessageSequence seq;
        seq.addEvent (juce::MidiMessage::textMetaEvent (3, juce::String::fromUTF8 (fp.name)), 0.0);
        seq.addEvent (juce::MidiMessage::tempoMetaEvent ((int) std::lround (60.0e6 / fp.bpm)), 0.0);
        seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);
        std::map<std::pair<int, int>, double> open;
        int outNotes = 0;
        auto tick = [&] (int64_t t) { return juce::jlimit (0.0, 16.0 * 960.0, (double) (t - from) / spb * 960.0); };
        for (const auto& e : h.output)
        {
            const auto key = std::make_pair (e.channel(), (int) e.d[1]);
            if (e.isOn() && e.t >= from && e.t < to)
            {
                open[key] = tick (e.t);
                seq.addEvent (juce::MidiMessage::noteOn (e.channel(), e.d[1], (juce::uint8) e.d[2]), tick (e.t));
                ++outNotes;
            }
            else if (e.isOff() && open.count (key) > 0)
            {
                seq.addEvent (juce::MidiMessage::noteOff (e.channel(), e.d[1]), juce::jmax (open[key] + 1.0, tick (e.t)));
                open.erase (key);
            }
        }
        seq.addEvent (juce::MidiMessage::endOfTrack(), 16.0 * 960.0);
        seq.sort();
        seq.updateMatchedPairs();
        juce::MidiFile mf;
        mf.setTicksPerQuarterNote (960);
        mf.addTrack (seq);

        const auto base = juce::String (i + 1).paddedLeft ('0', 2) + "_" + juce::String::fromUTF8 (fp.name).replaceCharacter (' ', '_');
        const auto midFile = dir.getChildFile (base + ".mid");
        const auto wavFile = dir.getChildFile (base + ".wav");
        midFile.deleteFile();
        wavFile.deleteFile();
        bool written = false;
        {
            juce::FileOutputStream os (midFile);
            written = os.openedOk() && mf.writeTo (os);
        }
        // niveau de la voix Preview tel quel (réglage par défaut -12 dB), puis rendu ramené à -18 LUFS
        // (pic plafonné à -1,5 dBFS) pour comparer les presets à l'oreille au même volume
        auto peakOf = [] (const juce::AudioBuffer<float>& b)
        {
            const float pk = juce::jmax (b.getMagnitude (0, 0, b.getNumSamples()), b.getMagnitude (1, 0, b.getNumSamples()));
            return juce::Decibels::gainToDecibels ((double) pk, -200.0);
        };
        const double rawPeak = peakOf (wav), rawLufs = integratedLufs (wav);
        const double gainDb = juce::jmin (-18.0 - rawLufs, -1.5 - rawPeak);
        wav.applyGain ((float) juce::Decibels::decibelsToGain (gainDb));
        const double peakDb = peakOf (wav);
        const double lufs = integratedLufs (wav);
        {
            juce::WavAudioFormat fmt;
            std::unique_ptr<juce::OutputStream> os = std::make_unique<juce::FileOutputStream> (wavFile);
            auto w = fmt.createWriterFor (os, juce::AudioFormatWriterOptions{}.withSampleRate (sr).withNumChannels (2).withBitsPerSample (24));
            written = written && w != nullptr && w->writeFromAudioSampleBuffer (wav, 0, wav.getNumSamples());
        }
        bool finite = true;
        for (int ch = 0; ch < 2; ++ch)
            for (int k = 0; k < wav.getNumSamples(); ++k)
                if (! std::isfinite (wav.getSample (ch, k))) finite = false;
        const bool closed = open.empty() && h.sim().sounding() == 0;

        check (written && finite && closed && outNotes > 0 && peakDb < -1.0 && lufs > -40.0,
               juce::String::fromUTF8 (fp.name),
               juce::String (fp.bpm) + " BPM, " + juce::String ((int) demo.size()) + u8 (" notes jou\xc3\xa9" "es -> ") + juce::String (outNotes)
                   + " notes MIDI ; voix Preview : pic " + juce::String (rawPeak, 1) + " dBFS, " + juce::String (rawLufs, 1)
                   + u8 (" LUFS ; WAV : pic ") + juce::String (peakDb, 1) + " dBFS, " + juce::String (lufs, 1) + " LUFS"
                   + (closed ? "" : u8 (", NOTE RESTEE")) + (finite ? "" : ", NaN"));
        table << "| " << juce::String::fromUTF8 (fp.name) << " | " << fp.bank << " | " << fp.bpm << " | " << (int) demo.size() << " | " << outNotes
              << " | " << juce::String (rawPeak, 1) << " / " << juce::String (rawLufs, 1) << " | " << juce::String (peakDb, 1) << " / " << juce::String (lufs, 1) << " |\n";
    }
    report << table;
}

//==============================================================================
// Presets et états corrompus ou piégés : un fichier .cfpreset reçu d'un inconnu, ou un projet Ableton abîmé,
// ne doit jamais faire planter le plugin, ni y faire entrer une valeur invalide (NaN, infini, hors plage).
//==============================================================================
void testHostileState()
{
    section (u8 ("S\xc3\xa9" "curit\xc3\xa9 : presets et \xc3\xa9tats corrompus ou pi\xc3\xa9g\xc3\xa9s"));
    auto dir = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cf_hostile");
    dir.deleteRecursively();
    dir.createDirectory();

    // un état valide sert de modèle : on y injecte des valeurs piégées
    juce::String tag;
    juce::StringArray ids;
    {
        Harness h;
        tag = h.proc->apvts.state.getType().toString();
        for (auto* p : h.proc->getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p)) ids.add (rp->getParameterID());
    }
    auto craft = [&] (const juce::String& badValue)
    {
        juce::String x = "<?xml version=\"1.0\"?>\n<" + tag + ">\n";
        for (auto& pid : ids) x << "  <PARAM id=\"" << pid << "\" value=\"" << badValue << "\"/>\n";
        x << "  <PADS>";
        for (int i = 0; i < 8; ++i) x << "<PAD degree=\"" << badValue << "\" type=\"" << badValue << "\" inversion=\"" << badValue << "\" octave=\"" << badValue << "\"/>";
        x << "</PADS>\n  <LEARNED intervals=\"" << badValue << " 999 -5 " << badValue << "\"/>\n</" << tag << ">\n";
        return x;
    };

    struct Case { juce::String name; juce::MemoryBlock bytes; };
    std::vector<Case> cases;
    auto addText = [&] (const juce::String& n, const juce::String& t) { cases.push_back ({ n, juce::MemoryBlock (t.toRawUTF8(), t.getNumBytesAsUTF8()) }); };
    {
        juce::Random r (99);
        juce::MemoryBlock junk (65536);
        for (size_t i = 0; i < junk.getSize(); ++i) junk[i] = (char) r.nextInt (256);
        cases.push_back ({ "octets au hasard (64 ko)", junk });
    }
    addText ("fichier vide", "");
    addText ("XML d'un autre plugin", "<?xml version=\"1.0\"?><AUTRE><PARAM id=\"mode\" value=\"3\"/></AUTRE>");
    addText ("XML tronque", craft ("1").substring (0, 200));
    for (auto bad : { "nan", "inf", "-inf", "1e30", "-1e30", "abc", "2147483647", "-2147483648" })
        addText (juce::String ("valeurs \"") + bad + "\"", craft (bad));

    int crashes = 0, badParams = 0, badMidi = 0, stuck = 0, nanAudio = 0, cnt = 0;
    juce::String detail;
    for (auto& c : cases)
    {
        for (int route = 0; route < 2; ++route)   // 0 : preset utilisateur, 1 : état du projet (setStateInformation)
        {
            Harness h;
            h.set (id::preview, 1.0f);
            if (route == 0)
            {
                auto f = dir.getChildFile ("piege.cfpreset");
                f.replaceWithData (c.bytes.getData(), c.bytes.getSize());
                h.proc->presets.loadUserPreset (f);
            }
            else
            {
                // comme un hôte : l'état passe par copyXmlToBinary quand c'est du XML, sinon les octets bruts
                juce::MemoryBlock state;
                if (auto xml = juce::XmlDocument::parse (c.bytes.toString()))
                    juce::AudioProcessor::copyXmlToBinary (*xml, state);
                else
                    state = c.bytes;
                h.proc->setStateInformation (state.getData(), (int) state.getSize());
            }
            h.proc->pollMessageThread();
            ++cnt;

            int localBad = 0;
            for (auto* p : h.proc->getParameters())
            {
                const float v = p->getValue();
                if (! std::isfinite (v) || v < 0.0f || v > 1.0f) ++localBad;
            }
            for (auto& pid : ids)
                if (! std::isfinite (h.proc->apvts.getRawParameterValue (pid)->load())) ++localBad;
            badParams += localBad;

            // on joue quand même : notes, transport, pads
            h.head.playing = true;
            for (int k = 0; k < 24; ++k)
            {
                h.noteOn (h.rendered + 100 + k * 900, 36 + (k * 7) % 60, 100);
                h.noteOff (h.rendered + 700 + k * 900, 36 + (k * 7) % 60);
            }
            h.afterBlock = [&nanAudio] (const juce::AudioBuffer<float>& a, int64_t)
            {
                for (int ch = 0; ch < a.getNumChannels(); ++ch)
                    for (int i = 0; i < a.getNumSamples(); ++i)
                        if (! std::isfinite (a.getSample (ch, i))) { ++nanAudio; return; }
            };
            h.run (48000);
            h.proc->requestPanic();
            h.run (4096);
            badMidi += (int) h.formatErrors;
            const auto sim = h.sim();
            badMidi += (int) sim.badData;
            stuck += sim.sounding();
            if (localBad > 0 && detail.isEmpty()) detail = c.name + (route == 0 ? " (preset)" : u8 (" (\xc3\xa9tat du projet)"));
        }
    }
    dir.deleteRecursively();
    check (crashes == 0 && badParams == 0 && badMidi == 0 && stuck == 0 && nanAudio == 0,
           u8 ("Fichiers pi\xc3\xa9g\xc3\xa9s : aucun plantage, aucune valeur invalide"),
           juce::String (cnt) + u8 (" chargements (") + juce::String ((int) cases.size()) + u8 (" fichiers x preset / \xc3\xa9tat du projet) : ")
               + juce::String (badParams) + u8 (" param\xc3\xa8tre(s) invalide(s)") + (detail.isNotEmpty() ? " (" + detail + ")" : juce::String())
               + ", " + juce::String (badMidi) + u8 (" message(s) MIDI invalide(s), ") + juce::String (stuck) + u8 (" note(s) rest\xc3\xa9" "e(s), ")
               + juce::String (nanAudio) + " bloc(s) audio NaN");
}
//==============================================================================
int main (int argc, char** argv)
{
    if (const char* t = std::getenv ("CF_ALLOC_TRACE")) AllocCounter::trace = juce::String (t) == "1";
    { void* warm[2]; (void) backtrace (warm, 2); }   // charge libgcc avant de compter
    juce::ScopedJuceInitialiser_GUI init;
    quickMode = argc > 1 && juce::String (argv[1]) == "quick";
    const juce::String only = argc > 1 && ! quickMode ? juce::String (argv[1]) : juce::String();
    auto run = [&] (const char* name) { return only.isEmpty() || only == name; };

    if (run ("theory"))  testTheoryTable();
    if (run ("names"))   testKnownChords();
    if (run ("scale"))   testInScale();
    if (run ("voicing")) testVoicing();
    if (run ("detect"))  testDetection();
    if (run ("modes"))   testModes();
    if (run ("midi"))    testMidiBasics();
    if (run ("offs"))    { testNoteOffs(); testAllNotesOff(); }
    if (run ("fuzz"))    testFuzz();
    if (run ("state"))   testState();
    if (run ("hostile")) testHostileState();
    if (run ("perf"))    testPerformance();
    if (run ("presets")) presetRenders (juce::File::getCurrentWorkingDirectory().getChildFile ("renders").getChildFile ("presets"));
    if (only == "uiprof") uiProfile();
    if (only == "mockups") mockups (juce::File::getCurrentWorkingDirectory().getChildFile ("maquettes"));
    if (run ("snap") || only.isEmpty()) snapshots (juce::File::getCurrentWorkingDirectory().getChildFile ("captures"));

    std::printf ("\n%s\n", failures == 0 ? "TOUS LES TESTS PASSENT" : ("ECHECS : " + juce::String (failures)).toRawUTF8());
    juce::File out = juce::File::getCurrentWorkingDirectory().getChildFile ("test-results.md");
    out.replaceWithText ("# Resultats du banc de tests de Confluence\n" + report, false, false, "\n");
    return failures == 0 ? 0 : 1;
}
