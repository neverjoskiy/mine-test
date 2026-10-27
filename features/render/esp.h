#pragma once

#include <jni.h>
#include <array>
#include <vector>

namespace Features::ESP {
    using Color = std::array<float, 4>;

    void SetConfig(bool enabled, const std::vector<bool>& elements, int boxStyle, const Color& box3dColor);
    void Tick(JNIEnv* env, jobject player, jobject world);
    void RenderOverlay();
    void Clear();
}
