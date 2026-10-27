#pragma once

#include "jni_bridge.h"
#include "minecraft_classes.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <utility>
#include <jni.h>
#include <imgui.h>

namespace Features::MC {
    constexpr double kPi = 3.14159265358979323846;

    struct EntitySnapshot {
        int id{};
        std::string name{ "Entity" };
        double x{};
        double y{};
        double z{};
        double prevX{};
        double prevY{};
        double prevZ{};
        float width{ 0.6f };
        float height{ 1.8f };
        float health{ 20.f };
        float distance{};
        bool player{};
        bool living{};
    };

    struct CameraSnapshot {
        double x{};
        double y{};
        double z{};
        float pitch{};
        float yaw{};
        float fov{ 70.f };
    };

    inline void Clear(JNIEnv* env) {
        if (env && env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    inline jobject GetMinecraft(JNIEnv* env) {
        if (!env || !SDK::Classes.MinecraftClient || !SDK::Classes.MinecraftClient_getInstance) {
            return nullptr;
        }

        jobject mc = env->CallStaticObjectMethod(SDK::Classes.MinecraftClient, SDK::Classes.MinecraftClient_getInstance);
        if (!mc) {
            Clear(env);
        }
        return mc;
    }

    inline jobject GetWorld(JNIEnv* env, jobject mc) {
        if (!env || !mc || !SDK::Classes.MinecraftClient_world) {
            return nullptr;
        }

        jobject world = env->GetObjectField(mc, SDK::Classes.MinecraftClient_world);
        if (!world) {
            Clear(env);
        }
        return world;
    }

    inline jobject GetPlayer(JNIEnv* env, jobject mc) {
        if (!env || !mc || !SDK::Classes.MinecraftClient_player) {
            return nullptr;
        }

        jobject player = env->GetObjectField(mc, SDK::Classes.MinecraftClient_player);
        if (!player) {
            Clear(env);
        }
        return player;
    }

    inline jobject GetOptions(JNIEnv* env, jobject mc) {
        if (!env || !mc || !SDK::Classes.MinecraftClient_options) {
            return nullptr;
        }

        jobject options = env->GetObjectField(mc, SDK::Classes.MinecraftClient_options);
        if (!options) {
            Clear(env);
        }
        return options;
    }

    inline bool ReadPosition(JNIEnv* env, jobject entity, double& x, double& y, double& z) {
        if (!env || !entity || !SDK::Classes.Entity_getX || !SDK::Classes.Entity_getY || !SDK::Classes.Entity_getZ) {
            return false;
        }

        x = env->CallDoubleMethod(entity, SDK::Classes.Entity_getX);
        y = env->CallDoubleMethod(entity, SDK::Classes.Entity_getY);
        z = env->CallDoubleMethod(entity, SDK::Classes.Entity_getZ);
        if (env->ExceptionCheck()) {
            Clear(env);
            return false;
        }
        return true;
    }

    template <typename Fn>
    void ForEachIterable(JNIEnv* env, jobject iterable, Fn&& fn) {
        if (!env || !iterable) {
            return;
        }

        jclass iterableClass = env->GetObjectClass(iterable);
        if (!iterableClass) {
            Clear(env);
            return;
        }

        jmethodID iteratorMethod = env->GetMethodID(iterableClass, "iterator", "()Ljava/util/Iterator;");
        env->DeleteLocalRef(iterableClass);
        if (!iteratorMethod) {
            Clear(env);
            return;
        }

        jobject iterator = env->CallObjectMethod(iterable, iteratorMethod);
        if (!iterator) {
            Clear(env);
            return;
        }

        jclass iteratorClass = env->GetObjectClass(iterator);
        jmethodID hasNext = iteratorClass ? env->GetMethodID(iteratorClass, "hasNext", "()Z") : nullptr;
        jmethodID next = iteratorClass ? env->GetMethodID(iteratorClass, "next", "()Ljava/lang/Object;") : nullptr;
        if (iteratorClass) {
            env->DeleteLocalRef(iteratorClass);
        }

        if (!hasNext || !next) {
            Clear(env);
            env->DeleteLocalRef(iterator);
            return;
        }

        for (int count = 0; count < 512; ++count) {
            jboolean more = env->CallBooleanMethod(iterator, hasNext);
            if (env->ExceptionCheck() || !more) {
                Clear(env);
                break;
            }

            jobject entity = env->CallObjectMethod(iterator, next);
            if (!entity) {
                Clear(env);
                continue;
            }

            fn(entity);
            Clear(env);
            env->DeleteLocalRef(entity);
        }

        env->DeleteLocalRef(iterator);
    }

    inline bool IsInstance(JNIEnv* env, jobject object, jclass cls) {
        if (!env || !object || !cls) {
            return false;
        }

        const bool result = env->IsInstanceOf(object, cls) == JNI_TRUE;
        Clear(env);
        return result;
    }

    inline bool LooksLikeLivingEntity(JNIEnv* env, jobject entity) {
        if (!env || !entity) {
            return false;
        }

        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        if (!ReadPosition(env, entity, x, y, z)) {
            return false;
        }

        double width = 0.6;
        double height = 1.8;
        if (SDK::Classes.Entity_getBoundingBox) {
            jobject box = env->CallObjectMethod(entity, SDK::Classes.Entity_getBoundingBox);
            if (box && !env->ExceptionCheck()) {
                const double minX = SDK::Classes.Box_minX ? env->GetDoubleField(box, SDK::Classes.Box_minX) : x - 0.3;
                const double minY = SDK::Classes.Box_minY ? env->GetDoubleField(box, SDK::Classes.Box_minY) : y;
                const double minZ = SDK::Classes.Box_minZ ? env->GetDoubleField(box, SDK::Classes.Box_minZ) : z - 0.3;
                const double maxX = SDK::Classes.Box_maxX ? env->GetDoubleField(box, SDK::Classes.Box_maxX) : x + 0.3;
                const double maxY = SDK::Classes.Box_maxY ? env->GetDoubleField(box, SDK::Classes.Box_maxY) : y + 1.8;
                const double maxZ = SDK::Classes.Box_maxZ ? env->GetDoubleField(box, SDK::Classes.Box_maxZ) : z + 0.3;
                width = std::max(maxX - minX, maxZ - minZ);
                height = maxY - minY;
                env->DeleteLocalRef(box);
            }
            Clear(env);
        } else if (SDK::Classes.Entity_getHeight) {
            height = env->CallFloatMethod(entity, SDK::Classes.Entity_getHeight);
            Clear(env);
        }

        return std::isfinite(width) && std::isfinite(height) &&
            width >= 0.25 && width <= 3.2 &&
            height >= 0.65 && height <= 4.2;
    }

    inline bool IsLivingTarget(JNIEnv* env, jobject entity, jobject localPlayer) {
        if (!env || !entity) {
            return false;
        }

        if (localPlayer && env->IsSameObject(entity, localPlayer)) {
            Clear(env);
            return false;
        }

        if (SDK::Classes.LivingEntity && IsInstance(env, entity, SDK::Classes.LivingEntity)) {
            return true;
        }

        if (SDK::Classes.RemotePlayerEntity && IsInstance(env, entity, SDK::Classes.RemotePlayerEntity)) {
            return true;
        }

        return LooksLikeLivingEntity(env, entity);
    }

    inline std::string CleanProfileName(std::string name) {
        std::string cleaned;
        cleaned.reserve(name.size());
        for (size_t i = 0; i < name.size(); ++i) {
            if (static_cast<unsigned char>(name[i]) == 0xC2 && i + 1 < name.size() && static_cast<unsigned char>(name[i + 1]) == 0xA7) {
                i += (i + 2 < name.size()) ? 2 : 1;
                continue;
            }
            if (name[i] == static_cast<char>(0xA7) && i + 1 < name.size()) {
                ++i;
                continue;
            }
            cleaned.push_back(name[i]);
        }

        const auto begin = std::find_if(cleaned.begin(), cleaned.end(), [](unsigned char ch) { return !std::isspace(ch); });
        const auto end = std::find_if(cleaned.rbegin(), cleaned.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base();
        if (begin >= end) {
            return "Player";
        }
        return std::string(begin, end);
    }

    inline jmethodID FindGameProfileMethod(JNIEnv* env, jobject entity) {
        if (!env || !entity) {
            return nullptr;
        }

        jclass entityClass = env->GetObjectClass(entity);
        if (!entityClass) {
            Clear(env);
            return nullptr;
        }

        constexpr const char* descriptor = "()Lcom/mojang/authlib/GameProfile;";
        jmethodID method = env->GetMethodID(entityClass, "method_7334", descriptor);
        Clear(env);
        if (!method) {
            method = env->GetMethodID(entityClass, "getGameProfile", descriptor);
            Clear(env);
        }
        if (!method) {
            method = env->GetMethodID(entityClass, "gI", descriptor);
            Clear(env);
        }

        env->DeleteLocalRef(entityClass);
        return method;
    }

    inline jobject FindGameProfileField(JNIEnv* env, jobject entity) {
        if (!env || !entity) {
            return nullptr;
        }

        jclass entityClass = env->GetObjectClass(entity);
        if (!entityClass) {
            Clear(env);
            return nullptr;
        }

        constexpr const char* descriptor = "Lcom/mojang/authlib/GameProfile;";
        jfieldID field = env->GetFieldID(entityClass, "field_7507", descriptor);
        Clear(env);
        if (!field) {
            field = env->GetFieldID(entityClass, "gameProfile", descriptor);
            Clear(env);
        }

        jobject profile = nullptr;
        if (field) {
            profile = env->GetObjectField(entity, field);
            if (env->ExceptionCheck()) {
                profile = nullptr;
                Clear(env);
            }
        }

        env->DeleteLocalRef(entityClass);
        return profile;
    }

    inline jmethodID FindGameProfileNameMethod(JNIEnv* env, jobject profile) {
        if (!env || !profile) {
            return nullptr;
        }

        jclass profileClass = env->GetObjectClass(profile);
        if (!profileClass) {
            Clear(env);
            return nullptr;
        }

        jmethodID method = env->GetMethodID(profileClass, "name", "()Ljava/lang/String;");
        Clear(env);
        if (!method) {
            method = env->GetMethodID(profileClass, "getName", "()Ljava/lang/String;");
            Clear(env);
        }
        env->DeleteLocalRef(profileClass);
        return method;
    }

    inline std::string GetPlayerName(JNIEnv* env, jobject entity) {
        if (!env || !entity) {
            return "Player";
        }

        jobject profile = nullptr;
        if (SDK::Classes.PlayerEntity_getGameProfile) {
            profile = env->CallObjectMethod(entity, SDK::Classes.PlayerEntity_getGameProfile);
            if (env->ExceptionCheck()) {
                profile = nullptr;
                Clear(env);
            }
        }
        if (!profile) {
            if (jmethodID runtimeMethod = FindGameProfileMethod(env, entity)) {
                profile = env->CallObjectMethod(entity, runtimeMethod);
                if (env->ExceptionCheck()) {
                    profile = nullptr;
                    Clear(env);
                }
            }
        }
        if (!profile) {
            profile = FindGameProfileField(env, entity);
        }
        if (!profile) {
            return "Player";
        }

        jmethodID nameMethod = SDK::Classes.GameProfile_getName ? SDK::Classes.GameProfile_getName : FindGameProfileNameMethod(env, profile);
        if (!nameMethod) {
            env->DeleteLocalRef(profile);
            return "Player";
        }

        jstring nameString = static_cast<jstring>(env->CallObjectMethod(profile, nameMethod));
        env->DeleteLocalRef(profile);
        if (env->ExceptionCheck()) {
            Clear(env);
            return "Player";
        }
        if (!nameString) {
            Clear(env);
            return "Player";
        }

        const char* chars = env->GetStringUTFChars(nameString, nullptr);
        if (env->ExceptionCheck()) {
            env->DeleteLocalRef(nameString);
            Clear(env);
            return "Player";
        }
        std::string name = chars ? chars : "Player";
        if (chars) {
            env->ReleaseStringUTFChars(nameString, chars);
        }
        env->DeleteLocalRef(nameString);
        return name.empty() ? "Player" : CleanProfileName(name);
    }

    inline bool ReadEntitySnapshot(JNIEnv* env, jobject entity, jobject localPlayer, double px, double py, double pz, EntitySnapshot& out) {
        if (!IsLivingTarget(env, entity, localPlayer)) {
            return false;
        }

        out.player = IsInstance(env, entity, SDK::Classes.PlayerEntity) || IsInstance(env, entity, SDK::Classes.RemotePlayerEntity);
        out.living = IsInstance(env, entity, SDK::Classes.LivingEntity);

        if (out.player && SDK::Classes.PlayerEntity_isAlive) {
            const jboolean alive = env->CallBooleanMethod(entity, SDK::Classes.PlayerEntity_isAlive);
            if (env->ExceptionCheck()) {
                Clear(env);
            } else if (!alive) {
                return false;
            }
        }

        if (!ReadPosition(env, entity, out.x, out.y, out.z)) {
            return false;
        }

        if (SDK::Classes.Entity_getId) {
            out.id = env->CallIntMethod(entity, SDK::Classes.Entity_getId);
            Clear(env);
        }

        out.prevX = SDK::Classes.Entity_prevX ? env->GetDoubleField(entity, SDK::Classes.Entity_prevX) : out.x;
        out.prevY = SDK::Classes.Entity_prevY ? env->GetDoubleField(entity, SDK::Classes.Entity_prevY) : out.y;
        out.prevZ = SDK::Classes.Entity_prevZ ? env->GetDoubleField(entity, SDK::Classes.Entity_prevZ) : out.z;
        Clear(env);

        if (SDK::Classes.Entity_getBoundingBox) {
            jobject box = env->CallObjectMethod(entity, SDK::Classes.Entity_getBoundingBox);
            if (box && !env->ExceptionCheck()) {
                const double minX = SDK::Classes.Box_minX ? env->GetDoubleField(box, SDK::Classes.Box_minX) : out.x - 0.3;
                const double minY = SDK::Classes.Box_minY ? env->GetDoubleField(box, SDK::Classes.Box_minY) : out.y;
                const double minZ = SDK::Classes.Box_minZ ? env->GetDoubleField(box, SDK::Classes.Box_minZ) : out.z - 0.3;
                const double maxX = SDK::Classes.Box_maxX ? env->GetDoubleField(box, SDK::Classes.Box_maxX) : out.x + 0.3;
                const double maxY = SDK::Classes.Box_maxY ? env->GetDoubleField(box, SDK::Classes.Box_maxY) : out.y + 1.8;
                const double maxZ = SDK::Classes.Box_maxZ ? env->GetDoubleField(box, SDK::Classes.Box_maxZ) : out.z + 0.3;
                out.width = static_cast<float>(std::clamp(std::max(maxX - minX, maxZ - minZ), 0.3, 3.0));
                out.height = static_cast<float>(std::clamp(maxY - minY, 0.6, 3.0));
                env->DeleteLocalRef(box);
            }
            Clear(env);
        } else if (SDK::Classes.Entity_getWidth) {
            out.width = std::clamp(env->CallFloatMethod(entity, SDK::Classes.Entity_getWidth), 0.3f, 3.0f);
            Clear(env);
        }

        if (out.living && SDK::Classes.LivingEntity_getHealth) {
            out.health = env->CallFloatMethod(entity, SDK::Classes.LivingEntity_getHealth);
            if (env->ExceptionCheck()) {
                out.health = 20.f;
                Clear(env);
            }
        }

        out.name = out.player ? GetPlayerName(env, entity) : "Entity";

        const double dx = out.x - px;
        const double dy = out.y - py;
        const double dz = out.z - pz;
        out.distance = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
        return out.distance >= 1.0f && out.distance <= 160.0f;
    }

    inline jobject GetEntityIterable(JNIEnv* env, jobject world) {
        if (!env || !world) {
            return nullptr;
        }

        jobject iterable = nullptr;
        if (SDK::Classes.ClientWorld_getEntities) {
            iterable = env->CallObjectMethod(world, SDK::Classes.ClientWorld_getEntities);
        }
        if (!iterable && SDK::Classes.ClientWorld_getPlayers) {
            Clear(env);
            iterable = env->CallObjectMethod(world, SDK::Classes.ClientWorld_getPlayers);
        }
        if (!iterable) {
            Clear(env);
        }
        return iterable;
    }

    inline bool ReadCamera(CameraSnapshot& cameraOut) {
        JNIEnv* env = JNIBridge::GetEnv();
        jobject mc = GetMinecraft(env);
        if (!env || !mc) {
            return false;
        }

        if (!SDK::Classes.MinecraftClient_gameRenderer || !SDK::Classes.GameRenderer_getCamera) {
            env->DeleteLocalRef(mc);
            return false;
        }

        jobject renderer = env->GetObjectField(mc, SDK::Classes.MinecraftClient_gameRenderer);
        if (!renderer) {
            Clear(env);
            env->DeleteLocalRef(mc);
            return false;
        }

        jobject camera = env->CallObjectMethod(renderer, SDK::Classes.GameRenderer_getCamera);
        if (!camera) {
            Clear(env);
            env->DeleteLocalRef(renderer);
            env->DeleteLocalRef(mc);
            return false;
        }

        jobject pos = SDK::Classes.Camera_getPos ? env->CallObjectMethod(camera, SDK::Classes.Camera_getPos) : nullptr;
        if (pos) {
            cameraOut.x = SDK::Classes.Vec3d_getX ? env->CallDoubleMethod(pos, SDK::Classes.Vec3d_getX) : 0.0;
            cameraOut.y = SDK::Classes.Vec3d_getY ? env->CallDoubleMethod(pos, SDK::Classes.Vec3d_getY) : 0.0;
            cameraOut.z = SDK::Classes.Vec3d_getZ ? env->CallDoubleMethod(pos, SDK::Classes.Vec3d_getZ) : 0.0;
            env->DeleteLocalRef(pos);
        }
        Clear(env);

        cameraOut.pitch = SDK::Classes.Camera_getPitch ? env->CallFloatMethod(camera, SDK::Classes.Camera_getPitch) : 0.0f;
        cameraOut.yaw = SDK::Classes.Camera_getYaw ? env->CallFloatMethod(camera, SDK::Classes.Camera_getYaw) : 0.0f;
        Clear(env);

        if (SDK::Classes.GameRenderer_getFov) {
            cameraOut.fov = env->CallFloatMethod(renderer, SDK::Classes.GameRenderer_getFov, camera, 1.0f, JNI_TRUE);
            if (env->ExceptionCheck() || cameraOut.fov < 30.f || cameraOut.fov > 140.f) {
                cameraOut.fov = 70.f;
                Clear(env);
            }
        }

        env->DeleteLocalRef(camera);
        env->DeleteLocalRef(renderer);
        env->DeleteLocalRef(mc);
        return true;
    }

    inline bool WorldToScreen(const CameraSnapshot& camera, double x, double y, double z, ImVec2 display, ImVec2& out) {
        const float dx = static_cast<float>(x - camera.x);
        const float dy = static_cast<float>(y - camera.y);
        const float dz = static_cast<float>(z - camera.z);

        const float yaw = camera.yaw * static_cast<float>(kPi / 180.0);
        const float pitch = camera.pitch * static_cast<float>(kPi / 180.0);
        const float cosYaw = std::cos(yaw);
        const float sinYaw = std::sin(yaw);

        const float xzX = dx * cosYaw + dz * sinYaw;
        const float xzZ = -dx * sinYaw + dz * cosYaw;

        const float cosPitch = std::cos(-pitch);
        const float sinPitch = std::sin(-pitch);
        const float yCam = dy * cosPitch - xzZ * sinPitch;
        const float zCam = dy * sinPitch + xzZ * cosPitch;
        const float xCam = xzX;

        if (zCam <= 0.01f || display.x <= 1.f || display.y <= 1.f) {
            return false;
        }

        const float aspect = display.x / display.y;
        const float tanHalf = std::tan((camera.fov * 0.5f) * static_cast<float>(kPi / 180.0));
        if (tanHalf <= 0.001f) {
            return false;
        }

        const float xNdc = (xCam / (zCam * tanHalf)) / aspect;
        const float yNdc = yCam / (zCam * tanHalf);

        out.x = display.x - ((xNdc + 1.0f) * 0.5f * display.x);
        out.y = (1.0f - yNdc) * 0.5f * display.y;

        return std::isfinite(out.x) && std::isfinite(out.y) &&
            out.x > -display.x && out.x < display.x * 2.f &&
            out.y > -display.y && out.y < display.y * 2.f;
    }
}
