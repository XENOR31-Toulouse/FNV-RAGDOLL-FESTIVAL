"""Genere le menu en jeu (MCM Extender, sans ESP) : data/MCM/NVReactiveRagdoll.json.

Usage : python tools/make_mcm.py

Les valeurs par defaut sont lues dans data/NVReactiveRagdoll.ini (une seule source de verite) : une option du menu
dont la cle manque dans le .ini fait echouer le script. Le menu ecrit dans Data/Config/NVReactiveRagdoll.ini
("saveFile"), que le plugin relit a chaud.
Textes sans accents : les polices du jeu et la lecture JSON ne les affichent pas toujours.
"""
import configparser
import json
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
INI = ROOT / "data" / "NVReactiveRagdoll.ini"
OUT = ROOT / "data" / "MCM" / "NVReactiveRagdoll.json"

# (section, cle, titre, aide, type, min, max, pas) ; type : "bool", "int", "float"
PAGES = [
    ("General", [
        ("General", "bEnabled", "Mod actif", "Active ou coupe toutes les reactions.", "bool"),
        ("General", "bLivingReactions", "Reactions des vivants",
         "Desactive : les PNJ vivants restent ceux du jeu de base, tout se joue a la mort. Active : ils tressaillent, titubent, tombent.", "bool"),
        ("General", "bElements", "Feu, electricite, explosions", "Reactions propres au feu, au courant et aux explosions.", "bool"),
        ("General", "bReactPlayer", "Le joueur reagit aussi", "Applique les poussees au joueur (jamais de chute ni de titubement).", "bool"),
        ("General", "bVerbose", "Journal detaille", "Une ligne par coup dans NVReactiveRagdoll.log (utile pour regler).", "bool"),
        ("Compatibility", "bYieldToExplosionReactions", "Laisser la place a Explosion Reactions",
         "Si Explosion Reactions est installe, il gere les vivants touches par une explosion.", "bool"),
    ]),
    ("Coups", [
        ("Hits", "fBasePush", "Poussee de base", "Poussee de chaque coup, avant les degats.", "int", 0, 100, 1),
        ("Hits", "fPushPerDamage", "Poussee par point de degat", "Ajoutee a la poussee de base pour chaque point de degat.", "float", 0, 5, 0.1),
        ("Hits", "fMaxPush", "Poussee maximale", "Plafond de la poussee physique d'un coup.", "int", 0, 400, 5),
        ("Hits", "fMeleeMult", "Multiplicateur melee", "Les coups au corps a corps poussent plus fort.", "float", 0, 5, 0.1),
        ("Hits", "fExplosionMult", "Multiplicateur explosion", "Les explosions poussent plus fort.", "float", 0, 5, 0.1),
        ("Hits", "fCriticalMult", "Multiplicateur critique", "Les coups critiques poussent plus fort.", "float", 0, 5, 0.1),
        ("Hits", "fNeighbourShare", "Part des os voisins", "Part de la poussee donnee aux os voisins de l'os touche.", "float", 0, 1, 0.05),
        ("Hits", "fUp", "Part verticale", "0 = poussee a plat ; plus haut = vers le haut.", "float", 0, 1, 0.05),
    ]),
    ("Vivants : titube et chute", [
        ("Reactions", "bStagger", "Tituber", "Au-dessus du seuil, le PNJ joue l'animation de titubement.", "bool"),
        ("Reactions", "bKnockdown", "Tomber", "Au-dessus du seuil, le PNJ est renverse puis se releve.", "bool"),
        ("Reactions", "fStaggerAt", "Seuil de titubement", "Charge de coups a partir de laquelle il titube (9mm ~40).", "int", 0, 400, 5),
        ("Reactions", "fKnockdownAt", "Seuil de chute", "Charge de coups a partir de laquelle il tombe (fusil a pompe de pres ~200).", "int", 0, 600, 5),
        ("Reactions", "fChargeHalfLife", "Duree de la charge", "Temps (s) pour que la charge de coups perde la moitie de sa valeur.", "float", 0.1, 3, 0.1),
        ("Reactions", "fStaggerCooldown", "Delai entre deux titubements", "En secondes, pour le meme PNJ.", "float", 0, 10, 0.1),
        ("Reactions", "fKnockdownForce", "Force de la chute", "Force donnee a la chute (comme la commande PushActorAway).", "int", 0, 200, 5),
        ("Reactions", "fKnockdownForceMaxMult", "Chute : multiplicateur maximal", "Un coup tres fort renverse jusqu'a ce multiple.", "float", 0.5, 5, 0.1),
        ("Reactions", "fKnockdownLocalPush", "Chute : poussee sur l'os touche", "Part de la poussee du coup ajoutee a l'os touche une fois en ragdoll.", "float", 0, 1, 0.05),
    ]),
    ("Mort", [
        ("Death", "bDeferFatalPush", "Poussee au point touche", "La poussee du coup fatal part de l'os touche, des que le ragdoll commence.", "bool"),
        ("Death", "fPushMult", "Force du coup fatal", "Multiplicateur de la poussee d'un coup fatal.", "float", 0, 5, 0.1),
        ("Death", "fCarryVelocity", "Elan garde", "Part de la vitesse de course gardee en tombant ou en mourant.", "float", 0, 2, 0.05),
        ("Death", "fVanillaImpulseMult", "Poussee du jeu de base", "Poussee que le jeu donne lui-meme aux corps (1 = jeu de base).", "float", 0, 2, 0.05),
        ("Death", "bGrabWound", "Main sur la plaie", "En mourant, une main va sur la blessure.", "bool"),
        ("Death", "fGrabChance", "Chance : main sur la plaie", "0 = jamais, 1 = toujours.", "float", 0, 1, 0.05),
        ("Death", "bStanding", "Reste debout un instant", "Tue d'une seule balle, un PNJ immobile reste debout puis s'effondre.", "bool"),
        ("Death", "fStandingChance", "Chance : reste debout", "0 = jamais, 1 = toujours.", "float", 0, 1, 0.05),
        ("Death", "fStandingTime", "Duree debout", "En secondes.", "float", 0, 3, 0.1),
        ("Death", "bHeadshotLimp", "Tir a la tete : s'effondre net", "Pas de gestes apres un tir mortel a la tete.", "bool"),
        ("Death", "bStumble", "Titube en mourant", "Il fait quelques pas dans le sens du coup avant que ses jambes cedent.", "bool"),
        ("Death", "fStumbleChance", "Chance : titube", "0 = jamais, 1 = toujours.", "float", 0, 1, 0.05),
        ("Death", "fStumbleTime", "Duree du titubement", "En secondes.", "float", 0, 3, 0.1),
        ("Death", "bWrithe", "Se tord au sol", "Une fois tombe, il bouge encore un peu, de plus en plus faiblement.", "bool"),
        ("Death", "fWritheChance", "Chance : se tord au sol", "0 = jamais, 1 = toujours.", "float", 0, 1, 0.05),
        ("Death", "fWritheTime", "Duree au sol", "En secondes.", "float", 0, 10, 0.5),
    ]),
    ("Muscles", [
        ("Muscles", "bEnabled", "Muscles actifs", "Pendant une chute ou une mort, le corps se raidit et les mains se tendent.", "bool"),
        ("Muscles", "bOnKnockdown", "Pendant les chutes", "Muscles quand le PNJ est renverse.", "bool"),
        ("Muscles", "bOnDeath", "A la mort", "Muscles qui lachent peu a peu a la mort.", "bool"),
        ("Muscles", "fKnockdownTime", "Duree (chute)", "En secondes.", "float", 0, 5, 0.1),
        ("Muscles", "fDeathTime", "Duree (mort)", "En secondes.", "float", 0, 5, 0.1),
        ("Muscles", "fJointTension", "Raideur des articulations", "0 = mou, 0.9 = raide.", "float", 0, 0.9, 0.05),
        ("Muscles", "fNeckTension", "Raideur du cou", "Empeche la tete de fouetter.", "float", 0, 0.9, 0.05),
        ("Muscles", "fArmPull", "Force des bras", "Force qui tend les mains pour se rattraper.", "int", 0, 100, 1),
        ("Muscles", "fArmDamping", "Amortissement des bras", "Plus haut = bras plus calmes.", "float", 0, 30, 0.5),
    ]),
    ("Creatures", [
        ("Creatures", "bEnabled", "Creatures actives", "Les creatures reagissent aussi aux coups.", "bool"),
        ("Creatures", "bStagger", "Creatures : tituber", "Les creatures peuvent tituber.", "bool"),
        ("Creatures", "bKnockdown", "Creatures : tomber", "Les creatures peuvent etre renversees.", "bool"),
        ("Creatures", "bMuscles", "Creatures : muscles", "Raideur et gestes pendant la chute et la mort.", "bool"),
        ("Creatures", "bMassScaling", "Seuils selon la masse", "Les grosses creatures (et super mutants) tiennent mieux debout.", "bool"),
        ("Creatures", "fThresholdMult", "Creatures : seuils", "Multiplicateur des seuils de titubement et de chute des creatures.", "float", 0.2, 5, 0.1),
    ]),
    ("Feu, courant, explosions", [
        ("Fire", "fBurnTime", "Feu : duree (vivant)", "En secondes apres le dernier coup de feu.", "float", 0, 10, 0.1),
        ("Fire", "fDeathTime", "Feu : duree (mort ou chute)", "Bras qui tapent les flammes, en secondes.", "float", 0, 10, 0.1),
        ("Fire", "fFlailTimer", "Feu : rythme des gestes", "Les mains changent de cible toutes les ... secondes.", "float", 0.1, 2, 0.05),
        ("Fire", "fLegKick", "Feu : coups de jambe", "Vitesse des coups de jambe.", "float", 0, 20, 0.5),
        ("Shock", "fShockTime", "Courant : duree", "En secondes.", "float", 0, 10, 0.1),
        ("Shock", "fShake", "Courant : tremblement", "Force du tremblement (mort ou chute).", "float", 0, 10, 0.1),
        ("Explosions", "fUp", "Explosion : part verticale", "Plus haut = projete plus haut.", "float", 0, 1, 0.05),
        ("Explosions", "fKnockdownMult", "Explosion : force de chute", "Multiplicateur de la chute causee par une explosion.", "float", 0, 5, 0.1),
        ("Explosions", "fFlyTime", "Explosion : duree du vol", "Bras en moulinet pendant au plus ... secondes.", "float", 0, 5, 0.1),
        ("Explosions", "fWindmillSpeed", "Explosion : moulinet", "Vitesse des bras en moulinet.", "float", 0, 20, 0.5),
    ]),
]


