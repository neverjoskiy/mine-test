#include "auto_respawn.h"
#include "minecraft_feature_utils.h"

#include <windows.h>

namespace {
    unsigned long long gLastRespawnMs = 0;
}

namespace Features::AutoRespawn {
    void Tick(JNIEnv* env, jobject player) {
        if (!Enabled.load() || !env || !player || !SDK::Classes.ClientPlayerEntity_requestRespawn) {
            return;
        }

        bool alive = true;
        if (SDK::Classes.PlayerEntity_isAlive) {
            alive = env->CallBooleanMethod(player, SDK::Classes.PlayerEntity_isAlive) == JNI_TRUE;
            MC::Clear(env);
        }

        if (alive) {
            return;
        }

        const unsigned long long now = GetTickCount64();
        if (now - gLastRespawnMs < 650) {
            return;
        }

        env->CallVoidMethod(player, SDK::Classes.ClientPlayerEntity_requestRespawn);
        MC::Clear(env);
        gLastRespawnMs = now;
    }
}
