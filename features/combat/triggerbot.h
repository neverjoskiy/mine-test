#pragma once

#include <jni.h>
#include <vector>

namespace Features::TriggerBot {
    void SetConfig(bool enabled, float distance, const std::vector<bool>& targets, const std::vector<bool>& noHit, bool smartCrits);
    void Tick(JNIEnv* env, jobject mc, jobject player);
}
