#include <windows.h>
#include <cstdio>
#include "memory.h"
#include "overlay.h"
#include "esp.h"
#include "menu.h"
#include "smoke.h"

// ============================================================
//  main.cpp
//  Replaces the internal project's DllMain + injection lifecycle:
//  this is a standalone process that only ever reads cs2.exe's
//  memory via ReadProcessMemory (see headers/memory.h) and draws its
//  own separate overlay window (see headers/overlay.h). No injection,
//  no hooking, no writes. -insecure / offline bot matches only — see
//  ../CLAUDE.md and ../POLICY.md.
// ============================================================

int main() {
    std::printf("[*] cs2_external -- external, read-only CS2 ESP (box/skeleton/healthbar)\n");
    std::printf("[*] -insecure, offline bot matches only.\n");
    std::printf("[*] waiting for cs2.exe...\n");

    if (!Overlay::Init()) {
        std::printf("[!] failed to create the overlay window\n");
        return 1;
    }
    std::printf("[+] overlay ready -- press END to exit\n");

    ULONGLONG lastAttachAttempt = 0;

    while (!(GetAsyncKeyState(VK_END) & 0x8000)) {
        Overlay::PumpMessages();

        bool attached = Memory::IsAttached();
        if (!attached) {
            ULONGLONG now = GetTickCount64();
            if (now - lastAttachAttempt > 1000) {
                lastAttachAttempt = now;
                if (Memory::Attach(L"cs2.exe")) {
                    attached = true;
                    std::printf("[+] attached to cs2.exe (pid %lu)\n", Memory::GetPid());
                }
            }
        }

        Menu::PollHotkey();

        // Overlay::Update hides the window on its own whenever the game
        // window can't be found/isn't visible (process not running yet,
        // minimized, or just closed) -- this is the "degrade gracefully,
        // never crash" behavior the external project's docs call for.
        bool overlayVisible = attached && Overlay::Update(Memory::GetPid());

        // Interactive only while the menu is open (drops click-through so the
        // panel catches clicks); otherwise the overlay stays click-through.
        Overlay::SetInteractive(Menu::IsOpen() && overlayVisible);

        if (attached) Smoke::Tick(); // write feature; self-throttled + self-gated

        if (overlayVisible && Overlay::BeginFrame()) {
            ESP::Render();
            Menu::Render();
            Overlay::EndFrame();
        }

        Sleep(6); // ~150 Hz poll/draw cap -- plenty for watching bots
    }

    std::printf("[~] END pressed -- shutting down\n");
    Overlay::Shutdown();
    Memory::Detach();
    return 0;
}
