#pragma once

#include <jni.h>

namespace Features::WorldTweaks {
    void SetFullBright(bool enabled);
    void SetNoRain(bool enabled);
    void SetWorldTime(bool enabled, float hour);
    void Tick(JNIEnv* env, jobject mc, jobject world);
}
