#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 GRVTY Records — voir les fichiers LICENSE et NOTICE
#
# Compile UN plugin GRVTY Records pour Windows (VST3 64 bits), le valide avec pluginval (strict, niveau 10)
# et fabrique son installeur .exe + l'archive de son code source.
# Usage (Git Bash sur Windows, Visual Studio 2022+, CMake, NSIS 3) :
#   bash packaging/windows/build-windows.sh FlowFilter|Undertow|Confluence|Ebb
# C'est ce que fait la compilation automatique GitHub (.github/workflows/windows.yml).
# Résultat dans dist/ : <Produit>-<version>-Windows-setup.exe, <Produit>-<version>-source.zip, SHA256SUMS-<clé>.txt

set -o pipefail
KEY="$1"
case "$KEY" in
  FlowFilter) PRODUCT="Flow Filter"; PFX=FF; DESC="filtre en mouvement pour tes boucles techno" ;;
  Undertow)   PRODUCT="Undertow";    PFX=UT; DESC="synthé FM avec séquenceur pour boucles techno hypnotiques" ;;
  Confluence) PRODUCT="Confluence";  PFX=CF; DESC="une note jouée devient un accord complet, envoyé en MIDI à ton synthé" ;;
  Ebb)        PRODUCT="Ebb";         PFX=EB; DESC="sidechain et mise en forme du volume, courbe dessinée à la main" ;;
  *) echo "Usage : $0 FlowFilter|Undertow|Confluence|Ebb"; exit 1 ;;
esac
cd "$(dirname "$0")/../.." || exit 1
ROOT="$(pwd -W)"            # chemin « D:/... » compris par Git Bash ET par les programmes Windows
PROJ="$ROOT/$KEY"
fail() { echo "ERREUR ($PRODUCT): $*"; echo "=== ECHEC ==="; exit 1; }
winpath() { cygpath -w "$1"; }

command -v cmake >/dev/null 2>&1 || fail "CMake introuvable"
MAKENSIS="$(command -v makensis)"
for m in "/c/Program Files (x86)/NSIS/makensis.exe" "/c/Program Files/NSIS/makensis.exe"; do
  [ -z "$MAKENSIS" ] && [ -x "$m" ] && MAKENSIS="$m"
done
[ -n "$MAKENSIS" ] || fail "NSIS (makensis) introuvable"

BUILD_NUMBER="$(tr -cd '0-9' < "$PROJ/.build_number")"
[ -n "$BUILD_NUMBER" ] || fail ".build_number illisible"
VERSION="1.$(( BUILD_NUMBER / 256 )).$(( BUILD_NUMBER % 256 ))"
NAME="${PRODUCT// /-}-$VERSION"
COMMIT="$(git rev-parse HEAD 2>/dev/null || echo 'inconnu')"
echo "=== $PRODUCT $VERSION pour Windows (commit $COMMIT) ==="

# 1. Compilation (Visual Studio 64 bits, runtime C++ intégré), dossier neuf, JUCE 8.0.12 téléchargé et vérifié
BUILD_DIR="$ROOT/build-windows/$KEY"
rm -rf "$BUILD_DIR"
CMAKE_ARGS=(-S "$PROJ" -B "$BUILD_DIR" -A x64 -DJUCE_PATH= "-D${PFX}_BUILD_NUMBER=$BUILD_NUMBER" "-D${PFX}_INSTALL_AFTER_BUILD=OFF")
[ -n "${GRVTY_SOURCE_URL:-}" ] && CMAKE_ARGS+=("-D${PFX}_SOURCE_URL=$GRVTY_SOURCE_URL")
cmake "${CMAKE_ARGS[@]}" || fail "configuration"
JUCE_COMMIT="29396c22c93392d6738e021b83196283d6e4d850"
[ "$(git -C "$BUILD_DIR/_deps/juce-src" rev-parse HEAD 2>/dev/null)" = "$JUCE_COMMIT" ] || fail "JUCE téléchargé n'est pas la version 8.0.12"
cmake --build "$BUILD_DIR" --config Release --target "${KEY}_VST3" --parallel || fail "compilation"
PLUGIN="$BUILD_DIR/${KEY}_artefacts/Release/VST3/$PRODUCT.vst3"
[ -f "$PLUGIN/Contents/x86_64-win/$PRODUCT.vst3" ] || fail "plugin introuvable après la compilation"

