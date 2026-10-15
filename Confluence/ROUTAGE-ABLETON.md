# Router Confluence vers un synthé dans Ableton Live

Ableton n'accepte pas qu'un plugin VST® d'effet MIDI soit placé juste avant un instrument.
Confluence est donc un **instrument** qui reçoit tes notes et renvoie des accords en MIDI.
Il se place sur sa propre piste MIDI, et la piste du synthé « écoute » sa sortie MIDI.

```
 clavier / contrôleur ──► [Piste 1 : Confluence] ──MIDI──► [Piste 2 : ton synthé] ──► son
```

## Première installation

1. Installe Confluence avec l'installeur d'une version publiée : `.pkg` sur Mac, `-Windows-setup.exe` sur Windows
   (voir `LISEZMOI.txt`), ou, depuis le code source, compile-le (`build.command` sur Mac).
2. Dans Ableton : **Réglages > Plug-Ins**, vérifie que les plugins VST3 sont activés
   (« Use VST3 Plug-in System Folders »), puis clique sur **Rescan**.
   Confluence apparaît dans le navigateur, dans **Plug-Ins > VST3 > GRVTY Records**.

## Brancher Confluence sur un synthé

1. **Piste 1 : Confluence**
   - Crée une piste MIDI (Cmd + Maj + T sur Mac, Ctrl + Maj + T sur Windows) et glisse **Confluence** dessus.
   - Entrée (« MIDI From ») : ton clavier ou contrôleur (ou « All Ins »).
   - Monitoring : **In**.
2. **Piste 2 : le synthé**
   - Crée une deuxième piste MIDI avec ton synthé (Wavetable, Drift, Analog ou un VST).
   - « MIDI From » : choisis **la piste 1** (celle qui porte Confluence).
   - Dans le menu juste en dessous (là où il y a « Pre FX » / « Post FX »), choisis **Confluence**.
   - Monitoring : **In**.
3. Joue une note : la piste 1 reçoit la note, Confluence calcule l'accord, la piste 2 le joue.

Astuce : sélectionne les deux pistes et groupe-les (Cmd + G sur Mac, Ctrl + G sur Windows) pour les déplacer ensemble.

Prends un synthé **polyphonique** : un synthé mono (comme Undertow) ne jouerait qu'une note de l'accord.

## Enregistrer les accords

Les accords arrivent sur la piste du synthé comme si tu les jouais : pour les enregistrer,
**arme la piste 2** (bouton d'armement), laisse son monitoring sur **In**, et enregistre comme d'habitude
(en Session ou en Arrangement). Le clip créé sur la piste 2 contient les accords complets.

Autres possibilités :
- garder le clip de notes simples sur la piste 1 : en lecture, il passe par Confluence et la piste 2 joue les accords
  (tu peux alors changer de gamme ou de voicing après coup) ;
- **glisser la progression des pads** depuis la zone « GLISSER VERS ABLETON » du plugin directement dans une piste
  (clip MIDI de 4 ou 8 mesures), ou l'exporter en `.mid` (boutons EXPORT, dossier **Musique / Confluence**).

## Plusieurs synthés

Plusieurs pistes peuvent écouter la même sortie de Confluence : refais l'étape 2 sur une troisième piste
pour doubler les accords avec un autre son.

## Entendre les accords sans routage

Active **PREVIEW** dans le plugin : une petite voix douce intégrée joue les accords directement sur la piste 1.
Elle est coupée par défaut (Confluence ne sort normalement que du MIDI).

## Si ça ne marche pas

- **« Confluence » n'apparaît pas dans le second menu** de la piste 2 : le plugin doit être chargé sur la piste 1,
  et il faut le format VST3 (Réglages > Plug-Ins, puis Rescan).
- **Aucun son** : le monitoring des deux pistes doit être sur **In** (ou la piste 1 armée en mode Auto).
- **Notes coincées** : bouton **PANIC** du plugin. Le Stop du transport, le bypass, un changement de preset
  et la désactivation du plugin coupent aussi toutes les notes.
- **Canal de sortie** : Ableton fusionne les canaux MIDI d'une piste à l'autre ; le réglage CANAL ne sert
  que pour un synthé externe ou multitimbral.

---
VST is a registered trademark of Steinberg Media Technologies GmbH. Ableton et Live sont des marques d'Ableton AG.
