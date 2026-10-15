# Undertow — journal de développement

Synthé FM monophonique à 2 oscillateurs + séquenceur, pour boucles de techno hypnotique, hardgroove et mentale.
JUCE 8, C++17, CMake. Même univers visuel et même chaîne de compilation que Flow Filter.

---

## Phase 1 — Squelette, moteur sonore, banc de tests (1er octobre 2026)

**Fait**
- Projet CMake (VST3, AU, Standalone), instrument avec entrée MIDI, codes `Jspr` / `Undt`.
- **69 paramètres, IDs figés** (`Source/Params.h`). On pourra en ajouter, jamais en renommer ni en retirer.
- Voix FM (`Source/dsp/FmVoice.h`) :
  - osc A porteur, osc B modulateur, tous deux audibles (A/B) ; forme sinus → triangle → carré arrondi ;
  - ratio B par pas musicaux ou libre ; Index, Feedback (moyenné sur 2 échantillons), mode B→A ou croisé A↔B ;
  - enveloppe Mod AD à courbe réglable vers Index, Cutoff, Pitch ;
  - filtre TPT (LP/BP/HP simultanés, fondu entre types), 2e étage pour le 24 dB, drive avant filtre ;
  - ADSR d'ampli qui repart du niveau courant (pas de clic au retrigger), legato, glide.
