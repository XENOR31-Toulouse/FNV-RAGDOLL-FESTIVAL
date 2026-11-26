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
	constexpr UInt32 kPluginVersion = 1;
	constexpr const char* kPluginName = "NVReactiveRagdoll";

	PluginHandle g_handle = 0xFFFFFFFF;
	NVSEMessagingInterface* g_messaging = nullptr;
	NVSEEventManagerInterface* g_events = nullptr;
	char g_gameDir[MAX_PATH] = {};
	char g_iniPath[MAX_PATH] = {};
	FILE* g_log = nullptr;

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
		float fatalWait = 0.6f;
		float fatalMult = 1.0f;
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
		float grabChance = 0.6f;
		float grabMaxDamage = 150.f;
		float grabPull = 1.5f;
		bool stand = true;
		float standChance = 0.5f;
		float standMaxDamage = 150.f;
		float standMaxSpeed = 2.f;
		float standTime = 0.8f;
		float standSupport = 0.9f;
		float standPushMult = 0.3f;
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
		c.grabMaxDamage = IniFloat("Death", "fGrabMaxDamage", c.grabMaxDamage);
		c.grabPull = IniFloat("Death", "fGrabPull", c.grabPull);
		c.stand = IniInt("Death", "bStanding", 1) != 0;
		c.standChance = IniFloat("Death", "fStandingChance", c.standChance);
		c.standMaxDamage = IniFloat("Death", "fStandingMaxDamage", c.standMaxDamage);
		c.standMaxSpeed = IniFloat("Death", "fStandingMaxSpeed", c.standMaxSpeed);
		c.standTime = IniFloat("Death", "fStandingTime", c.standTime);
		c.standSupport = IniFloat("Death", "fStandingSupport", c.standSupport);
		c.standPushMult = IniFloat("Death", "fStandingPushMult", c.standPushMult);
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
		c.forceFeedback = IniInt("RagdollAnim", "bRagdollFeedback", 1) != 0;
		g_cfg = c;
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
		bool stand;       // reste debout un instant, puis s'effondre
		int element;      // Element : le feu fait battre bras et jambes, l'electricite fait trembler
		bool explosion;   // projete en l'air : bras en moulinet et jambes qui pedalent tant qu'il vole
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
	};
	constexpr int kMaxMuscles = 8;
	SInt32 KnockedState(void* actorRef); // plus bas
	MuscleTask g_muscles[kMaxMuscles] = {};

	void ClearMuscles() { memset(g_muscles, 0, sizeof(g_muscles)); }

	void StartMuscles(void* target, bool dying, V3 fallDir, const DeathStyle* death)
	{
		if (!g_cfg.muscles || (dying ? !g_cfg.musclesOnDeath : !g_cfg.musclesOnKnockdown)) return;
		MuscleTask* slot = nullptr;
		for (MuscleTask& m : g_muscles)
		{
			if (m.refID == RefID(target)) { slot = &m; break; }
			if (!slot && !m.refID) slot = &m;
		}
		if (!slot) return; // trop de PNJ au sol en meme temps : les suivants tombent sans muscles
		fallDir.z = 0.f;
		DeathStyle ds = death ? *death : DeathStyle{};
		float duration = dying ? g_cfg.muscleDeathTime + (ds.stand ? g_cfg.standTime : 0.f) : g_cfg.muscleTime;
		if (ds.element == kElem_Fire && g_cfg.burnDeathTime > duration) duration = g_cfg.burnDeathTime;
		if (ds.element == kElem_Shock && g_cfg.shockTime > duration) duration = g_cfg.shockTime;
		if (ds.explosion && g_cfg.flyTime + 0.5f > duration) duration = g_cfg.flyTime + 0.5f;
		*slot = {RefID(target), target, GetTickCount(), GetTickCount(), duration, dying, Norm(fallDir), 0.f, ds};
		if (g_cfg.verbose)
			Log("Muscles de %08X : debut (%s%s%s%s%s%s%s), %.1f s", slot->refID, dying ? "mort" : "chute",
				ds.stand ? ", reste debout" : "", ds.grab ? ", main sur la plaie : " : "", ds.grab ? ds.wound : "",
				ds.element ? ", " : "", ElementName(ds.element), ds.explosion ? ", projete" : "", duration);
	}

	// inverse d'inertie moyenne (scalaire) : suffisant pour repartir un effort entre deux os
	float InvInertia(UInt8* rb) { V3 i = Load3(rb, hk::kInvInertia); return (i.x + i.y + i.z) / 3.f; }

	void AddLinVel(UInt8* rb, V3 dv) { float* v = &At<float>(rb, hk::kLinVelocity); v[0] += dv.x; v[1] += dv.y; v[2] += dv.z; }
	void AddAngVel(UInt8* rb, V3 dw) { float* w = &At<float>(rb, hk::kAngVelocity); w[0] += dw.x; w[1] += dw.y; w[2] += dw.z; }

	// Tire la main i vers target, la poitrine c en sens inverse (ressort amorti, PD)
	void PullHand(const Body* b, int hand, int chest, V3 target, float strength, float dt)
	{
		UInt8* h = b[hand].rb;
		UInt8* c = b[chest].rb;
		V3 e = target - BodyCenter(h);
		V3 vrel = Load3(h, hk::kLinVelocity) - Load3(c, hk::kLinVelocity);
		V3 dv = (e * g_cfg.armPull - vrel * g_cfg.armDamping) * (dt * strength);
		float l = Len(dv);
		if (l > g_cfg.armMaxDv) dv = dv * (g_cfg.armMaxDv / l);
		float imh = At<float>(h, hk::kInvMass), imc = At<float>(c, hk::kInvMass);
		if (imh + imc <= 0.f) return;
		V3 J = dv * (1.f / (imh + imc)); // impulsion qui change la vitesse relative de dv
		AddLinVel(h, J * imh);
		AddLinVel(c, J * -imc);
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
		int chest = FindBody(b, n, "Bip01 Spine2");
		if (chest < 0 || !hk::IsDynamic(At<UInt8>(b[chest].rb, hk::kMotionType))) return false; // plus de ragdoll

		// force : pleine puis decroissante (a la mort elle decroit des le debut : les muscles lachent,
		// apres le temps passe debout s'il y en a un)
		float t = age / m.duration;
		float standT = (m.dying && m.death.stand) ? g_cfg.standTime : 0.f;
		float strength = m.dying ? (age < standT ? 1.f : 1.f - (age - standT) / (m.duration - standT))
			: (t < 0.6f ? 1.f : (1.f - t) / 0.4f);
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
			for (const char* leg : {"Bip01 L Calf", "Bip01 R Calf"})
			{
				int k = FindBody(b, n, leg);
				if (k < 0 || !hk::IsDynamic(At<UInt8>(b[k].rb, hk::kMotionType))) continue;
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
			const char* limbs[4] = {"Bip01 L UpperArm", "Bip01 R UpperArm", "Bip01 L Thigh", "Bip01 R Thigh"};
			const float spin[4] = {1.f, 1.f, 1.f, -1.f};
			UInt8* c = b[chest].rb;
			float ic = InvInertia(c);
			for (int l = 0; l < 4; ++l)
			{
				int k = FindBody(b, n, limbs[l]);
				if (k < 0 || !hk::IsDynamic(At<UInt8>(b[k].rb, hk::kMotionType))) continue;
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

		// 0. reste debout un instant : poitrine et bassin portes contre la gravite, de moins en moins, les jambes cedent
		if (age < standT)
		{
			float support = g_cfg.standSupport * (1.f - age / standT);
			V3 lift = GravityOf(b[chest].rb) * (-support * dt);
			int pelvis = FindBody(b, n, "Bip01 Pelvis");
			AddLinVel(b[chest].rb, lift);
			touched[chest] = true;
			if (pelvis >= 0 && hk::IsDynamic(At<UInt8>(b[pelvis].rb, hk::kMotionType)))
			{
				AddLinVel(b[pelvis].rb, lift);
				touched[pelvis] = true;
			}
		}

		// 1. tension des articulations
		for (int k = 0; k < n; ++k)
		{
			int p = b[k].parent;
			if (p < 0) continue;
			UInt8* ck = b[k].rb;
			UInt8* cp = b[p].rb;
			if (!hk::IsDynamic(At<UInt8>(ck, hk::kMotionType)) || !hk::IsDynamic(At<UInt8>(cp, hk::kMotionType))) continue;
			float damp = g_cfg.jointDamping;
			if (b[k].name && (strstr(b[k].name, "Head") || strstr(b[k].name, "Neck"))) damp = g_cfg.neckDamping;
			if (elem == kElem_Shock) damp *= 2.f; // muscles contractes par le courant
			damp *= strength;
			if (damp > 0.9f) damp = 0.9f;
			damp = 1.f - powf(1.f - damp, dt * 60.f); // reglage donne pour 60 images/s, meme effet a tout framerate
			float ik = InvInertia(ck), ip = InvInertia(cp);
			if (ik + ip <= 0.f) continue;
			V3 wrel = Load3(ck, hk::kAngVelocity) - Load3(cp, hk::kAngVelocity);
			V3 T = wrel * (-damp / (ik + ip)); // impulsion angulaire qui retire damp de la rotation relative
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
			if (p.refID == RefID(target)) { slot = &p; break; } // nouveau coup fatal sur le meme PNJ : on remplace
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
			Body b[kMaxBodies];
			int n = CollectBodies(p.ref, b);
			int i = n ? FindBody(b, n, p.bone) : -1;
			if (i >= 0 && hk::IsDynamic(At<UInt8>(b[i].rb, hk::kMotionType)))
			{
				int carried = AddVelocityToAll(b, n, p.carry);
				int pushed = PushBodyAndNeighbours(b, n, i, p.J, BodyCenter(b[i].rb) + p.offset);
				StartMuscles(p.ref, !strcmp(p.what, "Mort"), p.carry + Norm(p.J), &p.death);
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
		*oldest = {id, 0.f, GetTickCount(), 0, 0, 0, 0};
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
		if (i < 0) i = FindBody(b, n, "Bip01 Spine2");
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

		bool fatal = (flags & hit::kFlag_IsFatal) != 0;
		if (fatal) push *= g_cfg.fatalMult;

		// classement : tressaille (toujours), titube, ou tombe selon la charge accumulee
		ActorState& st = StateOf(target);
		float charge = AddCharge(st, force);
		SInt32 knocked = KnockedState(target);
		bool standing = knocked == 0 && IsHighProcess(target) && target != Player();
		const char* reaction = "tressaille";
		V3 carry = MoveVelocity(target) * g_cfg.carryVelocity;

		// etape 6 : en feu ou electrocute pour un moment ; le premier coup de feu ou de courant fait tituber
		DWORD now = GetTickCount();
		bool elemStagger = false;
		if (elem && !fatal)
		{
			DWORD& until = elem == kElem_Fire ? st.burnUntil : st.shockUntil;
			float time = elem == kElem_Fire ? g_cfg.burnTime : g_cfg.shockTime;
			elemStagger = until <= now;
			until = now + (DWORD)(time * 1000.f);
		}

		// etape 5 : facon de mourir (etape 6 : et chute) ; le feu et le courant remplacent main sur la plaie et debout
		DeathStyle ds = {};
		ds.element = elem;
		ds.explosion = explosion;
		if (fatal)
		{
			reaction = "meurt";
			const char* bone = b[i].name ? b[i].name : "";
			bool head = strstr(bone, "Head") || strstr(bone, "Neck");
			float limit = head ? 0.5f : 1.f; // un tir a la tete tue net plus facilement
			float total = dmg * pellets;
			bool muscles = g_cfg.muscles && g_cfg.musclesOnDeath && !elem; // la facon de mourir passe par les muscles
			if (muscles && g_cfg.grab && !explosion && total < g_cfg.grabMaxDamage * limit && Chance(g_cfg.grabChance))
			{
				ds.grab = true;
				snprintf(ds.wound, sizeof(ds.wound), "%s", bone);
				ds.woundLocal = ToLocal(b[i].rb, p - BodyCenter(b[i].rb));
				// la main du cote oppose a un bras touche ; sinon au hasard
				ds.grabHand = strstr(bone, " L ") ? 1 : strstr(bone, " R ") ? 0 : (rand() & 1);
			}
			if (muscles && g_cfg.stand && !melee && !explosion && pellets == 1 && Len(MoveVelocity(target)) < g_cfg.standMaxSpeed
				&& total < g_cfg.standMaxDamage * limit && Chance(g_cfg.standChance))
			{
				ds.stand = true;
				push *= g_cfg.standPushMult; // la balle ne le jette pas : il reste debout puis s'effondre
				carry = {0, 0, 0};
			}
			if (ds.stand) reaction = ds.grab ? "meurt debout, main sur la plaie" : "meurt debout";
			else if (ds.grab) reaction = "meurt, main sur la plaie";
			else if (elem == kElem_Fire) reaction = "meurt en feu";
			else if (elem == kElem_Shock) reaction = "meurt electrocute";
			else if (explosion) reaction = "meurt, projete";
		}
		else if (standing && g_cfg.knockdown && charge >= g_cfg.knockAt)
		{
			float scale = charge / g_cfg.knockAt;
			if (scale > g_cfg.knockForceMax) scale = g_cfg.knockForceMax;
			void* srcRef = explosion && proj ? proj : attacker;
			V3 srcGame = srcRef ? Load3(srcRef, refr::kPos) : (p - dir) * 7.f;
			KnockDown(target, srcGame, g_cfg.knockForce * scale * (explosion ? g_cfg.explosionKnockMult : 1.f));
			// le ragdoll commence une image plus tard : poussee au point touche et elan, des qu'il est dynamique
			QueuePush(target, "Chute", b[i].name, dir * (push * g_cfg.knockLocalShare), p - BodyCenter(b[i].rb), carry, &ds);
			st.charge = 0.f;
			reaction = explosion ? "tombe, projete" : "tombe";
		}
		else if (standing && g_cfg.stagger && (charge >= g_cfg.staggerAt || elemStagger) && g_staggerGroup != 0xFFFFFFFF
			&& GetTickCount() - st.lastStagger > (DWORD)(g_cfg.staggerCooldown * 1000.f))
		{
			if (PlayAnimGroup(target, g_staggerGroup)) { st.lastStagger = GetTickCount(); reaction = "titube"; }
		}

		int pushed = PushBodyAndNeighbours(b, n, i, dir * push, p);
		bool deferred = !pushed && fatal && g_cfg.deferFatal;
		if (deferred) QueuePush(target, "Mort", b[i].name, dir * push, p - BodyCenter(b[i].rb), carry, &ds);
		else if (fatal && pushed)
		{
			// deja en ragdoll au moment du coup fatal : elan et muscles tout de suite
			AddVelocityToAll(b, n, carry);
			StartMuscles(target, true, carry + dir, &ds);
		}
		if (g_cfg.verbose)
			Log("Coup sur %08X par %08X : %s%s%s%s, degats %.1f x%d, drapeaux %X, os %s (%s), force %.1f, charge %.1f -> %s, "
				"poussee %.1f, %d corps pousses%s",
				RefID(target), RefID(attacker), explosion ? "explosion" : melee ? "melee" : "tir",
				elem ? " de " : "", ElementName(elem), fatal ? " FATAL" : "",
				dmg, pellets, flags, b[i].name ? b[i].name : "?", hk::MotionName(At<UInt8>(b[i].rb, hk::kMotionType)),
				force, charge, reaction, push, pushed, deferred ? " (mis en attente du ragdoll de mort)"
				: pushed ? "" : " (corps animes : le Ragdoll Feedback ne les a pas rendus dynamiques)");
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
				for (const char* limb : {"Bip01 L Forearm", "Bip01 R Forearm", "Bip01 L Hand", "Bip01 R Hand"})
				{
					int k = FindBody(b, n, limb);
					if (k >= 0) ApplyImpulse(b[k].rb, RandomDir() * g_cfg.livingFlail, BodyCenter(b[k].rb));
				}
			}
			if (shocked)
			{
				for (const char* part : {"Bip01 Spine2", "Bip01 Head", "Bip01 L UpperArm", "Bip01 R UpperArm"})
				{
					int k = FindBody(b, n, part);
					if (k >= 0) ApplyImpulse(b[k].rb, RandomDir() * g_cfg.livingShake, BodyCenter(b[k].rb));
				}
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
		int i = FindBody(b, n, "Bip01 Spine2");
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
	void CheckRagdollsMod()
	{
		char path[MAX_PATH];
		snprintf(path, sizeof(path), "%sData\\Ragdolls.esm", g_gameDir);
		if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES)
			Log("ATTENTION : Ragdolls.esm est present. Ce mod le remplace : desactive Ragdolls pour eviter deux reactions "
				"et deux reglages du Ragdoll Feedback.");
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
			CheckRagdollsMod();
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
		snprintf(g_iniPath, sizeof(g_iniPath), "%sData\\NVSE\\Plugins\\NVReactiveRagdoll.ini", g_gameDir);

		Log("NV Reactive Ragdoll v%u, xNVSE %08X, jeu %08X", kPluginVersion, nvse->nvseVersion, nvse->runtimeVersion);
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
