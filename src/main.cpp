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
		c.forceFeedback = IniInt("RagdollAnim", "bRagdollFeedback", 1) != 0;
		g_cfg = c;
		Log("Reglages lus : %s (%s)", g_iniPath,
			GetFileAttributesA(g_iniPath) != INVALID_FILE_ATTRIBUTES ? "trouve" : "ABSENT, valeurs par defaut");
		Log("  actif %d, poussee %.1f + %.2f/degat (max %.0f), voisins %.2f, test F7 %.0f",
			c.enabled, c.basePush, c.pushPerDamage, c.maxPush, c.neighbourShare, c.testPush);
	}

	// Reglages natifs du jeu ([RagdollAnim] de Fallout.ini) : ecrits seulement s'ils sont presents dans notre .ini.
	struct GameSetting { const char* key; UInt32 addr; bool isFloat; };
	const GameSetting kGameSettings[] = {
		{"bRagdollFeedback", ini::bRagdollFeedback, false},
		{"bRagdollAnim", ini::bRagdollAnim, false},
		{"bPoseMatching", ini::bPoseMatching, false},
		{"fHierarchyGain", ini::fHierarchyGain, true},
		{"fVelocityDamping", ini::fVelocityDamping, true},
		{"fAccelerationGain", ini::fAccelerationGain, true},
		{"fVelocityGain", ini::fVelocityGain, true},
		{"fPositionGain", ini::fPositionGain, true},
		{"fPositionMaxLinearVelocity", ini::fPositionMaxLinearVelocity, true},
		{"fPositionMaxAngularVelocity", ini::fPositionMaxAngularVelocity, true},
		{"fSnapGain", ini::fSnapGain, true},
		{"fSnapMaxLinearVelocity", ini::fSnapMaxLinearVelocity, true},
		{"fSnapMaxAngularVelocity", ini::fSnapMaxAngularVelocity, true},
		{"fSnapMaxLinearDistance", ini::fSnapMaxLinearDistance, true},
		{"fSnapMaxAngularDistance", ini::fSnapMaxAngularDistance, true},
		{"uiFeedbackTimeout", ini::uiFeedbackTimeout, false},
	};

	void ApplyGameSettings()
	{
		for (const GameSetting& s : kGameSettings)
		{
			void* setting = (void*)s.addr;
			const char* name = At<const char*>(setting, ini::kName);
			char buf[64];
			GetPrivateProfileStringA("RagdollAnim", s.key, "", buf, sizeof(buf), g_iniPath);
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

	// ------------------------------------------------------------------ reaction a un coup
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
		dir.z = g_cfg.up;
		dir = Norm(dir);

		float push = g_cfg.basePush + dmg * g_cfg.pushPerDamage;
		if (melee) push *= g_cfg.meleeMult;
		if (explosion) push *= g_cfg.explosionMult;
		if (flags & hit::kFlag_IsCritical) push *= g_cfg.criticalMult;
		if (push > g_cfg.maxPush) push = g_cfg.maxPush;

		int pushed = PushBodyAndNeighbours(b, n, i, dir * push, p);
		if (g_cfg.verbose)
			Log("Coup sur %08X par %08X : %s, degats %.1f, os %s (%s), poussee %.1f, %d corps pousses%s",
				RefID(target), RefID(attacker), explosion ? "explosion" : melee ? "melee" : "tir", dmg,
				b[i].name ? b[i].name : "?", hk::MotionName(At<UInt8>(b[i].rb, hk::kMotionType)), push, pushed,
				pushed ? "" : " (corps animes : le Ragdoll Feedback ne les a pas rendus dynamiques)");
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
			break;
		case NVSEMessagingInterface::kMessage_PostLoadGame:
			LoadConfig(); // relu a chaque chargement de partie, pour regler sans relancer le jeu
			ApplyGameSettings();
			break;
		case NVSEMessagingInterface::kMessage_MainGameLoop:
			PollKeys();
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
