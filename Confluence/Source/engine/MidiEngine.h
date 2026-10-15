// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "Resolver.h"
#include <limits>

//==============================================================================
/**
    Moteur MIDI temps réel de Confluence.

    - Chaque note jouée crée une « instance » d'accord : ses notes (8 voix + basse au maximum), leur état
      (en attente / jouée / terminée) et la touche qui l'a déclenchée.
    - Les notes à venir (strum, humanize, fin d'une durée fixe) sont rangées dans une file triée par date
      (tas binaire préalloué). Chaque bloc ne sort que les événements qui tombent dedans, à l'échantillon près :
      le résultat ne dépend pas de la taille des blocs.
    - Plusieurs accords peuvent partager une note : un compteur par (canal, note) fait qu'une note n'est coupée
      que quand plus aucun accord ne la tient. Une note déjà jouée par un autre accord est relancée
      (note-off puis note-on au même instant).
    - Un relâchement de touche coupe exactement les notes que cette touche a jouées, même si les réglages
      ont changé entre-temps (les notes sont mémorisées, jamais recalculées).

    Aucune allocation, aucun verrou : tout est préalloué.
*/
namespace Engine
{
/** Réglages de jeu lus à chaque bloc. */
struct PlaySettings
{
    int outChannel = 1;          // 1..16
    double strumStep = 0.0;      // écart entre deux notes, en échantillons
    int strumDir = 0;            // 0 haut, 1 bas, 2 alterné, 3 aléatoire
    double humanTime = 0.0;      // retard maximal, en échantillons (jamais d'avance)
    float humanVel = 0.0f;       // ± velocity
    int velMode = 0;             // 0 suit la touche, 1 fixe, 2 courbe
    int velFixed = 100;
    float velCurve = 0.0f;       // -1..1
    float balBass = 1.0f, balMid = 1.0f, balTop = 1.0f;
    bool fixedDuration = false;
    double durationSamples = 0.0;
    bool latch = false;
};

class MidiEngine
{
public:
    static constexpr int maxInstances = 256;
    static constexpr int maxSlots = Voicing::maxNotes + 1;
    static constexpr int maxEvents = maxInstances * maxSlots * 2;

    enum SlotState : uint8_t { slotEmpty = 0, slotPending, slotOn, slotDone };

    MidiEngine() { reset(); }

    void setSeed (uint32_t s) noexcept { rng = s == 0 ? 0x9e3779b9u : s; }

    /** Oublie tout sans rien envoyer (à n'utiliser que quand aucune note ne sonne). */
    void reset() noexcept
    {
        for (auto& in : instances) in = Instance {};
        for (auto& row : held) row.fill (-1);
        for (auto& row : refCount) row.fill (0);
        numEvents = 0;
        seq = 0;
        blockStart = 0;
        alternate = false;
        lastVoices = {};
    }

    //==============================================================================
    void beginBlock (int64_t absoluteStart, int numSamples, juce::MidiBuffer& output) noexcept
    {
        blockStart = absoluteStart;
        blockLength = numSamples;
        out = &output;
    }

    /** Sort les événements programmés jusqu'à la fin du bloc. */
    void endBlock() noexcept
    {
        flushUntil (blockStart + blockLength - 1);
        releaseFinishedInstances();
        out = nullptr;
    }

