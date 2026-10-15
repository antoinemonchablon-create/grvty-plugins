<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Resultats du banc de tests de Confluence

### Théorie : accords contre une table de référence calculée indépendamment

| Statut | Test | Mesure |
|---|---|---|
| OK | Formes d'accords (17 gammes x degrés x 12 types) | 2628 formes, 0 differences |
| OK | Note jouée -> accord (mode Gamme, voicing neutre) | 106704 accords, 0 differences |

### Théorie : accords connus (notes et noms écrits à la main)

| Statut | Test | Mesure |
|---|---|---|
| OK | Accords connus | 38 / 38 corrects (notes, nom, degré) |
| OK | Formes fixes (mode Accord fixe) | 21 / 21 noms corrects  |

### Aucune note hors de la gamme (modes Gamme, Touches blanches, Pads)

| Statut | Test | Mesure |
|---|---|---|
| OK | Notes dans la gamme | 646272 accords (17 gammes x 12 toniques x 3 modes x 12 types x 128 notes, voicings au hasard), 0 hors gamme |
| OK | Notes sans accord | 0 (plage trop étroite pour la classe de hauteur, hors des notes MIDI) |

### Voicing : renversements, ecartements, voix, plage, basse

| Statut | Test | Mesure |
|---|---|---|
| OK | Serré, renversement 0 | C3 E3 G3 B3 |
| OK | Renversement 1 | E3 G3 B3 C4 |
| OK | Renversement 2 | G3 B3 C4 E4 |
| OK | Renversement 3 | B3 C4 E4 G4 |
| OK | Ouvert | C3 G3 E4 B4 |
| OK | Drop 2 | G2 C3 E3 B3 |
| OK | Drop 3 | E2 C3 G3 B3 |
| OK | Large (2 octaves) | C2 G3 E4 B4 |
| OK | Voix max 4 sur Cm9 (sans quinte) | C3 D#3 A#3 D4 |
| OK | Voix max 3 + basse (la basse joue la fondamentale) | D#3 A#3 D4 + basse C2 |
| OK | 200 000 voicings au hasard | 200000 accords : 0 hors plage, 0 doublons, 0 trop de voix, 0 basses incorrectes |
| OK | Voice leading Auto (demi-tons par voix et par changement, Auto contre renversement 0) | I vi IV V (majeur, 7e) : 0.75 contre 4.89 ; ii V I vi (majeur, 9e) : 1.23 contre 6.89 ; i VI III VII (mineur, triades) : 0.90 contre 7.29 ; i iv v i (dorien, 7e) : 0.86 contre 4.00 ; i bII i bVII (phrygien, triades) : 1.62 contre 5.05 ;  |

### Détection de la gamme (mélodies de test connues, la bonne gamme dans le top 3)

| Statut | Test | Mesure |
|---|---|---|
| OK | La mineur (naturel) | rang 1 (A Mineur naturel, A Phrygien, E Phrygien) |
| OK | Do majeur | rang 1 (C Majeur, C Lydien, C Mixolydien) |
| OK | Ré dorien | rang 1 (D Dorien, F Lydien, B Locrien) |
| OK | Mi phrygien | rang 1 (E Phrygien, E Locrien, F Lydien) |
| OK | Sol mixolydien | rang 1 (G Mixolydien, D Dorien, B Locrien) |
| OK | Fa# mineur | rang 1 (F# Mineur naturel, F# Phrygien, C# Phrygien) |
| OK | Do mineur harmonique | rang 1 (C Mineur harmonique, C Mineure hongroise, G Phrygien dominant) |
| OK | La pentatonique mineure | rang 1 (A Pentatonique mineure, A Blues, A Phrygien) |
| OK | Mi phrygien dominant | rang 1 (E Phrygien dominant, E Double harmonique, A Mineur harmonique) |
| OK | Fa lydien | rang 1 (F Lydien, A Mineur naturel, C Majeur) |
| OK | Analyse dans le plugin (ré dorien joué) | 1er : D Dorien, 16 notes |
| OK | Enchainements proposés | V->I oui, ii->V oui, i->VII/iv oui, phrygien i->bII oui |

