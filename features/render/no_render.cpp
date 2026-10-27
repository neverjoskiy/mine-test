#include "no_render.h"
#include "minecraft_feature_utils.h"

#include <atomic>

namespace {
    std::atomic_bool gEnabled{ false };
    std::atomic_bool gFire{ false };
    std::atomic_bool gPotions{ false };
    std::atomic_bool gParticles{ false };
    std::atomic_bool gTotem{ false };
    std::atomic_bool gArmorStands{ false };
}

namespace Features::NoRender {
    void SetEnabled(bool enabled) {
        gEnabled = enabled;
    }

    void SetTargets(const std::vector<bool>& targets) {
        gFire = targets.size() > 0 && targets[0];
        gPotions = targets.size() > 1 && targets[1];
        gParticles = targets.size() > 2 && targets[2];
        gTotem = targets.size() > 3 && targets[3];
        gArmorStands = targets.size() > 4 && targets[4];
    }

    void Tick(JNIEnv* env, jobject mc, jobject player, jobject world) {
        if (!gEnabled.load()) {
            return;
        }

        if (gFire.load() && player && SDK::Classes.Entity_setFireTicks) {
            env->CallVoidMethod(player, SDK::Classes.Entity_setFireTicks, 0);
            MC::Clear(env);
        }

        if ((gParticles.load() || gPotions.load()) && SDK::Classes.MinecraftClient_particleManager && SDK::Classes.ParticleManager_clearParticles) {
            jobject particles = env->GetObjectField(mc, SDK::Classes.MinecraftClient_particleManager);
            if (particles) {
                env->CallVoidMethod(particles, SDK::Classes.ParticleManager_clearParticles);
                env->DeleteLocalRef(particles);
            }
            MC::Clear(env);
        }

        if (gTotem.load() && SDK::Classes.MinecraftClient_gameRenderer && SDK::Classes.GameRenderer_overlayRenderer && SDK::Classes.InGameOverlayRenderer_clearFloatingItem) {
            jobject renderer = env->GetObjectField(mc, SDK::Classes.MinecraftClient_gameRenderer);
            if (renderer) {
                jobject overlay = env->GetObjectField(renderer, SDK::Classes.GameRenderer_overlayRenderer);
                if (overlay) {
                    env->CallVoidMethod(overlay, SDK::Classes.InGameOverlayRenderer_clearFloatingItem);
                    env->DeleteLocalRef(overlay);
                }
                env->DeleteLocalRef(renderer);
            }
            MC::Clear(env);
        }

        if (gArmorStands.load() && world && SDK::Classes.ArmorStandEntity && SDK::Classes.Entity_setInvisible) {
            jobject iterable = MC::GetEntityIterable(env, world);
            if (iterable) {
                MC::ForEachIterable(env, iterable, [&](jobject entity) {
                    if (MC::IsInstance(env, entity, SDK::Classes.ArmorStandEntity)) {
                        env->CallVoidMethod(entity, SDK::Classes.Entity_setInvisible, JNI_TRUE);
                    }
                    MC::Clear(env);
                });
                env->DeleteLocalRef(iterable);
            }
            MC::Clear(env);
        }
    }
}
