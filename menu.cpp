#include "menu.h"
#include "overlay.h"
#include "config.h"
#include <windows.h>
#include <cstdio>
#include <algorithm>

// ============================================================
//  Immediate-mode menu. Each widget both draws itself and resolves its
//  own interaction against the frame's mouse snapshot, advancing a
//  running layout cursor (g_x/g_y). Slider drags are tracked by an
//  "active id" so the drag continues even if the cursor slips off the
//  track vertically — the one bit of state a stateless IM menu needs.
// ============================================================

namespace {

    // ---- palette ----
    const Draw::Color kPanelBg   = { 18, 18, 22, 236 };
    const Draw::Color kPanelEdge = { 82, 82, 94, 255 };
    const Draw::Color kTitle     = { 235, 235, 240, 255 };
    const Draw::Color kText      = { 200, 200, 208, 255 };
    const Draw::Color kAccent    = { 90, 160, 255, 255 };
    const Draw::Color kBoxBg     = { 44, 44, 52, 255 };
    const Draw::Color kTrack     = { 55, 55, 63, 255 };

    // ---- layout constants ----
    constexpr float kPanelX = 60.f, kPanelY = 40.f, kPanelW = 250.f;
    constexpr float kPad = 12.f, kRowH = 22.f, kSliderH = 22.f, kFont = 13.f;

    // ---- state ----
    bool  g_open       = false;
    bool  g_lmbDown    = false;
    bool  g_lmbPrev    = false;
    POINT g_mouse      = { 0, 0 }; // overlay-local
    int   g_activeSld  = -1;
    int   g_sldCounter = 0;

    float g_x = 0.f, g_y = 0.f; // layout cursor

    bool Hovered(float x, float y, float w, float h) {
        return g_mouse.x >= x && g_mouse.x < x + w &&
               g_mouse.y >= y && g_mouse.y < y + h;
    }
    bool Clicked(float x, float y, float w, float h) {
        return Hovered(x, y, w, h) && g_lmbDown && !g_lmbPrev;
    }

    Draw::Color Opaque(const Draw::Color& c) { return { c.r, c.g, c.b, 255 }; }

    void Checkbox(const char* label, bool& v) {
        const float s = 16.f;
        float bx = g_x, by = g_y;
        Draw::AddRectFilled(bx, by, bx + s, by + s, kBoxBg);
        Draw::AddRect(bx, by, bx + s, by + s, kPanelEdge, 1.f);
        if (v) Draw::AddRectFilled(bx + 3.f, by + 3.f, bx + s - 3.f, by + s - 3.f, kAccent);
        Draw::AddText(bx + s + 8.f, by - 1.f, kFont, kText, label);

        // Whole row is the hit target.
        if (Clicked(bx, by, kPanelW - 2.f * kPad, s)) v = !v;
        g_y += kRowH;
    }

    void SliderU8(const char* label, unsigned char& v) {
        int id = g_sldCounter++;
        float sx = g_x, sw = kPanelW - 2.f * kPad, sh = 6.f;
        float sy = g_y + 15.f;

        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s %d", label, static_cast<int>(v));
        Draw::AddText(sx, g_y - 2.f, kFont, kText, buf);

        Draw::AddRectFilled(sx, sy, sx + sw, sy + sh, kTrack);
        float frac = v / 255.f;
        Draw::AddRectFilled(sx, sy, sx + sw * frac, sy + sh, kAccent);
        // handle
        float hx = sx + sw * frac;
        Draw::AddRectFilled(hx - 2.f, sy - 3.f, hx + 2.f, sy + sh + 3.f, Opaque(kTitle));

        // Grab on press anywhere on the (padded) track; hold to drag.
        if (g_activeSld == -1 && g_lmbDown && !g_lmbPrev && Hovered(sx, sy - 6.f, sw, sh + 12.f))
            g_activeSld = id;
        if (g_activeSld == id) {
            if (!g_lmbDown) g_activeSld = -1;
            else {
                float f = std::clamp((g_mouse.x - sx) / sw, 0.f, 1.f);
                v = static_cast<unsigned char>(f * 255.f + 0.5f);
            }
        }
        g_y += kSliderH;
    }

