#include "smoke.h"
#include "config.h"
#include "memory.h"
#include "offsets.h"
#include "entities/entities.h"
#include <windows.h>
#include <cstring>

// ============================================================
//  Entity classification uses the same vtable->type-name chain
//  deadlocked-rust uses (entity/mod.rs):
//     vtable   = *(entity)
//     typeinfo = *(vtable - 0x8)
//     namePtr  = *(typeinfo + 0x8)
//     name     = string@namePtr   ("24C_SmokeGrenadeProjectile")
//  The "24" prefix is the length of the class name (Source2's own
//  reflection name format, not MSVC RTTI mangling).
// ============================================================

namespace {

    constexpr const char* kSmokeClass = "24C_SmokeGrenadeProjectile";
    constexpr int kMaxEntities = 1500;   // grenades sit above the player slots
    constexpr ULONGLONG kIntervalMs = 100; // 10 Hz — smoke color isn't time-critical

    bool ReadCString(uintptr_t addr, char* out, size_t cap) {
        if (!out || cap == 0) return false;
        std::memset(out, 0, cap);
        if (!Memory::ReadInto(addr, out, cap - 1)) return false;
        out[cap - 1] = '\0';
        return true;
    }

    bool IsSmoke(uintptr_t entity) {
        uintptr_t vtable = Memory::Read<uintptr_t>(entity);
        if (!Memory::IsValidPtr(vtable)) return false;
        uintptr_t typeInfo = Memory::Read<uintptr_t>(vtable - 0x8);
        if (!Memory::IsValidPtr(typeInfo)) return false;
        uintptr_t namePtr = Memory::Read<uintptr_t>(typeInfo + 0x8);
        if (!Memory::IsValidPtr(namePtr)) return false;

        char name[40];
        if (!ReadCString(namePtr, name, sizeof(name))) return false;
        return std::strcmp(name, kSmokeClass) == 0;
    }

}

namespace Smoke {

    void Tick() {
        if (!Config::bSmokeColor) return;
        if (!Memory::IsAttached()) return;

        static ULONGLONG lastRun = 0;
        ULONGLONG now = GetTickCount64();
        if (now - lastRun < kIntervalMs) return;
        lastRun = now;

        uintptr_t clientBase = Memory::GetModuleBase("client.dll");
        if (!clientBase) return;
        uintptr_t entListRaw = Memory::Read<uintptr_t>(clientBase + Offsets::dwEntityList);
        if (!Memory::IsValidPtr(entListRaw)) return;
        auto* entSystem = reinterpret_cast<CGameEntitySystem*>(entListRaw);

        float wanted[3] = {
            static_cast<float>(Config::colSmoke.r),
            static_cast<float>(Config::colSmoke.g),
            static_cast<float>(Config::colSmoke.b),
        };

        for (int i = 0; i < kMaxEntities; ++i) {
            uintptr_t ent = entSystem->GetEntityByIndex(i);
            if (!Memory::IsValidPtr(ent)) continue;
            if (!IsSmoke(ent)) continue;

            uintptr_t colorAddr = ent + Offsets::m_vSmokeColor;
            float cur[3] = {};
            Memory::ReadInto(colorAddr, cur, sizeof(cur));
            if (cur[0] != wanted[0] || cur[1] != wanted[1] || cur[2] != wanted[2])
                Memory::SafeWriteBuf(colorAddr, wanted, sizeof(wanted));
        }
    }

}
