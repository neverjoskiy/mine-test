#include "triggerbot.h"
#include "combat_control.h"
#include "input_utils.h"
#include "inventory_utils.h"
#include "minecraft_feature_utils.h"
#include "no_friend_damage.h"
#include "shift_tap.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <windows.h>

namespace {
    std::atomic_bool gEnabled{ false };
    std::atomic<float> gDistance{ 2.9f };
    std::atomic_bool gPlayers{ true };
    std::atomic_bool gHostiles{ false };
    std::atomic_bool gMobs{ false };
    std::atomic_bool gInvisibles{ false };
    std::atomic_bool gNoHitEat{ true };
    std::atomic_bool gNoHitBlock{ true };
    std::atomic_bool gSmartCrits{ true };
    ULONGLONG gLastAttackMs = 0;
    ULONGLONG gAttackReleaseMs = 0;
    bool gAttackKeyDown = false;

    jobject GetCrosshairEntity(JNIEnv* env, jobject mc) {
        if (!SDK::Classes.MinecraftClient_crosshairTarget || !SDK::Classes.EntityHitResult || !SDK::Classes.EntityHitResult_getEntity) {
            return nullptr;
        }

        jobject hit = env->GetObjectField(mc, SDK::Classes.MinecraftClient_crosshairTarget);
        if (!hit) {
            Features::MC::Clear(env);
            return nullptr;
        }

        if (!Features::MC::IsInstance(env, hit, SDK::Classes.EntityHitResult)) {
            env->DeleteLocalRef(hit);
            return nullptr;
        }

        jobject entity = env->CallObjectMethod(hit, SDK::Classes.EntityHitResult_getEntity);
        env->DeleteLocalRef(hit);
        if (!entity) {
            Features::MC::Clear(env);
            return nullptr;
        }
        return entity;
    }

    bool IsAllowedTarget(JNIEnv* env, jobject entity, jobject player) {
        if (!Features::MC::IsLivingTarget(env, entity, player)) {
            return false;
        }

        if (!gInvisibles.load() && SDK::Classes.Entity_isInvisible) {
            const bool invisible = env->CallBooleanMethod(entity, SDK::Classes.Entity_isInvisible) == JNI_TRUE;
            Features::MC::Clear(env);
            if (invisible) {
                return false;
            }
        }

        const bool isPlayer = Features::MC::IsInstance(env, entity, SDK::Classes.PlayerEntity) ||
            Features::MC::IsInstance(env, entity, SDK::Classes.RemotePlayerEntity);
        if (isPlayer) {
            return gPlayers.load() || (!gPlayers.load() && !gHostiles.load() && !gMobs.load());
        }

        return gHostiles.load() || gMobs.load();
    }

    bool IsCloseEnough(JNIEnv* env, jobject player, jobject entity) {
        double px = 0.0;
        double py = 0.0;
        double pz = 0.0;
        double ex = 0.0;
        double ey = 0.0;
        double ez = 0.0;
        if (!Features::MC::ReadPosition(env, player, px, py, pz) || !Features::MC::ReadPosition(env, entity, ex, ey, ez)) {
            return false;
        }

        const double dx = ex - px;
        const double dy = ey - py;
        const double dz = ez - pz;
        const double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
        return dist <= std::clamp(static_cast<double>(gDistance.load()), 1.0, 6.0);
    }

bool CooldownReady(JNIEnv* env, jobject player) {
    if (!SDK::Classes.PlayerEntity_getAttackCooldownProgress) {
        return GetTickCount64() - gLastAttackMs >= 520;
    }

    const float cooldown = env->CallFloatMethod(player, SDK::Classes.PlayerEntity_getAttackCooldownProgress, 0.0f);
    if (env->ExceptionCheck()) {
        Features::MC::Clear(env);
        return GetTickCount64() - gLastAttackMs >= 520;
    }
    
    bool onGround = true;
    if (SDK::Classes.Entity_onGround) {
        onGround = env->GetBooleanField(player, SDK::Classes.Entity_onGround) == JNI_TRUE;
        Features::MC::Clear(env);
    }
    
    ULONGLONG now = GetTickCount64();
    if (!onGround) {
        return cooldown >= 0.95f && (now - gLastAttackMs) >= 400;
    } else {
        return cooldown >= 0.92f && (now - gLastAttackMs) >= 120;
    }
}

    bool BoolMethod(JNIEnv* env, jobject entity, jmethodID method, bool fallback) {
        if (!method) {
            return fallback;
        }

        const bool result = env->CallBooleanMethod(entity, method) == JNI_TRUE;
        if (env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return fallback;
        }
        return result;
    }

    bool IsOnGround(JNIEnv* env, jobject player) {
        if (SDK::Classes.Entity_isOnGround) {
            return BoolMethod(env, player, SDK::Classes.Entity_isOnGround, true);
        }
        if (SDK::Classes.Entity_onGround) {
            const bool onGround = env->GetBooleanField(player, SDK::Classes.Entity_onGround) == JNI_TRUE;
            Features::MC::Clear(env);
            return onGround;
        }
        return true;
    }

