#pragma once
#include <atomic>
#include <jni.h>

namespace Features::GuiMove {
    inline std::atomic_bool Enabled{ false };

    void Tick(JNIEnv* env, jobject mc);
}