# 2. Textes légaux dans le plugin lui-même
mkdir -p "$PLUGIN/Contents/Resources"
cp "$PROJ/LICENSE" "$PROJ/NOTICE" "$PROJ/THIRD-PARTY-NOTICES.md" "$PLUGIN/Contents/Resources/" || fail "copie des textes légaux"

# 3. pluginval (strict, niveau 10)
PV_DIR="$ROOT/build-windows/pluginval"
if [ ! -f "$PV_DIR/pluginval.exe" ]; then
  mkdir -p "$PV_DIR"
  curl -fsSL -o "$PV_DIR/pluginval.zip" https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_Windows.zip \
    || fail "téléchargement de pluginval"
  powershell -NoProfile -Command "Expand-Archive -Force -Path '$(winpath "$PV_DIR/pluginval.zip")' -DestinationPath '$(winpath "$PV_DIR")'" \
    || fail "décompression de pluginval"
fi
LOG="$ROOT/pluginval-windows-$KEY.log"
"$PV_DIR/pluginval.exe" --strictness-level 10 --validate-in-process --timeout-ms 600000 \
  --validate "$(winpath "$PLUGIN")" > "$LOG" 2>&1 || { tail -60 "$LOG"; fail "pluginval : voir $(basename "$LOG")"; }
echo "PLUGINVAL $PRODUCT Windows x64 : SUCCESS"

# 4. Installeur NSIS
DIST="$ROOT/dist"; DOCS="$BUILD_DIR/docs"
mkdir -p "$DIST" "$DOCS"
SETUP="$NAME-Windows-setup.exe"
rm -f "$DIST/$SETUP"
for f in LICENSE NOTICE THIRD-PARTY-NOTICES.md README.md ROUTAGE-ABLETON.md; do
  [ -f "$PROJ/$f" ] && cp "$PROJ/$f" "$DOCS/"
done
sed -e "s|@PRODUCT_UPPER@|${PRODUCT^^}|g" -e "s|@PRODUCT@|$PRODUCT|g" -e "s|@VERSION@|$VERSION|g" \
    -e "s|@DESCRIPTION@|$DESC|g" -e "s|@SETUP@|$SETUP|g" -e "s|@SOURCEZIP@|$NAME-source.zip|g" \
    "$ROOT/packaging/windows/LISEZMOI.txt" > "$DOCS/LISEZMOI.txt" || fail "LISEZMOI"
cat > "$DOCS/BUILD-INFO.txt" <<EOF
$PRODUCT $VERSION — une création du label et collectif GRVTY Records, développée par Jasperr
Numéro de build $BUILD_NUMBER (cmake -D${PFX}_BUILD_NUMBER=$BUILD_NUMBER), commit $COMMIT, $(date '+%Y-%m-%d %H:%M')
JUCE 8.0.12, commit $JUCE_COMMIT (GNU AGPL v3) ; SDK VST 3 (MIT)
Windows x64, Visual Studio ; installeur non signé ; pluginval 1.0.4 strict niveau 10 : SUCCESS
EOF
"$MAKENSIS" -INPUTCHARSET UTF8 "-DPRODUCT=$PRODUCT" "-DDESCRIPTION=$DESC" "-DVERSION=$VERSION" \
  "-DPLUGIN_DIR=$(winpath "$PLUGIN")" "-DDOCS_DIR=$(winpath "$DOCS")" "-DOUTFILE=$(winpath "$DIST/$SETUP")" \
  "$(winpath "$ROOT/packaging/windows/installer.nsi")" || fail "makensis"

# 5. Code source de cette version (obligation de la licence AGPL) et empreintes
(cd "$ROOT" && git archive --format=zip --prefix="$NAME-source/" -o "$DIST/$NAME-source.zip" HEAD "$KEY") || fail "archive des sources"
(cd "$DIST" && sha256sum "$SETUP" "$NAME-source.zip" > "SHA256SUMS-$KEY.txt") || fail "empreintes"
cat "$DIST/SHA256SUMS-$KEY.txt"
echo "=== TERMINE : $DIST/$SETUP ==="
