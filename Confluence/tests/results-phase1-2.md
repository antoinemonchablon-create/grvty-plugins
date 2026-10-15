<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Banc de tests — phases 1 et 2 (Linux, 2026-10-07)

```

=== Théorie : accords contre une table de référence calculée indépendamment ===
OK     Formes d'accords (17 gammes x degrés x 12 types) : 2628 formes, 0 differences
OK     Note jouée -> accord (mode Gamme, voicing neutre) : 106704 accords, 0 differences

=== Théorie : accords connus (notes et noms écrits à la main) ===
OK     Accords connus : 38 / 38 corrects (notes, nom, degré)
OK     Formes fixes (mode Accord fixe) : 21 / 21 noms corrects 

=== Aucune note hors de la gamme (modes Gamme, Touches blanches, Pads) ===
OK     Notes dans la gamme : 646272 accords (17 gammes x 12 toniques x 3 modes x 12 types x 128 notes, voicings au hasard), 0 hors gamme
OK     Notes sans accord : 0 (plage trop étroite pour la classe de hauteur, hors des notes MIDI)

=== Voicing : renversements, ecartements, voix, plage, basse ===
OK     Serré, renversement 0 : C3 E3 G3 B3
OK     Renversement 1 : E3 G3 B3 C4
OK     Renversement 2 : G3 B3 C4 E4
OK     Renversement 3 : B3 C4 E4 G4
OK     Ouvert : C3 G3 E4 B4
OK     Drop 2 : G2 C3 E3 B3
OK     Drop 3 : E2 C3 G3 B3
OK     Large (2 octaves) : C2 G3 E4 B4
OK     Voix max 4 sur Cm9 (sans quinte) : C3 D#3 A#3 D4
OK     Voix max 3 + basse (la basse joue la fondamentale) : D#3 A#3 D4 + basse C2
OK     200 000 voicings au hasard : 200000 accords : 0 hors plage, 0 doublons, 0 trop de voix, 0 basses incorrectes
OK     Voice leading Auto (demi-tons par voix et par changement, Auto contre renversement 0) : I vi IV V (majeur, 7e) : 0.75 contre 4.89 ; ii V I vi (majeur, 9e) : 1.23 contre 6.89 ; i VI III VII (mineur, triades) : 0.90 contre 7.29 ; i iv v i (dorien, 7e) : 0.86 contre 4.00 ; i bII i bVII (phrygien, triades) : 1.62 contre 5.05 ; 

=== Détection de la gamme (mélodies de test connues, la bonne gamme dans le top 3) ===
OK     La mineur (naturel) : rang 1 (A Mineur naturel, A Phrygien, E Phrygien)
OK     Do majeur : rang 1 (C Majeur, C Lydien, C Mixolydien)
OK     Ré dorien : rang 1 (D Dorien, F Lydien, B Locrien)
OK     Mi phrygien : rang 1 (E Phrygien, E Locrien, F Lydien)
OK     Sol mixolydien : rang 1 (G Mixolydien, D Dorien, B Locrien)
OK     Fa# mineur : rang 1 (F# Mineur naturel, F# Phrygien, C# Phrygien)
OK     Do mineur harmonique : rang 1 (C Mineur harmonique, C Mineure hongroise, G Phrygien dominant)
OK     La pentatonique mineure : rang 1 (A Pentatonique mineure, A Blues, A Phrygien)
OK     Mi phrygien dominant : rang 1 (E Phrygien dominant, E Double harmonique, A Mineur harmonique)
OK     Fa lydien : rang 1 (F Lydien, A Mineur naturel, C Majeur)
OK     Analyse dans le plugin (ré dorien joué) : 1er : D Dorien, 16 notes
OK     Enchainements proposés : V->I oui, ii->V oui, i->VII/iv oui, phrygien i->bII oui

=== Modes de jeu : Gamme (hors gamme), Touches blanches, Accord fixe, Pads ===
OK     Note hors gamme (C#3 en do majeur) : Plus proche -> Dm7 ; Au-dessus -> Dm7 ; En dessous -> Cmaj7 ; Ignorer -> rien ; 
OK     Touches blanches en F# mineur : C3=F#m7 D3=G#m7b5 E3=Amaj7 F3=Bm7 G3=C#m7 A3=Dmaj7 B3=E7 
OK     Touches noires (C#3 = variante du degré i) : Sus4=F#sus4 Sus2=F#sus2 9e=F#m9 Mode parallèle=F#maj7 Muettes=rien
OK     Accord fixe m9 sur A, A#, C#, F# : Am9 A#m9 C#m9 F#m9 
OK     Pads (C1 a G1) : 8 notes sur 20 jouent un pad ; pad 1 = Am7

=== MIDI : latence nulle, strum, canal de sortie, bornes ===
OK     Latence 0 (strum et humanize à 0) : 480 accords sur 3 fréquences x 4 tailles de bloc : 0 accords dont une note n'est pas au même échantillon
OK     Strum (37,3 ms et 1/32 à 128 BPM) : 1008 notes, écart maximal 0.50 échantillon, 0 hors tolerance
OK     Canal de sortie respecté (canal 7, entrée sur 1 et 3) : toutes les notes et CC sur le canal 7
OK     Notes 0-127, horodatages dans le bloc et dans l'ordre : 19 evenements, 0 erreurs (notes jouees a C8+3 oct. et C-2)
OK     CC, pitch bend, sustain transmis : CC74 oui, pitch bend oui, sustain oui
OK     Pas de note restante : 0 note(s) restante(s)
OK     Durée fixe 1/16 (touche courte et touche longue) : 8 notes, durées 6000..6000 échantillons (attendu 6000)

=== Note-off : exactement les notes de son accord, Latch ===
OK     Relâchement après changement de tous les réglages : coupe 4 notes (A C E G, canal 1) ; 4 notes du 2e accord restent jusqu'a son relachement
OK     Notes partagées entre deux accords : E et G restent tant que Em est tenu, relancées proprement
OK     Latch : 4 notes tenues apres relachement, remplacees a la note suivante, coupees quand le Latch s'eteint

=== All Notes Off : Stop, bypass, changement de preset, desactivation, Panic ===
OK     Stop du transport : 4 notes sonnaient, 0 restent, 0 notes en attente parties ensuite
OK     Bypass : 4 notes sonnaient, 0 restent, 0 notes en attente parties ensuite
OK     Changement de preset : 4 notes sonnaient, 0 restent, 0 notes en attente parties ensuite
OK     Rechargement d'etat (projet) : 4 notes sonnaient, 0 restent, 0 notes en attente parties ensuite
OK     Desactivation du plugin : 4 notes sonnaient, 0 restent, 0 notes en attente parties ensuite
OK     Bouton Panic : 4 notes sonnaient, 0 restent, 0 notes en attente parties ensuite
OK     Panic (CC123 + sustain 0) et bypass (note telle quelle) : CC123 oui, sustain 0 oui, bypass : 1 note jouee telle quelle, coupee a la sortie du bypass

=== Notes bloquées : événements au hasard ===
OK     44.1 kHz, blocs de 1 : 100000 événements, 43971 note-on / 43971 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0
OK     44.1 kHz, blocs de 64 : 100000 événements, 48173 note-on / 48173 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0
OK     44.1 kHz, blocs de 512 : 100000 événements, 48263 note-on / 48263 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0
OK     44.1 kHz, blocs de 2048 : 100000 événements, 47516 note-on / 47516 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0
OK     48.0 kHz, blocs de 1 : 100000 événements, 44171 note-on / 44171 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0
OK     48.0 kHz, blocs de 64 : 100000 événements, 46633 note-on / 46633 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0
OK     48.0 kHz, blocs de 512 : 100000 événements, 47852 note-on / 47852 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0
OK     48.0 kHz, blocs de 2048 : 100000 événements, 46993 note-on / 46993 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0
OK     96.0 kHz, blocs de 1 : 100000 événements, 43211 note-on / 43211 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0
OK     96.0 kHz, blocs de 64 : 100000 événements, 45077 note-on / 45077 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0
OK     96.0 kHz, blocs de 512 : 100000 événements, 44789 note-on / 44789 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0
OK     96.0 kHz, blocs de 2048 : 100000 événements, 44339 note-on / 44339 note-off ; restantes 0, note-on en double 0, note-off orphelins 0, erreurs de format 0
OK     Total : 1200000 événements au hasard, 550988 notes jouées, aucune note bloquée

=== État : sauvegarde / rechargement, Learn, export MIDI ===
OK     Sauvegarde puis rechargement : 35 paramètres (0 différences), 8 pads identiques, accord appris identique
OK     Learn : C E G Bb joués ensemble -> forme 0 4 7 10, mode Accord fixe / Appris ; D joué ensuite -> D3 F#3 A3 C4
OK     Export MIDI (.mid relu) : 4 mesures : 8 accords identiques au plugin, fin a 4.00 mes. ; 8 mesures : 8 accords identiques au plugin, fin a 8.00 mes. ; 

=== Performance et allocations (bloc de 512 a 48 kHz) ===
OK     processBlock (MIDI seul : 8 voix + basse, strum, humanize, voice leading) : moyenne 2.19 µs (0.021 % d'un cœur), 99e centile 8.47 µs, pire 610.3 µs ; objectif < 0,5 %
OK     processBlock avec la voix Preview (information) : moyenne 279.52 µs (2.62 % d'un cœur), pire 13669.6 µs
OK     Aucune allocation dans processBlock : 0 allocation(s) sur 3000 blocs (accords, strum, humanize, Preview, Panic, bypass)

=== Interface provisoire : captures ===
OK     provisoire_1x.png : 960 x 600
OK     provisoire_2x.png : 1920 x 1200

TOUS LES TESTS PASSENT
```

## pluginval (Linux, strict niveau 10)

```
Starting tests in: pluginval / Scan for plugins located in: /home/claude/Confluence/build-linux/Confluence_artefacts/Release/VST3/Confluence.vst3...
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
