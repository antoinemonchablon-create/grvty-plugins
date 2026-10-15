#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 GRVTY Records — voir les fichiers LICENSE et NOTICE
#
# Compile Confluence pour Windows (VST3 64 bits), le valide avec pluginval et fabrique l'installeur .exe.
# À lancer dans Git Bash sur Windows, avec Visual Studio 2022 ou plus récent (ou ses Build Tools, « Développement
# Desktop en C++ »), CMake et NSIS 3. C'est ce que fait la compilation automatique GitHub (.github/workflows/build.yml).
#
# Résultat dans dist/ :
#   Confluence-v<version>-Windows-setup.exe   installeur (plugin + LICENSE, NOTICE, THIRD-PARTY-NOTICES, LISEZMOI...)
#   SHA256SUMS-Windows.txt                    empreinte de l'installeur
#
# Facultatif : export CF_SOURCE_URL="https://github.com/<compte>/<dépôt>"  (panneau « À propos », LISEZMOI, installeur)
# La version est celle de .build_number, comme sur Mac (release.command).

set -o pipefail
cd "$(dirname "$0")/../.." || exit 1
# chemin Windows « D:/... » : compris par Git Bash ET par les programmes Windows (cmake, git, curl, pluginval)
ROOT="$(pwd -W)"

fail() { echo "ERREUR: $*"; echo "=== ECHEC ==="; exit 1; }
winpath() { cygpath -w "$1"; }

echo "=== Confluence : version Windows ($(date)) ==="

command -v cmake >/dev/null 2>&1 || fail "CMake introuvable"
MAKENSIS="$(command -v makensis)"
for m in "/c/Program Files (x86)/NSIS/makensis.exe" "/c/Program Files/NSIS/makensis.exe"; do
  [ -z "$MAKENSIS" ] && [ -x "$m" ] && MAKENSIS="$m"
done
[ -n "$MAKENSIS" ] || fail "NSIS (makensis) introuvable"

[ -f .build_number ] || fail ".build_number absent"
BUILD_NUMBER="$(tr -cd '0-9' < .build_number)"
[ -n "$BUILD_NUMBER" ] || fail ".build_number illisible"
VERSION="1.$(( BUILD_NUMBER / 256 )).$(( BUILD_NUMBER % 256 ))"
NAME="Confluence-v$VERSION"
COMMIT="$(git rev-parse HEAD 2>/dev/null || echo 'inconnu')"
echo "Version : $VERSION (commit $COMMIT)"

# 1. Compilation (Visual Studio, 64 bits, runtime C++ intégré au plugin), dossier neuf, JUCE 8.0.12 téléchargé tel quel
BUILD_DIR="$ROOT/build-windows"
rm -rf "$BUILD_DIR"
# générateur par défaut de CMake sous Windows : la version de Visual Studio la plus récente installée
CMAKE_ARGS=(-B "$BUILD_DIR" -A x64 "-DCF_BUILD_NUMBER=$BUILD_NUMBER" -DCF_INSTALL_AFTER_BUILD=OFF)
[ -n "${CF_SOURCE_URL:-}" ] && CMAKE_ARGS+=("-DCF_SOURCE_URL=$CF_SOURCE_URL")
cmake "${CMAKE_ARGS[@]}" || fail "configuration"
JUCE_COMMIT="29396c22c93392d6738e021b83196283d6e4d850"
[ "$(git -C "$BUILD_DIR/_deps/juce-src" rev-parse HEAD 2>/dev/null)" = "$JUCE_COMMIT" ] || fail "JUCE téléchargé n'est pas la version 8.0.12 attendue"
cmake --build "$BUILD_DIR" --config Release --target Confluence_VST3 --parallel || fail "compilation"
PLUGIN="$BUILD_DIR/Confluence_artefacts/Release/VST3/Confluence.vst3"
[ -f "$PLUGIN/Contents/x86_64-win/Confluence.vst3" ] || fail "plugin introuvable après la compilation"

# 2. Textes légaux dans le plugin lui-même
mkdir -p "$PLUGIN/Contents/Resources"
cp LICENSE NOTICE THIRD-PARTY-NOTICES.md "$PLUGIN/Contents/Resources/" || fail "copie des textes légaux"

