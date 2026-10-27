#include "item_scroller.h"
#include "input_utils.h"
#include "inventory_utils.h"
#include "minecraft_feature_utils.h"

#include <atomic>
#include <imgui.h>
#include <windows.h>

namespace {
    std::atomic_bool gEnabled{ false };
    std::atomic<int> gDelayMs{ 80 };
    unsigned long long gLastMoveMs = 0;
    int gLastSlotId = -1;

    bool ShiftDown() {
        return Features::Input::DownImmediate(VK_SHIFT) || Features::Input::DownImmediate(VK_LSHIFT) || Features::Input::DownImmediate(VK_RSHIFT);
    }

    bool LeftDown() {
        return Features::Input::DownImmediate(VK_LBUTTON);
    }
}

namespace Features::ItemScroller {
    void SetConfig(bool enabled, int delayMs) {
        gEnabled = enabled;
        gDelayMs = delayMs;
    }

    void Tick(JNIEnv* env, jobject mc, jobject player) {
        if (!gEnabled.load() || !env || !mc || !player || !ImGui::GetCurrentContext() || !Input::GameInputActive()) {
            gLastSlotId = -1;
            return;
        }

        if (!ShiftDown() || !LeftDown()) {
            gLastSlotId = -1;
            return;
        }

        if (!SDK::Classes.MinecraftClient_currentScreen || !SDK::Classes.HandledScreen ||
            !SDK::Classes.HandledScreen_focusedSlot || !SDK::Classes.SlotActionType_QUICK_MOVE ||
            !SDK::Classes.Slot_id) {
            return;
        }

        jobject screen = env->GetObjectField(mc, SDK::Classes.MinecraftClient_currentScreen);
        if (!screen) {
            MC::Clear(env);
            gLastSlotId = -1;
            return;
        }

        if (!MC::IsInstance(env, screen, SDK::Classes.HandledScreen)) {
            env->DeleteLocalRef(screen);
            gLastSlotId = -1;
            return;
        }

        jobject slot = env->GetObjectField(screen, SDK::Classes.HandledScreen_focusedSlot);
        env->DeleteLocalRef(screen);
        if (!slot) {
            MC::Clear(env);
            return;
        }

        const int slotId = env->GetIntField(slot, SDK::Classes.Slot_id);
        if (env->ExceptionCheck() || slotId < 0) {
            env->DeleteLocalRef(slot);
            MC::Clear(env);
            return;
        }

        if (SDK::Classes.Slot_getStack && SDK::Classes.ItemStack_isEmpty) {
            jobject stack = env->CallObjectMethod(slot, SDK::Classes.Slot_getStack);
            if (!stack || env->ExceptionCheck()) {
                MC::Clear(env);
                if (stack) {
                    env->DeleteLocalRef(stack);
                }
                env->DeleteLocalRef(slot);
                return;
            }

            const bool empty = env->CallBooleanMethod(stack, SDK::Classes.ItemStack_isEmpty) == JNI_TRUE;
            env->DeleteLocalRef(stack);
            if (env->ExceptionCheck() || empty) {
                MC::Clear(env);
                env->DeleteLocalRef(slot);
                return;
            }
        }
        env->DeleteLocalRef(slot);

        const unsigned long long now = GetTickCount64();
        if (now - gLastMoveMs < static_cast<unsigned long long>(gDelayMs.load())) {
            return;
        }

        jobject quickMove = env->GetStaticObjectField(SDK::Classes.SlotActionType, SDK::Classes.SlotActionType_QUICK_MOVE);
        if (!quickMove) {
            MC::Clear(env);
            return;
        }

        if (Inventory::ClickSlot(env, mc, player, slotId, quickMove)) {
            gLastMoveMs = now;
            gLastSlotId = slotId;
        }
        env->DeleteLocalRef(quickMove);
    }
}
