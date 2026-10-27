#include "inventory_utils.h"
#include "minecraft_feature_utils.h"

#include <algorithm>
#include <cctype>

namespace {
    bool ContainsCaseInsensitive(std::string value, const char* token) {
        std::string needle = token ? token : "";
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        std::transform(needle.begin(), needle.end(), needle.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return !needle.empty() && value.find(needle) != std::string::npos;
    }
}

namespace Features::Inventory {
    std::string ReadJavaString(JNIEnv* env, jstring value, const char* fallback) {
        if (!env || !value) {
            return fallback ? fallback : "";
        }

        const char* chars = env->GetStringUTFChars(value, nullptr);
        if (env->ExceptionCheck()) {
            MC::Clear(env);
            env->DeleteLocalRef(value);
            return fallback ? fallback : "";
        }

        std::string out = chars ? chars : (fallback ? fallback : "");
        if (chars) {
            env->ReleaseStringUTFChars(value, chars);
        }
        env->DeleteLocalRef(value);
        return out.empty() && fallback ? fallback : out;
    }

    std::string ReadItemKey(JNIEnv* env, jobject stack) {
        if (!env || !stack) {
            return {};
        }

        if (SDK::Classes.ItemStack_isEmpty && env->CallBooleanMethod(stack, SDK::Classes.ItemStack_isEmpty) == JNI_TRUE) {
            MC::Clear(env);
            return {};
        }
        MC::Clear(env);

        if (!SDK::Classes.ItemStack_getItem || !SDK::Classes.Item_getTranslationKey) {
            return {};
        }

        jobject item = env->CallObjectMethod(stack, SDK::Classes.ItemStack_getItem);
        if (!item) {
            MC::Clear(env);
            return {};
        }

        jstring key = static_cast<jstring>(env->CallObjectMethod(item, SDK::Classes.Item_getTranslationKey));
        env->DeleteLocalRef(item);
        if (env->ExceptionCheck()) {
            MC::Clear(env);
            return {};
        }

        return ReadJavaString(env, key, "");
    }

    bool StackContains(JNIEnv* env, jobject stack, const char* token) {
        return ContainsCaseInsensitive(ReadItemKey(env, stack), token);
    }

    jobject GetInventory(JNIEnv* env, jobject player) {
        if (!env || !player || !SDK::Classes.PlayerEntity_getInventory) {
            return nullptr;
        }

        jobject inventory = env->CallObjectMethod(player, SDK::Classes.PlayerEntity_getInventory);
        if (!inventory) {
            MC::Clear(env);
        }
        return inventory;
    }

    jobject GetScreenHandler(JNIEnv* env, jobject player) {
        if (!env || !player || !SDK::Classes.PlayerEntity_currentScreenHandler) {
            return nullptr;
        }

        jobject handler = env->GetObjectField(player, SDK::Classes.PlayerEntity_currentScreenHandler);
        if (!handler) {
            MC::Clear(env);
        }
        return handler;
    }

    int GetSelectedSlot(JNIEnv* env, jobject inventory) {
        if (!env || !inventory) {
            return 0;
        }

        if (SDK::Classes.PlayerInventory_getSelectedSlot) {
            const int slot = env->CallIntMethod(inventory, SDK::Classes.PlayerInventory_getSelectedSlot);
            if (!env->ExceptionCheck()) {
                return std::clamp(slot, 0, 8);
            }
            MC::Clear(env);
        }

        if (SDK::Classes.PlayerInventory_selectedSlot) {
            const int slot = env->GetIntField(inventory, SDK::Classes.PlayerInventory_selectedSlot);
            MC::Clear(env);
            return std::clamp(slot, 0, 8);
        }

        return 0;
    }

    void SetSelectedSlot(JNIEnv* env, jobject inventory, int slot) {
        if (!env || !inventory) {
            return;
        }

        slot = std::clamp(slot, 0, 8);
        if (SDK::Classes.PlayerInventory_setSelectedSlot) {
            env->CallVoidMethod(inventory, SDK::Classes.PlayerInventory_setSelectedSlot, slot);
            MC::Clear(env);
            return;
        }

        if (SDK::Classes.PlayerInventory_selectedSlot) {
            env->SetIntField(inventory, SDK::Classes.PlayerInventory_selectedSlot, slot);
            MC::Clear(env);
        }
    }

