// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Flow Filter, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
static const juce::StringArray syncDivisionNames { "1/16", "1/8", "1/4", "1/2", "1 bar",
                                                   "2 bars", "4 bars", "8 bars", "16 bars", "32 bars" };

double FlowFilterAudioProcessor::syncDivisionInQuarters (int index)
{
    static const double q[] = { 0.25, 0.5, 1.0, 2.0, 4.0, 8.0, 16.0, 32.0, 64.0, 128.0 };
    return q[juce::jlimit (0, 9, index)];
}

double FlowFilterAudioProcessor::bandFrequency (int b, int numBands)
{
    return FilterBank::lowestHz * std::pow (FilterBank::highestHz / FilterBank::lowestHz,
                                            numBands > 1 ? (double) b / (numBands - 1) : 0.5);
}

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout FlowFilterAudioProcessor::createParameterLayout()
{
    using namespace juce;
    using P = AudioParameterFloat;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    auto pct = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (roundToInt (v * 100.0f)) + " %"; });
    auto db  = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (v, 1) + " dB"; });
    auto hz  = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 1) + " kHz"
                                                : String (v, v < 10.0f ? 2 : 0) + " Hz"; });
    auto ms  = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return v >= 1000.0f ? String (v / 1000.0f, 2) + " s" : String (roundToInt (v)) + " ms"; });
    auto num = AudioParameterFloatAttributes().withStringFromValueFunction (
        [] (float v, int) { return String (v, 2); });

    NormalisableRange<float> rateRange (0.0f, 4.0f, 0.001f);     rateRange.setSkewForCentre (0.5f);
    NormalisableRange<float> multRange (0.25f, 8.0f, 0.001f);    multRange.setSkewForCentre (1.5f);
    NormalisableRange<float> freqRange (20.0f, 20000.0f, 0.1f);  freqRange.setSkewForCentre (630.0f);
    NormalisableRange<float> relRange  (5.0f, 2000.0f, 0.1f);    relRange.setSkewForCentre (150.0f);
    NormalisableRange<float> resRange  (0.7f, 20.0f, 0.01f);     resRange.setSkewForCentre (4.5f);
    NormalisableRange<float> modRange  (0.01f, 10.0f, 0.001f);   modRange.setSkewForCentre (0.5f);
    NormalisableRange<float> unit (0.0f, 1.0f), bipolar (-1.0f, 1.0f);

    auto add = [&] (auto p) { params.push_back (std::move (p)); };

    // --- Mouvement
    add (std::make_unique<P> (ParameterID { ParamIDs::rate, 1 }, "Rate", rateRange, 0.25f, hz));
    add (std::make_unique<AudioParameterBool>   (ParameterID { ParamIDs::sync, 1 },    "Sync",    false));
    add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::syncDiv, 1 }, "Sync Rate", syncDivisionNames, 5));
    add (std::make_unique<AudioParameterBool>   (ParameterID { ParamIDs::reverse, 1 }, "Reverse", false));
    add (std::make_unique<AudioParameterBool>   (ParameterID { ParamIDs::freeze, 1 },  "Freeze",  false));

    // --- Vague
    add (std::make_unique<P> (ParameterID { ParamIDs::multiply, 1 }, "Multiply", multRange, 1.5f, num));
    add (std::make_unique<P> (ParameterID { ParamIDs::ripple, 1 },   "Ripple",   unit, 0.3f, pct));
    add (std::make_unique<P> (ParameterID { ParamIDs::scan, 1 },     "Scan",     unit, 0.0f, pct));
    add (std::make_unique<P> (ParameterID { ParamIDs::offset, 1 },   "Offset",   unit, 0.0f, pct));
    add (std::make_unique<P> (ParameterID { ParamIDs::invert, 1 },   "Invert",   bipolar, 1.0f, pct));
    add (std::make_unique<P> (ParameterID { ParamIDs::gravity, 1 },  "Gravity",  unit, 0.5f, pct));

    // --- Filtres
    add (std::make_unique<P> (ParameterID { ParamIDs::depth, 1 },     "Depth",     NormalisableRange<float> (0.0f, 60.0f, 0.1f), 30.0f, db));
    add (std::make_unique<P> (ParameterID { ParamIDs::resonance, 1 }, "Resonance", resRange, 4.5f, num));
    add (std::make_unique<P> (ParameterID { ParamIDs::analog, 1 },    "Analog",    unit, 0.0f, pct));
    add (std::make_unique<P> (ParameterID { ParamIDs::areaLow, 1 },   "Area Low",  freqRange, 20.0f, hz));
    add (std::make_unique<P> (ParameterID { ParamIDs::areaHigh, 1 },  "Area High", freqRange, 20000.0f, hz));
    add (std::make_unique<P> (ParameterID { ParamIDs::areaGain, 1 },  "Area Gain", NormalisableRange<float> (-60.0f, 6.0f, 0.1f), 0.0f, db));
    add (std::make_unique<P> (ParameterID { ParamIDs::areaSlope, 1 }, "Area Slope", NormalisableRange<float> (0.0f, 3.0f, 0.01f), 0.5f,
                              AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 2) + " oct"; })));
    add (std::make_unique<P> (ParameterID { ParamIDs::release, 1 },   "Release",   relRange, 60.0f, ms));
    add (std::make_unique<P> (ParameterID { ParamIDs::width, 1 },     "Width",     unit, 1.0f, pct));

    // --- Sortie
    add (std::make_unique<P> (ParameterID { ParamIDs::inGain, 1 },  "Input",  NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, db));
    add (std::make_unique<P> (ParameterID { ParamIDs::outGain, 1 }, "Output", NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f, db));
    add (std::make_unique<P> (ParameterID { ParamIDs::mix, 1 },     "Mix",    unit, 1.0f, pct));

    // --- Modulateurs (un par cible : Offset, Invert, Gravity)
    const char* targets[] = { "Offset", "Invert", "Gravity" };
    for (int i = 0; i < numMods; ++i)
    {
        const String t (targets[i]);
        add (std::make_unique<AudioParameterChoice> (ParameterID { ParamIDs::modShape (i), 1 }, t + " Mod Shape",
                                                     StringArray { "Sine", "Triangle", "Ramp", "Square", "Random" }, 0));
        add (std::make_unique<P> (ParameterID { ParamIDs::modRate (i), 1 },  t + " Mod Rate",  modRange, 0.2f, hz));
        add (std::make_unique<P> (ParameterID { ParamIDs::modDepth (i), 1 }, t + " Mod Depth", unit, 0.0f, pct));
    }

    return { params.begin(), params.end() };
}

