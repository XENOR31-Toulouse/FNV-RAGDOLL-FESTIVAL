// Fallout New Vegas 1.4.0.525 : les quelques structures et adresses du jeu dont le plugin a besoin.
// Offsets repris des sources de JIP LN NVSE (GPL-3, github.com/jazzisparis/JIP-LN-NVSE) et de xNVSE.
// On lit la memoire par offsets plutot que d'inclure tout le SDK : un seul .cpp a compiler.
#pragma once
#include <cstdint>

using UInt8 = std::uint8_t;
using UInt16 = std::uint16_t;
using UInt32 = std::uint32_t;
using SInt32 = std::int32_t;

template <typename T> inline T& At(const void* base, UInt32 off) { return *(T*)((const UInt8*)base + off); }

// ---- API des plugins NVSE (ABI publique, PluginAPI.h de xNVSE) ----
using PluginHandle = UInt32;

struct PluginInfo
{
	enum { kInfoVersion = 1 };
	UInt32 infoVersion;
	const char* name;
	UInt32 version;
};

struct NVSEInterface
{
	UInt32 nvseVersion;
	UInt32 runtimeVersion;
	UInt32 editorVersion;
	UInt32 isEditor;
	void* RegisterCommand;
	void* SetOpcodeBase;
	void* (*QueryInterface)(UInt32 id);
	PluginHandle (*GetPluginHandle)();
	void* RegisterTypedCommand;
	const char* (*GetRuntimeDirectory)();
	UInt32 isNogore;
};

enum { kInterface_Messaging = 2, kInterface_EventManager = 8 };
constexpr UInt32 kRuntime_1_4_0_525 = 0x040020D0;

struct NVSEMessagingInterface
{
	struct Message { const char* sender; UInt32 type; UInt32 dataLen; void* data; };
	using EventCallback = void (*)(Message* msg);
	enum { kMessage_PostLoad = 0, kMessage_PostLoadGame = 8, kMessage_DeferredInit = 18, kMessage_MainGameLoop = 20 };
	UInt32 version;
	bool (*RegisterListener)(PluginHandle listener, const char* sender, EventCallback handler);
	void* Dispatch;
};

struct NVSEEventManagerInterface
{
	// Pour les evenements classiques (OnHit...), xNVSE appelle func(nullptr, {arg0, arg1}).
	using NativeEventHandler = void (*)(void* thisObj, void* parameters);
	void* RegisterEvent;
	void* DispatchEvent;
	void* DispatchEventAlt;
	bool (*SetNativeEventHandler)(const char* eventName, NativeEventHandler func);
	bool (*RemoveNativeEventHandler)(const char* eventName, NativeEventHandler func);
};

// ---- Formes et references ----
namespace form
{
	constexpr UInt32 kTypeID = 0x04;      // UInt8 : type de forme
	constexpr UInt32 kRefID = 0x0C;       // UInt32
	constexpr UInt8 kType_Character = 0x3B;
	constexpr UInt8 kType_Creature = 0x3C;
}

namespace refr
{
	constexpr UInt32 kBaseForm = 0x20;    // TESForm*
	constexpr UInt32 kPos = 0x30;         // float[3], unites du jeu
	constexpr UInt32 kRenderState = 0x64; // RenderState* ; RenderState+0x14 = NiNode* racine 3D
	constexpr UInt32 kRenderRoot = 0x14;
}

namespace actor
{
	constexpr UInt32 kProcess = 0x68;          // BaseProcess*
	constexpr UInt32 kRagdollController = 0xAC; // bhkRagdollController*
	constexpr UInt32 kLifeState = 0x108;        // UInt32 : 0 vivant, 1 mourant, 2 mort, 3 inconscient
	constexpr UInt32 kVtbl_GetAnimData = 0x1E4; // Actor vtable : AnimData* GetAnimData()
	constexpr UInt32 kProcess_GetHitData = 0x770; // BaseProcess vtable : ActorHitData* GetHitData()
	// BaseProcess vtable : PushActorAway(Actor* pushed, float srcX, float srcY, float srcZ, float force)
	// = la commande de script PushActorAway : ragdoll, puis le jeu fait relever l'acteur
	constexpr UInt32 kProcess_PushActorAway = 0x418;
	constexpr UInt32 kProcess_Level = 0x28;      // UInt32 : 0 High, 1 MiddleHigh, 2 MiddleLow, 3 Low
	constexpr UInt32 kProcess_CharCtrl = 0x138;  // bhkCharacterController* (niveau <= 1)
	constexpr UInt32 kProcess_Knocked = 0x13C;   // SInt8 : 0 debout, 1 renverse, 2 ragdoll, 3 inconscient, 6 se releve
	constexpr UInt32 kCharCtrl_Velocity = 0x4F0; // hkVector4, unites Havok/s
}

