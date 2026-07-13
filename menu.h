#pragma once

// ============================================================
//  MENU
//  Tiny immediate-mode menu drawn with the Draw:: (GDI+) shim — no
//  ImGui. Toggled with INSERT. Input comes from polling the global
//  cursor + left mouse button each frame (no WndProc hit-testing),
//  converted to overlay-local coordinates via Overlay::Origin().
//  While open, Overlay::SetInteractive(true) drops the click-through
//  style so the panel actually receives clicks.
// ============================================================
namespace Menu {

    // Edge-detects INSERT and flips the open/closed state. Call once per
    // tick (even when the overlay is hidden) so the hotkey always works.
    void PollHotkey();

    bool IsOpen();

    // Draws the panel and processes widget interaction for this frame.
    // No-op when closed. Call inside the draw pass, after ESP::Render so
    // the menu sits on top.
    void Render();

}