    void SyncSelectedSlot(JNIEnv* env, jobject mc) {
        if (!env || !mc || !SDK::Classes.MinecraftClient_interactionManager || !SDK::Classes.ClientPlayerInteractionManager_syncSelectedSlot) {
            return;
        }

        jobject interaction = env->GetObjectField(mc, SDK::Classes.MinecraftClient_interactionManager);
        if (!interaction) {
            MC::Clear(env);
            return;
        }

        env->CallVoidMethod(interaction, SDK::Classes.ClientPlayerInteractionManager_syncSelectedSlot);
        env->DeleteLocalRef(interaction);
        MC::Clear(env);
    }

    jobject GetInventoryStack(JNIEnv* env, jobject inventory, int index) {
        if (!env || !inventory || index < 0) {
            return nullptr;
        }

        const jmethodID getStack = SDK::Classes.PlayerInventory_getStack ? SDK::Classes.PlayerInventory_getStack : SDK::Classes.Inventory_getStack;
        if (!getStack) {
            return nullptr;
        }

        jobject stack = env->CallObjectMethod(inventory, getStack, index);
        if (!stack) {
            MC::Clear(env);
        }
        return stack;
    }

    jobject GetScreenSlotStack(JNIEnv* env, jobject handler, int slotId) {
        if (!env || !handler || !SDK::Classes.ScreenHandler_getSlot || !SDK::Classes.Slot_getStack) {
            return nullptr;
        }

        jobject slot = env->CallObjectMethod(handler, SDK::Classes.ScreenHandler_getSlot, slotId);
        if (!slot) {
            MC::Clear(env);
            return nullptr;
        }

        jobject stack = env->CallObjectMethod(slot, SDK::Classes.Slot_getStack);
        env->DeleteLocalRef(slot);
        if (!stack) {
            MC::Clear(env);
        }
        return stack;
    }

    int InventoryIndexToScreenSlot(int index) {
        if (index >= 0 && index <= 8) {
            return 36 + index;
        }
        return index;
    }

    int FindHotbarItem(JNIEnv* env, jobject inventory, const char* token) {
        for (int i = 0; i < 9; ++i) {
            jobject stack = GetInventoryStack(env, inventory, i);
            const bool match = StackContains(env, stack, token);
            if (stack) {
                env->DeleteLocalRef(stack);
            }
            if (match) {
                return i;
            }
        }
        return -1;
    }

    int FindInventoryItem(JNIEnv* env, jobject inventory, const char* token) {
        for (int i = 0; i < 36; ++i) {
            jobject stack = GetInventoryStack(env, inventory, i);
            const bool match = StackContains(env, stack, token);
            if (stack) {
                env->DeleteLocalRef(stack);
            }
            if (match) {
                return i;
            }
        }
        return -1;
    }

    int FindChestplate(JNIEnv* env, jobject inventory) {
        for (int i = 0; i < 36; ++i) {
            jobject stack = GetInventoryStack(env, inventory, i);
            const std::string key = ReadItemKey(env, stack);
            if (stack) {
                env->DeleteLocalRef(stack);
            }
            if (ContainsCaseInsensitive(key, "chestplate")) {
                return i;
            }
        }
        return -1;
    }

    bool ClickSlot(JNIEnv* env, jobject mc, jobject player, int slotId, jobject actionType) {
        if (!env || !mc || !player || slotId < 0 || !actionType || !SDK::Classes.MinecraftClient_interactionManager ||
            !SDK::Classes.ClientPlayerInteractionManager_clickSlot || !SDK::Classes.ScreenHandler_syncId) {
            return false;
        }

        jobject handler = GetScreenHandler(env, player);
        if (!handler) {
            return false;
        }

        const int syncId = env->GetIntField(handler, SDK::Classes.ScreenHandler_syncId);
        env->DeleteLocalRef(handler);
        if (env->ExceptionCheck()) {
            MC::Clear(env);
            return false;
        }

        jobject interaction = env->GetObjectField(mc, SDK::Classes.MinecraftClient_interactionManager);
        if (!interaction) {
            MC::Clear(env);
            return false;
        }

        env->CallVoidMethod(interaction, SDK::Classes.ClientPlayerInteractionManager_clickSlot, syncId, slotId, 0, actionType, player);
        env->DeleteLocalRef(interaction);
        const bool ok = !env->ExceptionCheck();
        MC::Clear(env);
        return ok;
    }

