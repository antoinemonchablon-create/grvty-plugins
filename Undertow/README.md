# Undertow

Synthé FM monophonique à 2 oscillateurs avec séquenceur intégré (VST3 / AU), pour les boucles
mélodiques de techno hypnotique, hardgroove et mentale. Même univers visuel que Flow Filter.

## Compiler sur Mac

Double-clique sur `build.command` (ou `Compiler Undertow.terminal`). Le script :
1. trouve Xcode, CMake et JUCE 8 (`~/Documents/JUCE`) ;
2. incrémente le numéro de build (version affichée en haut à droite du plugin) ;
3. supprime l'ancienne version installée, compile le VST3 et l'AU, les installe ;
4. force macOS à relire la liste des Audio Units (`killall -9 AudioComponentRegistrar`) ;
5. valide les deux plugins avec pluginval (strict, niveau 10).

Tout est écrit dans `build.log` : chercher `TERMINE`, `Built target` et `PLUGINVAL ... SUCCESS`.

## Utilisation rapide

- **Mode SEQ** : lance Play dans Ableton, le séquenceur joue calé sur le tempo. Une note MIDI transpose la séquence (C3 = pas de transposition).
- **Mode KEYS** : le synthé se joue directement au clavier.
- Grille : clic = allumer/éteindre un pas, glisser vers le haut/bas = hauteur, Maj+clic = accent, Alt+clic = slide,
  clic droit = ratchet, probabilité, gate, vélocité, p-locks.
- Rangée du bas de la grille = piste Mod (glisser pour dessiner).
- Écran de gauche (LIVE) : le signal réel qui sort du plugin. ONDE = 2 cycles de la note jouée, MESURE = la dernière mesure.
  En fond, la forme théorique. Glisser verticalement = Index, horizontalement = Ratio ; double-clic = Index par défaut.
- Presets : 49 presets d'usine en 4 banques (Hardgroove 13, Hypnotic 13, Mental 19, Abyss 4 ;
  types PERC / BASS / SYNTH / FX / BLEEP / AMBIENT), flèches, menu avec un sous-menu par banque,
  SAVE (son + motif, panneau dans la fenêtre du plugin), bouton dossier (ouvre le dossier des presets dans le Finder),
  dé = son au hasard (le motif est gardé).
  Presets utilisateur : ~/Library/Audio/Presets/GRVTY Records/Undertow/*.utpreset (menu > MES PRESETS ;
  les anciens presets du dossier Jasperr y sont recopiés une fois).

## Fichiers

| Fichier | Rôle |
|---|---|
| `Source/Params.h` | Tous les paramètres (IDs figés) |
| `Source/dsp/FmVoice.h` | Voix FM : oscillateurs, FM, filtre, enveloppes |
| `Source/dsp/Envelopes.h` | ADSR d'ampli, enveloppe Mod AD à courbe |
| `Source/dsp/Modulators.h` | LFO, filtre à variables d'état |
| `Source/dsp/StereoFx.h` | Delay ping-pong, reverb |
| `Source/seq/Pattern.h` | Motif, gammes, générateurs, Mutate, sauvegarde |
| `Source/seq/Sequencer.h` | Lecture calée sur le transport d'Ableton |
| `Source/ui/LiquidUI.*` | Look « eau » repris de Flow Filter |
| `Source/PluginProcessor.*` | MIDI, transport, suréchantillonnage, effets, état |
| `Source/PluginEditor.*` | Interface (maquette A) : mise en page, fond en cache, redessin sélectif |
| `Source/ui/FmScreen.*` | Écran FM (forme d'onde réelle, gouttes et ondes) |
| `Source/ui/SeqGrid.*` | Grille d'eau du séquenceur, vague de lecture, piste Mod |
| `Source/ui/PresetBar.*` | Barre de presets (banques, SAVE, dé) |
| `Source/PresetManager.h` | Presets d'usine / utilisateur, Random |
| `Source/presets/FactoryPresets.h` | Les presets d'usine (son + motif en texte) |
| `tests/UndertowTests.cpp` | Banc de tests (audio, clics, aliasing, séquenceur, état, perf, captures) |

## Banc de tests (Linux)

```bash
cmake -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DJUCE_PATH=~/JUCE -DUT_BUILD_TESTS=ON
ninja -C build-linux UndertowTests
./build-linux/UndertowTests_artefacts/Release/UndertowTests          # tout
./build-linux/UndertowTests_artefacts/Release/UndertowTests clicks   # un seul test : safety, clicks, alias, seq, state, perf, presets, snap
./build-linux/UndertowTests_artefacts/Release/UndertowTests presets  # + rendus WAV de 4 mesures dans renders/
./build-linux/UndertowTests_artefacts/Release/UndertowTests calibrate # volume de chaque preset et gain à mettre
```
