<!-- SPDX-License-Identifier: AGPL-3.0-or-later -->
# Contribuer / Contributing

Merci de vouloir améliorer Confluence ! Bugs, idées de gammes ou de presets, corrections : tout est bienvenu.

## Avant de proposer une modification

1. **Licence** : en proposant une contribution (pull request, correctif, preset), tu acceptes qu'elle soit publiée
   sous la licence du projet, GNU AGPL v3 ou ultérieure, et tu certifies en avoir le droit
   ([Developer Certificate of Origin](https://developercertificate.org/) : ajoute `Signed-off-by: Ton Nom <email>`
   à tes commits avec `git commit -s`).
2. **Rien de copié** : pas de code, son, visuel, preset ou texte repris d'un produit dont tu n'as pas les droits.
3. **Identifiants des paramètres** : ne change jamais un identifiant de paramètre (`Source/Params.h`) ;
   les projets Ableton et les presets des utilisateurs en dépendent. Ajoute plutôt un nouveau paramètre.
4. **Temps réel** : rien qui alloue de la mémoire, prend un verrou ou touche au disque dans `processBlock`.
5. **Tests** : le banc de tests doit passer en entier :

   ```bash
   cmake -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DCF_BUILD_TESTS=ON
   ninja -C build-linux ConfluenceTests Confluence_VST3
   cd build-linux && ./ConfluenceTests_artefacts/Release/ConfluenceTests
   ```

6. **Nouveau fichier** : ajoute l'en-tête de licence en haut :

   ```cpp
   // SPDX-License-Identifier: AGPL-3.0-or-later
   // Copyright (C) 2026 <ton nom>
   ```

## Versions modifiées

Tu peux distribuer ta propre version modifiée de Confluence en respectant l'AGPL (même licence, code source fourni
avec, modifications indiquées) et les termes additionnels de [NOTICE](NOTICE) : **un autre nom**, et aucun usage
comme marque de « GRVTY Records », de « Jasperr », du nom du plugin ni de son logo.

*Contributions are accepted under AGPL-3.0-or-later with a DCO sign-off. Never change existing parameter IDs.
Run the full test bench before opening a pull request. Distributed forks must use a different name (see NOTICE).*
