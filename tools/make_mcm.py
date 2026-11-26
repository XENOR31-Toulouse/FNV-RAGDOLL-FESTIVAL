"""Genere le menu en jeu (MCM Extender, sans ESP) : data/MCM/NVReactiveRagdoll.json.

Usage : python tools/make_mcm.py

Les valeurs par defaut sont lues dans data/NVReactiveRagdoll.ini (une seule source de verite). Le script echoue si une
option du menu manque dans le .ini, ou si un reglage du .ini n'est pas dans le menu (sauf [Debug] et [RagdollAnim]).
Le menu ecrit dans Data/Config/NVReactiveRagdoll.ini ("saveFile"), que le plugin relit a chaud.
Textes sans accents : les polices du jeu et la lecture JSON ne les affichent pas toujours. MCM : 10 pages au plus.
"""
import configparser
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
INI = ROOT / "data" / "NVReactiveRagdoll.ini"
OUT = ROOT / "data" / "MCM" / "NVReactiveRagdoll.json"
NOT_IN_MENU = {"Debug", "RagdollAnim"}

B = "bool"
CHANCE = ("float", 0, 1, 0.05)
SECONDS = ("float", 0, 10, 0.1)

# (section, cle, titre, aide, type, [min, max, pas])
PAGES = [
    ("General", [
        ("General", "bEnabled", "Mod actif", "Active ou coupe tout le mod.", B),
        ("General", "bLivingReactions", "Reactions des vivants",
         "Desactive : les PNJ vivants restent ceux du jeu de base, tout se joue a la mort. Active : ils tressaillent, titubent, tombent.", B),
        ("General", "bElements", "Feu, electricite, explosions", "Morts propres au feu, au courant et aux explosions.", B),
        ("General", "bReactPlayer", "Le joueur reagit aussi", "Seulement avec les reactions des vivants.", B),
        ("General", "bVerbose", "Journal detaille", "Une ligne par coup dans NVReactiveRagdoll.log (utile pour regler).", B),
        ("Compatibility", "bYieldToExplosionReactions", "Laisser la place a Explosion Reactions",
         "Si Explosion Reactions est installe, il gere les vivants touches par une explosion.", B),
    ]),
    ("Agonie debout", [
        ("Balance", "bEnabled", "Agonie debout", "A sa mort, il essaie de tenir debout, fait des pas, encaisse, puis ses jambes cedent.", B),
        ("Balance", "fTimeMin", "Duree minimum", "Secondes debout, au moins.", "float", 0, 8, 0.1),
        ("Balance", "fTimeMax", "Duree maximum", "Secondes debout, au plus (tire au hasard entre les deux).", "float", 0, 8, 0.1),
        ("Balance", "fMaxDamage", "Degats maximum", "Au-dela, il tombe tout de suite.", "int", 0, 1000, 10),
        ("Balance", "bJointMuscles", "Muscles des articulations", "Comme Euphoria : chaque articulation tient la pose debout, les jambes portent le corps.", B),
        ("Balance", "fJointStrength", "Force des muscles", "Force qui ramene chaque os dans la pose debout.", "int", 0, 150, 1),
        ("Balance", "fLegStrength", "Force des jambes", "Les jambes portent le corps ; trop bas = il s'effondre.", "float", 0, 5, 0.1),
        ("Balance", "fSupport", "Coup de pouce", "Part du poids du haut du corps portee en plus des muscles.", "float", 0, 1, 0.05),
        ("Balance", "fPushMult", "Poussee de la balle qui tue", "Plus bas = il titube au lieu d'etre jete.", "float", 0, 2, 0.05),
        ("Balance", "fHitWeaken", "Balles encaissees", "Chaque balle pendant l'agonie lui retire cette part du temps restant.", "float", 0, 1, 0.05),
        ("Balance", "fArms", "Bras ecartes", "Force des bras qui s'ecartent pour l'equilibre.", "float", 0, 3, 0.1),
    ]),
    ("Agonie : muscles", [
        ("Balance", "fJointDamping", "Amortissement des muscles", "Plus haut = mouvements plus calmes.", "float", 0, 30, 0.5),
        ("Balance", "fJointMaxSpeed", "Vitesse max des muscles", "Rotation ajoutee au plus par image.", "float", 0, 30, 0.5),
        ("Balance", "fSpineStrength", "Force du dos", "Par rapport a la force des muscles.", "float", 0, 5, 0.1),
        ("Balance", "fArmStrength", "Force des bras", "Bras plus ou moins souples.", "float", 0, 5, 0.1),
        ("Balance", "fNeckStrength", "Force du cou", "Plus bas = la tete ballotte.", "float", 0, 5, 0.1),
        ("Balance", "fStepLegRelax", "Jambe qui fait le pas", "Part de sa force gardee pendant le pas.", "float", 0, 1, 0.05),
        ("Balance", "fFallPose", "Pose gardee en tombant", "0 = tombe comme un sac.", "float", 0, 1, 0.05),
    ]),
    ("Agonie : equilibre et pas", [
        ("Balance", "fDamping", "Freinage", "Freine ses glissades (ses pieds accrochent le sol).", "float", 0, 10, 0.1),
        ("Balance", "fUprightGain", "Torse tenu droit", "Force qui tient le torse droit.", "float", 0, 20, 0.5),
        ("Balance", "fSway", "Oscillation", "Amplitude du balancement du torse.", "float", 0, 1, 0.05),
        ("Balance", "fStepTime", "Duree d'un pas", "En secondes.", "float", 0.1, 1.5, 0.05),
        ("Balance", "fStepLead", "Anticipation", "Le pied va la ou il sera dans ... secondes.", "float", 0, 1.5, 0.05),
        ("Balance", "fStepPull", "Force du pas", "Force qui amene le pied.", "int", 0, 150, 1),
        ("Balance", "fStepDamping", "Amortissement du pas", "Plus haut = pas plus calmes.", "float", 0, 30, 0.5),
        ("Balance", "fStepLift", "Hauteur du pas", "Le pied se leve de ... (unites Havok).", "float", 0, 5, 0.1),
        ("Balance", "fStepMaxSpeed", "Vitesse max du pied", "Vitesse ajoutee au plus par image.", "float", 0, 20, 0.5),
    ]),
    ("Mort", [
        ("Death", "bDeferFatalPush", "Poussee au point touche", "La poussee du coup fatal part de l'os touche, des que le ragdoll commence.", B),
        ("Death", "fWaitTime", "Attente du ragdoll", "Secondes maximum entre le coup et le debut du ragdoll de mort.", "float", 0.1, 3, 0.1),
        ("Death", "fPushMult", "Force du coup fatal", "Multiplicateur de la poussee d'un coup fatal.", "float", 0, 5, 0.1),
        ("Death", "fCarryVelocity", "Elan garde", "Part de la vitesse de course gardee en mourant.", "float", 0, 2, 0.05),
        ("Death", "fVanillaImpulseMult", "Poussee du jeu de base", "Poussee que le jeu donne lui-meme aux corps (1 = jeu de base).", "float", 0, 2, 0.05),
        ("Death", "bHeadshotLimp", "Tir a la tete : s'effondre net", "Desactive : agonie debout comme les autres morts, la tete ballotte.", B),
        ("Death", "bGrabWound", "Main sur la plaie", "En mourant, une main va sur la blessure (la tete pour un tir a la tete).", B),
        ("Death", "fGrabChance", "Chance : main sur la plaie", "0 = jamais, 1 = toujours.", *CHANCE),
        ("Death", "fGrabPull", "Main sur la plaie : force", "Par rapport aux bras qui se protegent.", "float", 0, 5, 0.1),
        ("Death", "bWrithe", "Se tord au sol", "Une fois tombe, il bouge encore, de plus en plus faiblement.", B),
        ("Death", "fWritheChance", "Chance : se tord au sol", "0 = jamais, 1 = toujours.", *CHANCE),
        ("Death", "fWritheTime", "Duree au sol", "En secondes.", "float", 0, 10, 0.5),
        ("Death", "fWritheTimer", "Rythme des gestes", "Un nouveau geste toutes les ... secondes.", "float", 0.1, 3, 0.1),
        ("Death", "fWritheStrength", "Force des muscles au sol", "0 = mou.", "float", 0, 1, 0.05),
        ("Death", "fWrithePull", "Force des gestes", "Mains vers le ventre, jambes repliees.", "float", 0, 3, 0.1),
        ("Death", "fWritheMaxDamage", "Degats max", "Au-dela, il ne se tord pas.", "int", 0, 1000, 10),
    ]),
    ("Muscles", [
        ("Muscles", "bEnabled", "Muscles actifs", "Pendant une mort (ou une chute), le corps se raidit et les mains se tendent.", B),
        ("Muscles", "bOnKnockdown", "Pendant les chutes", "Muscles quand un vivant est renverse.", B),
        ("Muscles", "bOnDeath", "A la mort", "Muscles a la mort (necessaire aux facons de mourir).", B),
        ("Muscles", "fKnockdownTime", "Duree (chute)", "En secondes.", "float", 0, 5, 0.1),
        ("Muscles", "fDeathTime", "Duree (mort)", "Pendant la chute, en secondes.", "float", 0, 5, 0.1),
        ("Muscles", "fJointTension", "Raideur des articulations", "0 = mou, 0.9 = raide.", "float", 0, 0.9, 0.05),
        ("Muscles", "fNeckTension", "Raideur du cou", "Empeche la tete de fouetter.", "float", 0, 0.9, 0.05),
        ("Muscles", "fArmPull", "Force des bras", "Force qui tend les mains pour se rattraper.", "int", 0, 100, 1),
        ("Muscles", "fArmDamping", "Amortissement des bras", "Plus haut = bras plus calmes.", "float", 0, 30, 0.5),
        ("Muscles", "fArmMaxSpeed", "Vitesse max des bras", "Vitesse ajoutee au plus par image.", "float", 0, 20, 0.5),
    ]),
    ("Feu, courant, explosions", [
        ("Fire", "fDeathTime", "Feu : duree", "Bras qui tapent les flammes a la mort, en secondes.", *SECONDS),
        ("Fire", "fFlailTimer", "Feu : rythme des gestes", "Les mains changent de cible toutes les ... secondes.", "float", 0.1, 2, 0.05),
        ("Fire", "fArmPull", "Feu : force des bras", "Par rapport au reflexe de protection.", "float", 0, 5, 0.1),
        ("Fire", "fLegKick", "Feu : coups de jambe", "Vitesse des coups de jambe.", "float", 0, 20, 0.5),
        ("Fire", "fBurnTime", "Feu : duree (vivant)", "Reactions des vivants seulement.", *SECONDS),
        ("Fire", "fLivingFlail", "Feu : gestes (vivant)", "Reactions des vivants seulement.", "int", 0, 100, 1),
        ("Shock", "fShockTime", "Courant : duree", "En secondes.", *SECONDS),
        ("Shock", "fShake", "Courant : tremblement", "Force du tremblement a la mort.", "float", 0, 10, 0.1),
        ("Shock", "fLivingShake", "Courant : secousses (vivant)", "Reactions des vivants seulement.", "int", 0, 100, 1),
        ("Explosions", "fUp", "Explosion : part verticale", "Plus haut = projete plus haut.", "float", 0, 1, 0.05),
        ("Explosions", "fFlyTime", "Explosion : duree du vol", "Bras en moulinet pendant au plus ... secondes.", "float", 0, 5, 0.1),
        ("Explosions", "fFlyMinSpeed", "Explosion : vitesse de vol", "En dessous, il ne vole plus.", "float", 0, 20, 0.5),
        ("Explosions", "fWindmillSpeed", "Explosion : moulinet", "Vitesse des bras en moulinet.", "float", 0, 20, 0.5),
        ("Explosions", "fKnockdownMult", "Explosion : chute (vivant)", "Reactions des vivants seulement.", "float", 0, 5, 0.1),
    ]),
    ("Creatures", [
        ("Creatures", "bEnabled", "Creatures actives", "Les creatures meurent aussi facon Euphoria.", B),
        ("Creatures", "bMuscles", "Creatures : muscles", "Raideur et gestes a la mort.", B),
        ("Creatures", "bStagger", "Creatures : tituber (vivant)", "Reactions des vivants seulement.", B),
        ("Creatures", "bKnockdown", "Creatures : tomber (vivant)", "Reactions des vivants seulement.", B),
        ("Creatures", "bMassScaling", "Seuils selon la masse", "Les grosses creatures tiennent mieux debout (vivants).", B),
        ("Creatures", "fThresholdMult", "Creatures : seuils", "Multiplicateur des seuils des creatures (vivants).", "float", 0.2, 5, 0.1),
    ]),
    ("Force des coups", [
        ("Hits", "fBasePush", "Poussee de base", "Poussee de chaque coup, avant les degats.", "int", 0, 100, 1),
        ("Hits", "fPushPerDamage", "Poussee par point de degat", "Ajoutee a la poussee de base pour chaque point de degat.", "float", 0, 5, 0.1),
        ("Hits", "fMaxPush", "Poussee maximale", "Plafond de la poussee d'un coup.", "int", 0, 400, 5),
        ("Hits", "fMeleeMult", "Multiplicateur melee", "Les coups au corps a corps poussent plus fort.", "float", 0, 5, 0.1),
        ("Hits", "fExplosionMult", "Multiplicateur explosion", "Les explosions poussent plus fort.", "float", 0, 5, 0.1),
        ("Hits", "fCriticalMult", "Multiplicateur critique", "Les coups critiques poussent plus fort.", "float", 0, 5, 0.1),
        ("Hits", "fNeighbourShare", "Part des os voisins", "Part de la poussee donnee aux os voisins de l'os touche.", "float", 0, 1, 0.05),
        ("Hits", "fUp", "Part verticale", "0 = poussee a plat ; plus haut = vers le haut.", "float", 0, 1, 0.05),
    ]),
    ("Vivants : titube et chute", [
        ("Reactions", "bStagger", "Tituber", "Reactions des vivants : au-dessus du seuil, le PNJ titube.", B),
        ("Reactions", "bKnockdown", "Tomber", "Reactions des vivants : au-dessus du seuil, le PNJ est renverse puis se releve.", B),
        ("Reactions", "fStaggerAt", "Seuil de titubement", "Charge de coups a partir de laquelle il titube (9mm ~40).", "int", 0, 400, 5),
        ("Reactions", "fKnockdownAt", "Seuil de chute", "Charge de coups a partir de laquelle il tombe (fusil a pompe de pres ~200).", "int", 0, 600, 5),
        ("Reactions", "fChargeHalfLife", "Duree de la charge", "Temps (s) pour que la charge de coups perde la moitie de sa valeur.", "float", 0.1, 3, 0.1),
        ("Reactions", "fStaggerCooldown", "Delai entre deux titubements", "En secondes, pour le meme PNJ.", *SECONDS),
        ("Reactions", "fKnockdownForce", "Force de la chute", "Force donnee a la chute (comme PushActorAway).", "int", 0, 200, 5),
        ("Reactions", "fKnockdownForceMaxMult", "Chute : multiplicateur max", "Un coup tres fort renverse jusqu'a ce multiple.", "float", 0.5, 5, 0.1),
        ("Reactions", "fKnockdownLocalPush", "Chute : poussee sur l'os touche", "Part de la poussee ajoutee a l'os touche une fois au sol.", "float", 0, 1, 0.05),
    ]),
]