    // Float slider over [lo, hi]. Same drag model as SliderU8.
    void SliderFloat(const char* label, float& v, float lo, float hi) {
        int id = g_sldCounter++;
        float sx = g_x, sw = kPanelW - 2.f * kPad, sh = 6.f;
        float sy = g_y + 15.f;

        char buf[64];
        std::snprintf(buf, sizeof(buf), "%s %.1f", label, v);
        Draw::AddText(sx, g_y - 2.f, kFont, kText, buf);

        Draw::AddRectFilled(sx, sy, sx + sw, sy + sh, kTrack);
        float frac = std::clamp((v - lo) / (hi - lo), 0.f, 1.f);
        Draw::AddRectFilled(sx, sy, sx + sw * frac, sy + sh, kAccent);
        float hx = sx + sw * frac;
        Draw::AddRectFilled(hx - 2.f, sy - 3.f, hx + 2.f, sy + sh + 3.f, Opaque(kTitle));

        if (g_activeSld == -1 && g_lmbDown && !g_lmbPrev && Hovered(sx, sy - 6.f, sw, sh + 12.f))
            g_activeSld = id;
        if (g_activeSld == id) {
            if (!g_lmbDown) g_activeSld = -1;
            else {
                float f = std::clamp((g_mouse.x - sx) / sw, 0.f, 1.f);
                v = lo + f * (hi - lo);
            }
        }
        g_y += kSliderH;
    }

    void ColorEdit(const char* label, Draw::Color& c) {
        float sw = 20.f;
        Draw::AddText(g_x, g_y - 2.f, kFont, kText, label);
        Draw::AddRectFilled(g_x + kPanelW - 2.f * kPad - sw, g_y, g_x + kPanelW - 2.f * kPad, g_y + 14.f, Opaque(c));
        Draw::AddRect(g_x + kPanelW - 2.f * kPad - sw, g_y, g_x + kPanelW - 2.f * kPad, g_y + 14.f, kPanelEdge, 1.f);
        g_y += kRowH;
        SliderU8("R", c.r);
        SliderU8("G", c.g);
        SliderU8("B", c.b);
        g_y += 4.f;
    }

    void Separator() {
        Draw::AddRectFilled(g_x, g_y + 4.f, g_x + kPanelW - 2.f * kPad, g_y + 5.f, kPanelEdge);
        g_y += 12.f;
    }

}

namespace Menu {

    void PollHotkey() {
        static bool prev = false;
        bool now = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (now && !prev) g_open = !g_open;
        prev = now;
    }

    bool IsOpen() { return g_open; }

    void Render() {
        if (!g_open) return;

        // Mouse snapshot (global cursor -> overlay-local).
        POINT cur;
        GetCursorPos(&cur);
        POINT org = Overlay::Origin();
        g_mouse = { cur.x - org.x, cur.y - org.y };
        g_lmbDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
        g_sldCounter = 0;

        // Panel background (fixed height chosen to fit the content below).
        constexpr float kPanelH = 1010.f;
        Draw::AddRectFilled(kPanelX, kPanelY, kPanelX + kPanelW, kPanelY + kPanelH, kPanelBg);
        Draw::AddRect(kPanelX, kPanelY, kPanelX + kPanelW, kPanelY + kPanelH, kPanelEdge, 1.f);
        Draw::AddRectFilled(kPanelX, kPanelY, kPanelX + kPanelW, kPanelY + 26.f, kAccent);
        Draw::AddText(kPanelX + kPad, kPanelY + 4.f, 15.f, Opaque(kPanelBg), "cs2_external  \xE2\x80\x94  INSERT");

        g_x = kPanelX + kPad;
        g_y = kPanelY + 36.f;

        Checkbox("ESP master", Config::bESP);
        Checkbox("Team check (skip teammates)", Config::bTeamCheck);
        Checkbox("Box ESP", Config::bBoxESP);
        Checkbox("Health bar", Config::bHealthBar);
        Checkbox("Health bar gradient", Config::bHealthBarGradient);
        Checkbox("Skeleton", Config::bSkeleton);
        Checkbox("FOV changer", Config::bFovChanger);
        Checkbox("Smoke color", Config::bSmokeColor);

        Separator();
        Draw::AddText(g_x, g_y, kFont, kTitle, "Sizes");
        g_y += kRowH;
        SliderFloat("Box width", Config::fBoxThickness, 0.5f, 6.f);
        SliderFloat("Skeleton width", Config::fSkeletonThickness, 0.5f, 6.f);
        SliderFloat("Health bar width", Config::fHealthBarWidth, 2.f, 12.f);
        SliderFloat("FOV", Config::fDesiredFOV, 1.f, 179.f);

        Separator();
        Draw::AddText(g_x, g_y, kFont, kTitle, "Colors");
        g_y += kRowH;

        ColorEdit("Enemy box", Config::colEnemyBox);
        ColorEdit("Team box", Config::colTeamBox);
        ColorEdit("Health high", Config::colHealthBarFull);
        ColorEdit("Health low", Config::colHealthBarLow);
        ColorEdit("Skeleton", Config::colEnemySkeleton);
        ColorEdit("Smoke", Config::colSmoke);

        g_lmbPrev = g_lmbDown;
    }

}
