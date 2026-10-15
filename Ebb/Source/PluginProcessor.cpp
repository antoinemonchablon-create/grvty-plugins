// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Ebb, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "PluginProcessor.h"
#include "PluginEditor.h"

EbbAudioProcessor::EbbAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "EBB", Params::createLayout())
{
    namespace id = ParamIDs;
    r.amount    = apvts.getRawParameterValue (id::amount);
    r.curve     = apvts.getRawParameterValue (id::curve);
    r.rate      = apvts.getRawParameterValue (id::rate);
    r.trigger   = apvts.getRawParameterValue (id::trigger);
    r.nudge     = apvts.getRawParameterValue (id::nudge);
    r.threshold = apvts.getRawParameterValue (id::threshold);
    r.custom    = apvts.getRawParameterValue (id::custom);
    r.tension   = apvts.getRawParameterValue (id::tension);
    r.smooth    = apvts.getRawParameterValue (id::smooth);
    r.band      = apvts.getRawParameterValue (id::band);
    r.crossover = apvts.getRawParameterValue (id::crossover);

    shapeUi = Ebb::ShapeMath::defaultShape();
    undoStack.reserve (101);
    redoStack.reserve (101);
    publishShape();
    engine.prepare (48000.0);
    crossover.prepare (48000.0);
}

bool EbbAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainOut != juce::AudioChannelSet::mono() && mainOut != juce::AudioChannelSet::stereo())
        return false;
    if (mainIn != mainOut)
        return false;

    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }
    return true;
}

void EbbAudioProcessor::prepareToPlay (double sr, int)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    engine.prepare (sampleRate);
    crossover.prepare (sampleRate);
    xoverCur = r.crossover->load();
    crossover.setCutoff (xoverCur);
    xoverCoef = (float) (1.0 - std::exp (-16.0 / (0.03 * sampleRate)));   // fréquence lissée sur 30 ms (pas de 16 échantillons)
    bandRampLen = juce::jmax (1, (int) std::lround (0.04 * sampleRate));
    lastBand = getBand();
    bandW = { lastBand == Params::bandFull ? 1.0f : 0.0f, lastBand == Params::bandLow ? 1.0f : 0.0f, lastBand == Params::bandHigh ? 1.0f : 0.0f };
    bandRampLeft = 0;
    wasBypassed = false;
}

Ebb::Settings EbbAudioProcessor::readSettings() const noexcept
{
    Ebb::Settings s;
    s.amount      = r.amount->load() * 0.01f;
    s.curve       = (int) std::lround (r.curve->load());
    s.rate        = (int) std::lround (r.rate->load());
    s.trigger     = (int) std::lround (r.trigger->load());
    s.nudgeMs     = r.nudge->load();
    s.thresholdDb = r.threshold->load();
    s.custom      = r.custom->load() > 0.5f;
    s.tension     = r.tension->load() * 0.01f;
    s.smooth      = r.smooth->load() * 0.01f;
    return s;
}

void EbbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    auto mainBuf = getBusBuffer (buffer, false, 0);
    // canaux de sortie en trop (ne devrait pas arriver : sortie = entrée)
    for (int c = getMainBusNumInputChannels(); c < getMainBusNumOutputChannels(); ++c)
        buffer.clear (c, 0, numSamples);

    // --- sidechain
    const float* scPtr[2] = { nullptr, nullptr };
    int scCh = 0;
    if (getBusCount (true) > 1)
        if (auto* bus = getBus (true, 1); bus != nullptr && bus->isEnabled())
        {
            auto scBuf = getBusBuffer (buffer, true, 1);
            scCh = juce::jmin (2, scBuf.getNumChannels());
            for (int c = 0; c < scCh; ++c) scPtr[c] = scBuf.getReadPointer (c);
        }
    sidechainLive.store (scCh > 0, std::memory_order_relaxed);

    // --- position de l'hôte
    Ebb::Transport t;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            t.playing = pos->getIsPlaying();
            if (auto bpm = pos->getBpm()) { t.bpm = *bpm; t.hasBpm = *bpm > 1.0; }
            if (auto ppq = pos->getPpqPosition()) { t.ppq = *ppq; t.hasPosition = true; }
            if (auto sig = pos->getTimeSignature(); sig && sig->numerator > 0 && sig->denominator > 0)
                t.barBeats = 4.0 * (double) sig->numerator / (double) sig->denominator;
            if (auto bar = pos->getPpqPositionOfLastBarStart()) { t.barStartPpq = *bar; t.hasBarStart = true; }
        }
    if (t.hasBpm) lastBpm.store (t.bpm, std::memory_order_relaxed);

    auto s = readSettings();
    {
        // courbe perso : la dernière version publiée par l'interface (sans verrou)
        const auto& ct = customX.read();
        s.customTable = ct.r.data();
        s.customVersion = ct.version;
    }

    // --- notes MIDI (seulement utiles en mode MIDI, mais lues dans tous les cas pour vider le tampon)
    int numMidi = 0;
    for (const auto meta : midi)
    {
        const auto msg = meta.getMessage();
        if (msg.isNoteOn() && numMidi < (int) midiOffsets.size())
            midiOffsets[(size_t) numMidi++] = juce::jlimit (0, juce::jmax (0, numSamples - 1), meta.samplePosition);
    }
    midi.clear();

    wasBypassed = false;

    // --- multibande : nouveau mode -> fondu de 40 ms ; la séparation repart de zéro si elle ne tournait pas
    const int band = juce::jlimit (0, 2, getBand());
    if (band != lastBand)
    {
        if (bandW[0] >= 1.0f && bandRampLeft == 0)
            crossover.reset();
        for (int i = 0; i < 3; ++i)
            bandStep[(size_t) i] = ((i == band ? 1.0f : 0.0f) - bandW[(size_t) i]) / (float) bandRampLen;
        bandRampLeft = bandRampLen;
        lastBand = band;
    }
    const float xoverTarget = r.crossover->load();
    const int numCh = mainBuf.getNumChannels();

    // --- par morceaux de 1024 échantillons au plus (tableau de gains fixe : aucune allocation)
    const int chunkMax = (int) gains.size();
    const double qps = t.bpm / (60.0 * sampleRate);
    int midiIndex = 0;
    for (int start = 0; start < numSamples; start += chunkMax)
    {
        const int n = juce::jmin (chunkMax, numSamples - start);
        Ebb::Transport ct = t;
        if (t.playing && t.hasPosition) ct.ppq = t.ppq + (double) start * qps;

        // notes de ce morceau, ramenées au début du morceau
        int localOffsets[512];
        int numLocal = 0;
        while (midiIndex < numMidi && midiOffsets[(size_t) midiIndex] < start + n)
            localOffsets[numLocal++] = midiOffsets[(size_t) midiIndex++] - start;

        const float* scChunk[2] = { scPtr[0] != nullptr ? scPtr[0] + start : nullptr,
                                    scPtr[1] != nullptr ? scPtr[1] + start : nullptr };
        engine.process (s, ct, n, localOffsets, numLocal, scChunk, scCh, gains.data());

        if (bandRampLeft == 0 && band == Params::bandFull)
        {
            // pleine bande : le gain tel quel (à Amount 0 %, le son ressort identique à l'échantillon près)
            for (int c = 0; c < numCh; ++c)
                juce::FloatVectorOperations::multiply (mainBuf.getWritePointer (c, start), gains.data(), n);
            continue;
        }

        float* ch[2] = { mainBuf.getWritePointer (0, start), numCh > 1 ? mainBuf.getWritePointer (1, start) : nullptr };
        for (int i = 0; i < n; ++i)
        {
            if ((i & 15) == 0 && std::abs (xoverCur - xoverTarget) > 0.01f)
            {
                xoverCur *= std::exp (std::log (xoverTarget / xoverCur) * xoverCoef);
                if (std::abs (xoverCur - xoverTarget) < 0.05f) xoverCur = xoverTarget;
                crossover.setCutoff (xoverCur);
            }
            if (bandRampLeft > 0)
            {
                for (int k = 0; k < 3; ++k) bandW[(size_t) k] += bandStep[(size_t) k];
                if (--bandRampLeft == 0)
                    bandW = { band == Params::bandFull ? 1.0f : 0.0f, band == Params::bandLow ? 1.0f : 0.0f, band == Params::bandHigh ? 1.0f : 0.0f };
            }
            const float g = gains[(size_t) i];
            // plein : g x   |   graves : g graves + aigus   |   aigus : graves + g aigus
            const float wLowBand  = bandW[1] * g + bandW[2];
            const float wHighBand = bandW[1] + bandW[2] * g;
            const float wFull     = bandW[0] * g;
            for (int c = 0; c < numCh && c < 2; ++c)
            {
                const float x = ch[c][i];
                float lo, hi;
                crossover.process (c, x, lo, hi);
                ch[c][i] = wFull * x + wLowBand * lo + wHighBand * hi;
            }
        }
        crossover.snapToZero();
    }
}

void EbbAudioProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    // signal intact ; la courbe repart de 1 (sans saut) quand on sort du bypass
    engine.skip (buffer.getNumSamples());
    wasBypassed = true;
    midi.clear();
    for (int c = getMainBusNumInputChannels(); c < getMainBusNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());
}

//==============================================================================
// Courbe perso
//==============================================================================
void EbbAudioProcessor::publishShape()
{
    Ebb::ShapeMath::render (shapeUi, uiTable.r.data(), Ebb::CurveTables::size);
    uiTable.version = ++shapeVersion;
    customX.write (uiTable);
}

void EbbAudioProcessor::setParam (const char* id, float realValue)
{
    if (auto* p = apvts.getParameter (id))
    {
        const float v = p->convertTo0to1 (realValue);
        if (std::abs (p->getValue() - v) < 1.0e-6f)
            return;
        p->beginChangeGesture();
        p->setValueNotifyingHost (v);
        p->endChangeGesture();
    }
}

void EbbAudioProcessor::beginShapeGesture()
{
    if (gestureOpen)
        return;
    gestureOpen = true;
    if (undoStack.size() >= 100) undoStack.erase (undoStack.begin());
    undoStack.push_back (shapeUi);
    redoStack.clear();
}

void EbbAudioProcessor::endShapeGesture()
{
    // un clic sans mouvement ne laisse pas d'entrée « annuler » inutile
    if (gestureOpen && ! undoStack.empty() && undoStack.back() == shapeUi)
        undoStack.pop_back();
    gestureOpen = false;
}

void EbbAudioProcessor::setShape (const Ebb::ShapeData& s, bool undoable, bool selectCustom)
{
    auto clean = s;
    Ebb::ShapeMath::sanitize (clean);
    if (undoable && ! gestureOpen && clean != shapeUi)
    {
        if (undoStack.size() >= 100) undoStack.erase (undoStack.begin());
        undoStack.push_back (shapeUi);
        redoStack.clear();
    }
    shapeUi = clean;
    publishShape();
    if (selectCustom)
        setParam (ParamIDs::custom, 1.0f);
}

bool EbbAudioProcessor::undoShape()
{
    if (undoStack.empty())
        return false;
    redoStack.push_back (shapeUi);
    shapeUi = undoStack.back();
    undoStack.pop_back();
    publishShape();
    setParam (ParamIDs::custom, 1.0f);
    return true;
}

bool EbbAudioProcessor::redoShape()
{
    if (redoStack.empty())
        return false;
    undoStack.push_back (shapeUi);
    shapeUi = redoStack.back();
    redoStack.pop_back();
    publishShape();
    setParam (ParamIDs::custom, 1.0f);
    return true;
}

void EbbAudioProcessor::copyFactoryToShape (int curve, bool undoable)
{
    setShape (Ebb::ShapeMath::fromTable (Ebb::CurveTables::get().table (curve), Ebb::CurveTables::size), undoable, true);
}

//==============================================================================
// État
//==============================================================================
juce::ValueTree EbbAudioProcessor::createStateTree()
{
    auto tree = apvts.copyState();
    for (auto name : { "SHAPE", "EDITOR" })
        if (auto old = tree.getChildWithName (name); old.isValid())
            tree.removeChild (old, nullptr);

    juce::ValueTree shape ("SHAPE");
    shape.setProperty ("points", ShapeLibrary::toText (shapeUi), nullptr);
    tree.appendChild (shape, nullptr);

    juce::ValueTree editor ("EDITOR");
    editor.setProperty ("grid", grid, nullptr);
    tree.appendChild (editor, nullptr);
    return tree;
}

void EbbAudioProcessor::applyStateTree (const juce::ValueTree& tree)
{
    apvts.replaceState (tree.createCopy());

    Ebb::ShapeData s = Ebb::ShapeMath::defaultShape();
    if (auto shape = tree.getChildWithName ("SHAPE"); shape.isValid())
        ShapeLibrary::fromText (shape.getProperty ("points").toString(), s);
    shapeUi = s;
    undoStack.clear();
    redoStack.clear();
    publishShape();

    if (auto editor = tree.getChildWithName ("EDITOR"); editor.isValid())
        grid = juce::jlimit (0, 32, (int) editor.getProperty ("grid", 8));
}

void EbbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = createStateTree().createXml())
        copyXmlToBinary (*xml, destData);
}

void EbbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;
    applyStateTree (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EbbAudioProcessor();
}
