# NV Reactive Ragdoll

Plugin xNVSE pour Fallout New Vegas : les PNJ **vivants** réagissent physiquement aux coups. Le mod est autonome : il remplace le mod Ragdolls (ne pas les activer ensemble). Conception : [docs/00-conception.md](docs/00-conception.md).

État : version 0.6.0-alpha, **pas encore testée en jeu**. Les PNJ vivants restent exactement ceux du jeu de base : tout se joue à leur mort, façon Euphoria, exagérée. Presque chaque mort (tir à la tête compris) commence par une **agonie debout** : le PNJ essaie de tenir debout, cherche son équilibre et fait des pas pour se rattraper quand la balle ou son élan le déséquilibrent, écarte les bras, porte souvent la main à sa blessure, encaisse les balles suivantes, puis ses jambes cèdent ; il tombe en se protégeant et se tord au sol. Explosions, feu et courant ont leurs propres morts ; les créatures aussi. Les réactions des vivants (étapes 2 et 3) existent encore, désactivées (`bLivingReactions`). Étapes 0 à 8 écrites et compilées, **pas encore testées en jeu**. Un PNJ touché tressaille, titube ou tombe selon la force des coups reçus ; à la mort, la poussée part de l'endroit touché et le corps garde son élan. Pendant une chute ou une mort, ses articulations se raidissent et ses mains se tendent pour se rattraper. En mourant, il peut porter la main à sa blessure, ou rester debout un instant avant que ses jambes cèdent. Dans le feu, il bat des bras et des jambes ; électrocuté, il tremble ; projeté par une explosion, ses bras tournent en moulinet pendant le vol.

## Prérequis

- Fallout New Vegas 1.4.0.525, xNVSE 6.1 ou plus récent (6.4.9 installé ici)
- NV Compatibility Skeleton (squelette humain avec ses 18 corps physiques)
- Pour compiler : Visual Studio 2022 avec le C++ (déjà installé)

## Compiler

```
build.cmd
```

Le script produit `build\NVReactiveRagdoll.dll` (32 bits), le copie avec le .ini dans `dist\Data\NVSE\Plugins\`, et dans le mod Mod Organizer `NV Reactive Ragdoll (dev)` s'il existe. Le .ini du mod MO2 n'est jamais écrasé : tes réglages restent.

## Faire le paquet à publier

```
package.cmd
```

Compile, puis crée `dist\NVReactiveRagdoll-<version>.zip` (racine = dossier Data : `NVSE\Plugins\` avec la DLL et la licence, `Config\` avec le .ini, `MCM\` avec le menu). La version vient de `kVersionName` dans `src\main.cpp`. La description pour Nexus est dans [docs/nexus-description.bbcode](docs/nexus-description.bbcode).

## Compatibilité

- **Ragdolls** : à désactiver, ce mod le remplace. Le journal prévient s'il est actif.
- **Explosion Reactions** : détecté ; les PNJ vivants touchés par une explosion lui sont laissés (`bYieldToExplosionReactions` dans `[Compatibility]`).
- **Awesome / Dramatic Staggering** : compatibles, ils ne remplacent que les animations de coup du jeu.
- **Squelette** : au premier coup sur un humain, le journal vérifie qu'il porte bien ses corps physiques (18 avec NV Compatibility Skeleton).

## Tester en jeu

1. Dans Mod Organizer : active **NV Reactive Ragdoll (dev)** et désactive **Ragdolls**.
2. Lance le jeu, charge une partie près de PNJ.
3. Vise un PNJ et appuie sur **F8** : la liste de ses corps physiques s'écrit dans le journal.
4. Appuie sur **F7** : le PNJ visé reçoit une poussée au torse, puis ses corps sont listés à nouveau.
5. Tire sur des PNJ : chaque coup ajoute une ligne au journal, avec la charge accumulée et la réaction choisie (tressaille, titube, tombe, meurt). Essaie un pistolet, une rafale, un fusil à pompe de près et une attaque puissante en mêlée. Tue-en aussi quelques-uns : un coup fatal est marqué « FATAL », puis une ligne « Mort de … » dit si la poussée a été appliquée au ragdoll de mort, et après combien de millisecondes.
6. Quitte le jeu et lis `NVReactiveRagdoll.log` dans le dossier du jeu (`E:\SteamLibrary\steamapps\common\Fallout New Vegas\`).

Ce qu'il faut regarder dans le journal : la colonne « anime(keyframed) » ou « dynamique ». Le moteur ne peut pousser que des corps dynamiques. Si tous les corps d'un PNJ vivant restent « anime » même juste après un coup, c'est que le Ragdoll Feedback natif ne suffit pas, et on passe au plan B de la conception.

## Réglages

**En jeu** : menu pause, *Mod Configuration*, page **NV Reactive Ragdoll** (10 pages, 86 options : tous les réglages du .ini). Il faut [The Mod Configuration Menu](https://www.nexusmods.com/newvegas/mods/42507) et [MCM Extender](https://www.nexusmods.com/newvegas/mods/93642) ; sans eux, le mod marche quand même, réglable par le fichier. Les changements s'appliquent à la fermeture du menu pause.

**Fichier** : `Data\Config\NVReactiveRagdoll.ini` (c'est là que le menu écrit), relu à chaque chargement de partie et dès qu'il change. Un .ini à l'ancienne place (`NVSE\Plugins\`, version 0.1.0) est encore lu s'il n'y en a pas dans `Config\`. La section `[RagdollAnim]` écrit les réglages natifs du jeu qui pilotent le mélange animation / physique ; le journal affiche leurs valeurs avant et après.

Le menu est généré par `python tools\make_mcm.py` à partir du .ini (valeurs par défaut) : à relancer après avoir ajouté un réglage au menu.

## Sources et licence

GPL-3.0. Les offsets du jeu viennent de JIP LN NVSE (GPL-3) et de xNVSE. Idées reprises de Skyrim Delirium (GPL-3), Precision et PLANCK. Les sources de référence sont clonées dans `deps\` (hors Git).