    //==============================================================================
    /** Note jouée : calcule l'accord et le programme. Renvoie true si un accord a été déclenché. */
    bool noteOn (int offset, int inCh, int note, int velocity, const ChordSettings& cs, const PadSet& pads,
                 const LearnedShape& learned, const PlaySettings& ps, Resolved& resolved) noexcept
    {
        const int64_t t = blockStart + offset;
        flushUntil (t);

        // Latch : la note suivante libère les accords tenus dont la touche est relâchée
        releaseLatched (t);

        // même touche rejouée sans avoir été relâchée : l'ancien accord s'arrête
        if (auto old = held[(size_t) chIndex (inCh)][(size_t) note]; old >= 0)
            keyUp (old, t, true);

        if (! resolve (cs, pads, learned, note, lastVoices, resolved))
            return false;

        const int idx = allocateInstance (t);
        auto& in = instances[(size_t) idx];
        in.inputChannel = (uint8_t) chIndex (inCh);
        in.inputNote = (int8_t) note;
        in.keyDown = true;
        in.fixedDuration = ps.fixedDuration;
        in.channel = (uint8_t) juce::jlimit (1, 16, ps.outChannel);
        held[(size_t) in.inputChannel][(size_t) note] = (int16_t) idx;

        // notes de l'accord, de la plus grave à la plus aiguë (la basse d'abord)
        int pitches[maxSlots];
        int n = 0;
        const auto& v = resolved.voiced;
        if (v.bass >= 0) pitches[n++] = v.bass;
        for (int i = 0; i < v.voices.count && n < maxSlots; ++i) pitches[n++] = v.voices.n[i];
        lastVoices = v.voices;

        // ordre du strum
        int order[maxSlots];
        for (int i = 0; i < n; ++i) order[i] = i;
        int dir = ps.strumDir;
        if (dir == 2) { dir = alternate ? 1 : 0; alternate = ! alternate; }
        if (dir == 1) std::reverse (order, order + n);
        else if (dir == 3)
            for (int i = n - 1; i > 0; --i)
                std::swap (order[i], order[(int) (nextRandom() % (uint32_t) (i + 1))]);

        const float base = baseVelocity (velocity, ps);
        in.numSlots = n;
        for (int k = 0; k < n; ++k)
        {
            const int i = order[k];
            auto& s = in.slots[(size_t) i];
            s.pitch = (uint8_t) juce::jlimit (0, 127, pitches[i]);

            const bool isBass = i == 0;                 // la basse ajoutée, ou la voix la plus grave
            const bool isTop  = i == n - 1 && n > 1;
            const float bal = isBass ? ps.balBass : (isTop ? ps.balTop : ps.balMid);
            float vel = base * bal;
            if (ps.humanVel > 0.0f)
                vel += ps.humanVel * (2.0f * nextUnit() - 1.0f);
            s.velocity = (uint8_t) juce::jlimit (1, 127, (int) std::lround (vel));

            double delay = (double) k * ps.strumStep;
            if (ps.humanTime > 0.0)
                delay += ps.humanTime * (double) nextUnit();
            s.onTime = t + (int64_t) std::llround (delay);
            s.state = slotPending;
            push ({ s.onTime, 0, (uint16_t) idx, in.gen, (uint8_t) i, 1 });
            if (ps.fixedDuration)
                push ({ s.onTime + juce::jmax ((int64_t) 1, (int64_t) std::llround (ps.durationSamples)), 0,
                        (uint16_t) idx, in.gen, (uint8_t) i, 0 });
        }

        flushUntil (t);   // les notes sans retard partent tout de suite (latence nulle)
        return true;
    }

    /** Note jouée telle quelle, sans accord (bypass, apprentissage d'un accord). */
    void rawNoteOn (int offset, int inCh, int note, int velocity, int outChannel) noexcept
    {
        const int64_t t = blockStart + offset;
        flushUntil (t);
        if (auto old = held[(size_t) chIndex (inCh)][(size_t) note]; old >= 0)
            keyUp (old, t, true);

        const int idx = allocateInstance (t);
        auto& in = instances[(size_t) idx];
        in.inputChannel = (uint8_t) chIndex (inCh);
        in.inputNote = (int8_t) note;
        in.keyDown = true;
        in.channel = (uint8_t) juce::jlimit (1, 16, outChannel);
        in.numSlots = 1;
        in.raw = true;
        auto& s = in.slots[0];
        s.pitch = (uint8_t) note;
        s.velocity = (uint8_t) juce::jlimit (1, 127, velocity);
        s.onTime = t;
        s.state = slotPending;
        held[(size_t) in.inputChannel][(size_t) note] = (int16_t) idx;
        startSlot (in, 0, t);
    }

    void noteOff (int offset, int inCh, int note, bool latch) noexcept
    {
        const int64_t t = blockStart + offset;
        flushUntil (t);
        const auto idx = held[(size_t) chIndex (inCh)][(size_t) note];
        if (idx < 0) return;
        auto& in = instances[(size_t) idx];
        if (latch && ! in.fixedDuration && ! in.raw)
        {
            held[(size_t) in.inputChannel][(size_t) note] = -1;
            in.keyDown = false;
            in.latched = true;
            return;
        }
        keyUp (idx, t, false);
    }

    /** Latch désactivé : les accords tenus par le Latch s'arrêtent. */
    void releaseLatchedAt (int offset) noexcept
    {
        const int64_t t = blockStart + offset;
        flushUntil (t);
        releaseLatched (t);
    }

