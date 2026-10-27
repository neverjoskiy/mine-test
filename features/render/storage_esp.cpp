#include "storage_esp.h"
#include "headers/includes.h"
#include "minecraft_feature_utils.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>
#include <windows.h>

namespace {
    enum StorageType {
        Chest = 0,
        EnderChest,
        Shulker,
        Barrel,
        Furnace,
        Dispenser,
        Dropper,
        Hopper
    };

    struct StorageSnapshot {
        int type{};
        int x{};
        int y{};
        int z{};
        unsigned long long lastSeenMs{};
    };

    struct ScanState {
        bool active{ false };
        int centerX{};
        int centerY{};
        int centerZ{};
        int minX{};
        int maxX{};
        int minY{};
        int maxY{};
        int minZ{};
        int maxZ{};
        int x{};
        int y{};
        int z{};
    };

    std::atomic_bool gEnabled{ false };
    std::atomic<int> gMode{ 0 };
    std::mutex gMutex;
    Features::StorageESP::ColorSet gColors{
        Features::StorageESP::Color{ 1.0f, 0.54f, 0.12f, 0.80f },
        Features::StorageESP::Color{ 0.50f, 0.22f, 1.0f, 0.80f },
        Features::StorageESP::Color{ 0.92f, 0.36f, 1.0f, 0.80f },
        Features::StorageESP::Color{ 0.74f, 0.46f, 0.20f, 0.80f },
        Features::StorageESP::Color{ 0.58f, 0.62f, 0.68f, 0.80f },
        Features::StorageESP::Color{ 0.38f, 0.78f, 1.0f, 0.80f },
        Features::StorageESP::Color{ 0.38f, 1.0f, 0.70f, 0.80f },
        Features::StorageESP::Color{ 1.0f, 0.90f, 0.35f, 0.80f }
    };
    std::vector<StorageSnapshot> gSnapshots;
    ScanState gScan;
    unsigned long long gLastScanMs = 0;

    long long Key(int x, int y, int z) {
        const long long xx = static_cast<long long>(x) & 0x3ffffffLL;
        const long long yy = static_cast<long long>(y) & 0xfffLL;
        const long long zz = static_cast<long long>(z) & 0x3ffffffLL;
        return (xx << 38) | (zz << 12) | yy;
    }

    void ResetScan(int x, int y, int z) {
        constexpr int radius = 48;
        constexpr int down = 16;
        constexpr int up = 32;
        gScan.active = true;
        gScan.centerX = x;
        gScan.centerY = y;
        gScan.centerZ = z;
        gScan.minX = x - radius;
        gScan.maxX = x + radius;
        gScan.minY = std::clamp(y - down, -64, 320);
        gScan.maxY = std::clamp(y + up, -64, 320);
        gScan.minZ = z - radius;
        gScan.maxZ = z + radius;
        gScan.x = gScan.minX;
        gScan.y = gScan.minY;
        gScan.z = gScan.minZ;
    }

    bool AdvanceScan() {
        if (!gScan.active) {
            return false;
        }

        ++gScan.x;
        if (gScan.x <= gScan.maxX) {
            return true;
        }

        gScan.x = gScan.minX;
        ++gScan.z;
        if (gScan.z <= gScan.maxZ) {
            return true;
        }

        gScan.z = gScan.minZ;
        ++gScan.y;
        if (gScan.y <= gScan.maxY) {
            return true;
        }

        gScan.y = gScan.minY;
        return true;
    }

    bool Classify(JNIEnv* env, jobject blockEntity, int& type) {
        if (!env || !blockEntity) {
            return false;
        }

        if (Features::MC::IsInstance(env, blockEntity, SDK::Classes.EnderChestBlockEntity)) {
            type = EnderChest;
            return true;
        }
        if (Features::MC::IsInstance(env, blockEntity, SDK::Classes.ShulkerBoxBlockEntity)) {
            type = Shulker;
            return true;
        }
        if (Features::MC::IsInstance(env, blockEntity, SDK::Classes.BarrelBlockEntity)) {
            type = Barrel;
            return true;
        }
        if (Features::MC::IsInstance(env, blockEntity, SDK::Classes.DropperBlockEntity)) {
            type = Dropper;
            return true;
        }
        if (Features::MC::IsInstance(env, blockEntity, SDK::Classes.DispenserBlockEntity)) {
            type = Dispenser;
            return true;
        }
        if (Features::MC::IsInstance(env, blockEntity, SDK::Classes.HopperBlockEntity)) {
            type = Hopper;
            return true;
        }
        if (Features::MC::IsInstance(env, blockEntity, SDK::Classes.AbstractFurnaceBlockEntity) ||
            Features::MC::IsInstance(env, blockEntity, SDK::Classes.FurnaceBlockEntity) ||
            Features::MC::IsInstance(env, blockEntity, SDK::Classes.BlastFurnaceBlockEntity)) {
            type = Furnace;
            return true;
        }
        if (Features::MC::IsInstance(env, blockEntity, SDK::Classes.ChestBlockEntity)) {
            type = Chest;
            return true;
        }
        return false;
    }

    void UpsertSnapshot(int type, int x, int y, int z, unsigned long long now) {
        const long long key = Key(x, y, z);
        std::lock_guard<std::mutex> lock(gMutex);
        auto it = std::find_if(gSnapshots.begin(), gSnapshots.end(), [&](const StorageSnapshot& item) {
            return Key(item.x, item.y, item.z) == key;
        });
        if (it != gSnapshots.end()) {
            it->type = type;
            it->lastSeenMs = now;
            return;
        }
        gSnapshots.push_back(StorageSnapshot{ type, x, y, z, now });
    }

