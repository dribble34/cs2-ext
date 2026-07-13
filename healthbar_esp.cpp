#include "esp.h"
#include "config.h"
#include "overlay.h"
#include <algorithm>

// ============================================================
//  ESP Health Bar Drawing
//  Vertical bar just left of the box, filling bottom-up by health%.
//  Corners come from ESP::Render; this is pure drawing.
// ============================================================

static inline Draw::Color LerpColor(const Draw::Color& a, const Draw::Color& b, float t) {
    return Draw::MakeColor(
        static_cast<int>(a.r + (b.r - a.r) * t),
        static_cast<int>(a.g + (b.g - a.g) * t),
        static_cast<int>(a.b + (b.b - a.b) * t),
        static_cast<int>(a.a + (b.a - a.a) * t));
}

void ESP::DrawHealthBar(float boxLeft, float boxTop, float boxBottom, int health) {
    if (!Config::bHealthBar) return;

    health = std::clamp(health, 0, 100);
    float hpFrac = health / 100.f;

    float barRight = boxLeft - Config::fHealthBarSideGap;
    float barLeft  = barRight - Config::fHealthBarWidth;
    float barTop   = boxTop;
    float barBot   = boxBottom;
    float fillTop  = barBot - (barBot - barTop) * hpFrac;

    // Background.
    Draw::AddRectFilled(barLeft - 1.f, barTop - 1.f, barRight + 1.f, barBot + 1.f,
                         Draw::MakeColor(0, 0, 0, 180));

    if (hpFrac <= 0.f) return;

    if (Config::bHealthBarGradient) {
        // Top of fill lerps toward full-health color; bottom stays at the
        // low-health color, so both ends converge as health drops.
        Draw::Color topCol = LerpColor(Config::colHealthBarLow, Config::colHealthBarFull, hpFrac);
        Draw::Color botCol = Config::colHealthBarLow;
        Draw::AddRectFilledGradientV(barLeft, fillTop, barRight, barBot, topCol, botCol);
    } else {
        Draw::Color col = LerpColor(Config::colHealthBarLow, Config::colHealthBarFull, hpFrac);
        Draw::AddRectFilled(barLeft, fillTop, barRight, barBot, col);
    }
}
