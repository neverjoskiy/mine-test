#include "aim_assist.h"
#include "minecraft_feature_utils.h"
#include "no_friend_damage.h"
#include "input_utils.h"

#include <algorithm>
#include <atomic>
#include <cmath>

namespace {
    std::atomic_bool gEnabled{ false };
    std::atomic<float> gDistance{ 4.0f };
    std::atomic<float> gFov{ 90.0f };
    std::atomic<float> gSpeed{ 2.0f };
    std::atomic_bool gOnlyPlayers{ true };

    float WrapDegrees(float value) {
        while (value > 180.f) value -= 360.f;
        while (value < -180.f) value += 360.f;
        return value;
    }

    float ToRadians(float degrees) { return degrees * 3.14159265f / 180.f; }
}

namespace Features::AimAssist {
    void SetConfig(bool enabled, float distance, float fov, float speed, bool onlyPlayers) {
        gEnabled = enabled;
        gDistance = std::clamp(distance, 1.f, 6.f);
        gFov = std::clamp(fov, 20.f, 360.f);
        gSpeed = std::clamp(speed, 0.1f, 10.f);
        gOnlyPlayers = onlyPlayers;
    }

    void Tick(JNIEnv* env, jobject mc, jobject player) {
        if (!gEnabled.load() || !Features::Input::GameInputActive() || !env || !mc || !player) {
            return;
        }

        if (!SDK::Classes.Ready || !SDK::Classes.MinecraftClient_world) {
            return;
        }

        jobject world = Features::MC::GetWorld(env, mc);
        if (!world) {
            return;
        }

        double px = 0.0, py = 0.0, pz = 0.0;
        if (!Features::MC::ReadPosition(env, player, px, py, pz)) {
            env->DeleteLocalRef(world);
            return;
        }

        float yaw = 0.f;
        float pitch = 0.f;
        if (SDK::Classes.Entity_getYaw) {
            yaw = env->CallFloatMethod(player, SDK::Classes.Entity_getYaw);
            Features::MC::Clear(env);
        } else if (SDK::Classes.Entity_yaw) {
            yaw = env->GetFloatField(player, SDK::Classes.Entity_yaw);
            Features::MC::Clear(env);
        }
        if (SDK::Classes.Entity_getPitch) {
            pitch = env->CallFloatMethod(player, SDK::Classes.Entity_getPitch);
            Features::MC::Clear(env);
        } else if (SDK::Classes.Entity_pitch) {
            pitch = env->GetFloatField(player, SDK::Classes.Entity_pitch);
            Features::MC::Clear(env);
        }

        double eyeY = py;
        if (SDK::Classes.Entity_getEyeY) {
            eyeY = env->CallDoubleMethod(player, SDK::Classes.Entity_getEyeY);
            Features::MC::Clear(env);
        } else {
            eyeY = py + 1.62;
        }

        const float distanceLimit = gDistance.load();
        const float fovLimit = gFov.load();
        const bool onlyPlayers = gOnlyPlayers.load();

        // view direction
        const float yawRad = ToRadians(yaw);
        const float pitchRad = ToRadians(pitch);
        const float viewX = -std::sin(yawRad) * std::cos(pitchRad);
        const float viewY = -std::sin(pitchRad);
        const float viewZ = std::cos(yawRad) * std::cos(pitchRad);

        double bestDist = 1e9;
        double bestX = 0, bestY = 0, bestZ = 0;
        bool found = false;

        jobject iterable = Features::MC::GetEntityIterable(env, world);
        if (iterable) {
            Features::MC::ForEachIterable(env, iterable, [&](jobject entity) {
                if (!Features::MC::IsLivingTarget(env, entity, player)) {
                    return;
                }
                if (onlyPlayers) {
                    const bool isPlayer = Features::MC::IsInstance(env, entity, SDK::Classes.PlayerEntity) ||
                        Features::MC::IsInstance(env, entity, SDK::Classes.RemotePlayerEntity);
                    if (!isPlayer) {
                        return;
                    }
                }
                if (Features::NoFriendDamage::Enabled.load() && Features::NoFriendDamage::IsFriendTarget(env, entity)) {
                    return;
                }

                double ex = 0, ey = 0, ez = 0;
                if (!Features::MC::ReadPosition(env, entity, ex, ey, ez)) {
                    return;
                }
                const double dx = ex - px;
                const double dy = (ey + 0.5) - eyeY; // aim at body center
                const double dz = ez - pz;
                const double dist = std::sqrt(dx*dx + dy*dy + dz*dz);
                if (dist > distanceLimit || dist < 0.5) {
                    return;
                }

                // FOV check
                const float toX = static_cast<float>(dx / dist);
                const float toY = static_cast<float>(dy / dist);
                const float toZ = static_cast<float>(dz / dist);
                const float dot = viewX * toX + viewY * toY + viewZ * toZ;
                const float angle = std::acos(std::clamp(dot, -1.f, 1.f)) * 180.f / 3.14159265f;
                if (angle > fovLimit * 0.5f) {
                    return;
                }

                if (dist < bestDist) {
                    bestDist = dist;
                    bestX = ex;
                    bestY = ey + 0.5; // aim height
                    bestZ = ez;
                    found = true;
                }
            });
            env->DeleteLocalRef(iterable);
        }

        env->DeleteLocalRef(world);

        if (!found) {
            return;
        }

        // compute desired angles
        const double dx = bestX - px;
        const double dy = bestY - eyeY;
        const double dz = bestZ - pz;
        const double horiz = std::sqrt(dx*dx + dz*dz);
        float desiredYaw = std::atan2(-dx, dz) * 180.f / 3.14159265f; // Minecraft yaw convention? 
        float desiredPitch = -std::atan2(dy, horiz) * 180.f / 3.14159265f;

        // Smooth
        const float speed = gSpeed.load();
        float yawDelta = WrapDegrees(desiredYaw - yaw);
        float pitchDelta = desiredPitch - pitch;
        const float factor = std::clamp(speed * 0.05f, 0.01f, 0.5f);
        yawDelta *= factor;
        pitchDelta *= factor;

        const float newYaw = yaw + yawDelta;
        const float newPitch = std::clamp(pitch + pitchDelta, -90.f, 90.f);

        if (SDK::Classes.Entity_setYaw) {
            env->CallVoidMethod(player, SDK::Classes.Entity_setYaw, newYaw);
            Features::MC::Clear(env);
        } else if (SDK::Classes.Entity_yaw) {
            env->SetFloatField(player, SDK::Classes.Entity_yaw, newYaw);
            Features::MC::Clear(env);
        }
        if (SDK::Classes.Entity_setPitch) {
            env->CallVoidMethod(player, SDK::Classes.Entity_setPitch, newPitch);
            Features::MC::Clear(env);
        } else if (SDK::Classes.Entity_pitch) {
            env->SetFloatField(player, SDK::Classes.Entity_pitch, newPitch);
            Features::MC::Clear(env);
        }
        if (SDK::Classes.Entity_setHeadYaw) {
            env->CallVoidMethod(player, SDK::Classes.Entity_setHeadYaw, newYaw);
            Features::MC::Clear(env);
        }
        if (SDK::Classes.Entity_setBodyYaw) {
            env->CallVoidMethod(player, SDK::Classes.Entity_setBodyYaw, newYaw);
            Features::MC::Clear(env);
        }
    }
}