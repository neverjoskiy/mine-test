#include "feature_runtime.h"
#include "auto_respawn.h"
#include "mcf.h"
#include "discord_rpc.h"
#include "hitbox.h"
#include "item_scroller.h"
#include "name_protect.h"
#include "world_tweaks.h"
#include "no_render.h"
#include "gui_move.h"
#include "esp.h"
#include "hud.h"
#include "shift_tap.h"
#include "storage_esp.h"
#include "triggerbot.h"
#include "minecraft_feature_utils.h"

namespace Features::Runtime {
    void Tick() {
        JNIEnv* env = JNIBridge::GetEnv();
        if (!env || !SDK::Classes.MinecraftClient_getInstance) {
            return;
        }

        jobject mc = MC::GetMinecraft(env);
        if (!mc) {
            return;
        }

        jobject player = MC::GetPlayer(env, mc);
        jobject world = MC::GetWorld(env, mc);

        WorldTweaks::Tick(env, mc, world);
        NoRender::Tick(env, mc, player, world);
        GuiMove::Tick(env, mc);
        ShiftTap::Tick(env, mc);

        if (player) {
            AutoRespawn::Tick(env, player);
            DiscordRPC::Tick(env, mc, player);
            ItemScroller::Tick(env, mc, player);
            MCF::Tick(env, mc, player);
            TriggerBot::Tick(env, mc, player);
        }

        HUD::Tick(env, mc, player);

        if (player && world) {
            HitBox::Tick(env, player, world);
            ESP::Tick(env, player, world);
            StorageESP::Tick(env, player, world);
        } else {
            HitBox::Clear();
            ESP::Clear();
            StorageESP::Clear();
        }

        if (player) {
            NameProtect::Tick(env, mc, player, world);
        }

        if (world) {
            env->DeleteLocalRef(world);
        }
        if (player) {
            env->DeleteLocalRef(player);
        }
        env->DeleteLocalRef(mc);
    }
}