//==============================================================================
FlowFilterAudioProcessor::FlowFilterAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMETERS", createParameterLayout())
{
    namespace id = ParamIDs;
    pRate = p (id::rate);         pSync = p (id::sync);           pSyncDiv = p (id::syncDiv);
    pReverse = p (id::reverse);   pFreeze = p (id::freeze);
    pMultiply = p (id::multiply); pRipple = p (id::ripple);       pScan = p (id::scan);
    pOffset = p (id::offset);     pInvert = p (id::invert);       pGravity = p (id::gravity);
    pDepth = p (id::depth);       pResonance = p (id::resonance);
    pAnalog = p (id::analog);     pAreaLow = p (id::areaLow);     pAreaHigh = p (id::areaHigh);
    pAreaGain = p (id::areaGain); pAreaSlope = p (id::areaSlope); pRelease = p (id::release);     pWidth = p (id::width);
    pInGain = p (id::inGain);     pOutGain = p (id::outGain);     pMix = p (id::mix);

    for (int i = 0; i < numMods; ++i)
    {
        pModShape[(size_t) i] = p (id::modShape (i));
        pModRate[(size_t) i]  = p (id::modRate (i));
        pModDepth[(size_t) i] = p (id::modDepth (i));
    }

    for (auto& b : displayBands) b.store (1.0f);
    const float waveDefaults[] = { 1.5f, 0.3f, 0.0f, 0.0f, 1.0f, 0.5f };
    for (size_t i = 0; i < displayWave.size(); ++i) displayWave[i].store (waveDefaults[i]);
    for (auto& m : displayMods)  m.store (0.0f);
}

bool FlowFilterAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    const auto in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::stereo())
        return false;

    return in == juce::AudioChannelSet::stereo() || in == juce::AudioChannelSet::mono();
}

//==============================================================================
void FlowFilterAudioProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    filterBank.prepare (sampleRate);
    for (auto& m : mods) m.reset();
    phase = 0.0;

    inGainSm.reset (sampleRate, 0.02);
    outGainSm.reset (sampleRate, 0.02);
    mixSm.reset (sampleRate, 0.02);
    inGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pInGain->load()));
    outGainSm.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    mixSm.setCurrentAndTargetValue (pMix->load());
}

//==============================================================================
void FlowFilterAudioProcessor::updateBandTargets (const WaveEngine::Params& wp, float depthDb)
{
    wave.setState (phase, wp);

    displayPhase.store (phase, std::memory_order_relaxed);
    const float wv[] = { wp.multiply, wp.ripple, wp.scan, wp.offset, wp.invert, wp.gravity };
    for (size_t i = 0; i < displayWave.size(); ++i)
        displayWave[i].store (wv[i], std::memory_order_relaxed);

    const int   numBands = filterBank.getNumBands();
    const float areaLo   = juce::jmin (pAreaLow->load(), pAreaHigh->load());
    const float areaHi   = juce::jmax (pAreaLow->load(), pAreaHigh->load());
    const float areaGain = juce::Decibels::decibelsToGain (pAreaGain->load());
    const float slopeOct = pAreaSlope->load();

    for (int b = 0; b < numBands; ++b)
    {
        const auto f = (float) filterBank.getBandFrequency (b);

        // amplitude de la vague à la position de la bande -> gain en dB
        const float a = wave.valueAt ((float) b / (float) (numBands - 1));
        const float modulated = juce::Decibels::decibelsToGain (-depthDb * (1.0f - a), -100.0f);

        // distance (en octaves) hors de la zone modulée
        float outside = 0.0f;
        if (f < areaLo) outside = std::log2 (areaLo / f);
        if (f > areaHi) outside = std::log2 (f / areaHi);

        // Slope : fondu progressif entre bande modulée et niveau fixe
        float t = 0.0f;
        if (outside > 0.0f)
            t = slopeOct <= 0.001f ? 1.0f : juce::jmin (1.0f, outside / slopeOct);

        const float g = modulated + (areaGain - modulated) * t;

        filterBank.setTargetGain (b, g);
        displayBands[(size_t) b].store (g, std::memory_order_relaxed);
    }
    displayNumBands.store (numBands, std::memory_order_relaxed);

}

