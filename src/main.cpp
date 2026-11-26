// NV Reactive Ragdoll : reactions physiques des PNJ vivants aux coups (plugin xNVSE, Fallout New Vegas 1.4.0.525).
// Licence GPL-3.0 (offsets repris de JIP LN NVSE, GPL-3). Voir docs/00-conception.md.
//
// Etape 0 : le plugin se charge, ecrit son journal, lit son .ini, ecoute deux touches.
// Etape 1 : F8 liste les corps physiques du PNJ vise (os, masse, anime ou dynamique).
// Etape 2 : un coup pousse le corps touche et ses voisins (impulsion au point d'impact). F7 pousse le PNJ vise.
//           Le moteur ne pousse que des corps dynamiques : sur un vivant, c'est le « Ragdoll Feedback » natif
//           (section [RagdollAnim] du jeu) qui doit les rendre dynamiques. Le plugin l'active et journalise ce qu'il voit.
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstring>
#include <cstdlib>
#include <share.h>
#include <initializer_list>
#include "game.h"

namespace
{
	constexpr UInt32 kPluginVersion = 7;            // pour xNVSE : entier qui augmente a chaque version publiee
	constexpr const char* kVersionName = "0.7.0-alpha"; // pour les joueurs : version du paquet
	constexpr const char* kPluginName = "NVReactiveRagdoll";

	PluginHandle g_handle = 0xFFFFFFFF;
	NVSEMessagingInterface* g_messaging = nullptr;
	NVSEEventManagerInterface* g_events = nullptr;
	char g_gameDir[MAX_PATH] = {};
	char g_iniPath[MAX_PATH] = {};
	FILE* g_log = nullptr;
	bool g_explosionReactions = false; // le mod Explosion Reactions est actif (etape 8)
	float g_humanMass = 0.f;           // masse totale d'un squelette humain, mesuree au premier coup (etape 7)

	// ------------------------------------------------------------------ journal
	void Log(const char* fmt, ...)
	{
		if (!g_log) return;
		SYSTEMTIME t;
		GetLocalTime(&t);
		fprintf(g_log, "%02u:%02u:%02u.%03u ", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds);
		va_list args;
		va_start(args, fmt);
		vfprintf(g_log, fmt, args);
		va_end(args);
		fputc('\n', g_log);
		fflush(g_log);
	}

	// ------------------------------------------------------------------ reglages (.ini du plugin)
	struct Config
	{
		bool enabled = true;
		bool livingReactions = false; // 0 : les vivants restent ceux du jeu de base, tout se joue a la mort
		bool verbose = true;
		bool reactPlayer = false;
		int keyDump = VK_F8;
		int keyPush = VK_F7;
		float testPush = 150.f;
		// [Hits] impulsions en unites Havok (masse x unites Havok/s)
		float basePush = 20.f;
		float pushPerDamage = 1.5f;
		float maxPush = 120.f;
		float meleeMult = 1.5f;
		float explosionMult = 2.0f;
		float criticalMult = 1.3f;
		float neighbourShare = 0.4f;
		float up = 0.1f;
		// [Death] coup fatal : poussee gardee jusqu'au passage en ragdoll de mort
		bool deferFatal = true;
		float fatalWait = 1.0f;
		float fatalMult = 1.5f; // morts exagerees : la balle qui tue pousse fort
		float carryVelocity = 1.0f; // part de la vitesse de course gardee en tombant (mort ou renverse)
		// [Reactions] charge des coups : chaque coup ajoute sa force, la charge retombe avec le temps
		bool stagger = true;
		bool knockdown = true;
		float staggerAt = 60.f;
		float knockAt = 140.f;
		float stressHalfLife = 0.4f;
		float staggerCooldown = 1.5f;
		float knockForce = 30.f;
		float knockForceMax = 2.0f;
		float knockLocalShare = 0.5f;
		// [Muscles] etape 4
		bool muscles = true;
		bool musclesOnKnockdown = true;
		bool musclesOnDeath = true;
		float muscleTime = 1.5f;
		float muscleDeathTime = 0.8f;
		float jointDamping = 0.25f;
		float neckDamping = 0.5f;
		float armPull = 25.f;
		float armDamping = 8.f;
		float armMaxDv = 4.f;
		// [Death] etape 5 : facon de mourir
		bool grab = true;
		float grabChance = 0.7f;
		float grabPull = 1.5f;
		bool headshotLimp = false;
		// [Balance] agonie debout facon Euphoria : il cherche son equilibre, fait des pas, encaisse, puis lache
		bool balance = true;
		float balanceTimeMin = 1.5f;
		float balanceTimeMax = 3.5f;
		float balanceMaxDamage = 400.f;
		float balanceSupport = 0.35f; // petit coup de pouce : ce sont les muscles des jambes qui portent le corps
		float balancePushMult = 0.6f;
		float balanceDamping = 1.5f;
		float uprightGain = 6.f;
		float sway = 0.2f;
		float stepTime = 0.35f;
		float stepLead = 0.35f;
		float stepPull = 40.f;
		float stepDamping = 8.f;
		float stepLift = 1.5f;
		float stepMaxDv = 5.f;
		float balanceArms = 0.6f;
		float hitVitalityLoss = 0.35f;
		// [Balance] muscles d'articulation vers la pose debout photographiee a la mort
		bool pose = true;
		float poseStrength = 30.f;
		float poseDamping = 6.f;
		float poseMaxDv = 8.f;
		float legStrength = 1.5f;
		float spineStrength = 1.0f;
		float armStrength = 0.3f;
		float neckStrength = 0.6f;
		float stepLegRelax = 0.3f;
		float fallPose = 0.25f;
		bool writhe = true;
		float writheChance = 0.6f;
		float writheTime = 3.f;
		float writheTimer = 0.7f;
		float writheStrength = 0.35f;
		float writhePull = 0.5f;
		float writheMaxDamage = 400.f;
		// etape 6 : [Fire], [Shock], [Explosions]
		bool elements = true;
		float burnTime = 2.5f;
		float burnDeathTime = 2.5f;
		float flailTimer = 0.3f;
		float fireArmPull = 1.2f;
		float fireLegKick = 6.f;
		float livingFlail = 15.f;
		float shockTime = 1.5f;
		float shockShake = 1.5f;
		float livingShake = 8.f;
		float explosionUp = 0.5f;
		float explosionKnockMult = 1.5f;
		float flyTime = 1.2f;
		float flyMinSpeed = 3.f;
		float windmillSpeed = 8.f;
		// [Compatibility] etape 8
		bool yieldExplosionReactions = true;
		// [Creatures] etape 7
		bool creatures = true;
		bool creatureStagger = true;
		bool creatureKnockdown = true;
		bool creatureMuscles = true;
		bool massScaling = true;
		float creatureThreshold = 1.0f;
		// [RagdollAnim] reglages natifs du jeu
		bool forceFeedback = true;
	} g_cfg;

	float IniFloat(const char* sec, const char* key, float def)
	{
		char buf[64];
		GetPrivateProfileStringA(sec, key, "", buf, sizeof(buf), g_iniPath);
		return buf[0] ? (float)atof(buf) : def;
	}
	int IniInt(const char* sec, const char* key, int def) { return (int)GetPrivateProfileIntA(sec, key, def, g_iniPath); }

	// Relecture a chaud : le menu MCM enregistre le .ini quand on ferme le menu pause ; on regarde sa date
	// d'ecriture une fois par seconde.
	FILETIME g_iniTime = {};
	DWORD g_lastIniCheck = 0;

	bool IniWriteTime(FILETIME& out)
	{
		WIN32_FILE_ATTRIBUTE_DATA d;
		if (!GetFileAttributesExA(g_iniPath, GetFileExInfoStandard, &d)) return false;
		out = d.ftLastWriteTime;
		return true;
	}

