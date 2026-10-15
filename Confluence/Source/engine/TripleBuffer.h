// SPDX-License-Identifier: AGPL-3.0-or-later
// Copyright (C) 2026 GRVTY Records
// Ce fichier fait partie de Confluence, logiciel libre : vous pouvez le redistribuer et le modifier selon les termes
// de la GNU Affero General Public License v3 ou ultérieure. Fourni SANS AUCUNE GARANTIE. Voir les fichiers LICENSE et NOTICE.

#pragma once

#include <array>
#include <atomic>
#include <cstddef>

//==============================================================================
/**
    Échange sans verrou entre un seul écrivain et un seul lecteur (ex. interface -> audio) :
    l'écrivain publie une copie complète, le lecteur récupère toujours la dernière version publiée,
    jamais une version à moitié écrite. Aucune allocation.
*/
template <typename T>
class TripleBuffer
{
public:
    TripleBuffer() = default;

    /** Initialise les trois copies (avant que les deux threads ne tournent). */
    void reset (const T& value) noexcept
    {
        buffers[0] = value;
        buffers[1] = value;
        buffers[2] = value;
        state.store (1, std::memory_order_release);
        writeIndex = 0;
        readIndex = 2;
    }

    /** Écrivain : publie une nouvelle valeur. */
    void write (const T& value) noexcept
    {
        buffers[(size_t) writeIndex] = value;
        const int previous = state.exchange (writeIndex | dirtyBit, std::memory_order_acq_rel);
        writeIndex = previous & indexMask;
    }

    /** Lecteur : la dernière valeur publiée. */
    const T& read() noexcept
    {
        if (state.load (std::memory_order_acquire) & dirtyBit)
        {
            const int previous = state.exchange (readIndex, std::memory_order_acq_rel);
            readIndex = previous & indexMask;
        }
        return buffers[(size_t) readIndex];
    }

private:
    static constexpr int dirtyBit = 4, indexMask = 3;
    std::array<T, 3> buffers {};
    std::atomic<int> state { 1 };
    int writeIndex = 0, readIndex = 2;
};
