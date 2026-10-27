#include "autosprint.h"
#include "combat_control.h"
#include "gui_move.h"
#include "input_utils.h"
#include "minecraft_classes.h"
#include "jni_bridge.h"
#include "minecraft_feature_utils.h"
#include <windows.h>

namespace Features {
    void AutoSprint::Tick() {
        if (!Enabled.load()) return;
        if (!Input::GameInputActive()) return;
        if (CombatControl::ShouldSuppressSprint()) return;

        JNIEnv* env = JNIBridge::GetEnv();
        if (!env) return;

        jobject mc = MC::GetMinecraft(env);
        if (!mc) return;

        jobject player = MC::GetPlayer(env, mc);
        if (!player) {
            env->DeleteLocalRef(mc);
            return;
        }

        bool screenOpen = false;
        if (SDK::Classes.MinecraftClient_currentScreen) {
            jobject screen = env->GetObjectField(mc, SDK::Classes.MinecraftClient_currentScreen);
            screenOpen = screen != nullptr;
            if (screen) {
                env->DeleteLocalRef(screen);
            }
            MC::Clear(env);
        }
        if (screenOpen && !GuiMove::Enabled.load()) {
            env->DeleteLocalRef(player);
            env->DeleteLocalRef(mc);
            return;
        }

        jboolean isAlive = JNI_TRUE;
        jboolean isSneaking = JNI_FALSE;

        if (SDK::Classes.PlayerEntity_isAlive) {
            isAlive = env->CallBooleanMethod(player, SDK::Classes.PlayerEntity_isAlive);
            MC::Clear(env);
        }

        if (SDK::Classes.PlayerEntity_isSneaking) {
            isSneaking = env->CallBooleanMethod(player, SDK::Classes.PlayerEntity_isSneaking);
            MC::Clear(env);
        }

        bool forwardPressed = Input::DownImmediate('W');
        if (SDK::Classes.MinecraftClient_options && SDK::Classes.GameOptions_keyForward && SDK::Classes.KeyBinding_isPressed) {
            jobject options = MC::GetOptions(env, mc);
            jobject forwardKey = options ? env->GetObjectField(options, SDK::Classes.GameOptions_keyForward) : nullptr;
            if (forwardKey) {
                forwardPressed = forwardPressed || env->CallBooleanMethod(forwardKey, SDK::Classes.KeyBinding_isPressed) == JNI_TRUE;
                MC::Clear(env);
                if (forwardPressed && !screenOpen && SDK::Classes.KeyBinding_setPressed) {
                    env->CallVoidMethod(forwardKey, SDK::Classes.KeyBinding_setPressed, JNI_TRUE);
                    MC::Clear(env);
                }
                env->DeleteLocalRef(forwardKey);
            }
            if (options) {
                env->DeleteLocalRef(options);
            }
        }

        if (isAlive && !isSneaking) {
            if (forwardPressed) {
                env->CallVoidMethod(player, SDK::Classes.PlayerEntity_setSprinting, JNI_TRUE);
                MC::Clear(env);
            }
        }

        env->DeleteLocalRef(player);
        env->DeleteLocalRef(mc);
    }
}