	void LoadConfig()
	{
		Config c;
		c.enabled = IniInt("General", "bEnabled", 1) != 0;
		c.verbose = IniInt("General", "bVerbose", 1) != 0;
		c.reactPlayer = IniInt("General", "bReactPlayer", 0) != 0;
		c.keyDump = IniInt("Debug", "iDumpKey", VK_F8);
		c.keyPush = IniInt("Debug", "iPushKey", VK_F7);
		c.testPush = IniFloat("Debug", "fTestPush", c.testPush);
		c.basePush = IniFloat("Hits", "fBasePush", c.basePush);
		c.pushPerDamage = IniFloat("Hits", "fPushPerDamage", c.pushPerDamage);
		c.maxPush = IniFloat("Hits", "fMaxPush", c.maxPush);
		c.meleeMult = IniFloat("Hits", "fMeleeMult", c.meleeMult);
		c.explosionMult = IniFloat("Hits", "fExplosionMult", c.explosionMult);
		c.criticalMult = IniFloat("Hits", "fCriticalMult", c.criticalMult);
		c.neighbourShare = IniFloat("Hits", "fNeighbourShare", c.neighbourShare);
		c.up = IniFloat("Hits", "fUp", c.up);
		c.deferFatal = IniInt("Death", "bDeferFatalPush", 1) != 0;
		c.fatalWait = IniFloat("Death", "fWaitTime", c.fatalWait);
		c.fatalMult = IniFloat("Death", "fPushMult", c.fatalMult);
		c.carryVelocity = IniFloat("Death", "fCarryVelocity", c.carryVelocity);
		c.stagger = IniInt("Reactions", "bStagger", 1) != 0;
		c.knockdown = IniInt("Reactions", "bKnockdown", 1) != 0;
		c.staggerAt = IniFloat("Reactions", "fStaggerAt", c.staggerAt);
		c.knockAt = IniFloat("Reactions", "fKnockdownAt", c.knockAt);
		c.stressHalfLife = IniFloat("Reactions", "fChargeHalfLife", c.stressHalfLife);
		c.staggerCooldown = IniFloat("Reactions", "fStaggerCooldown", c.staggerCooldown);
		c.knockForce = IniFloat("Reactions", "fKnockdownForce", c.knockForce);
		c.knockForceMax = IniFloat("Reactions", "fKnockdownForceMaxMult", c.knockForceMax);
		c.knockLocalShare = IniFloat("Reactions", "fKnockdownLocalPush", c.knockLocalShare);
		c.muscles = IniInt("Muscles", "bEnabled", 1) != 0;
		c.musclesOnKnockdown = IniInt("Muscles", "bOnKnockdown", 1) != 0;
		c.musclesOnDeath = IniInt("Muscles", "bOnDeath", 1) != 0;
		c.muscleTime = IniFloat("Muscles", "fKnockdownTime", c.muscleTime);
		c.muscleDeathTime = IniFloat("Muscles", "fDeathTime", c.muscleDeathTime);
		c.jointDamping = IniFloat("Muscles", "fJointTension", c.jointDamping);
		c.neckDamping = IniFloat("Muscles", "fNeckTension", c.neckDamping);
		c.armPull = IniFloat("Muscles", "fArmPull", c.armPull);
		c.armDamping = IniFloat("Muscles", "fArmDamping", c.armDamping);
		c.armMaxDv = IniFloat("Muscles", "fArmMaxSpeed", c.armMaxDv);
		c.grab = IniInt("Death", "bGrabWound", 1) != 0;
		c.grabChance = IniFloat("Death", "fGrabChance", c.grabChance);
		c.grabPull = IniFloat("Death", "fGrabPull", c.grabPull);
		c.livingReactions = IniInt("General", "bLivingReactions", 0) != 0;
		c.headshotLimp = IniInt("Death", "bHeadshotLimp", 0) != 0;
		c.balance = IniInt("Balance", "bEnabled", 1) != 0;
		c.balanceTimeMin = IniFloat("Balance", "fTimeMin", c.balanceTimeMin);
		c.balanceTimeMax = IniFloat("Balance", "fTimeMax", c.balanceTimeMax);
		c.balanceMaxDamage = IniFloat("Balance", "fMaxDamage", c.balanceMaxDamage);
		c.balanceSupport = IniFloat("Balance", "fSupport", c.balanceSupport);
		c.balancePushMult = IniFloat("Balance", "fPushMult", c.balancePushMult);
		c.balanceDamping = IniFloat("Balance", "fDamping", c.balanceDamping);
		c.uprightGain = IniFloat("Balance", "fUprightGain", c.uprightGain);
		c.sway = IniFloat("Balance", "fSway", c.sway);
		c.stepTime = IniFloat("Balance", "fStepTime", c.stepTime);
		c.stepLead = IniFloat("Balance", "fStepLead", c.stepLead);
		c.stepPull = IniFloat("Balance", "fStepPull", c.stepPull);
		c.stepDamping = IniFloat("Balance", "fStepDamping", c.stepDamping);
		c.stepLift = IniFloat("Balance", "fStepLift", c.stepLift);
		c.stepMaxDv = IniFloat("Balance", "fStepMaxSpeed", c.stepMaxDv);
		c.balanceArms = IniFloat("Balance", "fArms", c.balanceArms);
		c.hitVitalityLoss = IniFloat("Balance", "fHitWeaken", c.hitVitalityLoss);
		c.pose = IniInt("Balance", "bJointMuscles", 1) != 0;
		c.poseStrength = IniFloat("Balance", "fJointStrength", c.poseStrength);
		c.poseDamping = IniFloat("Balance", "fJointDamping", c.poseDamping);
		c.poseMaxDv = IniFloat("Balance", "fJointMaxSpeed", c.poseMaxDv);
		c.legStrength = IniFloat("Balance", "fLegStrength", c.legStrength);
		c.spineStrength = IniFloat("Balance", "fSpineStrength", c.spineStrength);
		c.armStrength = IniFloat("Balance", "fArmStrength", c.armStrength);
		c.neckStrength = IniFloat("Balance", "fNeckStrength", c.neckStrength);
		c.stepLegRelax = IniFloat("Balance", "fStepLegRelax", c.stepLegRelax);
		c.fallPose = IniFloat("Balance", "fFallPose", c.fallPose);
		c.writhe = IniInt("Death", "bWrithe", 1) != 0;
		c.writheChance = IniFloat("Death", "fWritheChance", c.writheChance);
		c.writheTime = IniFloat("Death", "fWritheTime", c.writheTime);
		c.writheTimer = IniFloat("Death", "fWritheTimer", c.writheTimer);
		c.writheStrength = IniFloat("Death", "fWritheStrength", c.writheStrength);
		c.writhePull = IniFloat("Death", "fWrithePull", c.writhePull);
		c.writheMaxDamage = IniFloat("Death", "fWritheMaxDamage", c.writheMaxDamage);
		c.elements = IniInt("General", "bElements", 1) != 0;
		c.burnTime = IniFloat("Fire", "fBurnTime", c.burnTime);
		c.burnDeathTime = IniFloat("Fire", "fDeathTime", c.burnDeathTime);
		c.flailTimer = IniFloat("Fire", "fFlailTimer", c.flailTimer);
		c.fireArmPull = IniFloat("Fire", "fArmPull", c.fireArmPull);
		c.fireLegKick = IniFloat("Fire", "fLegKick", c.fireLegKick);
		c.livingFlail = IniFloat("Fire", "fLivingFlail", c.livingFlail);
		c.shockTime = IniFloat("Shock", "fShockTime", c.shockTime);
		c.shockShake = IniFloat("Shock", "fShake", c.shockShake);
		c.livingShake = IniFloat("Shock", "fLivingShake", c.livingShake);
		c.explosionUp = IniFloat("Explosions", "fUp", c.explosionUp);
		c.explosionKnockMult = IniFloat("Explosions", "fKnockdownMult", c.explosionKnockMult);
		c.flyTime = IniFloat("Explosions", "fFlyTime", c.flyTime);
		c.flyMinSpeed = IniFloat("Explosions", "fFlyMinSpeed", c.flyMinSpeed);
		c.windmillSpeed = IniFloat("Explosions", "fWindmillSpeed", c.windmillSpeed);
		c.yieldExplosionReactions = IniInt("Compatibility", "bYieldToExplosionReactions", 1) != 0;
		c.creatures = IniInt("Creatures", "bEnabled", 1) != 0;
		c.creatureStagger = IniInt("Creatures", "bStagger", 1) != 0;
		c.creatureKnockdown = IniInt("Creatures", "bKnockdown", 1) != 0;
		c.creatureMuscles = IniInt("Creatures", "bMuscles", 1) != 0;
		c.massScaling = IniInt("Creatures", "bMassScaling", 1) != 0;
		c.creatureThreshold = IniFloat("Creatures", "fThresholdMult", c.creatureThreshold);
		c.forceFeedback = IniInt("RagdollAnim", "bRagdollFeedback", 1) != 0;
		g_cfg = c;
		IniWriteTime(g_iniTime);
		Log("Reglages lus : %s (%s)", g_iniPath,
			GetFileAttributesA(g_iniPath) != INVALID_FILE_ATTRIBUTES ? "trouve" : "ABSENT, valeurs par defaut");
		Log("  actif %d, poussee %.1f + %.2f/degat (max %.0f), voisins %.2f, test F7 %.0f",
			c.enabled, c.basePush, c.pushPerDamage, c.maxPush, c.neighbourShare, c.testPush);
	}

	// Reglages natifs du jeu (Fallout.ini et GMST, meme structure en memoire) : ecrits seulement s'ils sont presents
	// dans notre .ini, dans la section indiquee.
	struct GameSetting { const char* section; const char* key; UInt32 addr; bool isFloat; };
	const GameSetting kGameSettings[] = {
		{"RagdollAnim", "bRagdollFeedback", ini::bRagdollFeedback, false},
		{"RagdollAnim", "bRagdollAnim", ini::bRagdollAnim, false},
		{"RagdollAnim", "bPoseMatching", ini::bPoseMatching, false},
		{"RagdollAnim", "fHierarchyGain", ini::fHierarchyGain, true},
		{"RagdollAnim", "fVelocityDamping", ini::fVelocityDamping, true},
		{"RagdollAnim", "fAccelerationGain", ini::fAccelerationGain, true},
		{"RagdollAnim", "fVelocityGain", ini::fVelocityGain, true},
		{"RagdollAnim", "fPositionGain", ini::fPositionGain, true},
		{"RagdollAnim", "fPositionMaxLinearVelocity", ini::fPositionMaxLinearVelocity, true},
		{"RagdollAnim", "fPositionMaxAngularVelocity", ini::fPositionMaxAngularVelocity, true},
		{"RagdollAnim", "fSnapGain", ini::fSnapGain, true},
		{"RagdollAnim", "fSnapMaxLinearVelocity", ini::fSnapMaxLinearVelocity, true},
		{"RagdollAnim", "fSnapMaxAngularVelocity", ini::fSnapMaxAngularVelocity, true},
		{"RagdollAnim", "fSnapMaxLinearDistance", ini::fSnapMaxLinearDistance, true},
		{"RagdollAnim", "fSnapMaxAngularDistance", ini::fSnapMaxAngularDistance, true},
		{"RagdollAnim", "uiFeedbackTimeout", ini::uiFeedbackTimeout, false},
		// poussee que le jeu donne lui-meme aux corps (dont la « kill impulse » des armes), au centre du corps
		{"Death", "fVanillaImpulseMult", gmst::fGameplayImpulseMultBiped, true},
	};

	void ApplyGameSettings()
	{
		for (const GameSetting& s : kGameSettings)
		{
			void* setting = (void*)s.addr;
			const char* name = At<const char*>(setting, ini::kName);
			char buf[64];
			GetPrivateProfileStringA(s.section, s.key, "", buf, sizeof(buf), g_iniPath);
			if (s.isFloat)
			{
				float before = At<float>(setting, ini::kValue);
				if (buf[0]) At<float>(setting, ini::kValue) = (float)atof(buf);
				Log("  %-32s %g -> %g", name ? name : s.key, before, At<float>(setting, ini::kValue));
			}
			else
			{
				UInt32 before = At<UInt32>(setting, ini::kValue);
				if (buf[0]) At<UInt32>(setting, ini::kValue) = (UInt32)atoi(buf);
				Log("  %-32s %u -> %u", name ? name : s.key, before, At<UInt32>(setting, ini::kValue));
			}
		}
	}

	void CheckIniChanged()
	{
		DWORD now = GetTickCount();
		if (now - g_lastIniCheck < 1000) return;
		g_lastIniCheck = now;
		FILETIME t;
		if (!IniWriteTime(t) || !CompareFileTime(&t, &g_iniTime)) return;
		Log("Reglages modifies (menu MCM ou fichier) : relecture");
		LoadConfig();
		ApplyGameSettings();
	}

