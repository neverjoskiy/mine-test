#pragma once

#include <array>
#include <atomic>
#include <windows.h>
#include <imgui.h>

namespace Features::Input {
    inline std::atomic_bool gForegroundActive{ true };
    inline std::atomic_bool gGameInputActive{ true };
    inline std::array<bool, ImGuiKey_COUNT> gDown{};
    inline std::array<bool, ImGuiKey_COUNT> gPressed{};
    inline std::array<bool, 256> gVkDown{};
    inline std::array<bool, 256> gVkPressed{};
    inline int gLastFrame = -1;
    inline unsigned long long gLastTick = 0;
    inline int gVkLastFrame = -1;
    inline unsigned long long gVkLastTick = 0;

    inline void ClearState() {
        gDown.fill(false);
        gPressed.fill(false);
        gVkDown.fill(false);
        gVkPressed.fill(false);
        gLastFrame = -1;
        gLastTick = 0;
        gVkLastFrame = -1;
        gVkLastTick = 0;
    }

    inline void SetForegroundActive(bool active) {
        const bool previous = gForegroundActive.exchange(active);
        if (previous && !active) {
            ClearState();
        }
    }

    inline bool ForegroundActive() {
        return gForegroundActive.load();
    }

    inline void SetGameInputActive(bool active) {
        const bool next = active && ForegroundActive();
        const bool previous = gGameInputActive.exchange(next);
        if (previous && !next) {
            ClearState();
        }
    }

    inline bool GameInputActive() {
        return gGameInputActive.load() && ForegroundActive();
    }

    inline int VkFromImGuiKey(int key) {
        if (key >= ImGuiKey_A && key <= ImGuiKey_Z) {
            return 'A' + (key - ImGuiKey_A);
        }
        if (key >= ImGuiKey_0 && key <= ImGuiKey_9) {
            return '0' + (key - ImGuiKey_0);
        }
        if (key >= ImGuiKey_F1 && key <= ImGuiKey_F24) {
            return VK_F1 + (key - ImGuiKey_F1);
        }
        if (key >= ImGuiKey_Keypad0 && key <= ImGuiKey_Keypad9) {
            return VK_NUMPAD0 + (key - ImGuiKey_Keypad0);
        }

        switch (key) {
        case ImGuiKey_Tab: return VK_TAB;
        case ImGuiKey_LeftArrow: return VK_LEFT;
        case ImGuiKey_RightArrow: return VK_RIGHT;
        case ImGuiKey_UpArrow: return VK_UP;
        case ImGuiKey_DownArrow: return VK_DOWN;
        case ImGuiKey_PageUp: return VK_PRIOR;
        case ImGuiKey_PageDown: return VK_NEXT;
        case ImGuiKey_Home: return VK_HOME;
        case ImGuiKey_End: return VK_END;
        case ImGuiKey_Insert: return VK_INSERT;
        case ImGuiKey_Delete: return VK_DELETE;
        case ImGuiKey_Backspace: return VK_BACK;
        case ImGuiKey_Space: return VK_SPACE;
        case ImGuiKey_Enter: return VK_RETURN;
        case ImGuiKey_Escape: return VK_ESCAPE;
        case ImGuiKey_LeftCtrl: return VK_LCONTROL;
        case ImGuiKey_LeftShift: return VK_LSHIFT;
        case ImGuiKey_LeftAlt: return VK_LMENU;
        case ImGuiKey_LeftSuper: return VK_LWIN;
        case ImGuiKey_RightCtrl: return VK_RCONTROL;
        case ImGuiKey_RightShift: return VK_RSHIFT;
        case ImGuiKey_RightAlt: return VK_RMENU;
        case ImGuiKey_RightSuper: return VK_RWIN;
        case ImGuiKey_Menu: return VK_APPS;
        case ImGuiKey_Apostrophe: return VK_OEM_7;
        case ImGuiKey_Comma: return VK_OEM_COMMA;
        case ImGuiKey_Minus: return VK_OEM_MINUS;
        case ImGuiKey_Period: return VK_OEM_PERIOD;
        case ImGuiKey_Slash: return VK_OEM_2;
        case ImGuiKey_Semicolon: return VK_OEM_1;
        case ImGuiKey_Equal: return VK_OEM_PLUS;
        case ImGuiKey_LeftBracket: return VK_OEM_4;
        case ImGuiKey_Backslash: return VK_OEM_5;
        case ImGuiKey_RightBracket: return VK_OEM_6;
        case ImGuiKey_GraveAccent: return VK_OEM_3;
        case ImGuiKey_CapsLock: return VK_CAPITAL;
        case ImGuiKey_ScrollLock: return VK_SCROLL;
        case ImGuiKey_NumLock: return VK_NUMLOCK;
        case ImGuiKey_PrintScreen: return VK_SNAPSHOT;
        case ImGuiKey_Pause: return VK_PAUSE;
        case ImGuiKey_KeypadDecimal: return VK_DECIMAL;
        case ImGuiKey_KeypadDivide: return VK_DIVIDE;
        case ImGuiKey_KeypadMultiply: return VK_MULTIPLY;
        case ImGuiKey_KeypadSubtract: return VK_SUBTRACT;
        case ImGuiKey_KeypadAdd: return VK_ADD;
        case ImGuiKey_KeypadEnter: return VK_RETURN;
        case ImGuiKey_KeypadEqual: return VK_OEM_PLUS;
        case ImGuiKey_MouseLeft: return VK_LBUTTON;
        case ImGuiKey_MouseRight: return VK_RBUTTON;
        case ImGuiKey_MouseMiddle: return VK_MBUTTON;
        case ImGuiKey_MouseX1: return VK_XBUTTON1;
        case ImGuiKey_MouseX2: return VK_XBUTTON2;
        default: return 0;
        }
    }

