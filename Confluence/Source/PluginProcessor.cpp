// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ConfluenceAudioProcessor::ConfluenceAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "CONFLUENCE", Params::createLayout())
{
    namespace id = ParamIDs;
    r.mode = raw (id::mode);             r.root = raw (id::root);           r.scale = raw (id::scale);
    r.outOfScale = raw (id::outOfScale); r.chordType = raw (id::chordType); r.blackKeys = raw (id::blackKeys);
    r.fixedShape = raw (id::fixedShape); r.style = raw (id::style);         r.padBase = raw (id::padBase);
    r.inversion = raw (id::inversion);   r.spread = raw (id::spread);       r.maxVoices = raw (id::maxVoices);
    r.lowNote = raw (id::lowNote);       r.highNote = raw (id::highNote);   r.bass = raw (id::bass);
    r.octave = raw (id::octave);         r.transpose = raw (id::transpose);
    r.strum = raw (id::strum);           r.strumDir = raw (id::strumDir);   r.strumSync = raw (id::strumSync);
    r.strumSyncRate = raw (id::strumSyncRate);
    r.humanTime = raw (id::humanTime);   r.humanVel = raw (id::humanVel);
    r.velMode = raw (id::velMode);       r.velFixed = raw (id::velFixed);   r.velCurve = raw (id::velCurve);
    r.balBass = raw (id::balBass);       r.balMid = raw (id::balMid);       r.balTop = raw (id::balTop);
    r.durMode = raw (id::durMode);       r.duration = raw (id::duration);   r.latch = raw (id::latch);
    r.outChannel = raw (id::outChannel); r.preview = raw (id::preview);     r.previewLevel = raw (id::previewLevel);

    // une instance neuve : une progression Hypnotic en la mineur sur les pads
    padsUi = Harmony::generatePads (Harmony::styleHypnotic, Theory::naturalMinor, 7);
    padsX.reset (padsUi);
    learnedX.reset (learnedUi);
    displayX.reset ({});
    for (auto& row : onsetSample) row.fill (-1);

    // graine du hasard (humanize, strum aléatoire), tirée hors du thread audio
    engine.setSeed ((uint32_t) juce::Random::getSystemRandom().nextInt() | 1u);
    out.ensureSize (65536);
}

void ConfluenceAudioProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr;
    preview.prepare (sr);
    out.ensureSize (65536);
    // l'état du moteur est gardé : s'il restait des notes (désactivation), elles seront coupées au prochain bloc
}

void ConfluenceAudioProcessor::releaseResources()
{
    deactivatePanic.store (true);
}

bool ConfluenceAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto o = layouts.getMainOutputChannelSet();
    if (o != juce::AudioChannelSet::stereo() && o != juce::AudioChannelSet::mono())
        return false;
    return layouts.getMainInputChannelSet().isDisabled();
}

//==============================================================================
Engine::ChordSettings ConfluenceAudioProcessor::readChordSettings() const
{
    Engine::ChordSettings cs;
    auto i = [] (std::atomic<float>* p) { return (int) std::lround (p->load (std::memory_order_relaxed)); };
    cs.mode = i (r.mode);
    cs.rootPc = i (r.root);
    cs.scale = i (r.scale);
    cs.outOfScale = i (r.outOfScale);
    cs.chordType = i (r.chordType);
    cs.blackKeys = i (r.blackKeys);
    cs.fixedShape = i (r.fixedShape);
    cs.padBase = i (r.padBase);
    cs.voicing.inversion = i (r.inversion);
    cs.voicing.spread = i (r.spread);
    cs.voicing.maxVoices = i (r.maxVoices);
    cs.voicing.low = i (r.lowNote);
    cs.voicing.high = i (r.highNote);
    cs.voicing.bassOctaves = i (r.bass);
    cs.voicing.shift = 12 * i (r.octave) + i (r.transpose);
    return cs;
}

