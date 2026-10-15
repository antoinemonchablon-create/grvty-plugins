#!/bin/bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Copyright (C) 2026 GRVTY Records — voir les fichiers LICENSE et NOTICE
#
# Prépare une version publiable de Confluence dans le dossier dist/ :
#   Confluence-v<version>-macOS.zip   installeur .pkg (Apple Silicon + Intel), LICENSE, NOTICE, THIRD-PARTY-NOTICES,
#                                     LISEZMOI, ROUTAGE-ABLETON, BUILD-INFO
#   Confluence-v<version>-source.zip  le code source exact de cette version (obligation de la licence AGPL)
#   JUCE-8.0.12-source.zip            le code source de JUCE compilé dans le plugin (même obligation)
#   SHA256SUMS.txt                    empreintes pour vérifier les téléchargements
#
# La version est celle de la dernière compilation (.build_number, mis à jour par build.command) : on publie ce qu'on a
# écouté dans Ableton. Le script refuse de travailler s'il reste des modifications non enregistrées dans git.
#
# Adresse publique du code source, affichée dans le panneau « À propos » et dans LISEZMOI :
#   export CF_SOURCE_URL="https://github.com/<compte>/<dépôt>"
# Signature Apple (facultative ; sans elle macOS demande « Ouvrir quand même » à l'installation).
# Attention : un certificat Developer ID affiche ton nom légal dans l'installeur.
#   export CF_SIGN_APP="Developer ID Application: Prénom Nom (TEAMID)"
#   export CF_SIGN_PKG="Developer ID Installer: Prénom Nom (TEAMID)"
#   export CF_NOTARY_PROFILE="confluence-notary"
#     (profil créé une seule fois avec : xcrun notarytool store-credentials confluence-notary
#      --apple-id <ton identifiant Apple> --team-id <TEAMID> --password <mot de passe pour app>)
#
# Rien n'est installé sur ce Mac : le plugin de développement (build.command) reste en place.

set -o pipefail
cd "$(dirname "$0")" || exit 1
ROOT="$(pwd)"
LOG="$ROOT/release.log"
exec > >(tee "$LOG") 2>&1

fail() { echo "ERREUR: $*"; echo "=== ECHEC ==="; exit 1; }

echo "=== Confluence : préparation d'une version publiable ($(date)) ==="

# 1. Outils
xcode-select -p >/dev/null 2>&1 || fail "outils de développement Apple absents (xcode-select --install)"
CMAKE="$(command -v cmake)"
for c in /opt/homebrew/bin/cmake /usr/local/bin/cmake /Applications/CMake.app/Contents/bin/cmake "$HOME/Library/Python/"*/bin/cmake; do
  [ -z "$CMAKE" ] && [ -x "$c" ] && CMAKE="$c"
done
[ -n "$CMAKE" ] || fail "CMake introuvable : lance d'abord build.command, qui l'installe"
for t in git pkgbuild productbuild ditto shasum codesign zip lipo rsync; do
  command -v "$t" >/dev/null 2>&1 || fail "outil $t introuvable"
done

# 2. Code source : tout doit être enregistré dans git, sinon l'archive source ne correspondrait pas au plugin
IN_GIT=0
COMMIT="(pas de dépôt git)"
if git -C "$ROOT" rev-parse --git-dir >/dev/null 2>&1; then
  IN_GIT=1
  if [ -n "$(git -C "$ROOT" status --porcelain)" ]; then
    git -C "$ROOT" status --short | head -20
    fail "des modifications ne sont pas enregistrées dans git : fais un commit (git add -A && git commit -s), puis relance"
  fi
  COMMIT="$(git -C "$ROOT" rev-parse HEAD) ($(git -C "$ROOT" describe --always --dirty))"
  # pseudonyme : prévient si un commit porte une adresse autre que l'adresse « noreply » de GitHub
  OTHER_MAILS="$(git -C "$ROOT" log --format='%ae%n%ce' | grep -v '@users.noreply.github.com$')"
  if [ -n "$OTHER_MAILS" ]; then
    echo "ATTENTION : certains commits portent une adresse e-mail personnelle (git log --format='%an <%ae>')."
  fi