- 2 LFO (5 formes, Hz ou synchro calée sur la position d'Ableton, 13 destinations, retrigger).
- FX : Drive, Delay ping-pong synchro (tone et saturation dans la boucle, fondu enchaîné quand la division change), petite Reverb.
- Suréchantillonnage Eco 2x / HQ 4x (demi-bande IIR), bascule par fondu en S.
- Limiteur doux de sécurité en sortie (jamais au-dessus de -0,2 dBFS).
- Interface de travail provisoire qui reprend le LookAndFeel et les composants de Flow Filter (copiés dans `Source/ui/LiquidUI.*`).

**Mesures (Linux, cloud, 2 cœurs, plus lent qu'un Mac M1)**

| Mesure | Valeur |
|---|---|
| NaN / inf / dénormaux (3 fréquences × 4 tailles de bloc × défaut/extrême) | 0 |
| Pic avec réglages extrêmes (Index, Feedback, Reso au max, FX à fond, Volume +6 dB) | -0,18 dBFS |
| Clics : 51 paramètres automatisés min → max d'un coup | pire cas +2,0 dB (filter type), seuil +6 dB |
| Clics : notes on / off / retrigger / slide | +0,24 dB (seuil +3 dB) |
| Changement complet de réglages en lecture | -63,9 dB |
| Aliasing FM ~5 kHz, Index 10 — Eco 2x | -44,8 dB (pire raie -43,6 dBc) |
| Aliasing FM ~5 kHz, Index 10 — HQ 4x | -89,9 dB (pire raie -93,7 dBc) |
| CPU, 512 éch. à 48 kHz, HQ + FX | 353 µs/bloc = 3,3 % d'un cœur |
| CPU, idem en Eco | 256 µs/bloc = 2,4 % d'un cœur |
| pluginval strict niveau 10, VST3 (Linux) | SUCCESS |

**3 postes les plus coûteux (HQ)** : voix + suréchantillonnage montant 83,5 %, reverb 8,1 %, delay + panoramique 5,5 %.
Pistes pour la phase 5 : lecture des paramètres des LFO à chaque échantillon, `tan()` / `log2()` / `pow()` par échantillon dans la voix,
suréchantillonnage montant appliqué à un signal nul (inutile).

**Bugs trouvés par le banc et corrigés**
- Sustain monté pendant le Decay : le niveau sautait d'un coup (+48 dB de discontinuité) → remonte maintenant en douceur.
- Env du filtre, Mod → Index/Cutoff/Pitch non lissés (+49 dB) → lissés sur 8 ms.
- Passage 12 → 24 dB : le 2e étage démarrait vide (+11,7 dB) → il tourne en permanence.
- Changement de division du delay : effet « bande » (+39,7 dB) → fondu enchaîné de 30 ms entre les deux lectures.
- Bascule Eco/HQ (+15,5 dB) → fondu en S de 5 ms + 2 ms de silence le temps que le nouveau filtre se stabilise.

---

## Phase 2 — Séquenceur et synchro transport (1er octobre 2026)

**Fait** (`Source/seq/`)
- 1 à 32 pas ; 1/8, 1/16, 1/32 et triolets ; swing 50-75 %.
- Par pas : note, octave, vélocité, accent, gate, slide (style 303 : la note glisse vers le pas suivant), probabilité, ratchet 1-4, p-locks Index et Cutoff.
- Piste Mod séparée (Index, Cutoff ou Ratio) avec sa propre longueur → polymétrie.
- 6 gammes + tonique, notes forcées dans la gamme ; transposition par note MIDI (référence C3) en mode SEQ, jeu direct en mode KEYS.
- Générateurs : Euclidien, Random dans la gamme (la tonique revient souvent), décalage gauche/droite, Mutate (variation calculée depuis l'original, une période sur deux).
- Probabilités et Mutate déterministes : une boucle rendue deux fois sonne pareil.
- Chaque pas est placé depuis la position d'Ableton (aucune accumulation) → aucune dérive possible, suit boucles et changements de tempo.
- Motif sauvegardé dans le projet (état du plugin) ; transmis au thread audio sans verrou (triple tampon).

**Mesures**

| Test | Résultat |
|---|---|
| 120 / 128 / 140 / 150 BPM × 64 mesures, 4 configurations (44,1 / 48 / 96 kHz, blocs 1 / 64 / 333 / 512, swing 50-66 %) | chaque pas à l'échantillon exact, dérive 0 |
| Stop | silence (-240 dBFS) après le release, aucune note bloquée |
| Relance à la mesure 5 | 1er pas à l'échantillon exact du Play |
| Boucle Ableton de 2 mesures × 6 | repart au pas 1 à l'échantillon exact, 32 pas par tour |
| Tempo 120 → 140 BPM en cours de lecture | 256 pas, 0 mal placé |
| Polymétrie 16 contre 13 | 0 erreur |
| Sauvegarde / rechargement | 69 paramètres + motif (32 pas, piste Mod, p-locks) identiques |

---

## Phase 3 — Interface définitive, maquette A « Lagon » (1er octobre 2026)

**Fait**
- 900 × 470, redimensionnable de 70 % à 200 % sans déformation (tout le contenu est mis à l'échelle), net sur Retina.
- Écran FM vivant : la surface de l'eau est la vraie forme d'onde (2 cycles, mêmes formules que la voix), elle respire avec l'enveloppe ;
  à chaque note une goutte tombe et forme des ondes dont le nombre et la force suivent l'Index (corail sur les accents).
  Glisser sur l'écran : vertical = Index, horizontal = Ratio.
- Grille d'eau : colonnes qui se remplissent selon la note, vague de lecture qui soulève les colonnes, 16 ou 32 pas affichés selon la longueur,
  note jouée affichée (convention Ableton C3 = 60), Maj+clic = accent, Alt+clic = slide, clic droit = ratchet, proba, gate, vélocité, p-locks, copier.
- Barre de presets reprise de Flow Filter (banques, SAVE, dé), onglets OSC / FILTER / MOD / FX
  (4 onglets au lieu de 3 : tous les réglages tiennent sans rien cacher), courbes d'enveloppes et de LFO.

**Performance de l'interface** (machine de test cloud, rendu logiciel, nettement plus lente qu'un Mac)

| Mesure | Avant optimisation | Après |
|---|---|---|
| Image complète | 17,8 ms | 10,5 ms (fond recalculé) / 2-5 ms (fond en cache) |
| Image animée (écran FM, grille, vumètre) | — | 4,1 ms |
| Charge en lecture | ~107 % d'un cœur | ~30 % |
| Charge au repos | ~25 % | ~4 % |
| Allocations par image | ~3 000 | ~590 (complète) / ~390 (animée) |

Comment : fond (eau + panneaux + textes fixes) calculé dans une image à la résolution de l'écran et rafraîchi 8 fois/s (4 au repos),
l'eau elle-même calculée à mi-résolution ; chaque contrôle garde son image et n'est redessiné que si sa valeur change ou s'il est survolé ;
seuls l'écran FM, la grille, le vumètre et le logo sont redessinés à 60 images/s ; polices et chemins réutilisés (0 allocation de notre côté).
Les allocations restantes sont faites par les fonctions de dessin de JUCE (texte, dégradés, remplissage), qu'on ne peut pas éviter sans réécrire le moteur de rendu.

---

## Phase 4a — Système de presets + 9 premiers presets (1er octobre 2026)

**Fait**
- Presets d'usine = son + motif (motifs écrits en texte lisible dans `Source/presets/FactoryPresets.h`).
- Presets utilisateur `.utpreset` (son + motif) dans `~/Library/Audio/Presets/Jasperr/Undertow/`.
- Dé = son au hasard dans des plages musicales (le motif est gardé) ; menu : motif au hasard (le son est gardé).
- Un preset ne touche jamais au mode SEQ/KEYS ni à la qualité Eco/HQ.
- Une nouvelle instance s'ouvre sur Breakwater : Play = une boucle qui sonne.
- L'accent joue aussi un peu plus fort (+30 % × Accent).
- Drive de sortie : courbe progressive (réglage fin en bas de course) — à faible valeur il écrasait le son en signal carré.

**Mesures (rendus de 4 mesures depuis le Play, au tempo de la banque)**

| Preset | Banque | BPM | LUFS | Pic |
|---|---|---|---|---|
| Breakwater | Hardgroove | 135 | -18,0 | -4,6 dBFS |
| Rolling Surf | Hardgroove | 135 | -18,0 | -11,1 dBFS |
| Steel Tide | Hardgroove | 135 | -18,0 | -6,3 dBFS |
| Abyssal Loop | Hypnotic | 130 | -18,0 | -6,4 dBFS |
| Slow Tide | Hypnotic | 130 | -18,0 | -7,2 dBFS |
| Deep Pressure | Hypnotic | 130 | -18,0 | -5,1 dBFS |
| Maelstrom | Mental | 142 | -18,0 | -10,2 dBFS |
| Vortex | Mental | 142 | -18,0 | -5,4 dBFS |
| Crosscurrent | Mental | 142 | -18,0 | -9,1 dBFS |

Écart de volume entre presets : 0,1 dB (exigé ±2 dB). Cible -18 LUFS : laisse de la place dans un mix ;
les sons percussifs (16 dB entre crête et volume moyen) dépasseraient -1 dBFS à -16 LUFS.

**Corrigé grâce aux spectrogrammes**
- Rolling Surf, Maelstrom, Crosscurrent : le Drive écrasait le signal en carré → courbe du Drive revue.
- Breakwater : un simple clic de 20 ms → decay d'ampli 170 ms.
- Crosscurrent : filtre trop ouvert, pas de « squelch » acide → cutoff plus bas, enveloppe Mod plus ample.

---

## Phase 4b — 20 presets de plus + oscilloscope en direct (1er octobre 2026)

**Oscilloscope** (écran de gauche, voyant LIVE)
- La courbe principale est maintenant le **signal réel qui sort du plugin** (après filtre, effets et volume).
- Vue ONDE : 2 cycles de la note jouée, calés sur un passage par zéro → l'image ne défile pas, on voit la vraie forme.
- Vue MESURE : enveloppe min/max de la dernière mesure, avec les 4 temps.
- La forme théorique (calculée depuis les réglages) reste en fantôme derrière ; la note jouée et le niveau de crête s'affichent.
- Côté audio : copie de la sortie dans deux tampons circulaires sans verrou (8192 échantillons + 4096 paquets min/max).

**20 nouveaux presets** — 29 au total, avec un type (PERC / BASS / SYNTH / FX) affiché dans la barre et le menu

| Hardgroove (135 BPM) | Type | Hypnotic (130 BPM) | Type |
|---|---|---|---|
| Tribal Current — toms FM, ratchets | PERC | Kelp Forest — pluck, piste Mod 13 pas | SYNTH |
| Steel Conga — congas, slaps en p-lock | PERC | Night Swim — cloches, grande reverb | SYNTH |
| Iron Bell — cloche à deux carrés | PERC | Dark Lagoon — 3 notes contre 16, p-locks cutoff | BASS |
| Ride Spray — ride métallique, panoramique S&H | PERC | Pendulum — Mutate toutes les 4 mesures | SYNTH |
| Rimshot Rattle — rim court, probabilités, Mutate | PERC | Bioluminescence — bleeps rares, delay triolet | FX |
| Rave Hoover — hoover rave, plongée de hauteur | SYNTH | Low Tide Drone — drone, Index en S&H | FX |
| Groundswell — basse + notes fantômes | BASS | Sonar — ping + delay dub | FX |
| Shoreline Stab — stab en quinte, swing 58 % | SYNTH | Dub Basin — accord dub (quinte), delay long | SYNTH |
| Wave Hammer — basse martelée, drive fort | BASS | Deep Rotation — basse roulante 16es | BASS |
| Flare Zap — zaps laser, panoramique | FX | Sea Foam — shaker / hat bruité | PERC |

Mesures : les 29 presets à **-18,0 LUFS** (écart 0,1 dB), pics de -2,2 à -13,5 dBFS (exigé < -1).
7 presets trop « pointus » (sons très courts ou très espacés, jusqu'à 22 dB entre crête et volume) ont été domptés
avec un peu de drive et des enveloppes plus pleines : Sea Foam, Sonar, Shoreline Stab, Iron Bell, Ride Spray, Dub Basin, Deep Rotation.

**Interface** : fond agrandi en mode simple (dégradés doux, aucune différence visible) → recalcul du fond 6,8 → ~4 ms ;
tracé de l'oscilloscope allégé 2,3 → 1,5 ms. Charge estimée (machine de test) : ~30 % en lecture, ~4 % au repos.

**Banc de tests** : 86 tests OK, pluginval strict niveau 10 : SUCCESS.

---

## Reste à faire
- Tes retours à l'écoute (presets à garder, modifier, remplacer).
- Phase 5 : boucles d'amélioration.

## Correctif v1.0.3 : plantage d'Ableton à la fermeture (macOS)
- Symptôme : pluginval Mac réussit tous ses tests puis s'arrête sur « mutex lock failed: Invalid argument » ; Ableton plante en quittant.
- Cause : le cache de polices `Ui::font` était un vecteur statique créé avant le registre de polices CoreText de JUCE (statique lui aussi). Au déchargement du plugin, le registre est détruit d'abord ; nos polices, détruites ensuite, verrouillent son mutex déjà détruit.
- Correctif : cache de polices et chemins de travail vidés quand la dernière fenêtre d'Undertow se ferme (`Ui::editorOpened/editorClosed`, membre `CacheGuard` de l'éditeur, détruit après le contenu). Étiquettes de temps de la grille en `const char*` au lieu de `juce::String` statiques.
- Vérification : 86 tests OK (dont 0 allocation dans paint après réouverture), pluginval strict 10 SUCCESS (Linux). À confirmer par pluginval sur le Mac (build.command).

## v1.0.4 : sauvegarde des presets réparée + 20 presets Mental / Abyss
### Sauvegarde des presets
- Symptôme (Ableton, Mac) : impossible d'enregistrer un preset ni d'ouvrir le dossier des presets. Le dossier ~/Library/Audio/Presets/Jasperr n'existait pas : aucune sauvegarde n'avait abouti.
- Cause : SAVE ouvrait une juce::AlertWindow, une fenêtre séparée. Dans Ableton sur Mac, elle s'ouvre derrière la fenêtre flottante du plugin ou derrière Live en plein écran, et reste invisible. L'ouverture du dossier passait par revealToUser (sélection dans le dossier parent), qui ne passe pas le Finder au premier plan.
- Correctif :
  - panneau « Enregistrer le preset » dessiné dans la fenêtre du plugin (champ de nom, Entrée = enregistrer, Échap / clic à côté = annuler, avertissement si le nom existe déjà, bouton REMPLACER) ;
  - bouton dossier dans la barre de presets + entrée de menu : crée le dossier puis l'ouvre dans le Finder (File::startAsProcess) ;
  - confirmation dans la barre (« ENREGISTRE » ou « ERREUR » pendant 2,5 s) ; saveUserPreset renvoie false si le dossier ne peut pas être créé ;
  - menu des presets rangé en sous-menus par banque + sous-menu MES PRESETS (49 presets d'usine, le menu restait trop long).
- Tests : nouveau test « Preset utilisateur (fichier) » : enregistrement, présence dans la liste, rechargement à l'identique (paramètres + motif + nom). Capture save_panel_2x.png.

### 20 nouveaux presets (49 au total)
- MENTAL (142 BPM), 16 de plus : Pressure Drop, Siren Surge, Cavitation, Electric Eel, Storm Surge, Whirl, Rapture of the Deep (les 7 du cahier des charges), Acid Vein ; bleeps : Bleep Signal, Pulse Code, Echo Probe, Telemetry ; roulements : Snare Roll Tide, Tom Cascade, Drum Avalanche, Gallop Roll.
- ABYSS (124 BPM, nouvelle banque, ambient très profond) : Hadal Zone, Abyss Cathedral, Trench Breath, Whale Fall.
- Ajustements : bleeps et Drum Avalanche trop « pointus » (crête 18-22 dB) -> notes plus longues, delay / reverb / drive léger (crête 11-14 dB). Hadal Zone (76 % d'énergie sous 60 Hz) et Whale Fall (43 %) remontés d'un cran pour rester audibles sur des moniteurs.
- Tous à -18.0 LUFS, pics entre -4 et -13 dBFS. Nouvelle couleur d'étiquette pour la banque ABYSS.

## Correctif v1.0.5 : plus de son après un changement de preset (séquenceur)
- Symptôme (Ableton) : les nouveaux presets ne sortent aucun son.
- Reproduction : nouveau test « Changement de preset en lecture » (un seul plugin, transport en marche, 49 presets enchaînés comme avec les flèches) : 16 presets muets (-70 à -200 dB). Le rendu par preset dans un plugin neuf ne voyait rien.
- Cause : le séquenceur retenait le numéro du dernier pas joué (lastFired) pour ne pas le rejouer, mais ce numéro dépend de la cadence. En passant de 1/32 à 1/16 (ou 1/16 à 1/8), le numéro courant est divisé par 2 et le séquenceur attendait de rattraper l'ancien numéro : silence d'autant plus long qu'on est loin dans le morceau. Les nouveaux presets utilisent 1/32, 1/16T et 1/8 ; les anciens étaient presque tous en 1/16, d'où le problème jamais vu avant. Le même bug touchait le réglage RATE tourné en lecture.
- Correctif : on garde aussi la position (en noires) du dernier pas joué ; quand la cadence change, le numéro du dernier pas est recalculé dans la nouvelle grille.
- Tests : 49 presets enchaînés à 44,1 / 48 kHz, depuis la mesure 1 et la mesure 121 : tous sonnent. Changement de cadence en lecture à la mesure 200 (1/32 > 1/16 > 1/8T > 1/8 > 1/32 > 1/32T > 1/16) : la note suivante part au plus tard à 67 % d'un pas. 111 tests OK, pluginval strict 10 SUCCESS.


## GRVTY Records et logiciel libre (15 octobre 2026, v1.0.6)

- Le fabricant devient **GRVTY Records** (Plug-Ins > GRVTY Records dans Ableton). Codes du plugin, BUNDLE_ID et IDs de paramètres inchangés : les projets Ableton et les presets existants se rechargent tels quels.
- Licence libre GNU AGPL v3 ou ultérieure : fichiers LICENSE, NOTICE (noms et logos réservés), THIRD-PARTY-NOTICES.md, en-têtes SPDX dans toutes les sources.
- Bouton ⓘ en haut à droite : panneau À propos (copyright, licence, absence de garantie, confidentialité, code source, marques). Mention « GRVTY RECORDS » sous le logo.
- Presets utilisateur : ~/Library/Audio/Presets/GRVTY Records/Undertow (les anciens du dossier Jasperr y sont recopiés une fois, rien n'est effacé).
- Aucun changement de son. JUCE 8.0.12 ; décodeurs FLAC/Ogg désactivés (inutiles).
- Tests (Linux) : 111 tests OK, 40 presets à -18 LUFS (écart 0,1 dB), panneau À propos vérifié, pluginval strict 10 : SUCCESS.
