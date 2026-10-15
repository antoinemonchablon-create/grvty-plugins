# Resultats du banc de tests

### Securite audio : NaN, inf, denormaux, pics

| Statut | Test | Mesure |
|---|---|---|
| OK | Defaut 44.1 kHz / bloc 1 | pic -5.71 dBFS, NaN/inf 0, denormaux 0 |
| OK | Defaut 44.1 kHz / bloc 64 | pic -5.71 dBFS, NaN/inf 0, denormaux 0 |
| OK | Defaut 44.1 kHz / bloc 512 | pic -5.71 dBFS, NaN/inf 0, denormaux 0 |
| OK | Defaut 44.1 kHz / bloc 2048 | pic -5.71 dBFS, NaN/inf 0, denormaux 0 |
| OK | Defaut 48.0 kHz / bloc 1 | pic -5.71 dBFS, NaN/inf 0, denormaux 0 |
| OK | Defaut 48.0 kHz / bloc 64 | pic -5.71 dBFS, NaN/inf 0, denormaux 0 |
| OK | Defaut 48.0 kHz / bloc 512 | pic -5.71 dBFS, NaN/inf 0, denormaux 0 |
| OK | Defaut 48.0 kHz / bloc 2048 | pic -5.71 dBFS, NaN/inf 0, denormaux 0 |
| OK | Defaut 96.0 kHz / bloc 1 | pic -5.71 dBFS, NaN/inf 0, denormaux 0 |
| OK | Defaut 96.0 kHz / bloc 64 | pic -5.71 dBFS, NaN/inf 0, denormaux 0 |
| OK | Defaut 96.0 kHz / bloc 512 | pic -5.71 dBFS, NaN/inf 0, denormaux 0 |
| OK | Defaut 96.0 kHz / bloc 2048 | pic -5.71 dBFS, NaN/inf 0, denormaux 0 |
| OK | Extreme 44.1 kHz / bloc 1 | pic -0.18 dBFS, NaN/inf 0, denormaux 0 |
| OK | Extreme 44.1 kHz / bloc 64 | pic -0.18 dBFS, NaN/inf 0, denormaux 0 |
| OK | Extreme 44.1 kHz / bloc 512 | pic -0.18 dBFS, NaN/inf 0, denormaux 0 |
| OK | Extreme 44.1 kHz / bloc 2048 | pic -0.18 dBFS, NaN/inf 0, denormaux 0 |
| OK | Extreme 48.0 kHz / bloc 1 | pic -0.18 dBFS, NaN/inf 0, denormaux 0 |
| OK | Extreme 48.0 kHz / bloc 64 | pic -0.18 dBFS, NaN/inf 0, denormaux 0 |
| OK | Extreme 48.0 kHz / bloc 512 | pic -0.18 dBFS, NaN/inf 0, denormaux 0 |
| OK | Extreme 48.0 kHz / bloc 2048 | pic -0.18 dBFS, NaN/inf 0, denormaux 0 |
| OK | Extreme 96.0 kHz / bloc 1 | pic -0.18 dBFS, NaN/inf 0, denormaux 0 |
| OK | Extreme 96.0 kHz / bloc 64 | pic -0.18 dBFS, NaN/inf 0, denormaux 0 |
| OK | Extreme 96.0 kHz / bloc 512 | pic -0.18 dBFS, NaN/inf 0, denormaux 0 |
| OK | Extreme 96.0 kHz / bloc 2048 | pic -0.18 dBFS, NaN/inf 0, denormaux 0 |

### Clics : automation rapide de chaque parametre (saut min -> max)

| Statut | Test | Mesure |
|---|---|---|
| OK | 51 parametres automatises | pire cas filterType : 2.0 dB (seuil +6 dB) |
| OK | Notes on / off / retrigger / slide | pente max 0.24 dB par rapport a un sinus propre (seuil +3 dB) |
| OK | Changement de reglages en lecture | discontinuite -62.8 dB (seuil +6 dB) |

### Aliasing : sinus FM a ~5 kHz, Index maximal

| Statut | Test | Mesure |
|---|---|---|
| OK | Eco 2x | repliements -44.8 dB de l'energie totale, pire raie -43.6 dBc |
| OK | HQ 4x | repliements -89.9 dB de l'energie totale, pire raie -93.7 dBc |

### Sequenceur : chaque pas a l'echantillon pres, aucune derive

