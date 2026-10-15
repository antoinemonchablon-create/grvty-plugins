// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie d'Undertow, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "PluginProcessor.h"
#include "PluginEditor.h"

#if UNDERTOW_PROFILE
 #include <chrono>
 namespace { inline double nowUs() { return std::chrono::duration<double, std::micro> (std::chrono::steady_clock::now().time_since_epoch()).count(); } }
 #define PROF_BEGIN(name) const double name##_t0 = nowUs();
 #define PROF_END(name, field) profile.field += nowUs() - name##_t0;
#else
 #define PROF_BEGIN(name)
 #define PROF_END(name, field)
#endif

//==============================================================================
UndertowAudioProcessor::UndertowAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", Params::createLayout())
{
    namespace id = ParamIDs;
    r.legato = raw (id::legato);       r.glide = raw (id::glide);           r.velSens = raw (id::velSens);
    r.shapeA = raw (id::oscAShape);    r.coarseA = raw (id::oscACoarse);    r.fineA = raw (id::oscAFine);
    r.shapeB = raw (id::oscBShape);    r.ratio = raw (id::oscBRatio);       r.free = raw (id::oscBFree);
    r.freeRatio = raw (id::oscBFreeRatio); r.fineB = raw (id::oscBFine);   r.mix = raw (id::oscMix);
    r.index = raw (id::fmIndex);       r.feedback = raw (id::fmFeedback);   r.fmMode = raw (id::fmMode);
    r.modA = raw (id::modAttack);      r.modD = raw (id::modDecay);         r.modC = raw (id::modCurve);
    r.modIdx = raw (id::modToIndex);   r.modCut = raw (id::modToCutoff);    r.modPitch = raw (id::modToPitch);
    r.fType = raw (id::filterType);    r.fSlope = raw (id::filterSlope);    r.cutoff = raw (id::cutoff);
    r.res = raw (id::resonance);       r.fEnv = raw (id::filterEnv);        r.fDrive = raw (id::filterDrive);
    r.keyTrack = raw (id::keyTrack);
    r.att = raw (id::ampAttack);       r.dec = raw (id::ampDecay);          r.sus = raw (id::ampSustain);   r.rel = raw (id::ampRelease);
    r.fxDrive = raw (id::fxDrive);     r.dTime = raw (id::delayTime);       r.dFb = raw (id::delayFeedback);
    r.dTone = raw (id::delayTone);     r.dMix = raw (id::delayMix);
    r.rSize = raw (id::reverbSize);    r.rDamp = raw (id::reverbDamp);      r.rMix = raw (id::reverbMix);
    r.os = raw (id::oversampling);     r.outGain = raw (id::outGain);
    r.seqMode = raw (id::seqMode);     r.seqLength = raw (id::seqLength);   r.seqRate = raw (id::seqRate);
    r.swing = raw (id::swing);         r.scale = raw (id::scale);           r.root = raw (id::rootNote);
    r.octave = raw (id::seqOctave);    r.accent = raw (id::accent);
    r.laneDest = raw (id::modLaneDest); r.laneLength = raw (id::modLaneLength); r.laneDepth = raw (id::modLaneDepth);
    r.mutAmt = raw (id::mutateAmount); r.mutBars = raw (id::mutateBars);

    for (int i = 0; i < Params::numLfos; ++i)
    {
        r.lShape[(size_t) i]    = raw (id::lfoShape (i));
        r.lSync[(size_t) i]     = raw (id::lfoSync (i));
        r.lRate[(size_t) i]     = raw (id::lfoRate (i));
        r.lSyncRate[(size_t) i] = raw (id::lfoSyncRate (i));
        r.lDest[(size_t) i]     = raw (id::lfoDest (i));
        r.lDepth[(size_t) i]    = raw (id::lfoDepth (i));
        r.lRetrig[(size_t) i]   = raw (id::lfoRetrig (i));
    }

    for (int i = 0; i < 2; ++i)
        oversamplers[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (
            1, (size_t) (i + 1), juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false);

    // une nouvelle instance s'ouvre directement sur le premier preset : Play = une boucle qui sonne
    presets.loadFactoryPreset (0);
}

bool UndertowAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void UndertowAudioProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;

    for (auto& os : oversamplers)
    {
        os->initProcessing ((size_t) chunkSize);
        os->reset();
    }
    activeOs = requestedOs = (int) r.os->load();
    osSwitchGain = 1.0f;
    osSwitching = false;

    voice.prepare (sampleRate);
    voice.setOversampling (activeOs == 0 ? 2 : 4);
    sequencer.reset();
    delay.prepare (sampleRate, 5.0);
    reverb.prepare (sampleRate);

    zeroBuffer.setSize (1, chunkSize);
    zeroBuffer.clear();
    monoBuffer.setSize (1, chunkSize);
    scratch.setSize (2, chunkSize);

    outGainSm.reset (sampleRate, 0.03);
    outGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (r.outGain->load()));

    lfoSmooth    = FastMath::onePoleCoeff (1.0f, sampleRate);
    weightSmooth = FastMath::onePoleCoeff (10.0f, sampleRate);
    for (auto& l : lfos) l = LfoState {};
    lfoClock = 0.0;
    laneValue = 0.0f;
    numHeld = 0;
    samplesProcessed = 0;
    lastMode = -1;
}

