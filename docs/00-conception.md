# NV Reactive Ragdoll : conception

Nom de travail. Objectif : les PNJ **vivants** réagissent physiquement aux coups (tressaillement, titube, se rattrape, tombe, se protège), au lieu de jouer seulement une animation ou de ne réagir qu'à la mort.

## 1. Ce qui existe déjà dans ta liste de mods (New Vegas)

| Mod | Ce qu'il fait | Comment |
|---|---|---|
| **Ragdolls** (MadAce, v5.3.9) | Réactions **à la mort** : saisir la plaie, se tenir debout un instant, battre des bras en feu ou dans une explosion, élan conservé | Scripts JIP : `SetAngularVelocity` / `SetLinearVelocity` sur les nœuds du ragdoll, `SetOnRagdollEventHandler`, nœuds `... Grab`. Réactive aussi le « Ragdoll Feedback » vanilla (`bRagdollFeedback`) |
| **NV Compatibility Skeleton** v13 | Squelette commun ; **intègre déjà le patch Ragdolls** (nœuds `Bip01 ... Grab`) | 18 `bhkRigidBody` (capsules), 11 `bhkRagdollConstraint`, 6 `bhkLimitedHingeConstraint`, 18 `bhkBlendCollisionObject` + 21 `bhkBlendController` |
| Awesome / Dramatic Staggering, Explosion Reactions | Réactions **animées** (kf) | Animations, pas de physique |

Ce qui manque, c'est la physique **sur les vivants**, et aucun plugin NVSE ne le fait aujourd'hui.

**Choix : le mod est autonome.** Il ne dépend pas du mod Ragdolls et le remplace : il gère aussi les réactions à la mort. Les deux ne doivent pas être actifs ensemble (le plugin le détecte et prévient dans le journal). En retirant Ragdolls, on perd trois choses qu'il faudra refaire nous-mêmes :
- la réactivation du « Ragdoll Feedback » du jeu (`bRagdollFeedback`) ;
- les squelettes de créatures retouchés (contraintes plus réalistes) : sans eux, les créatures reviennent aux ragdolls vanilla ;
- les `death.kf` par créature.

Le squelette humain NVCS garde ses nœuds `... Grab` : ce sont de simples nœuds, on peut s'en servir ou les ignorer.

Point clé trouvé dans le squelette : chaque os physique est un `bhkBlendCollisionObject`. C'est le mécanisme du moteur qui **mélange l'animation et le ragdoll** os par os. C'est l'équivalent New Vegas du « suivre l'animation » de Skyrim, et la base technique du mod.

## 2. Inspirations (Skyrim, récents)

