# Confluence — journal de développement

Générateur d'accords VST3 pour Ableton : une note jouée -> un accord dans la gamme, envoyé en MIDI à un autre instrument.
JUCE 8, C++17, CMake. Même univers visuel et même chaîne de compilation que Flow Filter et Undertow.

---

## Phase 1 — Squelette, moteur d'accords, routage Ableton, banc de tests (7 octobre 2026)

**Architecture imposée par Ableton**
- Ableton refuse un effet MIDI VST avant un instrument et ne lit pas la sortie MIDI d'un AU :
  Confluence est un **instrument VST3** avec entrée et sortie MIDI (`IS_SYNTH`, `NEEDS_MIDI_INPUT`, `NEEDS_MIDI_OUTPUT`),
  sortie audio stéréo silencieuse. Pas de version AU.
- Routage : piste 1 = Confluence, piste 2 = synthé avec « MIDI From » = piste 1 / Confluence (`ROUTAGE-ABLETON.md`).
- Voix Preview intégrée (sinus + 2 harmoniques, enveloppe douce, 16 voix), coupée par défaut.
- Codes : fabricant `Jspr` (le même que Flow Filter et Undertow), plugin `Cnfl`, bundle `com.jasperr.confluence`.

**35 paramètres, IDs figés** (`Source/Params.h`) : on pourra en ajouter, jamais en renommer, en retirer
ni changer leur plage ou leur liste de choix. Les pads, l'accord appris et le nom du preset sont dans l'état du plugin.

**Théorie** (`Source/theory/`)
- 17 gammes avec étiquette d'ambiance ; quantification (plus proche / au-dessus / en dessous / ignorer).
- 12 types d'accords. Gammes de 7 notes : empilement par pas de la gamme (tierces diatoniques).
  Pentatoniques, blues, hirajoshi, chromatique : chaque note de l'accord prend le premier intervalle d'une liste de préférence
  qui tombe dans la gamme (ex. pas de tierce sur ce degré -> quarte suspendue). Toutes les notes restent dans la gamme.
