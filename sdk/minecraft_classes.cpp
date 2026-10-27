#include "minecraft_classes.h"
#include <string>

namespace {
    jobject gClassLoader = nullptr;
    jmethodID gClassLoader_loadClass = nullptr;

    void Clear(JNIEnv* env) {
        if (env && env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }

    std::string BinaryName(const char* internalName) {
        std::string value = internalName ? internalName : "";
        for (char& ch : value) {
            if (ch == '/') {
                ch = '.';
            }
        }
        return value;
    }

    jobject CaptureContextClassLoader(JNIEnv* env) {
        if (!env) {
            return nullptr;
        }

        jclass threadClass = env->FindClass("java/lang/Thread");
        if (!threadClass) {
            Clear(env);
            return nullptr;
        }

        jmethodID currentThread = env->GetStaticMethodID(threadClass, "currentThread", "()Ljava/lang/Thread;");
        jmethodID getContextClassLoader = env->GetMethodID(threadClass, "getContextClassLoader", "()Ljava/lang/ClassLoader;");
        if (!currentThread || !getContextClassLoader) {
            Clear(env);
            env->DeleteLocalRef(threadClass);
            return nullptr;
        }

        jobject thread = env->CallStaticObjectMethod(threadClass, currentThread);
        env->DeleteLocalRef(threadClass);
        if (!thread) {
            Clear(env);
            return nullptr;
        }

        jobject loader = env->CallObjectMethod(thread, getContextClassLoader);
        env->DeleteLocalRef(thread);
        if (!loader) {
            Clear(env);
            return nullptr;
        }

        jclass loaderClass = env->FindClass("java/lang/ClassLoader");
        if (!loaderClass) {
            Clear(env);
            env->DeleteLocalRef(loader);
            return nullptr;
        }

        gClassLoader_loadClass = env->GetMethodID(loaderClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        env->DeleteLocalRef(loaderClass);
        if (!gClassLoader_loadClass) {
            Clear(env);
            env->DeleteLocalRef(loader);
            return nullptr;
        }

        if (!gClassLoader) {
            gClassLoader = env->NewGlobalRef(loader);
        }
        env->DeleteLocalRef(loader);
        return gClassLoader;
    }

    jclass LoadWithLoader(JNIEnv* env, jobject loader, const char* internalName) {
        const std::string binary = BinaryName(internalName);
        jstring name = env->NewStringUTF(binary.c_str());
        if (!name) {
            Clear(env);
            return nullptr;
        }

        jclass cls = static_cast<jclass>(env->CallObjectMethod(loader, gClassLoader_loadClass, name));
        env->DeleteLocalRef(name);
        if (!cls) {
            Clear(env);
            return nullptr;
        }
        return cls;
    }

    jclass LoadWithContextClassLoader(JNIEnv* env, const char* internalName) {
        jobject loader = gClassLoader ? gClassLoader : CaptureContextClassLoader(env);
        if (!loader || !gClassLoader_loadClass) {
            return nullptr;
        }

        jclass cls = LoadWithLoader(env, loader, internalName);
        if (cls) {
            return cls;
        }

        if (gClassLoader) {
            env->DeleteGlobalRef(gClassLoader);
            gClassLoader = nullptr;
            gClassLoader_loadClass = nullptr;
        }

        loader = CaptureContextClassLoader(env);
        if (!loader || !gClassLoader_loadClass) {
            return nullptr;
        }

        return LoadWithLoader(env, loader, internalName);
    }

    jclass GlobalClass(JNIEnv* env, const char* named, const char* intermediary) {
        jclass cls = env->FindClass(named);
        if (!cls) {
            Clear(env);
            cls = LoadWithContextClassLoader(env, named);
        }
        if (!cls) {
            Clear(env);
            cls = env->FindClass(intermediary);
        }
        if (!cls) {
            Clear(env);
            cls = LoadWithContextClassLoader(env, intermediary);
        }
        if (!cls) {
            Clear(env);
            return nullptr;
        }

        jclass global = static_cast<jclass>(env->NewGlobalRef(cls));
        env->DeleteLocalRef(cls);
        return global;
    }

    jmethodID Method(JNIEnv* env, jclass cls, const char* named, const char* namedDesc, const char* intermediary, const char* intermediaryDesc) {
        if (!cls) return nullptr;

        jmethodID method = env->GetMethodID(cls, named, namedDesc);
        if (!method) {
            Clear(env);
            method = env->GetMethodID(cls, intermediary, intermediaryDesc);
        }
        Clear(env);
        return method;
    }

    jmethodID StaticMethod(JNIEnv* env, jclass cls, const char* named, const char* namedDesc, const char* intermediary, const char* intermediaryDesc) {
        if (!cls) return nullptr;

        jmethodID method = env->GetStaticMethodID(cls, named, namedDesc);
        if (!method) {
            Clear(env);
            method = env->GetStaticMethodID(cls, intermediary, intermediaryDesc);
        }
        Clear(env);
        return method;
    }

    jfieldID Field(JNIEnv* env, jclass cls, const char* named, const char* namedDesc, const char* intermediary, const char* intermediaryDesc) {
        if (!cls) return nullptr;

        jfieldID field = env->GetFieldID(cls, named, namedDesc);
        if (!field) {
            Clear(env);
            field = env->GetFieldID(cls, intermediary, intermediaryDesc);
        }
        Clear(env);
        return field;
    }

    jfieldID StaticField(JNIEnv* env, jclass cls, const char* named, const char* namedDesc, const char* intermediary, const char* intermediaryDesc) {
        if (!cls) return nullptr;

        jfieldID field = env->GetStaticFieldID(cls, named, namedDesc);
        if (!field) {
            Clear(env);
            field = env->GetStaticFieldID(cls, intermediary, intermediaryDesc);
        }
        Clear(env);
        return field;
    }
}

namespace SDK {
    bool CacheAll(JNIEnv* env) {
        if (Classes.Ready) {
            return true;
        }

        if (!env) {
            return false;
        }

        Classes.MinecraftClient = GlobalClass(env, "net/minecraft/client/MinecraftClient", "net/minecraft/class_310");
        Classes.PlayerEntity = GlobalClass(env, "net/minecraft/entity/player/PlayerEntity", "net/minecraft/class_1657");
        Classes.ClientPlayerEntity = GlobalClass(env, "net/minecraft/client/network/ClientPlayerEntity", "net/minecraft/class_746");
        Classes.GameOptions = GlobalClass(env, "net/minecraft/client/option/GameOptions", "net/minecraft/class_315");
        Classes.Entity = GlobalClass(env, "net/minecraft/entity/Entity", "net/minecraft/class_1297");
        Classes.ClientWorld = GlobalClass(env, "net/minecraft/client/world/ClientWorld", "net/minecraft/class_638");
        Classes.Box = GlobalClass(env, "net/minecraft/util/math/Box", "net/minecraft/class_238");
        Classes.RemotePlayerEntity = GlobalClass(env, "net/minecraft/client/network/OtherClientPlayerEntity", "net/minecraft/class_742");
        Classes.LivingEntity = GlobalClass(env, "net/minecraft/entity/LivingEntity", "net/minecraft/class_1309");
        Classes.GameRenderer = GlobalClass(env, "net/minecraft/client/render/GameRenderer", "net/minecraft/class_757");
        Classes.Camera = GlobalClass(env, "net/minecraft/client/render/Camera", "net/minecraft/class_4184");
        Classes.Vec3d = GlobalClass(env, "net/minecraft/util/math/Vec3d", "net/minecraft/class_243");
        Classes.GameProfile = GlobalClass(env, "com/mojang/authlib/GameProfile", "com/mojang/authlib/GameProfile");
        Classes.Text = GlobalClass(env, "net/minecraft/text/Text", "net/minecraft/class_2561");
        Classes.SimpleOption = GlobalClass(env, "net/minecraft/client/option/SimpleOption", "net/minecraft/class_7172");
        Classes.JavaDouble = GlobalClass(env, "java/lang/Double", "java/lang/Double");
        Classes.ParticleManager = GlobalClass(env, "net/minecraft/client/particle/ParticleManager", "net/minecraft/class_702");
        Classes.InGameOverlayRenderer = GlobalClass(env, "net/minecraft/client/gui/hud/InGameOverlayRenderer", "net/minecraft/class_4603");
        Classes.InGameHud = GlobalClass(env, "net/minecraft/client/gui/hud/InGameHud", "net/minecraft/class_329");
        Classes.ChatHud = GlobalClass(env, "net/minecraft/client/gui/hud/ChatHud", "net/minecraft/class_338");
        Classes.ChatHudLine = GlobalClass(env, "net/minecraft/client/gui/hud/ChatHudLine", "net/minecraft/class_303");
        Classes.ArmorStandEntity = GlobalClass(env, "net/minecraft/entity/decoration/ArmorStandEntity", "net/minecraft/class_1531");
        Classes.KeyBinding = GlobalClass(env, "net/minecraft/client/option/KeyBinding", "net/minecraft/class_304");
        Classes.ClientPlayerInteractionManager = GlobalClass(env, "net/minecraft/client/network/ClientPlayerInteractionManager", "net/minecraft/class_636");
        Classes.ClientPlayNetworkHandler = GlobalClass(env, "net/minecraft/client/network/ClientPlayNetworkHandler", "net/minecraft/class_634");
        Classes.PlayerListEntry = GlobalClass(env, "net/minecraft/client/network/PlayerListEntry", "net/minecraft/class_640");
        Classes.ServerInfo = GlobalClass(env, "net/minecraft/client/network/ServerInfo", "net/minecraft/class_642");
        Classes.EntityHitResult = GlobalClass(env, "net/minecraft/util/hit/EntityHitResult", "net/minecraft/class_3966");
        Classes.ChatScreen = GlobalClass(env, "net/minecraft/client/gui/screen/ChatScreen", "net/minecraft/class_408");
        Classes.HandledScreen = GlobalClass(env, "net/minecraft/client/gui/screen/ingame/HandledScreen", "net/minecraft/class_465");
        Classes.StatusEffectInstance = GlobalClass(env, "net/minecraft/entity/effect/StatusEffectInstance", "net/minecraft/class_1293");
        Classes.Hand = GlobalClass(env, "net/minecraft/util/Hand", "net/minecraft/class_1268");
        Classes.Direction = GlobalClass(env, "net/minecraft/util/math/Direction", "net/minecraft/class_2350");
        Classes.EquipmentSlot = GlobalClass(env, "net/minecraft/entity/EquipmentSlot", "net/minecraft/class_1304");
        Classes.PlayerInventory = GlobalClass(env, "net/minecraft/entity/player/PlayerInventory", "net/minecraft/class_1661");
        Classes.Inventory = GlobalClass(env, "net/minecraft/inventory/Inventory", "net/minecraft/class_1263");
        Classes.ItemStack = GlobalClass(env, "net/minecraft/item/ItemStack", "net/minecraft/class_1799");
        Classes.Item = GlobalClass(env, "net/minecraft/item/Item", "net/minecraft/class_1792");
        Classes.InventoryScreen = GlobalClass(env, "net/minecraft/client/gui/screen/ingame/InventoryScreen", "net/minecraft/class_490");
        Classes.ScreenHandler = GlobalClass(env, "net/minecraft/screen/ScreenHandler", "net/minecraft/class_1703");
        Classes.Slot = GlobalClass(env, "net/minecraft/screen/slot/Slot", "net/minecraft/class_1735");
        Classes.SlotActionType = GlobalClass(env, "net/minecraft/screen/slot/SlotActionType", "net/minecraft/class_1713");
        Classes.World = GlobalClass(env, "net/minecraft/world/World", "net/minecraft/class_1937");
        Classes.Scoreboard = GlobalClass(env, "net/minecraft/scoreboard/Scoreboard", "net/minecraft/class_269");
        Classes.WorldProperties = GlobalClass(env, "net/minecraft/world/WorldProperties", "net/minecraft/class_5217");
        Classes.BlockPos = GlobalClass(env, "net/minecraft/util/math/BlockPos", "net/minecraft/class_2338");
        Classes.BlockEntity = GlobalClass(env, "net/minecraft/block/entity/BlockEntity", "net/minecraft/class_2586");
        Classes.ChestBlockEntity = GlobalClass(env, "net/minecraft/block/entity/ChestBlockEntity", "net/minecraft/class_2595");
        Classes.EnderChestBlockEntity = GlobalClass(env, "net/minecraft/block/entity/EnderChestBlockEntity", "net/minecraft/class_2611");
        Classes.ShulkerBoxBlockEntity = GlobalClass(env, "net/minecraft/block/entity/ShulkerBoxBlockEntity", "net/minecraft/class_2627");
        Classes.BarrelBlockEntity = GlobalClass(env, "net/minecraft/block/entity/BarrelBlockEntity", "net/minecraft/class_3719");
        Classes.AbstractFurnaceBlockEntity = GlobalClass(env, "net/minecraft/block/entity/AbstractFurnaceBlockEntity", "net/minecraft/class_2609");
        Classes.FurnaceBlockEntity = GlobalClass(env, "net/minecraft/block/entity/FurnaceBlockEntity", "net/minecraft/class_3866");
        Classes.BlastFurnaceBlockEntity = GlobalClass(env, "net/minecraft/block/entity/BlastFurnaceBlockEntity", "net/minecraft/class_3720");
        Classes.DispenserBlockEntity = GlobalClass(env, "net/minecraft/block/entity/DispenserBlockEntity", "net/minecraft/class_2601");
        Classes.DropperBlockEntity = GlobalClass(env, "net/minecraft/block/entity/DropperBlockEntity", "net/minecraft/class_2608");
        Classes.HopperBlockEntity = GlobalClass(env, "net/minecraft/block/entity/HopperBlockEntity", "net/minecraft/class_2614");

        if (!Classes.MinecraftClient || !Classes.PlayerEntity || !Classes.Entity) {
            return false;
        }

        Classes.MinecraftClient_getInstance = StaticMethod(
            env, Classes.MinecraftClient,
            "getInstance", "()Lnet/minecraft/client/MinecraftClient;",
            "method_1551", "()Lnet/minecraft/class_310;"
        );

        Classes.MinecraftClient_player = Field(
            env, Classes.MinecraftClient,
            "player", "Lnet/minecraft/client/network/ClientPlayerEntity;",
            "field_1724", "Lnet/minecraft/class_746;"
        );
        Classes.MinecraftClient_options = Field(
            env, Classes.MinecraftClient,
            "options", "Lnet/minecraft/client/option/GameOptions;",
            "field_1690", "Lnet/minecraft/class_315;"
        );
        Classes.MinecraftClient_world = Field(
            env, Classes.MinecraftClient,
            "world", "Lnet/minecraft/client/world/ClientWorld;",
            "field_1687", "Lnet/minecraft/class_638;"
        );
        Classes.MinecraftClient_gameRenderer = Field(
            env, Classes.MinecraftClient,
            "gameRenderer", "Lnet/minecraft/client/render/GameRenderer;",
            "field_1773", "Lnet/minecraft/class_757;"
        );
        Classes.MinecraftClient_particleManager = Field(
            env, Classes.MinecraftClient,
            "particleManager", "Lnet/minecraft/client/particle/ParticleManager;",
            "field_1713", "Lnet/minecraft/class_702;"
        );
        Classes.MinecraftClient_interactionManager = Field(
            env, Classes.MinecraftClient,
            "interactionManager", "Lnet/minecraft/client/network/ClientPlayerInteractionManager;",
            "field_1761", "Lnet/minecraft/class_636;"
        );
        Classes.MinecraftClient_crosshairTarget = Field(
            env, Classes.MinecraftClient,
            "crosshairTarget", "Lnet/minecraft/util/hit/HitResult;",
            "field_1765", "Lnet/minecraft/class_239;"
        );
        Classes.MinecraftClient_currentScreen = Field(
            env, Classes.MinecraftClient,
            "currentScreen", "Lnet/minecraft/client/gui/screen/Screen;",
            "field_1755", "Lnet/minecraft/class_437;"
        );
        Classes.MinecraftClient_inGameHud = Field(
            env, Classes.MinecraftClient,
            "inGameHud", "Lnet/minecraft/client/gui/hud/InGameHud;",
            "field_1705", "Lnet/minecraft/class_329;"
        );
        Classes.MinecraftClient_setScreen = Method(
            env, Classes.MinecraftClient,
            "setScreen", "(Lnet/minecraft/client/gui/screen/Screen;)V",
            "method_1507", "(Lnet/minecraft/class_437;)V"
        );
        Classes.MinecraftClient_getNetworkHandler = Method(
            env, Classes.MinecraftClient,
            "getNetworkHandler", "()Lnet/minecraft/client/network/ClientPlayNetworkHandler;",
            "method_1562", "()Lnet/minecraft/class_634;"
        );
        Classes.MinecraftClient_getGameProfile = Method(
            env, Classes.MinecraftClient,
            "getGameProfile", "()Lcom/mojang/authlib/GameProfile;",
            "method_53462", "()Lcom/mojang/authlib/GameProfile;"
        );
        Classes.MinecraftClient_getCurrentServerEntry = Method(
            env, Classes.MinecraftClient,
            "getCurrentServerEntry", "()Lnet/minecraft/client/network/ServerInfo;",
            "method_1558", "()Lnet/minecraft/class_642;"
        );

        Classes.PlayerEntity_setSprinting = Method(
            env, Classes.PlayerEntity,
            "setSprinting", "(Z)V",
            "method_5728", "(Z)V"
        );
        if (!Classes.PlayerEntity_setSprinting) {
            Classes.PlayerEntity_setSprinting = Method(env, Classes.Entity, "setSprinting", "(Z)V", "method_5728", "(Z)V");
        }

        Classes.PlayerEntity_isSneaking = Method(env, Classes.PlayerEntity, "isSneaking", "()Z", "method_5715", "()Z");
        if (!Classes.PlayerEntity_isSneaking) {
            Classes.PlayerEntity_isSneaking = Method(env, Classes.Entity, "isSneaking", "()Z", "method_5715", "()Z");
        }

        Classes.PlayerEntity_isAlive = Method(env, Classes.PlayerEntity, "isAlive", "()Z", "method_5805", "()Z");
        if (!Classes.PlayerEntity_isAlive) {
            Classes.PlayerEntity_isAlive = Method(env, Classes.Entity, "isAlive", "()Z", "method_5805", "()Z");
        }

        Classes.PlayerEntity_getGameProfile = Method(
            env, Classes.PlayerEntity,
            "getGameProfile", "()Lcom/mojang/authlib/GameProfile;",
            "method_7334", "()Lcom/mojang/authlib/GameProfile;"
        );
        Classes.PlayerEntity_gameProfile = Field(
            env, Classes.PlayerEntity,
            "gameProfile", "Lcom/mojang/authlib/GameProfile;",
            "field_7507", "Lcom/mojang/authlib/GameProfile;"
        );
        Classes.PlayerEntity_getAttackCooldownProgress = Method(
            env, Classes.PlayerEntity,
            "getAttackCooldownProgress", "(F)F",
            "method_7261", "(F)F"
        );
        Classes.PlayerEntity_getInventory = Method(
            env, Classes.PlayerEntity,
            "getInventory", "()Lnet/minecraft/entity/player/PlayerInventory;",
            "method_31548", "()Lnet/minecraft/class_1661;"
        );
        Classes.PlayerEntity_currentScreenHandler = Field(
            env, Classes.PlayerEntity,
            "currentScreenHandler", "Lnet/minecraft/screen/ScreenHandler;",
            "field_7512", "Lnet/minecraft/class_1703;"
        );
        Classes.ClientPlayerEntity_requestRespawn = Method(
            env, Classes.ClientPlayerEntity,
            "requestRespawn", "()V",
            "method_7331", "()V"
        );
        Classes.ClientPlayerEntity_renderYaw = Field(env, Classes.ClientPlayerEntity, "renderYaw", "F", "field_3932", "F");
        Classes.ClientPlayerEntity_renderPitch = Field(env, Classes.ClientPlayerEntity, "renderPitch", "F", "field_3916", "F");
        Classes.ClientPlayerEntity_lastRenderYaw = Field(env, Classes.ClientPlayerEntity, "lastRenderYaw", "F", "field_3931", "F");
        Classes.ClientPlayerEntity_lastRenderPitch = Field(env, Classes.ClientPlayerEntity, "lastRenderPitch", "F", "field_3914", "F");
        Classes.ClientPlayerEntity_lastYawClient = Field(env, Classes.ClientPlayerEntity, "lastYawClient", "F", "field_3941", "F");
        Classes.ClientPlayerEntity_lastPitchClient = Field(env, Classes.ClientPlayerEntity, "lastPitchClient", "F", "field_3925", "F");

        Classes.GameOptions_keyForward = Field(
            env, Classes.GameOptions,
            "forwardKey", "Lnet/minecraft/client/option/KeyBinding;",
            "field_1894", "Lnet/minecraft/class_304;"
        );
        Classes.GameOptions_keyBack = Field(
            env, Classes.GameOptions,
            "backKey", "Lnet/minecraft/client/option/KeyBinding;",
            "field_1881", "Lnet/minecraft/class_304;"
        );
        Classes.GameOptions_keyLeft = Field(
            env, Classes.GameOptions,
            "leftKey", "Lnet/minecraft/client/option/KeyBinding;",
            "field_1913", "Lnet/minecraft/class_304;"
        );
        Classes.GameOptions_keyRight = Field(
            env, Classes.GameOptions,
            "rightKey", "Lnet/minecraft/client/option/KeyBinding;",
            "field_1849", "Lnet/minecraft/class_304;"
        );
        Classes.GameOptions_keyJump = Field(
            env, Classes.GameOptions,
            "jumpKey", "Lnet/minecraft/client/option/KeyBinding;",
            "field_1903", "Lnet/minecraft/class_304;"
        );
        Classes.GameOptions_keySneak = Field(
            env, Classes.GameOptions,
            "sneakKey", "Lnet/minecraft/client/option/KeyBinding;",
            "field_1832", "Lnet/minecraft/class_304;"
        );
        Classes.GameOptions_keyAttack = Field(
            env, Classes.GameOptions,
            "attackKey", "Lnet/minecraft/client/option/KeyBinding;",
            "field_1886", "Lnet/minecraft/class_304;"
        );
        Classes.GameOptions_keyUse = Field(
            env, Classes.GameOptions,
            "useKey", "Lnet/minecraft/client/option/KeyBinding;",
            "field_1904", "Lnet/minecraft/class_304;"
        );
        Classes.GameOptions_getGamma = Method(
            env, Classes.GameOptions,
            "getGamma", "()Lnet/minecraft/client/option/SimpleOption;",
            "method_42473", "()Lnet/minecraft/class_7172;"
        );
        Classes.KeyBinding_isPressed = Method(env, Classes.KeyBinding, "isPressed", "()Z", "method_1434", "()Z");
        Classes.KeyBinding_setPressed = Method(env, Classes.KeyBinding, "setPressed", "(Z)V", "method_23481", "(Z)V");
        Classes.KeyBinding_timesPressed = Field(env, Classes.KeyBinding, "timesPressed", "I", "field_1661", "I");

        Classes.SimpleOption_getValue = Method(env, Classes.SimpleOption, "getValue", "()Ljava/lang/Object;", "method_41753", "()Ljava/lang/Object;");
        Classes.SimpleOption_setValue = Method(env, Classes.SimpleOption, "setValue", "(Ljava/lang/Object;)V", "method_41748", "(Ljava/lang/Object;)V");
        Classes.SimpleOption_value = Field(env, Classes.SimpleOption, "value", "Ljava/lang/Object;", "field_37868", "Ljava/lang/Object;");
        Classes.JavaDouble_valueOf = StaticMethod(env, Classes.JavaDouble, "valueOf", "(D)Ljava/lang/Double;", "valueOf", "(D)Ljava/lang/Double;");
        Classes.JavaDouble_doubleValue = Method(env, Classes.JavaDouble, "doubleValue", "()D", "doubleValue", "()D");

        Classes.ClientWorld_getEntities = Method(
            env, Classes.ClientWorld,
            "getEntities", "()Ljava/lang/Iterable;",
            "method_18112", "()Ljava/lang/Iterable;"
        );
        Classes.ClientWorld_getPlayers = Method(
            env, Classes.ClientWorld,
            "getPlayers", "()Ljava/util/List;",
            "method_18456", "()Ljava/util/List;"
        );

        Classes.Entity_getX = Method(env, Classes.Entity, "getX", "()D", "method_23317", "()D");
        Classes.Entity_getY = Method(env, Classes.Entity, "getY", "()D", "method_23318", "()D");
        Classes.Entity_getZ = Method(env, Classes.Entity, "getZ", "()D", "method_23321", "()D");
        Classes.Entity_getEyeY = Method(env, Classes.Entity, "getEyeY", "()D", "method_23320", "()D");
        Classes.Entity_getBoundingBox = Method(env, Classes.Entity, "getBoundingBox", "()Lnet/minecraft/util/math/Box;", "method_5829", "()Lnet/minecraft/class_238;");
        Classes.Entity_setBoundingBox = Method(env, Classes.Entity, "setBoundingBox", "(Lnet/minecraft/util/math/Box;)V", "method_5857", "(Lnet/minecraft/class_238;)V");
        Classes.Entity_getWidth = Method(env, Classes.Entity, "getWidth", "()F", "method_17681", "()F");
        Classes.Entity_getHeight = Method(env, Classes.Entity, "getHeight", "()F", "method_17682", "()F");
        Classes.Entity_getId = Method(env, Classes.Entity, "getId", "()I", "method_5628", "()I");
        Classes.Entity_getYaw = Method(env, Classes.Entity, "getYaw", "()F", "method_36454", "()F");
        Classes.Entity_setYaw = Method(env, Classes.Entity, "setYaw", "(F)V", "method_36456", "(F)V");
        Classes.Entity_getPitch = Method(env, Classes.Entity, "getPitch", "()F", "method_36455", "()F");
        Classes.Entity_setPitch = Method(env, Classes.Entity, "setPitch", "(F)V", "method_36457", "(F)V");
        Classes.Entity_changeLookDirection = Method(env, Classes.Entity, "changeLookDirection", "(DD)V", "method_5872", "(DD)V");
        Classes.Entity_setHeadYaw = Method(env, Classes.Entity, "setHeadYaw", "(F)V", "method_5847", "(F)V");
        Classes.Entity_setBodyYaw = Method(env, Classes.Entity, "setBodyYaw", "(F)V", "method_5636", "(F)V");
        Classes.Entity_isInvisible = Method(env, Classes.Entity, "isInvisible", "()Z", "method_5863", "()Z");
        Classes.Entity_setFireTicks = Method(env, Classes.Entity, "setFireTicks", "(I)V", "method_20803", "(I)V");
        Classes.Entity_setInvisible = Method(env, Classes.Entity, "setInvisible", "(Z)V", "method_5648", "(Z)V");
        Classes.Entity_isOnGround = Method(env, Classes.Entity, "isOnGround", "()Z", "method_24828", "()Z");
        Classes.Entity_isTouchingWater = Method(env, Classes.Entity, "isTouchingWater", "()Z", "method_5799", "()Z");
        Classes.Entity_isInLava = Method(env, Classes.Entity, "isInLava", "()Z", "method_5771", "()Z");
        Classes.Entity_prevX = Field(env, Classes.Entity, "prevX", "D", "field_6014", "D");
        Classes.Entity_prevY = Field(env, Classes.Entity, "prevY", "D", "field_6036", "D");
        Classes.Entity_prevZ = Field(env, Classes.Entity, "prevZ", "D", "field_5969", "D");
        Classes.Entity_yaw = Field(env, Classes.Entity, "yaw", "F", "field_6031", "F");
        Classes.Entity_pitch = Field(env, Classes.Entity, "pitch", "F", "field_5965", "F");
        Classes.Entity_lastYaw = Field(env, Classes.Entity, "lastYaw", "F", "field_5982", "F");
        Classes.Entity_lastPitch = Field(env, Classes.Entity, "lastPitch", "F", "field_6004", "F");
        Classes.Entity_onGround = Field(env, Classes.Entity, "onGround", "Z", "field_5952", "Z");
        Classes.Entity_fallDistance = Field(env, Classes.Entity, "fallDistance", "D", "field_6017", "D");

        Classes.Box_minX = Field(env, Classes.Box, "minX", "D", "field_1323", "D");
        Classes.Box_minY = Field(env, Classes.Box, "minY", "D", "field_1322", "D");
        Classes.Box_minZ = Field(env, Classes.Box, "minZ", "D", "field_1321", "D");
        Classes.Box_maxX = Field(env, Classes.Box, "maxX", "D", "field_1320", "D");
        Classes.Box_maxY = Field(env, Classes.Box, "maxY", "D", "field_1325", "D");
        Classes.Box_maxZ = Field(env, Classes.Box, "maxZ", "D", "field_1324", "D");
        Classes.Box_constructor = env->GetMethodID(Classes.Box, "<init>", "(DDDDDD)V");
        Clear(env);

        Classes.LivingEntity_getHealth = Method(env, Classes.LivingEntity, "getHealth", "()F", "method_6032", "()F");
        Classes.LivingEntity_getMaxHealth = Method(env, Classes.LivingEntity, "getMaxHealth", "()F", "method_6063", "()F");
        Classes.LivingEntity_getStatusEffects = Method(
            env, Classes.LivingEntity,
            "getStatusEffects", "()Ljava/util/Collection;",
            "method_6026", "()Ljava/util/Collection;"
        );
        Classes.LivingEntity_swingHand = Method(
            env, Classes.LivingEntity,
            "swingHand", "(Lnet/minecraft/util/Hand;)V",
            "method_6104", "(Lnet/minecraft/class_1268;)V"
        );
        Classes.LivingEntity_getEquippedStack = Method(
            env, Classes.LivingEntity,
            "getEquippedStack", "(Lnet/minecraft/entity/EquipmentSlot;)Lnet/minecraft/item/ItemStack;",
            "method_6118", "(Lnet/minecraft/class_1304;)Lnet/minecraft/class_1799;"
        );
        Classes.LivingEntity_isClimbing = Method(env, Classes.LivingEntity, "isClimbing", "()Z", "method_6101", "()Z");
        Classes.LivingEntity_isUsingItem = Method(env, Classes.LivingEntity, "isUsingItem", "()Z", "method_6115", "()Z");
        Classes.LivingEntity_isEating = Method(env, Classes.LivingEntity, "isEating", "()Z", "method_6115", "()Z");
        Classes.LivingEntity_isBlocking = Method(env, Classes.LivingEntity, "isBlocking", "()Z", "method_6039", "()Z");
        Classes.StatusEffectInstance_getDuration = Method(env, Classes.StatusEffectInstance, "getDuration", "()I", "method_5584", "()I");
        Classes.StatusEffectInstance_getAmplifier = Method(env, Classes.StatusEffectInstance, "getAmplifier", "()I", "method_5578", "()I");
        Classes.StatusEffectInstance_getTranslationKey = Method(env, Classes.StatusEffectInstance, "getTranslationKey", "()Ljava/lang/String;", "method_5586", "()Ljava/lang/String;");
        Classes.ClientPlayerInteractionManager_attackEntity = Method(
            env, Classes.ClientPlayerInteractionManager,
            "attackEntity", "(Lnet/minecraft/entity/player/PlayerEntity;Lnet/minecraft/entity/Entity;)V",
            "method_2918", "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1297;)V"
        );
        Classes.ClientPlayerInteractionManager_attackBlock = Method(
            env, Classes.ClientPlayerInteractionManager,
            "attackBlock", "(Lnet/minecraft/util/math/BlockPos;Lnet/minecraft/util/math/Direction;)Z",
            "method_2910", "(Lnet/minecraft/class_2338;Lnet/minecraft/class_2350;)Z"
        );
        Classes.ClientPlayerInteractionManager_interactItem = Method(
            env, Classes.ClientPlayerInteractionManager,
            "interactItem", "(Lnet/minecraft/entity/player/PlayerEntity;Lnet/minecraft/util/Hand;)Lnet/minecraft/util/ActionResult;",
            "method_2919", "(Lnet/minecraft/class_1657;Lnet/minecraft/class_1268;)Lnet/minecraft/class_1269;"
        );
        Classes.ClientPlayerInteractionManager_clickSlot = Method(
            env, Classes.ClientPlayerInteractionManager,
            "clickSlot", "(IIILnet/minecraft/screen/slot/SlotActionType;Lnet/minecraft/entity/player/PlayerEntity;)V",
            "method_2906", "(IIILnet/minecraft/class_1713;Lnet/minecraft/class_1657;)V"
        );
        Classes.ClientPlayerInteractionManager_syncSelectedSlot = Method(
            env, Classes.ClientPlayerInteractionManager,
            "syncSelectedSlot", "()V",
            "method_2911", "()V"
        );
        Classes.ClientPlayNetworkHandler_getPlayerList = Method(
            env, Classes.ClientPlayNetworkHandler,
            "getPlayerList", "()Ljava/util/Collection;",
            "method_2880", "()Ljava/util/Collection;"
        );
        Classes.ClientPlayNetworkHandler_getProfile = Method(
            env, Classes.ClientPlayNetworkHandler,
            "getProfile", "()Lcom/mojang/authlib/GameProfile;",
            "method_2879", "()Lcom/mojang/authlib/GameProfile;"
        );
        Classes.PlayerListEntry_getProfile = Method(
            env, Classes.PlayerListEntry,
            "getProfile", "()Lcom/mojang/authlib/GameProfile;",
            "method_2966", "()Lcom/mojang/authlib/GameProfile;"
        );
        Classes.PlayerListEntry_profile = Field(
            env, Classes.PlayerListEntry,
            "profile", "Lcom/mojang/authlib/GameProfile;",
            "field_3741", "Lcom/mojang/authlib/GameProfile;"
        );
        Classes.PlayerListEntry_displayName = Field(
            env, Classes.PlayerListEntry,
            "displayName", "Lnet/minecraft/text/Text;",
            "field_3743", "Lnet/minecraft/class_2561;"
        );
        Classes.PlayerListEntry_setDisplayName = Method(
            env, Classes.PlayerListEntry,
            "setDisplayName", "(Lnet/minecraft/text/Text;)V",
            "method_2962", "(Lnet/minecraft/class_2561;)V"
        );
        Classes.EntityHitResult_getEntity = Method(
            env, Classes.EntityHitResult,
            "getEntity", "()Lnet/minecraft/entity/Entity;",
            "method_17782", "()Lnet/minecraft/class_1297;"
        );
        Classes.Hand_MAIN_HAND = StaticField(
            env, Classes.Hand,
            "MAIN_HAND", "Lnet/minecraft/util/Hand;",
            "field_5808", "Lnet/minecraft/class_1268;"
        );
        Classes.Direction_UP = StaticField(
            env, Classes.Direction,
            "UP", "Lnet/minecraft/util/math/Direction;",
            "field_11036", "Lnet/minecraft/class_2350;"
        );
        Classes.PlayerInventory_getSelectedSlot = Method(env, Classes.PlayerInventory, "getSelectedSlot", "()I", "method_67532", "()I");
        Classes.PlayerInventory_setSelectedSlot = Method(env, Classes.PlayerInventory, "setSelectedSlot", "(I)V", "method_61496", "(I)V");
        Classes.PlayerInventory_getStack = Method(env, Classes.PlayerInventory, "getStack", "(I)Lnet/minecraft/item/ItemStack;", "method_5438", "(I)Lnet/minecraft/class_1799;");
        Classes.PlayerInventory_getSelectedStack = Method(env, Classes.PlayerInventory, "getSelectedStack", "()Lnet/minecraft/item/ItemStack;", "method_7391", "()Lnet/minecraft/class_1799;");
        Classes.PlayerInventory_selectedSlot = Field(env, Classes.PlayerInventory, "selectedSlot", "I", "field_7545", "I");
        Classes.Inventory_getStack = Method(env, Classes.Inventory, "getStack", "(I)Lnet/minecraft/item/ItemStack;", "method_5438", "(I)Lnet/minecraft/class_1799;");
        Classes.EquipmentSlot_MAINHAND = StaticField(env, Classes.EquipmentSlot, "MAINHAND", "Lnet/minecraft/entity/EquipmentSlot;", "field_6173", "Lnet/minecraft/class_1304;");
        Classes.EquipmentSlot_OFFHAND = StaticField(env, Classes.EquipmentSlot, "OFFHAND", "Lnet/minecraft/entity/EquipmentSlot;", "field_6171", "Lnet/minecraft/class_1304;");
        Classes.EquipmentSlot_FEET = StaticField(env, Classes.EquipmentSlot, "FEET", "Lnet/minecraft/entity/EquipmentSlot;", "field_6166", "Lnet/minecraft/class_1304;");
        Classes.EquipmentSlot_LEGS = StaticField(env, Classes.EquipmentSlot, "LEGS", "Lnet/minecraft/entity/EquipmentSlot;", "field_6172", "Lnet/minecraft/class_1304;");
        Classes.EquipmentSlot_CHEST = StaticField(env, Classes.EquipmentSlot, "CHEST", "Lnet/minecraft/entity/EquipmentSlot;", "field_6174", "Lnet/minecraft/class_1304;");
        Classes.EquipmentSlot_HEAD = StaticField(env, Classes.EquipmentSlot, "HEAD", "Lnet/minecraft/entity/EquipmentSlot;", "field_6169", "Lnet/minecraft/class_1304;");

        Classes.ItemStack_isEmpty = Method(env, Classes.ItemStack, "isEmpty", "()Z", "method_7960", "()Z");
        Classes.ItemStack_getItem = Method(env, Classes.ItemStack, "getItem", "()Lnet/minecraft/item/Item;", "method_7909", "()Lnet/minecraft/class_1792;");
        Classes.ItemStack_getCount = Method(env, Classes.ItemStack, "getCount", "()I", "method_7947", "()I");
        Classes.Item_getTranslationKey = Method(env, Classes.Item, "getTranslationKey", "()Ljava/lang/String;", "method_7876", "()Ljava/lang/String;");
        if (!Classes.Item_getTranslationKey) {
            Classes.Item_getTranslationKey = Method(env, Classes.Item, "getTranslationKey", "()Ljava/lang/String;", "method_63689", "()Ljava/lang/String;");
        }

        if (Classes.InventoryScreen) {
            Classes.InventoryScreen_constructor = env->GetMethodID(Classes.InventoryScreen, "<init>", "(Lnet/minecraft/entity/player/PlayerEntity;)V");
            if (!Classes.InventoryScreen_constructor) {
                Clear(env);
                Classes.InventoryScreen_constructor = env->GetMethodID(Classes.InventoryScreen, "<init>", "(Lnet/minecraft/class_1657;)V");
            }
            Clear(env);
        }
        Classes.ScreenHandler_syncId = Field(env, Classes.ScreenHandler, "syncId", "I", "field_7763", "I");
        Classes.ScreenHandler_getSlot = Method(env, Classes.ScreenHandler, "getSlot", "(I)Lnet/minecraft/screen/slot/Slot;", "method_7611", "(I)Lnet/minecraft/class_1735;");
        Classes.Slot_getStack = Method(env, Classes.Slot, "getStack", "()Lnet/minecraft/item/ItemStack;", "method_7677", "()Lnet/minecraft/class_1799;");
        Classes.Slot_id = Field(env, Classes.Slot, "id", "I", "field_7874", "I");
        Classes.SlotActionType_PICKUP = StaticField(env, Classes.SlotActionType, "PICKUP", "Lnet/minecraft/screen/slot/SlotActionType;", "field_7790", "Lnet/minecraft/class_1713;");
        Classes.SlotActionType_QUICK_MOVE = StaticField(env, Classes.SlotActionType, "QUICK_MOVE", "Lnet/minecraft/screen/slot/SlotActionType;", "field_7794", "Lnet/minecraft/class_1713;");
        Classes.HandledScreen_focusedSlot = Field(env, Classes.HandledScreen, "focusedSlot", "Lnet/minecraft/screen/slot/Slot;", "field_2787", "Lnet/minecraft/class_1735;");

        Classes.ClientWorld_setTime = Method(env, Classes.ClientWorld, "setTime", "(JJZ)V", "method_29089", "(JJZ)V");
        Classes.World_getScoreboard = Method(
            env, Classes.World,
            "getScoreboard", "()Lnet/minecraft/scoreboard/Scoreboard;",
            "method_8428", "()Lnet/minecraft/class_269;"
        );
        Classes.World_getLevelProperties = Method(env, Classes.World, "getLevelProperties", "()Lnet/minecraft/world/WorldProperties;", "method_8401", "()Lnet/minecraft/class_5217;");
        Classes.World_setRainGradient = Method(env, Classes.World, "setRainGradient", "(F)V", "method_8519", "(F)V");
        Classes.World_setThunderGradient = Method(env, Classes.World, "setThunderGradient", "(F)V", "method_8496", "(F)V");
        Classes.WorldProperties_setRaining = Method(env, Classes.WorldProperties, "setRaining", "(Z)V", "method_157", "(Z)V");
        Classes.World_getBlockEntity = Method(
            env, Classes.World,
            "getBlockEntity", "(Lnet/minecraft/util/math/BlockPos;)Lnet/minecraft/block/entity/BlockEntity;",
            "method_8321", "(Lnet/minecraft/class_2338;)Lnet/minecraft/class_2586;"
        );
        if (Classes.BlockPos) {
            Classes.BlockPos_constructor = env->GetMethodID(Classes.BlockPos, "<init>", "(III)V");
            Clear(env);
        }
        Classes.BlockPos_getX = Method(env, Classes.BlockPos, "getX", "()I", "method_10263", "()I");
        Classes.BlockPos_getY = Method(env, Classes.BlockPos, "getY", "()I", "method_10264", "()I");
        Classes.BlockPos_getZ = Method(env, Classes.BlockPos, "getZ", "()I", "method_10260", "()I");
        Classes.BlockEntity_getPos = Method(
            env, Classes.BlockEntity,
            "getPos", "()Lnet/minecraft/util/math/BlockPos;",
            "method_11016", "()Lnet/minecraft/class_2338;"
        );
        if (!Classes.BlockEntity_getPos) {
            Classes.BlockEntity_getPos = Method(
                env, Classes.BlockEntity,
                "getBlockPos", "()Lnet/minecraft/util/math/BlockPos;",
                "method_11016", "()Lnet/minecraft/class_2338;"
            );
        }
        Classes.BlockEntity_getBlockState = Method(
            env, Classes.BlockEntity,
            "getCachedState", "()Lnet/minecraft/block/BlockState;",
            "method_11010", "()Lnet/minecraft/class_2680;"
        );
        if (!Classes.BlockEntity_getBlockState) {
            Classes.BlockEntity_getBlockState = Method(
                env, Classes.BlockEntity,
                "getBlockState", "()Lnet/minecraft/block/BlockState;",
                "method_11010", "()Lnet/minecraft/class_2680;"
            );
        }

        Classes.GameRenderer_getCamera = Method(
            env, Classes.GameRenderer,
            "getCamera", "()Lnet/minecraft/client/render/Camera;",
            "method_19418", "()Lnet/minecraft/class_4184;"
        );
        Classes.GameRenderer_getFov = Method(
            env, Classes.GameRenderer,
            "getFov", "(Lnet/minecraft/client/render/Camera;FZ)F",
            "method_3196", "(Lnet/minecraft/class_4184;FZ)F"
        );
        Classes.GameRenderer_overlayRenderer = Field(
            env, Classes.GameRenderer,
            "overlayRenderer", "Lnet/minecraft/client/gui/hud/InGameOverlayRenderer;",
            "field_59967", "Lnet/minecraft/class_4603;"
        );

        Classes.Camera_getPos = Method(env, Classes.Camera, "getCameraPos", "()Lnet/minecraft/util/math/Vec3d;", "method_71156", "()Lnet/minecraft/class_243;");
        Classes.Camera_getPitch = Method(env, Classes.Camera, "getPitch", "()F", "method_19329", "()F");
        Classes.Camera_getYaw = Method(env, Classes.Camera, "getYaw", "()F", "method_19330", "()F");
        Classes.Vec3d_getX = Method(env, Classes.Vec3d, "getX", "()D", "method_10216", "()D");
        Classes.Vec3d_getY = Method(env, Classes.Vec3d, "getY", "()D", "method_10214", "()D");
        Classes.Vec3d_getZ = Method(env, Classes.Vec3d, "getZ", "()D", "method_10215", "()D");

        Classes.GameProfile_getName = Method(env, Classes.GameProfile, "name", "()Ljava/lang/String;", "name", "()Ljava/lang/String;");
        if (!Classes.GameProfile_getName) {
            Classes.GameProfile_getName = Method(env, Classes.GameProfile, "getName", "()Ljava/lang/String;", "getName", "()Ljava/lang/String;");
        }
        Classes.GameProfile_name = Field(env, Classes.GameProfile, "name", "Ljava/lang/String;", "name", "Ljava/lang/String;");
        Classes.GameProfile_id = Field(env, Classes.GameProfile, "id", "Ljava/util/UUID;", "id", "Ljava/util/UUID;");
        Classes.GameProfile_properties = Field(env, Classes.GameProfile, "properties", "Lcom/mojang/authlib/properties/PropertyMap;", "properties", "Lcom/mojang/authlib/properties/PropertyMap;");
        Classes.GameProfile_constructor = env->GetMethodID(Classes.GameProfile, "<init>", "(Ljava/util/UUID;Ljava/lang/String;Lcom/mojang/authlib/properties/PropertyMap;)V");
        Clear(env);
        Classes.GameProfile_propertiesMethod = Method(env, Classes.GameProfile, "properties", "()Lcom/mojang/authlib/properties/PropertyMap;", "properties", "()Lcom/mojang/authlib/properties/PropertyMap;");
        Classes.Text_literal = StaticMethod(env, Classes.Text, "literal", "(Ljava/lang/String;)Lnet/minecraft/text/MutableText;", "method_43470", "(Ljava/lang/String;)Lnet/minecraft/class_5250;");
        if (!Classes.Text_literal) {
            Classes.Text_literal = StaticMethod(env, Classes.Text, "of", "(Ljava/lang/String;)Lnet/minecraft/text/Text;", "method_30163", "(Ljava/lang/String;)Lnet/minecraft/class_2561;");
        }
        Classes.Text_getString = Method(env, Classes.Text, "getString", "()Ljava/lang/String;", "getString", "()Ljava/lang/String;");
        if (!Classes.Text_getString) {
            Classes.Text_getString = Method(env, Classes.Text, "getString", "()Ljava/lang/String;", "method_10851", "()Ljava/lang/String;");
        }
        Classes.InGameHud_getChatHud = Method(
            env, Classes.InGameHud,
            "getChatHud", "()Lnet/minecraft/client/gui/hud/ChatHud;",
            "method_1743", "()Lnet/minecraft/class_338;"
        );
        Classes.ChatHud_messages = Field(
            env, Classes.ChatHud,
            "messages", "Ljava/util/List;",
            "field_2061", "Ljava/util/List;"
        );
        Classes.ChatHud_refresh = Method(env, Classes.ChatHud, "refresh", "()V", "method_44813", "()V");
        if (Classes.ChatHudLine) {
            Classes.ChatHudLine_constructor = env->GetMethodID(Classes.ChatHudLine, "<init>", "(ILnet/minecraft/text/Text;Lnet/minecraft/network/message/MessageSignatureData;Lnet/minecraft/client/gui/hud/MessageIndicator;)V");
            if (!Classes.ChatHudLine_constructor) {
                Clear(env);
                Classes.ChatHudLine_constructor = env->GetMethodID(Classes.ChatHudLine, "<init>", "(ILnet/minecraft/class_2561;Lnet/minecraft/class_7469;Lnet/minecraft/class_7591;)V");
            }
            Clear(env);
        }
        Classes.ChatHudLine_creationTick = Field(env, Classes.ChatHudLine, "creationTick", "I", "comp_892", "I");
        Classes.ChatHudLine_content = Field(env, Classes.ChatHudLine, "content", "Lnet/minecraft/text/Text;", "comp_893", "Lnet/minecraft/class_2561;");
        Classes.ChatHudLine_signature = Field(env, Classes.ChatHudLine, "signature", "Lnet/minecraft/network/message/MessageSignatureData;", "comp_915", "Lnet/minecraft/class_7469;");
        Classes.ChatHudLine_indicator = Field(env, Classes.ChatHudLine, "indicator", "Lnet/minecraft/client/gui/hud/MessageIndicator;", "comp_894", "Lnet/minecraft/class_7591;");
        Classes.Scoreboard_scores = Field(
            env, Classes.Scoreboard,
            "scores", "Ljava/util/Map;",
            "field_1431", "Ljava/util/Map;"
        );
        Classes.Scoreboard_teamsByScoreHolder = Field(
            env, Classes.Scoreboard,
            "teamsByScoreHolder", "Lit/unimi/dsi/fastutil/objects/Object2ObjectMap;",
            "field_1427", "Lit/unimi/dsi/fastutil/objects/Object2ObjectMap;"
        );
        Classes.ServerInfo_address = Field(env, Classes.ServerInfo, "address", "Ljava/lang/String;", "field_3761", "Ljava/lang/String;");
        Classes.ParticleManager_clearParticles = Method(env, Classes.ParticleManager, "clearParticles", "()V", "method_3057", "()V");
        Classes.InGameOverlayRenderer_clearFloatingItem = Method(env, Classes.InGameOverlayRenderer, "clearFloatingItem", "()V", "method_70940", "()V");

        Classes.Ready = Classes.MinecraftClient_getInstance &&
            Classes.MinecraftClient_player &&
            Classes.PlayerEntity_setSprinting &&
            Classes.PlayerEntity_isSneaking &&
            Classes.PlayerEntity_isAlive;
        return Classes.Ready;
    }
}
