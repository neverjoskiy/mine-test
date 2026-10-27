#pragma once

#include <array>
#include <jni.h>

namespace Features::StorageESP {
    constexpr int kStorageTypeCount = 8;
    using Color = std::array<float, 4>;
    using ColorSet = std::array<Color, kStorageTypeCount>;

    void SetConfig(bool enabled, int mode, const ColorSet& colors);
    void Tick(JNIEnv* env, jobject player, jobject world);
    void RenderOverlay();
    void Clear();
}
