#include "hud.h"
#include "friends.h"
#include "headers/includes.h"
#include "minecraft_feature_utils.h"
#include "name_protect.h"

#include <algorithm>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace {
    struct TargetState {
        bool active{ false };
        std::string name{ "Player" };
        float health{ 20.f };
        float max_health{ 20.f };
        bool friend_target{ false };
    };

    struct PotionState {
        std::string name;
        int amplifier{ 0 };
        int seconds{ 0 };
    };

    std::mutex gHudMutex;
    TargetState gTarget;
    std::vector<PotionState> gPotions;
    bool gChatOpen{ false };

    std::string ReadJavaString(JNIEnv* env, jstring value, const char* fallback) {
        if (!env || !value) {
            return fallback;
        }

        const char* chars = env->GetStringUTFChars(value, nullptr);
        std::string out = chars ? chars : fallback;
        if (chars) {
            env->ReleaseStringUTFChars(value, chars);
        }
        env->DeleteLocalRef(value);
        return out.empty() ? fallback : out;
    }

    std::string PrettyEffectName(std::string key) {
        const size_t dot = key.find_last_of('.');
        if (dot != std::string::npos && dot + 1 < key.size()) {
            key = key.substr(dot + 1);
        }

        bool cap = true;
        for (char& ch : key) {
            if (ch == '_') {
                ch = ' ';
                cap = true;
                continue;
            }

            if (cap && ch >= 'a' && ch <= 'z') {
                ch = static_cast<char>(ch - 'a' + 'A');
            }
            cap = false;
        }

        return key.empty() ? "Effect" : key;
    }

    void SyncAccentFromSettings() {
        color_edit_t& style = cfg->get<color_edit_t>("Styles");
        static bool initialized{ false };
        if (!initialized) {
            const bool oldWhiteDefault = style.color[0] >= 0.99f && style.color[1] >= 0.99f && style.color[2] >= 0.99f;
            if (oldWhiteDefault) {
                style.color[0] = 103.f / 255.f;
                style.color[1] = 100.f / 255.f;
                style.color[2] = 1.f;
                style.color[3] = 1.f;
            }
            initialized = true;
        }

        clr->accent.Value.x = style.color[0];
        clr->accent.Value.y = style.color[1];
        clr->accent.Value.z = style.color[2];
        clr->accent.Value.w = style.color[3];

        clr->widgets.section_on = ImColor(
            std::clamp(style.color[0] * 0.35f + 0.18f, 0.f, 1.f),
            std::clamp(style.color[1] * 0.35f + 0.18f, 0.f, 1.f),
            std::clamp(style.color[2] * 0.35f + 0.18f, 0.f, 1.f),
            1.f
        );
        clr->widgets.widgets_active = ImColor(
            std::clamp(style.color[0] * 0.18f + 0.10f, 0.f, 1.f),
            std::clamp(style.color[1] * 0.18f + 0.10f, 0.f, 1.f),
            std::clamp(style.color[2] * 0.18f + 0.10f, 0.f, 1.f),
            1.f
        );
    }

    jobject GetCrosshairEntity(JNIEnv* env, jobject mc) {
        if (!env || !mc || !SDK::Classes.MinecraftClient_crosshairTarget || !SDK::Classes.EntityHitResult || !SDK::Classes.EntityHitResult_getEntity) {
            return nullptr;
        }

        jobject hit = env->GetObjectField(mc, SDK::Classes.MinecraftClient_crosshairTarget);
        if (!hit) {
            Features::MC::Clear(env);
            return nullptr;
        }

        if (!Features::MC::IsInstance(env, hit, SDK::Classes.EntityHitResult)) {
            env->DeleteLocalRef(hit);
            return nullptr;
        }

        jobject entity = env->CallObjectMethod(hit, SDK::Classes.EntityHitResult_getEntity);
        env->DeleteLocalRef(hit);
        if (!entity) {
            Features::MC::Clear(env);
        }
        return entity;
    }

    bool IsChatOpen(JNIEnv* env, jobject mc) {
        if (!env || !mc || !SDK::Classes.MinecraftClient_currentScreen || !SDK::Classes.ChatScreen) {
            return false;
        }

        jobject screen = env->GetObjectField(mc, SDK::Classes.MinecraftClient_currentScreen);
        if (!screen) {
            Features::MC::Clear(env);
            return false;
        }

        const bool result = Features::MC::IsInstance(env, screen, SDK::Classes.ChatScreen);
        env->DeleteLocalRef(screen);
        return result;
    }

    TargetState ReadTarget(JNIEnv* env, jobject mc, jobject player) {
        TargetState state;
        jobject entity = GetCrosshairEntity(env, mc);
        if (!entity || !player) {
            if (entity) {
                env->DeleteLocalRef(entity);
            }
            return state;
        }

        double px = 0.0;
        double py = 0.0;
        double pz = 0.0;
        Features::MC::ReadPosition(env, player, px, py, pz);

        Features::MC::EntitySnapshot snapshot;
        if (Features::MC::ReadEntitySnapshot(env, entity, player, px, py, pz, snapshot)) {
            state.active = true;
            state.name = Features::NameProtect::OriginalNameForId(snapshot.id, snapshot.name);
            state.friend_target = Features::Friends::IsFriend(state.name);
            state.health = std::clamp(snapshot.health, 0.f, 1000.f);
            state.max_health = 20.f;
        }

        if (state.active && SDK::Classes.LivingEntity_getMaxHealth && Features::MC::IsInstance(env, entity, SDK::Classes.LivingEntity)) {
            state.max_health = env->CallFloatMethod(entity, SDK::Classes.LivingEntity_getMaxHealth);
            if (env->ExceptionCheck() || state.max_health <= 0.f) {
                state.max_health = 20.f;
                Features::MC::Clear(env);
            }
        }

        env->DeleteLocalRef(entity);
        return state;
    }

    std::vector<PotionState> ReadPotions(JNIEnv* env, jobject player) {
        std::vector<PotionState> result;
        if (!env || !player || !SDK::Classes.LivingEntity_getStatusEffects || !SDK::Classes.StatusEffectInstance_getDuration) {
            return result;
        }

        jobject collection = env->CallObjectMethod(player, SDK::Classes.LivingEntity_getStatusEffects);
        if (!collection) {
            Features::MC::Clear(env);
            return result;
        }

        Features::MC::ForEachIterable(env, collection, [&](jobject effect) {
            PotionState potion;
            if (SDK::Classes.StatusEffectInstance_getTranslationKey) {
                potion.name = PrettyEffectName(ReadJavaString(env, static_cast<jstring>(env->CallObjectMethod(effect, SDK::Classes.StatusEffectInstance_getTranslationKey)), "Effect"));
                Features::MC::Clear(env);
            } else {
                potion.name = "Effect";
            }

            if (SDK::Classes.StatusEffectInstance_getAmplifier) {
                potion.amplifier = env->CallIntMethod(effect, SDK::Classes.StatusEffectInstance_getAmplifier);
                Features::MC::Clear(env);
            }

            const int ticks = env->CallIntMethod(effect, SDK::Classes.StatusEffectInstance_getDuration);
            Features::MC::Clear(env);
            potion.seconds = std::max(0, ticks / 20);
            result.push_back(std::move(potion));
        });

        env->DeleteLocalRef(collection);
        return result;
    }

    void drag_component(const ImRect& rect, slider_float_t& x, slider_float_t& y, int id, bool chatOpen) {
        static int dragging = 0;
        const bool canDrag = chatOpen || var->gui.menu_opened || var->gui.menu_alpha > 0.03f;

        if (canDrag && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && rect.Contains(ImGui::GetIO().MousePos)) {
            dragging = id;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            dragging = 0;
        }
        if (dragging != id) {
            return;
        }

        const ImVec2 delta = ImGui::GetIO().MouseDelta;
        x.callback = std::clamp(x.callback + delta.x, x.min, x.max);
        y.callback = std::clamp(y.callback + delta.y, y.min, y.max);
    }

    void draw_target_hud(ImDrawList* list, const TargetState& target, bool chatOpen, float alpha) {
        slider_float_t& x = cfg->get<slider_float_t>("TargetHUD X Position");
        slider_float_t& y = cfg->get<slider_float_t>("TargetHUD Y Position");
        const ImVec2 display = ImGui::GetIO().DisplaySize;
        const ImVec2 size = SCALE(182, 50);
        ImVec2 pos = ImVec2(x.callback, y.callback);
        if (pos.x <= 1.f && pos.y <= 1.f) {
            pos = ImVec2(display.x * 0.5f - size.x * 0.5f, display.y * 0.5f + SCALE(36));
        }

        const ImRect rect{ pos, pos + size };
        drag_component(rect, x, y, 1, chatOpen);

        const float maxHealth = std::max(1.f, target.max_health);
        const float pct = std::clamp(target.health / maxHealth, 0.f, 1.f);
        const int strongAlpha = std::clamp(static_cast<int>(245.f * alpha), 0, 255);
        const int softAlpha = std::clamp(static_cast<int>(90.f * alpha), 0, 255);
        const ImU32 healthColor = pct > 0.6f ? IM_COL32(84, 232, 138, strongAlpha) : (pct > 0.32f ? IM_COL32(255, 203, 86, strongAlpha) : IM_COL32(255, 90, 90, strongAlpha));

        draw->rect_filled(list, rect.Min, rect.Max, draw->get_clr(clr->window.child, 0.92f * alpha), SCALE(8));
        draw->rect_filled(list, rect.Min + SCALE(12, 34), rect.Max - SCALE(12, 10), IM_COL32(0, 0, 0, softAlpha), SCALE(4));
        draw->rect_filled(list, rect.Min + SCALE(12, 34), ImVec2(rect.Min.x + SCALE(12) + (size.x - SCALE(24)) * pct, rect.Max.y - SCALE(10)), healthColor, SCALE(4));

        const std::string displayName = Features::NameProtect::Apply(target.name, target.friend_target);
        draw->text_clipped(list, font->get(inter_medium_data, 14), rect.Min + SCALE(12, 7), rect.Max - SCALE(12, 0), draw->get_clr(target.friend_target ? ImVec4(92.f / 255.f, 230.f / 255.f, 126.f / 255.f, 1.f) : clr->widgets.text.Value, alpha), displayName.c_str(), nullptr, nullptr, ImVec2(0.f, 0.f));

        char hp[32]{};
        std::snprintf(hp, sizeof(hp), "%.0f HP", target.health);
        draw->text_clipped(list, font->get(inter_medium_data, 12), rect.Min + SCALE(12, 7), rect.Max - SCALE(12, 0), draw->get_clr(clr->widgets.text_inactive, alpha), hp, nullptr, nullptr, ImVec2(1.f, 0.f));
    }

    void draw_potions(ImDrawList* list, const std::vector<PotionState>& potions, bool chatOpen, float alpha) {
        if (potions.empty()) {
            return;
        }

        slider_float_t& x = cfg->get<slider_float_t>("Potions X Position");
        slider_float_t& y = cfg->get<slider_float_t>("Potions Y Position");
        float width = SCALE(154);
        for (const PotionState& potion : potions) {
            width = ImMax(width, gui->text_size(font->get(inter_medium_data, 12), potion.name.c_str()).x + SCALE(76));
        }

        const float height = SCALE(30 + 22 * static_cast<float>(potions.size()));
        const ImVec2 pos{ x.callback, y.callback };
        const ImRect rect{ pos, pos + ImVec2(width, height) };
        drag_component(rect, x, y, 2, chatOpen);

        draw->rect_filled(list, rect.Min, rect.Max, draw->get_clr(clr->window.child, 0.92f * alpha), SCALE(8));
        draw->text_clipped(list, font->get(inter_medium_data, 13), rect.Min + SCALE(12, 0), rect.Min + ImVec2(width, SCALE(30)), draw->get_clr(clr->widgets.text, alpha), "Potions", nullptr, nullptr, ImVec2(0.f, 0.5f));

        float rowY = SCALE(30);
        for (const PotionState& potion : potions) {
            char right[32]{};
            const int minutes = potion.seconds / 60;
            const int seconds = potion.seconds % 60;
            std::snprintf(right, sizeof(right), "%d %d:%02d", potion.amplifier + 1, minutes, seconds);

            draw->text_clipped(list, font->get(inter_medium_data, 12), rect.Min + ImVec2(SCALE(12), rowY), rect.Min + ImVec2(width - SCALE(12), rowY + SCALE(20)), draw->get_clr(clr->widgets.text_inactive, alpha), potion.name.c_str(), nullptr, nullptr, ImVec2(0.f, 0.5f));
            draw->text_clipped(list, font->get(inter_medium_data, 12), rect.Min + ImVec2(SCALE(12), rowY), rect.Min + ImVec2(width - SCALE(12), rowY + SCALE(20)), draw->get_clr(clr->accent, alpha), right, nullptr, nullptr, ImVec2(1.f, 0.5f));
            rowY += SCALE(22);
        }
    }

}

