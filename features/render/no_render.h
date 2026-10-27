#pragma once

#include <jni.h>
#include <vector>

namespace Features::NoRender {
    void SetEnabled(bool enabled);
    void SetTargets(const std::vector<bool>& targets);
    void Tick(JNIEnv* env, jobject mc, jobject player, jobject world);
}
