# Brancher Ebb dans Ableton

Ebb se pose comme n'importe quel effet audio, sur la piste qui doit pomper (basse, pad, bus de synthés, retour de reverb...).
Dans le navigateur : **Plug-Ins > VST3 > GRVTY Records > Ebb** (prends de préférence la version VST3 ; l'AU marche aussi).

Le menu **DÉCLENCHEMENT** choisit ce qui lance la courbe.

## SYNC (par défaut) : rien à brancher

La courbe suit le tempo et la position d'Ableton, calée sur les temps et les mesures.
Lance la lecture : le son pompe sur chaque temps (avec **Durée** = 1/4).

- Transport arrêté : la courbe continue au dernier tempo connu, pour régler Ebb sur un son tenu.
- **Nudge** (-20 à +20 ms) : décale la courbe, pour un kick qui n'attaque pas pile sur le temps
  (sample avec un peu de silence au début, kick décalé à la main, swing).

## AUDIO : le kick relance la courbe (sidechain)

1. Mets Ebb en mode **AUDIO**.
2. Dans l'en-tête d'Ebb (dans la chaîne de la piste), ouvre la section **Sidechain**
   (le bouton qui la déplie, à côté du nom du plugin).
3. Active-la, puis dans **Audio From** choisis la piste du kick. Dans le menu juste en dessous, prends **Post FX**
   (le fader du kick ne changera alors rien au déclenchement).
4. Règle le **Seuil** : un peu en dessous du niveau du kick. Le seuil est le niveau des graves du sidechain
   (filtré à 300 Hz) ; avec un kick à -6 dB, -20 dB marche bien.

Ebb réagit à l'**attaque** du kick, pas à son niveau : une basse tenue ou des charleys sur la même piste ne relancent rien.
Délai mesuré entre le début du kick et le départ de la courbe : 1 à 2,5 ms.

**Kick fantôme** : pour faire pomper sans entendre le kick (ou avec un rythme différent), crée une piste audio avec un kick,
baisse son fader à fond, et choisis-la dans Audio From (en Post FX ou Pre FX, pas Post Mixer).

Si le menu indique « choisis la piste du kick dans Sidechain » : la section Sidechain n'est pas active ou aucune piste n'est choisie.

## MIDI : chaque note relance la courbe

1. Mets Ebb en mode **MIDI**.
2. Crée une **piste MIDI** (sans instrument) et écris des notes là où le son doit pomper
   (sur chaque temps, ou le rythme exact du kick). La hauteur et la durée des notes ne comptent pas.
3. Dans **MIDI To** de cette piste, choisis la piste qui porte Ebb, puis dans le menu juste en dessous, choisis **Ebb**.
4. Lance la lecture. Pour jouer en direct au clavier, arme la piste MIDI (ou mets son monitoring sur In).

La courbe démarre à l'échantillon exact de chaque note et dure la valeur de **Durée** au tempo du projet.

## Si ça ne marche pas

- **Ebb n'apparaît pas** : Réglages > Plug-Ins, active « Utiliser les dossiers système VST3 » puis **Rescan**.
- **Rien ne pompe** : vérifie **Amount** (à 0 %, le son passe intact) et le mode de déclenchement.
- **SYNC qui ne suit pas la lecture** : vérifie que le transport d'Ableton tourne (sinon la courbe tourne au dernier tempo connu).
- **AUDIO qui ne déclenche pas** : baisse le Seuil ; vérifie Audio From et le menu Post FX.
- **MIDI qui ne déclenche pas** : la piste MIDI doit sortir vers la piste d'Ebb ET vers « Ebb » dans le second menu.