namespace Features::HUD {
    void Tick(JNIEnv* env, jobject mc, jobject player) {
        if (!env || !mc) {
            return;
        }

        TargetState target = player ? ReadTarget(env, mc, player) : TargetState{};
        std::vector<PotionState> potions = player ? ReadPotions(env, player) : std::vector<PotionState>{};
        const bool chatOpen = IsChatOpen(env, mc);

        std::lock_guard<std::mutex> lock(gHudMutex);
        gTarget = std::move(target);
        gPotions = std::move(potions);
        gChatOpen = chatOpen;
    }

    void Render() {
        SyncAccentFromSettings();

        static TargetState displayedTarget;
        static double lastTargetSeen = 0.0;
        static float targetAlpha = 0.f;
        static std::vector<PotionState> displayedPotions;
        static double lastPotionsSeen = 0.0;
        static float potionsAlpha = 0.f;

        TargetState target;
        std::vector<PotionState> potions;
        bool chatOpen = false;
        {
            std::lock_guard<std::mutex> lock(gHudMutex);
            target = gTarget;
            potions = gPotions;
            chatOpen = gChatOpen;
        }

        ImDrawList* list = ImGui::GetWindowDrawList();
        if (cfg->get<checkbox_t>("TargetHUD").callback) {
            const double now = ImGui::GetTime();
            if (target.active) {
                displayedTarget = target;
                lastTargetSeen = now;
            }

            const bool keepVisible = displayedTarget.active && (target.active || now - lastTargetSeen < 2.0);
            gui->easing(targetAlpha, keepVisible ? 1.f : 0.f, 7.f, static_easing);

            if (targetAlpha > 0.01f && displayedTarget.active) {
                draw_target_hud(list, displayedTarget, chatOpen, targetAlpha);
            } else if (!target.active && now - lastTargetSeen >= 2.0) {
                displayedTarget.active = false;
            }
        } else {
            targetAlpha = 0.f;
            displayedTarget.active = false;
        }
        if (cfg->get<checkbox_t>("Potions").callback) {
            const double now = ImGui::GetTime();
            if (!potions.empty()) {
                displayedPotions = potions;
                lastPotionsSeen = now;
            }

            const bool keepVisible = !displayedPotions.empty() && (!potions.empty() || now - lastPotionsSeen < 2.0);
            gui->easing(potionsAlpha, keepVisible ? 1.f : 0.f, 7.f, static_easing);

            if (potionsAlpha > 0.01f && !displayedPotions.empty()) {
                draw_potions(list, displayedPotions, chatOpen, potionsAlpha);
            } else if (potions.empty() && now - lastPotionsSeen >= 2.0) {
                displayedPotions.clear();
            }
        } else {
            potionsAlpha = 0.f;
            displayedPotions.clear();
        }
    }
}
