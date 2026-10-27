#pragma once

#include <jni.h>
#include <string>

namespace Features::Inventory {
    std::string ReadJavaString(JNIEnv* env, jstring value, const char* fallback = "");
    std::string ReadItemKey(JNIEnv* env, jobject stack);
    bool StackContains(JNIEnv* env, jobject stack, const char* token);

    jobject GetInventory(JNIEnv* env, jobject player);
    jobject GetScreenHandler(JNIEnv* env, jobject player);
    int GetSelectedSlot(JNIEnv* env, jobject inventory);
    void SetSelectedSlot(JNIEnv* env, jobject inventory, int slot);
    void SyncSelectedSlot(JNIEnv* env, jobject mc);
    jobject GetInventoryStack(JNIEnv* env, jobject inventory, int index);
    jobject GetScreenSlotStack(JNIEnv* env, jobject handler, int slotId);
    int InventoryIndexToScreenSlot(int index);

    int FindHotbarItem(JNIEnv* env, jobject inventory, const char* token);
    int FindInventoryItem(JNIEnv* env, jobject inventory, const char* token);
    int FindChestplate(JNIEnv* env, jobject inventory);

    bool ClickSlot(JNIEnv* env, jobject mc, jobject player, int slotId, jobject actionType);
    bool QuickMoveSlot(JNIEnv* env, jobject mc, jobject player, int slotId);
    bool SwapWithChestSlot(JNIEnv* env, jobject mc, jobject player, int sourceSlotId);
    void SwingMainHand(JNIEnv* env, jobject player);
    bool InteractMainHand(JNIEnv* env, jobject mc, jobject player);

    jobject GetCrosshairEntity(JNIEnv* env, jobject mc);
    int GetEntityId(JNIEnv* env, jobject entity);
}
