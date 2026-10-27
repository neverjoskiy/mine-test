#include "mcf.h"
#include "friends.h"
#include "inventory_utils.h"
#include "minecraft_feature_utils.h"
#include "name_protect.h"

#include <atomic>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <windows.h>

namespace {
    std::atomic_bool gEnabled{ false };
    unsigned long long gLastClickMs = 0;

    bool IsClose(JNIEnv* env, jobject player, jobject entity) {
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
        return std::sqrt(dx * dx + dy * dy + dz * dz) <= 6.0;
    }

    bool LooksLikeRealName(std::string name) {
        name.erase(std::remove_if(name.begin(), name.end(), [](unsigned char ch) {
            return std::isspace(ch) != 0;
        }), name.end());
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
            return static_cast<char>(std::tolower(ch));
        });
        return !name.empty() && name != "player" && name != "entity" && name != "lumaware.ru";
    }
}

namespace Features::MCF {
    void SetConfig(bool enabled) {
        gEnabled = enabled;
    }

    void Tick(JNIEnv* env, jobject mc, jobject player) {
        if (!gEnabled.load() || !env || !mc || !player) {
            return;
        }

        const unsigned long long now = GetTickCount64();
        if (now - gLastClickMs < 250) {
            return;
        }

        jobject target = Inventory::GetCrosshairEntity(env, mc);
        if (!target) {
            return;
        }

        const bool playerEntity = MC::IsInstance(env, target, SDK::Classes.PlayerEntity) ||
            MC::IsInstance(env, target, SDK::Classes.RemotePlayerEntity);
        if (playerEntity && IsClose(env, player, target)) {
            const int id = Inventory::GetEntityId(env, target);
            std::string name = MC::GetPlayerName(env, target);
            name = NameProtect::OriginalNameForId(id, name);
            if (LooksLikeRealName(name)) {
                Friends::Toggle(name);
                gLastClickMs = now;
            }
        }

        env->DeleteLocalRef(target);
    }
}
