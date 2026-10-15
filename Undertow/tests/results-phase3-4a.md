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
| OK | Motif transmis au thread audio | sans verrou (triple tampon) |

### Performance : 512 ech. a 48 kHz, HQ 4x, effets actifs

| Statut | Test | Mesure |
|---|---|---|
| OK | HQ 4x | moyenne 349.5 us/bloc (3.28 % d'un coeur), 99e centile 615.4 us, pire 3541.2 us |
| OK | Eco 2x | moyenne 232.6 us/bloc (2.18 % d'un coeur), 99e centile 400.9 us, pire 1499.9 us |

### Presets d'usine : chargement, volume (LUFS), pic, rendus WAV de 4 mesures

| Statut | Test | Mesure |
|---|---|---|
| OK | HARDGROOVE / Breakwater (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -4.6 dBFS, 7 pas actifs |
| OK | HARDGROOVE / Rolling Surf (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -11.1 dBFS, 12 pas actifs |
| OK | HARDGROOVE / Steel Tide (135 BPM) | -18.0 LUFS (cible -18 +-2), pic -6.3 dBFS, 8 pas actifs |
| OK | HYPNOTIC / Abyssal Loop (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -6.4 dBFS, 10 pas actifs |
| OK | HYPNOTIC / Slow Tide (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -7.2 dBFS, 6 pas actifs |
| OK | HYPNOTIC / Deep Pressure (130 BPM) | -18.0 LUFS (cible -18 +-2), pic -5.1 dBFS, 3 pas actifs |
| OK | MENTAL / Maelstrom (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -10.2 dBFS, 16 pas actifs |
| OK | MENTAL / Vortex (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -5.4 dBFS, 10 pas actifs |
| OK | MENTAL / Crosscurrent (142 BPM) | -18.0 LUFS (cible -18 +-2), pic -9.1 dBFS, 13 pas actifs |
| OK | Ecart de volume entre presets | 0.1 dB (de -18.0 a -18.0 LUFS) |
