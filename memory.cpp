#include "memory.h"
#include <tlhelp32.h>
#include <cstring>
#include <iterator>

namespace Memory {

    namespace {
        HANDLE g_hProcess = nullptr;
        DWORD  g_pid      = 0;

        DWORD FindProcessId(const wchar_t* exeName) {
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
            if (snap == INVALID_HANDLE_VALUE) return 0;

            PROCESSENTRY32W entry{};
            entry.dwSize = sizeof(entry);
            DWORD pid = 0;

            if (Process32FirstW(snap, &entry)) {
                do {
                    if (_wcsicmp(entry.szExeFile, exeName) == 0) {
                        pid = entry.th32ProcessID;
                        break;
                    }
                } while (Process32NextW(snap, &entry));
            }
            CloseHandle(snap);
            return pid;
        }
    }

    bool Attach(const wchar_t* exeName) {
        Detach();

        DWORD pid = FindProcessId(exeName);
        if (!pid) return false;

        // VM_READ for ESP, plus VM_WRITE/VM_OPERATION for the local-only write
        // features (FOV changer, smoke color). All writes target the local
        // client's own user-space memory in an offline -insecure session — see
        // ../POLICY.md. Still a loud, unmasked handle; no evasion.
        HANDLE h = OpenProcess(
            PROCESS_VM_READ | PROCESS_VM_WRITE | PROCESS_VM_OPERATION | PROCESS_QUERY_INFORMATION,
            FALSE, pid);
        if (!h) return false;

        g_hProcess = h;
        g_pid      = pid;
        return true;
    }

    bool IsAttached() {
        if (!g_hProcess) return false;
        DWORD exitCode = 0;
        if (!GetExitCodeProcess(g_hProcess, &exitCode)) return false;
        return exitCode == STILL_ACTIVE;
    }

    void Detach() {
        if (g_hProcess) {
            CloseHandle(g_hProcess);
            g_hProcess = nullptr;
        }
        g_pid = 0;
    }

    DWORD GetPid() {
        return g_pid;
    }

    uintptr_t GetModuleBase(const char* name) {
        if (!g_pid) return 0;

        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, g_pid);
        if (snap == INVALID_HANDLE_VALUE) return 0;

        // tlhelp32.h only defines an explicit W (wide) variant, not an
        // explicit A one — with the project's Unicode charset the unqualified
        // names already resolve to W, so use that explicitly and convert the
        // public API's narrow module name once for the comparison.
        wchar_t wname[64]{};
        MultiByteToWideChar(CP_UTF8, 0, name, -1, wname, static_cast<int>(std::size(wname)));

        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        uintptr_t base = 0;

        if (Module32FirstW(snap, &entry)) {
            do {
                if (_wcsicmp(entry.szModule, wname) == 0) {
                    base = reinterpret_cast<uintptr_t>(entry.modBaseAddr);
                    break;
                }
            } while (Module32NextW(snap, &entry));
        }
        CloseHandle(snap);
        return base;
    }

    bool SafeReadBuf(uintptr_t addr, void* dst, size_t size) {
        if (!g_hProcess || !addr || !dst || size == 0) return false;
        SIZE_T bytesRead = 0;
        return ReadProcessMemory(g_hProcess, reinterpret_cast<LPCVOID>(addr), dst, size, &bytesRead)
            && bytesRead == size;
    }

    bool SafeWriteBuf(uintptr_t addr, const void* src, size_t size) {
        // Unused this milestone (ESP is read-only) — kept for call-shape
        // parity with the internal project's Memory:: namespace.
        if (!g_hProcess || !addr || !src || size == 0) return false;
        SIZE_T bytesWritten = 0;
        return WriteProcessMemory(g_hProcess, reinterpret_cast<LPVOID>(addr), src, size, &bytesWritten)
            && bytesWritten == size;
    }

}