//==============================================================================
void UndertowAudioProcessor::readSettings()
{
    auto& s = voice.settings;
    s.legato = r.legato->load() > 0.5f;
    s.glideMs = r.glide->load();
    s.velSens = r.velSens->load();
    s.shapeA = r.shapeA->load();
    s.coarseA = r.coarseA->load();
    s.fineA = r.fineA->load();
    s.shapeB = r.shapeB->load();
    s.ratioIndex = (int) r.ratio->load();
    s.freeRatio = r.free->load() > 0.5f;
    s.freeRatioValue = r.freeRatio->load();
    s.fineB = r.fineB->load();
    s.mix = r.mix->load();
    s.index = r.index->load();
    s.feedback = r.feedback->load();
    s.fmMode = (int) r.fmMode->load();
    s.modAttack = r.modA->load();
    s.modDecay = r.modD->load();
    s.modCurve = r.modC->load();
    s.modToIndex = r.modIdx->load();
    s.modToCutoff = r.modCut->load();
    s.modToPitch = r.modPitch->load();
    s.filterType = (int) r.fType->load();
    s.filterSlope = (int) r.fSlope->load();
    s.cutoff = r.cutoff->load();
    s.resonance = r.res->load();
    s.filterEnv = r.fEnv->load();
    s.filterDrive = r.fDrive->load();
    s.keyTrack = r.keyTrack->load();
    s.attack = r.att->load();
    s.decay = r.dec->load();
    s.sustain = r.sus->load();
    s.release = r.rel->load();
    s.fxDrive = r.fxDrive->load();
    s.accentAmount = r.accent->load();
    voice.updateEnvelopeTimes();

    seqSettings.length = (int) r.seqLength->load();
    seqSettings.rate = (int) r.seqRate->load();
    seqSettings.swing = r.swing->load();
    seqSettings.scale = (int) r.scale->load();
    seqSettings.root = (int) r.root->load();
    seqSettings.octave = (int) r.octave->load();
    seqSettings.laneLength = (int) r.laneLength->load();
    seqSettings.mutateAmount = r.mutAmt->load();
    seqSettings.mutateBars = Choices::mutateBarValues[juce::jlimit (0, 4, (int) r.mutBars->load())];
    seqSettings.transpose = transpose;
}

//==============================================================================
void UndertowAudioProcessor::startNote (int note, float velocity, bool accent, bool slide, float lockIndex, float lockCutoff)
{
    const bool willRetrigger = ! ((voice.isGateOn() || slide) && voice.isActive());
    voice.noteOn (note, velocity, accent, slide, lockIndex, lockCutoff);

    if (willRetrigger || ! voice.settings.legato)
        for (int i = 0; i < Params::numLfos; ++i)
            if (r.lRetrig[(size_t) i]->load() > 0.5f)
            {
                lfos[(size_t) i].anchor = lfoClock;
                lfos[(size_t) i].freePhase = 0.0;
            }

    displayNote.store (note, std::memory_order_relaxed);
    lastNoteVelocity.store (velocity, std::memory_order_relaxed);
    lastNoteAccent.store (accent, std::memory_order_relaxed);
    noteCounter.fetch_add (1, std::memory_order_relaxed);
}