    inline int VkFromKeyOrVk(int key) {
        const int vk = VkFromImGuiKey(key);
        if (vk > 0) {
            return vk;
        }
        if (key > 0 && key < 256) {
            return key;
        }
        return 0;
    }

    inline void RefreshVk() {
        if (!GameInputActive()) {
            ClearState();
            return;
        }

        const int frame = ImGui::GetCurrentContext() ? ImGui::GetFrameCount() : -1;
        const unsigned long long tick = GetTickCount64();
        if ((frame >= 0 && frame == gVkLastFrame) || (frame < 0 && tick == gVkLastTick)) {
            return;
        }

        gVkLastFrame = frame;
        gVkLastTick = tick;

        for (int vk = 0; vk < 256; ++vk) {
            const bool wasDown = gVkDown[static_cast<size_t>(vk)];
            const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
            gVkDown[static_cast<size_t>(vk)] = down;
            gVkPressed[static_cast<size_t>(vk)] = down && !wasDown;
        }
    }

    inline void Refresh() {
        if (!GameInputActive()) {
            ClearState();
            return;
        }

        const int frame = ImGui::GetCurrentContext() ? ImGui::GetFrameCount() : -1;
        const unsigned long long tick = GetTickCount64();
        if ((frame >= 0 && frame == gLastFrame) || (frame < 0 && tick == gLastTick)) {
            return;
        }

        gLastFrame = frame;
        gLastTick = tick;

        for (int key = 0; key < ImGuiKey_COUNT; ++key) {
            const bool wasDown = gDown[static_cast<size_t>(key)];
            bool down = false;
            const int vk = VkFromImGuiKey(key);
            if (vk > 0) {
                down = (GetAsyncKeyState(vk) & 0x8000) != 0;
            } else if (ImGui::GetCurrentContext()) {
                down = ImGui::IsKeyDown(static_cast<ImGuiKey>(key));
            }

            gDown[static_cast<size_t>(key)] = down;
            gPressed[static_cast<size_t>(key)] = down && !wasDown;
        }
    }

    inline bool Down(int key) {
        if (!GameInputActive()) {
            return false;
        }
        if (key <= 0) {
            return false;
        }
        if (key >= ImGuiKey_COUNT || key < ImGuiKey_NamedKey_BEGIN) {
            const int vk = VkFromKeyOrVk(key);
            if (vk <= 0) {
                return false;
            }
            RefreshVk();
            return gVkDown[static_cast<size_t>(vk)];
        }
        Refresh();
        return gDown[static_cast<size_t>(key)];
    }

    inline bool Pressed(int key, bool repeat = false) {
        if (!GameInputActive()) {
            return false;
        }
        if (key <= 0) {
            return false;
        }
        if (key >= ImGuiKey_COUNT || key < ImGuiKey_NamedKey_BEGIN) {
            const int vk = VkFromKeyOrVk(key);
            if (vk <= 0) {
                return false;
            }
            RefreshVk();
            if (repeat) {
                return gVkDown[static_cast<size_t>(vk)];
            }
            return gVkPressed[static_cast<size_t>(vk)];
        }

        Refresh();
        if (repeat) {
            return gDown[static_cast<size_t>(key)];
        }
        return gPressed[static_cast<size_t>(key)];
    }

    inline bool DownImmediate(int key) {
        if (!GameInputActive()) {
            return false;
        }

        const int vk = VkFromKeyOrVk(key);
        if (vk > 0) {
            return (GetAsyncKeyState(vk) & 0x8000) != 0;
        }

        return key > 0 && ImGui::GetCurrentContext() && ImGui::IsKeyDown(static_cast<ImGuiKey>(key));
    }
}