    /** Aftertouch polyphonique sur la touche : envoyé à toutes les notes de son accord. */
    void polyAftertouch (int offset, int inCh, int note, int value) noexcept
    {
        const int64_t t = blockStart + offset;
        flushUntil (t);
        const auto idx = held[(size_t) chIndex (inCh)][(size_t) note];
        if (idx < 0 || out == nullptr) return;
        const auto& in = instances[(size_t) idx];
        for (int i = 0; i < in.numSlots; ++i)
            if (in.slots[(size_t) i].state == slotOn)
            {
                const uint8_t d[3] = { (uint8_t) (0xA0 | (in.channel - 1)), in.slots[(size_t) i].pitch, (uint8_t) juce::jlimit (0, 127, value) };
                out->addEvent (d, 3, offsetOf (t));
            }
    }

    /** Messages de canal (CC, pitch bend, aftertouch, program change) : recopiés sur le canal de sortie. */
    void passThrough (int offset, const uint8_t* data, int size, int outChannel) noexcept
    {
        const int64_t t = blockStart + offset;
        flushUntil (t);
        if (out == nullptr || size <= 0) return;
        if (data[0] >= 0x80 && data[0] < 0xF0 && size <= 3)
        {
            uint8_t d[3] = { (uint8_t) ((data[0] & 0xF0) | (juce::jlimit (1, 16, outChannel) - 1)), size > 1 ? data[1] : (uint8_t) 0, size > 2 ? data[2] : (uint8_t) 0 };
            out->addEvent (d, size, offsetOf (t));
        }
        else
            out->addEvent (data, size, offsetOf (t));
    }

    /** Coupe tout, tout de suite : notes jouées et notes en attente. */
    void panic (int offset, bool sendControllers = false, int outChannel = 1) noexcept
    {
        const int64_t t = blockStart + juce::jlimit (0, juce::jmax (0, blockLength - 1), offset);
        for (int i = 0; i < maxInstances; ++i)
            if (instances[(size_t) i].used)
                releaseInstance (i, t);
        numEvents = 0;

        // filet de sécurité : rien ne doit rester compté
        for (int ch = 0; ch < 16; ++ch)
            for (int p = 0; p < 128; ++p)
                if (refCount[(size_t) ch][(size_t) p] > 0)
                {
                    refCount[(size_t) ch][(size_t) p] = 0;
                    emit (0x80, ch + 1, p, 0, t);
                }
        for (auto& row : held) row.fill (-1);

        if (sendControllers && out != nullptr)
        {
            const uint8_t ch = (uint8_t) (juce::jlimit (1, 16, outChannel) - 1);
            const uint8_t sus[3] = { (uint8_t) (0xB0 | ch), 64, 0 }, allOff[3] = { (uint8_t) (0xB0 | ch), 123, 0 };
            out->addEvent (sus, 3, offsetOf (t));
            out->addEvent (allOff, 3, offsetOf (t));
        }
        lastVoices = {};
    }

    //==============================================================================
    /** Nombre de notes qui sonnent en ce moment (tests, affichage). */
    int numSounding() const noexcept
    {
        int n = 0;
        for (const auto& row : refCount) for (auto c : row) n += c > 0 ? 1 : 0;
        return n;
    }
    int numActiveInstances() const noexcept
    {
        int n = 0;
        for (const auto& in : instances) n += in.used ? 1 : 0;
        return n;
    }
    int numPendingEvents() const noexcept { return numEvents; }

    /** Notes qui sonnent, tous canaux confondus (128 bits). */
    void soundingMask (uint32_t bits[4]) const noexcept
    {
        for (int i = 0; i < 4; ++i) bits[i] = 0;
        for (const auto& row : refCount)
            for (int p = 0; p < 128; ++p)
                if (row[(size_t) p] > 0) bits[p >> 5] |= 1u << (p & 31);
    }
    const Voicing::Notes& getLastVoices() const noexcept { return lastVoices; }

private:
    struct Slot
    {
        int64_t onTime = 0;
        uint8_t pitch = 0, velocity = 0;
        uint8_t state = slotEmpty;
    };

    struct Instance
    {
        bool used = false, keyDown = false, latched = false, fixedDuration = false, raw = false;
        uint32_t gen = 0;
        int64_t startTime = 0;
        uint8_t inputChannel = 0, channel = 1;
        int8_t inputNote = -1;
        int numSlots = 0;
        std::array<Slot, maxSlots> slots {};
    };

    struct Event
    {
        int64_t time;
        uint32_t seq;
        uint16_t instance;
        uint32_t gen;
        uint8_t slot;
        uint8_t type;   // 0 = note-off (durée fixe), 1 = note-on
    };