void UndertowAudioProcessor::handleMidi (const juce::MidiMessage& m, bool keysMode)
{
    const float nan = std::numeric_limits<float>::quiet_NaN();

    if (m.isNoteOn())
    {
        if (! keysMode)
        {
            transpose = juce::jlimit (-36, 36, m.getNoteNumber() - 60);   // la séquence est transposée (référence C3)
            return;
        }
        if (numHeld < (int) heldNotes.size())
            heldNotes[(size_t) numHeld++] = m.getNoteNumber();
        startNote (m.getNoteNumber(), m.getFloatVelocity(), false, false, nan, nan);
    }
    else if (m.isNoteOff())
    {
        if (! keysMode)
            return;

        const int n = m.getNoteNumber();
        for (int i = 0; i < numHeld; ++i)
            if (heldNotes[(size_t) i] == n)
            {
                for (int j = i; j < numHeld - 1; ++j) heldNotes[(size_t) j] = heldNotes[(size_t) j + 1];
                --numHeld;
                break;
            }

        if (numHeld > 0)
        {
            if (voice.getCurrentNote() == n)   // retour à la note précédente encore tenue, en legato
                startNote (heldNotes[(size_t) numHeld - 1], 0.8f, false, false, nan, nan);
        }
        else
        {
            voice.noteOff();
        }
    }
    else if (m.isAllNotesOff() || m.isAllSoundOff())
    {
        numHeld = 0;
        voice.noteOff();
    }
}

//==============================================================================
void UndertowAudioProcessor::advanceLfos (double qps) noexcept
{
    lfoClock += qps;
    for (int i = 0; i < Params::numLfos; ++i)
    {
        auto& l = lfos[(size_t) i];
        if (r.lSync[(size_t) i]->load() < 0.5f)
        {
            l.freePhase += (double) r.lRate[(size_t) i]->load() / sampleRate;
            if (l.freePhase >= 1.0) { l.freePhase -= std::floor (l.freePhase); ++l.freeCycle; }
        }
    }
}

void UndertowAudioProcessor::computeMods (VoiceMods& m, float& pan) noexcept
{
    m = VoiceMods {};
    pan = 0.0f;

    for (int i = 0; i < Params::numLfos; ++i)
    {
        auto& l = lfos[(size_t) i];
        const int shape = (int) r.lShape[(size_t) i]->load();
        const int dest  = (int) r.lDest[(size_t) i]->load();
        const float depth = r.lDepth[(size_t) i]->load();

        double ph;
        int64_t cycle;
        if (r.lSync[(size_t) i]->load() > 0.5f)
        {
            const double period = Choices::lfoSyncQuarters[juce::jlimit (0, 17, (int) r.lSyncRate[(size_t) i]->load())];
            const double pos = (lfoClock - l.anchor) / period;
            cycle = (int64_t) std::floor (pos);
            ph = pos - (double) cycle;
        }
        else
        {
            ph = l.freePhase;
            cycle = l.freeCycle;
        }

        const float v = Lfo::shapeValue (shape, (float) ph, cycle, 0xA5u + (uint32_t) i * 977u);
        l.out += (v - l.out) * lfoSmooth;

        for (int d = 1; d < Choices::numLfoDests; ++d)
        {
            const float target = d == dest ? depth : 0.0f;
            l.weight[(size_t) d] += (target - l.weight[(size_t) d]) * weightSmooth;
        }

        const auto& w = l.weight;
        const float o = l.out;
        using namespace Choices;
        m.pitch     += w[dPitch] * 24.0f * o;
        m.index     += w[dIndex] * 10.0f * o;
        m.ratioOct  += w[dRatio] * 2.0f * o;
        m.feedback  += w[dFeedback] * o;
        m.cutoff    += w[dCutoff] * 60.0f * o;
        m.resonance += w[dResonance] * o;
        m.shapeA    += w[dShapeA] * o;
        m.shapeB    += w[dShapeB] * o;
        m.mix       += w[dMix] * o;
        m.drive     += w[dDrive] * o;
        pan         += w[dPan] * o;

        const float wa = w[dAmp];
        const float u = wa >= 0.0f ? o : -o;
        m.ampGain *= 1.0f - std::abs (wa) * (1.0f - u) * 0.5f;
    }

    // piste Mod du séquenceur (valeur tenue pendant le pas)
    const int laneDest = (int) r.laneDest->load();
    const float lv = laneValue * r.laneDepth->load();
    if (laneDest == Choices::laneIndex)       m.index  += lv * 10.0f;
    else if (laneDest == Choices::laneCutoff) m.cutoff += lv * 48.0f;
    else if (laneDest == Choices::laneRatio)
    {
        if (voice.settings.freeRatio) m.ratioOct += lv * 2.0f;
        else                          m.ratioSteps = (int) std::lround (lv * 11.0f);
    }
}

