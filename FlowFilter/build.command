#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 GRVTY Records — voir les fichiers LICENSE et NOTICE
# Compile Flow Filter (VST3 + AU) et l'installe dans ~/Library/Audio/Plug-Ins
cd "$(dirname "$0")"
LOG="$(pwd)/build.log"
exec > >(tee "$LOG") 2>&1

echo "=== Flow Filter — une création GRVTY Records : compilation ($(date)) ==="

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
BUILD_NUMBER=$(( $(cat .build_number 2>/dev/null || echo 0) + 1 ))
echo "$BUILD_NUMBER" > .build_number
VERSION="1.$(( BUILD_NUMBER / 256 )).$(( BUILD_NUMBER % 256 ))"
echo "Version : $VERSION"

if pgrep -x "Live" >/dev/null; then
  echo "ATTENTION : Ableton Live est ouvert. Quitte-le puis relance-le apres la compilation pour charger la nouvelle version."
fi

# 5. On retire l'ancienne version installee (evite qu'Ableton / macOS gardent l'ancien code en cache)
rm -rf "$HOME/Library/Audio/Plug-Ins/VST3/Flow Filter.vst3" "$HOME/Library/Audio/Plug-Ins/Components/Flow Filter.component"

# 6. Configuration + compilation (Apple Silicon)
"$CMAKE" -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64 -DFF_BUILD_NUMBER=$BUILD_NUMBER $JUCE_ARG || { echo "ERREUR configuration"; exit 1; }
"$CMAKE" --build build --config Release --target FlowFilter_VST3 FlowFilter_AU -j "$(sysctl -n hw.ncpu)" || { echo "ERREUR compilation"; exit 1; }

# 7. macOS garde une liste en cache des Audio Units : on la force a se reconstruire
killall -9 AudioComponentRegistrar 2>/dev/null

echo ""
echo "=== Plugins installes (version $VERSION) ==="
ls -d "$HOME/Library/Audio/Plug-Ins/VST3/Flow Filter.vst3" "$HOME/Library/Audio/Plug-Ins/Components/Flow Filter.component" 2>/dev/null
/usr/libexec/PlistBuddy -c "Print :CFBundleShortVersionString" "$HOME/Library/Audio/Plug-Ins/VST3/Flow Filter.vst3/Contents/Info.plist" 2>/dev/null
echo "=== TERMINE : quitte et relance Ableton, la version $VERSION s'affiche en haut du plugin ==="
