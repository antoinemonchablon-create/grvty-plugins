# Ebb — journal de développement

Sidechain « volume shaper » VST3 / AU pour Ableton : une courbe de volume calée sur le tempo, une note MIDI ou un kick
fait pomper le son de la piste. JUCE 8, C++17, CMake. Même univers visuel et même chaîne de compilation que
Flow Filter, Undertow et Confluence. Concept inspiré des plugins « un gros bouton » ; noms, courbes, code et visuels originaux.

---

## Phase 1 — Squelette, moteur, 3 déclenchements, routage Ableton, banc de tests (13 octobre 2026)

**Format**
- Effet audio VST3 + AU (AU de type « Music Effect » : le type d'effet Audio Unit qui accepte le MIDI).
- Bus : entrée / sortie principales mono ou stéréo (même format), entrée **Sidechain** mono, stéréo ou désactivée.
  Entrée MIDI pour le mode MIDI. Latence 0.
- Codes : fabricant `Jspr` (le même que Flow Filter, Undertow et Confluence), plugin `Ebbp`, bundle `com.jasperr.ebb`.

**6 paramètres, IDs figés** (`Source/Params.h`) : `amount`, `curve`, `rate`, `trigger`, `nudge`, `threshold`.
On pourra en ajouter, jamais en renommer, en retirer ni changer leur plage ou leur liste de choix (un test le vérifie).

**Courbes** (`Source/dsp/Curves.h`) : 12 formes originales (Splash, Snap, Ripple, Current, Ledge, Twin, Gate, Swell,
Drift, Breath, Surge, Backwash), écrites en formules puis précalculées en tables de 8192 points (interpolation linéaire).
Gain = 1 - Amount x courbe, toujours entre 0 et 1.

**Moteur** (`Source/dsp/PumpEngine.h`)
- SYNC : position calculée à chaque échantillon depuis la position d'Ableton (pas d'erreur qui s'accumule), courbes
  recalées à chaque mesure (signature lue chez l'hôte), Nudge -20 / +20 ms. Transport arrêté : roue libre au dernier tempo.
- MIDI : chaque note-on relance la courbe à son échantillon exact ; durée = Durée au tempo du projet ; à la fin,
  retour en douceur (15 ms).
- AUDIO : détecteur d'attaque dans les graves (passe-bas 300 Hz, enveloppe rapide, référence = plus haut niveau
  entre 25 et 2 ms avant, +3 dB et montée d'au moins 70 % du seuil, réarmement à 60 % du pic après 35 ms).
- Anti-clic : le gain ne bouge jamais de plus de 1/(1 ms) par échantillon (relance, changement de courbe, de Durée,
  d'Amount, de mode, Play / Stop, saut de boucle). Amount lissé sur 30 ms.
- Aucune allocation, aucun verrou dans processBlock (gains calculés par morceaux de 1024 dans un tableau fixe).

**Interface provisoire** : la vague (profil de la courbe), la goutte qui la parcourt, le niveau de l'eau qui suit le gain
réel, un rond dans l'eau à chaque départ ; gros bouton Amount ; Courbe, Durée, Déclenchement, Nudge, Seuil.
Fond et textes en cache, comme Confluence.

**Corrections pendant la phase**
- Détecteur v1 (enveloppe lente) : déclenchait jusqu'à 9,5 ms en retard sur un kick à longue queue (la référence restait
  gonflée par le kick précédent), et une fois sur une basse. Remplacé par la référence « plus haut niveau 25-2 ms avant »
  + condition de montée : 1,0 à 2,5 ms sur tous les kicks testés, rien sur une basse tenue.
- Twin avait une marche au milieu de la courbe : la seconde baisse a maintenant sa propre attaque (1 % de la durée).
- En « 1 mesure », le journal des départs ratait des mesures (numéro de mesure relatif à la mesure en cours) : numéro absolu.
  Le son n'était pas touché, seulement l'affichage du départ.

**Mesures (Linux, cloud, 2 cœurs)** — détail dans `tests/results-phase1.md`

| Mesure | Valeur |
|---|---|
| 12 courbes, table contre formules réécrites à part | écart max 0,0006 |
| Rendu réel d'un cycle (1 mesure à 60 BPM) contre référence + anti-clic réécrits à part | écart max 0,0006 |
| NaN / inf / dénormaux, 44,1 / 48 / 96 kHz, blocs 1 / 64 / 512 / 2048, mono et stéréo, automation au hasard | 0 sur 6,8 millions d'échantillons |
| Sortie plus forte que l'entrée | 0 |
| Amount 0 % (SYNC, MIDI, AUDIO, 12 configurations) | identique bit à bit, 2,26 millions d'échantillons |
| Saut de gain max par échantillon (≈ 230 changements par fréquence : courbe, Durée, Amount, mode, Play/Stop, saut, Nudge, relance MIDI) | égal à la limite de 1 ms, jamais au-dessus |
| SYNC : débuts de courbe, 7 durées x 3 fréquences x 4 tempos (120 / 128 / 140 / 150), 64 mesures | 33 108 débuts, 0 échantillon d'écart, 0 manquant |
| Nudge +10 / -7,5 ms ; Play au milieu d'un temps ; Stop ; boucle ; 120 -> 140 BPM ; 3/4 ; sans tête de lecture | 0 échantillon d'écart |
| MIDI : départ de courbe = note (12 configurations) | 116 notes, 0 échantillon d'écart |
| AUDIO : 6 kicks (128 / 140 / 150 / 124 / 128 / 174 BPM, faibles, longue queue, avec charleys, mono) | 384 / 384, délai 1,0 à 2,5 ms (objectif < 3 ms) |
| AUDIO : basse tenue et liée, sub-basse 31-33 Hz, charleys seuls (8 s chacun, seuil -40 dB) | 0 déclenchement pendant le son (1 au tout début de la basse, qui démarre après un silence) |
| Formats : sidechain absent / mono / stéréo, entrée mono / stéréo | 6 / 6 |
| Sauvegarde / rechargement | 50 états au hasard, 0 différence |
| CPU (bloc de 512 à 48 kHz, stéréo, mode AUDIO avec kicks) | 10 µs = 0,1 % d'un cœur (objectif < 0,3 %) |
| Allocations dans processBlock (3 000 blocs, 3 modes, MIDI, bypass, Play/Stop) | 0 |
| Dessin à 1x (rendu logiciel) : image pleine / image courante, 95e centile | 5,7 ms / 1,5 ms (objectif < 16 ms) |
| Dessin à 2x (rendu logiciel Linux, information) : image pleine (8 par seconde au plus) / image courante, moyenne | 18 à 20 ms / 6 à 9 ms |
| Allocations dans paint() pendant l'animation (notre code) | 0 |
| Au repos (AUDIO sans kick) | 0 image redessinée en dehors du fond (4 par seconde) |
| pluginval strict niveau 10, VST3 (Linux) | SUCCESS |

À 2x, le rendu logiciel de Linux dépasse 16 ms sur l'image pleine (fond d'eau recalculé, 8 fois par seconde au plus).
Sur Mac, les images passent par CoreGraphics ; à vérifier à l'œil dans Ableton (fluidité de la goutte).

**Maquettes d'interface** (`maquettes/`) : A « Houle », B « Puits », C « Marée ». En attente du choix pour la phase 2.

**Livraison sur le Mac (14 octobre 2026)** : 29 fichiers transférés, vérifiés octet par octet (sha256 + cmp). v1.0.1 compilée sans erreur
(`Built target Ebb_VST3` et `Ebb_AU`), installée dans `~/Library/Audio/Plug-Ins`, **pluginval strict 10 sur le Mac : SUCCESS pour le VST3 et l'AU**.

---

## Phase 2 — Interface « Houle », courbe perso, Tension, Lissage, multibande (14 octobre 2026)

Choix d'Antoine : maquette **A « Houle »** ; pouvoir **dessiner sa propre courbe** ; en plus : **Tension + Lissage** et
**multibande**.

**5 paramètres ajoutés** (versionHint 2, après les 6 de la phase 1, jamais à leur place) : `custom` (Ma courbe),
`tension` (-100 à +100 %), `smooth` (0-100 %), `band` (Plein / Graves / Aigus), `crossover` (40 Hz - 2 kHz, 150 Hz).
Un projet enregistré avec la phase 1 se recharge avec ses réglages, les nouveaux à leur valeur par défaut (testé).

**Courbe perso** (`Source/dsp/Shape.h`) : jusqu'à 64 points (x, niveau, courbure du segment), marches possibles
(deux points au même x). Rendue en table de 8192 points comme les courbes d'usine ; l'interface la publie au thread audio
par triple tampon (aucun verrou, aucune allocation). Enregistrée dans l'état du plugin (projet Ableton) avec 9 chiffres
significatifs (aller-retour exact) ; « mes courbes » en fichiers `.ebbshape` (`~/Library/Audio/Presets/Jasperr/Ebb/Courbes`).
Une courbe d'usine se convertit en points (Ramer-Douglas-Peucker, 26 points au plus, écart < 0,006) pour la retoucher.

**Tension** : r' = r / (r + (1 - r) K), K = 2^(3 x tension), par échantillon, lissée sur 30 ms ; à 0, aucun calcul (son
identique bit à bit). **Lissage** : 3 passes de moyenne glissante circulaire sur la table (jusqu'à 8 % de la durée),
recalculé seulement quand la courbe ou le lissage changent (O(n), environ 30 µs).

**Multibande** (`Source/dsp/Crossover.h`) : Linkwitz-Riley 24 dB/oct en structure TPT (même algorithme que JUCE, en
tableaux fixes). GRAVES : g x graves + aigus ; AIGUS : graves + g x aigus ; graves + aigus = passe-tout, donc niveau
inchangé quand rien ne pompe. Fondu de 40 ms quand on change de bande, fréquence lissée sur 30 ms.
En PLEIN, le son ne passe pas par la séparation (identique bit à bit à Amount 0 %).

**Éditeur** (`Source/ui/CurveView.*`) : clic = point, glisser, poignée de courbure, double-clic / clic droit = supprimer,
grille aimantée (—, 4, 8, 16, 32 ; niveaux par 1/16), Alt = libre ; annuler / rétablir (un geste = une étape, 100 étapes) ;
MIROIR, INVERSER, EFFACER ; MES COURBES, ENREGISTRER (panneau dans la fenêtre du plugin, comme Confluence).
En édition, la courbe est à pleine échelle, avec en blanc ce qu'on entend (Amount, Tension, Lissage).

**Interface** 920 x 400, redimensionnable 70-200 %. Les 12 courbes + MA COURBE en vignettes, durée et déclenchement en
pilules, Nudge / Seuil / Fréq grisés quand ils ne servent pas, légende « seuls les graves pompent · < 150 Hz ».
Le fond d'eau est agrandi en deux temps à 2x (quart -> taille logique -> taille physique) : 25 % moins cher.

**Mesures (Linux, cloud, 2 cœurs)** — 104 tests, tous OK (`tests/results-phase2.md`)

| Mesure | Valeur |
|---|---|
| 300 courbes perso au hasard (marches, courbures) : table contre référence réécrite à part | écart max 6e-7 |
| Courbe perso jouée (moteur, échange sans verrou) | écart max 0,0001 |
| Tension -100 / -50 / +50 / +100 % contre formule | écart max 0,0003 ; à 0 : identique bit à bit |
| Lissage 10 / 40 / 100 % : table contre lissage naïf, et son | écart 0,000000 / 0,00001 |
| Amount 0 % en GRAVES et AIGUS, 30 Hz à 15 kHz | niveau inchangé à 0,0003 dB près |
| Pendant la baisse, séparation 150 Hz | GRAVES : 40 Hz -46 dB, 2 kHz 0,00 dB ; AIGUS : 40 Hz -0,04 dB, 2 kHz -90 dB |
| Bande et fréquence changées pendant la lecture | pas de clic (saut max = pente du signal) |
| Anti-clic avec Tension, Lissage, courbe perso changés pendant la lecture (≈ 230 changements par fréquence) | jamais au-dessus de la limite de 1 ms |
| Éditeur à la souris : ajout aimanté, glisser, Alt, courbure, double-clic, annuler / rétablir, miroir, inverser, vignettes, enregistrer / recharger | 13 / 13 |
| État : 30 états au hasard (11 paramètres, points, table, grille) ; projet de la phase 1 | 0 différence |
| CPU, courbe d'usine pleine bande / tout allumé (courbe perso, Tension, Lissage, GRAVES) | 0,09 % / 0,21 % d'un cœur (objectif < 0,3 %) |
| Allocations dans processBlock (3 000 blocs, tout changé au hasard) ; dans paint() pendant l'animation | 0 ; 0 |
| Dessin à 1x : image pleine / image courante, 95e centile | 8 ms / 1,9 ms (objectif < 16 ms) |
| Dessin à 2x, rendu logiciel Linux (information) : image pleine (8/s au plus) / image courante, moyenne | 26 ms / 7,5 ms |
| pluginval strict niveau 10, VST3 (Linux) | SUCCESS |

**Livraison sur le Mac (14 octobre 2026, phase 2)** : 40 fichiers transférés, vérifiés octet par octet (sha256 + cmp). v1.0.2 compilée sans erreur
(`Built target Ebb_VST3` et `Ebb_AU`), **pluginval strict 10 sur le Mac : SUCCESS pour le VST3 et l'AU**.


## GRVTY Records et logiciel libre (15 octobre 2026, v1.0.3)

- Le fabricant devient **GRVTY Records** (Plug-Ins > GRVTY Records dans Ableton). Codes du plugin, BUNDLE_ID et IDs de paramètres inchangés : les projets Ableton et les presets existants se rechargent tels quels.
- Licence libre GNU AGPL v3 ou ultérieure : fichiers LICENSE, NOTICE (noms et logos réservés), THIRD-PARTY-NOTICES.md, en-têtes SPDX dans toutes les sources.
- Bouton ⓘ en haut à droite : panneau À propos (copyright, licence, absence de garantie, confidentialité, code source, marques). Mention « GRVTY RECORDS » sous le logo.
- Courbes utilisateur : ~/Library/Audio/Presets/GRVTY Records/Ebb/Courbes (les anciennes du dossier Jasperr y sont recopiées une fois, rien n'est effacé).
- Aucun changement de son. JUCE 8.0.12 ; décodeurs FLAC/Ogg désactivés (inutiles).
- Tests (Linux) : 105 tests OK, panneau À propos vérifié, pluginval strict 10 : SUCCESS.
