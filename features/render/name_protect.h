#pragma once

#include <jni.h>
#include <string>

namespace Features::NameProtect {
    void SetConfig(bool enabled, bool onFriends);
    void Tick(JNIEnv* env, jobject mc, jobject player, jobject world);

    bool Enabled();
    std::string Apply(const std::string& name, bool isFriend, bool isLocal = false);
    std::string OriginalNameForId(int entityId, const std::string& fallback);
}
