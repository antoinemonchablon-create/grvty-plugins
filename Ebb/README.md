# Ebb

Sidechain « volume shaper » (VST3 / AU) : une courbe de volume calée sur le tempo fait pomper une basse, un pad ou un bus,
sans aucun routage. Le niveau ne monte jamais : à Amount 0 %, le son ressort intact.
12 courbes d'usine, ou ta propre courbe dessinée dans l'éditeur. Tension, Lissage, et multibande pour ne faire pomper
que les graves (ou que les aigus).
Même univers visuel et même chaîne de compilation que Flow Filter, Undertow et Confluence.

**Pour le mode AUDIO (kick en sidechain) et le mode MIDI : voir [ROUTAGE-ABLETON.md](ROUTAGE-ABLETON.md).**

## Compiler sur Mac

Double-clique sur `Compiler Ebb.terminal` (ou `build.command`). Le script :
1. trouve Xcode, CMake et JUCE 8 (`~/Documents/JUCE`) ;
2. incrémente le numéro de build (version affichée en haut à droite du plugin) ;
3. supprime l'ancienne version installée, compile le VST3 et l'AU, les installe dans `~/Library/Audio/Plug-Ins`,
   et force macOS à relire la liste des Audio Units ;
4. valide les deux avec pluginval (strict, niveau 10).

Tout est écrit dans `build.log` : chercher `TERMINE`, `Built target` et `PLUGINVAL ... SUCCESS`.

## Réglages

| Réglage | Rôle |
|---|---|
| **Amount** | Profondeur de la baisse, 0 à 100 %. À 100 %, le volume tombe à zéro au départ de la courbe. |
| **Courbe** | 12 formes, de la plus sèche à la plus douce : Splash, Snap, Ripple (la pompe classique, par défaut), Current, Ledge (palier), Twin (double pompe), Gate, Swell, Drift, Breath (respiration), Surge (rampe), Backwash (inverse). La 13e vignette, **MA COURBE**, est ta courbe dessinée. |
| **Tension** | -100 à +100 %. + : plus sèche (le son revient plus vite) ; - : plus molle (il reste bas plus longtemps). Marche sur toutes les courbes. |
| **Lissage** | 0 à 100 %. Arrondit les angles et les marches de la courbe (jusqu'à 8 % de sa durée). La courbe est lissée comme une boucle : à fort lissage, la baisse commence un peu avant le temps. |
| **Durée** | 1/16, 1/8, 1/4 triolet, 1/8 pointé, 1/4 (défaut), 1/2, 1 mesure. Les courbes repartent à chaque mesure. |
| **Déclenchement** | SYNC (tempo d'Ableton), MIDI (chaque note), AUDIO (le kick du sidechain). |
| **Nudge** | SYNC : décale la courbe de -20 à +20 ms. |
| **Seuil** | AUDIO : niveau des graves du sidechain qui relance la courbe (-60 à 0 dB, -20 dB par défaut). |
| **Multibande** | PLEIN : tout le son pompe. GRAVES : seulement sous la fréquence (le sub s'efface sous le kick, les aigus restent). AIGUS : seulement au-dessus. Séparation Linkwitz-Riley 24 dB/oct, de 40 Hz à 2 kHz (150 Hz par défaut). Quand rien ne pompe, le niveau reste le même à toutes les fréquences. |

Anti-clic : le gain met toujours au moins 1 ms pour passer de 100 % à 0 %, y compris quand la courbe repart en plein
milieu, quand on change de courbe, de durée, d'Amount, de Tension, de Lissage, de courbe perso, ou quand Ableton saute
dans une boucle. Les changements de bande se font en fondu (40 ms). Latence nulle.

## Dessiner sa courbe

Clique sur **ÉDITER** : la courbe que tu entends devient ta courbe perso (MA COURBE), à pleine échelle (en haut = plein
volume). En fin trait blanc : ce que tu entends vraiment (avec Amount, Tension et Lissage).

| Geste | Effet |
|---|---|
| Clic dans le vide | Ajoute un point (et le déplace tant que tu tiens le bouton) |
| Glisser un point | Le déplace, entre ses voisins. Les points carrés (début et fin) ne bougent qu'en hauteur |
| Glisser une poignée (losange au milieu d'un segment) vers le haut ou le bas | Bombe ou creuse le segment |
| Double-clic (ou clic droit) sur un point | Le supprime |
| Double-clic sur une poignée | Segment droit |
| Alt (ou Cmd) pendant le geste | Sans aimant |

- **GRILLE** : aimant sur 4, 8, 16 ou 32 divisions de la courbe (et niveaux par 1/16), ou sans grille (—).
- **Annuler / rétablir** : chaque geste est une étape (100 étapes).
- **MIROIR** : la courbe à l'envers dans le temps. **INVERSER** : de haut en bas. **EFFACER** : repart d'une pompe simple.
- En édition, un clic sur une courbe d'usine la copie dans ta courbe, pour la retoucher. Hors édition, il choisit la courbe d'usine ;
  MA COURBE revient à ta courbe.
- **ENREGISTRER** : range ta courbe dans `Bibliothèque / Audio / Presets / GRVTY Records / Ebb / Courbes` (fichier `.ebbshape`).
  **MES COURBES** : les recharger, copier une courbe d'usine, ouvrir le dossier.
- Ta courbe est enregistrée avec le projet Ableton.

## Fichiers

| Fichier | Rôle |
|---|---|
| `Source/Params.h` | Les 11 paramètres (6 figés depuis la phase 1, 5 ajoutés en phase 2) |
| `Source/dsp/Curves.h` | Les 12 courbes (formules + tables de 8192 points) et les durées |
| `Source/dsp/Shape.h` | Courbe perso (points, courbures, rendu en table, conversion), Tension, Lissage |
| `Source/dsp/Crossover.h` | Séparation graves / aigus (Linkwitz-Riley 24 dB/oct) |
| `Source/dsp/PumpEngine.h` | Moteur : synchro tempo, déclenchements MIDI et AUDIO, détecteur de kick, anti-clic |
| `Source/dsp/TripleBuffer.h` | Échange sans verrou interface -> audio (courbe perso) |
| `Source/ShapeLibrary.h` | Mes courbes : texte et fichiers `.ebbshape` |
| `Source/PluginProcessor.*` | Bus (principal + sidechain), position d'Ableton, MIDI, multibande, courbe perso, annuler, état |
| `Source/PluginEditor.*` | Interface (maquette A « Houle ») |
| `Source/ui/CurveView.*` | La vague, la goutte, le niveau de l'eau, et l'éditeur de courbe |
| `Source/ui/CurveStrip.*` | Les vignettes des courbes et le panneau « Enregistrer ma courbe » |
| `Source/ui/Controls.*` | Pilules, boutons à icône, menus |
| `Source/ui/LiquidUI.*` | Look « eau » repris de Flow Filter, Undertow et Confluence |
| `tests/EbbTests.cpp`, `tests/Phase2Tests.h` | Banc de tests |
| `tests/Mockups.h` | Maquettes d'interface (A, B, C) |

## Banc de tests (Linux)

```bash
cmake -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH=~/JUCE -DEB_BUILD_TESTS=ON
ninja -C build-linux EbbTests Ebb_VST3
cd build-linux && ./EbbTests_artefacts/Release/EbbTests          # tout
./EbbTests_artefacts/Release/EbbTests bands   # un seul groupe : ids, curves, shape, audio, zero, clicks, sync, transport,
                                              # midi, kick, sidechain, state, perf, alloc, shapes, custom, bands, state2,
                                              # edit, snap ; mockups (maquettes), uiprof (profil du dessin)
```
