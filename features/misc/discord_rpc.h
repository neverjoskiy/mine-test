#pragma once

#include <jni.h>

namespace Features::DiscordRPC {
    void SetConfig(bool enabled);
    void Tick(JNIEnv* env, jobject mc, jobject player);
    void Shutdown();
}