else
  echo "ATTENTION : pas de dépôt git ; l'archive source contiendra les fichiers tels qu'ils sont sur le disque."
fi

# 3. Version : celle de la dernière compilation (pas de nouveau numéro)
[ -f .build_number ] || fail ".build_number absent : lance d'abord build.command"
BUILD_NUMBER="$(tr -cd '0-9' < .build_number)"
[ -n "$BUILD_NUMBER" ] || fail ".build_number illisible"
VERSION="1.$(( BUILD_NUMBER / 256 )).$(( BUILD_NUMBER % 256 ))"
NAME="Confluence-v$VERSION"
echo "Version : $VERSION (commit $COMMIT)"
[ -n "${CF_SOURCE_URL:-}" ] || echo "ATTENTION : CF_SOURCE_URL n'est pas défini ; le panneau « À propos » renverra à l'archive source."

# 4. Compilation universelle (Apple Silicon + Intel), dossier neuf, JUCE 8.0.12 téléchargé tel quel depuis GitHub
JUCE_VERSION="8.0.12"
JUCE_COMMIT="29396c22c93392d6738e021b83196283d6e4d850"
BUILD_DIR="$ROOT/build-release"
rm -rf "$BUILD_DIR"
CMAKE_ARGS=(-B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release "-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64" -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0
            "-DCF_BUILD_NUMBER=$BUILD_NUMBER" -DCF_INSTALL_AFTER_BUILD=OFF)
[ -n "${CF_SOURCE_URL:-}" ] && CMAKE_ARGS+=("-DCF_SOURCE_URL=$CF_SOURCE_URL")
"$CMAKE" "${CMAKE_ARGS[@]}" || fail "configuration"
JUCE_SRC="$BUILD_DIR/_deps/juce-src"
[ "$(git -C "$JUCE_SRC" rev-parse HEAD 2>/dev/null)" = "$JUCE_COMMIT" ] || fail "JUCE téléchargé n'est pas le commit attendu de la version $JUCE_VERSION"
"$CMAKE" --build "$BUILD_DIR" --config Release --target Confluence_VST3 -j "$(sysctl -n hw.ncpu)" || fail "compilation"
PLUGIN="$BUILD_DIR/Confluence_artefacts/Release/VST3/Confluence.vst3"
[ -d "$PLUGIN" ] || fail "plugin introuvable après la compilation"
lipo "$PLUGIN/Contents/MacOS/Confluence" -verify_arch arm64 x86_64 || fail "le plugin n'est pas universel (arm64 + x86_64)"
echo "Architectures : $(lipo -archs "$PLUGIN/Contents/MacOS/Confluence")"

# 5. Textes légaux dans le plugin lui-même, puis signature
mkdir -p "$PLUGIN/Contents/Resources"
cp LICENSE NOTICE THIRD-PARTY-NOTICES.md "$PLUGIN/Contents/Resources/" || fail "copie des textes légaux"
if [ -n "${CF_SIGN_APP:-}" ]; then
  codesign --force --timestamp --options runtime --sign "$CF_SIGN_APP" "$PLUGIN" || fail "signature du plugin"
  SIGNED="Developer ID"
else
  codesign --force --sign - "$PLUGIN" || fail "signature ad hoc"
  SIGNED="ad hoc (pas de Developer ID)"
fi
codesign --verify --strict --verbose=1 "$PLUGIN" || fail "vérification de la signature"

# 6. Validation pluginval (strict, niveau 10) : Apple Silicon, puis Intel si Rosetta est installé
PV=""
for p in "$ROOT/tools/pluginval.app/Contents/MacOS/pluginval" "$ROOT/../Undertow/tools/pluginval.app/Contents/MacOS/pluginval"; do
  [ -z "$PV" ] && [ -x "$p" ] && PV="$p"
done
[ -n "$PV" ] || fail "pluginval introuvable : lance d'abord build.command, qui le télécharge"
HOST_ARCH="$(uname -m)"
"$PV" --strictness-level 10 --timeout-ms 600000 --validate "$PLUGIN" > "$ROOT/pluginval-release-$HOST_ARCH.log" 2>&1 \
  || fail "pluginval ($HOST_ARCH) : voir pluginval-release-$HOST_ARCH.log"