    bool QuickMoveSlot(JNIEnv* env, jobject mc, jobject player, int slotId) {
        if (!env || slotId < 0 || !SDK::Classes.SlotActionType_QUICK_MOVE) {
            return false;
        }

        jobject quickMove = env->GetStaticObjectField(SDK::Classes.SlotActionType, SDK::Classes.SlotActionType_QUICK_MOVE);
        if (!quickMove) {
            MC::Clear(env);
            return false;
        }

        const bool ok = ClickSlot(env, mc, player, slotId, quickMove);
        env->DeleteLocalRef(quickMove);
        return ok;
    }

    bool SwapWithChestSlot(JNIEnv* env, jobject mc, jobject player, int sourceSlotId) {
        if (!env || sourceSlotId < 0 || !SDK::Classes.SlotActionType_PICKUP) {
            return false;
        }

        jobject pickup = env->GetStaticObjectField(SDK::Classes.SlotActionType, SDK::Classes.SlotActionType_PICKUP);
        if (!pickup) {
            MC::Clear(env);
            return false;
        }

        constexpr int chestArmorSlot = 6;
        const bool ok =
            ClickSlot(env, mc, player, sourceSlotId, pickup) &&
            ClickSlot(env, mc, player, chestArmorSlot, pickup) &&
            ClickSlot(env, mc, player, sourceSlotId, pickup);

        env->DeleteLocalRef(pickup);
        if (ok) {
            return true;
        }

        return QuickMoveSlot(env, mc, player, sourceSlotId);
    }

    void SwingMainHand(JNIEnv* env, jobject player) {
        if (!env || !player || !SDK::Classes.Hand || !SDK::Classes.Hand_MAIN_HAND || !SDK::Classes.LivingEntity_swingHand) {
            return;
        }

        jobject hand = env->GetStaticObjectField(SDK::Classes.Hand, SDK::Classes.Hand_MAIN_HAND);
        if (!hand) {
            MC::Clear(env);
            return;
        }

        env->CallVoidMethod(player, SDK::Classes.LivingEntity_swingHand, hand);
        MC::Clear(env);
        env->DeleteLocalRef(hand);
    }

    bool InteractMainHand(JNIEnv* env, jobject mc, jobject player) {
        if (!env || !mc || !player || !SDK::Classes.MinecraftClient_interactionManager ||
            !SDK::Classes.ClientPlayerInteractionManager_interactItem || !SDK::Classes.Hand_MAIN_HAND) {
            return false;
        }

        jobject interaction = env->GetObjectField(mc, SDK::Classes.MinecraftClient_interactionManager);
        jobject hand = env->GetStaticObjectField(SDK::Classes.Hand, SDK::Classes.Hand_MAIN_HAND);
        if (!interaction || !hand) {
            MC::Clear(env);
            if (interaction) env->DeleteLocalRef(interaction);
            if (hand) env->DeleteLocalRef(hand);
            return false;
        }

        jobject result = env->CallObjectMethod(interaction, SDK::Classes.ClientPlayerInteractionManager_interactItem, player, hand);
        if (result) {
            env->DeleteLocalRef(result);
        }
        env->DeleteLocalRef(hand);
        env->DeleteLocalRef(interaction);
        const bool ok = !env->ExceptionCheck();
        MC::Clear(env);
        return ok;
    }

    jobject GetCrosshairEntity(JNIEnv* env, jobject mc) {
        if (!env || !mc || !SDK::Classes.MinecraftClient_crosshairTarget || !SDK::Classes.EntityHitResult || !SDK::Classes.EntityHitResult_getEntity) {
            return nullptr;
        }

        jobject hit = env->GetObjectField(mc, SDK::Classes.MinecraftClient_crosshairTarget);
        if (!hit) {
            MC::Clear(env);
            return nullptr;
        }

        if (!MC::IsInstance(env, hit, SDK::Classes.EntityHitResult)) {
            env->DeleteLocalRef(hit);
            return nullptr;
        }

        jobject entity = env->CallObjectMethod(hit, SDK::Classes.EntityHitResult_getEntity);
        env->DeleteLocalRef(hit);
        if (!entity) {
            MC::Clear(env);
        }
        return entity;
    }

    int GetEntityId(JNIEnv* env, jobject entity) {
        if (!env || !entity || !SDK::Classes.Entity_getId) {
            return 0;
        }

        const int id = env->CallIntMethod(entity, SDK::Classes.Entity_getId);
        MC::Clear(env);
        return id;
    }
}
