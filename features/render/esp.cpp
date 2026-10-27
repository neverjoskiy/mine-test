#include "esp.h"
#include "friends.h"
#include "headers/includes.h"
#include "minecraft_feature_utils.h"
#include "name_protect.h"

#include <algorithm>
#include <atomic>
#include <cfloat>
#include <cstdio>
#include <mutex>
#include <vector>

namespace {
    std::atomic_bool gEnabled{ false };
    std::atomic_bool gDrawBox{ true };
    std::atomic_bool gDrawName{ true };
    std::atomic_bool gDrawDistance{ true };
    std::atomic_bool gDrawHealth{ true };
    std::atomic<int> gBoxStyle{ 1 };
    Features::ESP::Color g3dBoxColor{ 0.45f, 0.74f, 1.0f, 0.45f };

    std::mutex gSnapshotsMutex;
    std::vector<Features::MC::EntitySnapshot> gSnapshots;

    void DrawBox(ImDrawList* drawList, ImVec2 min, ImVec2 max, ImU32 color, int style) {
        if (style == 0) {
            drawList->AddRect(ImVec2(min.x - 1.f, min.y - 1.f), ImVec2(max.x + 1.f, max.y + 1.f), IM_COL32(0, 0, 0, 215), 2.f, 0, 2.6f);
            drawList->AddRect(min, max, color, 2.f, 0, 1.5f);
            return;
        }

        const float w = max.x - min.x;
        const float h = max.y - min.y;
        const float corner = std::max(7.f, std::min(w, h) * 0.24f);
        const ImU32 shadow = IM_COL32(0, 0, 0, 225);

        auto line = [&](ImVec2 a, ImVec2 b, ImU32 col, float thick) {
            drawList->AddLine(a, b, col, thick);
        };

        for (int pass = 0; pass < 2; ++pass) {
            const ImU32 col = pass == 0 ? shadow : color;
            const float thick = pass == 0 ? 3.4f : 1.8f;
            line(min, ImVec2(min.x + corner, min.y), col, thick);
            line(min, ImVec2(min.x, min.y + corner), col, thick);
            line(ImVec2(max.x, min.y), ImVec2(max.x - corner, min.y), col, thick);
            line(ImVec2(max.x, min.y), ImVec2(max.x, min.y + corner), col, thick);
            line(ImVec2(min.x, max.y), ImVec2(min.x + corner, max.y), col, thick);
            line(ImVec2(min.x, max.y), ImVec2(min.x, max.y - corner), col, thick);
            line(max, ImVec2(max.x - corner, max.y), col, thick);
            line(max, ImVec2(max.x, max.y - corner), col, thick);
        }
    }

    void DrawCenteredText(ImDrawList* drawList, const char* text, ImVec2 pos, ImU32 color, float sizePx = 12.f) {
        ImFont* textFont = font->get(inter_medium_data, sizePx);
        const ImVec2 size = textFont->CalcTextSizeA(textFont->FontSize, FLT_MAX, 0.f, text);
        const ImVec2 p(pos.x - size.x * 0.5f, pos.y);
        drawList->AddText(textFont, textFont->FontSize, ImVec2(p.x + 1.f, p.y + 1.f), IM_COL32(0, 0, 0, 220), text);
        drawList->AddText(textFont, textFont->FontSize, p, color, text);
    }

    ImU32 ColorU32(const Features::ESP::Color& color, float alphaScale) {
        const int r = std::clamp(static_cast<int>(color[0] * 255.0f), 0, 255);
        const int g = std::clamp(static_cast<int>(color[1] * 255.0f), 0, 255);
        const int b = std::clamp(static_cast<int>(color[2] * 255.0f), 0, 255);
        const int a = std::clamp(static_cast<int>(color[3] * alphaScale * 255.0f), 0, 255);
        return IM_COL32(r, g, b, a);
    }

    void DrawFace(ImDrawList* drawList, const ImVec2 projected[8], const int indices[4], ImU32 fill, ImU32 outline) {
        ImVec2 points[4] = {
            projected[indices[0]],
            projected[indices[1]],
            projected[indices[2]],
            projected[indices[3]]
        };
        drawList->AddConvexPolyFilled(points, 4, fill);
        drawList->AddPolyline(points, 4, outline, ImDrawFlags_Closed, 1.2f);
    }

    bool Draw3DBox(ImDrawList* drawList, const Features::MC::CameraSnapshot& camera, const Features::MC::EntitySnapshot& entity, ImVec2 display, const Features::ESP::Color& color) {
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

        ImVec2 projected[8]{};
        for (int i = 0; i < 8; ++i) {
            if (!Features::MC::WorldToScreen(camera, corners[i][0], corners[i][1], corners[i][2], display, projected[i])) {
                return false;
            }
        }

        const ImU32 fill = ColorU32(color, 0.88f);
        const ImU32 outline = ColorU32(color, 1.0f);
        const int faces[6][4] = {
            { 0, 1, 2, 3 },
            { 4, 5, 6, 7 },
            { 0, 1, 5, 4 },
            { 1, 2, 6, 5 },
            { 2, 3, 7, 6 },
            { 3, 0, 4, 7 }
        };

        for (const auto& face : faces) {
            DrawFace(drawList, projected, face, fill, outline);
        }
        return true;
    }