- 22 formes fixes + forme apprise. Noms d'accords (Am9, Fmaj9(#11), Bm7b5, Ebaug...) et chiffrage romain (i, ii°, III+, bVII).
- Voicing : limite de voix (la quinte et les doublures partent d'abord, puis la fondamentale si la basse la joue),
  renversements 0-3 et Auto (voice leading : le candidat qui bouge le moins, sans dériver loin de la position de départ),
  serré / ouvert / drop 2 / drop 3 / large, plage basse-haute (repli à l'octave), basse à -1 ou -2 octaves.

**Moteur MIDI** (`Source/engine/MidiEngine.h`)
- Une instance par accord joué, notes mémorisées : un relâchement coupe exactement ce que la touche a joué,
  même si les réglages ont changé entre-temps.
- File d'événements triée (tas préalloué) : strum, humanize (retard seulement) et durées fixes à l'échantillon près,
  indépendamment de la taille des blocs. Les événements d'accords arrêtés sont retirés quand la file se remplit.
- Compteur par (canal, note) : une note partagée par deux accords n'est coupée qu'au dernier relâchement ;
  rejouée par un nouvel accord, elle est relancée (note-off puis note-on au même instant), jamais doublée.
- Coupure de toutes les notes (jouées et en attente) : Stop du transport, bypass, changement de preset, rechargement d'état,
  désactivation du plugin (au bloc suivant), bouton Panic (avec sustain 0 et CC123 en plus).
- Bypass : les notes passent telles quelles (et sont coupées à la sortie du bypass).
- CC, pitch bend, aftertouch de canal, program change : recopiés sur le canal de sortie ; aftertouch polyphonique envoyé
  à toutes les notes de l'accord de la touche.
- Aucune allocation, aucun verrou dans processBlock ; interface -> audio par triple tampon sans verrou ;
  hasard (xorshift) initialisé hors du thread audio.

**Mesures (Linux, cloud, 2 cœurs)**

| Mesure | Valeur |
|---|---|
| Formes d'accords contre une table de référence calculée indépendamment | 2 628 formes, 0 différence |
| Note jouée -> accord (17 gammes x 12 toniques x 12 types) | 106 704 accords, 0 différence |
| Accords connus écrits à la main (notes, nom, degré) | 38 / 38 ; formes fixes 21 / 21 |
| Notes hors gamme (modes Gamme, Touches blanches, Pads, voicings au hasard) | 646 272 accords, 0 hors gamme |
| 200 000 voicings au hasard | 0 hors plage, 0 doublon, 0 voix en trop, 0 basse fausse |
| Notes bloquées : 100 000 événements au hasard x 12 configurations (44,1 / 48 / 96 kHz, blocs 1 / 64 / 512 / 2048) | 1 200 000 événements, environ 551 000 notes par passage, 0 restante, 0 note-on doublé, 0 note-off orphelin |
| Latence (strum et humanize à 0) | 480 accords, toutes les notes au même échantillon que la touche |
| Strum 37,3 ms et 1/32 à 128 BPM, sur les 12 configurations | écart maximal 0,5 échantillon |
| Durée fixe 1/16 à 120 BPM | 6 000 échantillons exactement (touche courte ou longue) |
| Coupures (Stop, bypass, preset, état, désactivation, Panic) | 0 note restante, 0 note en attente partie ensuite |
| Sauvegarde / rechargement | 35 paramètres + 8 pads + accord appris identiques |
| CPU MIDI seul (bloc de 512 à 48 kHz, 8 voix + basse, strum, humanize, voice leading) | 2,1 µs en moyenne = 0,02 % d'un cœur (objectif < 0,5 %), 99e centile 7,6 µs |
| CPU avec la voix Preview | 260 µs = 2,4 % d'un cœur (information) |
| Allocations dans processBlock (3 000 blocs, Preview, Panic, bypass compris) | 0 |
| pluginval strict niveau 10, VST3 (Linux) | SUCCESS |

Le « pire » temps d'un bloc varie d'une mesure à l'autre (de 0,08 à 1,9 ms) : ce sont les interruptions de la machine
de test (2 cœurs partagés), pas le plugin ; le 99e centile reste stable autour de 7 µs.

---

## Phase 2 — Modes de jeu, gammes, suggestions, pads, export MIDI (7 octobre 2026)

**Fait**
- 4 modes (`Source/engine/Resolver.h`) : Gamme, Touches blanches (touches noires : Sus4, Sus2, 9e, mode parallèle ou muettes),
  Accord fixe (+ LEARN), Pads (8 notes à partir de C1 par défaut).
- LEARN : les touches enfoncées ensemble puis relâchées deviennent la forme « Appris » (intervalles depuis la note la plus basse,
  8 notes au maximum) ; le plugin passe en mode Accord fixe / Appris. Pendant l'écoute, on entend ce qu'on joue.
- Gammes conseillées par style (Deep / House, Hypnotic, Tech House, Mental).
- ANALYSER : poids de chaque note jouée (nombre + durée) et de la note la plus grave ; classement des couples
  tonique + gamme (part des notes dans la gamme, importance de la tonique, de sa quinte et de sa tierce, notes de la gamme jamais jouées).
- Enchaînements : tables de fonctions harmoniques (majeur, mineur) + couleur du style ; les 3 meilleurs degrés après l'accord joué.
- Générateur de pads : 2 progressions types du style (4 + 4 pads), en renversement Auto.
- Export MIDI 4 / 8 mesures (`Source/MidiExport.h`) : mêmes accords que le plugin, voice leading et strum compris ;
  glisser-déposer vers une piste d'Ableton, ou fichier dans Musique / Confluence.
- Interface de travail provisoire : tous les réglages, Learn, Analyser, gammes du style, enchaînements, Panic, les 8 pads,
  export et glisser-déposer. Elle sera remplacée en phase 3 par la maquette choisie.

**Mesures**

| Test | Résultat |
|---|---|
| Hors gamme (C#3 en do majeur) | plus proche Dm7, au-dessus Dm7, en dessous Cmaj7, ignorer : rien |
| Touches blanches en F# mineur | C..B = F#m7 G#m7b5 Amaj7 Bm7 C#m7 Dmaj7 E7 ; C# = F#sus4 / F#sus2 / F#m9 / F#maj7 (emprunt) / rien |
| Détection : 10 mélodies (mineur, majeur, dorien, phrygien, mixolydien, F# mineur, mineur harmonique, penta, phrygien dominant, lydien) | 10 / 10 au 1er rang |
| Analyse dans le plugin (ré dorien joué) | D Dorien au 1er rang, 16 notes comptées |
| Voice leading Auto contre renversement 0 (demi-tons par voix et par changement) | 0,75 / 4,89 ; 1,23 / 6,89 ; 0,90 / 7,29 ; 0,86 / 4,00 ; 1,62 / 5,05 |
| Learn (C E G Bb) | forme 0 4 7 10, puis D -> D F# A C |
| Export MIDI relu (4 et 8 mesures) | 8 accords identiques à ceux joués dans le plugin, longueur exacte |

**Livraison sur le Mac (7 octobre 2026)**
- Transfert par un dossier de staging neuf (`.livraison/<date>/`) : archive vérifiée par SHA-256, chaque fichier comparé
  octet par octet (cmp) au fichier envoyé avant la compilation : 26 / 26 identiques.
- 1re compilation (v1.0.1) : échec sur macOS, `major` (la gamme majeure) entrait en conflit avec la fonction système
  `major()` de `sys/types.h`, absente sous Linux. Identifiant renommé `majorScale`.
- v1.0.2 : `Built target Confluence_VST3`, installé dans `~/Library/Audio/Plug-Ins/VST3`, **pluginval strict 10 sur le Mac : SUCCESS**.
  4 avertissements : 2 viennent de JUCE / du SDK Apple, 2 d'un nom de membre de l'interface provisoire (sans effet, elle sera remplacée en phase 3).

---

## Phase 3 — Maquettes (7 octobre 2026)

Trois dispositions rendues avec les vrais composants (`tests/Mockups.h`, captures dans `maquettes/`), en attente de ton choix :
- **A « Lagon »** (900 x 520) : la disposition d'Undertow. Écran de l'accord au-dessus du clavier, sur la même largeur :
  les bulles montent des touches jusqu'au nom de l'accord ; petit tourbillon en coin ; onglets à droite ; pads en 4 x 2.
- **B « Tourbillon »** (900 x 560) : le cercle des quintes au centre, l'accord dans l'œil, les enchaînements conseillés
  en courants pointillés avec leur degré ; gamme et gammes du style à gauche, onglets à droite ; pads en une rangée de 8
  au-dessus d'un clavier de 4 octaves.
- **C « Courant »** (1000 x 400) : bande large et basse comme un device d'Ableton ; réglages de l'onglet sur une ligne,
  mer au centre (le nom de l'accord flotte, le clavier au fond), pads en 2 x 4 à droite, tourbillon sous la gamme.

---

## Phase 3 — Interface définitive, disposition C « Courant » (7 octobre 2026)

**Fait**
- Fenêtre 1000 x 470, redimensionnable en gardant ses proportions (de 70 % à 200 %), nette sur Retina (chaque image en cache
  est calculée à l'échelle réelle de l'écran).
- En haut : navigateur de presets (banque, flèches, menu par banque, SAVE avec panneau de nom, dossier, dé), PANIC, version.
- Colonne gauche : mode, tonique, gamme, style ; 3 pilules « gammes du style » ou « détection » (ANALYSER → STOP → GAMMES) ;
  tourbillon des enchaînements (cercle des quintes, accord joué en corail, 3 degrés conseillés en courants pointillés
  avec leur chiffrage ; pas de conseil en mode Accord fixe, l'accord pouvant sortir de la gamme).
- Onglets CHORD / VOICING / PLAY sur une ligne (VOICING montre aussi la place des notes de l'accord sur 4 octaves).
- La mer : nom de l'accord et degré en surface, notes en bulles (la basse en corail), clavier C1-C5 sous l'eau
  (gamme éclairée, tonique entourée, notes jouées en lumière, touche tenue marquée d'un point), jouable à la souris
  (velocity selon la hauteur du clic).
- Pads 2 x 4 (pad 1 en bas à gauche comme sur Push) : clic = joue, clic droit = degré / type / renversement / octave ;
  le pad joué se remplit et ondule. GÉNÉRER, pilule MIDI (glisser vers Ableton, menu d'export).
- Relecture des captures : logo « confluence CHORDS » (le mot était coupé en deux), libellés des menus qui ne sont plus tronqués
  (« Plus proche »), « Note pad 1 » au lieu de « Pads : 1re », velocity « Touche » au lieu de « Suit la touche ».

**Optimisation du dessin** (95e centile de l'image pleine : 17,5 ms au premier essai, au-dessus de l'objectif de 16 ms)
- Eau du fond calculée au quart de la résolution puis agrandie (dégradés très doux, aucune différence visible),
  particules à pleine résolution ; panneaux et textes du fond dans un calque fixe redessiné seulement quand il change.
- Pads au repos, nœuds du tourbillon, nom de l'accord, touches blanches et touches noires éteintes : en images,
  recalculées seulement quand elles changent ; pad joué : seule l'eau est dessinée en direct.
- Dégradés réutilisés (`Ui::setGradient`) : `Graphics::setGradientFill` alloue un dégradé à chaque appel.
- Contour en pointillés de la pilule MIDI calculé une fois par taille.

**Banc de tests renforcé**
- Le compteur d'allocations intercepte maintenant malloc / calloc / realloc (les tableaux de JUCE n'utilisent pas `new`) et ne
  compte que le thread mesuré. processBlock : toujours 0 allocation sur 3 000 blocs.
- Nouveau test : allocations de notre code de dessin, mesurées avec un contexte graphique qui ne dessine rien.

**Mesures** (rendu logiciel de JUCE sur la machine de test, 1x ; le Mac dessine avec CoreGraphics)

| Test | Résultat |
|---|---|
| Captures des 3 onglets à 1x et 2x, pad joué, panneau SAVE, 150 % | `build-linux/captures/`, relues une par une |
| Image pleine (fond recalculé, 8 fois/s en jeu) | moyenne 9,0 ms, 95e centile 14,6 ms (objectif < 16 ms) |
| Image partielle (mer, bulles, tourbillon, pad joué ; 52 fois/s) | moyenne 3,0 ms, 95e centile 5,0 ms |
| Allocations de notre code pendant l'animation | 0 sur 73 images ; seules les images où un nouvel accord s'affiche remettent le nom et le tourbillon en page (JUCE alloue pour mettre un texte en page) |
| Allocations du moteur de rendu de JUCE (tables de bords, dégradés) | ~3 000 par image, hors de notre code |
| Charge estimée de l'interface | ~23 % d'un cœur en jeu, ~3,6 % au repos (fond à 4 images/s, rien d'autre) |
| Clavier et pads à la souris | l'accord sonne par Confluence et se relâche au lâcher |

---

## Phase 4 — Presets : les 6 premiers (7 octobre 2026)

**Fait**
- Système de presets (`Source/PresetManager.*`) : 3 banques d'usine, presets utilisateur `.cfpreset`, dé (réglages au hasard
  dans des plages musicales + pads regénérés). Changer de preset coupe toutes les notes.
- 2 presets par banque, chacun avec ses 8 pads et une phrase de démonstration de 4 mesures :
  - DEEP & HOUSE : **Warm Current** (m9 en ré dorien, ouvert, strum 15 ms ; phrase i-IV-VII-v-III-IV-i),
    **Lagoon Keys** (fa majeur, 7e, basse à l'octave, voice leading ; ii-V-I-vi).
  - HYPNOTIC & DUB : **Dub Chamber** (do mineur, m7 serrés dans le médium, 1/8 pointée, velocity fixe ; stabs dub),
    **Fog Bank** (la mineur en Touches blanches, sus2 / sus4 sur les touches noires, strum 30 ms).
  - TECH HOUSE & STABS : **Splash Stab** (fa mineur, m7 serrés, 1/16, velocity en courbe),
    **Rave Tide** (accord fixe m7 façon rave, 1/8, velocity 110).
- Rendus (`build-linux/renders/presets/`) : la phrase est jouée deux fois dans le plugin au tempo de la banque, on garde
  la seconde passe pour que la fin retombe sur le début (boucle sans coupure). Le `.mid` est la sortie MIDI du plugin
  (ce que reçoit ton synthé), le WAV la voix Preview, ramené à -18 LUFS pour comparer les presets au même volume.

**Mesures**

| Preset | BPM | Notes jouées → notes MIDI sorties | Voix Preview (réglage -12 dB) | WAV livré |
|---|---|---|---|---|
| Warm Current | 122 | 7 → 35 | pic -14,0 dBFS, -25,8 LUFS | pic -6,2 dBFS, -18 LUFS |
| Lagoon Keys | 120 | 4 → 20 | pic -13,9 dBFS, -27,2 LUFS | pic -4,7 dBFS, -18 LUFS |
| Dub Chamber | 128 | 16 → 64 | pic -17,1 dBFS, -26,6 LUFS | pic -8,5 dBFS, -18 LUFS |
| Fog Bank | 130 | 6 → 24 | pic -18,4 dBFS, -27,1 LUFS | pic -9,3 dBFS, -18 LUFS |
| Splash Stab | 126 | 18 → 72 | pic -16,8 dBFS, -27,7 LUFS | pic -7,0 dBFS, -18 LUFS |
| Rave Tide | 128 | 19 → 76 | pic -16,4 dBFS, -26,0 LUFS | pic -8,4 dBFS, -18 LUFS |

Toutes les notes sorties sont refermées dans les 4 mesures ; aucun NaN ; pic < -1 dBFS partout.
Les 18 autres presets attendent ton écoute de ces 6.

**Validation** : 93 tests OK (`tests/results-phase3-4.md`), pluginval strict niveau 10 (Linux) : SUCCESS.

**Livraison sur le Mac (7 octobre 2026)**
- Dossier de staging neuf `.livraison/20261007-234859/` : archive vérifiée par SHA-256, 65 fichiers relus depuis le Mac et
  comparés octet par octet (cmp) au manifeste : 65 / 65 identiques. Captures dans `captures/`, rendus dans `rendus/presets/`.
  Les 2 captures de l'interface provisoire ont été rangées dans `.livraison/20261007-234859/ancien/` (rien n'est effacé).
- **v1.0.3** : `Built target Confluence_VST3`, 0 erreur, installé dans `~/Library/Audio/Plug-Ins/VST3`,
  **pluginval strict 10 sur le Mac : SUCCESS**. 2 avertissements : un du SDK Apple, un champ inutilisé de l'éditeur
  (supprimé ensuite dans les sources, compilé à la prochaine livraison).

---

## Sécurité : presets et projets piégés (14 octobre 2026)

- Nouveau test `hostile` : 12 fichiers corrompus ou piégés (octets au hasard, fichier vide, XML d'un autre plugin, XML tronqué,
  valeurs nan / inf / 1e30 / texte / entiers extrêmes), chargés comme preset utilisateur ET comme état de projet Ableton.
- Avant : un preset contenant `value="nan"` faisait entrer 40 valeurs invalides (NaN) dans les paramètres.
- Correction : à chaque chargement, chaque valeur est vérifiée (NaN ou infini -> valeur par défaut, sinon ramenée dans sa plage).
- Après : 24 chargements, 0 paramètre invalide, 0 message MIDI invalide, 0 note restée, 0 bloc audio NaN. 94 tests OK.
- Pas encore livré sur le Mac (partira avec la prochaine livraison).

---

## Publication en logiciel libre (14 octobre 2026)

Décision : Confluence est publié en open source, sous **GNU AGPL v3 ou ultérieure** (l'option open source de JUCE 8 ;
la licence gratuite fermée de JUCE et sa limite de revenus ne s'appliquent donc pas).

**Fait**
- `LICENSE` : texte officiel actuel de l'AGPL v3 publié par gnu.org (661 lignes).
- `THIRD-PARTY-NOTICES.md` : JUCE 8.0.12 (AGPLv3), SDK VST 3.8.0 (MIT), HarfBuzz, SheenBidi (Apache 2.0), zlib,
  libpng, libjpeg (IJG), extensions PreSonus ; marques (VST, Ableton, Apple).
- En-tête `SPDX-License-Identifier: AGPL-3.0-or-later` + copyright dans les 31 fichiers sources, CMake et scripts.
- JUCE 8.0.12 partout (comme sur le Mac ; le SDK VST3 livré avec JUCE 8.0.10 était encore sous l'ancienne licence
  Steinberg). Décodeurs FLAC et Ogg Vorbis désactivés : moins de code tiers embarqué.
- Bouton **ⓘ** et panneau « À propos » : les mentions qu'une interface doit afficher sous l'AGPL (copyright, absence de
  garantie, droit de redistribuer, où lire la licence), code source, confidentialité, composants tiers, marques.
  Le panneau est une image fixe (aucun redessin pendant l'animation). Adresse du dépôt : variable CMake `CF_SOURCE_URL`.
- `README.md` (FR + résumé EN), `SECURITY.md`, `CONTRIBUTING.md` (DCO), `CHANGELOG.md`, `packaging/LISEZMOI.txt`,
  `.gitignore`, `.gitattributes` ; guides privés (non publiés) dans `prive/` : `PUBLIER.md`, `mentions-legales-modele.md`.
- `release.command` : version universelle (arm64 + x86_64) avec JUCE 8.0.12, installeur `.pkg` vers
  `/Library/Audio/Plug-Ins/VST3`, signature Developer ID et notarisation si configurées (sinon signature ad hoc),
  archive du code source exact, `SHA256SUMS.txt`. N'installe rien sur le Mac de développement.

**Relecture indépendante** (un second agent, sans avoir vu le travail se faire) puis corrections :
- pseudonyme protégé : guides privés sortis du dépôt (`prive/`, ignoré par git), identité git « Jasperr » + adresse
  noreply dans le guide, avertissement sur le nom légal affiché par un certificat Developer ID ;
- code source complet (AGPL art. 6) : `release.command` refuse un dépôt avec des modifications non enregistrées,
  archive exactement le commit compilé (`git archive`), télécharge JUCE 8.0.12 tel quel (commit vérifié) et publie
  aussi son code source ; dossier de compilation neuf à chaque version ;
- version : `.build_number` est suivi par git et lu par CMake ; une version publiée garde le numéro écouté dans Ableton ;
- termes additionnels de l'article 7 dans un fichier `NOTICE` (autre nom pour une version modifiée distribuée,
  aucun droit de marque), au lieu d'une phrase qui réservait aussi les « visuels » (non permis par l'AGPL) ;
- `THIRD-PARTY-NOTICES.md` : licences ISC (`hb-ucd`) et MIT (`fasthash`) de HarfBuzz ajoutées, texte zlib exact de
  JUCE 8.0.12, JUCE en AGPL v3 « seule » ;
- panneau « À propos » : phrases inexactes corrigées (où lire la licence, archive source), « dans les limites permises
  par la loi », logo VST Compatible officiel (règles d'usage du SDK VST 3.8), « VST® » à la première mention partout ;
- `release.command` : textes légaux copiés DANS le plugin avant la signature, binaire universel vérifié, paquet qui
  n'installe que `Confluence.vst3`, notarisation seulement avec plugin et installeur signés, pluginval arm64 et Intel ;
- `LISEZMOI` : bon chemin Ableton (Plug-Ins > VST3 > Jasperr, dossiers système VST3), macOS 11-12, chemin des presets,
  version et adresse remplies automatiquement ; `ROUTAGE-ABLETON.md` utilisable par un utilisateur final.

**Mesures** : 95 tests OK avec JUCE 8.0.12 (dont « Panneau À propos » et « Fichiers piégés »), pluginval strict 10 :
SUCCESS ; `release.command` : `bash -n` et shellcheck sans remarque.

---

## GRVTY Records et version Windows (14 octobre 2026)

**GRVTY Records**
- Fabricant affiché dans Ableton : « GRVTY Records » (Plug-Ins > VST3 > GRVTY Records). Les codes internes
  (`Jspr` / `Cnfl`) ne changent pas : les projets Ableton existants retrouvent le plugin.
- « Une création du label et collectif GRVTY Records, développée par Jasperr » : en-tête du plugin (sous le logo),
  panneau ⓘ, `NOTICE`, README, installeurs Mac et Windows, en-têtes des sources (© GRVTY Records).
- `NOTICE` rédigé pour rester juste tant que GRVTY Records n'est pas une personne morale (droits exercés par l'auteur
  qui publie sous ce nom) et après une éventuelle cession à une association (`prive/PUBLIER.md`, point 2).
- Presets personnels : Mac `~/Library/Audio/Presets/GRVTY Records/Confluence` (copie unique des anciens presets du
  dossier `Jasperr/Confluence`, rien n'est effacé), Windows `Documents\GRVTY Records\Confluence\Presets`.

**Windows**
- Le compilateur de Microsoft n'est pas disponible ici (et JUCE 8 refuse MinGW) : la version Windows est fabriquée
  par la compilation automatique GitHub (`windows-latest`, Visual Studio) ou sur un PC Windows avec
  `packaging/windows/build-windows.sh` (Git Bash, CMake, Visual Studio, NSIS) : VST3 64 bits, pluginval strict 10,
  installeur `Confluence-v<version>-Windows-setup.exe`.
- Runtime C++ intégré au plugin (`CMAKE_MSVC_RUNTIME_LIBRARY` statique) : aucune DLL à installer, pas de conflit avec
  le runtime déjà chargé par l'hôte. Sources compilées en UTF-8 (`/utf-8`).
- Installeur NSIS : plugin dans `C:\Program Files\Common Files\VST3`, textes et désinstalleur dans
  `C:\Program Files\GRVTY Records\Confluence`, entrée « Applications installées ». Testé sous Wine (version
  64 bits du programme d'installation) : installation, clé de désinstallation, désinstallation propre.
- `.github/workflows/build.yml` : contrôle du tag, banc de tests Linux + pluginval, Windows (.exe), Mac
  (`release.command`), Release brouillon avec tous les fichiers et le logo VST ; pluginval 1.0.4 épinglé.

**Relecture indépendante** (3e passage) : chemins Windows passés en forme `D:/...` (sinon la compilation Windows
échouait), runtime C++ statique, `NOTICE` et droits du label, `SECURITY.md` (copie des anciens presets), guide privé
(association à 2 fondateurs, cession écrite plugin par plugin, dépôt de marque qui publie le nom du déposant).

**Mesure** : la machine de test de cette session est environ 40 % plus lente que la précédente ; l'image pleine
passait à 19 ms (95e centile). L'agrandissement de l'eau du fond par le moteur logiciel de JUCE coûtait 5,7 ms :
sur Linux, il est maintenant fait directement en deux passes (1,7 ms). Image pleine : 9,0 ms en moyenne,
95e centile 12,7 ms. Mac et Windows gardent leur propre moteur (CoreGraphics, Direct2D).
95 tests OK, pluginval strict 10 (Linux) : SUCCESS.

**Sur le Mac (v1.0.4)**
- `build.command` : TERMINE, aucune erreur (un avertissement du SDK d'Apple, `wstring_convert`), Built target,
  pluginval SUCCESS ; Confluence installé apparaît chez « GRVTY Records » dans Ableton. Livraison : 85/85 fichiers
  identiques (cmp).
- `release.command`, 1er essai : ÉCHEC à la fabrication du paquet. Sur ce macOS, `pkgbuild --analyze` ne met pas la
  clé `BundleIsRelocatable` dans sa liste, et `PlistBuddy Set` ne crée pas une clé absente. Corrigé : les clés
  `BundleIsRelocatable` (false), `BundleIsVersionChecked` (false) et `BundleOverwriteAction` (upgrade) sont
  supprimées puis ajoutées ; script `preinstall` qui retire l'ancien `Confluence.vst3` avant l'installation.
- 2e essai : TERMINE. pluginval arm64 et x86_64 (Rosetta) : SUCCESS. `dist/` : `Confluence-v1.0.4-macOS.zip`
  (installeur 3,7 Mo + LICENSE, NOTICE, THIRD-PARTY-NOTICES, LISEZMOI, ROUTAGE-ABLETON, BUILD-INFO),
  `Confluence-v1.0.4-source.zip`, `JUCE-8.0.12-source.zip`, `SHA256SUMS.txt` (3/3 OK).
- Paquet ouvert et vérifié ici : non relocalisable, `preinstall` présent, contenu = `Confluence.vst3` seul, textes
  légaux dans `Contents/Resources`, NOTICE au nom de GRVTY Records. Le contenu portait aussi l'attribut
  `com.apple.provenance` posé par macOS (sans effet) : `release.command` efface maintenant les attributs étendus
  avant `pkgbuild` (pris en compte au prochain lancement).
- Dossier commun `Slink/Installeurs` : `Confluence-1.0.4-Installeur.pkg` (copie de l'installeur ci-dessus,
  cmp identique) ; l'ancien 1.0.3 rangé dans `anciennes versions/`. `make_installer.sh` lit maintenant le fabricant
  dans le CMakeLists de chaque plugin (Confluence : GRVTY Records, les autres : Jasperr) et met LICENSE / NOTICE /
  THIRD-PARTY-NOTICES dans le plugin quand le projet en a ; « Lisez-moi » mis à jour (GRVTY Records, Mac uniquement).

---

## Reste à faire
- Publication : choisir le nom (Confluence est une marque d'Atlassian), créer le dépôt GitHub, éventuellement le
  compte Apple Developer (voir `prive/PUBLIER.md`).
- Installeur Windows `.exe` : fabriqué par la compilation automatique GitHub dès que le dépôt existe (ou sur un PC
  Windows avec `packaging/windows/build-windows.sh`).
- Flow Filter, Undertow et Ebb : toujours au nom de Jasperr, sans version Windows (non modifiés ici).
- Phase 4 : les 18 autres presets, d'après tes retours sur ces 6.
- Phase 5 : boucles d'amélioration (objectif à définir).