def main():
    ini = configparser.ConfigParser(inline_comment_prefixes=(";",), interpolation=None)
    ini.optionxform = str
    ini.read(INI, encoding="utf-8")

    if len(PAGES) > 10:
        raise SystemExit("MCM : 10 pages au plus")
    in_menu = set()
    submenus = {}
    for page_no, (page, options) in enumerate(PAGES, start=1):
        opts = {}
        for n, (section, key, title, mouse, kind, *scale) in enumerate(options, start=1):
            if not ini.has_option(section, key):
                raise SystemExit(f"{INI.name} : [{section}] {key} manque (valeur par defaut du menu)")
            if (section, key) in in_menu:
                raise SystemExit(f"[{section}] {key} est deux fois dans le menu")
            in_menu.add((section, key))
            value = float(ini.get(section, key).strip())
            opt = {"title": title, "enable": 1, "mouse": mouse,
                   "vars": [{"configINI": f"{section}:{key}", "default": value if kind == "float" else int(value)}]}
            if kind == B:
                opt["type"] = 5
            else:
                lo, hi, step = scale
                text = f"{step:g}"
                opt["type"] = 2 if kind == "int" else 2.5
                opt["scale"] = {"valueDecimal": 0 if kind == "int" else len(text.split(".")[1]) if "." in text else 0,
                                "valueIncrement": step, "valueMin": lo, "valueMax": hi}
            opts[str(n)] = opt
        submenus[str(page_no)] = {"listTitle": page, "pageTitle": page, "columns": 1, "enable": 1, "options": opts}
    submenus["1"]["active"] = 1

    missing = [f"[{s}] {k}" for s in ini.sections() if s not in NOT_IN_MENU for k in ini[s] if (s, k) not in in_menu]
    if missing:
        raise SystemExit("Reglages du .ini absents du menu : " + ", ".join(missing))

    menu = {
        "modName": "NVReactiveRagdoll",
        "displayName": "NV Reactive Ragdoll",
        "description": "Morts facon Euphoria. Les changements s'appliquent a la fermeture du menu pause.",
        "saveFile": "NVReactiveRagdoll.ini",
        "minMCMVersion": 1.0,
        "minModVersion": 1.0,
        "submenus": submenus,
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(menu, indent=4, ensure_ascii=True) + "\n", encoding="utf-8")
    print(f"{OUT.relative_to(ROOT)} : {len(PAGES)} pages, {len(in_menu)} options (tous les reglages du .ini)")


if __name__ == "__main__":
    main()
