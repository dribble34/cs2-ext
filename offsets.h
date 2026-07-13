#pragma once
#include <cstddef>

// ============================================================
//  OFFSETS
//  Trimmed to what ESP (box + skeleton + health bar) needs.
//  Re-dumped 2026-07-10 via cs2-dumper (https://github.com/a2x/cs2-dumper)
//  against the user's currently-running CS2 build — the prior literals
//  (carried over from the internal dribbleHook project, build 14165,
//  dumped 2026-06-20) had drifted after a game update: dwEntityList,
//  dwViewMatrix, dwLocalPlayerPawn, dwLocalPlayerController,
//  C_BaseEntity::m_iTeamNum, and CCSPlayerController::m_hPlayerPawn/
//  m_bPawnIsAlive all moved. m_pGameSceneNode/m_iHealth/m_vecAbsOrigin
//  matched byte-for-byte.
//
//  These are hardcoded literals with no runtime re-resolution: the
//  internal project's live schema-walk (schema_scanner.cpp) and
//  signature scanner (pattern_scanner.cpp) both ran IN-PROCESS and
//  can't work here without code injection, which is out of scope by
//  this project's POLICY.md. If the values below drift again after a
//  future CS2 update, re-run cs2-dumper and update this file by hand.
// ============================================================
namespace Offsets {

    // client.dll relative pointers.
    inline std::ptrdiff_t dwEntityList             = 0x254EE60;
    inline std::ptrdiff_t dwViewMatrix             = 0x23A9340;
    inline std::ptrdiff_t dwLocalPlayerPawn        = 0x23A4238;
    inline std::ptrdiff_t dwLocalPlayerController  = 0x237EBA0;

    // CEntityInstance / C_BaseEntity fields.
    inline std::ptrdiff_t m_pGameSceneNode         = 0x330;   // CGameSceneNode*
    inline std::ptrdiff_t m_iHealth                = 0x34C;   // int32
    inline std::ptrdiff_t m_iTeamNum               = 0x3E7;   // uint8 — pawn only
    inline std::ptrdiff_t m_vecAbsOrigin           = 0xC8;    // Vector3, inside CGameSceneNode

    // m_vecViewOffset (CNetworkViewOffsetVector, Vector at +0x0). The .z is
    // the eye height above the feet origin — ~64 standing, ~46 crouched — so
    // it doubles as a cheap crouch-aware head-height for the box, no bones.
    inline std::ptrdiff_t m_vecViewOffset          = 0xE78;

    // CCSPlayerController fields.
    inline std::ptrdiff_t m_hPlayerPawn            = 0x914;   // CHandle<C_CSPlayerPawn>
    inline std::ptrdiff_t m_bPawnIsAlive           = 0x91C;   // bool
    inline std::ptrdiff_t m_iDesiredFOV            = 0x78C;   // uint32 (FOV changer)

    // Bone-matrix chain (restored 2026-07-14 with the CORRECT model-state
    // offset for this build). deadlocked-rust reads the live bone array as
    //   bone_data = *(sceneNode + m_modelState + m_skeletonInstance)
    //   bonePos   = *(bone_data + boneIndex * 0x20)   // Vec3 at CTransform+0
    // where m_modelState = CSkeletonInstance::m_modelState and
    // m_skeletonInstance = CBodyComponentSkeletonInstance::m_skeletonInstance.
    // Our first attempt used 0x150 (stale, from the old build) + 0x80 = 0x1D0
    // and read garbage; this build's dump has m_modelState = 0x140, so the
    // real chain is 0x140 + 0x80 = 0x1C0.
    inline std::ptrdiff_t m_modelState             = 0x140;  // CSkeletonInstance::m_modelState
    inline std::ptrdiff_t BONE_ARRAY_OFFSET        = 0x80;   // m_skeletonInstance (bone array ptr)
    inline std::ptrdiff_t BONE_STRIDE              = 0x20;   // sizeof(CTransform)
    inline std::ptrdiff_t BONE_X                   = 0x00;
    inline std::ptrdiff_t BONE_Y                   = 0x04;
    inline std::ptrdiff_t BONE_Z                   = 0x08;

    // C_SmokeGrenadeProjectile (smoke color modulation — a write feature).
    inline std::ptrdiff_t m_bDidSmokeEffect        = 0x127C; // bool
    inline std::ptrdiff_t m_vSmokeColor            = 0x1284; // Vector (0..255 floats)

    // Standard CS2 player skeleton bone indices (match deadlocked-rust).
    namespace Bones {
        constexpr int Pelvis    =  1;   // Hip / root
        constexpr int Spine1    =  2;
        constexpr int Spine2    =  3;
        constexpr int Spine3    =  4;
        constexpr int Spine4    =  5;   // upper chest
        constexpr int Neck      =  6;
        constexpr int Head      =  7;

        constexpr int ShoulderL =  9;
        constexpr int ElbowL    = 10;
        constexpr int HandL     = 11;

        constexpr int ShoulderR = 13;
        constexpr int ElbowR    = 14;
        constexpr int HandR     = 15;

        constexpr int HipL      = 17;
        constexpr int KneeL     = 18;
        constexpr int FootL     = 19;

        constexpr int HipR      = 20;
        constexpr int KneeR     = 21;
        constexpr int FootR     = 22;
    }
}
