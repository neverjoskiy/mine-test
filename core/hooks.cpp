#define IMGUI_DEFINE_MATH_OPERATORS
#include "hooks.h"
#include <MinHook.h>
#include <gl/GL.h>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_win32.h>
#include "headers/includes.h"
#include "data/images.h"
#include "jni_bridge.h"
#include "../sdk/minecraft_classes.h"
#include "../features/movement/autosprint.h"
#include "../features/misc/auto_respawn.h"
#include "../features/misc/mcf.h"
#include "../features/misc/discord_rpc.h"
#include "../features/combat/hitbox.h"
#include "../features/misc/item_scroller.h"
#include "../features/render/name_protect.h"
#include "../features/combat/no_friend_damage.h"
#include "../features/render/world_tweaks.h"
#include "../features/render/no_render.h"
#include "../features/movement/gui_move.h"
#include "../features/render/esp.h"
#include "../features/common/feature_runtime.h"
#include "../features/render/hud.h"
#include "../features/common/input_utils.h"
#include "../features/combat/shift_tap.h"
#include "../features/render/storage_esp.h"
#include "../features/combat/triggerbot.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Hooks {
    namespace {
        using GLFWwindow = void;
        using GlfwGetCurrentContext_t = GLFWwindow* (__cdecl*)();
        using GlfwSetInputMode_t = void(__cdecl*)(GLFWwindow*, int, int);
        using GlfwGetInputMode_t = int(__cdecl*)(GLFWwindow*, int);
        using GlfwRawMouseMotionSupported_t = int(__cdecl*)();
        using GlBindBuffer_t = void(WINAPI*)(GLenum target, GLuint buffer);
        using GlBindVertexArray_t = void(WINAPI*)(GLuint array);
        using GlBindSampler_t = void(WINAPI*)(GLuint unit, GLuint sampler);
        using GlUseProgram_t = void(WINAPI*)(GLuint program);
        using GlActiveTexture_t = void(WINAPI*)(GLenum texture);
        using GlBindFramebuffer_t = void(WINAPI*)(GLenum target, GLuint framebuffer);

        constexpr int GLFW_CURSOR = 0x00033001;
        constexpr int GLFW_RAW_MOUSE_MOTION = 0x00033005;
        constexpr int GLFW_CURSOR_NORMAL = 0x00034001;
        constexpr int GLFW_CURSOR_DISABLED = 0x00034003;
        constexpr int GLFW_FALSE = 0;
        constexpr GLenum GL_ARRAY_BUFFER_LOCAL = 0x8892;
        constexpr GLenum GL_ELEMENT_ARRAY_BUFFER_LOCAL = 0x8893;
        constexpr GLenum GL_ARRAY_BUFFER_BINDING_LOCAL = 0x8894;
        constexpr GLenum GL_ELEMENT_ARRAY_BUFFER_BINDING_LOCAL = 0x8895;
        constexpr GLenum GL_VERTEX_ARRAY_BINDING_LOCAL = 0x85B5;
        constexpr GLenum GL_CURRENT_PROGRAM_LOCAL = 0x8B8D;
        constexpr GLenum GL_ACTIVE_TEXTURE_LOCAL = 0x84E0;
        constexpr GLenum GL_TEXTURE0_LOCAL = 0x84C0;
        constexpr GLenum GL_SAMPLER_BINDING_LOCAL = 0x8919;
        constexpr GLenum GL_PIXEL_PACK_BUFFER_LOCAL = 0x88EB;
        constexpr GLenum GL_PIXEL_UNPACK_BUFFER_LOCAL = 0x88EC;
        constexpr GLenum GL_PIXEL_PACK_BUFFER_BINDING_LOCAL = 0x88ED;
        constexpr GLenum GL_PIXEL_UNPACK_BUFFER_BINDING_LOCAL = 0x88EF;
        constexpr GLenum GL_PACK_ALIGNMENT_LOCAL = 0x0D05;
        constexpr GLenum GL_PACK_ROW_LENGTH_LOCAL = 0x0D02;
        constexpr GLenum GL_UNPACK_ALIGNMENT_LOCAL = 0x0CF5;
        constexpr GLenum GL_UNPACK_ROW_LENGTH_LOCAL = 0x0CF2;
        constexpr GLenum GL_DRAW_FRAMEBUFFER_LOCAL = 0x8CA9;
        constexpr GLenum GL_READ_FRAMEBUFFER_LOCAL = 0x8CA8;
        constexpr GLenum GL_DRAW_FRAMEBUFFER_BINDING_LOCAL = 0x8CA6;
        constexpr GLenum GL_READ_FRAMEBUFFER_BINDING_LOCAL = 0x8CAA;

        HWND gWindow = nullptr;
        WNDPROC gOriginalWndProc = nullptr;
        bool gInitialized = false;
        std::atomic_bool gShuttingDown{ false };
        std::atomic_bool gShutdownRequested{ false };
        std::atomic_bool gRenderShutdownComplete{ false };
        thread_local bool gInsideSwapBuffers = false;
        void* gSwapBuffersTarget = nullptr;
        void* gWglSwapBuffersTarget = nullptr;
        void* gGlfwSwapBuffersTarget = nullptr;
        GLuint gImageTexture = 0;
        std::atomic_bool gSdkReady{ false };
        std::atomic_int gActiveSwapCalls{ 0 };
        int gSkipUiFramesAfterDpi = 0;

        HMODULE gGlfwModule = nullptr;
        GlfwGetCurrentContext_t glfwGetCurrentContextFn = nullptr;
        GlfwSetInputMode_t glfwSetInputModeFn = nullptr;
        GlfwGetInputMode_t glfwGetInputModeFn = nullptr;
        GlfwRawMouseMotionSupported_t glfwRawMouseMotionSupportedFn = nullptr;
        GlBindBuffer_t glBindBufferFn = nullptr;
        GlBindVertexArray_t glBindVertexArrayFn = nullptr;
        GlBindSampler_t glBindSamplerFn = nullptr;
        GlUseProgram_t glUseProgramFn = nullptr;
        GlActiveTexture_t glActiveTextureFn = nullptr;
        GlBindFramebuffer_t glBindFramebufferFn = nullptr;
        bool gCursorReleased = false;
        bool gSavedCursorState = false;
        int gSavedCursorMode = GLFW_CURSOR_DISABLED;
        int gSavedRawMouse = GLFW_FALSE;

        bool IsMinecraftForegroundWindow() {
            if (!gWindow || !IsWindow(gWindow)) {
                return false;
            }

            HWND foreground = GetForegroundWindow();
            if (!foreground) {
                return false;
            }

            if (foreground == gWindow || IsChild(gWindow, foreground) || IsChild(foreground, gWindow)) {
                return true;
            }

            return GetAncestor(foreground, GA_ROOT) == GetAncestor(gWindow, GA_ROOT);
        }

        void PumpMenuMouseInput() {
            if (!gWindow || !IsMinecraftForegroundWindow() || (!var->gui.menu_opened && var->gui.menu_alpha <= 0.03f)) {
                return;
            }

            POINT cursor{};
            if (GetCursorPos(&cursor) && ScreenToClient(gWindow, &cursor)) {
                ImGui::GetIO().AddMousePosEvent(static_cast<float>(cursor.x), static_cast<float>(cursor.y));
            }

            ImGuiIO& io = ImGui::GetIO();
            io.AddMouseButtonEvent(0, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
            io.AddMouseButtonEvent(1, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
            io.AddMouseButtonEvent(2, (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0);
        }

        bool IsMouseMessage(UINT msg) {
            return msg >= WM_MOUSEFIRST && msg <= WM_MOUSELAST;
        }

        bool IsKeyboardMessage(UINT msg) {
            return msg >= WM_KEYFIRST && msg <= WM_KEYLAST;
        }

        bool LoadGlfw() {
            if (glfwGetCurrentContextFn && glfwSetInputModeFn && glfwGetInputModeFn) {
                return true;
            }

            gGlfwModule = GetModuleHandleA("glfw.dll");
            if (!gGlfwModule) {
                return false;
            }

            glfwGetCurrentContextFn = reinterpret_cast<GlfwGetCurrentContext_t>(GetProcAddress(gGlfwModule, "glfwGetCurrentContext"));
            glfwSetInputModeFn = reinterpret_cast<GlfwSetInputMode_t>(GetProcAddress(gGlfwModule, "glfwSetInputMode"));
            glfwGetInputModeFn = reinterpret_cast<GlfwGetInputMode_t>(GetProcAddress(gGlfwModule, "glfwGetInputMode"));
            glfwRawMouseMotionSupportedFn = reinterpret_cast<GlfwRawMouseMotionSupported_t>(GetProcAddress(gGlfwModule, "glfwRawMouseMotionSupported"));

            return glfwGetCurrentContextFn && glfwSetInputModeFn && glfwGetInputModeFn;
        }

        bool LoadGlfwForSwapHook() {
            for (int attempt = 0; attempt < 50; ++attempt) {
                if (LoadGlfw()) {
                    return true;
                }
                Sleep(100);
            }
            return false;
        }

        void WriteHookDebug(const char* message) {
            CreateDirectoryA("C:\\Nvidia", nullptr);
            CreateDirectoryA("C:\\Nvidia\\Legit", nullptr);
            FILE* file = nullptr;
            if (fopen_s(&file, "C:\\Nvidia\\Legit\\render_hook.log", "a") != 0 || !file) {
                return;
            }
            std::fprintf(file, "%llu %s\n", static_cast<unsigned long long>(GetTickCount64()), message);
            std::fclose(file);
        }

        template <typename T>
        T LoadGlProc(const char* name) {
            PROC proc = wglGetProcAddress(name);
            const auto value = reinterpret_cast<intptr_t>(proc);
            if (proc && value != 1 && value != 2 && value != 3 && value != -1) {
                return reinterpret_cast<T>(proc);
            }

            HMODULE opengl32 = GetModuleHandleA("opengl32.dll");
            if (opengl32) {
                return reinterpret_cast<T>(GetProcAddress(opengl32, name));
            }

            return nullptr;
        }

        GlBindBuffer_t LoadGlBindBuffer() {
            if (glBindBufferFn) {
                return glBindBufferFn;
            }

            glBindBufferFn = LoadGlProc<GlBindBuffer_t>("glBindBuffer");
            return glBindBufferFn;
        }

        GlBindVertexArray_t LoadGlBindVertexArray() {
            if (!glBindVertexArrayFn) {
                glBindVertexArrayFn = LoadGlProc<GlBindVertexArray_t>("glBindVertexArray");
            }
            return glBindVertexArrayFn;
        }

        GlBindSampler_t LoadGlBindSampler() {
            if (!glBindSamplerFn) {
                glBindSamplerFn = LoadGlProc<GlBindSampler_t>("glBindSampler");
            }
            return glBindSamplerFn;
        }

        GlUseProgram_t LoadGlUseProgram() {
            if (!glUseProgramFn) {
                glUseProgramFn = LoadGlProc<GlUseProgram_t>("glUseProgram");
            }
            return glUseProgramFn;
        }

        GlActiveTexture_t LoadGlActiveTexture() {
            if (!glActiveTextureFn) {
                glActiveTextureFn = LoadGlProc<GlActiveTexture_t>("glActiveTexture");
            }
            return glActiveTextureFn;
        }

        GlBindFramebuffer_t LoadGlBindFramebuffer() {
            if (!glBindFramebufferFn) {
                glBindFramebufferFn = LoadGlProc<GlBindFramebuffer_t>("glBindFramebuffer");
            }
            return glBindFramebufferFn;
        }

        void RestoreCapability(GLenum capability, GLboolean enabled) {
            if (enabled) {
                glEnable(capability);
            } else {
                glDisable(capability);
            }
        }

        struct ScopedMinecraftGlState {
            GlBindBuffer_t bindBuffer = nullptr;
            GlBindVertexArray_t bindVertexArray = nullptr;
            GlBindSampler_t bindSampler = nullptr;
            GlUseProgram_t useProgram = nullptr;
            GlActiveTexture_t activeTexture = nullptr;
            GlBindFramebuffer_t bindFramebuffer = nullptr;

            GLint arrayBuffer = 0;
            GLint elementArrayBuffer = 0;
            GLint vertexArray = 0;
            GLint program = 0;
            GLint activeTextureUnit = GL_TEXTURE0_LOCAL;
            GLint texture2D = 0;
            GLint sampler = 0;
            GLint pixelPackBuffer = 0;
            GLint pixelUnpackBuffer = 0;
            GLint packAlignment = 4;
            GLint packRowLength = 0;
            GLint unpackAlignment = 4;
            GLint unpackRowLength = 0;
            GLint drawFramebuffer = 0;
            GLint readFramebuffer = 0;
            GLint viewport[4]{};
            GLint scissorBox[4]{};
            GLboolean blend = GL_FALSE;
            GLboolean cullFace = GL_FALSE;
            GLboolean depthTest = GL_FALSE;
            GLboolean stencilTest = GL_FALSE;
            GLboolean scissorTest = GL_FALSE;

            ScopedMinecraftGlState() {
                bindBuffer = LoadGlBindBuffer();
                bindVertexArray = LoadGlBindVertexArray();
                bindSampler = LoadGlBindSampler();
                useProgram = LoadGlUseProgram();
                activeTexture = LoadGlActiveTexture();
                bindFramebuffer = LoadGlBindFramebuffer();

                glGetIntegerv(GL_ARRAY_BUFFER_BINDING_LOCAL, &arrayBuffer);
                glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING_LOCAL, &elementArrayBuffer);
                if (bindVertexArray) {
                    glGetIntegerv(GL_VERTEX_ARRAY_BINDING_LOCAL, &vertexArray);
                }
                if (useProgram) {
                    glGetIntegerv(GL_CURRENT_PROGRAM_LOCAL, &program);
                }
                if (activeTexture) {
                    glGetIntegerv(GL_ACTIVE_TEXTURE_LOCAL, &activeTextureUnit);
                    activeTexture(GL_TEXTURE0_LOCAL);
                }
                glGetIntegerv(GL_TEXTURE_BINDING_2D, &texture2D);
                if (bindSampler) {
                    glGetIntegerv(GL_SAMPLER_BINDING_LOCAL, &sampler);
                }
                glGetIntegerv(GL_PIXEL_PACK_BUFFER_BINDING_LOCAL, &pixelPackBuffer);
                glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING_LOCAL, &pixelUnpackBuffer);
                glGetIntegerv(GL_PACK_ALIGNMENT_LOCAL, &packAlignment);
                glGetIntegerv(GL_PACK_ROW_LENGTH_LOCAL, &packRowLength);
                glGetIntegerv(GL_UNPACK_ALIGNMENT_LOCAL, &unpackAlignment);
                glGetIntegerv(GL_UNPACK_ROW_LENGTH_LOCAL, &unpackRowLength);
                if (bindFramebuffer) {
                    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING_LOCAL, &drawFramebuffer);
                    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING_LOCAL, &readFramebuffer);
                }
                glGetIntegerv(GL_VIEWPORT, viewport);
                glGetIntegerv(GL_SCISSOR_BOX, scissorBox);

                blend = glIsEnabled(GL_BLEND);
                cullFace = glIsEnabled(GL_CULL_FACE);
                depthTest = glIsEnabled(GL_DEPTH_TEST);
                stencilTest = glIsEnabled(GL_STENCIL_TEST);
                scissorTest = glIsEnabled(GL_SCISSOR_TEST);

                if (bindBuffer) {
                    bindBuffer(GL_PIXEL_PACK_BUFFER_LOCAL, 0);
                    bindBuffer(GL_PIXEL_UNPACK_BUFFER_LOCAL, 0);
                }
                glPixelStorei(GL_PACK_ALIGNMENT_LOCAL, 1);
                glPixelStorei(GL_PACK_ROW_LENGTH_LOCAL, 0);
                glPixelStorei(GL_UNPACK_ALIGNMENT_LOCAL, 1);
                glPixelStorei(GL_UNPACK_ROW_LENGTH_LOCAL, 0);
            }

            ~ScopedMinecraftGlState() {
                if (bindFramebuffer) {
                    bindFramebuffer(GL_DRAW_FRAMEBUFFER_LOCAL, static_cast<GLuint>(drawFramebuffer));
                    bindFramebuffer(GL_READ_FRAMEBUFFER_LOCAL, static_cast<GLuint>(readFramebuffer));
                }

                if (useProgram) {
                    useProgram(static_cast<GLuint>(program));
                }

                if (activeTexture) {
                    activeTexture(GL_TEXTURE0_LOCAL);
                }
                glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(texture2D));
                if (bindSampler) {
                    bindSampler(0, static_cast<GLuint>(sampler));
                }
                if (activeTexture) {
                    activeTexture(static_cast<GLenum>(activeTextureUnit));
                }

                if (bindVertexArray) {
                    bindVertexArray(static_cast<GLuint>(vertexArray));
                }
                if (bindBuffer) {
                    bindBuffer(GL_ARRAY_BUFFER_LOCAL, static_cast<GLuint>(arrayBuffer));
                    bindBuffer(GL_ELEMENT_ARRAY_BUFFER_LOCAL, static_cast<GLuint>(elementArrayBuffer));
                    bindBuffer(GL_PIXEL_PACK_BUFFER_LOCAL, static_cast<GLuint>(pixelPackBuffer));
                    bindBuffer(GL_PIXEL_UNPACK_BUFFER_LOCAL, static_cast<GLuint>(pixelUnpackBuffer));
                }

                glPixelStorei(GL_PACK_ALIGNMENT_LOCAL, packAlignment);
                glPixelStorei(GL_PACK_ROW_LENGTH_LOCAL, packRowLength);
                glPixelStorei(GL_UNPACK_ALIGNMENT_LOCAL, unpackAlignment);
                glPixelStorei(GL_UNPACK_ROW_LENGTH_LOCAL, unpackRowLength);
                glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
                glScissor(scissorBox[0], scissorBox[1], scissorBox[2], scissorBox[3]);

                RestoreCapability(GL_BLEND, blend);
                RestoreCapability(GL_CULL_FACE, cullFace);
                RestoreCapability(GL_DEPTH_TEST, depthTest);
                RestoreCapability(GL_STENCIL_TEST, stencilTest);
                RestoreCapability(GL_SCISSOR_TEST, scissorTest);
            }
        };

        void ApplyCursorMode(bool menuOpen) {
            if (!LoadGlfw()) {
                return;
            }

            GLFWwindow* window = glfwGetCurrentContextFn();
            if (!window) {
                return;
            }

            if (menuOpen) {
                if (!gCursorReleased) {
                    gSavedCursorMode = glfwGetInputModeFn(window, GLFW_CURSOR);
                    gSavedRawMouse = glfwGetInputModeFn(window, GLFW_RAW_MOUSE_MOTION);
                    gSavedCursorState = true;
                    gCursorReleased = true;

                    glfwSetInputModeFn(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    if (!glfwRawMouseMotionSupportedFn || glfwRawMouseMotionSupportedFn()) {
                        glfwSetInputModeFn(window, GLFW_RAW_MOUSE_MOTION, GLFW_FALSE);
                    }
                }
                return;
            }

            if (gCursorReleased) {
                glfwSetInputModeFn(window, GLFW_CURSOR, gSavedCursorState ? gSavedCursorMode : GLFW_CURSOR_DISABLED);
                if (!glfwRawMouseMotionSupportedFn || glfwRawMouseMotionSupportedFn()) {
                    glfwSetInputModeFn(window, GLFW_RAW_MOUSE_MOTION, gSavedCursorState ? gSavedRawMouse : GLFW_FALSE);
                }
                gCursorReleased = false;
                gSavedCursorState = false;
            }
        }

        ImTextureID CreateTextureFromMemory(const unsigned char* data, int size) {
            int width = 0;
            int height = 0;
            int channels = 0;
            stbi_uc* pixels = stbi_load_from_memory(data, size, &width, &height, &channels, 4);
            if (!pixels) {
                return nullptr;
            }

            GLint previousTexture = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
            GLint previousPixelUnpackBuffer = 0;
            GlBindBuffer_t bindBuffer = LoadGlBindBuffer();
            if (bindBuffer) {
                glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING_LOCAL, &previousPixelUnpackBuffer);
                bindBuffer(GL_PIXEL_UNPACK_BUFFER_LOCAL, 0);
            }
            GLint previousUnpackAlignment = 0;
            GLint previousUnpackRowLength = 0;
            glGetIntegerv(GL_UNPACK_ALIGNMENT_LOCAL, &previousUnpackAlignment);
            glGetIntegerv(GL_UNPACK_ROW_LENGTH_LOCAL, &previousUnpackRowLength);
            glPixelStorei(GL_UNPACK_ALIGNMENT_LOCAL, 1);
            glPixelStorei(GL_UNPACK_ROW_LENGTH_LOCAL, 0);

            GLuint texture = 0;
            glGenTextures(1, &texture);
            glBindTexture(GL_TEXTURE_2D, texture);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(previousTexture));
            glPixelStorei(GL_UNPACK_ALIGNMENT_LOCAL, previousUnpackAlignment);
            glPixelStorei(GL_UNPACK_ROW_LENGTH_LOCAL, previousUnpackRowLength);
            if (bindBuffer) {
                bindBuffer(GL_PIXEL_UNPACK_BUFFER_LOCAL, static_cast<GLuint>(previousPixelUnpackBuffer));
            }

            stbi_image_free(pixels);
            return reinterpret_cast<ImTextureID>(static_cast<intptr_t>(texture));
        }

        void LoadGuiTextures() {
            if (!var->gui.image_texture) {
                var->gui.image_texture = CreateTextureFromMemory(image_data, sizeof(image_data));
                gImageTexture = static_cast<GLuint>(reinterpret_cast<intptr_t>(var->gui.image_texture));
            }
        }

        void PrimeFonts() {
            const float iconSizes[] = { 7.f, 8.f, 10.f, 12.f, 16.f, 22.f };
            const float textSizes[] = { 11.f, 12.f, 13.f, 14.f, 15.f, 16.f, 17.f };

            for (float size : iconSizes) {
                font->get(icon_data, size);
            }

            for (float size : textSizes) {
                font->get(inter_medium_data, size);
            }
        }

        void SyncFeatureState() {
            Features::AutoSprint::Enabled = cfg->get<checkbox_t>("Sprint").callback;
            Features::HitBox::SetConfig(
                cfg->get<checkbox_t>("HitBox").callback,
                0.f,
                cfg->get<slider_float_t>("HitBox X Position").callback,
                false
            );
            Features::TriggerBot::SetConfig(
                cfg->get<checkbox_t>("TriggerBot").callback,
                cfg->get<slider_float_t>("TriggerBot Distance").callback,
                cfg->get<multi_dropdown_t>("TriggerBot Targets").callback,
                cfg->get<multi_dropdown_t>("TriggerBot No Hit").callback,
                cfg->get<checkbox_t>("Smart Crits").callback
            );
            Features::WorldTweaks::SetFullBright(cfg->get<checkbox_t>("WorldTweaks").callback && cfg->get<checkbox_t>("FullBright").callback);
            Features::WorldTweaks::SetNoRain(cfg->get<checkbox_t>("WorldTweaks").callback && cfg->get<checkbox_t>("No Rain").callback);
            Features::WorldTweaks::SetWorldTime(cfg->get<checkbox_t>("World Time").callback, cfg->get<slider_float_t>("World Time Hour").callback);
            Features::NoRender::SetEnabled(cfg->get<checkbox_t>("NoRender").callback);
            Features::NoRender::SetTargets(cfg->get<multi_dropdown_t>("NoRender Targets").callback);
            Features::GuiMove::Enabled = cfg->get<checkbox_t>("GuiMove").callback;
            Features::ShiftTap::Enabled = cfg->get<checkbox_t>("ShiftTap").callback;
            Features::NoFriendDamage::Enabled = cfg->get<checkbox_t>("NoFriendDamage").callback;
            Features::AutoRespawn::Enabled = cfg->get<checkbox_t>("AutoRespawn").callback;
            Features::MCF::SetConfig(
                cfg->get<checkbox_t>("MCF").callback
            );
            Features::ItemScroller::SetConfig(
                cfg->get<checkbox_t>("ItemScroller").callback,
                cfg->get<slider_int_t>("ItemScroller Delay").callback
            );
            Features::NameProtect::SetConfig(
                cfg->get<checkbox_t>("NameProtect").callback,
                cfg->get<checkbox_t>("NameProtect On Friends").callback
            );
            Features::DiscordRPC::SetConfig(cfg->get<checkbox_t>("DiscordRPC").callback);
            Features::ESP::SetConfig(
                cfg->get<checkbox_t>("ESP").callback,
                cfg->get<multi_dropdown_t>("ESP Elements").callback,
                cfg->get<dropdown_t>("ESP Box Style").callback,
                cfg->get<color_edit_t>("ESP 3D Box Color").color
            );
            Features::StorageESP::SetConfig(
                cfg->get<checkbox_t>("StorageESP").callback,
                cfg->get<dropdown_t>("StorageESP Mode").callback,
                {
                    cfg->get<color_edit_t>("StorageESP Chest").color,
                    cfg->get<color_edit_t>("StorageESP EnderChest").color,
                    cfg->get<color_edit_t>("StorageESP Shulker").color,
                    cfg->get<color_edit_t>("StorageESP Barrel").color,
                    cfg->get<color_edit_t>("StorageESP Furnace").color,
                    cfg->get<color_edit_t>("StorageESP Dispenser").color,
                    cfg->get<color_edit_t>("StorageESP Dropper").color,
                    cfg->get<color_edit_t>("StorageESP Hopper").color
                }
            );
        }

        void ClearOneShotModuleToggles() {
            cfg->get<checkbox_t>("MCF").callback = false;
        }

        void ClearJniException(JNIEnv* env) {
            if (env && env->ExceptionCheck()) {
                env->ExceptionClear();
            }
        }

        bool ShouldRenderGameplayOverlays() {
            if (!SDK::Classes.Ready || !SDK::Classes.MinecraftClient_getInstance || !SDK::Classes.MinecraftClient_currentScreen) {
                return true;
            }

            JNIEnv* env = JNIBridge::GetEnv();
            if (!env) {
                return true;
            }

            jobject mc = env->CallStaticObjectMethod(SDK::Classes.MinecraftClient, SDK::Classes.MinecraftClient_getInstance);
            if (!mc) {
                ClearJniException(env);
                return true;
            }

            jobject screen = env->GetObjectField(mc, SDK::Classes.MinecraftClient_currentScreen);
            env->DeleteLocalRef(mc);
            if (!screen) {
                ClearJniException(env);
                return true;
            }

            const bool chatOpen = SDK::Classes.ChatScreen && env->IsInstanceOf(screen, SDK::Classes.ChatScreen) == JNI_TRUE;
            ClearJniException(env);
            env->DeleteLocalRef(screen);
            return chatOpen;
        }

        void RenderOverlayLayer() {
            const ImVec2 display = ImGui::GetIO().DisplaySize;
            if (display.x <= 1.f || display.y <= 1.f) {
                return;
            }

            ImGui::SetNextWindowPos(ImVec2(0.f, 0.f), ImGuiCond_Always);
            ImGui::SetNextWindowSize(display, ImGuiCond_Always);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));

            const ImGuiWindowFlags overlayFlags =
                ImGuiWindowFlags_NoDecoration |
                ImGuiWindowFlags_NoInputs |
                ImGuiWindowFlags_NoSavedSettings |
                ImGuiWindowFlags_NoBackground |
                ImGuiWindowFlags_NoNav |
                ImGuiWindowFlags_NoMove |
                ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoScrollWithMouse |
                ImGuiWindowFlags_NoBringToFrontOnFocus;

            ImGui::Begin("##legit_overlay_layer", nullptr, overlayFlags);
            if (ShouldRenderGameplayOverlays()) {
                Features::ESP::RenderOverlay();
                Features::StorageESP::RenderOverlay();
                Features::HitBox::RenderOverlay();
                Features::HUD::Render();
                gui->render_hud();
            }
            ImGui::End();

            ImGui::PopStyleColor();
            ImGui::PopStyleVar(2);
        }

        void EnsureSdkCache() {
            if (gSdkReady.load()) {
                return;
            }

            JNIEnv* env = JNIBridge::GetEnv();
            if (env && SDK::CacheAll(env)) {
                gSdkReady = true;
            }
        }

        bool TryEnableHook(void* target, LPVOID detour, LPVOID* original) {
            if (!target) {
                return false;
            }

            const MH_STATUS createStatus = MH_CreateHook(target, detour, original);
            if (createStatus != MH_OK && createStatus != MH_ERROR_ALREADY_CREATED) {
                return false;
            }

            const MH_STATUS enableStatus = MH_EnableHook(target);
            if (enableStatus == MH_OK || enableStatus == MH_ERROR_ENABLED) {
                return true;
            }

            MH_RemoveHook(target);
            if (original) {
                *original = nullptr;
            }
            return false;
        }

        LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
            if (gInitialized && ImGui::GetCurrentContext()) {
                const bool handledByImGui = ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam) != 0;

                if (var->gui.menu_opened) {
                    if (handledByImGui || IsMouseMessage(msg) || IsKeyboardMessage(msg) || msg == WM_INPUT) {
                        return true;
                    }
                }

                if (!var->gui.menu_opened && (msg == WM_LBUTTONDOWN || msg == WM_LBUTTONDBLCLK) && Features::NoFriendDamage::ShouldCancelCurrentAttack()) {
                    return true;
                }
            }

            return gOriginalWndProc ? CallWindowProcA(gOriginalWndProc, hWnd, msg, wParam, lParam) : DefWindowProcA(hWnd, msg, wParam, lParam);
        }

        bool InitImGui(HDC hdc) {
            gWindow = WindowFromDC(hdc);
            if (!gWindow) {
                return false;
            }

            IMGUI_CHECKVERSION();
            ImGui::CreateContext();

            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.IniFilename = nullptr;

            ImGui::StyleColorsDark();
            ImGui_ImplWin32_Init(gWindow);
            ImGui_ImplOpenGL3_Init("#version 150");

            gui->initialize();
            PrimeFonts();
            LoadGuiTextures();

            gOriginalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(gWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProc)));
            gInitialized = true;
            gRenderShutdownComplete = false;
            return true;
        }

        void ShutdownGuiOnRenderThread() {
            ApplyCursorMode(false);
            Features::DiscordRPC::Shutdown();

            if (gOriginalWndProc && gWindow && IsWindow(gWindow)) {
                SetWindowLongPtrA(gWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(gOriginalWndProc));
                gOriginalWndProc = nullptr;
            }

            if (gInitialized && ImGui::GetCurrentContext()) {
                if (gImageTexture) {
                    glDeleteTextures(1, &gImageTexture);
                    gImageTexture = 0;
                    var->gui.image_texture = nullptr;
                }

                ImGui_ImplOpenGL3_Shutdown();
                ImGui_ImplWin32_Shutdown();
                ImGui::DestroyContext();
                gInitialized = false;
            }

            gShuttingDown = true;
            gRenderShutdownComplete = true;
        }

        void RenderFrame(HDC hdc) {
            if (!gInitialized && !InitImGui(hdc)) {
                return;
            }

            ScopedMinecraftGlState minecraftGlState;
            const bool dpiWasChanged = var->gui.dpi_changed;
            font->update();
            if (dpiWasChanged) {
                gSkipUiFramesAfterDpi = 2;
                return;
            }
            if (gSkipUiFramesAfterDpi > 0) {
                --gSkipUiFramesAfterDpi;
                return;
            }

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplWin32_NewFrame();
            PumpMenuMouseInput();
            ImGui::NewFrame();

            const bool minecraftFocused = IsMinecraftForegroundWindow();
            Features::Input::SetForegroundActive(minecraftFocused);

            if (minecraftFocused && (GetAsyncKeyState(VK_RSHIFT) & 1)) {
                var->gui.menu_opened = !var->gui.menu_opened;
                if (!var->gui.menu_opened) {
                    var->gui.settings_opened = false;
                }
            }

            const bool menuCapturesInput = var->gui.menu_opened || var->gui.menu_alpha > 0.03f;
            Features::Input::SetGameInputActive(minecraftFocused && !menuCapturesInput);
            ApplyCursorMode(minecraftFocused && menuCapturesInput);
            ImGui::GetIO().MouseDrawCursor = false;

            EnsureSdkCache();
            cfg->apply_keybinds();
            SyncFeatureState();
            if (menuCapturesInput) {
                ClearOneShotModuleToggles();
                SyncFeatureState();
            }

            if (SDK::Classes.Ready) {
                Features::AutoSprint::Tick();
                Features::Runtime::Tick();
                ClearOneShotModuleToggles();
            }

            RenderOverlayLayer();
            cfg->auto_save();
            gui->easing(var->gui.menu_alpha, var->gui.menu_opened ? 1.f : 0.f, 10.f, static_easing);

            bool menuRendered = false;
            if (var->gui.menu_alpha > 0.01f) {
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, var->gui.menu_alpha);
                gui->render();
                ImGui::PopStyleVar();
                menuRendered = true;
            }

            if (menuRendered) {
                SyncFeatureState();
                cfg->auto_save();
            }

            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }

        void RenderBeforeSwap(HDC hdc) {
            if (gShutdownRequested.load()) {
                if (!gRenderShutdownComplete.load()) {
                    ShutdownGuiOnRenderThread();
                }
            } else if (!gShuttingDown.load()) {
                RenderFrame(hdc);
            }
        }

        BOOL RenderAndCallOriginal(SwapBuffers_t original, HDC hdc) {
            if (!original) {
                return FALSE;
            }

            if (gInsideSwapBuffers) {
                return original(hdc);
            }

            if (!hdc || !wglGetCurrentContext()) {
                return original(hdc);
            }

            gActiveSwapCalls.fetch_add(1, std::memory_order_acq_rel);
            gInsideSwapBuffers = true;
            RenderBeforeSwap(hdc);

            const BOOL result = original(hdc);
            gInsideSwapBuffers = false;
            gActiveSwapCalls.fetch_sub(1, std::memory_order_acq_rel);
            return result;
        }

        void RenderAndCallOriginalGlfw(void* window) {
            if (!OriginalGlfwSwapBuffers) {
                return;
            }

            if (gInsideSwapBuffers) {
                OriginalGlfwSwapBuffers(window);
                return;
            }

            HDC hdc = wglGetCurrentDC();
            if (!hdc || !wglGetCurrentContext()) {
                OriginalGlfwSwapBuffers(window);
                return;
            }

            gActiveSwapCalls.fetch_add(1, std::memory_order_acq_rel);
            gInsideSwapBuffers = true;
            RenderBeforeSwap(hdc);

            OriginalGlfwSwapBuffers(window);
            gInsideSwapBuffers = false;
            gActiveSwapCalls.fetch_sub(1, std::memory_order_acq_rel);
        }
    }

    bool InitOpenGL() {
        gShutdownRequested = false;
        gShuttingDown = false;
        gRenderShutdownComplete = false;
        gActiveSwapCalls = 0;

        HMODULE opengl32 = GetModuleHandleA("opengl32.dll");
        if (!opengl32) {
            opengl32 = LoadLibraryA("opengl32.dll");
        }
        if (opengl32) {
            gWglSwapBuffersTarget = reinterpret_cast<void*>(GetProcAddress(opengl32, "wglSwapBuffers"));
        }

        const MH_STATUS initStatus = MH_Initialize();
        if (initStatus != MH_OK && initStatus != MH_ERROR_ALREADY_INITIALIZED) {
            return false;
        }

        if (TryEnableHook(gWglSwapBuffersTarget, &WglSwapBuffers_Hook, reinterpret_cast<LPVOID*>(&OriginalWglSwapBuffers))) {
            WriteHookDebug("hook=wglSwapBuffers preferred finish=off");
            gSwapBuffersTarget = nullptr;
            gGlfwSwapBuffersTarget = nullptr;
            OriginalSwapBuffers = nullptr;
            OriginalGlfwSwapBuffers = nullptr;
            return true;
        }

        if (LoadGlfwForSwapHook()) {
            gGlfwSwapBuffersTarget = reinterpret_cast<void*>(GetProcAddress(gGlfwModule, "glfwSwapBuffers"));
            if (TryEnableHook(gGlfwSwapBuffersTarget, &GlfwSwapBuffers_Hook, reinterpret_cast<LPVOID*>(&OriginalGlfwSwapBuffers))) {
                WriteHookDebug("hook=glfwSwapBuffers fallback finish=off");
                gSwapBuffersTarget = nullptr;
                gWglSwapBuffersTarget = nullptr;
                OriginalSwapBuffers = nullptr;
                OriginalWglSwapBuffers = nullptr;
                return true;
            }
        }

        HMODULE gdi32 = GetModuleHandleA("gdi32.dll");
        if (!gdi32) {
            gdi32 = LoadLibraryA("gdi32.dll");
        }

        if (!gdi32) {
            return false;
        }

        gSwapBuffersTarget = reinterpret_cast<void*>(GetProcAddress(gdi32, "SwapBuffers"));
        if (TryEnableHook(gSwapBuffersTarget, &SwapBuffers_Hook, reinterpret_cast<LPVOID*>(&OriginalSwapBuffers))) {
            WriteHookDebug("hook=SwapBuffers fallback finish=off");
            gWglSwapBuffersTarget = nullptr;
            gGlfwSwapBuffersTarget = nullptr;
            OriginalWglSwapBuffers = nullptr;
            OriginalGlfwSwapBuffers = nullptr;
            return true;
        }

        WriteHookDebug("hook=failed");
        return false;
    }

    void RequestShutdown() {
        gShutdownRequested = true;
    }

    bool IsShutdownRequested() {
        return gShutdownRequested.load();
    }

    bool IsRenderShutdownComplete() {
        return gRenderShutdownComplete.load();
    }

    void Shutdown() {
        RequestShutdown();

        const ULONGLONG started = GetTickCount64();
        if (!gInitialized) {
            gRenderShutdownComplete = true;
        }

        while ((!gRenderShutdownComplete.load() || gActiveSwapCalls.load(std::memory_order_acquire) > 0) && GetTickCount64() - started < 2000) {
            Sleep(10);
        }

        gShuttingDown = true;
        if (gActiveSwapCalls.load(std::memory_order_acquire) > 0) {
            return;
        }

        if (gSwapBuffersTarget) {
            MH_DisableHook(gSwapBuffersTarget);
        }
        if (gWglSwapBuffersTarget) {
            MH_DisableHook(gWglSwapBuffersTarget);
        }
        if (gGlfwSwapBuffersTarget) {
            MH_DisableHook(gGlfwSwapBuffersTarget);
        }
        MH_Uninitialize();

        if (gOriginalWndProc && gWindow && IsWindow(gWindow)) {
            SetWindowLongPtrA(gWindow, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(gOriginalWndProc));
            gOriginalWndProc = nullptr;
        }

        OriginalSwapBuffers = nullptr;
        OriginalWglSwapBuffers = nullptr;
        OriginalGlfwSwapBuffers = nullptr;
        gSwapBuffersTarget = nullptr;
        gWglSwapBuffersTarget = nullptr;
        gGlfwSwapBuffersTarget = nullptr;
    }

    BOOL WINAPI SwapBuffers_Hook(HDC hdc) {
        return RenderAndCallOriginal(OriginalSwapBuffers, hdc);
    }

    BOOL WINAPI WglSwapBuffers_Hook(HDC hdc) {
        return RenderAndCallOriginal(OriginalWglSwapBuffers, hdc);
    }

    void __cdecl GlfwSwapBuffers_Hook(void* window) {
        RenderAndCallOriginalGlfw(window);
    }
}