	// ------------------------------------------------------------------ petits vecteurs
	struct V3 { float x, y, z; };
	V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
	V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
	V3 operator*(V3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
	float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	V3 Cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
	float Len(V3 a) { return sqrtf(Dot(a, a)); }
	V3 Norm(V3 a) { float l = Len(a); return l > 1e-5f ? a * (1.f / l) : V3{0, 0, 0}; }
	V3 Load3(const void* base, UInt32 off) { const float* f = &At<float>(base, off); return {f[0], f[1], f[2]}; }

	// ------------------------------------------------------------------ corps physiques d'un acteur
	struct Body
	{
		const char* name;
		UInt8* rb;      // hkpRigidBody*
		int parent;     // indice du corps ancetre le plus proche, -1 pour la racine
	};
	constexpr int kMaxBodies = 64;

	UInt8* RigidBodyOf(void* node)
	{
		void* coll = At<void*>(node, ni::kCollisionObject);
		if (!coll) return nullptr;
		void* wo = At<void*>(coll, ni::kCollision_WorldObject);
		if (!wo) return nullptr;
		UInt8* hko = At<UInt8*>(wo, ni::kBhkRef_HkObject);
		if (!hko || At<UInt8>(hko, hk::kCollisionType) != 1) return nullptr;
		return hko;
	}

	bool IsNode(void* obj)
	{
		using Fn = void* (__thiscall*)(void*);
		void** vtbl = *(void***)obj;
		return ((Fn)vtbl[ni::kVtbl_GetNiNode / 4])(obj) != nullptr;
	}

	void CollectRec(void* node, int parentBody, Body* out, int& n, int depth)
	{
		if (!node || depth > 48 || n >= kMaxBodies) return;
		int here = parentBody;
		if (UInt8* rb = RigidBodyOf(node))
		{
			out[n] = {At<const char*>(node, ni::kName), rb, parentBody};
			here = n++;
		}
		if (!IsNode(node)) return;
		void** data = At<void**>(node, ni::kChildren + 4);
		UInt16 count = At<UInt16>(node, ni::kChildren + 0xA);
		for (UInt16 i = 0; data && i < count; ++i)
			if (data[i]) CollectRec(data[i], here, out, n, depth + 1);
	}

	// Lit l'arbre 3D sous protection : une adresse invalide ne fait pas planter le jeu, elle renvoie 0 corps.
	int CollectBodies(void* ref, Body* out)
	{
		int n = 0;
		__try
		{
			void* rs = At<void*>(ref, refr::kRenderState);
			void* root = rs ? At<void*>(rs, refr::kRenderRoot) : nullptr;
			CollectRec(root, -1, out, n, 0);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			Log("ERREUR : lecture du squelette impossible (exception %08X)", GetExceptionCode());
			return 0;
		}
		return n;
	}

	V3 BodyCenter(UInt8* rb) { return Load3(rb, hk::kCenterOfMass); }

	// Impulsion J (unites Havok) au point p (monde, unites Havok) sur un corps dynamique : vitesse lineaire J/m,
	// vitesse angulaire I^-1 (r x J), avec I^-1 diagonale dans le repere du corps.
	bool ApplyImpulse(UInt8* rb, V3 J, V3 p)
	{
		UInt8 type = At<UInt8>(rb, hk::kMotionType);
		if (!hk::IsDynamic(type)) return false;
		float invMass = At<float>(rb, hk::kInvMass);
		V3 r = p - BodyCenter(rb);
		V3 L = Cross(r, J);
		V3 c0 = Load3(rb, hk::kRotation), c1 = Load3(rb, hk::kRotation + 0x10), c2 = Load3(rb, hk::kRotation + 0x20);
		V3 inv = Load3(rb, hk::kInvInertia);
		V3 local = {Dot(c0, L) * inv.x, Dot(c1, L) * inv.y, Dot(c2, L) * inv.z};
		V3 dw = c0 * local.x + c1 * local.y + c2 * local.z;
		V3 dv = J * invMass;
		float* lv = &At<float>(rb, hk::kLinVelocity);
		float* av = &At<float>(rb, hk::kAngVelocity);
		lv[0] += dv.x; lv[1] += dv.y; lv[2] += dv.z;
		av[0] += dw.x; av[1] += dw.y; av[2] += dw.z;
		using Fn = void(__thiscall*)(void*);
		((Fn)hk::kAddr_UpdateMotion)(rb);
		return true;
	}

	// ------------------------------------------------------------------ utilitaires acteurs
	bool IsActor(void* ref)
	{
		if (!ref) return false;
		UInt8 t = At<UInt8>(ref, form::kTypeID);
		return t == form::kType_Character || t == form::kType_Creature;
	}
	void* Player() { return *(void**)player::kAddr_PlayerCharacter; }
	bool IsDead(void* actorRef) { UInt32 s = At<UInt32>(actorRef, actor::kLifeState); return s == 1 || s == 2; }
	UInt32 RefID(void* ref) { return ref ? At<UInt32>(ref, form::kRefID) : 0; }

	void* HitDataOf(void* actorRef)
	{
		void* process = At<void*>(actorRef, actor::kProcess);
		if (!process) return nullptr;
		using Fn = void* (__thiscall*)(void*);
		void** vtbl = *(void***)process;
		return ((Fn)vtbl[actor::kProcess_GetHitData / 4])(process);
	}

	int NearestBody(const Body* b, int n, V3 p)
	{
		int best = -1;
		float bestD = 1e30f;
		for (int i = 0; i < n; ++i)
		{
			float d = Len(BodyCenter(b[i].rb) - p);
			if (d < bestD) { bestD = d; best = i; }
		}
		return best;
	}

	int FindBody(const Body* b, int n, const char* name)
	{
		for (int i = 0; i < n; ++i)
			if (b[i].name && !_stricmp(b[i].name, name)) return i;
		return -1;
	}

	// Etape 7 : roles des os sur n'importe quel squelette (creatures comprises), pas seulement l'humain.
	float MassOf(UInt8* rb) { float im = At<float>(rb, hk::kInvMass); return im > 0.f ? 1.f / im : 0.f; }

	float TotalMass(const Body* b, int n)
	{
		float m = 0.f;
		for (int k = 0; k < n; ++k) m += MassOf(b[k].rb);
		return m;
	}

	// torse : Bip01 Spine2 chez l'humain ; sinon le corps le plus lourd (le tronc d'un chien, d'un deathclaw...)
	int ChestOf(const Body* b, int n)
	{
		int i = FindBody(b, n, "Bip01 Spine2");
		if (i >= 0) return i;
		float best = -1.f;
		for (int k = 0; k < n; ++k)
			if (MassOf(b[k].rb) > best) { best = MassOf(b[k].rb); i = k; }
		return i;
	}

	// bassin : Bip01 Pelvis ; sinon le corps racine (sans parent)
	int PelvisOf(const Body* b, int n)
	{
		int i = FindBody(b, n, "Bip01 Pelvis");
		if (i >= 0) return i;
		for (int k = 0; k < n; ++k)
			if (b[k].parent < 0) return k;
		return -1;
	}

	bool HasChild(const Body* b, int n, int i)
	{
		for (int k = 0; k < n; ++k)
			if (b[k].parent == i) return true;
		return false;
	}

	// bouts des membres : corps sans enfant (mains, pieds, queue, tete...), hors torse
	int LimbEnds(const Body* b, int n, int chest, int* out, int max)
	{
		int c = 0;
		for (int k = 0; k < n && c < max; ++k)
			if (k != chest && !HasChild(b, n, k)) out[c++] = k;
		return c;
	}

	// racines des membres : enfants directs du torse ou du bassin (bras, cuisses, cou...)
	int LimbRoots(const Body* b, int n, int chest, int pelvis, int* out, int max)
	{
		int c = 0;
		for (int k = 0; k < n && c < max; ++k)
			if (k != chest && k != pelvis && (b[k].parent == chest || b[k].parent == pelvis)) out[c++] = k;
		return c;
	}

	bool IsHumanoid(const Body* b, int n) { return FindBody(b, n, "Bip01 L Hand") >= 0 && FindBody(b, n, "Bip01 R Hand") >= 0; }

	// Pousse le corps i (part 1) et ses voisins directs, parent et enfants (part fNeighbourShare).
	// Renvoie le nombre de corps effectivement pousses (0 si tout est anime).
	int PushBodyAndNeighbours(const Body* b, int n, int i, V3 J, V3 p)
	{
		int pushed = ApplyImpulse(b[i].rb, J, p) ? 1 : 0;
		V3 Jn = J * g_cfg.neighbourShare;
		if (g_cfg.neighbourShare > 0.f)
			for (int k = 0; k < n; ++k)
				if (k != i && (k == b[i].parent || b[k].parent == i))
					pushed += ApplyImpulse(b[k].rb, Jn, BodyCenter(b[k].rb)) ? 1 : 0;
		return pushed;
	}

	void DumpBodies(void* ref)
	{
		Body b[kMaxBodies];
		int n = CollectBodies(ref, b);
		V3 pos = Load3(ref, refr::kPos);
		Log("=== Corps physiques de %08X (pos %.0f %.0f %.0f), %d corps, ragdoll controller %p",
			RefID(ref), pos.x, pos.y, pos.z, n, At<void*>(ref, actor::kRagdollController));
		int dynamic = 0;
		for (int i = 0; i < n; ++i)
		{
			UInt8 type = At<UInt8>(b[i].rb, hk::kMotionType);
			float invMass = At<float>(b[i].rb, hk::kInvMass);
			V3 c = BodyCenter(b[i].rb) * 7.f; // retour en unites du jeu, pour comparer avec pos
			dynamic += hk::IsDynamic(type) ? 1 : 0;
			Log("  [%2d] %-22s parent %2d  %-18s masse %6.2f  centre %.0f %.0f %.0f",
				i, b[i].name ? b[i].name : "?", b[i].parent, hk::MotionName(type),
				invMass > 0.f ? 1.f / invMass : 0.f, c.x, c.y, c.z);
		}
		Log("=== %d dynamiques / %d", dynamic, n);
	}

	// ------------------------------------------------------------------ muscles pendant la chute (etape 4)
	// Inspire de Skyrim Delirium : le ragdoll du jeu reste le corps, on lui ajoute des « muscles » a chaque image.
	// 1. Tension : chaque articulation freine la rotation de l'os par rapport a son parent (le cou plus fort),
	//    l'effort etant partage entre les deux os selon leur inertie : rien ne tourne « tout seul ».
	// 2. Reflexe de protection : chaque main est tiree vers un point devant et sous la poitrine, dans le sens de la
	//    chute (pour se rattraper), avec la force opposee sur la poitrine : le corps entier n'est pas propulse.
	// Les axes locaux des os de New Vegas ne sont pas documentes : tout est exprime dans l'espace du monde.
	// Etape 6 : element du coup
	enum Element { kElem_None = 0, kElem_Fire, kElem_Shock };
	const char* ElementName(int e) { return e == kElem_Fire ? "feu" : e == kElem_Shock ? "electricite" : ""; }

	// Etape 5 : facon de mourir, choisie au coup fatal (inspire du mod Ragdolls) ; etape 6 : feu, electricite,
	// explosion, aussi pour une chute
	struct DeathStyle
	{
		bool grab;        // une main va sur la plaie
		int grabHand;     // 0 gauche, 1 droite
		char wound[32];   // os touche
		V3 woundLocal;    // point de la plaie dans le repere de l'os
		int element;      // Element : le feu fait battre bras et jambes, l'electricite fait trembler
		bool explosion;   // projete en l'air : bras en moulinet et jambes qui pedalent tant qu'il vole
		bool balance;     // agonie debout facon Euphoria : cherche son equilibre, fait des pas, encaisse, puis lache
		bool writhe;      // une fois au sol, se tord quelques secondes (mains vers le ventre, jambes repliees)
		bool limp;        // s'effondre net, sans muscles (option tir a la tete)
		bool headWound;   // touche a la tete : la tete ballotte pendant l'agonie
		float upTime;     // duree de l'agonie debout, secondes (raccourcie par chaque balle encaissee)
	};

	// direction aleatoire, unitaire
	V3 RandomDir()
	{
		for (;;)
		{
			V3 v = {(float)rand() / RAND_MAX * 2.f - 1.f, (float)rand() / RAND_MAX * 2.f - 1.f, (float)rand() / RAND_MAX * 2.f - 1.f};
			float l = Len(v);
			if (l > 0.1f && l <= 1.f) return v * (1.f / l);
		}
	}

	// Repere d'un os : 3 colonnes de rotation (hkMatrix3x4)
	V3 ToWorld(UInt8* rb, V3 l)
	{
		return Load3(rb, hk::kRotation) * l.x + Load3(rb, hk::kRotation + 0x10) * l.y + Load3(rb, hk::kRotation + 0x20) * l.z;
	}
	V3 ToLocal(UInt8* rb, V3 w)
	{
		return {Dot(Load3(rb, hk::kRotation), w), Dot(Load3(rb, hk::kRotation + 0x10), w), Dot(Load3(rb, hk::kRotation + 0x20), w)};
	}

	// Rotations des os : matrice 3x3 par colonnes (R * v = c0 v.x + c1 v.y + c2 v.z), comme hkMatrix3 de Havok
	struct M3 { V3 c[3]; };
	M3 RotOf(UInt8* rb) { return {{Load3(rb, hk::kRotation), Load3(rb, hk::kRotation + 0x10), Load3(rb, hk::kRotation + 0x20)}}; }
	V3 MulV(const M3& a, V3 v) { return a.c[0] * v.x + a.c[1] * v.y + a.c[2] * v.z; }
	M3 Mul(const M3& a, const M3& b) { return {{MulV(a, b.c[0]), MulV(a, b.c[1]), MulV(a, b.c[2])}}; }
	M3 Tr(const M3& a)
	{
		return {{{a.c[0].x, a.c[1].x, a.c[2].x}, {a.c[0].y, a.c[1].y, a.c[2].y}, {a.c[0].z, a.c[1].z, a.c[2].z}}};
	}
	float El(const M3& a, int row, int col) { const V3& c = a.c[col]; return row == 0 ? c.x : row == 1 ? c.y : c.z; }
	// rotation -> axe x angle (radians) ; 0 pour une rotation nulle (ou d'un demi-tour, rare ici)
	V3 RotVec(const M3& r)
	{
		float cosA = (El(r, 0, 0) + El(r, 1, 1) + El(r, 2, 2) - 1.f) * 0.5f;
		if (cosA > 1.f) cosA = 1.f;
		if (cosA < -1.f) cosA = -1.f;
		V3 v = {El(r, 2, 1) - El(r, 1, 2), El(r, 0, 2) - El(r, 2, 0), El(r, 1, 0) - El(r, 0, 1)}; // 2 sin(a) x axe
		float s = Len(v);
		return s < 1e-5f ? V3{0, 0, 0} : v * (acosf(cosA) / s);
	}

	// Gravite du monde physique ou se trouve le corps (unites Havok/s2)
	V3 GravityOf(UInt8* rb)
	{
		void* world = At<void*>(rb, hkworld::kWorldOfObject);
		V3 g = world ? Load3(world, hkworld::kGravity) : V3{0, 0, -9.8f};
		if (Len(g) < 0.1f || Len(g) > 1000.f) g = {0, 0, -9.8f};
		static bool logged = false;
		if (!logged) { logged = true; Log("Gravite Havok : %.2f %.2f %.2f", g.x, g.y, g.z); }
		return g;
	}

	bool Chance(float p) { return p > 0.f && (float)rand() / RAND_MAX < p; }

	struct MuscleTask
	{
		UInt32 refID;    // 0 = case libre
		void* ref;
		DWORD start;
		DWORD last;
		float duration;  // secondes
		bool dying;      // a la mort, les muscles lachent progressivement
		V3 fallDir;      // horizontal, unitaire
		float reach;     // longueur du bras (epaule -> main), unites Havok, mesuree a la premiere image
		DeathStyle death;
		V3 flail[2];     // feu : direction ou chaque main bat, changee toutes les fFlailTimer s
		DWORD nextFlail;
		int stepFoot;    // agonie : pied qui fait le pas en cours (0 gauche, 1 droit, -1 aucun)
		DWORD stepEnd;   // fin du pas en cours
		V3 stepTarget;   // ou le pied va se poser
		// pose debout photographiee a la mort : rotation de chaque os par rapport a son parent, vers laquelle ses
		// muscles d'articulation le ramenent (methode de Skyrim Delirium : la pose animee du PNJ est tenue)
		bool hasPose;
		int poseCount;
		M3 pose[kMaxBodies];
	};
	constexpr int kMaxMuscles = 8;
	SInt32 KnockedState(void* actorRef); // plus bas
	MuscleTask g_muscles[kMaxMuscles] = {};

	void ClearMuscles() { memset(g_muscles, 0, sizeof(g_muscles)); }

	// Photographie la pose du corps (celle que l'animation lui donnait au moment ou le ragdoll de mort demarre)
	void CapturePose(MuscleTask& m)
	{
		Body b[kMaxBodies];
		int n = CollectBodies(m.ref, b);
		for (int k = 0; k < n; ++k)
			m.pose[k] = b[k].parent >= 0 ? Mul(Tr(RotOf(b[b[k].parent].rb)), RotOf(b[k].rb)) : RotOf(b[k].rb);
		m.poseCount = n;
		m.hasPose = n > 1;
	}

	void StartMuscles(void* target, bool dying, V3 fallDir, const DeathStyle* death)
	{
		if (!g_cfg.muscles || (dying ? !g_cfg.musclesOnDeath : !g_cfg.musclesOnKnockdown)) return;
		if (At<UInt8>(target, form::kTypeID) == form::kType_Creature && !g_cfg.creatureMuscles) return;
		MuscleTask* slot = nullptr;
		for (MuscleTask& m : g_muscles)
		{
			if (m.refID == RefID(target))
			{
				if (m.dying) return; // deja en train de mourir : une rafale ne relance pas sa mort a chaque balle
				slot = &m;
				break;
			}
			if (!slot && !m.refID) slot = &m;
		}
		if (!slot) return; // trop de PNJ au sol en meme temps : les suivants tombent sans muscles
		fallDir.z = 0.f;
		DeathStyle ds = death ? *death : DeathStyle{};
		if (!dying) { ds.balance = ds.writhe = ds.grab = false; ds.upTime = 0.f; } // l'agonie n'est que pour la mort
		if (dying && ds.limp && !ds.element)
		{
			if (g_cfg.verbose) Log("Muscles de %08X : aucun (tir a la tete, s'effondre net)", RefID(target));
			return;
		}
		// mort : agonie debout (balance), puis chute, puis au sol (writhe)
		float duration = dying ? ds.upTime + g_cfg.muscleDeathTime + (ds.writhe ? g_cfg.writheTime : 0.f) : g_cfg.muscleTime;
		if (ds.element == kElem_Fire && g_cfg.burnDeathTime > duration) duration = g_cfg.burnDeathTime;
		if (ds.element == kElem_Shock && g_cfg.shockTime > duration) duration = g_cfg.shockTime;
		if (ds.explosion && g_cfg.flyTime + 0.5f > duration) duration = g_cfg.flyTime + 0.5f;
		*slot = {RefID(target), target, GetTickCount(), GetTickCount(), duration, dying, Norm(fallDir), 0.f, ds};
		slot->stepFoot = -1;
		slot->hasPose = false;
		if (dying && ds.balance) CapturePose(*slot);
		if (g_cfg.verbose)
			Log("Muscles de %08X : debut (%s%s%s%s%s%s%s%s), %.1f s", slot->refID, dying ? "mort" : "chute",
				ds.balance ? ", agonie debout" : "", ds.writhe ? ", se tord au sol" : "",
				ds.grab ? ", main sur la plaie : " : "", ds.grab ? ds.wound : "",
				ds.element ? ", " : "", ElementName(ds.element), ds.explosion ? ", projete" : "", duration);
	}

	// Une balle encaissee pendant l'agonie debout : il lui reste moins de forces (fHitWeaken de son temps restant).
	// Renvoie true si le PNJ etait en train de mourir (agonie en cours).
	bool WeakenDying(void* target)
	{
		for (MuscleTask& m : g_muscles)
		{
			if (m.refID != RefID(target) || !m.dying) continue;
			float age = (GetTickCount() - m.start) / 1000.f;
			if (m.death.balance && age < m.death.upTime)
			{
				float left = (m.death.upTime - age) * (1.f - g_cfg.hitVitalityLoss);
				if (left < 0.15f) left = 0.15f;
				float cut = m.death.upTime - (age + left);
				m.death.upTime -= cut;
				m.duration -= cut;
				if (g_cfg.verbose) Log("Agonie de %08X : balle encaissee, encore %.1f s debout", m.refID, left);
			}
			return true;
		}
		return false;
	}

	// inverse d'inertie moyenne (scalaire) : suffisant pour repartir un effort entre deux os
	float InvInertia(UInt8* rb) { V3 i = Load3(rb, hk::kInvInertia); return (i.x + i.y + i.z) / 3.f; }

	void AddLinVel(UInt8* rb, V3 dv) { float* v = &At<float>(rb, hk::kLinVelocity); v[0] += dv.x; v[1] += dv.y; v[2] += dv.z; }
	void AddAngVel(UInt8* rb, V3 dw) { float* w = &At<float>(rb, hk::kAngVelocity); w[0] += dw.x; w[1] += dw.y; w[2] += dw.z; }

	// Tire l'os `hand` vers target, l'os d'appui `chest` en sens inverse (ressort amorti, PD) : un muscle tire entre
	// deux os, le corps entier n'est pas propulse.
	void PullBody(const Body* b, int hand, int chest, V3 target, float kp, float kd, float maxDv, float strength, float dt)
	{
		UInt8* h = b[hand].rb;
		UInt8* c = b[chest].rb;
		V3 e = target - BodyCenter(h);
		V3 vrel = Load3(h, hk::kLinVelocity) - Load3(c, hk::kLinVelocity);
		V3 dv = (e * kp - vrel * kd) * (dt * strength);
		float l = Len(dv);
		if (l > maxDv) dv = dv * (maxDv / l);
		float imh = At<float>(h, hk::kInvMass), imc = At<float>(c, hk::kInvMass);
		if (imh + imc <= 0.f) return;
		V3 J = dv * (1.f / (imh + imc)); // impulsion qui change la vitesse relative de dv
		AddLinVel(h, J * imh);
		AddLinVel(c, J * -imc);
	}

	void PullHand(const Body* b, int hand, int chest, V3 target, float strength, float dt)
	{
		PullBody(b, hand, chest, target, g_cfg.armPull, g_cfg.armDamping, g_cfg.armMaxDv, strength, dt);
	}

	// Renvoie false quand la tache est finie
	bool UpdateMuscle(MuscleTask& m, DWORD now)
	{
		if (forms::LookupByID(m.refID) != m.ref) return false;
		float age = (now - m.start) / 1000.f;
		if (age > m.duration) return false;
		if (!m.dying && KnockedState(m.ref) == 6) return false; // il se releve : on rend la main au jeu
		float dt = (now - m.last) / 1000.f;
		m.last = now;
		if (dt <= 0.f) return true;
		if (dt > 0.05f) dt = 0.05f;

		Body b[kMaxBodies];
		int n = CollectBodies(m.ref, b);
		int chest = ChestOf(b, n);
		if (chest < 0 || !hk::IsDynamic(At<UInt8>(b[chest].rb, hk::kMotionType))) return false; // plus de ragdoll
		bool humanoid = IsHumanoid(b, n);
		int pelvis = PelvisOf(b, n);

		// force. Chute : pleine puis decroissante. Mort, en trois temps : debout (stand / stumble) a pleine force,
		// chute ou les muscles lachent, puis au sol (writhe) avec une force faible qui s'eteint.
		float t = age / m.duration;
		float standT = m.dying ? m.death.upTime : 0.f;
		float fallEnd = standT + g_cfg.muscleDeathTime;
		bool writhing = m.dying && m.death.writhe && age >= fallEnd;
		float strength;
		if (!m.dying) strength = t < 0.6f ? 1.f : (1.f - t) / 0.4f;
		else if (age < standT) strength = 1.f;
		else if (!writhing)
		{
			float floor = m.death.writhe ? g_cfg.writheStrength : 0.f;
			strength = 1.f - (1.f - floor) * (age - standT) / (fallEnd - standT > 0.f ? fallEnd - standT : 1.f);
		}
		else strength = g_cfg.writheStrength * (1.f - (age - fallEnd) / (g_cfg.writheTime > 0.f ? g_cfg.writheTime : 1.f));
		if (strength < 0.f) strength = 0.f;
		bool touched[kMaxBodies] = {};
		int elem = m.death.element;
		// projete par une explosion : tant qu'il vole vite, bras en moulinet et jambes qui pedalent
		bool flying = m.death.explosion && age < g_cfg.flyTime
			&& Len(Load3(b[chest].rb, hk::kLinVelocity)) > g_cfg.flyMinSpeed;

		// 4. feu : toutes les fFlailTimer s, chaque main repart vers un autre endroit du corps (elle tape les
		//    flammes) et les jambes donnent un coup au hasard
		if (elem == kElem_Fire && now >= m.nextFlail)
		{
			m.nextFlail = now + (DWORD)(g_cfg.flailTimer * 1000.f);
			for (V3& f : m.flail) f = Norm(RandomDir() + V3{0, 0, 0.6f});
			// humain : les mollets ; creature : le bout de chaque membre (pattes, queue, pinces...)
			int kick[8];
			int nk = 0;
			if (humanoid)
			{
				for (const char* leg : {"Bip01 L Calf", "Bip01 R Calf"})
					if ((kick[nk] = FindBody(b, n, leg)) >= 0) ++nk;
			}
			else nk = LimbEnds(b, n, chest, kick, 8);
			for (int j = 0; j < nk; ++j)
			{
				int k = kick[j];
				if (!hk::IsDynamic(At<UInt8>(b[k].rb, hk::kMotionType))) continue;
				AddAngVel(b[k].rb, RandomDir() * (g_cfg.fireLegKick * strength));
				touched[k] = true;
			}
		}

		// 5. electricite : chaque os tremble, a chaque image
		if (elem == kElem_Shock)
		{
			float shake = g_cfg.shockShake * strength * (dt * 60.f);
			for (int k = 0; k < n; ++k)
			{
				if (!hk::IsDynamic(At<UInt8>(b[k].rb, hk::kMotionType))) continue;
				AddAngVel(b[k].rb, RandomDir() * shake);
				touched[k] = true;
			}
		}

		// 6. moulinet : les bras tournent autour de l'axe lateral, les jambes pedalent (en sens oppose l'une de
		//    l'autre) ; l'effort inverse va sur la poitrine
		if (flying)
		{
			V3 axis = Norm(Cross(m.fallDir, V3{0, 0, 1}));
			// humain : bras et cuisses ; creature : chaque membre qui part du tronc, un sur deux en sens inverse
			int limb[8];
			float spin[8];
			int nl = 0;
			if (humanoid)
			{
				const char* names[4] = {"Bip01 L UpperArm", "Bip01 R UpperArm", "Bip01 L Thigh", "Bip01 R Thigh"};
				const float dirs[4] = {1.f, 1.f, 1.f, -1.f};
				for (int l = 0; l < 4; ++l)
					if ((limb[nl] = FindBody(b, n, names[l])) >= 0) spin[nl++] = dirs[l];
			}
			else
			{
				nl = LimbRoots(b, n, chest, pelvis, limb, 8);
				for (int l = 0; l < nl; ++l) spin[l] = (l & 1) ? -1.f : 1.f;
			}
			UInt8* c = b[chest].rb;
			float ic = InvInertia(c);
			for (int l = 0; l < nl; ++l)
			{
				int k = limb[l];
				if (!hk::IsDynamic(At<UInt8>(b[k].rb, hk::kMotionType))) continue;
				V3 want = axis * (g_cfg.windmillSpeed * spin[l]);
				V3 rel = Load3(b[k].rb, hk::kAngVelocity) - Load3(c, hk::kAngVelocity);
				V3 dw = (want - rel) * (0.2f * strength);
				float ik = InvInertia(b[k].rb);
				if (ik <= 0.f) continue;
				AddAngVel(b[k].rb, dw);
				AddAngVel(c, dw * (-ic / ik));
				touched[k] = touched[chest] = true;
			}
		}

		// 0. Agonie debout, facon Euphoria. Le mourant essaie de tenir debout :
		//    - le haut du corps est soulage de son poids (fSupport), pleinement pendant 70 % de l'agonie puis de moins
		//      en moins : a la fin ses jambes cedent ;
		//    - le torse est tenu droit (axe bassin -> poitrine), penche vers ou il part et oscille un peu ;
		//    - equilibre : on calcule ou il va (centre de masse + vitesse x fStepLead, le « point de capture ») ; s'il
		//      s'eloigne de ses pieds, un pied se leve et va se poser la-bas : il fait des pas pour se rattraper. Une balle
		//      qui le pousse, ou l'elan de sa course, le font donc tituber de lui-meme, dans le bon sens ;
		//    - ses bras s'ecartent pour garder l'equilibre (sauf la main sur la plaie).
		bool upright = age < standT;
		float vital = 0.f; // forces qui restent pendant l'agonie : 1 pendant 70 % du temps, puis jusqu'a 0
		if (upright)
		{
			float hold = standT * 0.7f;
			vital = age < hold ? 1.f : 1.f - (age - hold) / (standT - hold);
			float support = g_cfg.balanceSupport * vital;
			V3 g = GravityOf(b[chest].rb);
			int ends[16];
			int ne = humanoid ? 0 : LimbEnds(b, n, chest, ends, 16);
			float mass = 0.f;
			V3 com = {0, 0, 0}, comV = {0, 0, 0};
			for (int k = 0; k < n; ++k)
			{
				if (!hk::IsDynamic(At<UInt8>(b[k].rb, hk::kMotionType))) continue;
				bool lowerLeg = false;
				if (humanoid) lowerLeg = b[k].name && (strstr(b[k].name, "Calf") || strstr(b[k].name, "Foot"));
				else for (int j = 0; j < ne; ++j) lowerLeg |= ends[j] == k;
				if (lowerLeg) continue;
				AddLinVel(b[k].rb, g * (-support * dt));
				touched[k] = true;
				float mk = MassOf(b[k].rb);
				com = com + BodyCenter(b[k].rb) * mk;
				comV = comV + Load3(b[k].rb, hk::kLinVelocity) * mk;
				mass += mk;
			}
			if (mass > 0.f) { com = com * (1.f / mass); comV = comV * (1.f / mass); }

			bool hasPelvis = pelvis >= 0 && pelvis != chest && hk::IsDynamic(At<UInt8>(b[pelvis].rb, hk::kMotionType));
			if (hasPelvis && mass > 0.f)
			{
				// torse tenu droit, penche vers ou il part, qui oscille
				V3 hv = {comV.x, comV.y, 0.f};
				float speed = Len(hv);
				V3 leanDir = speed > 0.05f ? hv * (1.f / speed) : m.fallDir;
				float lean = speed * 0.15f > 0.6f ? 0.6f : speed * 0.15f;
				V3 side = Norm(Cross(leanDir, V3{0, 0, 1}));
				V3 want = Norm(V3{0, 0, 1} + leanDir * lean + side * (g_cfg.sway * sinf(age * 4.8f))
					+ m.fallDir * (g_cfg.sway * 0.5f * sinf(age * 3.1f)));
				V3 spine = Norm(BodyCenter(b[chest].rb) - BodyCenter(b[pelvis].rb));
				V3 axis = Cross(spine, want);
				float s = Len(axis);
				if (s > 1e-4f)
				{
					// le tronc entier (bassin et poitrine ensemble) est ramene vers la verticale voulue : c'est son
					// equilibre ; les jambes, tenues par leurs muscles dans la pose, suivent le bassin
					V3 wWant = axis * (asinf(s > 1.f ? 1.f : s) * g_cfg.uprightGain / s);
					V3 wNow = (Load3(b[chest].rb, hk::kAngVelocity) + Load3(b[pelvis].rb, hk::kAngVelocity)) * 0.5f;
					float k = dt * 10.f > 1.f ? 1.f : dt * 10.f;
					V3 dw = (wWant - wNow) * (k * vital);
					AddAngVel(b[chest].rb, dw);
					AddAngVel(b[pelvis].rb, dw);
				}

				// il ne glisse pas comme sur de la glace : sa vitesse horizontale s'use (ses pieds freinent)
				float brake = dt * g_cfg.balanceDamping > 1.f ? 1.f : dt * g_cfg.balanceDamping;
				for (int part : {chest, pelvis})
				{
					V3 v = Load3(b[part].rb, hk::kLinVelocity);
					AddLinVel(b[part].rb, V3{-v.x, -v.y, 0.f} * brake);
				}
				touched[chest] = touched[pelvis] = true;

				// les pas : un pied a la fois va se poser sous le point de capture
				int feet[2] = {humanoid ? FindBody(b, n, "Bip01 L Foot") : -1, humanoid ? FindBody(b, n, "Bip01 R Foot") : -1};
				if (feet[0] >= 0 && feet[1] >= 0 && hk::IsDynamic(At<UInt8>(b[feet[0]].rb, hk::kMotionType))
					&& hk::IsDynamic(At<UInt8>(b[feet[1]].rb, hk::kMotionType)))
				{
					V3 fp[2] = {BodyCenter(b[feet[0]].rb), BodyCenter(b[feet[1]].rb)};
					V3 mid = (fp[0] + fp[1]) * 0.5f;
					V3 cp = com + comV * g_cfg.stepLead;
					V3 err = {cp.x - mid.x, cp.y - mid.y, 0.f};
					float legLen = Len(BodyCenter(b[pelvis].rb) - mid);
					if (m.stepFoot >= 0 && now >= m.stepEnd) m.stepFoot = -1;
					if (m.stepFoot < 0 && Len(err) > legLen * 0.12f)
					{
						// le pied le plus loin du point de capture fait le pas ; l'ecart entre les pieds est garde
						V3 d0 = {cp.x - fp[0].x, cp.y - fp[0].y, 0.f}, d1 = {cp.x - fp[1].x, cp.y - fp[1].y, 0.f};
						int f = Len(d0) > Len(d1) ? 0 : 1;
						V3 off = fp[f] - mid;
						off.z = 0.f;
						m.stepFoot = f;
						m.stepEnd = now + (DWORD)(g_cfg.stepTime * 1000.f);
						m.stepTarget = {cp.x + off.x, cp.y + off.y, fp[f].z};
					}
					if (m.stepFoot >= 0)
					{
						int k = feet[m.stepFoot];
						float total = g_cfg.stepTime > 0.01f ? g_cfg.stepTime : 0.01f;
						float phase = 1.f - (float)(m.stepEnd - now) / (total * 1000.f);
						if (phase < 0.f) phase = 0.f;
						if (phase > 1.f) phase = 1.f;
						V3 target = m.stepTarget;
						target.z += g_cfg.stepLift * sinf(3.14159f * phase); // le pied se leve puis se pose
						PullBody(b, k, pelvis, target, g_cfg.stepPull, g_cfg.stepDamping, g_cfg.stepMaxDv, vital, dt);
						touched[k] = true;
					}
				}
			}
		}

		// 7. au sol (writhe) : toutes les fWritheTimer s, les mains changent d'endroit pres du ventre, les jambes se
		//    replient ; une creature agite doucement le bout de ses membres
		if (writhing && now >= m.nextFlail)
		{
			m.nextFlail = now + (DWORD)(g_cfg.writheTimer * 1000.f);
			for (V3& f : m.flail) f = Norm(RandomDir() + V3{0, 0, -0.3f});
			if (!humanoid)
			{
				int ends[8];
				int ne = LimbEnds(b, n, chest, ends, 8);
				for (int j = 0; j < ne; ++j)
				{
					if (!hk::IsDynamic(At<UInt8>(b[ends[j]].rb, hk::kMotionType))) continue;
					AddAngVel(b[ends[j]].rb, RandomDir() * (g_cfg.fireLegKick * 0.3f * strength));
					touched[ends[j]] = true;
				}
			}
		}
		if (writhing && humanoid && pelvis >= 0)
		{
			V3 pel = BodyCenter(b[pelvis].rb);
			for (const char* foot : {"Bip01 L Foot", "Bip01 R Foot"})
			{
				int k = FindBody(b, n, foot);
				if (k < 0 || !hk::IsDynamic(At<UInt8>(b[k].rb, hk::kMotionType))) continue;
				V3 target = pel + (BodyCenter(b[k].rb) - pel) * 0.6f; // genoux qui se replient
				PullHand(b, k, pelvis, target, strength * g_cfg.writhePull, dt);
				touched[k] = touched[pelvis] = true;
			}
		}

		// 1. muscles des articulations.
		//    Agonie (pose photographiee) : chaque articulation est un muscle qui ramene l'os vers sa rotation de la
		//    pose debout par rapport a son parent (ressort amorti : force fPoseStrength, amortissement fPoseDamping).
		//    Les jambes sont plus fortes (elles portent le corps) et faiblissent avec les forces (vital) : a la fin
		//    de l'agonie les genoux plient et il s'affaisse. La jambe qui fait un pas est relachee. Pendant la chute,
		//    une petite part de la pose reste (fFallPose). Sinon : simple raideur (la rotation relative est freinee).
		//    L'effort est partage entre les deux os selon leur inertie : rien ne tourne « tout seul ».
		bool posing = m.hasPose && m.poseCount == n && !writhing && !flying && elem == kElem_None && g_cfg.pose;
		float poseScale = upright ? 0.25f + 0.75f * vital : (m.dying ? g_cfg.fallPose * strength : 0.f);
		for (int k = 0; k < n; ++k)
		{
			int p = b[k].parent;
			if (p < 0) continue;
			UInt8* ck = b[k].rb;
			UInt8* cp = b[p].rb;
			if (!hk::IsDynamic(At<UInt8>(ck, hk::kMotionType)) || !hk::IsDynamic(At<UInt8>(cp, hk::kMotionType))) continue;
			float ik = InvInertia(ck), ip = InvInertia(cp);
			if (ik + ip <= 0.f) continue;
			const char* nm = b[k].name ? b[k].name : "";
			bool headPart = strstr(nm, "Head") || strstr(nm, "Neck");
			V3 wrel = Load3(ck, hk::kAngVelocity) - Load3(cp, hk::kAngVelocity);
			V3 dw;
			if (posing && poseScale > 0.f)
			{
				bool leg = strstr(nm, "Thigh") || strstr(nm, "Calf") || strstr(nm, "Foot");
				bool arm = strstr(nm, "Clavicle") || strstr(nm, "Arm") || strstr(nm, "Hand");
				float mul = leg ? g_cfg.legStrength : arm ? g_cfg.armStrength : headPart ? g_cfg.neckStrength : g_cfg.spineStrength;
				if (headPart && m.death.headWound && upright) mul *= 0.1f;    // touche a la tete : elle ballotte
				if (leg && upright) mul *= vital;                             // les jambes lachent en premier
				if (leg && upright && m.stepFoot >= 0 && strstr(nm, m.stepFoot ? " R " : " L "))
					mul *= g_cfg.stepLegRelax;                                // la jambe qui fait le pas se laisse porter
				M3 want = Mul(RotOf(cp), m.pose[k]);                           // ou l'os devrait etre (monde)
				V3 e = RotVec(Mul(want, Tr(RotOf(ck))));                       // rotation qui l'y amene
				dw = (e * (g_cfg.poseStrength * mul * poseScale) - wrel * (g_cfg.poseDamping * poseScale)) * dt;
				float l = Len(dw);
				if (l > g_cfg.poseMaxDv) dw = dw * (g_cfg.poseMaxDv / l);
			}
			else
			{
				float damp = headPart ? ((m.death.headWound && upright) ? 0.03f : g_cfg.neckDamping) : g_cfg.jointDamping;
				if (elem == kElem_Shock) damp *= 2.f; // muscles contractes par le courant
				damp *= strength;
				if (damp > 0.9f) damp = 0.9f;
				damp = 1.f - powf(1.f - damp, dt * 60.f); // reglage donne pour 60 images/s, meme effet a tout framerate
				dw = wrel * -damp;
			}
			V3 T = dw * (1.f / (ik + ip)); // impulsion angulaire qui change la rotation relative de dw
			AddAngVel(ck, T * ik);
			AddAngVel(cp, T * -ip);
			touched[k] = touched[p] = true;
		}

		// 2. les mains se tendent pour amortir la chute
		const char* sides[2][2] = {{"Bip01 L Hand", "Bip01 L UpperArm"}, {"Bip01 R Hand", "Bip01 R UpperArm"}};
		V3 chestPos = BodyCenter(b[chest].rb);
		int wound = m.death.grab ? FindBody(b, n, m.death.wound) : -1;
		for (int s = 0; s < 2 && !flying && elem != kElem_Shock; ++s)
		{
			int hand = FindBody(b, n, sides[s][0]);
			int shoulder = FindBody(b, n, sides[s][1]);
			if (hand < 0 || shoulder < 0 || !hk::IsDynamic(At<UInt8>(b[hand].rb, hk::kMotionType))) continue;
			V3 sh = BodyCenter(b[shoulder].rb);
			if (m.reach <= 0.f) m.reach = Len(BodyCenter(b[hand].rb) - sh) * 1.6f; // centre du bras -> longueur
			V3 target;
			float pull = strength;
			if (wound >= 0 && s == m.death.grabHand)
			{
				// 3. la main va sur la plaie, qui suit l'os touche dans sa chute
				target = BodyCenter(b[wound].rb) + ToWorld(b[wound].rb, m.death.woundLocal);
				pull *= g_cfg.grabPull;
			}
			else if (elem == kElem_Fire)
			{
				target = chestPos + m.flail[s] * (m.reach * 0.7f);
				pull *= g_cfg.fireArmPull;
			}
			else if (writhing)
			{
				// au sol : la main erre pres du ventre et de la poitrine
				target = chestPos + m.flail[s] * (m.reach * 0.35f) + V3{0, 0, -m.reach * 0.2f};
				pull *= g_cfg.writhePull;
			}
			else if (upright)
			{
				// agonie debout : le bras s'ecarte sur le cote pour l'equilibre, et bat un peu
				V3 side = sh - chestPos;
				side.z = 0.f;
				side = Norm(side);
				float wobble = sinf(age * 5.3f + s * 1.7f);
				target = sh + side * (m.reach * 0.7f) + V3{0, 0, m.reach * (0.05f + 0.15f * wobble)};
				pull *= g_cfg.balanceArms;
			}
			else
			{
				V3 side = sh - chestPos;
				side.z = 0.f;
				target = sh + m.fallDir * (m.reach * 0.6f) + side * 0.3f + V3{0, 0, -m.reach * 0.5f};
			}
			PullHand(b, hand, chest, target, pull, dt);
			touched[hand] = touched[chest] = true;
		}

		using Fn = void(__thiscall*)(void*);
		for (int k = 0; k < n; ++k)
			if (touched[k]) ((Fn)hk::kAddr_UpdateMotion)(b[k].rb);
		return true;
	}

	void UpdateMuscles()
	{
		DWORD now = GetTickCount();
		for (MuscleTask& m : g_muscles)
		{
			if (!m.refID) continue;
			if (!UpdateMuscle(m, now))
			{
				if (g_cfg.verbose) Log("Muscles de %08X : fin apres %lu ms", m.refID, now - m.start);
				m.refID = 0;
			}
		}
	}

	// ------------------------------------------------------------------ coup fatal : poussee differee
	// Au moment du coup fatal, le PNJ est souvent encore anime : le moteur ignorerait la poussee. On la garde, et
	// on l'applique des que ses corps deviennent dynamiques (ragdoll de mort), au meme os et au meme point de l'os.
	// Meme mecanisme pour un PNJ renverse (PushActorAway) : son ragdoll ne commence qu'une image ou deux plus tard.
	struct PendingPush
	{
		UInt32 refID;      // 0 = case libre
		void* ref;         // compare a la table des formes avant chaque usage
		char bone[32];
		V3 J;              // impulsion, unites Havok
		V3 offset;         // point d'impact - centre de l'os au moment du coup
		V3 carry;          // vitesse de course a ajouter a tous les corps (unites Havok/s)
		const char* what;  // "Mort" ou "Chute", pour le journal
		DWORD start;       // GetTickCount
		DeathStyle death;  // facon de mourir (Mort seulement)
	};
	constexpr int kMaxPending = 16;
	PendingPush g_pending[kMaxPending] = {};

	void ClearPending() { memset(g_pending, 0, sizeof(g_pending)); }

	// Ajoute la meme vitesse a tous les corps dynamiques : l'elan que l'acteur avait avant de tomber.
	int AddVelocityToAll(const Body* b, int n, V3 v)
	{
		if (Len(v) < 1e-3f) return 0;
		int done = 0;
		using Fn = void(__thiscall*)(void*);
		for (int k = 0; k < n; ++k)
		{
			if (!hk::IsDynamic(At<UInt8>(b[k].rb, hk::kMotionType))) continue;
			float* lv = &At<float>(b[k].rb, hk::kLinVelocity);
			lv[0] += v.x; lv[1] += v.y; lv[2] += v.z;
			((Fn)hk::kAddr_UpdateMotion)(b[k].rb);
			++done;
		}
		return done;
	}

	void QueuePush(void* target, const char* what, const char* bone, V3 J, V3 offset, V3 carry, const DeathStyle* death)
	{
		PendingPush* slot = nullptr;
		for (PendingPush& p : g_pending)
		{
			// nouveau coup sur le meme PNJ, de meme nature : on remplace (le dernier coup avant la mort est le bon)
			if (p.refID == RefID(target) && p.what && !strcmp(p.what, what)) { slot = &p; break; }
			if (!slot && !p.refID) slot = &p;
		}
		if (!slot) slot = &g_pending[0]; // file pleine : on ecrase la plus ancienne case
		slot->refID = RefID(target);
		slot->ref = target;
		snprintf(slot->bone, sizeof(slot->bone), "%s", bone ? bone : "Bip01 Spine2");
		slot->J = J;
		slot->offset = offset;
		slot->carry = carry;
		slot->what = what;
		slot->start = GetTickCount();
		slot->death = death ? *death : DeathStyle{};
	}

	void ProcessPending()
	{
		DWORD now = GetTickCount();
		for (PendingPush& p : g_pending)
		{
			if (!p.refID) continue;
			DWORD waited = now - p.start;
			if (forms::LookupByID(p.refID) != p.ref) { p.refID = 0; continue; } // decharge entre-temps
			bool death = !strcmp(p.what, "Mort");
			bool dead = IsDead(p.ref);
			if (death && !dead)
			{
				// toujours vivant : ce coup n'etait pas le dernier ; rien a faire (les vivants restent ceux du jeu)
				if (waited > (DWORD)(g_cfg.fatalWait * 1000.f)) p.refID = 0;
				continue;
			}
			Body b[kMaxBodies];
			int n = CollectBodies(p.ref, b);
			int i = n ? FindBody(b, n, p.bone) : -1;
			if (i >= 0 && hk::IsDynamic(At<UInt8>(b[i].rb, hk::kMotionType)))
			{
				int carried = AddVelocityToAll(b, n, p.carry);
				int pushed = PushBodyAndNeighbours(b, n, i, p.J, BodyCenter(b[i].rb) + p.offset);
				StartMuscles(p.ref, death, p.carry + Norm(p.J), &p.death);
				if (g_cfg.verbose)
					Log("%s de %08X : poussee %.1f sur %s appliquee apres %lu ms, %d corps pousses, elan %.2f sur %d corps",
						p.what, p.refID, Len(p.J), p.bone, waited, pushed, Len(p.carry), carried);
				p.refID = 0;
			}
			else if (waited > (DWORD)(g_cfg.fatalWait * 1000.f))
			{
				if (g_cfg.verbose)
					Log("%s de %08X : toujours anime apres %lu ms, poussee abandonnee", p.what, p.refID, waited);
				p.refID = 0;
			}
		}
	}

	// ------------------------------------------------------------------ etat des acteurs : charge des coups
	// Chaque coup ajoute sa force a la « charge » du PNJ ; la charge perd la moitie de sa valeur toutes les
	// fChargeHalfLife secondes. Une rafale ou plusieurs balles rapprochees font donc tituber puis tomber.
	struct ActorState
	{
		UInt32 refID;
		float charge;
		DWORD lastHit;
		DWORD lastStagger;
		DWORD burnUntil;   // etape 6 : en feu jusqu'a (GetTickCount)
		DWORD shockUntil;  // electrocute jusqu'a
		DWORD nextFlail;
		DWORD deathTick;   // premiere fois qu'on l'a vu mort : un coup bien apres = tir sur un cadavre
	};
	constexpr int kMaxStates = 32;
	ActorState g_states[kMaxStates] = {};

	ActorState& StateOf(void* ref)
	{
		UInt32 id = RefID(ref);
		ActorState* oldest = &g_states[0];
		for (ActorState& s : g_states)
		{
			if (s.refID == id) return s;
			if (s.lastHit < oldest->lastHit) oldest = &s;
		}
		*oldest = {id, 0.f, GetTickCount(), 0, 0, 0, 0, 0};
		return *oldest;
	}

	float AddCharge(ActorState& s, float amount)
	{
		DWORD now = GetTickCount();
		float dt = (now - s.lastHit) / 1000.f;
		if (g_cfg.stressHalfLife > 0.f) s.charge *= powf(0.5f, dt / g_cfg.stressHalfLife);
		s.charge += amount;
		s.lastHit = now;
		return s.charge;
	}

	// ------------------------------------------------------------------ titube et tombe (fonctions du jeu)
	void* ProcessOf(void* actorRef) { return At<void*>(actorRef, actor::kProcess); }

	bool IsHighProcess(void* actorRef)
	{
		void* p = ProcessOf(actorRef);
		return p && At<UInt32>(p, actor::kProcess_Level) == 0;
	}

	SInt32 KnockedState(void* actorRef)
	{
		void* p = ProcessOf(actorRef);
		if (!p || At<UInt32>(p, actor::kProcess_Level) > 1) return -1;
		return At<signed char>(p, actor::kProcess_Knocked);
	}

	// Vitesse de deplacement de l'acteur (controleur de personnage), unites Havok/s
	V3 MoveVelocity(void* actorRef)
	{
		void* p = ProcessOf(actorRef);
		if (!p || At<UInt32>(p, actor::kProcess_Level) > 1) return {0, 0, 0};
		void* cc = At<void*>(p, actor::kProcess_CharCtrl);
		return cc ? Load3(cc, actor::kCharCtrl_Velocity) : V3{0, 0, 0};
	}

	UInt32 g_staggerGroup = 0xFFFFFFFF; // cherche par son nom au demarrage

	void FindAnimGroups()
	{
		for (UInt32 i = 0; i < anim::kGroupCount; ++i)
		{
			const char* name = At<const char*>((void*)anim::kAddr_GroupInfos, i * anim::kGroupInfoSize);
			if (name && !_stricmp(name, "Stagger")) g_staggerGroup = i;
		}
		if (g_staggerGroup != 0xFFFFFFFF) Log("Groupe d'animation Stagger : %02X", g_staggerGroup);
		else Log("ERREUR : groupe d'animation Stagger introuvable, titubement desactive");
	}

	// Joue le groupe d'animation sur l'acteur (meme suite d'appels que la commande PlayGroup)
	bool PlayAnimGroup(void* actorRef, UInt32 group)
	{
		using GetAnimData = void* (__thiscall*)(void*);
		void* ad = ((GetAnimData)(*(void***)actorRef)[actor::kVtbl_GetAnimData / 4])(actorRef);
		if (!ad) return false;
		UInt32 full = ((UInt32(__thiscall*)(void*, UInt32, UInt32, UInt32, void*))anim::kAddr_ResolveGroup)(actorRef, group, 0, 0, ad);
		((void(__thiscall*)(void*, UInt32, UInt32, SInt32, SInt32))anim::kAddr_PlayGroup)(ad, full & 0xFFFF, 1, -1, -1);
		((void(__thiscall*)(void*, UInt32, UInt32))anim::kAddr_ActorAnimNotify)(actorRef, 0xE, At<UInt32>(ad, anim::kAnimData_E4));
		((void(__thiscall*)(void*, UInt32))anim::kAddr_ActorAnimDone)(actorRef, 1);
		return true;
	}

	// Renverse l'acteur loin du point src (unites du jeu), comme la commande PushActorAway
	void KnockDown(void* actorRef, V3 src, float force)
	{
		void* p = ProcessOf(actorRef);
		using Fn = void(__thiscall*)(void*, void*, float, float, float, float);
		((Fn)(*(void***)p)[actor::kProcess_PushActorAway / 4])(p, actorRef, src.x, src.y, src.z, force);
	}

	// ------------------------------------------------------------------ reaction a un coup
	// Etape 7 : decrit une fois le squelette de chaque type de creature (forme de base) : nombre de corps, masse,
	// torse retenu, et les noms des os physiques.
	void LogCreatureSkeleton(void* ref, const Body* b, int n, float mass)
	{
		static UInt32 seen[64] = {};
		static int count = 0;
		UInt32 base = RefID(At<void*>(ref, refr::kBaseForm));
		for (int k = 0; k < count; ++k)
			if (seen[k] == base) return;
		if (count < 64) seen[count++] = base;
		char names[512];
		int len = 0;
		names[0] = 0;
		for (int k = 0; k < n && len < (int)sizeof(names) - 40; ++k)
			len += snprintf(names + len, sizeof(names) - len, "%s%s", k ? ", " : "", b[k].name ? b[k].name : "?");
		int chest = ChestOf(b, n);
		Log("Creature %08X : %d corps, masse %.1f (humain %.1f), torse %s, %s : %s", base, n, mass, g_humanMass,
			chest >= 0 && b[chest].name ? b[chest].name : "?", IsHumanoid(b, n) ? "humanoide" : "non humanoide", names);
	}

	// Element du coup : par la resistance de l'arme (feu, electricite, IEM), sinon par le type du projectile (flamme)
	int ElementOf(void* weapon, void* proj, void* attacker, bool explosion)
	{
		if (weapon && At<UInt8>(weapon, form::kTypeID) == 0x28)
		{
			SInt32 r = At<SInt32>(weapon, weap::kResistType);
			if (r == av::kFireResist) return kElem_Fire;
			if (r == av::kElectricResist || r == av::kEmpResist) return kElem_Shock;
		}
		if (proj && proj != attacker && !explosion)
		{
			void* base = At<void*>(proj, refr::kBaseForm);
			if (base && At<UInt8>(base, form::kTypeID) == 0x33 && At<UInt16>(base, projform::kType) == projform::kType_Flame)
				return kElem_Fire;
		}
		return kElem_None;
	}

	void React(void* target, void* attacker)
	{
		void* hd = HitDataOf(target);
		if (!hd) { if (g_cfg.verbose) Log("Coup sur %08X : pas de donnees de coup", RefID(target)); return; }

		Body b[kMaxBodies];
		int n = CollectBodies(target, b);
		if (!n) return;

		bool creature = At<UInt8>(target, form::kTypeID) == form::kType_Creature;
		float mass = TotalMass(b, n);

		// etape 8 : le squelette humain doit porter ses corps physiques (NV Compatibility Skeleton en a 18).
		// Sa masse sert de reference pour les creatures (etape 7).
		static bool skeletonChecked = false;
		if (!skeletonChecked && !creature)
		{
			skeletonChecked = true;
			if (n < 15 || FindBody(b, n, "Bip01 Spine2") < 0)
				Log("ATTENTION : le premier PNJ touche n'a que %d corps physiques (18 attendus avec NV Compatibility Skeleton). "
					"Un autre squelette l'ecrase peut-etre : charge NV Compatibility Skeleton en dernier.", n);
			else
			{
				g_humanMass = mass;
				Log("Squelette humain : %d corps physiques, masse %.1f, ok", n, mass);
			}
		}

		// etape 7 : chaque type de creature est decrit une fois dans le journal (utile pour regler)
		if (creature) LogCreatureSkeleton(target, b, n, mass);

		// taille : les seuils de titubement et de chute suivent la masse (un deathclaw ne tombe pas comme un rat-taupe)
		float size = 1.f;
		if (g_cfg.massScaling && g_humanMass > 0.f && mass > 0.f)
		{
			size = sqrtf(mass / g_humanMass);
			if (size < 0.5f) size = 0.5f;
			if (size > 3.f) size = 3.f;
		}
		float thresholdMult = size * (creature ? g_cfg.creatureThreshold : 1.f);

		UInt32 flags = At<UInt32>(hd, hit::kFlags);
		float dmg = At<float>(hd, hit::kHealthDmg);
		void* proj = At<void*>(hd, hit::kProjectile);
		bool explosion = (flags & hit::kFlag_IsExplosionHit) != 0;
		bool melee = !explosion && (!proj || proj == attacker);
		void* weapon = At<void*>(hd, hit::kWeapon);
		int elem = g_cfg.elements ? ElementOf(weapon, proj, attacker, explosion) : kElem_None;
		V3 impact = Load3(hd, hit::kImpactPos);
		bool hasImpact = Len(impact) > 1.f;

		// corps touche : le plus proche du point d'impact, sinon le torse
		int i = hasImpact ? NearestBody(b, n, impact * hk::kGameToHavok) : -1;
		if (i < 0) i = ChestOf(b, n);
		if (i < 0) i = 0;
		V3 p = hasImpact ? impact * hk::kGameToHavok : BodyCenter(b[i].rb);

		// direction : de la source (tireur, ou centre de l'explosion) vers le point touche, a plat, plus un peu de hauteur
		void* from = explosion && proj ? proj : attacker;
		V3 src = from ? Load3(from, refr::kPos) * hk::kGameToHavok : p - V3{0, 1, 0};
		V3 dir = p - src;
		dir.z = 0.f;
		dir = Norm(dir);
		dir.z = explosion ? g_cfg.explosionUp : g_cfg.up; // une explosion souleve
		dir = Norm(dir);

		// force du coup ; un fusil a pompe compte tous ses plombs (xNVSE ne signale qu'un coup par image)
		int pellets = (!explosion && weapon) ? At<UInt8>(weapon, weap::kNumProjectiles) : 1;
		if (pellets < 1) pellets = 1;
		float force = (g_cfg.basePush + dmg * g_cfg.pushPerDamage) * pellets;
		if (melee) force *= g_cfg.meleeMult;
		if (explosion) force *= g_cfg.explosionMult;
		if (flags & hit::kFlag_IsCritical) force *= g_cfg.criticalMult;
		float push = force > g_cfg.maxPush ? g_cfg.maxPush : force; // impulsion physique, plafonnee

		// Coup fatal : le drapeau du jeu, ou l'acteur deja mourant. Sinon on ne le sait pas encore : la poussee de mort
		// est gardee en attente et ne servira que s'il est mort quand son ragdoll demarre.
		bool fatal = (flags & hit::kFlag_IsFatal) != 0 || IsDead(target);
		bool living = g_cfg.livingReactions; // 0 par defaut : les vivants restent ceux du jeu de base
		float deathPush = push * g_cfg.fatalMult;

		// classement des vivants : tressaille, titube, ou tombe selon la charge accumulee (si living)
		ActorState& st = StateOf(target);
		float charge = living ? AddCharge(st, force) : 0.f;
		if (!living) st.lastHit = GetTickCount(); // garde la case de ce PNJ (les plus anciennes sont reprises)
		// cadavre : deja mort depuis plus d'une seconde ; il est seulement pousse, sans nouvelle facon de mourir
		// en agonie : la balle le pousse (il titube de lui-meme) et lui retire des forces
		bool corpse = false, agony = false;
		if (IsDead(target))
		{
			if (!st.deathTick) st.deathTick = GetTickCount();
			agony = WeakenDying(target);
			corpse = agony || GetTickCount() - st.deathTick > 1000;
		}
		SInt32 knocked = KnockedState(target);
		bool standing = living && knocked == 0 && IsHighProcess(target) && target != Player();
		// Explosion Reactions joue deja ses animations sur un vivant touche par une explosion : on lui laisse la place
		if (explosion && g_explosionReactions && g_cfg.yieldExplosionReactions) standing = false;
		const char* reaction = living ? "tressaille" : "vivant : jeu de base";
		V3 carry = MoveVelocity(target) * g_cfg.carryVelocity;
		V3 deathCarry = carry;
		float staggerAt = g_cfg.staggerAt * thresholdMult;
		float knockAt = g_cfg.knockAt * thresholdMult;
		bool canStagger = g_cfg.stagger && (!creature || g_cfg.creatureStagger);
		bool canKnock = g_cfg.knockdown && (!creature || g_cfg.creatureKnockdown);

		// etape 6 : en feu ou electrocute pour un moment ; le premier coup de feu ou de courant fait tituber
		DWORD now = GetTickCount();
		bool elemStagger = false;
		if (elem && !fatal && living)
		{
			DWORD& until = elem == kElem_Fire ? st.burnUntil : st.shockUntil;
			float time = elem == kElem_Fire ? g_cfg.burnTime : g_cfg.shockTime;
			elemStagger = until <= now;
			until = now + (DWORD)(time * 1000.f);
		}

		// etape 5 : facon de mourir (etape 6 : et chute) ; le feu et le courant remplacent main sur la plaie et debout
		// Facon de mourir, choisie a chaque coup (on ne sait pas toujours lequel sera fatal) : elle ne sert que si le PNJ
		// meurt. Facon Euphoria, exageree : presque chaque mort (tir a la tete compris) commence par une agonie debout :
		// il cherche son equilibre, fait des pas, encaisse les balles, puis ses jambes cedent et il se tord au sol.
		// Seules les explosions, le feu, le courant et les degats enormes (fMaxDamage) le jettent directement.
		DeathStyle ds = {};
		ds.element = elem;
		ds.explosion = explosion;
		{
			const char* bone = b[i].name ? b[i].name : "";
			bool head = strstr(bone, "Head") || strstr(bone, "Neck");
			float total = dmg * pellets;
			bool muscles = g_cfg.muscles && g_cfg.musclesOnDeath && !elem; // la facon de mourir passe par les muscles
			ds.headWound = head;
			ds.limp = g_cfg.headshotLimp && head && !elem && !explosion;
			if (muscles && !ds.limp)
			{
				if (g_cfg.grab && !explosion && IsHumanoid(b, n) && Chance(g_cfg.grabChance))
				{
					ds.grab = true;
					snprintf(ds.wound, sizeof(ds.wound), "%s", bone);
					ds.woundLocal = ToLocal(b[i].rb, p - BodyCenter(b[i].rb));
					// la main du cote oppose a un bras touche ; sinon au hasard
					ds.grabHand = strstr(bone, " L ") ? 1 : strstr(bone, " R ") ? 0 : (rand() & 1);
				}
				if (g_cfg.balance && !explosion && total < g_cfg.balanceMaxDamage)
				{
					ds.balance = true;
					float r = (float)rand() / RAND_MAX;
					ds.upTime = g_cfg.balanceTimeMin + r * (g_cfg.balanceTimeMax - g_cfg.balanceTimeMin);
					deathPush *= g_cfg.balancePushMult; // la balle le deseguilibre au lieu de le jeter : il titube
				}
				ds.writhe = g_cfg.writhe && !explosion && total < g_cfg.writheMaxDamage && Chance(g_cfg.writheChance);
			}
		}
		if (fatal)
		{
			reaction = ds.limp ? "meurt, s'effondre net" : ds.balance ? "agonie debout" : elem == kElem_Fire ? "meurt en feu"
				: elem == kElem_Shock ? "meurt electrocute" : explosion ? "meurt, projete" : "meurt";
		}
		else if (standing && canKnock && charge >= knockAt)
		{
			float scale = charge / knockAt;
			if (scale > g_cfg.knockForceMax) scale = g_cfg.knockForceMax;
			void* srcRef = explosion && proj ? proj : attacker;
			V3 srcGame = srcRef ? Load3(srcRef, refr::kPos) : (p - dir) * 7.f;
			KnockDown(target, srcGame, g_cfg.knockForce * scale * size * (explosion ? g_cfg.explosionKnockMult : 1.f));
			// le ragdoll commence une image plus tard : poussee au point touche et elan, des qu'il est dynamique
			QueuePush(target, "Chute", b[i].name, dir * (push * g_cfg.knockLocalShare), p - BodyCenter(b[i].rb), carry, &ds);
			st.charge = 0.f;
			reaction = explosion ? "tombe, projete" : "tombe";
		}
		else if (standing && canStagger && (charge >= staggerAt || elemStagger) && g_staggerGroup != 0xFFFFFFFF
			&& GetTickCount() - st.lastStagger > (DWORD)(g_cfg.staggerCooldown * 1000.f))
		{
			if (PlayAnimGroup(target, g_staggerGroup)) { st.lastStagger = GetTickCount(); reaction = "titube"; }
		}

		// Poussee tout de suite : sur un vivant seulement si living ; a la mort, si le ragdoll est deja la
		int pushed = (living || fatal) ? PushBodyAndNeighbours(b, n, i, dir * (fatal ? deathPush : push), p) : 0;
		bool deferred = false;
		if (corpse)
			reaction = agony ? "agonie : encaisse la balle" : "cadavre : poussee seulement";
		else if (fatal && pushed)
		{
			// deja en ragdoll au moment du coup fatal : elan et muscles tout de suite
			AddVelocityToAll(b, n, deathCarry);
			StartMuscles(target, true, deathCarry + dir, &ds);
		}
		else if (g_cfg.deferFatal && !pushed)
		{
			// gardee jusqu'au ragdoll de mort ; sans effet si le PNJ est encore vivant a ce moment-la
			QueuePush(target, "Mort", b[i].name, dir * deathPush, p - BodyCenter(b[i].rb), deathCarry, &ds);
			deferred = fatal;
		}
		if (g_cfg.verbose)
			Log("Coup sur %s %08X par %08X : %s%s%s%s, degats %.1f x%d, drapeaux %X, os %s (%s), force %.1f, "
				"charge %.1f (seuils %.0f / %.0f, taille %.2f) -> %s, poussee %.1f, %d corps pousses%s",
				creature ? "creature" : "PNJ", RefID(target), RefID(attacker), explosion ? "explosion" : melee ? "melee" : "tir",
				elem ? " de " : "", ElementName(elem), fatal ? " FATAL" : "",
				dmg, pellets, flags, b[i].name ? b[i].name : "?", hk::MotionName(At<UInt8>(b[i].rb, hk::kMotionType)),
				force, charge, staggerAt, knockAt, size, reaction, fatal ? deathPush : push, pushed,
				deferred ? " (mis en attente du ragdoll de mort)"
				: pushed || !living || fatal ? "" : " (corps animes : le Ragdoll Feedback ne les a pas rendus dynamiques)");
	}

	// Etape 6 : PNJ vivants en feu ou electrocutes. Si le Ragdoll Feedback rend leurs corps dynamiques, les mains
	// battent (feu) ou le haut du corps tremble (courant). Sinon il ne reste que le titubement du premier coup.
	void UpdateLiving()
	{
		DWORD now = GetTickCount();
		for (ActorState& s : g_states)
		{
			bool burning = s.burnUntil > now, shocked = s.shockUntil > now;
			if (!s.refID || (!burning && !shocked)) continue;
			void* ref = forms::LookupByID(s.refID);
			if (!ref || !IsActor(ref) || KnockedState(ref) != 0) continue;
			Body b[kMaxBodies];
			int n = CollectBodies(ref, b);
			if (burning && now >= s.nextFlail)
			{
				s.nextFlail = now + (DWORD)(g_cfg.flailTimer * 1000.f);
				// humain : avant-bras et mains ; creature : le bout de chaque membre
				int ends[8];
				int ne = 0;
				if (IsHumanoid(b, n))
				{
					for (const char* limb : {"Bip01 L Forearm", "Bip01 R Forearm", "Bip01 L Hand", "Bip01 R Hand"})
						if ((ends[ne] = FindBody(b, n, limb)) >= 0) ++ne;
				}
				else ne = LimbEnds(b, n, ChestOf(b, n), ends, 8);
				for (int j = 0; j < ne; ++j)
					ApplyImpulse(b[ends[j]].rb, RandomDir() * g_cfg.livingFlail, BodyCenter(b[ends[j]].rb));
			}
			if (shocked)
			{
				// torse et racines des membres (tete et bras chez l'humain)
				int chest = ChestOf(b, n);
				int parts[9];
				int np = LimbRoots(b, n, chest, PelvisOf(b, n), parts, 8);
				if (chest >= 0) parts[np++] = chest;
				for (int j = 0; j < np; ++j)
					ApplyImpulse(b[parts[j]].rb, RandomDir() * g_cfg.livingShake, BodyCenter(b[parts[j]].rb));
			}
		}
	}

	void OnHit(void* /*thisObj*/, void* parameters)
	{
		if (!g_cfg.enabled) return;
		void** args = (void**)parameters;
		void* a0 = args[0];
		void* a1 = args[1];
		// xNVSE passe (victime, attaquant) ; on verifie avec les donnees de coup plutot que de le supposer.
		void* target = a0;
		void* attacker = a1;
		__try
		{
			if (!IsActor(target) || (IsActor(a1) && HitDataOf(a1) && At<void*>(HitDataOf(a1), hit::kTarget) == a1
				&& !(HitDataOf(a0) && At<void*>(HitDataOf(a0), hit::kTarget) == a0)))
			{
				target = a1;
				attacker = a0;
			}
			if (!IsActor(target)) return;
			if (target == Player() && !g_cfg.reactPlayer) return;
			if (At<UInt8>(target, form::kTypeID) == form::kType_Creature && !g_cfg.creatures) return;
			React(target, attacker);
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			Log("ERREUR : exception %08X pendant OnHit", GetExceptionCode());
		}
	}

	// ------------------------------------------------------------------ touches de test
	bool GameHasFocus()
	{
		DWORD pid = 0;
		GetWindowThreadProcessId(GetForegroundWindow(), &pid);
		return pid == GetCurrentProcessId();
	}

	bool Pressed(int vk, bool& wasDown)
	{
		bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
		bool edge = down && !wasDown;
		wasDown = down;
		return edge;
	}

	void* CrosshairActor()
	{
		void* im = *(void**)ui::kAddr_InterfaceManager;
		void* ref = im ? At<void*>(im, ui::kCrosshairRef) : nullptr;
		return IsActor(ref) ? ref : nullptr;
	}

	void TestPush(void* target)
	{
		Body b[kMaxBodies];
		int n = CollectBodies(target, b);
		if (!n) { Log("F7 : aucun corps physique sur %08X", RefID(target)); return; }
		int i = ChestOf(b, n);
		if (i < 0) i = 0;
		V3 p = BodyCenter(b[i].rb);
		V3 from = Load3(Player(), refr::kPos) * hk::kGameToHavok;
		V3 dir = p - from;
		dir.z = 0.f;
		dir = Norm(dir);
		dir.z = g_cfg.up;
		dir = Norm(dir);
		int pushed = PushBodyAndNeighbours(b, n, i, dir * g_cfg.testPush, p);
		Log("F7 : poussee %.0f sur %s de %08X, %d corps pousses (%s)", g_cfg.testPush, b[i].name ? b[i].name : "?",
			RefID(target), pushed, hk::MotionName(At<UInt8>(b[i].rb, hk::kMotionType)));
	}

	void PollKeys()
	{
		static bool dumpDown = false, pushDown = false;
		bool focus = GameHasFocus();
		bool dump = Pressed(g_cfg.keyDump, dumpDown) && focus;
		bool push = Pressed(g_cfg.keyPush, pushDown) && focus;
		if (!dump && !push) return;
		__try
		{
			void* target = CrosshairActor();
			if (!target) { Log("Touche de test : aucun acteur sous le viseur"); return; }
			if (dump) DumpBodies(target);
			if (push) { TestPush(target); DumpBodies(target); }
		}
		__except (EXCEPTION_EXECUTE_HANDLER)
		{
			Log("ERREUR : exception %08X pendant une touche de test", GetExceptionCode());
		}
	}

	// ------------------------------------------------------------------ messages NVSE
	bool DataFileExists(const char* relative)
	{
		char path[MAX_PATH];
		snprintf(path, sizeof(path), "%sData\\%s", g_gameDir, relative);
		return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
	}

	// Etape 8 : mods qui font le meme travail. Sous Mod Organizer, seuls les mods actives sont visibles dans Data.
	void CheckOtherMods()
	{
		if (DataFileExists("Ragdolls.esm"))
			Log("ATTENTION : Ragdolls.esm est present. Ce mod le remplace : desactive Ragdolls pour eviter deux reactions "
				"a la mort et deux reglages du Ragdoll Feedback.");
		g_explosionReactions = DataFileExists("NVSE\\Plugins\\Scripts\\ln_ExplosionReactions.txt");
		if (g_explosionReactions)
			Log("Explosion Reactions detecte : %s", g_cfg.yieldExplosionReactions
				? "les PNJ vivants touches par une explosion jouent ses animations, sans titubement ni chute de notre part"
				: "bYieldToExplosionReactions=0 : nos titubements et chutes s'ajoutent aux siennes");
	}

	void OnMessage(NVSEMessagingInterface::Message* msg)
	{
		switch (msg->type)
		{
		case NVSEMessagingInterface::kMessage_PostLoad:
			if (g_events && g_events->SetNativeEventHandler("OnHit", OnHit))
				Log("Ecoute de l'evenement OnHit : ok");
			else
				Log("ERREUR : impossible d'ecouter OnHit (xNVSE trop ancien ?)");
			break;
		case NVSEMessagingInterface::kMessage_DeferredInit:
			Log("Reglages natifs [RagdollAnim] (avant -> apres) :");
			ApplyGameSettings();
			CheckOtherMods();
			__try { FindAnimGroups(); }
			__except (EXCEPTION_EXECUTE_HANDLER) { Log("ERREUR : table des groupes d'animation illisible"); }
			break;
		case NVSEMessagingInterface::kMessage_PostLoadGame:
			LoadConfig(); // relu a chaque chargement de partie, pour regler sans relancer le jeu
			ApplyGameSettings();
			ClearPending();
			ClearMuscles();
			break;
		case NVSEMessagingInterface::kMessage_MainGameLoop:
			CheckIniChanged();
			PollKeys();
			__try { ProcessPending(); }
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				Log("ERREUR : exception %08X pendant une poussee differee", GetExceptionCode());
				ClearPending();
			}
			__try { UpdateLiving(); }
			__except (EXCEPTION_EXECUTE_HANDLER) { Log("ERREUR : exception %08X pendant feu / electricite", GetExceptionCode()); }
			__try { UpdateMuscles(); }
			__except (EXCEPTION_EXECUTE_HANDLER)
			{
				Log("ERREUR : exception %08X pendant les muscles", GetExceptionCode());
				ClearMuscles();
			}
			break;
		}
	}
}