| Statut | Test | Mesure |
|---|---|---|
| OK | 120 BPM, 48.0 kHz, bloc 512, swing 50%, 1/16 | 1024/1024 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 128 BPM, 48.0 kHz, bloc 512, swing 50%, 1/16 | 1024/1024 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 140 BPM, 48.0 kHz, bloc 512, swing 50%, 1/16 | 1024/1024 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 150 BPM, 48.0 kHz, bloc 512, swing 50%, 1/16 | 1024/1024 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 120 BPM, 44.1 kHz, bloc 333, swing 58%, 1/16 | 1024/1024 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 128 BPM, 44.1 kHz, bloc 333, swing 58%, 1/16 | 1024/1024 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 140 BPM, 44.1 kHz, bloc 333, swing 58%, 1/16 | 1024/1024 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 150 BPM, 44.1 kHz, bloc 333, swing 58%, 1/16 | 1024/1024 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 120 BPM, 96.0 kHz, bloc 64, swing 50%, 1/16T | 1536/1536 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 128 BPM, 96.0 kHz, bloc 64, swing 50%, 1/16T | 1536/1536 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 140 BPM, 96.0 kHz, bloc 64, swing 50%, 1/16T | 1536/1536 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 150 BPM, 96.0 kHz, bloc 64, swing 50%, 1/16T | 1536/1536 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 120 BPM, 48.0 kHz, bloc 1, swing 66%, 1/32 | 2048/2048 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 128 BPM, 48.0 kHz, bloc 1, swing 66%, 1/32 | 2048/2048 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 140 BPM, 48.0 kHz, bloc 1, swing 66%, 1/32 | 2048/2048 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |
| OK | 150 BPM, 48.0 kHz, bloc 1, swing 66%, 1/32 | 2048/2048 pas sur 64 mesures, ecart max 0 ech., derive finale 0 ech. |

### Sequenceur : Play / Stop, boucle, changement de tempo

| Statut | Test | Mesure |
|---|---|---|
| OK | Stop | silence -240.0 dBFS une demi-seconde apres le Stop (la note ne reste pas bloquee) |
| OK | Relance a la mesure 5 | premier pas (n. 65) a l'echantillon exact du Play |
| OK | Boucle de 2 mesures x6 | chaque tour repart au pas 1, a l'echantillon exact, 32 pas par tour |
| OK | Tempo 120 -> 140 BPM | 256 pas, 0 mal places |
| OK | Polymetrie 16 contre 13 | 183 pas de la piste Mod, 0 erreurs |
| OK | Notes forcees dans la gamme | 366 notes testees sur 6 gammes, 0 hors gamme |
| OK | Mutate | varie, identique d'un rendu a l'autre, l'original n'est jamais modifie |

### Etat : sauvegarde puis rechargement

| Statut | Test | Mesure |
|---|---|---|
| OK | Sauvegarde / rechargement | 69 parametres (0 differences), motif identique (32 pas + piste Mod + p-locks) |
| OK | Preset utilisateur (fichier) | enregistre, dans la liste, recharge : 0 difference, motif identique, nom ok |
| OK | Motif transmis au thread audio | sans verrou (triple tampon) |

### Performance : 512 ech. a 48 kHz, HQ 4x, effets actifs