### Modes de jeu : Gamme (hors gamme), Touches blanches, Accord fixe, Pads

| Statut | Test | Mesure |
|---|---|---|
| OK | Note hors gamme (C#3 en do majeur) | Plus proche -> Dm7 ; Au-dessus -> Dm7 ; En dessous -> Cmaj7 ; Ignorer -> rien ;  |
| OK | Touches blanches en F# mineur | C3=F#m7 D3=G#m7b5 E3=Amaj7 F3=Bm7 G3=C#m7 A3=Dmaj7 B3=E7  |
| OK | Touches noires (C#3 = variante du degré i) | Sus4=F#sus4 Sus2=F#sus2 9e=F#m9 Mode parallèle=F#maj7 Muettes=rien |
| OK | Accord fixe m9 sur A, A#, C#, F# | Am9 A#m9 C#m9 F#m9  |
| OK | Pads (C1 a G1) | 8 notes sur 20 jouent un pad ; pad 1 = Am7 |

### MIDI : latence nulle, strum, canal de sortie, bornes

| Statut | Test | Mesure |
|---|---|---|
| OK | Latence 0 (strum et humanize à 0) | 480 accords sur 3 fréquences x 4 tailles de bloc : 0 accords dont une note n'est pas au même échantillon |
| OK | Strum (37,3 ms et 1/32 à 128 BPM) | 1008 notes, écart maximal 0.50 échantillon, 0 hors tolerance |
| OK | Canal de sortie respecté (canal 7, entrée sur 1 et 3) | toutes les notes et CC sur le canal 7 |
| OK | Notes 0-127, horodatages dans le bloc et dans l'ordre | 19 evenements, 0 erreurs (notes jouees a C8+3 oct. et C-2) |
| OK | CC, pitch bend, sustain transmis | CC74 oui, pitch bend oui, sustain oui |
| OK | Pas de note restante | 0 note(s) restante(s) |
| OK | Durée fixe 1/16 (touche courte et touche longue) | 8 notes, durées 6000..6000 échantillons (attendu 6000) |

### Note-off : exactement les notes de son accord, Latch

| Statut | Test | Mesure |
|---|---|---|
| OK | Relâchement après changement de tous les réglages | coupe 4 notes (A C E G, canal 1) ; 4 notes du 2e accord restent jusqu'a son relachement |
| OK | Notes partagées entre deux accords | E et G restent tant que Em est tenu, relancées proprement |
| OK | Latch | 4 notes tenues apres relachement, remplacees a la note suivante, coupees quand le Latch s'eteint |

### All Notes Off : Stop, bypass, changement de preset, desactivation, Panic

| Statut | Test | Mesure |
|---|---|---|
| OK | Stop du transport | 4 notes sonnaient, 0 restent, 0 notes en attente parties ensuite |
| OK | Bypass | 4 notes sonnaient, 0 restent, 0 notes en attente parties ensuite |
| OK | Changement de preset | 4 notes sonnaient, 0 restent, 0 notes en attente parties ensuite |
| OK | Rechargement d'etat (projet) | 4 notes sonnaient, 0 restent, 0 notes en attente parties ensuite |
| OK | Desactivation du plugin | 4 notes sonnaient, 0 restent, 0 notes en attente parties ensuite |
| OK | Bouton Panic | 4 notes sonnaient, 0 restent, 0 notes en attente parties ensuite |
| OK | Panic (CC123 + sustain 0) et bypass (note telle quelle) | CC123 oui, sustain 0 oui, bypass : 1 note jouee telle quelle, coupee a la sortie du bypass |

### Notes bloquées : événements au hasard

| Statut | Test | Mesure |
|---|---|---|
| OK | 44.1 kHz, blocs de 1 | 100000 événements, 43931 note-on / 43931 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0 |
| OK | 44.1 kHz, blocs de 64 | 100000 événements, 48177 note-on / 48177 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0 |
| OK | 44.1 kHz, blocs de 512 | 100000 événements, 48244 note-on / 48244 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0 |
| OK | 44.1 kHz, blocs de 2048 | 100000 événements, 47555 note-on / 47555 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0 |
| OK | 48.0 kHz, blocs de 1 | 100000 événements, 44177 note-on / 44177 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0 |
| OK | 48.0 kHz, blocs de 64 | 100000 événements, 46665 note-on / 46665 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0 |
| OK | 48.0 kHz, blocs de 512 | 100000 événements, 47863 note-on / 47863 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0 |
| OK | 48.0 kHz, blocs de 2048 | 100000 événements, 46917 note-on / 46917 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0 |
| OK | 96.0 kHz, blocs de 1 | 100000 événements, 43274 note-on / 43274 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0 |
| OK | 96.0 kHz, blocs de 64 | 100000 événements, 45074 note-on / 45074 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0 |
| OK | 96.0 kHz, blocs de 512 | 100000 événements, 44713 note-on / 44713 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0 |
| OK | 96.0 kHz, blocs de 2048 | 100000 événements, 44305 note-on / 44305 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0 |
| OK | Total | 1200000 événements au hasard, 550895 notes jouées, aucune note bloquée |

### État : sauvegarde / rechargement, Learn, export MIDI

| Statut | Test | Mesure |
|---|---|---|
| OK | Sauvegarde puis rechargement | 35 paramètres (0 différences), 8 pads identiques, accord appris identique |
| OK | Learn | C E G Bb joués ensemble -> forme 0 4 7 10, mode Accord fixe / Appris ; D joué ensuite -> D3 F#3 A3 C4 |
| OK | Export MIDI (.mid relu) | 4 mesures : 8 accords identiques au plugin, fin a 4.00 mes. ; 8 mesures : 8 accords identiques au plugin, fin a 8.00 mes. ;  |

### Sécurité : presets et états corrompus ou piégés

| Statut | Test | Mesure |
|---|---|---|
| OK | Fichiers piégés : aucun plantage, aucune valeur invalide | 24 chargements (12 fichiers x preset / état du projet) : 0 paramètre(s) invalide(s), 0 message(s) MIDI invalide(s), 0 note(s) restée(s), 0 bloc(s) audio NaN |

### Performance et allocations (bloc de 512 a 48 kHz)

| Statut | Test | Mesure |
|---|---|---|
| OK | processBlock (MIDI seul : 8 voix + basse, strum, humanize, voice leading) | moyenne 3.88 µs (0.036 % d'un cœur), 99e centile 10.51 µs, pire 77.6 µs ; objectif < 0,5 % |
| OK | processBlock avec la voix Preview (information) | moyenne 269.37 µs (2.53 % d'un cœur), pire 2926.0 µs |
| OK | Aucune allocation dans processBlock | 0 allocation(s) sur 3000 blocs (accords, strum, humanize, Preview, Panic, bypass) |

### Presets d'usine : rendus de 4 mesures (.mid sortie MIDI + WAV voix Preview), pic < -1 dBFS

| Statut | Test | Mesure |
|---|---|---|
| OK | Warm Current | 122 BPM, 7 notes jouées -> 35 notes MIDI ; voix Preview : pic -14.9 dBFS, -25.8 LUFS ; WAV : pic -7.0 dBFS, -18.0 LUFS |
| OK | Lagoon Keys | 120 BPM, 4 notes jouées -> 20 notes MIDI ; voix Preview : pic -15.6 dBFS, -27.2 LUFS ; WAV : pic -6.4 dBFS, -18.0 LUFS |
| OK | Dub Chamber | 128 BPM, 16 notes jouées -> 64 notes MIDI ; voix Preview : pic -16.9 dBFS, -26.5 LUFS ; WAV : pic -8.4 dBFS, -18.0 LUFS |
| OK | Fog Bank | 130 BPM, 6 notes jouées -> 24 notes MIDI ; voix Preview : pic -16.8 dBFS, -27.5 LUFS ; WAV : pic -7.3 dBFS, -18.0 LUFS |
| OK | Splash Stab | 126 BPM, 18 notes jouées -> 72 notes MIDI ; voix Preview : pic -16.4 dBFS, -27.7 LUFS ; WAV : pic -6.6 dBFS, -18.0 LUFS |
| OK | Rave Tide | 128 BPM, 19 notes jouées -> 76 notes MIDI ; voix Preview : pic -16.4 dBFS, -26.0 LUFS ; WAV : pic -8.4 dBFS, -18.0 LUFS |

| Preset | Banque | BPM | Notes jouees | Notes MIDI sorties | Preview : pic / LUFS | WAV : pic / LUFS |
|---|---|---|---|---|---|---|
| Warm Current | DEEP & HOUSE | 122 | 7 | 35 | -14.9 / -25.8 | -7.0 / -18.0 |
| Lagoon Keys | DEEP & HOUSE | 120 | 4 | 20 | -15.6 / -27.2 | -6.4 / -18.0 |
| Dub Chamber | HYPNOTIC & DUB | 128 | 16 | 64 | -16.9 / -26.5 | -8.4 / -18.0 |
| Fog Bank | HYPNOTIC & DUB | 130 | 6 | 24 | -16.8 / -27.5 | -7.3 / -18.0 |
| Splash Stab | TECH HOUSE & STABS | 126 | 18 | 72 | -16.4 / -27.7 | -6.6 / -18.0 |
| Rave Tide | TECH HOUSE & STABS | 128 | 19 | 76 | -16.4 / -26.0 | -8.4 / -18.0 |

### Interface : captures, temps de dessin, allocations, repos

| Statut | Test | Mesure |
|---|---|---|
| OK | tab_chord_1x.png | 1000 x 470 |
| OK | tab_chord_2x.png | 2000 x 940 |
| OK | tab_voicing_1x.png | 1000 x 470 |
| OK | tab_voicing_2x.png | 2000 x 940 |
| OK | tab_play_1x.png | 1000 x 470 |
| OK | tab_play_2x.png | 2000 x 940 |
| OK | Pad 3 joue a la souris | l'accord du pad sonne pendant le clic, s'arrete au relachement |
| OK | Clavier de l'interface (A2 clique) | la touche joue l'accord par Confluence, puis le relache |
| OK | save_panel_2x.png | panneau Enregistrer affiche dans la fenetre |
| OK | Panneau « À propos » (about_2x.png) | copyright, licence AGPL v3, absence de garantie, droit de redistribuer, où lire la licence, code source, confidentialité, composants tiers, marques ; se referme |
| OK | Redimensionnement 150 % | 1500 x 705 |
| OK | Image pleine (fond recalcule, 1x, rendu logiciel) | moyenne 9.42 ms, 95e centile 13.41 ms (3194.22 allocations du moteur de rendu) |
| OK | Image partielle (mer, bulles, tourbillon) | moyenne 4.37 ms, 95e centile 6.84 ms (2997.9 allocations du moteur de rendu) |
| OK | Aucune allocation dans paint() pendant l'animation (notre code) | 0 allocation sur 73 images d'animation ; 1198 par image où un nouvel accord s'affiche (7 images : nom et tourbillon remis en page une fois) |
| OK | Polices et chemins mis en cache | 0 allocation (attendu 0) |
| OK | Charge de l'interface (estimation, 1x) | en jeu ~30.3 % d'un coeur, au repos ~3.8 % |
| OK | Passage au ralenti au repos | fond a 4 images/s, rien d'autre ne bouge |
