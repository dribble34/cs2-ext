#include "esp.h"
#include "offsets.h"
#include "memory.h"
#include "config.h"
#include "overlay.h"
#include "entities/entities.h"
#include <cmath>
#include <algorithm>
#include <cstdio>

// ============================================================
//  ESP MODULE — box + health bar only (read-only).
//  No bones: the box is built from the abs origin (feet) and the
//  crouch-aware view-offset height (head), then projected the way
//  deadlocked-rust does it — two world-space points on the player's
//  vertical axis, both run through WorldToScreen, and the box sized
//  off the projected height. Because both points share the same world
//  X/Y, they project to (near) the same screen X, so the box stays
//  upright and never skews the way a garbage head-bone read did.
//
//  Skeleton is bone-based again now that the bone-matrix offset is
//  correct (offsets.h). FOV changer writes the local controller's
//  m_iDesiredFOV each frame — the canonical, spawn-gated path.
// ============================================================

// Bone array pointer: *(sceneNode + m_modelState + BONE_ARRAY_OFFSET).
static uintptr_t GetBoneArr(uintptr_t sceneNode) {
    return Memory::Read<uintptr_t>(sceneNode + Offsets::m_modelState + Offsets::BONE_ARRAY_OFFSET);
}

void ESP::Render() {
    if (!Config::bESP) return;
    if (!Memory::IsAttached()) return;

    static uintptr_t clientBase = 0;
    if (!clientBase) {
        clientBase = Memory::GetModuleBase("client.dll");
        if (!clientBase) return;
    }

    float viewMatrix[16];
    if (!Memory::ReadInto(clientBase + Offsets::dwViewMatrix, viewMatrix, sizeof(viewMatrix))) return;

    uintptr_t entListRaw = Memory::Read<uintptr_t>(clientBase + Offsets::dwEntityList);
    if (!Memory::IsValidPtr(entListRaw)) return;
    auto* entSystem = reinterpret_cast<CGameEntitySystem*>(entListRaw);

    uintptr_t localControllerRaw = Memory::Read<uintptr_t>(clientBase + Offsets::dwLocalPlayerController);
    uint32_t  localPawnHandle = 0;
    if (Memory::IsValidPtr(localControllerRaw))
        localPawnHandle = reinterpret_cast<CCSPlayerController*>(localControllerRaw)->m_hPlayerPawn().ToInt();
    uintptr_t localPawn = reinterpret_cast<uintptr_t>(entSystem->ResolveHandle<C_CSPlayerPawn>(localPawnHandle));

    uint8_t localTeam = 0;
    if (Memory::IsValidPtr(localPawn))
        localTeam = reinterpret_cast<C_CSPlayerPawn*>(localPawn)->m_iTeamNum();

    // ── FOV changer ─────────────────────────────────────────────────────
    // Write m_iDesiredFOV on the local controller; the game lerps the view
    // FOV toward it. Gated only on being spawned (valid controller + pawn),
    // which is the reliable path — the camera-services FOV guard the Rust
    // source used stays 0 at default FOV and never passes.
    if (Config::bFovChanger && Memory::IsValidPtr(localControllerRaw) && Memory::IsValidPtr(localPawn)) {
        uint32_t wanted = static_cast<uint32_t>(std::clamp(Config::fDesiredFOV, 1.f, 179.f));
        Memory::Write<uint32_t>(
            reinterpret_cast<CCSPlayerController*>(localControllerRaw)->m_iDesiredFOV_addr(), wanted);
    }

    Draw::Size screen = Draw::ScreenSize();

    for (int i = 1; i <= 64; ++i) {
        uintptr_t controllerRaw = entSystem->GetEntityByIndex(i);
        if (!Memory::IsValidPtr(controllerRaw) || controllerRaw == localControllerRaw) continue;
        auto* controllerSdk = reinterpret_cast<CCSPlayerController*>(controllerRaw);

        if (!controllerSdk->m_bPawnIsAlive()) continue;

        uint32_t hPawn = controllerSdk->m_hPlayerPawn().ToInt();
        C_CSPlayerPawn* pawnSdk = entSystem->ResolveHandle<C_CSPlayerPawn>(hPawn);
        uintptr_t pawn = reinterpret_cast<uintptr_t>(pawnSdk);
        if (!Memory::IsValidPtr(pawn) || pawn == localPawn) continue;

        uint8_t team    = pawnSdk->m_iTeamNum();
        bool    isEnemy = (team != localTeam);
        if (Config::bTeamCheck && localTeam != 0 && !isEnemy) continue;

        int health = std::clamp(static_cast<int32_t>(pawnSdk->m_iHealth()), 0, 100);
        if (health <= 0) continue;

        CGameSceneNode* sceneNodeSdk = pawnSdk->m_pGameSceneNode();
        uintptr_t sceneNode = reinterpret_cast<uintptr_t>(sceneNodeSdk);
        if (!Memory::IsValidPtr(sceneNode)) continue;

        Vector3 origin = sceneNodeSdk->GetAbsOrigin();

        // Crouch-aware head height from the eye/view offset; fall back to a
        // standing ~64u if the read looks bogus.
        float eyeZ = pawnSdk->m_vecViewOffset().z;
        if (eyeZ < 30.f || eyeZ > 90.f) eyeZ = 64.f;

        // Two world points on the player's vertical axis. Pad 6u above the
        // eye (head sits above the eyes) and 4u below the feet so the box
        // wraps the model rather than cutting it off — mirrors the small
        // padding deadlocked-rust adds to the box height.
        Vector3 topWorld    = origin; topWorld.z    += eyeZ + 6.f;
        Vector3 bottomWorld = origin; bottomWorld.z -= 4.f;

        Vector2 sTop, sBottom;
        if (!Math::WorldToScreen(topWorld,    sTop,    viewMatrix, screen.w, screen.h)) continue;
        if (!Math::WorldToScreen(bottomWorld, sBottom, viewMatrix, screen.w, screen.h)) continue;

        float boxH = sBottom.y - sTop.y;
        if (boxH < 4.f) continue;                 // too small / degenerate
        float halfW = boxH * 0.25f;               // width : height = 0.5

        float left   = sTop.x - halfW;
        float right   = sTop.x + halfW;
        float top    = sTop.y;
        float bottom = sBottom.y;

        if (Config::bSkeleton) {
            uintptr_t boneArr = GetBoneArr(sceneNode);
            if (Memory::IsValidPtr(boneArr))
                ESP::DrawSkeleton(boneArr, viewMatrix, screen.w, screen.h, isEnemy);
        }
        ESP::DrawBox(left, top, right, bottom, isEnemy);
        ESP::DrawHealthBar(left, top, bottom, health);
    }
}