void ConfluenceAudioProcessor::readPlaySettings (Engine::PlaySettings& ps, double bpm) const
{
    auto f = [] (std::atomic<float>* p) { return p->load (std::memory_order_relaxed); };
    auto i = [&f] (std::atomic<float>* p) { return (int) std::lround (f (p)); };
    const double samplesPerBeat = 60.0 / juce::jmax (20.0, bpm) * sampleRate;

    ps.outChannel = juce::jlimit (1, 16, i (r.outChannel));
    ps.strumStep = f (r.strumSync) > 0.5f ? Choices::strumRateBeats (i (r.strumSyncRate)) * samplesPerBeat
                                          : (double) f (r.strum) * 0.001 * sampleRate;
    ps.strumDir = i (r.strumDir);
    ps.humanTime = (double) f (r.humanTime) * 0.001 * sampleRate;
    ps.humanVel = f (r.humanVel);
    ps.velMode = i (r.velMode);
    ps.velFixed = i (r.velFixed);
    ps.velCurve = f (r.velCurve);
    ps.balBass = f (r.balBass);
    ps.balMid = f (r.balMid);
    ps.balTop = f (r.balTop);
    ps.fixedDuration = i (r.durMode) == 1;
    ps.durationSamples = Choices::durationBeats (i (r.duration)) * samplesPerBeat;
    ps.latch = f (r.latch) > 0.5f;
}

//==============================================================================
void ConfluenceAudioProcessor::processBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = audio.getNumSamples();
    audio.clear();

    // --- transport
    bool playing = false;
    double bpm = 120.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
        }
    lastBpm.store (bpm, std::memory_order_relaxed);

    const auto cs = readChordSettings();
    Engine::PlaySettings ps;
    readPlaySettings (ps, bpm);

    out.clear();
    engine.beginBlock (sampleCounter, n, out);

    // --- coupures : sortie du bypass, Panic, changement de preset, désactivation, Stop du transport
    const bool panicButton = panicRequest.exchange (false);
    if (wasBypassed || panicButton || presetPanic.exchange (false) || deactivatePanic.exchange (false)
        || (wasPlaying && ! playing))
    {
        engine.panic (0, panicButton, ps.outChannel);
        activePad.store (-1, std::memory_order_relaxed);
    }
    wasBypassed = false;
    wasPlaying = playing;

    if (lastLatch && ! ps.latch)
        engine.releaseLatchedAt (0);
    lastLatch = ps.latch;

    // --- Learn : début / annulation demandés par l'interface
    if (const int req = learnRequest.exchange (0); req != 0)
    {
        learning.store (req == 1);
        learnHeld.fill (false);
        learnSet.fill (false);
    }

    if (analysisReset.exchange (false))
    {
        aWeight.fill (0.0f);
        aBass.fill (0.0f);
        for (int i = 0; i < 12; ++i) { analysisWeight[(size_t) i].store (0.0f); analysisBass[(size_t) i].store (0.0f); }
        analysisCount.store (0);
        for (auto& row : onsetSample) row.fill (-1);
    }

    // --- notes jouées à la souris (clavier et pads de l'interface), au début du bloc
    drainUiNotes (cs, ps, false);

    // --- MIDI entrant, dans l'ordre
    for (const auto m : midi)
    {
        const auto* d = m.data;
        const int size = m.numBytes;
        const int offset = juce::jlimit (0, juce::jmax (0, n - 1), m.samplePosition);
        if (size <= 0) continue;
        const int status = d[0] & 0xF0;
        const int ch = (d[0] & 0x0F) + 1;

        if (size >= 3 && status == 0x90 && d[2] > 0)
            handleNoteOn (offset, ch, d[1], d[2], cs, ps);
        else if (size >= 3 && (status == 0x80 || status == 0x90))
            handleNoteOff (offset, ch, d[1], cs, ps);
        else if (size >= 3 && status == 0xA0)
            engine.polyAftertouch (offset, ch, d[1], d[2]);
        else
            engine.passThrough (offset, d, size, ps.outChannel);
    }

    finishBlock (audio, midi, n);
}

void ConfluenceAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi)
{
    // Bypass : les notes passent telles quelles (on entend la note jouée, sans accord)
    const int n = audio.getNumSamples();
    audio.clear();
    Engine::PlaySettings ps;
    readPlaySettings (ps, lastBpm.load());

    out.clear();
    engine.beginBlock (sampleCounter, n, out);
    if (! wasBypassed)
    {
        engine.panic (0, false, ps.outChannel);
        activePad.store (-1, std::memory_order_relaxed);
        wasBypassed = true;
    }

    drainUiNotes ({}, ps, true);

    for (const auto m : midi)
    {
        const auto* d = m.data;
        const int offset = juce::jlimit (0, juce::jmax (0, n - 1), m.samplePosition);
        if (m.numBytes <= 0) continue;
        const int status = d[0] & 0xF0, ch = (d[0] & 0x0F) + 1;
        if (m.numBytes >= 3 && status == 0x90 && d[2] > 0)
            engine.rawNoteOn (offset, ch, d[1], d[2], ps.outChannel);
        else if (m.numBytes >= 3 && (status == 0x80 || status == 0x90))
            engine.noteOff (offset, ch, d[1], false);
        else
            engine.passThrough (offset, d, m.numBytes, ps.outChannel);
    }
    finishBlock (audio, midi, n);
}

void ConfluenceAudioProcessor::finishBlock (juce::AudioBuffer<float>& audio, juce::MidiBuffer& midi, int n)
{
    engine.endBlock();

    // affichage : notes qui sonnent, touches tenues
    {
        uint32_t s[4] = {}, in[4] = {};
        engine.soundingMask (s);
        for (int k = 0; k < 128; ++k)
            if (inputHeld[(size_t) k] > 0) in[k >> 5] |= 1u << (k & 31);
        for (int i = 0; i < 4; ++i)
        {
            soundingBits[(size_t) i].store (s[i], std::memory_order_relaxed);
            inputBits[(size_t) i].store (in[i], std::memory_order_relaxed);
        }
    }

    midi.clear();
    midi.addEvents (out, 0, n, 0);

    // voix Preview : joue exactement le MIDI qui sort
    const bool previewOn = r.preview->load (std::memory_order_relaxed) > 0.5f;
    if (previewOn)
        preview.process (audio, out, r.previewLevel->load (std::memory_order_relaxed));
    else if (previewWasOn)
        preview.allOff (true);
    previewWasOn = previewOn;

    sampleCounter += n;
}

//==============================================================================
void ConfluenceAudioProcessor::playFromUi (int note, int velocity, bool pad)
{
    int start1, size1, start2, size2;
    uiFifo.prepareToWrite (1, start1, size1, start2, size2);
    if (size1 + size2 < 1) return;
    uiNotes[(size_t) (size1 > 0 ? start1 : start2)] = { (int16_t) juce::jlimit (0, 127, note), (int16_t) juce::jlimit (0, 127, velocity), pad };
    uiFifo.finishedWrite (1);
}

void ConfluenceAudioProcessor::drainUiNotes (const Engine::ChordSettings& cs, const Engine::PlaySettings& ps, bool bypassed)
{
    // canal 16 : clavier de l'interface ; canal 15 : pads de l'interface (séparés des notes du clavier MIDI)
    int start1, size1, start2, size2;
    const int ready = uiFifo.getNumReady();
    if (ready == 0) return;
    uiFifo.prepareToRead (ready, start1, size1, start2, size2);
    auto handle = [&] (const UiNote& u)
    {
        const int ch = u.pad ? 15 : 16;
        if (u.pad)
        {
            auto padCs = cs;
            padCs.mode = Engine::modePads;
            const int note = padCs.padBase + juce::jlimit (0, Engine::numPads - 1, (int) u.note);
            if (bypassed) return;
            if (u.velocity > 0) handleNoteOn (0, ch, note, u.velocity, padCs, ps);
            else                handleNoteOff (0, ch, note, padCs, ps);
            return;
        }
        if (bypassed)
        {
            if (u.velocity > 0) engine.rawNoteOn (0, ch, u.note, u.velocity, ps.outChannel);
            else                engine.noteOff (0, ch, u.note, false);
            return;
        }
        if (u.velocity > 0) handleNoteOn (0, ch, u.note, u.velocity, cs, ps);
        else                handleNoteOff (0, ch, u.note, cs, ps);
    };
    for (int i = 0; i < size1; ++i) handle (uiNotes[(size_t) (start1 + i)]);
    for (int i = 0; i < size2; ++i) handle (uiNotes[(size_t) (start2 + i)]);
    uiFifo.finishedRead (size1 + size2);
}

