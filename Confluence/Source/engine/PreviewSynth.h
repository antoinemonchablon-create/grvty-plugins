// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <JuceHeader.h>
#include <array>

//==============================================================================
/**
    Voix Preview : un petit son doux et simple (sinus + harmoniques légères, enveloppe de piano électrique)
    pour entendre les accords sans router Confluence vers un synthé. Désactivée par défaut.
    Elle joue exactement le MIDI qui sort du plugin, à l'échantillon près.
*/
class PreviewSynth
{
public:
    static constexpr int numVoices = 16;

    void prepare (double sampleRate) noexcept
    {
        sr = (float) juce::jmax (8000.0, sampleRate);
        attackInc = 1.0f / (0.004f * sr);
        decayCoef = std::exp (-1.0f / (0.45f * sr));
        releaseCoef = std::exp (-1.0f / (0.16f * sr));
        lpCoef = 1.0f - std::exp (-juce::MathConstants<float>::twoPi * 3200.0f / sr);
        gain.reset (sampleRate, 0.03);
        allOff (true);
    }

    void allOff (bool hard) noexcept
    {
        for (auto& v : voices)
        {
            if (hard) v = Voice {};
            else if (v.stage != idle) v.stage = release;
        }
        if (hard) lpL = lpR = 0.0f;
    }

    bool isSilent() const noexcept
    {
        for (const auto& v : voices) if (v.stage != idle) return false;
        return std::abs (lpL) < 1.0e-6f && std::abs (lpR) < 1.0e-6f;
    }

    /** Joue les notes de 'events' dans le tampon audio (ajouté au contenu). */
    void process (juce::AudioBuffer<float>& audio, const juce::MidiBuffer& events, float levelDb) noexcept
    {
        gain.setTargetValue (juce::Decibels::decibelsToGain (levelDb));
        const int n = audio.getNumSamples();
        int pos = 0;
        for (const auto m : events)
        {
            const int at = juce::jlimit (0, n, m.samplePosition);
            render (audio, pos, at - pos);
            pos = at;
            if (m.numBytes < 3) continue;
            const auto status = m.data[0] & 0xF0;
            if (status == 0x90 && m.data[2] > 0) noteOn (m.data[1], m.data[2]);
            else if (status == 0x80 || status == 0x90) noteOff (m.data[1]);
            else if (status == 0xB0 && (m.data[1] == 123 || m.data[1] == 120)) allOff (false);
        }
        render (audio, pos, n - pos);
    }

private:
    enum Stage { idle = 0, attack, decay, release };

    struct Voice
    {
        int note = -1;
        int stage = idle;
        float phase = 0.0f, inc = 0.0f, env = 0.0f, vel = 0.0f, panL = 0.7f, panR = 0.7f;
        uint32_t age = 0;
    };

    void noteOn (int note, int velocity) noexcept
    {
        Voice* target = nullptr;
        for (auto& v : voices) if (v.note == note && v.stage != idle) target = &v;
        if (target == nullptr)
            for (auto& v : voices) if (v.stage == idle) { target = &v; break; }
        if (target == nullptr)
        {
            target = &voices[0];
            for (auto& v : voices) if (v.age < target->age) target = &v;   // la plus ancienne
        }
        auto& v = *target;
        v.note = note;
        v.stage = attack;
        v.inc = (float) juce::MidiMessage::getMidiNoteInHertz (note) / sr;
        v.vel = 0.25f + 0.75f * (float) velocity / 127.0f;
        const float pan = juce::jlimit (0.0f, 1.0f, 0.5f + (float) (note - 60) / 72.0f);
        v.panL = std::cos (pan * juce::MathConstants<float>::halfPi);
        v.panR = std::sin (pan * juce::MathConstants<float>::halfPi);
        v.age = ++counter;
    }

    void noteOff (int note) noexcept
    {
        for (auto& v : voices)
            if (v.note == note && v.stage != idle && v.stage != release)
                v.stage = release;
    }

    void render (juce::AudioBuffer<float>& audio, int start, int len) noexcept
    {
        if (len <= 0) return;
        auto* L = audio.getWritePointer (0);
        auto* R = audio.getNumChannels() > 1 ? audio.getWritePointer (1) : nullptr;
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        for (int i = start; i < start + len; ++i)
        {
            float sl = 0.0f, sr2 = 0.0f;
            for (auto& v : voices)
            {
                if (v.stage == idle) continue;
                switch (v.stage)
                {
                    case attack:  v.env += attackInc; if (v.env >= 1.0f) { v.env = 1.0f; v.stage = decay; } break;
                    case decay:   v.env = 0.55f + (v.env - 0.55f) * decayCoef; break;
                    case release: v.env *= releaseCoef; if (v.env < 1.0e-4f) { v.stage = idle; v.note = -1; v.env = 0.0f; } break;
                    default: break;
                }
                const float p = v.phase * twoPi;
                const float s = (std::sin (p) + 0.22f * std::sin (2.0f * p) + 0.07f * std::sin (3.0f * p)) * v.env * v.vel;
                v.phase += v.inc;
                if (v.phase >= 1.0f) v.phase -= 1.0f;
                sl += s * v.panL;
                sr2 += s * v.panR;
            }
            const float g = gain.getNextValue() * 0.22f;
            lpL += lpCoef * (sl - lpL);
            lpR += lpCoef * (sr2 - lpR);
            const float outL = std::tanh (lpL * g * 1.2f) / 1.2f, outR = std::tanh (lpR * g * 1.2f) / 1.2f;
            L[i] += outL;
            if (R != nullptr) R[i] += outR;
            else L[i] += outR;
        }
    }

    std::array<Voice, numVoices> voices {};
    float sr = 48000.0f, attackInc = 0.0f, decayCoef = 0.0f, releaseCoef = 0.0f, lpCoef = 0.5f, lpL = 0.0f, lpR = 0.0f;
    uint32_t counter = 0;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> gain { 0.25f };
};
