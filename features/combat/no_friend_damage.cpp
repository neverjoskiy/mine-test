#include "no_friend_damage.h"
#include "friends.h"
#include "inventory_utils.h"
#include "minecraft_feature_utils.h"
#include "name_protect.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace {
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

namespace Features::NoFriendDamage {
    bool IsFriendTarget(JNIEnv* env, jobject entity) {
        if (!env || !entity) {
            return false;
        }

        const bool playerEntity = MC::IsInstance(env, entity, SDK::Classes.PlayerEntity) ||
            MC::IsInstance(env, entity, SDK::Classes.RemotePlayerEntity);
        if (!playerEntity) {
            return false;
        }

        const int id = Inventory::GetEntityId(env, entity);
        std::string name = MC::GetPlayerName(env, entity);
        name = NameProtect::OriginalNameForId(id, name);
        if (!LooksLikeRealName(name)) {
            return false;
        }
        return Friends::IsFriend(name);
    }

    bool ShouldCancelCurrentAttack() {
        if (!Enabled.load() || !SDK::Classes.Ready) {
            return false;
        }

        JNIEnv* env = JNIBridge::GetEnv();
        if (!env) {
            return false;
        }

        jobject mc = MC::GetMinecraft(env);
        if (!mc) {
            return false;
        }

        jobject target = Inventory::GetCrosshairEntity(env, mc);
        const bool cancel = target && IsFriendTarget(env, target);
        if (target) {
            env->DeleteLocalRef(target);
        }
        env->DeleteLocalRef(mc);
        return cancel;
    }
}
