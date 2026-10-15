# Flow Filter

Banc de 32 filtres passe-bande dont le gain est animé par une vague "aquatique" de 16 nœuds.
Bandes paires → canal gauche, impaires → canal droit (compatible mono).

## Compiler sur Mac

Prérequis : Xcode + CMake (`brew install cmake`).

```bash
cd ~/Documents/Slink/FlowFilter
cmake -B build -G Xcode -DJUCE_PATH=~/JUCE     # remplace ~/JUCE par ton dossier JUCE
cmake --build build --config Release
```

Sans `-DJUCE_PATH`, JUCE est téléchargé automatiquement.
Le VST3 et l'AU sont copiés tout seuls dans `~/Library/Audio/Plug-Ins/`.
Dans Ableton : Préférences > Plug-ins > « Rescan ».

Pour travailler dans Xcode : `open build/FlowFilter.xcodeproj`.

## Fichiers

| Fichier | Rôle |
|---|---|
| `Source/WaveEngine.h` | Moteur de vague (Multiply, Ripple, Scan, Offset, Invert, Gravity) |
| `Source/FilterBank.h` | 32 filtres SVF, répartition L/R, lissage (Release), compensation de gain |
| `Source/ModLfo.h` | LFO interne (sinus, triangle, rampe, carré, aléatoire) |
| `Source/PluginProcessor.*` | Paramètres, synchro tempo, traitement audio |
| `Source/PluginEditor.*` | Interface : visualiseur des bandes + contrôles |
