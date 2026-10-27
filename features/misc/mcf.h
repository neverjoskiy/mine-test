#pragma once

#include <jni.h>

namespace Features::MCF {
    void SetConfig(bool enabled);
    void Tick(JNIEnv* env, jobject mc, jobject player);
}
