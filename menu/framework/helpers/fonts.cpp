#include "../headers/includes.h"
#include "../../thirdparty/imgui/imgui_freetype.h"
#include "imgui_impl_opengl3.h"

#include <algorithm>

void c_font::update()
{
    if (var->gui.dpi_changed)
    {
        var->gui.dpi = std::clamp(var->gui.stored_dpi / 100.f, 0.75f, 2.0f);
        ImGui::GetIO().FontGlobalScale = var->gui.dpi;
        var->gui.dpi_changed = false;
    }

    bool rebuild_fonts = data.empty();
    for (const auto& font_t : data)
        rebuild_fonts = rebuild_fonts || font_t.font == nullptr;

    if (rebuild_fonts)
    {
        ImFontConfig cfg;
        cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_Bitmap;
        cfg.FontDataOwnedByAtlas = false;

        ImGuiIO& io = ImGui::GetIO();
        ImGui_ImplOpenGL3_DestroyFontsTexture();
        io.Fonts->Clear();

        for (auto& font_t : data)
        {
            font_t.font = io.Fonts->AddFontFromMemoryTTF(font_t.data.data(), font_t.data.size(), scale_impl(font_t.size, 1.f), &cfg, io.Fonts->GetGlyphRangesCyrillic());
        }

        io.Fonts->Build();
    }
}

ImFont* c_font::get(std::vector<unsigned char> font_data, float size)
{
    for (auto& font : data)
    {
        if (font.data == font_data && font.size == size)
        {
            if (!font.font && ImGui::GetCurrentContext() && !GImGui->WithinFrameScope)
                update();

            return font.font ? font.font : ImGui::GetFont();
        }
    }

    add(font_data, size);

    var->gui.dpi_changed = true;
    if (ImGui::GetCurrentContext() && !GImGui->WithinFrameScope)
        update();

    return get(font_data, size);
}

void c_font::add(std::vector<unsigned char> font_data, float size)
{
    data.push_back({ font_data, size, nullptr });
}