//==============================================================================
void ConfluenceAudioProcessor::handleNoteOn (int offset, int ch, int note, int vel,
                                             const Engine::ChordSettings& cs, const Engine::PlaySettings& ps)
{
    collectAnalysis (true, ch, note, offset);

    if (learning.load (std::memory_order_relaxed))
    {
        // pendant l'apprentissage on entend ce qu'on joue, sans accord
        learnHeld[(size_t) note] = true;
        learnSet[(size_t) note] = true;
        engine.rawNoteOn (offset, ch, note, vel, ps.outChannel);
        return;
    }

    Engine::Resolved res;
    if (engine.noteOn (offset, ch, note, vel, cs, padsX.read(), learnedX.read(), ps, res))
    {
        ChordDisplay d;
        const auto& v = res.voiced;
        if (v.bass >= 0) d.notes[d.count++] = v.bass;
        for (int i = 0; i < v.voices.count; ++i) d.notes[d.count++] = v.voices.n[i];
        d.bass = v.bass;
        d.rootPc = v.rootPc;
        d.chordRootPc = res.chordRootPc;
        d.padIndex = res.padIndex;
        d.inputNote = note;
        d.borrowed = res.borrowed;
        d.counter = ++displayCounter;
        displayX.write (d);
        if (res.padIndex >= 0) activePad.store (res.padIndex, std::memory_order_relaxed);
    }
}

void ConfluenceAudioProcessor::handleNoteOff (int offset, int ch, int note, const Engine::ChordSettings& cs,
                                              const Engine::PlaySettings& ps)
{
    collectAnalysis (false, ch, note, offset);
    engine.noteOff (offset, ch, note, ps.latch);

    if (cs.mode == Engine::modePads && note - cs.padBase == activePad.load (std::memory_order_relaxed))
        activePad.store (-1, std::memory_order_relaxed);

    if (learning.load (std::memory_order_relaxed) && learnHeld[(size_t) note])
    {
        learnHeld[(size_t) note] = false;
        bool anyHeld = false;
        for (auto h : learnHeld) anyHeld = anyHeld || h;
        if (! anyHeld)
        {
            int notes[128];
            int count = 0;
            for (int i = 0; i < 128; ++i) if (learnSet[(size_t) i]) notes[count++] = i;
            learnSet.fill (false);
            if (count >= 2)
            {
                // intervalles depuis la note la plus basse (8 notes au maximum)
                count = juce::jmin (count, (int) Theory::maxChordTones);
                for (int i = 0; i < count; ++i)
                    learnResult[(size_t) i].store (notes[i] - notes[0], std::memory_order_relaxed);
                learnResultCount.store (count, std::memory_order_relaxed);
                learnResultSeq.fetch_add (1, std::memory_order_release);
                learning.store (false);
            }
        }
    }
}

void ConfluenceAudioProcessor::collectAnalysis (bool on, int ch, int note, int offset)
{
    auto& held = inputHeld[(size_t) note];
    if (on)
    {
        bool lowest = true;
        for (int k = 0; k < note; ++k) if (inputHeld[(size_t) k] > 0) { lowest = false; break; }
        held = 1;
        if (! analysing.load (std::memory_order_relaxed)) return;

        const int pc = note % 12;
        aWeight[(size_t) pc] += 1.0f;
        if (lowest) aBass[(size_t) pc] += 1.0f;
        onsetSample[(size_t) (ch - 1)][(size_t) note] = sampleCounter + offset;
        analysisWeight[(size_t) pc].store (aWeight[(size_t) pc], std::memory_order_relaxed);
        analysisBass[(size_t) pc].store (aBass[(size_t) pc], std::memory_order_relaxed);
        analysisCount.fetch_add (1, std::memory_order_relaxed);
    }
    else
    {
        held = 0;
        auto& onset = onsetSample[(size_t) (ch - 1)][(size_t) note];
        if (onset >= 0 && analysing.load (std::memory_order_relaxed))
        {
            const int pc = note % 12;
            const double seconds = (double) (sampleCounter + offset - onset) / sampleRate;
            aWeight[(size_t) pc] += 0.5f * (float) juce::jlimit (0.0, 4.0, seconds);
            analysisWeight[(size_t) pc].store (aWeight[(size_t) pc], std::memory_order_relaxed);
        }
        onset = -1;
    }
}

