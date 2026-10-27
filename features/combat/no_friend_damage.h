#pragma once

#include <atomic>
#include <jni.h>

namespace Features::NoFriendDamage {
    inline std::atomic_bool Enabled{ false };

    bool IsFriendTarget(JNIEnv* env, jobject entity);
    bool ShouldCancelCurrentAttack();
}
