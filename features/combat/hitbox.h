#pragma once

#include <jni.h>

namespace Features::HitBox {
    void SetConfig(bool enabled, float yExpand, float zExpand, bool visible);
    void Tick(JNIEnv* env, jobject player, jobject world);
    void RenderOverlay();
    void Clear();
}