//==============================================================================
void FlowFilterAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0 || buffer.getNumChannels() < 2)
        return;

    if (getTotalNumInputChannels() == 1)
        buffer.copyFrom (1, 0, buffer, 0, 0, numSamples);

    // --- Paramètres lus une fois par bloc
    filterBank.setQ (pResonance->load());
    filterBank.setWidth (pWidth->load());
    filterBank.setReleaseMs (pRelease->load());

    inGainSm.setTargetValue  (juce::Decibels::decibelsToGain (pInGain->load()));
    outGainSm.setTargetValue (juce::Decibels::decibelsToGain (pOutGain->load()));
    mixSm.setTargetValue     (pMix->load());

    const bool   synced    = pSync->load() > 0.5f;
    const bool   frozen    = pFreeze->load() > 0.5f;
    const double direction = pReverse->load() > 0.5f ? -1.0 : 1.0;
    const double cycleQuarters = syncDivisionInQuarters ((int) pSyncDiv->load());
    const float  analog    = pAnalog->load();

    // --- Tempo de l'hôte
    double bpm = 120.0;
    std::optional<double> ppqAtStart;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto b = pos->getBpm()) bpm = *b;
            if (pos->getIsPlaying())
                if (auto ppq = pos->getPpqPosition())
                    ppqAtStart = *ppq;
        }

    const double cyclesPerSecond = synced ? (bpm / 60.0) / cycleQuarters : (double) pRate->load();

    WaveEngine::Params wp;
    wp.multiply = pMultiply->load();
    wp.ripple   = pRipple->load();
    wp.scan     = pScan->load();
    wp.offset   = pOffset->load();
    wp.invert   = pInvert->load();
    wp.gravity  = pGravity->load();

    auto* left  = buffer.getWritePointer (0);
    auto* right = buffer.getWritePointer (1);
    float inPeak = 0.0f, outPeak = 0.0f;

    for (int start = 0; start < numSamples; start += controlBlockSize)
    {
        const int n = juce::jmin (controlBlockSize, numSamples - start);
        const double dt = (double) n / sampleRate;

        // --- Position de la vague
        if (! frozen)
        {
            if (synced && ppqAtStart.has_value())
            {
                const double ppq = *ppqAtStart + ((double) start / sampleRate) * bpm / 60.0;
                phase = direction * ppq / cycleQuarters;
            }
            else
            {
                phase += direction * cyclesPerSecond * dt;
                phase -= std::floor (phase);
            }
        }

        // --- Modulateurs : Offset, Invert, Gravity
        auto pw = wp;
        std::array<float, numMods> mv {};
        for (int i = 0; i < numMods; ++i)
        {
            mv[(size_t) i] = mods[(size_t) i].advance (pModRate[(size_t) i]->load(), (int) pModShape[(size_t) i]->load(), dt)
                           * pModDepth[(size_t) i]->load();
            displayMods[(size_t) i].store (mv[(size_t) i], std::memory_order_relaxed);
        }
        pw.offset  = pw.offset + mv[0] * 0.5f;
        pw.invert  = juce::jlimit (-1.0f, 1.0f, pw.invert + mv[1]);
        pw.gravity = juce::jlimit (0.0f, 1.0f, pw.gravity + mv[2] * 0.5f);

        filterBank.updateAnalog (analog, dt);
        updateBandTargets (pw, pDepth->load());

        // --- Audio
        for (int i = start; i < start + n; ++i)
        {
            const float gIn  = inGainSm.getNextValue();
            const float dryL = left[i]  * gIn;
            const float dryR = right[i] * gIn;
            inPeak = juce::jmax (inPeak, std::abs (dryL), std::abs (dryR));

            float wetL, wetR;
            filterBank.processSample (dryL, dryR, wetL, wetR);

            const float m    = mixSm.getNextValue();
            const float gOut = outGainSm.getNextValue();
            left[i]  = (dryL + (wetL - dryL) * m) * gOut;
            right[i] = (dryR + (wetR - dryR) * m) * gOut;
            outPeak = juce::jmax (outPeak, std::abs (left[i]), std::abs (right[i]));
        }
    }

    inputLevel.store (juce::jmax (inPeak, inputLevel.load() * 0.8f));
    outputLevel.store (juce::jmax (outPeak, outputLevel.load() * 0.8f));
}

//==============================================================================
juce::AudioProcessorEditor* FlowFilterAudioProcessor::createEditor()
{
    return new FlowFilterAudioProcessorEditor (*this);
}

void FlowFilterAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void FlowFilterAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new FlowFilterAudioProcessor();
}
