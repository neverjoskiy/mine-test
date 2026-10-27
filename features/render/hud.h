#pragma once

#include <jni.h>

namespace Features::HUD {
    void Tick(JNIEnv* env, jobject mc, jobject player);
    void Render();
}
