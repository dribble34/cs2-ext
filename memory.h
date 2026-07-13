#pragma once
#include <windows.h>
#include <cstdint>
#include <type_traits>

// ============================================================
//  MEMORY (external)
//  Same Read/Write/ReadInto/IsValidPtr/ResolveHandle call shapes as
//  the old in-process headers/esp_memory.h, now backed by
//  ReadProcessMemory/WriteProcessMemory against a remote process
//  handle instead of an in-process memcpy. No SEH needed here:
//  RPM/WPM fail by returning FALSE instead of faulting the caller,
//  which is what the old __try/__except dance had to guard against.
// ============================================================
namespace Memory {

    // Attaches to the first running process named `exeName` (e.g. L"cs2.exe").
    // Opens a PROCESS_VM_READ | PROCESS_QUERY_INFORMATION handle only — this
    // project performs no writes. Returns false if no such process exists or
    // the handle can't be opened.
    bool Attach(const wchar_t* exeName);

    // True once Attach() succeeded and the target process is still alive.
    // Callers should re-check this every tick; a dead/exited target makes
    // this false again so the overlay can hide instead of drawing stale data.
    bool IsAttached();

    void Detach();

    // PID of the attached process, or 0 if not attached. Used by overlay.cpp
    // to find the game's top-level window (GetWindowThreadProcessId match).
    DWORD GetPid();

    // Base address of `name` (e.g. "client.dll") inside the attached process.
    // Returns 0 if not attached or the module isn't loaded yet — callers poll.
    uintptr_t GetModuleBase(const char* name);

    // Non-inlined helpers containing the actual RPM/WPM calls.
    bool SafeReadBuf(uintptr_t addr, void* dst, size_t size);
    bool SafeWriteBuf(uintptr_t addr, const void* src, size_t size);

    template<typename T>
    inline T Read(uintptr_t addr) {
        static_assert(std::is_trivially_copyable_v<T>, "Read type must be trivially copyable.");
        T value{};
        if (SafeReadBuf(addr, &value, sizeof(T))) {
            return value;
        }
        return T{};
    }

    template<typename T>
    inline bool Write(uintptr_t addr, const T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "Write type must be trivially copyable.");
        return SafeWriteBuf(addr, &value, sizeof(T));
    }

    inline bool ReadInto(uintptr_t addr, void* dst, size_t n) {
        if (!dst || n == 0) return false;
        return SafeReadBuf(addr, dst, n);
    }

    // Sanity range check for a pointer in the 64-bit user address space.
    inline bool IsValidPtr(uintptr_t p) {
        return p > 0x10000 && p < 0x7FFFFFFFFFFFULL;
    }

    // Resolves a CS2 entity handle to a pawn/controller pointer via the entity
    // list. Standard CS2 entity list layout: each chunk holds 512 entries at
    // stride 0x70. (Kept for parity with CHandle<T>::Get in sdk/c_handle.h;
    // sdk/entities/entities.h's CGameEntitySystem::ResolveHandle is the
    // preferred path for new code.)
    inline uintptr_t ResolveHandle(uintptr_t entSystem, uint32_t handle) {
        if (!handle || handle == 0xFFFFFFFF) return 0;
        uint32_t index      = handle & 0x7FFF;
        uint32_t chunkIndex = index >> 9;
        uint32_t entryIndex = index & 0x1FF;
        uintptr_t chunk = Read<uintptr_t>(entSystem + 0x10 + chunkIndex * 8);
        if (!IsValidPtr(chunk)) return 0;
        uintptr_t entity = Read<uintptr_t>(chunk + entryIndex * 0x70);
        return IsValidPtr(entity) ? entity : 0;
    }
}
