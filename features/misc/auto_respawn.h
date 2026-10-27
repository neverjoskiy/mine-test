#pragma once

#include <atomic>
#include <jni.h>

namespace Features::AutoRespawn {
    inline std::atomic_bool Enabled{ false };

    void Tick(JNIEnv* env, jobject player);
}