extern "C"
{
	__declspec(dllexport) bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info)
	{
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = kPluginName;
		info->version = kPluginVersion;
		if (nvse->isEditor) return false; // rien a faire dans le GECK
		return nvse->runtimeVersion == kRuntime_1_4_0_525;
	}

	__declspec(dllexport) bool NVSEPlugin_Load(NVSEInterface* nvse)
	{
		snprintf(g_gameDir, sizeof(g_gameDir), "%s", nvse->GetRuntimeDirectory ? nvse->GetRuntimeDirectory() : "");
		char logPath[MAX_PATH];
		snprintf(logPath, sizeof(logPath), "%sNVReactiveRagdoll.log", g_gameDir);
		g_log = _fsopen(logPath, "w", _SH_DENYWR);
		// Data\Config\ : c'est la que le menu MCM (MCM Extender) lit et ecrit. Ancienne place (0.1.0) acceptee.
		snprintf(g_iniPath, sizeof(g_iniPath), "%sData\\Config\\NVReactiveRagdoll.ini", g_gameDir);
		if (GetFileAttributesA(g_iniPath) == INVALID_FILE_ATTRIBUTES)
		{
			char old[MAX_PATH];
			snprintf(old, sizeof(old), "%sData\\NVSE\\Plugins\\NVReactiveRagdoll.ini", g_gameDir);
			if (GetFileAttributesA(old) != INVALID_FILE_ATTRIBUTES) snprintf(g_iniPath, sizeof(g_iniPath), "%s", old);
		}

		Log("NV Reactive Ragdoll %s, xNVSE %08X, jeu %08X", kVersionName, nvse->nvseVersion, nvse->runtimeVersion);
		srand(GetTickCount());
		LoadConfig();

		g_handle = nvse->GetPluginHandle();
		g_messaging = (NVSEMessagingInterface*)nvse->QueryInterface(kInterface_Messaging);
		g_events = (NVSEEventManagerInterface*)nvse->QueryInterface(kInterface_EventManager);
		if (!g_messaging || !g_messaging->RegisterListener(g_handle, "NVSE", OnMessage))
		{
			Log("ERREUR : interface de messages NVSE indisponible");
			return false;
		}
		if (!g_events) Log("ERREUR : interface d'evenements xNVSE indisponible (xNVSE 6.1+ requis)");
		return true;
	}
}