# 3. Validation pluginval (strict, niveau 10)
PV_DIR="$BUILD_DIR/pluginval"
mkdir -p "$PV_DIR"
curl -fsSL -o "$PV_DIR/pluginval.zip" https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip \
  || fail "téléchargement de pluginval"
if command -v unzip >/dev/null 2>&1; then
  (cd "$PV_DIR" && unzip -o -q pluginval.zip) || fail "décompression de pluginval"
else
  powershell -NoProfile -Command "Expand-Archive -Force -Path '$(winpath "$PV_DIR/pluginval.zip")' -DestinationPath '$(winpath "$PV_DIR")'" \
    || fail "décompression de pluginval"
fi
[ -x "$PV_DIR/pluginval.exe" ] || [ -f "$PV_DIR/pluginval.exe" ] || fail "pluginval.exe introuvable dans l'archive"
"$PV_DIR/pluginval.exe" --strictness-level 10 --validate-in-process --timeout-ms 600000 --output-dir "$(winpath "$PV_DIR")" \
  --validate "$(winpath "$PLUGIN")" > "$ROOT/pluginval-windows.log" 2>&1 \
  || { cat "$ROOT/pluginval-windows.log"; fail "pluginval (Windows) : voir pluginval-windows.log"; }
echo "PLUGINVAL Windows x64 : SUCCESS"

# 4. Installeur NSIS
DIST="$ROOT/dist"
DOCS="$BUILD_DIR/docs"
mkdir -p "$DIST" "$DOCS"
rm -f "$DIST/$NAME-Windows-setup.exe" "$DIST/SHA256SUMS-Windows.txt"
cp LICENSE NOTICE THIRD-PARTY-NOTICES.md ROUTAGE-ABLETON.md "$DOCS/" || fail "copie des textes"
URL_TEXT="${CF_SOURCE_URL:-la page où tu as téléchargé Confluence}"
URL_SED="$(printf '%s' "$URL_TEXT" | sed 's/[&|\\]/\\&/g')"   # & | et \ ont un sens pour sed
sed -e "s|@VERSION@|$VERSION|g" -e "s|@SOURCE_URL@|$URL_SED|g" packaging/windows/LISEZMOI.txt > "$DOCS/LISEZMOI.txt" || fail "LISEZMOI"
cat > "$DOCS/BUILD-INFO.txt" <<EOF
Confluence $VERSION (numéro de build $BUILD_NUMBER : cmake -DCF_BUILD_NUMBER=$BUILD_NUMBER)
Date : $(date '+%Y-%m-%d %H:%M')
Commit : $COMMIT
JUCE : 8.0.12, commit $JUCE_COMMIT (GNU AGPL v3) ; SDK VST 3 : 3.8.0 (MIT)
Windows x64, $(grep -m1 '^CMAKE_GENERATOR:' "$BUILD_DIR/CMakeCache.txt" | cut -d= -f2) ; installeur non signé ; pluginval 1.0.4 : SUCCESS
EOF
NSIS_ARGS=(-INPUTCHARSET UTF8 "-DVERSION=$VERSION" "-DPLUGIN_DIR=$(winpath "$PLUGIN")" "-DDOCS_DIR=$(winpath "$DOCS")"
           "-DOUTFILE=$(winpath "$DIST/$NAME-Windows-setup.exe")")
[ -n "${CF_SOURCE_URL:-}" ] && NSIS_ARGS+=("-DSOURCE_URL=$CF_SOURCE_URL")
"$MAKENSIS" "${NSIS_ARGS[@]}" "$(winpath "$ROOT/packaging/windows/installer.nsi")" || fail "makensis"

# 5. Empreinte SHA-256
(cd "$DIST" && sha256sum "$NAME-Windows-setup.exe" > SHA256SUMS-Windows.txt) || fail "empreinte"
cat "$DIST/SHA256SUMS-Windows.txt"
echo "=== TERMINE : $DIST/$NAME-Windows-setup.exe ==="
