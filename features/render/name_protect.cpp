#include "name_protect.h"
#include "friends.h"
#include "inventory_utils.h"
#include "minecraft_feature_utils.h"

#include <atomic>
#include <algorithm>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {
    constexpr const char* kProtectedName = "lumaware.ru";

    std::atomic_bool gEnabled{ false };
    std::atomic_bool gOnFriends{ false };
    std::mutex gMutex;
    std::unordered_map<int, std::string> gOriginalNames;
    std::unordered_map<int, std::string> gOriginalProfileNames;

    jclass gSystemClass = nullptr;
    jmethodID gIdentityHashCode = nullptr;
    jclass gListClass = nullptr;
    jmethodID gListSize = nullptr;
    jmethodID gListGet = nullptr;
    jmethodID gListSet = nullptr;
    jclass gMapClass = nullptr;
    jmethodID gMapGet = nullptr;
    jmethodID gMapPut = nullptr;
    jmethodID gMapRemove = nullptr;
    jmethodID gMapKeySet = nullptr;
    jclass gStringClass = nullptr;

    int IdentityHash(JNIEnv* env, jobject object) {
        if (!env || !object) {
            return 0;
        }

        if (!gSystemClass) {
            jclass local = env->FindClass("java/lang/System");
            if (!local) {
                Features::MC::Clear(env);
                return 0;
            }
            gSystemClass = static_cast<jclass>(env->NewGlobalRef(local));
            env->DeleteLocalRef(local);
        }

        if (gSystemClass && !gIdentityHashCode) {
            gIdentityHashCode = env->GetStaticMethodID(gSystemClass, "identityHashCode", "(Ljava/lang/Object;)I");
            Features::MC::Clear(env);
        }

        if (!gSystemClass || !gIdentityHashCode) {
            return 0;
        }

        const jint value = env->CallStaticIntMethod(gSystemClass, gIdentityHashCode, object);
        if (env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return 0;
        }
        return static_cast<int>(value);
    }

    jobject GetProfile(JNIEnv* env, jobject entity) {
        if (!env || !entity) {
            return nullptr;
        }

        if (SDK::Classes.PlayerEntity_getGameProfile) {
            jobject profile = env->CallObjectMethod(entity, SDK::Classes.PlayerEntity_getGameProfile);
            if (profile && !env->ExceptionCheck()) {
                return profile;
            }
            Features::MC::Clear(env);
        }

        if (jmethodID method = Features::MC::FindGameProfileMethod(env, entity)) {
            jobject profile = env->CallObjectMethod(entity, method);
            if (profile && !env->ExceptionCheck()) {
                return profile;
            }
            Features::MC::Clear(env);
        }

        return Features::MC::FindGameProfileField(env, entity);
    }

    std::string ReadProfileName(JNIEnv* env, jobject profile) {
        if (!env || !profile) {
            return {};
        }

        jmethodID method = SDK::Classes.GameProfile_getName ? SDK::Classes.GameProfile_getName : Features::MC::FindGameProfileNameMethod(env, profile);
        if (!method) {
            return {};
        }

        jstring value = static_cast<jstring>(env->CallObjectMethod(profile, method));
        if (env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return {};
        }

        return Features::Inventory::ReadJavaString(env, value, "");
    }

    jfieldID FindProfileNameField(JNIEnv* env, jobject profile) {
        if (!env || !profile) {
            return nullptr;
        }

        if (SDK::Classes.GameProfile_name) {
            return SDK::Classes.GameProfile_name;
        }

        jclass profileClass = env->GetObjectClass(profile);
        if (!profileClass) {
            Features::MC::Clear(env);
            return nullptr;
        }

        jfieldID field = env->GetFieldID(profileClass, "name", "Ljava/lang/String;");
        env->DeleteLocalRef(profileClass);
        Features::MC::Clear(env);
        return field;
    }

    void SetProfileName(JNIEnv* env, jobject profile, const std::string& name) {
        if (!env || !profile) {
            return;
        }

        jfieldID field = FindProfileNameField(env, profile);
        if (!field) {
            return;
        }

        jstring value = env->NewStringUTF(name.c_str());
        if (!value) {
            Features::MC::Clear(env);
            return;
        }

        env->SetObjectField(profile, field, value);
        env->DeleteLocalRef(value);
        Features::MC::Clear(env);
    }

    jobject CloneProfileWithName(JNIEnv* env, jobject profile, const std::string& name) {
        if (!env || !profile || !SDK::Classes.GameProfile || !SDK::Classes.GameProfile_constructor) {
            return nullptr;
        }

        jobject id = SDK::Classes.GameProfile_id ? env->GetObjectField(profile, SDK::Classes.GameProfile_id) : nullptr;
        Features::MC::Clear(env);

        jobject properties = nullptr;
        if (SDK::Classes.GameProfile_propertiesMethod) {
            properties = env->CallObjectMethod(profile, SDK::Classes.GameProfile_propertiesMethod);
            if (env->ExceptionCheck()) {
                properties = nullptr;
                Features::MC::Clear(env);
            }
        }
        if (!properties && SDK::Classes.GameProfile_properties) {
            properties = env->GetObjectField(profile, SDK::Classes.GameProfile_properties);
            Features::MC::Clear(env);
        }

        jstring value = env->NewStringUTF(name.c_str());
        if (!value) {
            if (id) env->DeleteLocalRef(id);
            if (properties) env->DeleteLocalRef(properties);
            Features::MC::Clear(env);
            return nullptr;
        }

        jobject replacement = env->NewObject(SDK::Classes.GameProfile, SDK::Classes.GameProfile_constructor, id, value, properties);
        env->DeleteLocalRef(value);
        if (id) env->DeleteLocalRef(id);
        if (properties) env->DeleteLocalRef(properties);

        if (!replacement || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return nullptr;
        }
        return replacement;
    }

    void RememberProfileAlias(JNIEnv* env, jobject profile, const std::string& original);

    void ReplaceEntityProfile(JNIEnv* env, jobject entity, jobject profile, const std::string& name) {
        if (!env || !entity || !profile) {
            return;
        }

        jfieldID field = SDK::Classes.PlayerEntity_gameProfile;
        if (!field) {
            jclass entityClass = env->GetObjectClass(entity);
            if (entityClass) {
                field = env->GetFieldID(entityClass, "gameProfile", "Lcom/mojang/authlib/GameProfile;");
                Features::MC::Clear(env);
                if (!field) {
                    field = env->GetFieldID(entityClass, "field_7507", "Lcom/mojang/authlib/GameProfile;");
                    Features::MC::Clear(env);
                }
                env->DeleteLocalRef(entityClass);
            }
        }
        if (!field) {
            return;
        }

        jobject replacement = CloneProfileWithName(env, profile, name);
        if (!replacement) {
            return;
        }

        const std::string original = ReadProfileName(env, profile);
        RememberProfileAlias(env, replacement, original == kProtectedName ? name : original);
        env->SetObjectField(entity, field, replacement);
        env->DeleteLocalRef(replacement);
        Features::MC::Clear(env);
    }

    jobject LiteralText(JNIEnv* env, const std::string& text) {
        if (!env || !SDK::Classes.Text_literal) {
            return nullptr;
        }

        jstring value = env->NewStringUTF(text.c_str());
        if (!value) {
            Features::MC::Clear(env);
            return nullptr;
        }

        jobject result = env->CallStaticObjectMethod(SDK::Classes.Text, SDK::Classes.Text_literal, value);
        env->DeleteLocalRef(value);
        if (!result || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return nullptr;
        }
        return result;
    }

    bool EnsureListMethods(JNIEnv* env) {
        if (!env) {
            return false;
        }
        if (!gListClass) {
            jclass local = env->FindClass("java/util/List");
            if (!local) {
                Features::MC::Clear(env);
                return false;
            }
            gListClass = static_cast<jclass>(env->NewGlobalRef(local));
            env->DeleteLocalRef(local);
        }
        if (!gListClass) {
            return false;
        }
        if (!gListSize) gListSize = env->GetMethodID(gListClass, "size", "()I");
        if (!gListGet) gListGet = env->GetMethodID(gListClass, "get", "(I)Ljava/lang/Object;");
        if (!gListSet) gListSet = env->GetMethodID(gListClass, "set", "(ILjava/lang/Object;)Ljava/lang/Object;");
        Features::MC::Clear(env);
        return gListSize && gListGet && gListSet;
    }

    bool EnsureMapMethods(JNIEnv* env) {
        if (!env) {
            return false;
        }
        if (!gMapClass) {
            jclass local = env->FindClass("java/util/Map");
            if (!local) {
                Features::MC::Clear(env);
                return false;
            }
            gMapClass = static_cast<jclass>(env->NewGlobalRef(local));
            env->DeleteLocalRef(local);
        }
        if (!gMapClass) {
            return false;
        }
        if (!gMapGet) gMapGet = env->GetMethodID(gMapClass, "get", "(Ljava/lang/Object;)Ljava/lang/Object;");
        if (!gMapPut) gMapPut = env->GetMethodID(gMapClass, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");
        if (!gMapRemove) gMapRemove = env->GetMethodID(gMapClass, "remove", "(Ljava/lang/Object;)Ljava/lang/Object;");
        if (!gMapKeySet) gMapKeySet = env->GetMethodID(gMapClass, "keySet", "()Ljava/util/Set;");
        Features::MC::Clear(env);
        return gMapGet && gMapPut && gMapRemove && gMapKeySet;
    }

    std::string ReadText(JNIEnv* env, jobject text) {
        if (!env || !text || !SDK::Classes.Text_getString) {
            return {};
        }

        jstring value = static_cast<jstring>(env->CallObjectMethod(text, SDK::Classes.Text_getString));
        if (env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return {};
        }
        return Features::Inventory::ReadJavaString(env, value, "");
    }

    std::string ReplaceAliases(std::string text, const std::vector<std::string>& aliases) {
        for (const std::string& alias : aliases) {
            if (alias.empty() || alias == kProtectedName) {
                continue;
            }

            size_t pos = 0;
            while ((pos = text.find(alias, pos)) != std::string::npos) {
                text.replace(pos, alias.size(), kProtectedName);
                pos += std::char_traits<char>::length(kProtectedName);
            }
        }
        return text;
    }

    std::vector<std::string> ProtectedAliases(const std::string& localName) {
        std::vector<std::string> aliases;
        auto add = [&](const std::string& name) {
            if (name.empty() || name == kProtectedName) {
                return;
            }
            if (std::find(aliases.begin(), aliases.end(), name) == aliases.end()) {
                aliases.push_back(name);
            }
        };

        add(localName);
        if (gOnFriends.load()) {
            std::lock_guard<std::mutex> lock(gMutex);
            for (const auto& [_, name] : gOriginalNames) {
                if (Features::Friends::IsFriend(name)) {
                    add(name);
                }
            }
            for (const auto& [_, name] : gOriginalProfileNames) {
                if (Features::Friends::IsFriend(name)) {
                    add(name);
                }
            }
        }
        return aliases;
    }

    jobject CopyChatLine(JNIEnv* env, jobject line, jobject content) {
        if (!env || !line || !content || !SDK::Classes.ChatHudLine_constructor ||
            !SDK::Classes.ChatHudLine_creationTick || !SDK::Classes.ChatHudLine_signature || !SDK::Classes.ChatHudLine_indicator) {
            return nullptr;
        }

        const jint creationTick = env->GetIntField(line, SDK::Classes.ChatHudLine_creationTick);
        jobject signature = env->GetObjectField(line, SDK::Classes.ChatHudLine_signature);
        jobject indicator = env->GetObjectField(line, SDK::Classes.ChatHudLine_indicator);
        if (env->ExceptionCheck()) {
            if (signature) env->DeleteLocalRef(signature);
            if (indicator) env->DeleteLocalRef(indicator);
            Features::MC::Clear(env);
            return nullptr;
        }

        jobject replacement = env->NewObject(SDK::Classes.ChatHudLine, SDK::Classes.ChatHudLine_constructor, creationTick, content, signature, indicator);
        if (signature) env->DeleteLocalRef(signature);
        if (indicator) env->DeleteLocalRef(indicator);
        if (!replacement || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return nullptr;
        }
        return replacement;
    }

    void ProtectChatHud(JNIEnv* env, jobject mc, const std::vector<std::string>& aliases) {
        if (!env || !mc || aliases.empty() || !SDK::Classes.MinecraftClient_inGameHud ||
            !SDK::Classes.InGameHud_getChatHud || !SDK::Classes.ChatHud_messages || !SDK::Classes.ChatHudLine_content ||
            !EnsureListMethods(env)) {
            return;
        }

        jobject hud = env->GetObjectField(mc, SDK::Classes.MinecraftClient_inGameHud);
        if (!hud || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return;
        }

        jobject chat = env->CallObjectMethod(hud, SDK::Classes.InGameHud_getChatHud);
        env->DeleteLocalRef(hud);
        if (!chat || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return;
        }

        jobject messages = env->GetObjectField(chat, SDK::Classes.ChatHud_messages);
        if (!messages || env->ExceptionCheck()) {
            env->DeleteLocalRef(chat);
            Features::MC::Clear(env);
            return;
        }

        const jint size = env->CallIntMethod(messages, gListSize);
        if (env->ExceptionCheck()) {
            env->DeleteLocalRef(messages);
            env->DeleteLocalRef(chat);
            Features::MC::Clear(env);
            return;
        }

        bool changed = false;
        for (jint i = 0; i < size; ++i) {
            jobject line = env->CallObjectMethod(messages, gListGet, i);
            if (!line || env->ExceptionCheck()) {
                Features::MC::Clear(env);
                continue;
            }

            jobject content = env->GetObjectField(line, SDK::Classes.ChatHudLine_content);
            if (!content || env->ExceptionCheck()) {
                if (content) env->DeleteLocalRef(content);
                env->DeleteLocalRef(line);
                Features::MC::Clear(env);
                continue;
            }

            const std::string originalText = ReadText(env, content);
            const std::string protectedText = ReplaceAliases(originalText, aliases);
            if (!protectedText.empty() && protectedText != originalText) {
                jobject text = LiteralText(env, protectedText);
                jobject replacement = text ? CopyChatLine(env, line, text) : nullptr;
                if (replacement) {
                    jobject old = env->CallObjectMethod(messages, gListSet, i, replacement);
                    if (old) env->DeleteLocalRef(old);
                    if (!env->ExceptionCheck()) {
                        changed = true;
                    }
                    Features::MC::Clear(env);
                    env->DeleteLocalRef(replacement);
                }
                if (text) env->DeleteLocalRef(text);
            }

            env->DeleteLocalRef(content);
            env->DeleteLocalRef(line);
        }

        if (changed && SDK::Classes.ChatHud_refresh) {
            env->CallVoidMethod(chat, SDK::Classes.ChatHud_refresh);
            Features::MC::Clear(env);
        }

        env->DeleteLocalRef(messages);
        env->DeleteLocalRef(chat);
    }

    void ReplaceMapKeys(JNIEnv* env, jobject map, const std::vector<std::string>& aliases) {
        if (!env || !map || aliases.empty() || !EnsureMapMethods(env)) {
            return;
        }
        if (!gStringClass) {
            jclass local = env->FindClass("java/lang/String");
            if (!local) {
                Features::MC::Clear(env);
                return;
            }
            gStringClass = static_cast<jclass>(env->NewGlobalRef(local));
            env->DeleteLocalRef(local);
        }

        jobject keySet = env->CallObjectMethod(map, gMapKeySet);
        if (!keySet || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return;
        }

        std::vector<std::pair<std::string, std::string>> replacements;
        Features::MC::ForEachIterable(env, keySet, [&](jobject key) {
            if (!key || !gStringClass || !env->IsInstanceOf(key, gStringClass)) {
                Features::MC::Clear(env);
                return;
            }
            jstring stringKey = static_cast<jstring>(key);
            const char* chars = env->GetStringUTFChars(stringKey, nullptr);
            std::string oldKey = chars ? chars : "";
            if (chars) {
                env->ReleaseStringUTFChars(stringKey, chars);
            }
            const std::string newKey = ReplaceAliases(oldKey, aliases);
            if (!newKey.empty() && newKey != oldKey) {
                replacements.emplace_back(std::move(oldKey), newKey);
            }
        });
        env->DeleteLocalRef(keySet);

        for (const auto& [oldKey, newKey] : replacements) {
            jstring oldJava = env->NewStringUTF(oldKey.c_str());
            jstring newJava = env->NewStringUTF(newKey.c_str());
            if (!oldJava || !newJava) {
                if (oldJava) env->DeleteLocalRef(oldJava);
                if (newJava) env->DeleteLocalRef(newJava);
                Features::MC::Clear(env);
                continue;
            }

            jobject value = env->CallObjectMethod(map, gMapGet, oldJava);
            Features::MC::Clear(env);
            if (value) {
                jobject removed = env->CallObjectMethod(map, gMapRemove, oldJava);
                if (removed) env->DeleteLocalRef(removed);
                Features::MC::Clear(env);

                jobject previous = env->CallObjectMethod(map, gMapPut, newJava, value);
                if (previous) env->DeleteLocalRef(previous);
                env->DeleteLocalRef(value);
                Features::MC::Clear(env);
            }

            env->DeleteLocalRef(oldJava);
            env->DeleteLocalRef(newJava);
        }
    }

    void ProtectScoreboard(JNIEnv* env, jobject world, const std::vector<std::string>& aliases) {
        if (!env || !world || aliases.empty() || !SDK::Classes.World_getScoreboard) {
            return;
        }

        jobject scoreboard = env->CallObjectMethod(world, SDK::Classes.World_getScoreboard);
        if (!scoreboard || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return;
        }

        if (SDK::Classes.Scoreboard_scores) {
            jobject scores = env->GetObjectField(scoreboard, SDK::Classes.Scoreboard_scores);
            if (scores && !env->ExceptionCheck()) {
                ReplaceMapKeys(env, scores, aliases);
                env->DeleteLocalRef(scores);
            }
            Features::MC::Clear(env);
        }

        if (SDK::Classes.Scoreboard_teamsByScoreHolder) {
            jobject teams = env->GetObjectField(scoreboard, SDK::Classes.Scoreboard_teamsByScoreHolder);
            if (teams && !env->ExceptionCheck()) {
                ReplaceMapKeys(env, teams, aliases);
                env->DeleteLocalRef(teams);
            }
            Features::MC::Clear(env);
        }

        env->DeleteLocalRef(scoreboard);
    }

    void RememberProfileAlias(JNIEnv* env, jobject profile, const std::string& original) {
        if (!env || !profile || original.empty() || original == kProtectedName) {
            return;
        }

        const int identity = IdentityHash(env, profile);
        if (identity == 0) {
            return;
        }

        std::lock_guard<std::mutex> lock(gMutex);
        gOriginalProfileNames.emplace(identity, original);
    }

    void ProtectPlayerListEntry(JNIEnv* env, jobject entry, jobject profile, const std::string& desired, const std::string& original) {
        if (!env || !entry || !profile || desired.empty()) {
            return;
        }

        SetProfileName(env, profile, desired);

        if (SDK::Classes.PlayerListEntry_profile) {
            jobject replacement = CloneProfileWithName(env, profile, desired);
            if (replacement) {
                RememberProfileAlias(env, replacement, original);
                env->SetObjectField(entry, SDK::Classes.PlayerListEntry_profile, replacement);
                env->DeleteLocalRef(replacement);
                Features::MC::Clear(env);
            }
        }

        jobject text = LiteralText(env, desired);
        if (text) {
            if (SDK::Classes.PlayerListEntry_setDisplayName) {
                env->CallVoidMethod(entry, SDK::Classes.PlayerListEntry_setDisplayName, text);
            } else if (SDK::Classes.PlayerListEntry_displayName) {
                env->SetObjectField(entry, SDK::Classes.PlayerListEntry_displayName, text);
            }
            env->DeleteLocalRef(text);
            Features::MC::Clear(env);
        }
    }

    std::string RememberOriginal(int id, const std::string& current) {
        if (id == 0) {
            return current;
        }

        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gOriginalNames.find(id);
        if (it != gOriginalNames.end()) {
            return it->second;
        }

        if (!current.empty() && current != kProtectedName) {
            it = gOriginalNames.emplace(id, current).first;
            return it->second;
        }

        return current;
    }

    std::string RememberProfileOriginal(JNIEnv* env, jobject profile, const std::string& current) {
        const int identity = IdentityHash(env, profile);
        if (identity == 0) {
            return current;
        }

        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gOriginalProfileNames.find(identity);
        if (it != gOriginalProfileNames.end()) {
            return it->second;
        }

        if (!current.empty() && current != kProtectedName) {
            it = gOriginalProfileNames.emplace(identity, current).first;
            return it->second;
        }

        return current;
    }

    std::string ProtectProfile(JNIEnv* env, jobject profile, bool protect) {
        if (!env || !profile) {
            return {};
        }

        const std::string current = ReadProfileName(env, profile);
        const std::string original = RememberProfileOriginal(env, profile, current);
        const std::string desired = gEnabled.load() && protect ? kProtectedName : original;

        if (!desired.empty() && current != desired) {
            SetProfileName(env, profile, desired);
        }

        return original;
    }

    std::string ProtectEntity(JNIEnv* env, jobject entity, bool isLocal) {
        if (!env || !entity) {
            return {};
        }

        const bool playerEntity = Features::MC::IsInstance(env, entity, SDK::Classes.PlayerEntity) ||
            Features::MC::IsInstance(env, entity, SDK::Classes.RemotePlayerEntity);
        if (!playerEntity) {
            return {};
        }

        jobject profile = GetProfile(env, entity);
        if (!profile) {
            return {};
        }

        const int id = Features::Inventory::GetEntityId(env, entity);
        const std::string current = ReadProfileName(env, profile);
        const std::string profileOriginal = RememberProfileOriginal(env, profile, current);
        const std::string original = RememberOriginal(id, profileOriginal.empty() ? current : profileOriginal);
        const bool protect = isLocal || (gOnFriends.load() && Features::Friends::IsFriend(original));
        const std::string desired = gEnabled.load() && protect ? kProtectedName : original;

        if (!desired.empty() && current != desired) {
            ReplaceEntityProfile(env, entity, profile, desired);
            SetProfileName(env, profile, desired);
        }

        env->DeleteLocalRef(profile);
        return original;
    }

    void ProtectClientProfile(JNIEnv* env, jobject mc) {
        if (!env || !mc || !SDK::Classes.MinecraftClient_getGameProfile) {
            return;
        }

        jobject profile = env->CallObjectMethod(mc, SDK::Classes.MinecraftClient_getGameProfile);
        if (!profile || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return;
        }

        ProtectProfile(env, profile, true);
        env->DeleteLocalRef(profile);
    }

    void ProtectPlayerList(JNIEnv* env, jobject mc, const std::string& localName) {
        if (!env || !mc || !SDK::Classes.MinecraftClient_getNetworkHandler ||
            !SDK::Classes.ClientPlayNetworkHandler_getPlayerList || !SDK::Classes.PlayerListEntry_getProfile) {
            return;
        }

        jobject networkHandler = env->CallObjectMethod(mc, SDK::Classes.MinecraftClient_getNetworkHandler);
        if (!networkHandler || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return;
        }

        jobject list = env->CallObjectMethod(networkHandler, SDK::Classes.ClientPlayNetworkHandler_getPlayerList);
        env->DeleteLocalRef(networkHandler);
        if (!list || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return;
        }

        Features::MC::ForEachIterable(env, list, [&](jobject entry) {
            jobject profile = env->CallObjectMethod(entry, SDK::Classes.PlayerListEntry_getProfile);
            if (!profile || env->ExceptionCheck()) {
                Features::MC::Clear(env);
                return;
            }

            const std::string current = ReadProfileName(env, profile);
            const std::string original = RememberProfileOriginal(env, profile, current);
            const bool isLocal = !localName.empty() && original == localName;
            const bool protect = isLocal || (gOnFriends.load() && Features::Friends::IsFriend(original));
            const std::string desired = gEnabled.load() && protect ? kProtectedName : original;
            if (!desired.empty() && (current != desired || protect)) {
                ProtectPlayerListEntry(env, entry, profile, desired, original);
            }

            env->DeleteLocalRef(profile);
        });

        env->DeleteLocalRef(list);
        Features::MC::Clear(env);
    }

    void ProtectNetworkProfile(JNIEnv* env, jobject mc) {
        if (!env || !mc || !SDK::Classes.MinecraftClient_getNetworkHandler || !SDK::Classes.ClientPlayNetworkHandler_getProfile) {
            return;
        }

        jobject networkHandler = env->CallObjectMethod(mc, SDK::Classes.MinecraftClient_getNetworkHandler);
        if (!networkHandler || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return;
        }

        jobject profile = env->CallObjectMethod(networkHandler, SDK::Classes.ClientPlayNetworkHandler_getProfile);
        env->DeleteLocalRef(networkHandler);
        if (!profile || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return;
        }

        ProtectProfile(env, profile, true);
        env->DeleteLocalRef(profile);
    }
}