//==============================================================================
std::vector<Harmony::Candidate> ConfluenceAudioProcessor::getAnalysisRanking (int count) const
{
    std::array<float, 12> w {}, b {};
    for (int i = 0; i < 12; ++i) { w[(size_t) i] = analysisWeight[(size_t) i].load(); b[(size_t) i] = analysisBass[(size_t) i].load(); }
    return Harmony::rankScales (w, b, count);
}

void ConfluenceAudioProcessor::setPads (const Engine::PadSet& p)
{
    padsUi = p;
    padsX.write (p);
}

void ConfluenceAudioProcessor::setPad (int index, const Engine::Pad& p)
{
    if (! juce::isPositiveAndBelow (index, Engine::numPads)) return;
    padsUi.pads[(size_t) index] = p;
    padsX.write (padsUi);
}

void ConfluenceAudioProcessor::setLearned (const Engine::LearnedShape& l)
{
    learnedUi = l;
    learnedX.write (l);
}

void ConfluenceAudioProcessor::generatePads (uint32_t seed)
{
    const auto cs = readChordSettings();
    const int style = (int) std::lround (r.style->load());
    setPads (Harmony::generatePads (style, cs.scale, seed));
}

void ConfluenceAudioProcessor::pollMessageThread()
{
    const auto seq = learnResultSeq.load (std::memory_order_acquire);
    if (seq == learnSeenSeq) return;
    learnSeenSeq = seq;

    Engine::LearnedShape l;
    l.count = juce::jlimit (0, (int) Theory::maxChordTones, learnResultCount.load());
    for (int i = 0; i < l.count; ++i)
        l.iv[i] = (int8_t) juce::jlimit (0, 127, learnResult[(size_t) i].load());
    setLearned (l);

    // l'accord appris devient la forme jouée, en mode Accord fixe
    auto setChoice = [this] (const char* id, int index)
    {
        if (auto* p = apvts.getParameter (id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 ((float) index));
            p->endChangeGesture();
        }
    };
    setChoice (ParamIDs::fixedShape, Theory::fixedLearned);
    setChoice (ParamIDs::mode, Engine::modeFixed);
}

//==============================================================================
MidiExport::Options ConfluenceAudioProcessor::exportOptions (int bars) const
{
    MidiExport::Options o;
    o.bpm = lastBpm.load();
    o.bars = bars;
    const int strumSync = r.strumSync->load() > 0.5f;
    o.strumBeats = strumSync ? Choices::strumRateBeats ((int) std::lround (r.strumSyncRate->load()))
                             : (double) r.strum->load() * 0.001 * o.bpm / 60.0;
    o.strumDir = (int) std::lround (r.strumDir->load());
    o.velocity = (int) std::lround (r.velMode->load()) == 1 ? (int) std::lround (r.velFixed->load()) : 100;
    o.fixedDuration = (int) std::lround (r.durMode->load()) == 1;
    o.durationBeats = Choices::durationBeats ((int) std::lround (r.duration->load()));
    return o;
}

bool ConfluenceAudioProcessor::exportMidi (const juce::File& file, int bars)
{
    const auto mf = MidiExport::progression (readChordSettings(), padsUi, learnedUi, exportOptions (bars));
    return MidiExport::write (mf, file);
}

juce::File ConfluenceAudioProcessor::getExportFolder()
{
    return juce::File::getSpecialLocation (juce::File::userMusicDirectory).getChildFile ("Confluence");
}

