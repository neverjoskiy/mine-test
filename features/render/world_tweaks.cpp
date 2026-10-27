#include "world_tweaks.h"
#include "minecraft_feature_utils.h"

#include <algorithm>
#include <atomic>

namespace {
    std::atomic_bool gFullBright{ false };
    std::atomic_bool gNoRain{ false };
    std::atomic_bool gWorldTime{ false };
    std::atomic<float> gWorldTimeHour{ 12.0f };
    bool gGammaStored = false;
    double gOriginalGamma = 1.0;
}

namespace Features::WorldTweaks {
    void SetFullBright(bool enabled) {
        gFullBright = enabled;
    }

    void SetNoRain(bool enabled) {
        gNoRain = enabled;
    }

    void SetWorldTime(bool enabled, float hour) {
        gWorldTime = enabled;
        gWorldTimeHour = hour;
    }

    void Tick(JNIEnv* env, jobject mc, jobject world) {
        if (world && gWorldTime.load() && SDK::Classes.ClientWorld_setTime) {
            const float hour = std::clamp(gWorldTimeHour.load(), 0.0f, 24.0f);
            const long long dayTime = static_cast<long long>((hour / 24.0f) * 24000.0f) % 24000LL;
            env->CallVoidMethod(world, SDK::Classes.ClientWorld_setTime, static_cast<jlong>(0), static_cast<jlong>(dayTime), JNI_FALSE);
            MC::Clear(env);
        }

        if (world && gNoRain.load()) {
            if (SDK::Classes.World_getLevelProperties && SDK::Classes.WorldProperties_setRaining) {
                jobject properties = env->CallObjectMethod(world, SDK::Classes.World_getLevelProperties);
                if (properties) {
                    env->CallVoidMethod(properties, SDK::Classes.WorldProperties_setRaining, JNI_FALSE);
                    env->DeleteLocalRef(properties);
                }
                MC::Clear(env);
            }
            if (SDK::Classes.World_setRainGradient) {
                env->CallVoidMethod(world, SDK::Classes.World_setRainGradient, 0.0f);
                MC::Clear(env);
            }
            if (SDK::Classes.World_setThunderGradient) {
                env->CallVoidMethod(world, SDK::Classes.World_setThunderGradient, 0.0f);
                MC::Clear(env);
            }
        }

        if (!SDK::Classes.GameOptions_getGamma || !SDK::Classes.SimpleOption_getValue || !SDK::Classes.JavaDouble_valueOf) {
            return;
        }

        jobject options = MC::GetOptions(env, mc);
        if (!options) {
            return;
        }

        jobject gamma = env->CallObjectMethod(options, SDK::Classes.GameOptions_getGamma);
        env->DeleteLocalRef(options);
        if (!gamma) {
            MC::Clear(env);
            return;
        }

        const bool enabled = gFullBright.load();
        if (enabled && !gGammaStored) {
            jobject current = env->CallObjectMethod(gamma, SDK::Classes.SimpleOption_getValue);
            if (current && SDK::Classes.JavaDouble_doubleValue) {
                gOriginalGamma = env->CallDoubleMethod(current, SDK::Classes.JavaDouble_doubleValue);
                gGammaStored = !env->ExceptionCheck();
                env->DeleteLocalRef(current);
            }
            MC::Clear(env);
        }

        if (enabled || gGammaStored) {
            const double targetGamma = enabled ? 15.0 : gOriginalGamma;
            jobject boxed = env->CallStaticObjectMethod(SDK::Classes.JavaDouble, SDK::Classes.JavaDouble_valueOf, targetGamma);
            if (boxed) {
                if (SDK::Classes.SimpleOption_value) {
                    env->SetObjectField(gamma, SDK::Classes.SimpleOption_value, boxed);
                } else if (SDK::Classes.SimpleOption_setValue) {
                    env->CallVoidMethod(gamma, SDK::Classes.SimpleOption_setValue, boxed);
                }
                env->DeleteLocalRef(boxed);
            }
            MC::Clear(env);
        }

        if (!enabled) {
            gGammaStored = false;
        }

        env->DeleteLocalRef(gamma);
    }
}
