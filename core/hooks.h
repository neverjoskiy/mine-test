#pragma once
#include <windows.h>

namespace Hooks {
    bool InitOpenGL();
    void RequestShutdown();
    bool IsShutdownRequested();
    bool IsRenderShutdownComplete();
    void Shutdown();

    typedef BOOL(WINAPI* SwapBuffers_t)(HDC hdc);
    inline SwapBuffers_t OriginalSwapBuffers = nullptr;
    inline SwapBuffers_t OriginalWglSwapBuffers = nullptr;

    BOOL WINAPI SwapBuffers_Hook(HDC hdc);
    BOOL WINAPI WglSwapBuffers_Hook(HDC hdc);

    typedef void(__cdecl* GlfwSwapBuffers_t)(void* window);
    inline GlfwSwapBuffers_t OriginalGlfwSwapBuffers = nullptr;

    void __cdecl GlfwSwapBuffers_Hook(void* window);
}
