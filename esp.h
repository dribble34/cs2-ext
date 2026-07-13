#pragma once
#include "esp_math.h"
#include <cstdint>

// ============================================================
//  ESP
//  Box + health bar only (bone-based skeleton was dropped 2026-07-14 —
//  see the note in offsets.h). The box is built from the abs origin +
//  view-offset height, projected the robust deadlocked-rust way.
// ============================================================
namespace ESP {

    // Reads the local player, entity list, and view matrix from the
    // attached process and draws a box + health bar for every other alive
    // player entity. Call once per tick between Overlay::BeginFrame() and
    // Overlay::EndFrame().
    void Render();

    // Screen-space axis-aligned box, given its four edges (pixels).
    void DrawBox(float left, float top, float right, float bottom, bool isEnemy);

    // Vertical health bar just left of the box's left edge.
    void DrawHealthBar(float boxLeft, float boxTop, float boxBottom, int health);

    // Bone-line skeleton for one pawn's bone array.
    void DrawSkeleton(uintptr_t boneArr, const float* viewMatrix,
                      float screenW, float screenH, bool isEnemy);

}