//==============================================================================
void UndertowAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    readSettings();

    // --- mode SEQ / KEYS
    const int mode = (int) r.seqMode->load();
    const bool keysMode = mode == 1;
    if (mode != lastMode)
    {
        voice.noteOff();
        numHeld = 0;
        sequencer.reset();
        lastMode = mode;
    }

    // --- transport
    bool playing = false;
    double ppq = 0.0, bpm = 120.0, barQ = 4.0;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = juce::jlimit (20.0, 999.0, *b);
            if (auto p = pos->getPpqPosition()) ppq = *p; else playing = false;
            if (auto ts = pos->getTimeSignature()) barQ = ts->numerator * 4.0 / juce::jmax (1, ts->denominator);
        }
    transportPlaying.store (playing, std::memory_order_relaxed);
    const double qps = bpm / (60.0 * sampleRate);
    if (playing)
        lfoClock = ppq;

    // --- notes MIDI d'abord (en mode SEQ elles règlent la transposition du bloc)
    for (const auto meta : midi)
        if (! keysMode && meta.getMessage().isNoteOn())
            handleMidi (meta.getMessage(), false);
    seqSettings.transpose = transpose;

    const auto& pat = pattern.acquire();
    const int numEvents = keysMode ? 0 : sequencer.process (pat, seqSettings, playing, ppq, bpm, barQ, sampleRate, numSamples);

    // --- paramètres des effets (une fois par bloc, lissés à l'intérieur)
    const double delayQ = Choices::delayQuarters[juce::jlimit (0, 9, (int) r.dTime->load())];
    delay.setParameters ((float) (delayQ * 60.0 / bpm * sampleRate), r.dFb->load(), r.dTone->load(), r.dMix->load());
    reverb.setParameters (r.rSize->load(), r.rDamp->load(), r.rMix->load());
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (r.outGain->load()));

    requestedOs = (int) r.os->load();
    if (requestedOs != activeOs)
        osSwitching = true;

    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);

    auto midiIt = midi.begin();
    int evIndex = 0;
    float peak = 0.0f;

    for (int chunkStart = 0; chunkStart < numSamples; chunkStart += chunkSize)
    {
        const int n = juce::jmin (chunkSize, numSamples - chunkStart);
        auto& os = *oversamplers[(size_t) activeOs];
        const int factor = activeOs == 0 ? 2 : 4;

        PROF_BEGIN (v)
        juce::dsp::AudioBlock<const float> zeros (zeroBuffer.getArrayOfReadPointers(), 1, (size_t) n);
        auto up = os.processSamplesUp (zeros);
        float* osData = up.getChannelPointer (0);

        float panVals[chunkSize];

        for (int i = 0; i < n; ++i)
        {
            const int s = chunkStart + i;

            // événements MIDI (mode KEYS) à cet échantillon
            if (keysMode)
                while (midiIt != midi.end() && (*midiIt).samplePosition <= s)
                {
                    handleMidi ((*midiIt).getMessage(), true);
                    ++midiIt;
                }

            // événements du séquenceur à cet échantillon
            while (evIndex < numEvents && sequencer.getEvent (evIndex).sample <= s)
            {
                const auto& e = sequencer.getEvent (evIndex++);
                if (e.type == Sequencer::Event::lane)
                    laneValue = e.laneValue;
                else if (e.type == Sequencer::Event::noteOff)
                    voice.noteOff();
                else
                {
                    startNote (e.note, e.velocity, e.accent, e.slide,
                               e.lockIndex, e.lockCutoff);
                    if (triggerLog.enabled && triggerLog.onsets.size() < triggerLog.onsets.capacity())
                        triggerLog.onsets.emplace_back (samplesProcessed + s, e.globalStep);
                }
            }

            VoiceMods mods;
            computeMods (mods, panVals[i]);
            voice.renderBaseSample (mods, osData + (size_t) i * (size_t) factor);
            advanceLfos (qps);
        }
        PROF_END (v, voice)

        PROF_BEGIN (d)
        juce::dsp::AudioBlock<float> monoBlock (monoBuffer.getArrayOfWritePointers(), 1, (size_t) n);
        os.processSamplesDown (monoBlock);
        PROF_END (d, down)

        const float* mono = monoBuffer.getReadPointer (0);
        float* L = outL + chunkStart;
        float* R = outR + chunkStart;

        // changement de qualité : on baisse le son sur un morceau de bloc, on bascule, on remonte
        const float osMaxStep = (float) (1.0 / (0.005 * sampleRate));   // fondu de 5 ms

        PROF_BEGIN (dl)
        for (int i = 0; i < n; ++i)
        {
            // pendant la bascule, et 2 ms après (le nouveau filtre se stabilise), le son reste coupé
            const float osTarget = (osSwitching || osHold > 0) ? 0.0f : 1.0f;
            if (osHold > 0) --osHold;
            osSwitchGain += juce::jlimit (-osMaxStep, osMaxStep, osTarget - osSwitchGain);
            panSmoothed += (juce::jlimit (-1.0f, 1.0f, panVals[i]) - panSmoothed) * lfoSmooth;
            const float fadeGain = osSwitchGain * osSwitchGain * (3.0f - 2.0f * osSwitchGain);   // courbe en S
            const float x = mono[i] * fadeGain;
            const float angle = (panSmoothed + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
            L[i] = x * std::cos (angle) * juce::MathConstants<float>::sqrt2;
            R[i] = x * std::sin (angle) * juce::MathConstants<float>::sqrt2;
        }
        if (! delay.isSilent())
            for (int i = 0; i < n; ++i)
                delay.process (0.5f * (L[i] + R[i]), L[i], R[i]);
        PROF_END (dl, delay)

        PROF_BEGIN (rv)
        if (! reverb.isSilent())
            reverb.process (L, R, scratch.getWritePointer (0), scratch.getWritePointer (1), n);
        PROF_END (rv, reverb)

        if (osSwitching && osSwitchGain <= 1.0e-4f)
        {
            activeOs = requestedOs;
            oversamplers[(size_t) activeOs]->reset();
            voice.setOversampling (activeOs == 0 ? 2 : 4);
            osSwitching = false;
            osSwitchGain = 0.0f;
            osHold = (int) (0.002 * sampleRate);
        }

        // volume + limiteur doux de sécurité (jamais au-dessus de -0.2 dBFS)
        for (int i = 0; i < n; ++i)
        {
            const float g = outGainSm.getNextValue();
            for (float* ch : { L + i, R + i })
            {
                float x = *ch * g;
                const float ax = std::abs (x);
                if (ax > 0.8f)
                    x = std::copysign (0.8f + 0.18f * FastMath::tanh ((ax - 0.8f) * (1.0f / 0.18f)), x);
                *ch = x;
                peak = std::max (peak, std::abs (x));
            }
        }

        // oscilloscope : la sortie réelle, en mono
        {
            uint32_t w = scopeWrite.load (std::memory_order_relaxed);
            uint32_t ew = envWrite.load (std::memory_order_relaxed);
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (L[i] + R[i]);
                scope[(size_t) (w & (scopeSize - 1))].store (m, std::memory_order_relaxed);
                ++w;
                if (envAccCount == 0) { envAccMin = m; envAccMax = m; }
                else { envAccMin = juce::jmin (envAccMin, m); envAccMax = juce::jmax (envAccMax, m); }
                if (++envAccCount == envChunk)
                {
                    envMin[(size_t) (ew & (envSize - 1))].store (envAccMin, std::memory_order_relaxed);
                    envMax[(size_t) (ew & (envSize - 1))].store (envAccMax, std::memory_order_relaxed);
                    ++ew;
                    envAccCount = 0;
                }
            }
            scopeWrite.store (w, std::memory_order_release);
            envWrite.store (ew, std::memory_order_release);
        }
    }

    // MIDI restant (mode KEYS)
    if (keysMode)
        for (; midiIt != midi.end(); ++midiIt)
            handleMidi ((*midiIt).getMessage(), true);

    samplesProcessed += numSamples;
    outputLevel.store (juce::jmax (peak, outputLevel.load (std::memory_order_relaxed) * 0.8f), std::memory_order_relaxed);
    displayIndex.store (voice.getCurrentIndex(), std::memory_order_relaxed);
    displayRatio.store (voice.getCurrentRatio(), std::memory_order_relaxed);
    displayFreq.store (voice.getCurrentFrequency(), std::memory_order_relaxed);
    displaySampleRate.store (sampleRate, std::memory_order_relaxed);
    displayBpm.store (bpm, std::memory_order_relaxed);
    displayPpq.store (ppq + qps * numSamples, std::memory_order_relaxed);
    displayModEnv.store (voice.getModEnvValue(), std::memory_order_relaxed);
    displayAmpEnv.store (voice.getAmpEnvValue(), std::memory_order_relaxed);

   #if UNDERTOW_PROFILE
    ++profile.blocks;
   #endif
}

//==============================================================================
void UndertowAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto old = state.getChildWithName ("PATTERN"); old.isValid())
        state.removeChild (old, nullptr);
    state.appendChild (PatternIO::toValueTree (pattern.get()), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void UndertowAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    auto tree = juce::ValueTree::fromXml (*xml);
    auto pat = tree.getChildWithName ("PATTERN");
    if (pat.isValid())
    {
        pattern.edit() = PatternIO::fromValueTree (pat);
        pattern.publish();
        tree.removeChild (pat, nullptr);
    }
    apvts.replaceState (tree);
}

//==============================================================================
juce::AudioProcessorEditor* UndertowAudioProcessor::createEditor()
{
    return new UndertowAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new UndertowAudioProcessor();
}