//==============================================================================
juce::ValueTree ConfluenceAudioProcessor::createStateTree()
{
    auto tree = apvts.copyState();
    for (auto name : { "PADS", "LEARNED" })
        if (auto old = tree.getChildWithName (name); old.isValid())
            tree.removeChild (old, nullptr);

    juce::ValueTree padsTree ("PADS");
    for (const auto& p : padsUi.pads)
    {
        juce::ValueTree pt ("PAD");
        pt.setProperty ("degree", (int) p.degree, nullptr);
        pt.setProperty ("type", (int) p.type, nullptr);
        pt.setProperty ("inversion", (int) p.inversion, nullptr);
        pt.setProperty ("octave", (int) p.octave, nullptr);
        padsTree.appendChild (pt, nullptr);
    }
    tree.appendChild (padsTree, nullptr);

    juce::ValueTree learnedTree ("LEARNED");
    juce::StringArray iv;
    for (int i = 0; i < learnedUi.count; ++i) iv.add (juce::String ((int) learnedUi.iv[i]));
    learnedTree.setProperty ("intervals", iv.joinIntoString (" "), nullptr);
    tree.appendChild (learnedTree, nullptr);
    return tree;
}

void ConfluenceAudioProcessor::applyStateTree (const juce::ValueTree& source, bool keepRoutingParams)
{
    // un preset reçu d'un inconnu ou un projet abîmé peut contenir n'importe quoi :
    // chaque valeur est vérifiée (NaN, infini -> valeur par défaut ; sinon ramenée dans sa plage)
    auto tree = source.createCopy();
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
        {
            auto child = tree.getChildWithProperty ("id", rp->getParameterID());
            if (! child.isValid()) continue;
            const double raw = child.getProperty ("value").toString().getDoubleValue();
            const auto& range = rp->getNormalisableRange();
            const float v = std::isfinite (raw) ? juce::jlimit (range.start, range.end, (float) raw)
                                                : rp->convertFrom0to1 (rp->getDefaultValue());
            child.setProperty ("value", v, nullptr);
        }

    if (keepRoutingParams)
    {
        // preset utilisateur : on garde le canal de sortie, la Preview et la note de départ des pads
        for (auto* p : getParameters())
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*> (p))
            {
                if (PresetManager::isKeptParam (rp->getParameterID()))
                    continue;
                auto child = tree.getChildWithProperty ("id", rp->getParameterID());
                const float v = child.isValid() ? (float) child.getProperty ("value") : rp->convertFrom0to1 (rp->getDefaultValue());
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (rp->convertTo0to1 (v));
                rp->endChangeGesture();
            }
    }
    else
        apvts.replaceState (tree.createCopy());

    if (auto padsTree = tree.getChildWithName ("PADS"); padsTree.isValid())
    {
        Engine::PadSet ps;
        for (int i = 0; i < juce::jmin (Engine::numPads, padsTree.getNumChildren()); ++i)
        {
            const auto pt = padsTree.getChild (i);
            auto& p = ps.pads[(size_t) i];
            p.degree    = (int8_t) juce::jlimit (0, 11, (int) pt.getProperty ("degree", 0));
            p.type      = (int8_t) juce::jlimit (0, Theory::numChordTypes - 1, (int) pt.getProperty ("type", (int) Theory::ctSeventh));
            p.inversion = (int8_t) juce::jlimit (0, 4, (int) pt.getProperty ("inversion", 4));
            p.octave    = (int8_t) juce::jlimit (-2, 2, (int) pt.getProperty ("octave", 0));
        }
        setPads (ps);
    }

    Engine::LearnedShape l;
    if (auto lt = tree.getChildWithName ("LEARNED"); lt.isValid())
    {
        auto tokens = juce::StringArray::fromTokens (lt.getProperty ("intervals").toString(), " ", "");
        tokens.removeEmptyStrings();
        for (int i = 0; i < juce::jmin ((int) Theory::maxChordTones, tokens.size()); ++i)
            l.iv[l.count++] = (int8_t) juce::jlimit (0, 127, tokens[i].getIntValue());
    }
    setLearned (l);
}

void ConfluenceAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void ConfluenceAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    applyStateTree (juce::ValueTree::fromXml (*xml), false);
    notifyPresetChange();
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ConfluenceAudioProcessor();
}