echo "PLUGINVAL $HOST_ARCH : SUCCESS"
PV_INTEL="non testé (Rosetta absent, ou pluginval sans version Intel)"
[ "$HOST_ARCH" = "x86_64" ] && PV_INTEL="SUCCESS (Mac Intel)"
if [ "$HOST_ARCH" != "x86_64" ] && arch -x86_64 /usr/bin/true 2>/dev/null && lipo -archs "$PV" 2>/dev/null | grep -q x86_64; then
  if arch -x86_64 "$PV" --strictness-level 10 --timeout-ms 600000 --validate "$PLUGIN" > "$ROOT/pluginval-release-x86_64.log" 2>&1; then
    PV_INTEL="SUCCESS"
  else
    fail "pluginval (Intel, via Rosetta) : voir pluginval-release-x86_64.log"
  fi
fi
echo "PLUGINVAL x86_64 : $PV_INTEL"

# 7. Installeur .pkg : installe Confluence.vst3 dans /Library/Audio/Plug-Ins/VST3 (tous les utilisateurs)
DIST="$ROOT/dist"
WORK="$DIST/.work"
rm -rf "$WORK" "$DIST/$NAME-macOS" "$DIST/$NAME-macOS.zip" "$DIST/$NAME-source.zip" "$DIST/JUCE-$JUCE_VERSION-source.zip" "$DIST/SHA256SUMS.txt"
mkdir -p "$WORK/root" "$DIST/$NAME-macOS"
ditto "$PLUGIN" "$WORK/root/Confluence.vst3" || fail "copie du plugin"
xattr -cr "$WORK/root" 2>/dev/null   # attributs étendus du Mac de compilation (provenance...) : rien à livrer
pkgbuild --analyze --root "$WORK/root" "$WORK/components.plist" >/dev/null || fail "pkgbuild --analyze"
/usr/libexec/PlistBuddy -c "Print :0" "$WORK/components.plist" >/dev/null 2>&1 || fail "pkgbuild --analyze n'a pas trouvé Confluence.vst3"
# le plugin s'installe toujours au même endroit (même si une autre copie existe ailleurs sur le Mac), y compris
# par-dessus une version plus récente ou identique ; selon la version de macOS, ces clés sont absentes ou présentes
PB=/usr/libexec/PlistBuddy
for k in BundleIsRelocatable BundleIsVersionChecked BundleOverwriteAction; do $PB -c "Delete :0:$k" "$WORK/components.plist" >/dev/null 2>&1; done
$PB -c "Add :0:BundleIsRelocatable bool false" -c "Add :0:BundleIsVersionChecked bool false" \
    -c "Add :0:BundleOverwriteAction string upgrade" "$WORK/components.plist" || fail "réglage des clés du paquet"
[ "$($PB -c "Print :0:BundleIsRelocatable" "$WORK/components.plist")" = "false" ] || fail "BundleIsRelocatable n'est pas à false"
# avant l'installation : retire l'ancienne version (aucun fichier périmé mélangé au nouveau plugin)
mkdir -p "$WORK/scripts"
printf '#!/bin/sh\nrm -rf "/Library/Audio/Plug-Ins/VST3/Confluence.vst3"\nexit 0\n' > "$WORK/scripts/preinstall"
chmod 755 "$WORK/scripts/preinstall"
pkgbuild --root "$WORK/root" --component-plist "$WORK/components.plist" --scripts "$WORK/scripts" \
  --identifier com.grvtyrecords.confluence.vst3 \
  --version "$VERSION" --install-location /Library/Audio/Plug-Ins/VST3 "$WORK/Confluence-vst3.pkg" || fail "pkgbuild"
PKG="$DIST/$NAME-macOS/$NAME.pkg"
if [ -n "${CF_SIGN_PKG:-}" ]; then
  productbuild --package "$WORK/Confluence-vst3.pkg" --sign "$CF_SIGN_PKG" "$PKG" || fail "productbuild (signé)"
else
  productbuild --package "$WORK/Confluence-vst3.pkg" "$PKG" || fail "productbuild"
fi