- **Skyrim Delirium** (2026, GPL-3, source : github.com/YoucefNabil/skyrim-delirium). Réactions façon GTA IV / Euphoria. Le plus complet, et lisible.
  - Le ragdoll du jeu reste le corps (masse, gravité, collisions). Un moteur de comportement calcule des « muscles » et les applique en **couples (torques) sur chaque articulation**, répartis entre les deux corps selon leur inertie. Pas de moteurs Havok : de simples impulsions angulaires à chaque pas physique.
  - Machine d'états : coup → tressaille (petit coup) / fait un pas pour se rattraper (moyen) / tombe (au-delà) → au sol (se tord) → relevé par l'animation du jeu.
  - Réglages par situation en **N·s** (impulsion) : `[Melee]` (puissance, bash, furtif, bloqué, poids de l'arme en racine carrée), `[Arrows]`, `[Explosions]` (vol, bras en moulinet, réception), `[Stagger]` (remplace le stagger du jeu par une poussée), `[Bump]`, `[Falls]`, `[Down]`.
  - Outil de test : une touche pousse le PNJ sous le viseur ; une autre rend tous les corps au jeu. Rechargement à chaud du .ini.
  - Pièges notés dans leur code : friction des articulations du jeu trop forte (mettre à 0 pendant la réaction, la remettre après) ; le nœud racine saute en passant en ragdoll (le lisser) ; garder la vitesse du PNJ en entrant en réaction.
- **Precision** (Skyrim) : impulsion sur le membre touché **et les membres voisins**, le PNJ tressaille dans le sens du coup.
- **PLANCK** (Skyrim VR) : le ragdoll est toujours présent et suit la pose animée ; les réactions viennent d'impulsions, pas d'animations.

Les pages Nexus refusent la lecture automatique : les détails de Precision et PLANCK viennent des résumés de recherche. Delirium a été lu dans son code.

**Licence :** reprendre des idées est libre. Copier du code de Delirium oblige à publier notre mod sous GPL-3 avec ses sources.

## 3. Architecture proposée

Un **plugin NVSE en C++** (xNVSE, Visual Studio 2022, **32 bits** : FalloutNV.exe est x86), plus un .ini. Pas d'ESP au départ.

Avantage par rapport à Skyrim : il n'y a qu'une seule version de l'exécutable (1.4.0.525), donc des adresses fixes. Les définitions des classes Havok du jeu (`bhkRagdollController`, `bhkRigidBody`, `hkpRigidBody`…) sont dans les sources publiques de JIP LN NVSE et xNVSE.

```
Coup reçu (événement OnHit + point d'impact du projectile)
   │  force = dégâts × arme × type (balle, mêlée, explosion) en N·s
   ▼
Classement  ──► tressaille | titube | tombe
   │
   ├─ tressaille : os touché + voisins passent en partie « dynamiques » (blend),
   │               impulsion au point d'impact, retour à l'animation en ~0,3 s
   ├─ titube     : idem + animation de stagger dans la bonne direction (kNVSE),
   │               le haut du corps reste physique
   ├─ tombe      : ragdoll complet avec la vitesse du PNJ, « muscles » (couples)
                   pour protéger la tête et tendre les bras, puis relevé du jeu
   └─ mort       : même chaîne que la chute (pas de mod Ragdolls) : la réaction
                   en cours continue dans le ragdoll de mort, muscles qui
                   lâchent progressivement, main vers la plaie, élan conservé
```

Modules (même découpage que Delirium) : `hooks` (coups, impacts, passage en ragdoll), `body` (accès aux 18 corps du squelette NVCS par nom d'os, blend, impulsions, couples), `reactions` (machine d'états), `settings` (.ini relu à chaud), `debug` (touche « pousser le PNJ visé », commande console, journal).

## 4. Étapes

| # | Étape | Terminé quand |
|---|---|---|
| 0 | Projet C++ NVSE qui se charge, journal, touche de test | `nvse.log` montre le plugin, la touche écrit dans le journal |
| 1 | Lire les 18 corps physiques d'un PNJ vivant et leur blend | Le journal liste os, masse, état clé/dynamique du PNJ visé |
| 2 | **Tressaillement** : impulsion sur l'os touché + voisins | Une balle dans l'épaule fait reculer l'épaule, puis l'animation reprend |
| 3 | Titube / tombe selon la force, avec la vitesse conservée | Fusil à pompe à bout portant = chute ; pistolet = tressaille |
| 4 | « Muscles » pendant la chute (bras de protection, tête) | Le PNJ se réceptionne au lieu de tomber comme un sac |
| 5 | **Mort** : la réaction continue dans le ragdoll de mort, élan conservé, force au point d'impact | Le mod Ragdolls désactivé, les morts restent aussi vivantes qu'avec lui |
| 6 | Explosions, feu, électricité | Réglages par type dans le .ini |
| 7 | Créatures (squelettes et contraintes propres au mod) | Les créatures principales réagissent sans le mod Ragdolls |
| 8 | Compatibilité : staggers animés, NVCS, TTW ; avertissement si Ragdolls est actif | Aucun conflit dans la liste de mods actuelle |

### Avancement (26/11/2026)

- Étapes 0 à 2 écrites et compilées (`src/main.cpp`), **pas encore testées en jeu**.
- Découverte : le moteur a un système natif de mélange animation / ragdoll, la section `[RagdollAnim]` de Fallout.ini (`bRagdollFeedback`, `fPositionGain`, `fVelocityGain`, `fHierarchyGain`, `fSnapGain`…). Le mod Ragdolls ne faisait qu'activer `bRagdollFeedback`. Notre plugin l'active et permet de régler les gains depuis son .ini.
- Début de l'étape 5 : au coup fatal, si le PNJ est encore animé, la poussée attend son ragdoll de mort (0,6 s au plus) et s'applique au même os, au même point, dans le même sens. Elle remplace la « force locationnelle » de Ragdolls. La poussée de mort propre à chaque arme (vanilla) s'ajoute encore : à revoir après le test.
- Étape 3 écrite : chaque coup ajoute sa force à une « charge » par PNJ (demi-vie 0,4 s ; un fusil à pompe compte tous ses plombs). Sous `fStaggerAt` le PNJ tressaille, au-dessus il joue l'animation `Stagger` du jeu, à partir de `fKnockdownAt` il tombe via la fonction du jeu derrière `PushActorAway` (le jeu le fait se relever), puis la poussée au point touché et son élan de course sont ajoutés à son ragdoll. L'élan est aussi ajouté au ragdoll de mort.
- Étape 4 écrite : pendant une chute (1,5 s) ou une mort (0,8 s, muscles qui lâchent), les articulations se raidissent (rotation relative freinée, cou plus raide) et chaque main est tirée par un ressort amorti vers un point devant et sous l'épaule, dans le sens de la chute ; la force opposée va sur la poitrine. Tout est exprimé dans l'espace du monde, car les axes locaux des os ne sont pas documentés. Les muscles s'arrêtent quand le PNJ commence à se relever.
- Étape 5 écrite (façon de mourir, choisie au coup fatal) : **main sur la plaie** (60 %, la main opposée à un bras touché ; le point visé suit l'os touché, exprimé dans son repère) et **reste debout** (50 %, PNJ immobile tué d'une seule balle : poitrine et bassin portés contre la gravité du monde Havok pendant 0,8 s, de moins en moins, puis les jambes cèdent ; la balle pousse alors 3 fois moins). La poussée vanilla (`fGameplayImpulseMultBiped`, dont la « kill impulse » des armes) passe à 0,5 comme dans Ragdolls. Reste pour plus tard : feu et électricité (étape 6).
- Étape 6 écrite. L'élément est reconnu sans liste d'armes : champ « résistance » de l'arme (feu `0x34`, électricité `0x3A`, IEM `0x3D`) ou projectile de type flamme. **Feu** : vivant en feu 2,5 s (mains qui battent si le corps est physique), mort ou chute : mains qui tapent les flammes et jambes qui donnent des coups, cible changée toutes les 0,3 s. **Électricité** : tremblement de chaque os, muscles contractés. **Explosions** : poussée plus verticale, chute plus forte, et tant que le corps vole, bras en moulinet et jambes qui pédalent, puis mains tendues pour la réception. Le premier coup de feu ou de courant fait tituber.
- Le test en jeu (F8 / F7 / tirs) doit dire si les corps d'un PNJ vivant deviennent dynamiques au moment d'un coup.

## 5. Risques

- Le blend os par os sur un PNJ vivant est l'hypothèse centrale. Elle est à **prouver en étape 1-2** avant d'aller plus loin. Plan B : ragdoll complet très court + impulsions (plus brutal, mais faisable à coup sûr).
- La version de Havok de New Vegas est ancienne : les moteurs de contrainte ne sont pas garantis. On utilise donc des couples, comme Delirium.
- Les créatures ont chacune leur squelette : on commence par les humains (squelette `_Male`).
