#include "discord_rpc.h"
#include "inventory_utils.h"
#include "minecraft_feature_utils.h"
#include "name_protect.h"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <string>
#include <windows.h>

namespace {
    constexpr const char* kClientId = "1500861868716064839";
    constexpr const char* kLargeImage = "https://i.ibb.co/W4FrtkTy/rpc.png";
    constexpr const char* kBuyUrl = "https://lumaware.ru/";
    constexpr const char* kDiscordUrl = "https://discord.gg/lumaclient";

    enum class IpcOpcode : std::uint32_t {
        Handshake = 0,
        Frame = 1,
        Close = 2,
    };

    std::atomic_bool gEnabled{ false };
    HANDLE gPipe = INVALID_HANDLE_VALUE;
    bool gHandshakeSent = false;
    unsigned long long gLastConnectMs = 0;
    unsigned long long gLastUpdateMs = 0;
    long long gStartedAt = 0;
    std::string gLastServer;
    std::string gLastError;

    unsigned long long NowMs() {
        return GetTickCount64();
    }

    long long UnixSeconds() {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
    }

    std::string EscapeJson(const std::string& value) {
        std::string out;
        out.reserve(value.size() + 8);
        for (char ch : value) {
            switch (ch) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    char buf[8]{};
                    std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(ch));
                    out += buf;
                } else {
                    out.push_back(ch);
                }
                break;
            }
        }
        return out;
    }

    bool WriteFrame(IpcOpcode opcode, const std::string& payload) {
        if (gPipe == INVALID_HANDLE_VALUE) {
            return false;
        }

        const std::uint32_t op = static_cast<std::uint32_t>(opcode);
        const std::uint32_t len = static_cast<std::uint32_t>(payload.size());
        DWORD written = 0;
        if (!WriteFile(gPipe, &op, sizeof(op), &written, nullptr) || written != sizeof(op)) {
            return false;
        }
        if (!WriteFile(gPipe, &len, sizeof(len), &written, nullptr) || written != sizeof(len)) {
            return false;
        }
        if (len > 0 && (!WriteFile(gPipe, payload.data(), len, &written, nullptr) || written != len)) {
            return false;
        }
        return true;
    }

    bool ReadFrame(IpcOpcode& opcode, std::string& payload) {
        payload.clear();
        opcode = IpcOpcode::Close;

        if (gPipe == INVALID_HANDLE_VALUE) {
            return false;
        }

        DWORD available = 0;
        if (!PeekNamedPipe(gPipe, nullptr, 0, nullptr, &available, nullptr)) {
            return false;
        }
        if (available < sizeof(std::uint32_t) * 2) {
            return false;
        }

        std::uint32_t op = 0;
        std::uint32_t len = 0;
        DWORD read = 0;
        if (!ReadFile(gPipe, &op, sizeof(op), &read, nullptr) || read != sizeof(op)) {
            return false;
        }
        if (!ReadFile(gPipe, &len, sizeof(len), &read, nullptr) || read != sizeof(len)) {
            return false;
        }
        if (len > 65536) {
            return false;
        }

        payload.resize(len);
        if (len > 0 && (!ReadFile(gPipe, payload.data(), len, &read, nullptr) || read != len)) {
            return false;
        }

        opcode = static_cast<IpcOpcode>(op);
        return true;
    }

    bool DrainPipe() {
        bool sawError = false;

        for (int i = 0; i < 8; ++i) {
            IpcOpcode opcode{};
            std::string payload;
            if (!ReadFrame(opcode, payload)) {
                break;
            }

            if (opcode == IpcOpcode::Close || payload.find("\"evt\":\"ERROR\"") != std::string::npos) {
                gLastError = payload;
                sawError = true;
            }
        }

        return !sawError;
    }

    void Disconnect(bool clearPresence) {
        if (gPipe != INVALID_HANDLE_VALUE) {
            if (clearPresence && gHandshakeSent) {
                const DWORD pid = GetCurrentProcessId();
                const std::string clear = std::string("{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":") +
                    std::to_string(pid) + ",\"activity\":null},\"nonce\":\"clear\"}";
                WriteFrame(IpcOpcode::Frame, clear);
            }
            CloseHandle(gPipe);
        }

        gPipe = INVALID_HANDLE_VALUE;
        gHandshakeSent = false;
        gLastUpdateMs = 0;
    }

    bool Connect() {
        if (gPipe != INVALID_HANDLE_VALUE) {
            return true;
        }

        const unsigned long long now = NowMs();
        if (now - gLastConnectMs < 5000) {
            return false;
        }
        gLastConnectMs = now;

        for (int i = 0; i < 10; ++i) {
            char pipeNameA[64]{};
            char pipeNameB[64]{};
            std::snprintf(pipeNameA, sizeof(pipeNameA), "\\\\.\\pipe\\discord-ipc-%d", i);
            std::snprintf(pipeNameB, sizeof(pipeNameB), "\\\\?\\pipe\\discord-ipc-%d", i);

            HANDLE pipe = CreateFileA(pipeNameA, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (pipe == INVALID_HANDLE_VALUE) {
                pipe = CreateFileA(pipeNameB, GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
                if (pipe == INVALID_HANDLE_VALUE) {
                    continue;
                }
            }

            gPipe = pipe;
            const std::string handshake = std::string("{\"v\":1,\"client_id\":\"") + kClientId + "\"}";
            if (!WriteFrame(IpcOpcode::Handshake, handshake)) {
                Disconnect(false);
                continue;
            }

            gHandshakeSent = true;
            Sleep(15);
            DrainPipe();
            return true;
        }

        return false;
    }

    std::string ReadServer(JNIEnv* env, jobject mc) {
        if (!env || !mc || !SDK::Classes.MinecraftClient_getCurrentServerEntry || !SDK::Classes.ServerInfo_address) {
            return "Singleplayer";
        }

        jobject server = env->CallObjectMethod(mc, SDK::Classes.MinecraftClient_getCurrentServerEntry);
        if (!server || env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return "Singleplayer";
        }

        jstring address = static_cast<jstring>(env->GetObjectField(server, SDK::Classes.ServerInfo_address));
        env->DeleteLocalRef(server);
        if (env->ExceptionCheck()) {
            Features::MC::Clear(env);
            return "Singleplayer";
        }

        std::string value = Features::Inventory::ReadJavaString(env, address, "Singleplayer");
        return value.empty() ? "Singleplayer" : value;
    }

    std::string ReadPlayerName(JNIEnv* env, jobject player) {
        if (!env || !player) {
            return "Player";
        }

        std::string name = Features::MC::GetPlayerName(env, player);
        const int id = Features::Inventory::GetEntityId(env, player);
        name = Features::NameProtect::OriginalNameForId(id, name);
        return name.empty() ? "Player" : name;
    }

    enum class ButtonMode {
        Objects,
        LabelsWithMetadata,
        None
    };

    std::string BuildPresencePayload(const std::string& playerName, const std::string& server, ButtonMode buttonMode) {
        const DWORD pid = GetCurrentProcessId();
        std::string payload =
            std::string("{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":") + std::to_string(pid) +
            ",\"activity\":{\"application_id\":\"" + kClientId +
            "\",\"type\":0,\"details\":\"User: " + EscapeJson(playerName) +
            "\",\"state\":\"In: " + EscapeJson(server) +
            "\",\"timestamps\":{\"start\":" + std::to_string(gStartedAt) +
            "},\"assets\":{\"large_image\":\"" + kLargeImage +
            "\",\"large_text\":\"LumaWare\"}";

        if (buttonMode == ButtonMode::Objects) {
            payload += std::string(",\"buttons\":[{\"label\":\"Buy\",\"url\":\"") + kBuyUrl +
                "\"},{\"label\":\"Discord\",\"url\":\"" + kDiscordUrl + "\"}]";
        } else if (buttonMode == ButtonMode::LabelsWithMetadata) {
            payload += std::string(",\"buttons\":[\"Buy\",\"Discord\"],\"metadata\":{\"button_urls\":[\"") +
                kBuyUrl + "\",\"" + kDiscordUrl + "\"]}";
        }

        payload += std::string(",\"instance\":true}},\"nonce\":\"legit-") +
            std::to_string(NowMs()) + "\"}";
        return payload;
    }

    bool SendPayload(const std::string& payload) {
        if (!Connect()) {
            return false;
        }

        if (!WriteFrame(IpcOpcode::Frame, payload)) {
            Disconnect(false);
            return false;
        }
        Sleep(10);
        if (!DrainPipe()) {
            Disconnect(false);
            return false;
        }
        return true;
    }

    bool SendPresence(const std::string& playerName, const std::string& server) {
        if (!Connect()) {
            return false;
        }
        DrainPipe();

        if (gStartedAt == 0 || (!gLastServer.empty() && gLastServer != server)) {
            gStartedAt = UnixSeconds();
        }
        gLastServer = server;

        if (SendPayload(BuildPresencePayload(playerName, server, ButtonMode::Objects))) {
            return true;
        }

        Disconnect(false);
        gLastConnectMs = 0;
        if (SendPayload(BuildPresencePayload(playerName, server, ButtonMode::LabelsWithMetadata))) {
            return true;
        }

        Disconnect(false);
        gLastConnectMs = 0;
        return SendPayload(BuildPresencePayload(playerName, server, ButtonMode::None));
    }
}

namespace Features::DiscordRPC {
    void SetConfig(bool enabled) {
        const bool wasEnabled = gEnabled.exchange(enabled);
        if (!wasEnabled && enabled) {
            gStartedAt = 0;
            gLastServer.clear();
            gLastUpdateMs = 0;
            gLastConnectMs = 0;
            gLastError.clear();
        }
        if (wasEnabled && !enabled) {
            Disconnect(true);
            gStartedAt = 0;
            gLastServer.clear();
            gLastError.clear();
        }
    }

    void Tick(JNIEnv* env, jobject mc, jobject player) {
        if (!gEnabled.load()) {
            return;
        }

        if (!mc || !player) {
            Disconnect(true);
            gStartedAt = 0;
            gLastServer.clear();
            return;
        }

        const unsigned long long now = NowMs();
        if (gPipe != INVALID_HANDLE_VALUE) {
            DrainPipe();
        }
        if (now - gLastUpdateMs < 10000 && gPipe != INVALID_HANDLE_VALUE) {
            return;
        }

        const std::string playerName = ReadPlayerName(env, player);
        const std::string server = ReadServer(env, mc);
        if (SendPresence(playerName, server)) {
            gLastUpdateMs = now;
        }
    }

    void Shutdown() {
        Disconnect(true);
        gEnabled = false;
    }
}