    bool CanCritical(JNIEnv* env, jobject player) {
        if (IsOnGround(env, player)) {
            return false;
        }

        if (SDK::Classes.Entity_fallDistance) {
            const double fallDistance = env->GetDoubleField(player, SDK::Classes.Entity_fallDistance);
            Features::MC::Clear(env);
            if (fallDistance <= 0.0) {
                return false;
            }
        }

        if (BoolMethod(env, player, SDK::Classes.LivingEntity_isClimbing, false)) {
            return false;
        }
        if (BoolMethod(env, player, SDK::Classes.Entity_isTouchingWater, false)) {
            return false;
        }
        if (BoolMethod(env, player, SDK::Classes.Entity_isInLava, false)) {
            return false;
        }

        return true;
    }

    bool ShouldHoldAttack(JNIEnv* env, jobject player) {
        if (gNoHitEat.load()) {
            const bool usingItem = BoolMethod(env, player, SDK::Classes.LivingEntity_isUsingItem, false) ||
                BoolMethod(env, player, SDK::Classes.LivingEntity_isEating, false);
            if (usingItem) {
                return true;
            }
        }

        if (gNoHitBlock.load() && BoolMethod(env, player, SDK::Classes.LivingEntity_isBlocking, false)) {
            return true;
        }

        return false;
    }

    bool CriticalGateReady(JNIEnv* env, jobject player) {
        const bool criticalReady = CanCritical(env, player);
        if (!gSmartCrits.load()) {
            return criticalReady;
        }

        const bool jumpHeld = Features::Input::DownImmediate(VK_SPACE);
        const bool airborne = !IsOnGround(env, player);
        if (!jumpHeld && !airborne) {
            return true;
        }

        return criticalReady;
    }

    void ResetSprint(JNIEnv* env, jobject player, unsigned long long suppressMs) {
        if (SDK::Classes.PlayerEntity_setSprinting) {
            env->CallVoidMethod(player, SDK::Classes.PlayerEntity_setSprinting, JNI_FALSE);
            Features::MC::Clear(env);
        }
        Features::CombatControl::SuppressSprintFor(suppressMs);
    }

    bool SetAttackKey(JNIEnv* env, jobject mc, bool pressed, bool click) {
        if (!env || !mc || !SDK::Classes.GameOptions_keyAttack || !SDK::Classes.KeyBinding_setPressed) {
            return false;
        }

        jobject options = Features::MC::GetOptions(env, mc);
        if (!options) {
            return false;
        }

        jobject attackKey = env->GetObjectField(options, SDK::Classes.GameOptions_keyAttack);
        env->DeleteLocalRef(options);
        if (!attackKey) {
            Features::MC::Clear(env);
            return false;
        }

        env->CallVoidMethod(attackKey, SDK::Classes.KeyBinding_setPressed, pressed ? JNI_TRUE : JNI_FALSE);
        Features::MC::Clear(env);

        if (pressed && click && SDK::Classes.KeyBinding_timesPressed) {
            jint presses = env->GetIntField(attackKey, SDK::Classes.KeyBinding_timesPressed);
            if (env->ExceptionCheck()) {
                Features::MC::Clear(env);
                presses = 0;
            }
            env->SetIntField(attackKey, SDK::Classes.KeyBinding_timesPressed, presses + 1);
            Features::MC::Clear(env);
        }

        env->DeleteLocalRef(attackKey);
        return true;
    }

    void ReleaseAttackKeyIfNeeded(JNIEnv* env, jobject mc) {
        if (!gAttackKeyDown || GetTickCount64() < gAttackReleaseMs) {
            return;
        }

        if (SetAttackKey(env, mc, false, false)) {
            gAttackKeyDown = false;
        }
    }
}

namespace Features::TriggerBot {
    void SetConfig(bool enabled, float distance, const std::vector<bool>& targets, const std::vector<bool>& noHit, bool smartCrits) {
        gEnabled = enabled;
        gDistance = distance;
        gPlayers = targets.empty() || targets[0];
        gHostiles = targets.size() > 1 && targets[1];
        gMobs = targets.size() > 2 && targets[2];
        gInvisibles = targets.size() > 3 && targets[3];
        gNoHitEat = noHit.empty() || noHit[0];
        gNoHitBlock = noHit.size() > 1 ? noHit[1] : true;
        gSmartCrits = smartCrits;
    }

void Tick(JNIEnv* env, jobject mc, jobject player) {
    if (env && mc) {
        ReleaseAttackKeyIfNeeded(env, mc);
    }

    if (!gEnabled.load() || !Features::Input::GameInputActive() || !mc || !player) {
        return;
    }

    jobject target = GetCrosshairEntity(env, mc);
    if (!target) {
        return;
    }

    if (!IsAllowedTarget(env, target, player) ||
        (Features::NoFriendDamage::Enabled.load() && Features::NoFriendDamage::IsFriendTarget(env, target)) ||
        !IsCloseEnough(env, player, target) ||
        !CooldownReady(env, player) ||
        ShouldHoldAttack(env, player) ||
        !CriticalGateReady(env, player)) {
        env->DeleteLocalRef(target);
        return;
    }

    ResetSprint(env, player, 170);
    if (!SetAttackKey(env, mc, true, true)) {
        env->DeleteLocalRef(target);
        return;
    }
    gAttackKeyDown = true;
    gAttackReleaseMs = GetTickCount64() + 45;
    Features::ShiftTap::RequestTap();

    gLastAttackMs = GetTickCount64();
    env->DeleteLocalRef(target);
}
}
