#include "../headers/functions.h"
#include "../headers/widgets.h"

#include <algorithm>

namespace
{
    const char* module_option_from_child(std::string_view name)
    {
        if (name == "HITBOX") return "HitBox";
        if (name == "TRIGGERBOT") return "TriggerBot";
        if (name == "SHIFT TAP") return "ShiftTap";
        if (name == "NO FRIEND DAMAGE") return "NoFriendDamage";
        if (name == "SPRINT") return "Sprint";
        if (name == "GUI MOVE") return "GuiMove";
        if (name == "ESP") return "ESP";
        if (name == "WORLD TWEAKS") return "WorldTweaks";
        if (name == "WORLD TIME") return "World Time";
        if (name == "NO RENDER") return "NoRender";
        if (name == "STORAGE ESP") return "StorageESP";
        if (name == "ITEM SCROLLER") return "ItemScroller";
        if (name == "MCF") return "MCF";
        if (name == "NAME PROTECT") return "NameProtect";
        if (name == "AUTO RESPAWN") return "AutoRespawn";
        if (name == "DISCORD RPC") return "DiscordRPC";
        return nullptr;
    }

    void render_header_keybind(std::string_view child_name, checkbox_t* option, const ImRect& title_rect)
    {
        if (!option || !option->keybind)
            return;

        struct bind_state
        {
            float alpha{ 0.f };
            float tab_new_alpha{ 0.f };
            float tab_list_alpha{ 0.f };
            bool opened{ false };
            bool hovered{ false };
            int key{ 0 };
            int mode{ 0 };
            bool value{ true };
            bool show{ true };
            int tab{ -1 };
        };

        const std::string id = "header_bind_" + std::string(child_name);
        bind_state* state = gui->anim_container<bind_state>(gui->get_window()->GetID(id.c_str()));

        const bool title_hovered = title_rect.Contains(gui->mouse_pos()) && gui->is_window_hovered(0);
        if (title_hovered && gui->mouse_clicked(mouse_button_middle))
            state->opened = !state->opened;

        if (state->opened && (gui->mouse_clicked(mouse_button_left) || gui->mouse_clicked(mouse_button_right) || gui->mouse_clicked(mouse_button_middle)) && !state->hovered && !title_hovered)
            state->opened = false;

        const ImRect icon_rect{ title_rect.Max - SCALE(26, 20), title_rect.Max - SCALE(4, 0) };
        draw->text_clipped(gui->window_drawlist(), font->get(icon_data, 10), icon_rect.Min, icon_rect.Max, draw->get_clr(option->state.empty() ? clr->widgets.text_inactive : clr->accent), "I", nullptr, nullptr, ImVec2(0.5f, 0.5f));

        gui->easing(state->alpha, state->opened ? 1.f : 0.f, 8.f, static_easing);
        gui->easing(state->tab_new_alpha, state->tab == 0 ? 1.f : 0.f, 8.f, static_easing);
        gui->easing(state->tab_list_alpha, state->tab == 1 ? 1.f : 0.f, 8.f, static_easing);

        if (state->alpha <= 0.01f)
            return;

        gui->push_var(style_var_alpha, state->alpha);
        gui->push_var(style_var_window_padding, SCALE(elements->keybind.padding_main));
        gui->push_var(style_var_item_spacing, SCALE(elements->keybind.spacing_main));
        gui->push_var(style_var_window_rounding, SCALE(elements->keybind.rounding_window));
        gui->push_color(style_col_popup_bg, draw->get_clr(clr->window.background));
        gui->push_var(style_var_popup_border_size, 0.f);
        gui->push_color(style_col_border, 0);

        gui->set_next_window_pos(title_rect.Min + SCALE(0, 20));
        gui->set_next_window_size(SCALE(elements->keybind.main_size));
        gui->begin("header_keybind_window" + std::string(child_name), nullptr, window_flags_tooltip | window_flags_always_use_window_padding | window_flags_no_saved_settings | window_flags_no_focus_on_appearing | window_flags_always_auto_resize | window_flags_no_decoration | window_flags_no_scrollbar | window_flags_no_scroll_with_mouse);
        {
            gui->set_window_focus();
            state->hovered = gui->is_window_hovered(0);
            widgets->button_keybind("New bind", "O", 0, state->tab);
            widgets->button_keybind("Hotkeys", "P", 1, state->tab);
            if (widgets->button_keybind_reset("Reset", "Q").value_changed)
                option->state.clear();
        }
        gui->end();
        gui->pop_var(5);
        gui->pop_color(2);

        if (state->tab_new_alpha > 0.01f)
        {
            gui->push_var(style_var_alpha, state->tab_new_alpha * state->alpha);
            gui->push_var(style_var_window_padding, SCALE(elements->keybind.padding));
            gui->push_var(style_var_item_spacing, SCALE(elements->keybind.spacing));
            gui->push_var(style_var_window_rounding, SCALE(elements->keybind.rounding_window));
            gui->push_color(style_col_popup_bg, draw->get_clr(clr->window.background));
            gui->push_var(style_var_popup_border_size, 0.f);
            gui->push_color(style_col_border, 0);

            gui->set_next_window_size(SCALE(elements->keybind.size_window));
            gui->set_next_window_pos(title_rect.Min + SCALE(elements->keybind.main_size.x + elements->keybind.padding.x, 20));
            gui->begin("header_keybind_new" + std::string(child_name), nullptr, window_flags_tooltip | window_flags_always_use_window_padding | window_flags_no_saved_settings | window_flags_no_focus_on_appearing | window_flags_always_auto_resize | window_flags_no_decoration | window_flags_no_scrollbar | window_flags_no_scroll_with_mouse);
            {
                gui->set_window_focus();
                state->hovered |= gui->get_window()->Rect().Contains(gui->mouse_pos());

                if (widgets->key_select("key_select", &state->key).value_changed && state->key > 0)
                {
                    const key_data_t bind{ state->key, state->mode, state->value, state->show };
                    const bool exists = std::any_of(option->state.begin(), option->state.end(), [&](const key_data_t& value) {
                        return value.key == bind.key && value.mode == bind.mode && value.value == bind.value && value.show == bind.show;
                    });
                    if (!exists)
                        option->state.push_back(bind);
                    state->key = 0;
                    state->mode = 0;
                    state->value = true;
                    state->show = true;
                }
                widgets->mode_select("mode_select", &state->mode);
                widgets->checkbox_keybind("Value", &state->value);
                widgets->checkbox_keybind("Show in binds", &state->show);
            }
            gui->end();
            gui->pop_var(5);
            gui->pop_color(2);
        }

        if (state->tab_list_alpha > 0.01f)
        {
            gui->push_var(style_var_alpha, state->tab_list_alpha * state->alpha);
            gui->push_var(style_var_window_padding, SCALE(8, 8));
            gui->push_var(style_var_item_spacing, SCALE(4, 4));
            gui->push_var(style_var_window_rounding, SCALE(elements->keybind.rounding_window));
            gui->push_color(style_col_popup_bg, draw->get_clr(clr->window.background));
            gui->push_var(style_var_popup_border_size, 0.f);
            gui->push_color(style_col_border, 0);

            gui->set_next_window_size(SCALE(120, 120));
            gui->set_next_window_pos(title_rect.Min + SCALE(elements->keybind.main_size.x + elements->keybind.padding.x, 20));
            gui->begin("header_keybind_list" + std::string(child_name), nullptr, window_flags_tooltip | window_flags_always_use_window_padding | window_flags_no_saved_settings | window_flags_no_focus_on_appearing | window_flags_always_auto_resize | window_flags_no_decoration | window_flags_no_scrollbar);
            {
                gui->set_window_focus();
                state->hovered |= gui->get_window()->Rect().Contains(gui->mouse_pos());

                for (int i = 0; i < static_cast<int>(option->state.size()); ++i)
                {
                    if (widgets->button_keybind_added(std::to_string(i), option->state.at(i).key).value_changed)
                        option->state.erase(option->state.begin() + i--);
                }
            }
            gui->end();
            gui->pop_var(5);
            gui->pop_color(2);
        }
    }
}

