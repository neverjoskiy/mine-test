#include <windows.h>
#include <tlhelp32.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <cwchar>
#include "jni_bridge.h"
#include "hooks.h"
#include "../sdk/minecraft_classes.h"

namespace {
    HANDLE gInstanceMutex = nullptr;
    constexpr wchar_t kInstanceMutexName[] = L"Local\\LumaWareLegitClientSingleton";

    bool AnotherClientDllLoaded(HMODULE selfModule) {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, GetCurrentProcessId());
        if (snapshot == INVALID_HANDLE_VALUE) {
            return false;
        }

        MODULEENTRY32W entry{};
        entry.dwSize = sizeof(entry);
        bool duplicate = false;
        if (Module32FirstW(snapshot, &entry)) {
            do {
                if (entry.hModule != selfModule && _wcsicmp(entry.szModule, L"legit-client.dll") == 0) {
                    duplicate = true;
                    break;
                }
            } while (Module32NextW(snapshot, &entry));
        }

        CloseHandle(snapshot);
        return duplicate;
    }

    bool AcquireInstanceGuard(HMODULE selfModule) {
        if (AnotherClientDllLoaded(selfModule)) {
            return false;
        }

        gInstanceMutex = CreateMutexW(nullptr, TRUE, kInstanceMutexName);
        if (!gInstanceMutex) {
            return false;
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            CloseHandle(gInstanceMutex);
            gInstanceMutex = nullptr;
            return false;
        }

        return true;
    }

    void ReleaseInstanceGuard() {
        if (!gInstanceMutex) {
            return;
        }

        ReleaseMutex(gInstanceMutex);
        CloseHandle(gInstanceMutex);
        gInstanceMutex = nullptr;
    }
}

DWORD WINAPI InitClient(LPVOID param) {
    HMODULE selfModule = static_cast<HMODULE>(param);
    if (!AcquireInstanceGuard(selfModule)) {
        return 0;
    }

    const bool hooksReady = Hooks::InitOpenGL();

    while (!Hooks::IsShutdownRequested()) {
        if (GetAsyncKeyState(VK_END) & 1) {
            Hooks::RequestShutdown();
            break;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    if (hooksReady) {
        Hooks::RequestShutdown();
        Hooks::Shutdown();
    }

    ReleaseInstanceGuard();
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID reserved) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        HANDLE thread = CreateThread(nullptr, 0, InitClient, hModule, 0, nullptr);
        if (thread) {
            CloseHandle(thread);
        }
    }
    return TRUE;
}
