# Résultats du banc de tests d'Ebb — phase 1 (Linux, 13 octobre 2026)

### Paramètres figés (IDs, plages, choix)

| Statut | Test | Mesure |
|---|---|---|
| OK | 6 paramètres, IDs / plages / défauts / nombre de choix | amount, curve, rate, trigger, nudge, threshold |

### Courbes : tables contre référence calculée à part

| Statut | Test | Mesure |
|---|---|---|
| OK | Courbe Splash | écart max 0.000605 |
| OK | Courbe Snap | écart max 0.000308 |
| OK | Courbe Ripple | écart max 0.000055 |
| OK | Courbe Current | écart max 0.000042 |
| OK | Courbe Ledge | écart max 0.000001 |
| OK | Courbe Twin | écart max 0.000105 |
| OK | Courbe Gate | écart max 0.000004 |
| OK | Courbe Swell | écart max 0.000000 |
| OK | Courbe Drift | écart max 0.000000 |
| OK | Courbe Breath | écart max 0.000000 |
| OK | Courbe Surge | écart max 0.000000 |
| OK | Courbe Backwash | écart max 0.000000 |
| OK | Toutes les valeurs entre 0 et 1 | 0 hors plage |

### Forme mesurée en sortie = référence

| Statut | Test | Mesure |
|---|---|---|
| OK | Rendu Splash | écart max 0.00060 |
| OK | Rendu Snap | écart max 0.00031 |
| OK | Rendu Ripple | écart max 0.00005 |
| OK | Rendu Current | écart max 0.00004 |
| OK | Rendu Ledge | écart max 0.00000 |
| OK | Rendu Twin | écart max 0.00011 |
| OK | Rendu Gate | écart max 0.00000 |
| OK | Rendu Swell | écart max 0.00000 |
| OK | Rendu Drift | écart max 0.00000 |
| OK | Rendu Breath | écart max 0.00000 |
| OK | Rendu Surge | écart max 0.00000 |
| OK | Rendu Backwash | écart max 0.00000 |

### Audio : 44,1 / 48 / 96 kHz, blocs 1 / 64 / 512 / 2048, mono et stéréo

| Statut | Test | Mesure |
|---|---|---|
| OK | Aucun NaN, inf ni dénormal | 6771600 échantillons, 24 configurations |
| OK | La sortie ne dépasse jamais l'entrée | 0 échantillon(s) plus fort(s) |

### Amount 0 % = signal intact

| Statut | Test | Mesure |
|---|---|---|
| OK | Identique bit à bit (SYNC, MIDI, AUDIO ; 12 configurations) | 2257200 échantillons, 0 différent(s) |

### Anti-clic : variation du gain d'un échantillon à l'autre

| Statut | Test | Mesure |
|---|---|---|
| OK | 44.1 kHz : saut max du gain | 0.02268 par echantillon (limite 0.02268 = 1 ms pour 0 -> 100 %), 231 changements : courbe, Rate, Amount, mode, Play/Stop, saut de position, Nudge, relance MIDI |
| OK | 48.0 kHz : saut max du gain | 0.02083 par echantillon (limite 0.02083 = 1 ms pour 0 -> 100 %), 236 changements : courbe, Rate, Amount, mode, Play/Stop, saut de position, Nudge, relance MIDI |
| OK | 96.0 kHz : saut max du gain | 0.01042 par echantillon (limite 0.01042 = 1 ms pour 0 -> 100 %), 230 changements : courbe, Rate, Amount, mode, Play/Stop, saut de position, Nudge, relance MIDI |

### SYNC : début de chaque courbe à l'échantillon attendu (64 mesures)

| Statut | Test | Mesure |
|---|---|---|
| OK | 120 BPM | 8277 débuts de courbe (7 durées x 3 fréquences), écart max 0 échantillon, 0 manquant(s), 0 en trop |
| OK | 128 BPM | 8277 débuts de courbe (7 durées x 3 fréquences), écart max 0 échantillon, 0 manquant(s), 0 en trop |
| OK | 140 BPM | 8277 débuts de courbe (7 durées x 3 fréquences), écart max 0 échantillon, 0 manquant(s), 0 en trop |
| OK | 150 BPM | 8277 débuts de courbe (7 durées x 3 fréquences), écart max 0 échantillon, 0 manquant(s), 0 en trop |
| OK | Nudge +10.0 ms | 64 temps, écart max 0 échantillon |
| OK | Nudge -7.5 ms | 64 temps, écart max 0 échantillon |

### Transport : Play / Stop, boucle, tempo, signature

| Statut | Test | Mesure |
|---|---|---|
| OK | Play au milieu d'un temps | premier départ à l'échantillon 12000 (attendu 12000), puis 36000 |
| OK | Stop : continue au dernier tempo | 9 départs, écart max 0 échantillon |
| OK | Boucle de 2 mesures (ppq 4 → 12) | 30 départs comparés, 0 décalé(s) |
| OK | Tempo 120 → 140 BPM | 64 temps après le changement, écart max 0 |
| OK | 3/4 : une courbe par mesure de 3 temps | 8 départs, 0 décalé(s) |
| OK | Sans tête de lecture : roue libre à 120 BPM | 8 départs, 0 décalé(s) |

### MIDI : départ à l'échantillon exact de la note

| Statut | Test | Mesure |
|---|---|---|
| OK | Départ de courbe = note | 116 notes (12 configurations), écart max 0 échantillon, 0 manquante(s) |
| OK | Le volume baisse dès l'échantillon de la note | 0 note(s) sans baisse immédiate |

