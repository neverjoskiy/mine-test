#pragma once

#include <jni.h>

namespace Features::ItemScroller {
    void SetConfig(bool enabled, int delayMs);
    void Tick(JNIEnv* env, jobject mc, jobject player);
}