namespace Features::NameProtect {
    void SetConfig(bool enabled, bool onFriends) {
        gEnabled = enabled;
        gOnFriends = onFriends;
    }

    void Tick(JNIEnv* env, jobject mc, jobject player, jobject world) {
        if (!env || !player) {
            return;
        }

        const std::string localName = ProtectEntity(env, player, true);
        ProtectClientProfile(env, mc);
        ProtectNetworkProfile(env, mc);
        ProtectPlayerList(env, mc, localName);
        if (gEnabled.load()) {
            const std::vector<std::string> aliases = ProtectedAliases(localName);
            ProtectChatHud(env, mc, aliases);
            ProtectScoreboard(env, world, aliases);
        }

        if (world) {
            jobject iterable = MC::GetEntityIterable(env, world);
            if (iterable) {
                MC::ForEachIterable(env, iterable, [&](jobject entity) {
                    const bool isLocal = env->IsSameObject(entity, player) == JNI_TRUE;
                    ProtectEntity(env, entity, isLocal);
                });

                env->DeleteLocalRef(iterable);
            }
        }

        MC::Clear(env);
    }

    bool Enabled() {
        return gEnabled.load();
    }

    std::string Apply(const std::string& name, bool isFriend, bool isLocal) {
        if (!gEnabled.load()) {
            return name;
        }
        if (isLocal || (gOnFriends.load() && isFriend)) {
            return kProtectedName;
        }
        return name;
    }

    std::string OriginalNameForId(int entityId, const std::string& fallback) {
        if (entityId == 0) {
            return fallback;
        }

        std::lock_guard<std::mutex> lock(gMutex);
        auto it = gOriginalNames.find(entityId);
        return it == gOriginalNames.end() ? fallback : it->second;
    }
}
