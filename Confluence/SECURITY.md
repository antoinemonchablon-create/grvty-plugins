<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Sécurité / Security

## Signaler une faille

Si tu trouves une faille de sécurité (un preset, un projet ou un flux MIDI qui fait planter le plugin ou l'hôte,
corrompt un projet, exécute du code, etc.), **ne la publie pas dans une issue publique**. Signale-la en privé :
onglet **Security > Report a vulnerability** du dépôt GitHub de Confluence.

Indique la version (en haut à droite du plugin), le système, l'hôte (Ableton Live x.y) et, si possible, le fichier
qui provoque le problème. Tu recevras une réponse dès que possible ; la correction est publiée dans une nouvelle version
et mentionnée dans `CHANGELOG.md`, avec ton nom si tu le souhaites.

*To report a vulnerability, please use GitHub's private "Report a vulnerability" form on the repository's Security tab
rather than a public issue.*

## Versions suivies

Seule la dernière version publiée reçoit des corrections de sécurité.

## Ce que fait (et ne fait pas) Confluence

- Aucun accès réseau, aucune télémétrie, aucune mise à jour automatique, aucune donnée collectée : le code de
  Confluence n'appelle aucune fonction réseau. Seuls les boutons LICENCE et CODE SOURCE du panneau « À propos »
  demandent au navigateur d'ouvrir une page (`URL::launchInDefaultBrowser`). Le navigateur web intégré de JUCE
  est retiré à la compilation (`JUCE_WEB_BROWSER=0`).
- Fichiers lus : l'état du plugin fourni par l'hôte (projet Ableton) et les presets `.cfpreset` choisis par
  l'utilisateur. Chaque valeur lue est vérifiée : nombre invalide (NaN, infini) remplacé par la valeur par défaut,
  valeur hors plage ramenée dans sa plage. Le banc de tests (`tests/`, groupe `hostile`) charge des fichiers piégés
  ou corrompus à chaque version.
- Fichiers écrits, uniquement à la demande de l'utilisateur : les presets utilisateur
  (Mac : `~/Library/Audio/Presets/GRVTY Records/Confluence` ; Windows : `Documents\GRVTY Records\Confluence\Presets`),
  les exports MIDI (dossier Musique / Confluence) et, pour le
  glisser-déposer vers Ableton, un fichier MIDI temporaire dans le dossier temporaire du système. Une seule fois
  sur Mac, les presets de l'ancien dossier `~/Library/Audio/Presets/Jasperr/Confluence` sont recopiés dans le nouveau
  dossier (l'ancien n'est ni modifié ni effacé).
- Thread audio : aucune allocation mémoire (vérifié par le banc de tests), aucun verrou, aucun accès disque.
- Chaque version publiée est validée avec pluginval en mode strict (niveau 10) : sur Mac par `release.command`
  (Apple Silicon, et Intel quand Rosetta est installé), sur Windows par `packaging/windows/build-windows.sh`,
  et sur Linux par la compilation automatique.