### AUDIO : déclenchement par le kick du sidechain

| Statut | Test | Mesure |
|---|---|---|
| OK | Kick 128 BPM, -6 dB, 48 kHz | 64 / 64 déclenchements, délai 1.21 - 1.25 ms (objectif < 3 ms) |
| OK | Kick long (909) 140 BPM, 44,1 kHz | 64 / 64 déclenchements, délai 1.09 - 1.70 ms (objectif < 3 ms) |
| OK | Kick + charleys 150 BPM, 96 kHz, mono | 64 / 64 déclenchements, délai 1.22 - 1.23 ms (objectif < 3 ms) |
| OK | Kick faible (-18 dB), seuil -30 dB, 48 kHz | 64 / 64 déclenchements, délai 1.31 - 1.33 ms (objectif < 3 ms) |
| OK | Kick avec longue queue (encore -6 dB au kick suivant), 128 BPM | 64 / 64 déclenchements, délai 1.00 - 2.50 ms (objectif < 3 ms) |
| OK | Kick 174 BPM (drum & bass), 48 kHz | 64 / 64 déclenchements, délai 1.21 - 1.23 ms (objectif < 3 ms) |
| OK | Basse tenue et liée (55-82 Hz) seule, 8 s, seuil -40 dB | 0 déclenchement(s) pendant la note ; 1 au tout début (le son démarre après un silence : c'est une attaque) |
| OK | Sub-basse tenue (31-33 Hz) seule, 8 s, seuil -40 dB | 0 déclenchement(s) pendant la note ; 1 au tout début (le son démarre après un silence : c'est une attaque) |
| OK | Charleys seuls (8 s, -6 dB, seuil -40 dB) | 0 déclenchement(s) |

### Formats de bus : sidechain absent / mono / stéréo

| Statut | Test | Mesure |
|---|---|---|
| OK | 6 formats acceptés, déclenchements seulement avec sidechain | mono/sans=0, mono/mono=5, mono/stereo=5, stereo/sans=0, stereo/mono=5, stereo/stereo=5 |
| OK | Formats refusés (entrée stéréo / sortie mono, 5.1) | OK |

### Sauvegarde / rechargement

| Statut | Test | Mesure |
|---|---|---|
| OK | 50 états au hasard, 6 paramètres | 0 différence(s) |

### Performance (bloc de 512 a 48 kHz, stereo, mode AUDIO avec kicks)

| Statut | Test | Mesure |
|---|---|---|
| OK | CPU moyen | 10.59 µs par bloc = 0.099 % d'un cœur (objectif < 0,3 %) |
| OK | 99e centile / pire bloc | 35.48 µs / 6219.1 µs (le pire inclut les interruptions de la machine de test) |

### Allocations dans processBlock

| Statut | Test | Mesure |
|---|---|---|
| OK | 3 000 blocs (3 modes, MIDI, courbes, Amount, bypass, Play/Stop) | 0 allocation(s) |

### Interface provisoire : captures, dessin, repos

| Statut | Test | Mesure |
|---|---|---|
| OK | Captures | /home/claude/Ebb/build-linux/captures/ebb_provisoire@1x.png et @2x |
| OK | Image pleine à 1x (fond recalculé) | moyenne 4.65 ms, 95e centile 5.75 ms (objectif < 16 ms) |
| OK | Image courante à 1x (vague, eau, goutte, logo) | moyenne 1.28 ms, 95e centile 1.64 ms (objectif < 16 ms) |
| OK | Image pleine à 2x (fond recalculé) | moyenne 17.70 ms, 95e centile 23.70 ms (information : rendu logiciel Linux, 8 images pleines par seconde au plus) |
| OK | Image courante à 2x (vague, eau, goutte, logo) | moyenne 6.05 ms, 95e centile 16.19 ms (information : rendu logiciel Linux, 8 images pleines par seconde au plus) |
| OK | Aucune allocation dans paint() pendant l'animation (notre code) | 0 allocation(s) sur 80 images |
| OK | Au repos (AUDIO sans kick) | 0 image(s) active(s) sur 120 (fond rafraîchi 4 fois/s seulement) |

## pluginval (Linux, strict niveau 10, VST3)

```
Starting tests in: pluginval / Scan for plugins located in: /home/claude/Ebb/build-linux/Ebb_artefacts/Release/VST3/Ebb.vst3...
Starting tests in: pluginval / Open plugin (cold)...
Starting tests in: pluginval / Open plugin (warm)...
Starting tests in: pluginval / Plugin info...
Starting tests in: pluginval / Plugin programs...
Starting tests in: pluginval / Editor...
Starting tests in: pluginval / Open editor whilst processing...
Starting tests in: pluginval / Audio processing...
Starting tests in: pluginval / Non-releasing audio processing...
Starting tests in: pluginval / Plugin state...
Starting tests in: pluginval / Plugin state restoration...
Starting tests in: pluginval / Automation...
Starting tests in: pluginval / Editor Automation...
Starting tests in: pluginval / Automatable Parameters...
Starting tests in: pluginval / Parameters...
Starting tests in: pluginval / Background thread state...
Starting tests in: pluginval / Parameter thread safety...
Starting tests in: pluginval / auval...
Starting tests in: pluginval / vst3 validator...
Starting tests in: pluginval / Basic bus...
Starting tests in: pluginval / Listing available buses...
Starting tests in: pluginval / Enabling all buses...
Starting tests in: pluginval / Disabling non-main busses...
Starting tests in: pluginval / Restoring default layout...
Starting tests in: pluginval / Fuzz parameters...
SUCCESS
```
