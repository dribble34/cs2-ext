#include "esp.h"
#include "config.h"
#include "overlay.h"

// ============================================================
//  ESP Box Drawing
//  Screen-space axis-aligned box with a black outline underneath the
//  colored one for contrast. Corners come pre-computed from ESP::Render
//  (feet/head projected on the player's vertical axis), so this is pure
//  drawing — no world math here.
// ============================================================

void ESP::DrawBox(float left, float top, float right, float bottom, bool isEnemy) {
    if (!Config::bBoxESP) return;

    Draw::Color colBox = isEnemy ? Config::colEnemyBox : Config::colTeamBox;
    float t = Config::fBoxThickness;

    Draw::AddRect(left, top, right, bottom, Draw::MakeColor(0, 0, 0, 255), t + 2.f);
    Draw::AddRect(left, top, right, bottom, colBox, t);
}
