// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include "engine/Resolver.h"

//==============================================================================
/**
    Export de la progression des 8 pads en clip MIDI (4 ou 8 mesures, 4/4) :
    mêmes accords et même voicing que dans le plugin (voice leading compris), strum en temps musical.
*/
namespace MidiExport
{
struct Options
{
    double bpm = 124.0;
    int bars = 4;               // 4 : un pad toutes les 2 noires ; 8 : un pad par mesure
    double strumBeats = 0.0;    // écart entre deux notes, en noires
    int strumDir = 0;           // 0 haut, 1 bas, 2 alterné, 3 aléatoire (graine fixe : export reproductible)
    int velocity = 100;
    bool fixedDuration = false;
    double durationBeats = 0.25;
};

inline juce::MidiFile progression (Engine::ChordSettings cs, const Engine::PadSet& pads, const Engine::LearnedShape& learned,
                                   const Options& o, std::vector<std::vector<int>>* chordsOut = nullptr)
{
    constexpr int ppq = 960;
    cs.mode = Engine::modePads;

    juce::MidiMessageSequence seq;
    seq.addEvent (juce::MidiMessage::textMetaEvent (3, "Confluence"), 0.0);
    seq.addEvent (juce::MidiMessage::tempoMetaEvent (juce::roundToInt (60000000.0 / juce::jmax (20.0, o.bpm))), 0.0);
    seq.addEvent (juce::MidiMessage::timeSignatureMetaEvent (4, 4), 0.0);

    const double slotBeats = o.bars >= 8 ? 4.0 : 2.0;
    Voicing::Notes previous;
    bool alternate = false;
    uint32_t rng = 0x2545F491u;

    for (int i = 0; i < Engine::numPads; ++i)
    {
        Engine::Resolved r;
        if (! Engine::resolve (cs, pads, learned, cs.padBase + i, previous, r))
            continue;
        previous = r.voiced.voices;

        std::vector<int> notes;
        if (r.voiced.bass >= 0) notes.push_back (r.voiced.bass);
        for (int k = 0; k < r.voiced.voices.count; ++k) notes.push_back (r.voiced.voices.n[k]);
        if (chordsOut != nullptr) chordsOut->push_back (notes);

        std::vector<int> order (notes.size());
        for (size_t k = 0; k < order.size(); ++k) order[k] = (int) k;
        int dir = o.strumDir;
        if (dir == 2) { dir = alternate ? 1 : 0; alternate = ! alternate; }
        if (dir == 1) std::reverse (order.begin(), order.end());
        if (dir == 3)
            for (int k = (int) order.size() - 1; k > 0; --k)
            {
                rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
                std::swap (order[(size_t) k], order[(size_t) (rng % (uint32_t) (k + 1))]);
            }

        const double start = (double) i * slotBeats;
        const double end = start + (o.fixedDuration ? juce::jmin (o.durationBeats, slotBeats) : slotBeats);
        for (size_t k = 0; k < order.size(); ++k)
        {
            const double on = juce::jmin (start + (double) k * o.strumBeats, end - 1.0 / 64.0);
            const int note = notes[(size_t) order[k]];
            seq.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) juce::jlimit (1, 127, o.velocity)), on * ppq);
            seq.addEvent (juce::MidiMessage::noteOff (1, note), end * ppq);
        }
    }
    seq.updateMatchedPairs();
    seq.sort();

    juce::MidiFile file;
    file.setTicksPerQuarterNote (ppq);
    file.addTrack (seq);
    return file;
}

inline bool write (const juce::MidiFile& mf, const juce::File& f)
{
    f.getParentDirectory().createDirectory();
    f.deleteFile();
    juce::FileOutputStream os (f);
    if (! os.openedOk()) return false;
    return mf.writeTo (os, 1);
}
}