// Lecture d'une animation par groupe (sequence de Actor::PlayAnimGroup dans JIP LN)
namespace anim
{
	constexpr UInt32 kAddr_ResolveGroup = 0x897910; // thiscall(Actor*, group, 0, 0, AnimData*) -> UInt16 id complet
	constexpr UInt32 kAddr_PlayGroup = 0x494740;    // thiscall(AnimData*, UInt16 id, 1, -1, -1)
	constexpr UInt32 kAddr_ActorAnimNotify = 0x8A73E0; // thiscall(Actor*, 0xE, AnimData+0xE4)
	constexpr UInt32 kAddr_ActorAnimDone = 0x55D420;   // thiscall(Actor*, 1)
	constexpr UInt32 kAnimData_E4 = 0xE4;
	// Table des groupes d'animation : 0xF5 entrees de 0x24 octets, nom en +0
	constexpr UInt32 kAddr_GroupInfos = 0x11977D8;
	constexpr UInt32 kGroupInfoSize = 0x24;
	constexpr UInt32 kGroupCount = 0xF5;
}

namespace weap
{
	constexpr UInt32 kNumProjectiles = 0x11E; // UInt8 : plombs par tir (fusils a pompe)
	constexpr UInt32 kResistType = 0x16C;     // SInt32 : valeur d'acteur qui resiste aux degats de l'arme
}

// Valeurs d'acteur de resistance (JIP GameForms.h)
namespace av
{
	constexpr SInt32 kFireResist = 0x34;
	constexpr SInt32 kElectricResist = 0x3A;
	constexpr SInt32 kEmpResist = 0x3D;
}

// BGSProjectile : type en +0x62 (UInt16) ; 8 = flamme (lance-flammes, incinerateur)
namespace projform
{
	constexpr UInt32 kType = 0x62;
	constexpr UInt16 kType_Flame = 8;
}

// ActorHitData (0x64)
namespace hit
{
	constexpr UInt32 kSource = 0x00;      // Actor*
	constexpr UInt32 kTarget = 0x04;      // Actor*
	constexpr UInt32 kProjectile = 0x08;  // Projectile* ou Explosion*
	constexpr UInt32 kHitLocation = 0x10; // SInt32
	constexpr UInt32 kHealthDmg = 0x14;   // float
	constexpr UInt32 kWeapon = 0x30;      // TESObjectWEAP*
	constexpr UInt32 kImpactPos = 0x38;   // float[3], unites du jeu
	constexpr UInt32 kImpactAngle = 0x44; // float[3]
	constexpr UInt32 kFlags = 0x58;       // UInt32
	enum { kFlag_IsCritical = 4, kFlag_IsFatal = 0x10, kFlag_IsSneakAttack = 0x400, kFlag_IsExplosionHit = 0x2000 };
}

// ---- NetImmerse ----
namespace ni
{
	constexpr UInt32 kName = 0x08;            // const char* (NiFixedString)
	constexpr UInt32 kParent = 0x18;          // NiNode*
	constexpr UInt32 kCollisionObject = 0x1C; // bhkNiCollisionObject* (bhkBlendCollisionObject sur les squelettes)
	constexpr UInt32 kChildren = 0x9C;        // NiTArray<NiAVObject*> : +4 data, +0xA firstFreeEntry
	constexpr UInt32 kCollision_WorldObject = 0x10; // bhkWorldObject* (bhkRigidBody)
	constexpr UInt32 kBhkRef_HkObject = 0x08;       // hkpWorldObject*
	// NiObject vtable slot 0x0C : GetNiNode(), renvoie this pour un NiNode, nullptr sinon (formes, lumieres...)
	constexpr UInt32 kVtbl_GetNiNode = 0x0C;
}

// ---- Havok (hkpRigidBody, 0x220) ----
namespace hk
{
	constexpr UInt32 kCollisionType = 0x28;   // UInt8 : 1 = corps rigide
	constexpr UInt32 kFriction = 0x90;
	constexpr UInt32 kMotion = 0xE0;          // hkpMotion integre
	constexpr UInt32 kMotionType = 0xE8;      // UInt8
	constexpr UInt32 kRotation = 0xF0;        // hkMatrix3x4 : 3 colonnes hkVector4
	constexpr UInt32 kTranslation = 0x120;    // hkVector4
	constexpr UInt32 kCenterOfMass = 0x140;   // hkVector4 (centre de masse, monde)
	constexpr UInt32 kInvInertia = 0x1A0;     // float[3], inertie inverse locale (diagonale)
	constexpr UInt32 kInvMass = 0x1AC;        // float
	constexpr UInt32 kLinVelocity = 0x1B0;    // hkVector4
	constexpr UInt32 kAngVelocity = 0x1C0;    // hkVector4
	constexpr UInt32 kAddr_UpdateMotion = 0xC9C1D0; // thiscall(hkpRigidBody*) : reveille le corps apres un changement de vitesse

