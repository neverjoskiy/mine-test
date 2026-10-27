#pragma once

#include <atomic>
#include <jni.h>

namespace Features::ShiftTap {
    inline std::atomic_bool Enabled{ false };

    void RequestTap();
    void Tick(JNIEnv* env, jobject mc);
}
