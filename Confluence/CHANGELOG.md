<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Historique des versions

Le numéro de version augmente à chaque compilation (le dernier chiffre suit le numéro de build).

## 1.0.4 — 14 octobre 2026

- **GRVTY Records** : Confluence est une création du label et collectif GRVTY Records (développement : Jasperr).
  Fabricant affiché dans Ableton : Plug-Ins > VST3 > GRVTY Records. Presets personnels sur Mac dans
  `~/Library/Audio/Presets/GRVTY Records/Confluence` (les anciens presets du dossier « Jasperr » y sont recopiés).
- **Windows** : version 64 bits avec installeur `.exe` (NSIS), fabriquée par la compilation automatique GitHub
  (`.github/workflows/build.yml`) ou par `packaging/windows/build-windows.sh`.
- **Logiciel libre** : Confluence est publié sous licence GNU AGPL v3 ou ultérieure (`LICENSE`), avec ses termes
  additionnels (`NOTICE`), les mentions des composants tiers (`THIRD-PARTY-NOTICES.md`) et un en-tête de licence
  dans chaque fichier source.
- **Panneau « À propos »** (bouton ⓘ en haut à droite) : licence, absence de garantie, code source,
  confidentialité, composants tiers, marques et logo VST Compatible de Steinberg.
- **Sécurité** : un preset ou un projet trafiqué ne peut plus faire entrer de valeur invalide (NaN, infini,
  hors plage) dans le plugin ; nouveau test de fichiers piégés.
- JUCE 8.0.12 et SDK VST 3.8.0 (licence MIT) ; décodeurs FLAC et Ogg Vorbis de JUCE retirés (inutiles).
- `release.command` : installeur `.pkg` universel (Apple Silicon + Intel) validé par pluginval, archives du code
  source de Confluence et de JUCE, empreintes SHA-256, signature et notarisation Apple si un compte développeur est
  configuré. L'installeur retire d'abord l'ancienne version du plugin et réinstalle toujours au même endroit.

## 1.0.3 — 7 octobre 2026

- Interface définitive (disposition « Courant ») : mer et clavier sous l'eau, bulles, tourbillon des enchaînements,
  8 pads, onglets CHORD / VOICING / PLAY, navigateur de presets.
- 6 presets d'usine (2 par banque) avec leur phrase de démonstration.

## 1.0.2 — 7 octobre 2026

- Moteur d'accords : 4 modes (Gamme, Touches blanches, Accord fixe + Learn, Pads), 17 gammes, voicing, strum,
  humanize, durées, latch, voix Preview, détection de gamme, enchaînements conseillés, export MIDI.
- Interface de travail provisoire.

## 1.0.1 — 7 octobre 2026

- Première compilation sur Mac (non publiée).
