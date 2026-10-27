#include "shift_tap.h"
#include "input_utils.h"
#include "minecraft_feature_utils.h"

#include <atomic>
#include <windows.h>

namespace {
    std::atomic<unsigned long long> gSneakUntilMs{ 0 };

    void SetSneak(JNIEnv* env, jobject mc, bool pressed) {
        if (!env || !mc || !SDK::Classes.GameOptions_keySneak || !SDK::Classes.KeyBinding_setPressed) {
            return;
        }

        jobject options = Features::MC::GetOptions(env, mc);
        if (!options) {
            return;
        }

        jobject key = env->GetObjectField(options, SDK::Classes.GameOptions_keySneak);
        env->DeleteLocalRef(options);
        if (!key) {
            Features::MC::Clear(env);
            return;
        }

        env->CallVoidMethod(key, SDK::Classes.KeyBinding_setPressed, pressed ? JNI_TRUE : JNI_FALSE);
        Features::MC::Clear(env);
        env->DeleteLocalRef(key);
    }
}

namespace Features::ShiftTap {
    void RequestTap() {
        if (!Enabled.load()) {
            return;
        }
        gSneakUntilMs = GetTickCount64() + 70;
    }

    void Tick(JNIEnv* env, jobject mc) {
        const unsigned long long until = gSneakUntilMs.load();
        if (!until || !env || !mc) {
            return;
        }

        const unsigned long long now = GetTickCount64();
        if (now <= until) {
            SetSneak(env, mc, true);
            return;
        }

        gSneakUntilMs = 0;
        if (!Features::Input::DownImmediate(VK_SHIFT) && !Features::Input::DownImmediate(VK_LSHIFT) && !Features::Input::DownImmediate(VK_RSHIFT)) {
            SetSneak(env, mc, false);
        }
    }
}