# 8. Notarisation Apple : seulement si le plugin ET l'installeur sont signés Developer ID
NOTARIZED="non"
if [ -n "${CF_SIGN_APP:-}" ] && [ -n "${CF_SIGN_PKG:-}" ] && [ -n "${CF_NOTARY_PROFILE:-}" ]; then
  echo "Notarisation Apple (quelques minutes)..."
  xcrun notarytool submit "$PKG" --keychain-profile "$CF_NOTARY_PROFILE" --wait || fail "notarisation"
  xcrun stapler staple "$PKG" || fail "agrafage du ticket de notarisation"
  NOTARIZED="oui"
fi

# 9. Archive du plugin : installeur + textes légaux + mode d'emploi
INFO="Confluence $VERSION (numéro de build $BUILD_NUMBER : cmake -DCF_BUILD_NUMBER=$BUILD_NUMBER)
Date : $(date '+%Y-%m-%d %H:%M')
Commit : $COMMIT
JUCE : $JUCE_VERSION, commit $JUCE_COMMIT (GNU AGPL v3) ; SDK VST 3 : 3.8.0 (MIT)
Signature : $SIGNED ; notarisation Apple : $NOTARIZED ; pluginval : $HOST_ARCH SUCCESS, x86_64 $PV_INTEL"
cp LICENSE NOTICE THIRD-PARTY-NOTICES.md ROUTAGE-ABLETON.md "$DIST/$NAME-macOS/" || fail "copie des textes"
URL_TEXT="${CF_SOURCE_URL:-la page où tu as téléchargé Confluence}"
URL_SED="$(printf '%s' "$URL_TEXT" | sed 's/[&|\\]/\\&/g')"   # & | et \ ont un sens pour sed
sed -e "s|@VERSION@|$VERSION|g" -e "s|@SOURCE_URL@|$URL_SED|g" packaging/mac/LISEZMOI.txt > "$DIST/$NAME-macOS/LISEZMOI.txt" || fail "LISEZMOI"
echo "$INFO" > "$DIST/$NAME-macOS/BUILD-INFO.txt"
(cd "$DIST" && ditto -c -k --keepParent "$NAME-macOS" "$NAME-macOS.zip") || fail "archive du plugin"

# 10. Archives du code source : Confluence (exactement le commit compilé) et JUCE
mkdir -p "$WORK/$NAME-source"
echo "$INFO" > "$WORK/$NAME-source/BUILD-INFO.txt"
if [ "$IN_GIT" = "1" ]; then
  git -C "$ROOT" archive --format=zip --prefix="$NAME-source/" -o "$DIST/$NAME-source.zip" HEAD || fail "git archive"
  (cd "$WORK" && zip -q "$DIST/$NAME-source.zip" "$NAME-source/BUILD-INFO.txt") || fail "ajout de BUILD-INFO"
else
  rsync -a --exclude '.git' --exclude 'build' --exclude 'build-*' --exclude 'dist' --exclude 'tools' --exclude '.livraison' \
    --exclude 'prive' --exclude 'rendus' --exclude 'Compiler Confluence.terminal' --exclude '*.log' --exclude 'test-results.md' \
    --exclude '.DS_Store' "$ROOT/" "$WORK/$NAME-source/" || fail "copie des sources"
  (cd "$WORK" && zip -q -r -X "$DIST/$NAME-source.zip" "$NAME-source") || fail "archive des sources"
fi
git -C "$JUCE_SRC" archive --format=zip --prefix="JUCE-$JUCE_VERSION/" -o "$DIST/JUCE-$JUCE_VERSION-source.zip" HEAD \
  || fail "archive des sources de JUCE"

# 11. Empreintes SHA-256
(cd "$DIST" && shasum -a 256 "$NAME-macOS.zip" "$NAME-source.zip" "JUCE-$JUCE_VERSION-source.zip" > SHA256SUMS.txt) || fail "empreintes"
rm -rf "$WORK"

echo ""
echo "=== Contenu de dist/ ==="
ls -la "$DIST"
cat "$DIST/SHA256SUMS.txt"
pkgutil --check-signature "$PKG" | head -3
[ "$NOTARIZED" = "oui" ] && spctl --assess --type install --verbose "$PKG"
echo ""
echo "$INFO"
echo "=== TERMINE : publie les 3 archives et SHA256SUMS.txt, puis pose le tag v$VERSION sur ce commit ==="