    static int chIndex (int ch) noexcept { return juce::jlimit (0, 15, ch - 1); }
    int offsetOf (int64_t t) const noexcept { return (int) juce::jlimit ((int64_t) 0, (int64_t) juce::jmax (0, blockLength - 1), t - blockStart); }

    // --- générateur pseudo-aléatoire (xorshift), initialisé hors du thread audio
    uint32_t nextRandom() noexcept { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }
    float nextUnit() noexcept { return (float) (nextRandom() >> 8) * (1.0f / 16777216.0f); }

    static float baseVelocity (int v, const PlaySettings& ps) noexcept
    {
        if (ps.velMode == 1) return (float) ps.velFixed;
        if (ps.velMode == 2)
        {
            const float x = juce::jlimit (0.0f, 1.0f, (float) v / 127.0f);
            return 127.0f * std::pow (x, std::pow (3.0f, -ps.velCurve));
        }
        return (float) v;
    }

    //==============================================================================
    // file des événements : tas binaire (le plus tôt en tête ; à égalité, note-off avant note-on, puis ordre d'arrivée)
    static bool later (const Event& a, const Event& b) noexcept
    {
        if (a.time != b.time) return a.time > b.time;
        if (a.type != b.type) return a.type > b.type;
        return a.seq > b.seq;
    }

    void push (Event e) noexcept
    {
        if (numEvents >= maxEvents)
            compact();
        if (numEvents >= maxEvents) return;   // impossible : une instance vivante a au plus 2 événements par note
        e.seq = seq++;
        int i = numEvents++;
        events[(size_t) i] = e;
        while (i > 0)
        {
            const int parent = (i - 1) / 2;
            if (! later (events[(size_t) parent], events[(size_t) i])) break;
            std::swap (events[(size_t) parent], events[(size_t) i]);
            i = parent;
        }
    }

    /** Événement encore utile ? (les accords arrêtés laissent des événements périmés dans la file) */
    bool isLive (const Event& e) const noexcept
    {
        const auto& in = instances[e.instance];
        if (! in.used || in.gen != e.gen) return false;
        const auto st = in.slots[e.slot].state;
        return e.type == 1 ? st == slotPending : (st == slotPending || st == slotOn);
    }

    /** File pleine : on retire les événements périmés puis on reconstruit le tas. */
    void compact() noexcept
    {
        int k = 0;
        for (int i = 0; i < numEvents; ++i)
            if (isLive (events[(size_t) i]))
                events[(size_t) k++] = events[(size_t) i];
        numEvents = k;
        std::make_heap (events.begin(), events.begin() + numEvents, [] (const Event& a, const Event& b) { return later (a, b); });
    }

    Event pop() noexcept
    {
        Event top = events[0];
        events[0] = events[(size_t) --numEvents];
        int i = 0;
        for (;;)
        {
            const int l = 2 * i + 1, r = l + 1;
            int m = i;
            if (l < numEvents && later (events[(size_t) m], events[(size_t) l])) m = l;
            if (r < numEvents && later (events[(size_t) m], events[(size_t) r])) m = r;
            if (m == i) break;
            std::swap (events[(size_t) m], events[(size_t) i]);
            i = m;
        }
        return top;
    }

    /** Sort tous les événements programmés jusqu'à l'instant t inclus (jamais au-delà du bloc). */
    void flushUntil (int64_t t) noexcept
    {
        const int64_t limit = juce::jmin (t, blockStart + blockLength - 1);
        while (numEvents > 0 && events[0].time <= limit)
        {
            const auto e = pop();
            auto& in = instances[e.instance];
            if (! in.used || in.gen != e.gen) continue;
            auto& s = in.slots[e.slot];
            if (e.type == 1)
            {
                if (s.state == slotPending)
                    startSlot (in, e.slot, e.time);
            }
            else if (s.state == slotOn)
            {
                stopSlot (in, e.slot, e.time);
                if (allDone (in))
                    freeInstance (in);
            }
        }
    }

    void startSlot (Instance& in, int slot, int64_t t) noexcept
    {
        auto& s = in.slots[(size_t) slot];
        auto& rc = refCount[(size_t) (in.channel - 1)][s.pitch];
        if (rc > 0)
            emit (0x80, in.channel, s.pitch, 0, t);   // déjà tenue par un autre accord : on la relance
        emit (0x90, in.channel, s.pitch, s.velocity, t);
        ++rc;
        s.state = slotOn;
    }

