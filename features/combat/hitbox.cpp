#include "hitbox.h"
#include "minecraft_feature_utils.h"

#include <algorithm>
#include <atomic>
#include <mutex>
#include <vector>
#include <imgui.h>

namespace {
    std::atomic_bool gEnabled{ false };
    std::atomic_bool gVisible{ false };
    std::atomic<float> gYExpand{ 0.3f };
    std::atomic<float> gZHalf{ 0.3f };

    std::mutex gSnapshotsMutex;
    std::vector<Features::MC::EntitySnapshot> gSnapshots;

    double ReadBaseHeight(JNIEnv* env, jobject entity, double fallbackY) {
        if (!SDK::Classes.Entity_getBoundingBox) {
            return 1.8;
        }

        jobject box = env->CallObjectMethod(entity, SDK::Classes.Entity_getBoundingBox);
        if (!box || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return 1.8;
        }

        const double minY = SDK::Classes.Box_minY ? env->GetDoubleField(box, SDK::Classes.Box_minY) : fallbackY;
        const double maxY = SDK::Classes.Box_maxY ? env->GetDoubleField(box, SDK::Classes.Box_maxY) : fallbackY + 1.8;
        env->DeleteLocalRef(box);
        if (env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return 1.8;
        }

        return std::clamp(maxY - minY, 0.6, 2.6);
    }

    void ApplyHitbox(JNIEnv* env, jobject entity) {
        if (!SDK::Classes.Entity_setBoundingBox || !SDK::Classes.Box || !SDK::Classes.Box_constructor) {
            return;
        }

        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        if (!Features::MC::ReadPosition(env, entity, x, y, z)) {
            return;
        }

        const double baseHeight = ReadBaseHeight(env, entity, y);
        const double half = std::clamp(static_cast<double>(gZHalf.load()), 0.3, 2.0);
        const double yExtra = std::clamp(static_cast<double>(gYExpand.load()), 0.0, 2.0);

        jobject newBox = env->NewObject(
            SDK::Classes.Box,
            SDK::Classes.Box_constructor,
            x - half,
            y,
            z - half,
            x + half,
            y + baseHeight + yExtra,
            z + half
        );
        if (!newBox) {
            Features::MC::Clear(env);
            return;
        }

        env->CallVoidMethod(entity, SDK::Classes.Entity_setBoundingBox, newBox);
        Features::MC::Clear(env);
        env->DeleteLocalRef(newBox);
    }

    bool ProjectEntityBox(const Features::MC::CameraSnapshot& camera, const Features::MC::EntitySnapshot& entity, ImVec2 display, ImVec2& min, ImVec2& max) {
        const double half = std::max(0.25f, entity.width * 0.5f);
        const double minX = entity.x - half;
        const double maxX = entity.x + half;
        const double minY = entity.y;
        const double maxY = entity.y + entity.height;
        const double minZ = entity.z - half;
        const double maxZ = entity.z + half;

        const double corners[8][3] = {
            { minX, minY, minZ }, { maxX, minY, minZ }, { maxX, minY, maxZ }, { minX, minY, maxZ },
            { minX, maxY, minZ }, { maxX, maxY, minZ }, { maxX, maxY, maxZ }, { minX, maxY, maxZ }
        };

        int valid = 0;
        min = ImVec2(999999.f, 999999.f);
        max = ImVec2(-999999.f, -999999.f);

        for (const auto& corner : corners) {
            ImVec2 point;
            if (!Features::MC::WorldToScreen(camera, corner[0], corner[1], corner[2], display, point)) {
                continue;
            }

            ++valid;
            min.x = std::min(min.x, point.x);
            min.y = std::min(min.y, point.y);
            max.x = std::max(max.x, point.x);
            max.y = std::max(max.y, point.y);
        }

        return valid >= 2 && max.x > min.x && max.y > min.y;
    }
}

namespace Features::HitBox {
    void SetConfig(bool enabled, float yExpand, float zExpand, bool visible) {
        gEnabled = enabled;
        gVisible = visible;
        gYExpand = yExpand;
        gZHalf = zExpand;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(gSnapshotsMutex);
        gSnapshots.clear();
    }

    void Tick(JNIEnv* env, jobject player, jobject world) {
        const bool enabled = gEnabled.load();
        const bool visible = gVisible.load();
        if (!enabled && !visible) {
            Clear();
            return;
        }

        double px = 0.0;
        double py = 0.0;
        double pz = 0.0;
        if (!MC::ReadPosition(env, player, px, py, pz)) {
            Clear();
            return;
        }

        std::vector<MC::EntitySnapshot> fresh;
        if (visible) {
            fresh.reserve(64);
        }

        jobject iterable = MC::GetEntityIterable(env, world);
        if (iterable) {
            MC::ForEachIterable(env, iterable, [&](jobject entity) {
                if (!MC::IsLivingTarget(env, entity, player)) {
                    return;
                }

                if (enabled) {
                    ApplyHitbox(env, entity);
                }

                if (visible) {
                    MC::EntitySnapshot snapshot;
                    if (MC::ReadEntitySnapshot(env, entity, player, px, py, pz, snapshot)) {
                        snapshot.width = std::max(snapshot.width, gZHalf.load() * 2.f);
                        snapshot.height = std::min(snapshot.height + gYExpand.load(), 4.0f);
                        fresh.push_back(std::move(snapshot));
                    }
                }
            });
            env->DeleteLocalRef(iterable);
        }
        MC::Clear(env);

        std::lock_guard<std::mutex> lock(gSnapshotsMutex);
        gSnapshots = std::move(fresh);
    }

    void RenderOverlay() {
        if (!gVisible.load()) {
            return;
        }

        MC::CameraSnapshot camera;
        if (!MC::ReadCamera(camera)) {
            return;
        }

        std::vector<MC::EntitySnapshot> snapshots;
        {
            std::lock_guard<std::mutex> lock(gSnapshotsMutex);
            snapshots = gSnapshots;
        }

        if (snapshots.empty()) {
            return;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const ImU32 color = IM_COL32(255, 255, 255, 125);

        for (const MC::EntitySnapshot& entity : snapshots) {
            ImVec2 min;
            ImVec2 max;
            if (!ProjectEntityBox(camera, entity, display, min, max)) {
                continue;
            }

            const float w = max.x - min.x;
            const float h = max.y - min.y;
            if (w < 8.f || h < 16.f || w > display.x * 0.55f || h > display.y * 0.9f) {
                continue;
            }

            drawList->AddRect(ImVec2(min.x - 1.f, min.y - 1.f), ImVec2(max.x + 1.f, max.y + 1.f), IM_COL32(0, 0, 0, 180), 1.f, 0, 2.f);
            drawList->AddRect(min, max, color, 1.f, 0, 1.1f);
        }
    }
}
