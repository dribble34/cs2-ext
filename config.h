#pragma once
#include "overlay.h"   // Draw::Color

// ============================================================
//  CONFIG
//  Runtime-configurable ESP settings, trimmed to box/skeleton/health
//  bar. No ImGui menu this milestone, so these are plain compile-time
//  namespace constants — flip them here and rebuild. (The internal
//  project's config.h used C++17 inline variables for the same reason:
//  every translation unit shares one definition with no separate .cpp.)
// ============================================================
namespace Config {

    // ---- Master -------------------------------------------------
    inline bool bESP       = true;
    inline bool bTeamCheck = true;   // skip teammates when true

    // ---- Box ESP --------------------------------------------------
    inline bool         bBoxESP       = true;
    inline float         fBoxThickness = 1.5f;
    inline Draw::Color   colEnemyBox   = { 255,  64, 128, 240 };
    inline Draw::Color   colTeamBox    = { 33, 191, 255, 240 };

    // ---- Health Bar -------------------------------------------------
    inline bool         bHealthBar         = true;
    inline float         fHealthBarWidth    = 4.0f;
    inline float         fHealthBarSideGap  = 3.0f;
    inline bool         bHealthBarGradient = true;
    inline Draw::Color   colHealthBarFull   = { 0, 230, 102, 255 };
    inline Draw::Color   colHealthBarLow    = { 255,  46,  26, 255 };

    // ---- Skeleton ---------------------------------------------------
    inline bool         bSkeleton          = true;
    inline float         fSkeletonThickness = 1.5f;
    inline Draw::Color   colEnemySkeleton   = { 235, 235, 240, 235 };
    inline Draw::Color   colTeamSkeleton    = { 120, 200, 255, 235 };

    // ---- FOV changer (write) ---------------------------------------
    inline bool  bFovChanger = false;
    inline float fDesiredFOV = 110.f;   // 1..179

    // ---- Smoke color (write) ---------------------------------------
    inline bool         bSmokeColor = false;
    inline Draw::Color   colSmoke    = { 255, 60, 200, 255 };

}
