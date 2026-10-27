cmake_minimum_required(VERSION 3.20)
project(legit-client LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# Allow configuring older third-party CMake projects (e.g. FreeType/MinHook)
# with modern CMake 4.x.
set(CMAKE_POLICY_VERSION_MINIMUM "3.5.0")

# Dependencies
FetchContent_Declare(
    minhook
    GIT_REPOSITORY https://github.com/TsudaKageyu/minhook.git
    GIT_TAG master
)
FetchContent_MakeAvailable(minhook)

FetchContent_Declare(
    freetype
    GIT_REPOSITORY https://github.com/freetype/freetype.git
    GIT_TAG VER-2-13-2
)
FetchContent_MakeAvailable(freetype)

# Includes
include_directories(
    "${CMAKE_CURRENT_SOURCE_DIR}/core"
    "${CMAKE_CURRENT_SOURCE_DIR}/sdk"
    "${CMAKE_CURRENT_SOURCE_DIR}/features"
    "${CMAKE_CURRENT_SOURCE_DIR}/features/combat"
    "${CMAKE_CURRENT_SOURCE_DIR}/features/common"
    "${CMAKE_CURRENT_SOURCE_DIR}/features/misc"
    "${CMAKE_CURRENT_SOURCE_DIR}/features/movement"
    "${CMAKE_CURRENT_SOURCE_DIR}/features/render"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/headers"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/settings"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/thirdparty/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/thirdparty/imgui/backends"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/thirdparty/stb"
)

# Source files
set(SOURCES
    "core/main.cpp"
    "core/jni_bridge.cpp"
    "core/hooks.cpp"
    "features/combat/hitbox.cpp"
    "features/combat/no_friend_damage.cpp"
    "features/combat/shift_tap.cpp"
    "features/combat/triggerbot.cpp"
    "features/common/feature_runtime.cpp"
    "features/common/inventory_utils.cpp"
    "features/misc/auto_respawn.cpp"
    "features/misc/discord_rpc.cpp"
    "features/misc/friends.cpp"
    "features/misc/item_scroller.cpp"
    "features/misc/mcf.cpp"
    "features/movement/autosprint.cpp"
    "features/movement/gui_move.cpp"
    "features/render/esp.cpp"
    "features/render/hud.cpp"
    "features/render/name_protect.cpp"
    "features/render/no_render.cpp"
    "features/render/storage_esp.cpp"
    "features/render/world_tweaks.cpp"
    "sdk/minecraft_classes.cpp"

    # GUI Framework
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/gui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/helpers/blur.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/helpers/config.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/helpers/draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/helpers/fonts.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/helpers/search.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/widgets/button.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/widgets/checkbox.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/widgets/child.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/widgets/color_edit.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/widgets/dropdown.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/widgets/helpers.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/widgets/keybind.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/widgets/notify.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/widgets/section.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/widgets/slider.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/widgets/text_field.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/framework/widgets/window.cpp"

    # ImGui
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/thirdparty/imgui/imgui.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/thirdparty/imgui/imgui_draw.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/thirdparty/imgui/imgui_tables.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/thirdparty/imgui/imgui_widgets.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/thirdparty/imgui/imgui_freetype.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/thirdparty/imgui/imgui_impl_win32.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/thirdparty/imgui/imgui_impl_dx11.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/menu/thirdparty/imgui/imgui_impl_opengl3.cpp"
)

# JNI
find_package(Java REQUIRED)
find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_compile_definitions(${PROJECT_NAME} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)

target_link_libraries(${PROJECT_NAME} PRIVATE
    ${JNI_LIBRARIES}
    d3d11
    dxgi
    dwmapi
    d3dcompiler
    opengl32
    minhook
    freetype
)
