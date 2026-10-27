#include "gui_move.h"
#include "input_utils.h"
#include "minecraft_feature_utils.h"
#include "minecraft_classes.h"

#include <windows.h>

namespace {
    bool KeyDown(int vk) {
        return Features::Input::DownImmediate(vk);
    }

    void SetKey(JNIEnv* env, jobject options, jfieldID field, bool pressed) {
        if (!env || !options || !field || !SDK::Classes.KeyBinding_setPressed) {
            return;
        }

        jobject key = env->GetObjectField(options, field);
        if (!key) {
            Features::MC::Clear(env);
            return;
        }

        env->CallVoidMethod(key, SDK::Classes.KeyBinding_setPressed, pressed ? JNI_TRUE : JNI_FALSE);
        Features::MC::Clear(env);
        env->DeleteLocalRef(key);
    }

    void ReleaseMovementKeys(JNIEnv* env, jobject options) {
        SetKey(env, options, SDK::Classes.GameOptions_keyForward, false);
        SetKey(env, options, SDK::Classes.GameOptions_keyBack, false);
        SetKey(env, options, SDK::Classes.GameOptions_keyLeft, false);
        SetKey(env, options, SDK::Classes.GameOptions_keyRight, false);
        SetKey(env, options, SDK::Classes.GameOptions_keyJump, false);
        SetKey(env, options, SDK::Classes.GameOptions_keySneak, false);
    }
}

namespace Features::GuiMove {
    void Tick(JNIEnv* env, jobject mc) {
        if (!env || !mc || !SDK::Classes.MinecraftClient_options || !SDK::Classes.MinecraftClient_currentScreen) {
            return;
        }

        jobject screen = env->GetObjectField(mc, SDK::Classes.MinecraftClient_currentScreen);
        if (!screen) {
            MC::Clear(env);
            return;
        }

        const bool chatOpen = SDK::Classes.ChatScreen && MC::IsInstance(env, screen, SDK::Classes.ChatScreen);
        env->DeleteLocalRef(screen);

        jobject options = MC::GetOptions(env, mc);
        if (!options) {
            return;
        }

        if (!Enabled.load() || !Features::Input::GameInputActive() || chatOpen) {
            ReleaseMovementKeys(env, options);
            env->DeleteLocalRef(options);
            return;
        }

        SetKey(env, options, SDK::Classes.GameOptions_keyForward, KeyDown('W'));
        SetKey(env, options, SDK::Classes.GameOptions_keyBack, KeyDown('S'));
        SetKey(env, options, SDK::Classes.GameOptions_keyLeft, KeyDown('A'));
        SetKey(env, options, SDK::Classes.GameOptions_keyRight, KeyDown('D'));
        SetKey(env, options, SDK::Classes.GameOptions_keyJump, KeyDown(VK_SPACE));
        SetKey(env, options, SDK::Classes.GameOptions_keySneak, KeyDown(VK_SHIFT));

        env->DeleteLocalRef(options);
    }
}