def default_of(ini, section, key):
    if not ini.has_option(section, key):
        raise SystemExit(f"{INI.name} : [{section}] {key} manque (valeur par defaut du menu)")
    return float(ini.get(section, key).split(";")[0].strip())


def main():
    ini = configparser.ConfigParser(inline_comment_prefixes=(";",))
    ini.optionxform = str
    ini.read(INI, encoding="utf-8")

    submenus = {}
    for page_no, (page, options) in enumerate(PAGES, start=1):
        opts = {}
        for n, (section, key, title, mouse, kind, *scale) in enumerate(options, start=1):
            value = default_of(ini, section, key)
            opt = {"title": title, "enable": 1, "mouse": mouse,
                   "vars": [{"configINI": f"{section}:{key}", "default": int(value) if kind != "float" else value}]}
            if kind == "bool":
                opt["type"] = 5
            else:
                lo, hi, step = scale
                decimals = 0 if kind == "int" else len(f"{step:g}".split(".")[1]) if "." in f"{step:g}" else 0
                opt["type"] = 2 if kind == "int" else 2.5
                opt["scale"] = {"valueDecimal": decimals, "valueIncrement": step, "valueMin": lo, "valueMax": hi}
            opts[str(n)] = opt
        submenus[str(page_no)] = {"listTitle": page, "pageTitle": page, "columns": 1, "enable": 1, "options": opts}
        if page_no == 1:
            submenus["1"]["active"] = 1

    menu = {
        "modName": "NVReactiveRagdoll",
        "displayName": "NV Reactive Ragdoll",
        "description": "Reactions physiques des PNJ aux coups. Les changements s'appliquent a la fermeture du menu.",
        "saveFile": "NVReactiveRagdoll.ini",
        "minMCMVersion": 1.0,
        "minModVersion": 1.0,
        "requirements": [{"type": "file", "file": "NVSE/Plugins/NVReactiveRagdoll.dll"}],
        "submenus": submenus,
    }
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(menu, indent=4, ensure_ascii=True) + "\n", encoding="utf-8")
    count = sum(len(o) for _, o in PAGES)
    print(f"{OUT.relative_to(ROOT)} : {len(PAGES)} pages, {count} options")


if __name__ == "__main__":
    main()