void c_widgets::begin_child(std::string_view name)
{
    struct child_state
    {
        float height{ 0 };
        float width{ 0 };
    };

    ImGuiWindow* window = gui->get_window();
    const ImVec2 pos = window->DC.CursorPos + SCALE(0, elements->child.name);

    child_state* anim = gui->anim_container<child_state>(window->GetID(name.data()));
    anim->width = (gui->content_max().x - var->style.item_spacing.x - var->style.window_padding.x) / 2.f;
    draw->rect_filled(gui->window_drawlist(), pos, pos + ImVec2(anim->width, anim->height), draw->get_clr(clr->window.child), SCALE(elements->child.rounding));
    draw->rect(gui->window_drawlist(), pos, pos + ImVec2(anim->width, anim->height), draw->get_clr(clr->window.rect), SCALE(elements->child.rounding));

    const ImRect title_rect{ window->DC.CursorPos, pos + ImVec2(anim->width, 0) };
    draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 14), title_rect.Min, title_rect.Max, draw->get_clr(clr->widgets.text_inactive), name.data());
    if (const char* module_option = module_option_from_child(name))
        render_header_keybind(name, cfg->find<checkbox_t>(module_option), title_rect);

    gui->push_var(style_var_window_padding, SCALE(elements->child.padding,0));
    gui->set_screen_pos(pos, pos_all);
    gui->begin_def_child(name.data() + std::string("content_child"), ImVec2(anim->width, anim->height), 0, window_flags_always_use_window_padding | window_flags_no_move | window_flags_nav_flattened | window_flags_no_saved_settings | window_flags_no_scrollbar | window_flags_no_scroll_with_mouse);
    gui->push_var(style_var_item_spacing, SCALE(0,1));

    anim->height = gui->get_window()->ContentSize.y;
}

void c_widgets::end_child()
{
    gui->pop_var();
    gui->end_def_child();
    gui->pop_var();
}