    bool ProjectEntityBox(const Features::MC::CameraSnapshot& camera, const Features::MC::EntitySnapshot& entity, ImVec2 display, ImVec2& min, ImVec2& max) {
        const double interpX = entity.x;
        const double interpY = entity.y;
        const double interpZ = entity.z;
        const double half = std::max(0.25f, entity.width * 0.5f);

        const double minX = interpX - half;
        const double maxX = interpX + half;
        const double minY = interpY;
        const double maxY = interpY + entity.height;
        const double minZ = interpZ - half;
        const double maxZ = interpZ + half;

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

        if (valid < 2 || max.x <= min.x || max.y <= min.y) {
            return false;
        }

        const float boxWidth = max.x - min.x;
        const float boxHeight = max.y - min.y;
        return boxWidth >= 8.f && boxHeight >= 16.f && boxWidth <= display.x * 0.48f && boxHeight <= display.y * 0.85f;
    }

    void StabilizeSnapshot(Features::MC::EntitySnapshot& snapshot, const std::vector<Features::MC::EntitySnapshot>& previous) {
        if (snapshot.player) {
            snapshot.width = 0.62f;
            snapshot.height = 1.82f;
        } else {
            snapshot.width = std::clamp(snapshot.width, 0.45f, 1.15f);
            snapshot.height = std::clamp(snapshot.height, 0.75f, 2.35f);
        }

        if (snapshot.id == 0) {
            return;
        }

        auto it = std::find_if(previous.begin(), previous.end(), [&](const Features::MC::EntitySnapshot& old) {
            return old.id == snapshot.id && old.player == snapshot.player;
        });
        if (it == previous.end()) {
            return;
        }

        constexpr double posBlend = 0.45;
        constexpr float sizeBlend = 0.35f;
        snapshot.x = it->x + (snapshot.x - it->x) * posBlend;
        snapshot.y = it->y + (snapshot.y - it->y) * posBlend;
        snapshot.z = it->z + (snapshot.z - it->z) * posBlend;
        snapshot.width = it->width + (snapshot.width - it->width) * sizeBlend;
        snapshot.height = it->height + (snapshot.height - it->height) * sizeBlend;
    }
}

namespace Features::ESP {
    void SetConfig(bool enabled, const std::vector<bool>& elements, int boxStyle, const Color& box3dColor) {
        gEnabled = enabled;
        gDrawBox = elements.empty() || elements[0];
        gDrawName = elements.size() > 1 ? elements[1] : true;
        gDrawDistance = elements.size() > 2 ? elements[2] : true;
        gDrawHealth = elements.size() > 3 ? elements[3] : true;
        gBoxStyle = std::clamp(boxStyle, 0, 2);
        g3dBoxColor = box3dColor;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(gSnapshotsMutex);
        gSnapshots.clear();
    }

    void Tick(JNIEnv* env, jobject player, jobject world) {
        if (!gEnabled.load()) {
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
        fresh.reserve(64);
        std::vector<MC::EntitySnapshot> previous;
        {
            std::lock_guard<std::mutex> lock(gSnapshotsMutex);
            previous = gSnapshots;
        }

        jobject iterable = MC::GetEntityIterable(env, world);
        if (iterable) {
            MC::ForEachIterable(env, iterable, [&](jobject entity) {
                MC::EntitySnapshot snapshot;
                if (MC::ReadEntitySnapshot(env, entity, player, px, py, pz, snapshot)) {
                    StabilizeSnapshot(snapshot, previous);
                    fresh.push_back(std::move(snapshot));
                }
            });
            env->DeleteLocalRef(iterable);
        }
        MC::Clear(env);

        std::lock_guard<std::mutex> lock(gSnapshotsMutex);
        gSnapshots = std::move(fresh);
    }

    void RenderOverlay() {
        if (!gEnabled.load()) {
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

        for (const MC::EntitySnapshot& entity : snapshots) {
            ImVec2 min;
            ImVec2 max;
            if (!ProjectEntityBox(camera, entity, display, min, max)) {
                continue;
            }

            const std::string rawName = NameProtect::OriginalNameForId(entity.id, entity.name);
            const bool isFriend = entity.player && Friends::IsFriend(rawName);
            const ImU32 accent = isFriend ? IM_COL32(92, 230, 126, 248) : (entity.player ? IM_COL32(115, 190, 255, 248) : IM_COL32(255, 170, 90, 238));
            if (gDrawBox.load()) {
                const int style = gBoxStyle.load();
                if (style == 2) {
                    Draw3DBox(drawList, camera, entity, display, g3dBoxColor);
                } else {
                    DrawBox(drawList, min, max, accent, style);
                }
            }

            const float boxHeight = max.y - min.y;
            if (gDrawHealth.load()) {
                const float pct = std::clamp(entity.health / 20.f, 0.f, 1.f);
                const float barHeight = boxHeight * pct;
                const ImU32 healthColor = pct > 0.65f ? IM_COL32(80, 230, 120, 242) : (pct > 0.35f ? IM_COL32(255, 205, 80, 242) : IM_COL32(255, 80, 80, 242));
                drawList->AddRectFilled(ImVec2(min.x - 6, min.y), ImVec2(min.x - 3, max.y), IM_COL32(0, 0, 0, 165), 1.f);
                drawList->AddRectFilled(ImVec2(min.x - 5, max.y - barHeight), ImVec2(min.x - 4, max.y), healthColor, 1.f);
            }

            if (gDrawName.load()) {
                const std::string displayName = NameProtect::Apply(rawName, isFriend);
                DrawCenteredText(drawList, displayName.c_str(), ImVec2((min.x + max.x) * 0.5f, min.y - 16.f), isFriend ? IM_COL32(92, 230, 126, 248) : IM_COL32(255, 255, 255, 248), 12.f);
            }

            if (gDrawDistance.load()) {
                char distance[32]{};
                std::snprintf(distance, sizeof(distance), "%.0fm", entity.distance);
                DrawCenteredText(drawList, distance, ImVec2((min.x + max.x) * 0.5f, max.y + 4.f), IM_COL32(215, 228, 240, 235), 11.f);
            }
        }
    }
}
