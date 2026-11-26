# NV Reactive Ragdoll

Plugin xNVSE pour Fallout New Vegas : les PNJ **vivants** réagissent physiquement aux coups. Le mod est autonome : il remplace le mod Ragdolls (ne pas les activer ensemble). Conception : [docs/00-conception.md](docs/00-conception.md).

État : étapes 0 à 6 écrites et compilées, **pas encore testées en jeu**. Un PNJ touché tressaille, titube ou tombe selon la force des coups reçus ; à la mort, la poussée part de l'endroit touché et le corps garde son élan. Pendant une chute ou une mort, ses articulations se raidissent et ses mains se tendent pour se rattraper. En mourant, il peut porter la main à sa blessure, ou rester debout un instant avant que ses jambes cèdent. Dans le feu, il bat des bras et des jambes ; électrocuté, il tremble ; projeté par une explosion, ses bras tournent en moulinet pendant le vol.

## Prérequis

- Fallout New Vegas 1.4.0.525, xNVSE 6.1 ou plus récent (6.4.9 installé ici)
- NV Compatibility Skeleton (squelette humain avec ses 18 corps physiques)
- Pour compiler : Visual Studio 2022 avec le C++ (déjà installé)

## Compiler

```
build.cmd
```

Le script produit `build\NVReactiveRagdoll.dll` (32 bits), le copie avec le .ini dans `dist\Data\NVSE\Plugins\`, et dans le mod Mod Organizer `NV Reactive Ragdoll (dev)` s'il existe. Le .ini du mod MO2 n'est jamais écrasé : tes réglages restent.

## Tester en jeu

1. Dans Mod Organizer : active **NV Reactive Ragdoll (dev)** et désactive **Ragdolls**.
2. Lance le jeu, charge une partie près de PNJ.
3. Vise un PNJ et appuie sur **F8** : la liste de ses corps physiques s'écrit dans le journal.
4. Appuie sur **F7** : le PNJ visé reçoit une poussée au torse, puis ses corps sont listés à nouveau.
5. Tire sur des PNJ : chaque coup ajoute une ligne au journal, avec la charge accumulée et la réaction choisie (tressaille, titube, tombe, meurt). Essaie un pistolet, une rafale, un fusil à pompe de près et une attaque puissante en mêlée. Tue-en aussi quelques-uns : un coup fatal est marqué « FATAL », puis une ligne « Mort de … » dit si la poussée a été appliquée au ragdoll de mort, et après combien de millisecondes.
6. Quitte le jeu et lis `NVReactiveRagdoll.log` dans le dossier du jeu (`E:\SteamLibrary\steamapps\common\Fallout New Vegas\`).

Ce qu'il faut regarder dans le journal : la colonne « anime(keyframed) » ou « dynamique ». Le moteur ne peut pousser que des corps dynamiques. Si tous les corps d'un PNJ vivant restent « anime » même juste après un coup, c'est que le Ragdoll Feedback natif ne suffit pas, et on passe au plan B de la conception.

## Réglages

`NVSE\Plugins\NVReactiveRagdoll.ini`, relu à chaque chargement de partie. La section `[RagdollAnim]` écrit les réglages natifs du jeu qui pilotent le mélange animation / physique. Le journal affiche leurs valeurs avant et après au démarrage.

## Sources et licence

GPL-3.0. Les offsets du jeu viennent de JIP LN NVSE (GPL-3) et de xNVSE. Idées reprises de Skyrim Delirium (GPL-3), Precision et PLANCK. Les sources de référence sont clonées dans `deps\` (hors Git).