| Statut | Test | Mesure |
|---|---|---|
| OK | HQ 4x | moyenne 367.1 us/bloc (3.44 % d'un coeur), 99e centile 552.9 us, pire 4934.4 us |
| OK | Eco 2x | moyenne 258.0 us/bloc (2.42 % d'un coeur), 99e centile 464.6 us, pire 2847.8 us |

### Changement de preset en lecture (comme dans Ableton)

| Statut | Test | Mesure |
|---|---|---|
| OK | 44.1 kHz, bloc 128, depuis la mesure 1 : 49 presets enchaines | tous sonnent |
| OK | 48.0 kHz, bloc 128, depuis la mesure 1 : 49 presets enchaines | tous sonnent |
| OK | 44.1 kHz, bloc 128, depuis la mesure 121 : 49 presets enchaines | tous sonnent |

### Changement de cadence en lecture

| Statut | Test | Mesure |
|---|---|---|
| OK | 1/32 > 1/16 > 1/8T > 1/8 > 1/32 > 1/32T > 1/16 (mesure 200) | premiere note au plus tard a 67 % d'un pas apres le changement (seuil 110 %) |

### Presets d'usine : chargement, volume (LUFS), pic, rendus WAV de 4 mesures

| Statut | Test | Mesure |
|---|---|---|
| OK | HARDGROOVE / Breakwater (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -4.6 dBFS, 7 pas actifs |
| OK | HARDGROOVE / Rolling Surf (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -11.1 dBFS, 12 pas actifs |
| OK | HARDGROOVE / Steel Tide (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -6.3 dBFS, 8 pas actifs |
| OK | HARDGROOVE / Tribal Current (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -9.3 dBFS, 10 pas actifs |
| OK | HARDGROOVE / Steel Conga (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -6.0 dBFS, 10 pas actifs |
| OK | HARDGROOVE / Iron Bell (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -6.3 dBFS, 5 pas actifs |
| OK | HARDGROOVE / Ride Spray (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -2.2 dBFS, 12 pas actifs |
| OK | HARDGROOVE / Rimshot Rattle (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -5.3 dBFS, 10 pas actifs |
| OK | HARDGROOVE / Rave Hoover (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -6.3 dBFS, 5 pas actifs |
| OK | HARDGROOVE / Groundswell (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -7.6 dBFS, 10 pas actifs |
| OK | HARDGROOVE / Shoreline Stab (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -4.0 dBFS, 5 pas actifs |
| OK | HARDGROOVE / Wave Hammer (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -13.5 dBFS, 16 pas actifs |
| OK | HARDGROOVE / Flare Zap (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -5.3 dBFS, 3 pas actifs |
| OK | HYPNOTIC / Abyssal Loop (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -6.4 dBFS, 10 pas actifs |
| OK | HYPNOTIC / Slow Tide (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -7.2 dBFS, 6 pas actifs |
| OK | HYPNOTIC / Deep Pressure (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -5.1 dBFS, 3 pas actifs |
| OK | HYPNOTIC / Kelp Forest (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -7.2 dBFS, 9 pas actifs |
| OK | HYPNOTIC / Night Swim (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -7.3 dBFS, 5 pas actifs |
| OK | HYPNOTIC / Dark Lagoon (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -7.3 dBFS, 16 pas actifs |
| OK | HYPNOTIC / Pendulum (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -5.3 dBFS, 9 pas actifs |
| OK | HYPNOTIC / Bioluminescence (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -7.0 dBFS, 6 pas actifs |
| OK | HYPNOTIC / Low Tide Drone (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -8.9 dBFS, 16 pas actifs |
| OK | HYPNOTIC / Sonar (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -4.8 dBFS, 2 pas actifs |
| OK | HYPNOTIC / Dub Basin (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -5.2 dBFS, 4 pas actifs |
| OK | HYPNOTIC / Deep Rotation (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -6.6 dBFS, 12 pas actifs |
| OK | HYPNOTIC / Sea Foam (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -2.8 dBFS, 16 pas actifs |
| OK | MENTAL / Maelstrom (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -10.2 dBFS, 16 pas actifs |
| OK | MENTAL / Vortex (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -5.4 dBFS, 10 pas actifs |
| OK | MENTAL / Crosscurrent (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -9.1 dBFS, 13 pas actifs |
| OK | MENTAL / Pressure Drop (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -12.2 dBFS, 8 pas actifs |
| OK | MENTAL / Siren Surge (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -12.8 dBFS, 16 pas actifs |
| OK | MENTAL / Cavitation (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -8.4 dBFS, 11 pas actifs |
| OK | MENTAL / Electric Eel (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -9.8 dBFS, 16 pas actifs |
| OK | MENTAL / Storm Surge (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -8.8 dBFS, 25 pas actifs |
| OK | MENTAL / Whirl (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -8.0 dBFS, 16 pas actifs |
| OK | MENTAL / Rapture of the Deep (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -6.9 dBFS, 16 pas actifs |
| OK | MENTAL / Acid Vein (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -9.5 dBFS, 14 pas actifs |
| OK | MENTAL / Bleep Signal (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -6.4 dBFS, 6 pas actifs |
| OK | MENTAL / Pulse Code (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -4.1 dBFS, 10 pas actifs |
| OK | MENTAL / Echo Probe (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -5.2 dBFS, 3 pas actifs |
| OK | MENTAL / Telemetry (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -6.7 dBFS, 23 pas actifs |
| OK | MENTAL / Snare Roll Tide (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -4.2 dBFS, 13 pas actifs |
| OK | MENTAL / Tom Cascade (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -9.1 dBFS, 9 pas actifs |
| OK | MENTAL / Drum Avalanche (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -4.5 dBFS, 22 pas actifs |
| OK | MENTAL / Gallop Roll (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -9.8 dBFS, 20 pas actifs |
| OK | ABYSS / Hadal Zone (124 BPM) | -18.0 LUFS (cible -18 +-2), pic -4.3 dBFS, 16 pas actifs |
| OK | ABYSS / Abyss Cathedral (124 BPM) | -18.0 LUFS (cible -18 +-2), pic -7.1 dBFS, 4 pas actifs |
| OK | ABYSS / Trench Breath (124 BPM) | -18.0 LUFS (cible -18 +-2), pic -6.3 dBFS, 16 pas actifs |
| OK | ABYSS / Whale Fall (124 BPM) | -18.0 LUFS (cible -18 +-2), pic -4.6 dBFS, 4 pas actifs |
| OK | Ecart de volume entre presets | 0.1 dB (de -18.0 a -18.0 LUFS) |