    void PurgeSnapshots(double px, double py, double pz, unsigned long long now) {
        const int mode = gMode.load();
        const bool cached = mode == 1;
        const unsigned long long maxAge = cached ? 60ull * 60ull * 1000ull : 2ull * 60ull * 1000ull;
        const double maxDistance = cached ? 512.0 : 160.0;
        std::lock_guard<std::mutex> lock(gMutex);
        gSnapshots.erase(std::remove_if(gSnapshots.begin(), gSnapshots.end(), [&](const StorageSnapshot& item) {
            const double dx = static_cast<double>(item.x) + 0.5 - px;
            const double dy = static_cast<double>(item.y) + 0.5 - py;
            const double dz = static_cast<double>(item.z) + 0.5 - pz;
            const double distSq = dx * dx + dy * dy + dz * dz;
            return now - item.lastSeenMs > maxAge || distSq > maxDistance * maxDistance;
        }), gSnapshots.end());
    }

    void ScanOne(JNIEnv* env, jobject world, unsigned long long now) {
        if (!env || !world || !SDK::Classes.BlockPos || !SDK::Classes.BlockPos_constructor || !SDK::Classes.World_getBlockEntity) {
            return;
        }

        jobject pos = env->NewObject(SDK::Classes.BlockPos, SDK::Classes.BlockPos_constructor, gScan.x, gScan.y, gScan.z);
        if (!pos) {
            Features::MC::Clear(env);
            return;
        }

        jobject blockEntity = env->CallObjectMethod(world, SDK::Classes.World_getBlockEntity, pos);
        env->DeleteLocalRef(pos);
        if (!blockEntity) {
            Features::MC::Clear(env);
            return;
        }

        int type = 0;
        if (Classify(env, blockEntity, type)) {
            UpsertSnapshot(type, gScan.x, gScan.y, gScan.z, now);
        }
        env->DeleteLocalRef(blockEntity);
        Features::MC::Clear(env);
    }

    ImU32 ColorU32(const Features::StorageESP::Color& color, float alphaScale) {
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

    void DrawBox(ImDrawList* drawList, const Features::MC::CameraSnapshot& camera, const StorageSnapshot& item, const Features::StorageESP::Color& color, ImVec2 display) {
        const double x0 = static_cast<double>(item.x) + 0.10;
        const double y0 = static_cast<double>(item.y) + 0.05;
        const double z0 = static_cast<double>(item.z) + 0.10;
        const double x1 = static_cast<double>(item.x) + 0.90;
        const double y1 = static_cast<double>(item.y) + 0.90;
        const double z1 = static_cast<double>(item.z) + 0.90;

        const double corners[8][3] = {
            { x0, y0, z0 }, { x1, y0, z0 }, { x1, y0, z1 }, { x0, y0, z1 },
            { x0, y1, z0 }, { x1, y1, z0 }, { x1, y1, z1 }, { x0, y1, z1 }
        };

        ImVec2 projected[8]{};
        for (int i = 0; i < 8; ++i) {
            if (!Features::MC::WorldToScreen(camera, corners[i][0], corners[i][1], corners[i][2], display, projected[i])) {
                return;
            }
        }

        const ImU32 fill = ColorU32(color, 0.80f);
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
    }
}

namespace Features::StorageESP {
    void SetConfig(bool enabled, int mode, const ColorSet& colors) {
        gEnabled = enabled;
        gMode = std::clamp(mode, 0, 1);

        std::lock_guard<std::mutex> lock(gMutex);
        gColors = colors;
    }

    void Clear() {
        std::lock_guard<std::mutex> lock(gMutex);
        gSnapshots.clear();
        gScan.active = false;
    }

    void Tick(JNIEnv* env, jobject player, jobject world) {
        if (!gEnabled.load() || !env || !player || !world) {
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

        const int cx = static_cast<int>(std::floor(px));
        const int cy = static_cast<int>(std::floor(py));
        const int cz = static_cast<int>(std::floor(pz));
        const int dx = cx - gScan.centerX;
        const int dy = cy - gScan.centerY;
        const int dz = cz - gScan.centerZ;
        constexpr int resetDistanceSq = 400;
        if (!gScan.active || dx * dx + dy * dy + dz * dz > resetDistanceSq) {
            ResetScan(cx, cy, cz);
        }

        const unsigned long long now = GetTickCount64();

        constexpr unsigned long long interval = 20ull;
        if (now - gLastScanMs >= interval) {
            gLastScanMs = now;
            constexpr int scansPerStep = 640;
            for (int i = 0; i < scansPerStep; ++i) {
                ScanOne(env, world, now);
                AdvanceScan();
            }
        }

        PurgeSnapshots(px, py, pz, now);
    }

    void RenderOverlay() {
        if (!gEnabled.load()) {
            return;
        }

        MC::CameraSnapshot camera;
        if (!MC::ReadCamera(camera)) {
            return;
        }

        std::vector<StorageSnapshot> snapshots;
        ColorSet colors;
        {
            std::lock_guard<std::mutex> lock(gMutex);
            snapshots = gSnapshots;
            colors = gColors;
        }

        if (snapshots.empty()) {
            return;
        }

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        for (const StorageSnapshot& item : snapshots) {
            if (item.type < 0 || item.type >= kStorageTypeCount) {
                continue;
            }
            DrawBox(drawList, camera, item, colors[static_cast<size_t>(item.type)], display);
        }
    }
}