    void stopSlot (Instance& in, int slot, int64_t t) noexcept
    {
        auto& s = in.slots[(size_t) slot];
        auto& rc = refCount[(size_t) (in.channel - 1)][s.pitch];
        if (rc > 0 && --rc == 0)
            emit (0x80, in.channel, s.pitch, 0, t);
        s.state = slotDone;
    }

    void emit (uint8_t status, int channel, int pitch, int velocity, int64_t t) noexcept
    {
        if (out == nullptr) return;
        const uint8_t d[3] = { (uint8_t) (status | (juce::jlimit (1, 16, channel) - 1)), (uint8_t) juce::jlimit (0, 127, pitch),
                               (uint8_t) juce::jlimit (0, 127, velocity) };
        out->addEvent (d, 3, offsetOf (t));
    }

    static bool allDone (const Instance& in) noexcept
    {
        for (int i = 0; i < in.numSlots; ++i)
            if (in.slots[(size_t) i].state == slotPending || in.slots[(size_t) i].state == slotOn)
                return false;
        return true;
    }

    void freeInstance (Instance& in) noexcept
    {
        if (in.inputNote >= 0 && in.keyDown)
        {
            auto& h = held[in.inputChannel][(size_t) in.inputNote];
            if (h >= 0 && &instances[(size_t) h] == &in) h = -1;
        }
        const auto g = in.gen + 1;
        in = Instance {};
        in.gen = g;
    }

    /** Arrête l'instance : ses notes jouées sont coupées, ses notes en attente annulées. */
    void releaseInstance (int idx, int64_t t) noexcept
    {
        auto& in = instances[(size_t) idx];
        for (int i = 0; i < in.numSlots; ++i)
        {
            auto& s = in.slots[(size_t) i];
            if (s.state == slotOn) stopSlot (in, i, t);
            else if (s.state == slotPending) s.state = slotDone;
        }
        freeInstance (in);
    }

    /** Touche relâchée : un accord tenu s'arrête ; un accord à durée fixe continue jusqu'à sa fin. */
    void keyUp (int idx, int64_t t, bool retrigger) noexcept
    {
        auto& in = instances[(size_t) idx];
        if (in.inputNote >= 0)
            held[in.inputChannel][(size_t) in.inputNote] = -1;
        in.keyDown = false;
        juce::ignoreUnused (retrigger);
        if (in.fixedDuration)
            return;   // durée fixe : l'accord finit sa durée (une nouvelle frappe crée un nouvel accord)
        releaseInstance (idx, t);
    }

    void releaseLatched (int64_t t) noexcept
    {
        for (int i = 0; i < maxInstances; ++i)
        {
            const auto& in = instances[(size_t) i];
            if (in.used && in.latched && ! in.keyDown)
                releaseInstance (i, t);
        }
    }

    /** Instances à durée fixe dont toutes les notes sont finies : libérées. */
    void releaseFinishedInstances() noexcept
    {
        for (auto& in : instances)
            if (in.used && in.numSlots > 0 && allDone (in))
                freeInstance (in);
    }

    int allocateInstance (int64_t t) noexcept
    {
        int oldest = 0;
        int64_t oldestTime = std::numeric_limits<int64_t>::max();
        for (int i = 0; i < maxInstances; ++i)
        {
            auto& in = instances[(size_t) i];
            if (! in.used)
            {
                const auto g = in.gen;
                in = Instance {};
                in.gen = g;
                in.used = true;
                in.startTime = t;
                return i;
            }
            if (in.startTime < oldestTime) { oldestTime = in.startTime; oldest = i; }
        }
        // plus de place (des centaines d'accords à la fois) : le plus ancien est arrêté
        releaseInstance (oldest, t);
        auto& in = instances[(size_t) oldest];
        in.used = true;
        in.startTime = t;
        return oldest;
    }

    //==============================================================================
    std::array<Instance, maxInstances> instances {};
    std::array<std::array<int16_t, 128>, 16> held {};
    std::array<std::array<uint16_t, 128>, 16> refCount {};
    std::array<Event, maxEvents> events {};
    int numEvents = 0;
    uint32_t seq = 0;
    uint32_t rng = 0x9e3779b9u;
    int64_t blockStart = 0;
    int blockLength = 0;
    bool alternate = false;
    Voicing::Notes lastVoices;
    juce::MidiBuffer* out = nullptr;
};
} // namespace Engine
