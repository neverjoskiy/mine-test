#pragma once

#include <jni.h>

namespace Features::AimAssist {
    void SetConfig(bool enabled, float distance, float fov, float speed, bool onlyPlayers);
    void Tick(JNIEnv* env, jobject mc, jobject player);
}