	enum MotionType : UInt8 { kSphere = 2, kBox = 3, kKeyframed = 4, kFixed = 5, kThinBox = 6, kCharacter = 7 };
	inline bool IsDynamic(UInt8 t) { return t == kSphere || t == kBox || t == kThinBox; }
	inline const char* MotionName(UInt8 t)
	{
		switch (t)
		{
		case kSphere: return "dynamique(sphere)";
		case kBox: return "dynamique(box)";
		case kKeyframed: return "anime(keyframed)";
		case kFixed: return "fixe";
		case kThinBox: return "dynamique(thinbox)";
		case kCharacter: return "personnage";
		default: return "?";
		}
	}

	// Echelle Havok de Fallout 3 / New Vegas : 1 unite Havok = 7 unites du jeu.
	constexpr float kGameToHavok = 1.0f / 7.0f;
}

// ---- Interface et reglages ----
namespace ui
{
	constexpr UInt32 kAddr_InterfaceManager = 0x11D8A80; // InterfaceManager**
	constexpr UInt32 kCrosshairRef = 0xFC;               // TESObjectREFR*
}

// Table de toutes les formes chargees : NiTPointerMap<TESForm> (+4 nb de seaux, +8 seaux ; entree : +0 suivante,
// +4 cle, +8 forme). Sert a verifier qu'une reference gardee quelques images existe encore.
namespace forms
{
	constexpr UInt32 kAddr_FormsMap = 0x11C54C0; // NiTPointerMap<TESForm>**
	inline void* LookupByID(UInt32 refID)
	{
		void* map = *(void**)kAddr_FormsMap;
		if (!map) return nullptr;
		UInt32 numBuckets = At<UInt32>(map, 4);
		void** buckets = At<void**>(map, 8);
		if (!numBuckets || !buckets) return nullptr;
		for (void* e = buckets[refID % numBuckets]; e; e = At<void*>(e, 0))
			if (At<UInt32>(e, 4) == refID) return At<void*>(e, 8);
		return nullptr;
	}
}

namespace player
{
	constexpr UInt32 kAddr_PlayerCharacter = 0x11DEA3C; // PlayerCharacter**
}

// Reglages INI du jeu (classe Setting : +0 vtable, +4 valeur, +8 nom). Adresses : JIP settings_enum.h
namespace ini
{
	constexpr UInt32 bRagdollFeedback = 0x1267C54;
	constexpr UInt32 bRagdollAnim = 0x1267C24;
	constexpr UInt32 bPoseMatching = 0x1267C60;
	constexpr UInt32 fHierarchyGain = 0x1267C78;
	constexpr UInt32 fVelocityDamping = 0x1267C84;
	constexpr UInt32 fAccelerationGain = 0x1267C90;
	constexpr UInt32 fVelocityGain = 0x1267C9C;
	constexpr UInt32 fPositionGain = 0x1267CA8;
	constexpr UInt32 fPositionMaxLinearVelocity = 0x1267CB4;
	constexpr UInt32 fPositionMaxAngularVelocity = 0x1267CC0;
	constexpr UInt32 fSnapGain = 0x1267CCC;
	constexpr UInt32 fSnapMaxLinearVelocity = 0x1267CD8;
	constexpr UInt32 fSnapMaxAngularVelocity = 0x1267CE4;
	constexpr UInt32 fSnapMaxLinearDistance = 0x1267CF0;
	constexpr UInt32 fSnapMaxAngularDistance = 0x1267CFC;
	constexpr UInt32 uiFeedbackTimeout = 0x1267E04;
	constexpr UInt32 kValue = 0x04;
	constexpr UInt32 kName = 0x08;
}

// Reglages de jeu (GMST), meme structure que ci-dessus. Adresses : JIP settings_enum.h
namespace gmst
{
	constexpr UInt32 fGameplayImpulseMultBiped = 0x11D0574;
}

// Gravite : hkpWorldObject +0x08 = hkpWorld*, hkpWorld +0x10 = hkVector4 gravite (unites Havok/s2)
namespace hkworld
{
	constexpr UInt32 kWorldOfObject = 0x08;
	constexpr UInt32 kGravity = 0x10;
}
