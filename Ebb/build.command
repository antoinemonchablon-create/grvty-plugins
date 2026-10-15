#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 GRVTY Records — voir les fichiers LICENSE et NOTICE
# Compile Ebb (VST3 + AU), l'installe dans ~/Library/Audio/Plug-Ins, puis le valide avec pluginval (strict, niveau 10)
cd "$(dirname "$0")"
LOG="$(pwd)/build.log"
exec > >(tee "$LOG") 2>&1

echo "=== Ebb — une création GRVTY Records : compilation ($(date)) ==="

# 1. Compilateur
if ! xcode-select -p >/dev/null 2>&1; then
  echo "ERREUR: outils de developpement Apple absents. Lance : xcode-select --install"
  exit 1
fi
echo "Xcode : $(xcode-select -p)"

# 2. CMake
CMAKE="$(command -v cmake)"
for c in /opt/homebrew/bin/cmake /usr/local/bin/cmake /Applications/CMake.app/Contents/bin/cmake "$HOME/Library/Python/"*/bin/cmake; do
  [ -z "$CMAKE" ] && [ -x "$c" ] && CMAKE="$c"
done
if [ -z "$CMAKE" ]; then
  if [ -x /opt/homebrew/bin/brew ]; then
    echo "Installation de CMake via Homebrew..."
    /opt/homebrew/bin/brew install cmake && CMAKE=/opt/homebrew/bin/cmake
  else
    echo "Installation de CMake via pip (utilisateur)..."
    python3 -m pip install --user --quiet cmake
    for c in "$HOME/Library/Python/"*/bin/cmake; do [ -x "$c" ] && CMAKE="$c"; done
  fi
fi
[ -z "$CMAKE" ] && { echo "ERREUR: impossible d'installer CMake"; exit 1; }
echo "CMake : $CMAKE ($("$CMAKE" --version | head -1))"

# 3. JUCE (version 8 requise), sinon telechargement automatique
JUCE_ARG=""
for d in "$HOME/JUCE" "$HOME/Applications/JUCE" "/Applications/JUCE" "$HOME/Documents/JUCE" "$HOME/Downloads/JUCE" "$HOME/Desktop/JUCE"; do
  if [ -f "$d/CMakeLists.txt" ] && grep -q "project(JUCE VERSION 8" "$d/CMakeLists.txt"; then
    JUCE_ARG="-DJUCE_PATH=$d"; echo "JUCE trouve : $d"; break
  fi
done
[ -z "$JUCE_ARG" ] && echo "JUCE 8 non trouve sur le disque : telechargement automatique"

# 4. Numero de build : augmente a chaque compilation -> nouvelle version du plugin
FIRST_BUILD=0
[ -f .build_number ] || FIRST_BUILD=1
BUILD_NUMBER=$(( $(cat .build_number 2>/dev/null || echo 0) + 1 ))
echo "$BUILD_NUMBER" > .build_number
VERSION="1.$(( BUILD_NUMBER / 256 )).$(( BUILD_NUMBER % 256 ))"
echo "Version : $VERSION"

if pgrep -x "Live" >/dev/null; then
  echo "ATTENTION : Ableton Live est ouvert. Quitte-le puis relance-le apres la compilation pour charger la nouvelle version."
fi

# 5. On retire l'ancienne version installee (evite qu'Ableton / macOS gardent l'ancien code en cache)
VST3_DEST="$HOME/Library/Audio/Plug-Ins/VST3/Ebb.vst3"
AU_DEST="$HOME/Library/Audio/Plug-Ins/Components/Ebb.component"
rm -rf "$VST3_DEST" "$AU_DEST"

# 6. Configuration + compilation (Apple Silicon)
"$CMAKE" -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DEB_BUILD_NUMBER=$BUILD_NUMBER $JUCE_ARG || { echo "ERREUR configuration"; exit 1; }
"$CMAKE" --build build --config Release --target Ebb_VST3 Ebb_AU -j "$(sysctl -n hw.ncpu)" || { echo "ERREUR compilation"; exit 1; }

# 7. macOS garde une liste en cache des Audio Units : on la force a se reconstruire
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "=== Plugins installes (version $VERSION) ==="
ls -d "$VST3_DEST" "$AU_DEST" 2>/dev/null
/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$VST3_DEST/Contents/Info.plist" 2>/dev/null

# 8. Validation pluginval (strict, niveau 10) du VST3 et de l'AU
PV=""
for p in "$(pwd)/tools/pluginval.app/Contents/MacOS/pluginval" "$(pwd)/../Undertow/tools/pluginval.app/Contents/MacOS/pluginval"; do
  [ -z "$PV" ] && [ -x "$p" ] && PV="$p"
done
if [ -z "$PV" ]; then
  echo "Telechargement de pluginval..."
  mkdir -p tools
  curl -sSL -o tools/pluginval.zip https://github.com/Tracktion/pluginval/releases/latest/download/pluginval_macOS.zip \
    && (cd tools && unzip -o -q pluginval.zip)
  [ -x "$(pwd)/tools/pluginval.app/Contents/MacOS/pluginval" ] && PV="$(pwd)/tools/pluginval.app/Contents/MacOS/pluginval"
fi
if [ -n "$PV" ]; then
  sleep 3
  for P in "$VST3_DEST" "$AU_DEST"; do
    NAME="$(basename "$P")"
    if "$PV" --strictness-level 10 --timeout-ms 600000 --validate "$P" > "pluginval-$NAME.log" 2>&1; then
      echo "PLUGINVAL $NAME : SUCCESS"
    else
      echo "PLUGINVAL $NAME : ECHEC (voir pluginval-$NAME.log)"
      grep -E "FAILED|\*\*\*" "pluginval-$NAME.log" | head -20
    fi
  done
else
  echo "PLUGINVAL : non disponible (pas de connexion ?), validation sautee"
fi

if [ "$FIRST_BUILD" = "1" ]; then
  echo ""
  echo "PREMIERE COMPILATION : dans Ableton, Preferences > Plug-Ins > Rescan (ou maintenir Alt en cliquant sur Rescan)"
  echo "pour que Ebb apparaisse dans Plug-Ins > GRVTY Records."
fi

echo "=== TERMINE : quitte et relance Ableton, la version $VERSION s'affiche en haut a droite du plugin ==="
