#pragma once
#include <windows.h>

// ============================================================
//  OVERLAY
//  Replaces the internal project's ImGui/D3D11 overlay with a plain
//  Win32 layered window (WS_EX_LAYERED | WS_EX_TRANSPARENT |
//  WS_EX_TOPMOST), drawn into with GDI+ and pushed to screen via
//  UpdateLayeredWindow. No ImGui, no D3D, no game-process hooking —
//  this window belongs entirely to our own process.
//
//  Draw:: gives the ported ESP draw modules (box_esp.cpp,
//  healthbar_esp.cpp, skeleton_esp.cpp) the same call shapes the old
//  ImGui draw list had (AddRect/AddRectFilled/AddLine/AddText/
//  CalcTextSize) so those files port with only a mechanical
//  find/replace of the namespace prefix.
// ============================================================
namespace Overlay {

    // Creates the layered window (hidden until the first successful
    // Update()) and starts GDI+. Call once at startup.
    bool Init();

    // Tears down GDI+ and destroys the window. Call once at shutdown.
    void Shutdown();

    // Locates/re-locates the game window by PID, and if found, moves/
    // resizes our overlay to match its client rect and shows it;
    // otherwise hides the overlay. Returns true iff the overlay is
    // currently positioned over a live game window (i.e. it's safe to
    // draw this tick). Call once per tick before BeginFrame().
    bool Update(DWORD gamePid);

    // Pumps this process's own window messages. Call once per tick.
    void PumpMessages();

    // Clears the backing bitmap to fully transparent and prepares it
    // for drawing. No-op (returns false) if the overlay isn't shown.
    bool BeginFrame();

    // Pushes the backing bitmap to screen via UpdateLayeredWindow.
    void EndFrame();

    // Toggles interactivity. Interactive (menu open) drops WS_EX_TRANSPARENT
    // so opaque menu pixels catch mouse clicks (transparent ESP areas still
    // pass through to the game) and pulls the overlay to the foreground so the
    // game releases the mouse cursor. Non-interactive restores click-through.
    // No-ops when the requested state already matches.
    void SetInteractive(bool interactive);

    // Screen-space top-left of the overlay window (== the game's client
    // origin). Menu input converts the global cursor into overlay-local
    // coordinates with this.
    POINT Origin();

}

namespace Draw {

    struct Color { unsigned char r, g, b, a; };
    inline Color MakeColor(int r, int g, int b, int a = 255) {
        return { static_cast<unsigned char>(r), static_cast<unsigned char>(g),
                 static_cast<unsigned char>(b), static_cast<unsigned char>(a) };
    }

    struct Size { float w, h; };

    // Current overlay size (tracks the game window's client rect).
    Size ScreenSize();

    void AddRect(float x0, float y0, float x1, float y1, Color color, float thickness);
    void AddRectFilled(float x0, float y0, float x1, float y1, Color color);
    // Vertical gradient fill: `top` at y0, `bottom` at y1.
    void AddRectFilledGradientV(float x0, float y0, float x1, float y1, Color top, Color bottom);
    void AddLine(float x0, float y0, float x1, float y1, Color color, float thickness);

    // Default-size text (matches CalcTextSize(text)).
    void AddText(float x, float y, Color color, const char* text);
    Size CalcTextSize(const char* text);

    // Explicit-size text (matches CalcTextSize(fontSize, text)) — used by
    // the health-value readout, which scales independently of everything else.
    void AddText(float x, float y, float fontSize, Color color, const char* text);
    Size CalcTextSize(float fontSize, const char* text);

}
