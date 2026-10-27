#include "../headers/functions.h"
#include "../headers/widgets.h"

#include <array>
#include <windows.h>

const char* const keys[] = { "Tab", "Left", "Right", "Up", "Down", "Page Up", "Page Down", "Home", "End", "Insert", "Delete", "Backspace", "Space", "Enter", "Escape", "Ctrl", "Shift", "Alt", "Super", "Ctrl", "Shift", "Alt", "Super", "Menu", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15", "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "Apostrophe", "Comma", "Minus", "Period", "Slash", "Semicolon", "Equal", "Left Bracket", "Backslash", "Right Bracket", "Grave Accent", "Caps Lock", "Scroll Lock", "Num Lock", "Print Screen", "Pause", "Keypad 0", "Keypad 1", "Keypad 2", "Keypad 3", "Keypad 4", "Keypad 5", "Keypad 6", "Keypad 7", "Keypad 8", "Keypad 9", "Keypad .", "Keypad /", "Keypad *", "Keypad -", "Keypad +", "Keypad Enter", "Keypad =", "App Back", "App Forward", "Gamepad Start", "Gamepad Back", "Gamepad Face Left", "Gamepad Face Right", "Gamepad Face Up", "Gamepad Face Down", "Gamepad Dpad Left", "Gamepad Dpad Right", "Gamepad Dpad Up", "Gamepad Dpad Down", "Gamepad L1", "Gamepad R1", "Gamepad L2", "Gamepad R2", "Gamepad L3", "Gamepad R3", "Gamepad L Stick Left", "Gamepad L Stick Right", "Gamepad L Stick Up", "Gamepad L Stick Down", "Gamepad R Stick Left", "Gamepad R Stick Right", "Gamepad R Stick Up", "Gamepad R Stick Down", "Mouse 1", "Mouse 2", "Mouse 3", "Mouse 4", "Mouse 5", "Mouse Wheel X", "Mouse Wheel Y", "Ctrl", "Shift", "Alt", "Super" };

const char* get_key_name(ImGuiKey key)
{
    if (key == ImGuiKey_None || key < ImGuiKey_NamedKey_BEGIN || key >= ImGuiKey_COUNT)
        return "-";

    return keys[key - ImGuiKey_NamedKey_BEGIN];
}

namespace
{
    struct vk_key_t
    {
        int vk;
        ImGuiKey key;
    };

    const vk_key_t action_keys[] = {
        { VK_MBUTTON, ImGuiKey_MouseMiddle }, { VK_XBUTTON1, ImGuiKey_MouseX1 }, { VK_XBUTTON2, ImGuiKey_MouseX2 },
        { VK_LBUTTON, ImGuiKey_MouseLeft }, { VK_RBUTTON, ImGuiKey_MouseRight },
        { VK_TAB, ImGuiKey_Tab }, { VK_SPACE, ImGuiKey_Space }, { VK_RETURN, ImGuiKey_Enter }, { VK_ESCAPE, ImGuiKey_Escape },
        { VK_LSHIFT, ImGuiKey_LeftShift }, { VK_RSHIFT, ImGuiKey_RightShift }, { VK_SHIFT, ImGuiKey_LeftShift },
        { VK_LCONTROL, ImGuiKey_LeftCtrl }, { VK_RCONTROL, ImGuiKey_RightCtrl }, { VK_CONTROL, ImGuiKey_LeftCtrl },
        { VK_LMENU, ImGuiKey_LeftAlt }, { VK_RMENU, ImGuiKey_RightAlt }, { VK_MENU, ImGuiKey_LeftAlt },
        { '0', ImGuiKey_0 }, { '1', ImGuiKey_1 }, { '2', ImGuiKey_2 }, { '3', ImGuiKey_3 }, { '4', ImGuiKey_4 },
        { '5', ImGuiKey_5 }, { '6', ImGuiKey_6 }, { '7', ImGuiKey_7 }, { '8', ImGuiKey_8 }, { '9', ImGuiKey_9 },
        { 'A', ImGuiKey_A }, { 'B', ImGuiKey_B }, { 'C', ImGuiKey_C }, { 'D', ImGuiKey_D }, { 'E', ImGuiKey_E },
        { 'F', ImGuiKey_F }, { 'G', ImGuiKey_G }, { 'H', ImGuiKey_H }, { 'I', ImGuiKey_I }, { 'J', ImGuiKey_J },
        { 'K', ImGuiKey_K }, { 'L', ImGuiKey_L }, { 'M', ImGuiKey_M }, { 'N', ImGuiKey_N }, { 'O', ImGuiKey_O },
        { 'P', ImGuiKey_P }, { 'Q', ImGuiKey_Q }, { 'R', ImGuiKey_R }, { 'S', ImGuiKey_S }, { 'T', ImGuiKey_T },
        { 'U', ImGuiKey_U }, { 'V', ImGuiKey_V }, { 'W', ImGuiKey_W }, { 'X', ImGuiKey_X }, { 'Y', ImGuiKey_Y }, { 'Z', ImGuiKey_Z },
        { VK_F1, ImGuiKey_F1 }, { VK_F2, ImGuiKey_F2 }, { VK_F3, ImGuiKey_F3 }, { VK_F4, ImGuiKey_F4 },
        { VK_F5, ImGuiKey_F5 }, { VK_F6, ImGuiKey_F6 }, { VK_F7, ImGuiKey_F7 }, { VK_F8, ImGuiKey_F8 },
        { VK_F9, ImGuiKey_F9 }, { VK_F10, ImGuiKey_F10 }, { VK_F11, ImGuiKey_F11 }, { VK_F12, ImGuiKey_F12 },
        { VK_F13, ImGuiKey_F13 }, { VK_F14, ImGuiKey_F14 }, { VK_F15, ImGuiKey_F15 }, { VK_F16, ImGuiKey_F16 },
        { VK_F17, ImGuiKey_F17 }, { VK_F18, ImGuiKey_F18 }, { VK_F19, ImGuiKey_F19 }, { VK_F20, ImGuiKey_F20 },
        { VK_F21, ImGuiKey_F21 }, { VK_F22, ImGuiKey_F22 }, { VK_F23, ImGuiKey_F23 }, { VK_F24, ImGuiKey_F24 },
        { VK_LEFT, ImGuiKey_LeftArrow }, { VK_RIGHT, ImGuiKey_RightArrow }, { VK_UP, ImGuiKey_UpArrow }, { VK_DOWN, ImGuiKey_DownArrow },
        { VK_PRIOR, ImGuiKey_PageUp }, { VK_NEXT, ImGuiKey_PageDown }, { VK_HOME, ImGuiKey_Home }, { VK_END, ImGuiKey_End },
        { VK_INSERT, ImGuiKey_Insert }, { VK_DELETE, ImGuiKey_Delete }, { VK_BACK, ImGuiKey_Backspace },
        { VK_NUMPAD0, ImGuiKey_Keypad0 }, { VK_NUMPAD1, ImGuiKey_Keypad1 }, { VK_NUMPAD2, ImGuiKey_Keypad2 },
        { VK_NUMPAD3, ImGuiKey_Keypad3 }, { VK_NUMPAD4, ImGuiKey_Keypad4 }, { VK_NUMPAD5, ImGuiKey_Keypad5 },
        { VK_NUMPAD6, ImGuiKey_Keypad6 }, { VK_NUMPAD7, ImGuiKey_Keypad7 }, { VK_NUMPAD8, ImGuiKey_Keypad8 },
        { VK_NUMPAD9, ImGuiKey_Keypad9 }, { VK_DECIMAL, ImGuiKey_KeypadDecimal }, { VK_DIVIDE, ImGuiKey_KeypadDivide },
        { VK_MULTIPLY, ImGuiKey_KeypadMultiply }, { VK_SUBTRACT, ImGuiKey_KeypadSubtract }, { VK_ADD, ImGuiKey_KeypadAdd },
        { VK_OEM_7, ImGuiKey_Apostrophe }, { VK_OEM_COMMA, ImGuiKey_Comma }, { VK_OEM_MINUS, ImGuiKey_Minus },
        { VK_OEM_PERIOD, ImGuiKey_Period }, { VK_OEM_2, ImGuiKey_Slash }, { VK_OEM_1, ImGuiKey_Semicolon },
        { VK_OEM_PLUS, ImGuiKey_Equal }, { VK_OEM_4, ImGuiKey_LeftBracket }, { VK_OEM_5, ImGuiKey_Backslash },
        { VK_OEM_6, ImGuiKey_RightBracket }, { VK_OEM_3, ImGuiKey_GraveAccent }, { VK_CAPITAL, ImGuiKey_CapsLock },
        { VK_SCROLL, ImGuiKey_ScrollLock }, { VK_NUMLOCK, ImGuiKey_NumLock }, { VK_SNAPSHOT, ImGuiKey_PrintScreen },
        { VK_PAUSE, ImGuiKey_Pause }, { VK_APPS, ImGuiKey_Menu }
    };

    ImGuiKey action_key_from_vk(int vk)
    {
        for (const auto& item : action_keys)
        {
            if (item.vk == vk)
                return item.key;
        }
        return ImGuiKey_None;
    }

    int action_vk_from_key(ImGuiKey key)
    {
        for (const auto& item : action_keys)
        {
            if (item.key == key)
                return item.vk;
        }
        return 0;
    }

    void normalize_stored_key(int* key)
    {
        if (!key || *key <= 0 || *key >= ImGuiKey_NamedKey_BEGIN)
            return;

        const ImGuiKey converted = action_key_from_vk(*key);
        if (converted != ImGuiKey_None)
            *key = converted;
    }

    void normalize_action_key(int* key)
    {
        if (!key || *key <= 0)
            return;

        if (*key < 256)
            return;

        const int vk = action_vk_from_key(static_cast<ImGuiKey>(*key));
        *key = vk > 0 ? vk : 0;
    }

    const char* get_action_key_name(int key)
    {
        if (key <= 0)
            return "-";

        const ImGuiKey imgui_key = key < 256 ? action_key_from_vk(key) : static_cast<ImGuiKey>(key);
        const char* name = get_key_name(imgui_key);
        return name && name[0] ? name : "-";
    }

    bool action_any_down()
    {
        for (const auto& item : action_keys)
        {
            if ((GetAsyncKeyState(item.vk) & 0x8000) != 0)
                return true;
        }
        return false;
    }

    ImGuiKey action_pressed_imgui_key()
    {
        for (const auto& item : action_keys)
        {
            if ((GetAsyncKeyState(item.vk) & 0x8000) != 0)
                return item.key;
        }
        return ImGuiKey_None;
    }

    int action_pressed_vk()
    {
        for (const auto& item : action_keys)
        {
            if ((GetAsyncKeyState(item.vk) & 0x8000) != 0)
                return item.vk;
        }
        return 0;
    }
}

widget_t c_widgets::key_select(std::string_view name, int* key)
{
    struct anim_t
    {
        bool active{ false };
        bool waiting_release{ false };
        float alpha{ 0 };
    };

    widget_t data = gui->register_item(name, SCALE(elements->keybind.key_select_size));
    anim_t* anim = gui->anim_container<anim_t>(data->id);
    normalize_stored_key(key);

    const ImRect plus_rect{ data->rect.GetTR() - SCALE(elements->keybind.key_select_size.y,0) ,data->rect.Max };
    const ImRect main_rect{ data->rect.Min ,plus_rect.GetBL() };

    std::string buf_display = anim->active ? "-" : get_key_name((ImGuiKey)*key);

    const bool main_clicked = main_rect.Contains(gui->mouse_pos()) && gui->mouse_released(mouse_button_left) && gui->is_window_hovered(0);
    const bool plus_clicked = plus_rect.Contains(gui->mouse_pos()) &&
        (gui->mouse_clicked(mouse_button_left) || gui->mouse_released(mouse_button_left)) && gui->is_window_hovered(0);

    if (main_clicked || plus_clicked)
    {
        anim->active = true;
        anim->waiting_release = true;
    }

    data->value_changed = false;

    if (anim->active)
    {
        if (anim->waiting_release)
        {
            if (!action_any_down())
                anim->waiting_release = false;
        }
        else
        {
            if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0)
            {
                *key = 0;
                anim->active = false;
                data->value_changed = true;
            }
            else
            {
                const ImGuiKey pressed = action_pressed_imgui_key();
                if (pressed != ImGuiKey_None && pressed != ImGuiKey_Escape)
                {
                    *key = pressed;
                    anim->active = false;
                    data->value_changed = true;
                }
            }
        }
    }
    draw->rect_filled(gui->window_drawlist(), data->rect.Min, data->rect.Max, draw->get_clr(clr->widgets.widgets_active), SCALE(elements->keybind.rounding_widgets));

    draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 12), data->rect.Min + SCALE(elements->keybind.indent_text, 0), data->rect.Max, draw->get_clr(clr->widgets.text), buf_display.data(), gui->text_end(buf_display.data()), NULL, ImVec2(0.f, 0.5f));
    draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 16), plus_rect.Min, plus_rect.Max, draw->get_clr(clr->widgets.text), "+", NULL, NULL, ImVec2(0.5f, 0.5f));

    return data;
}

widget_t c_widgets::action_key_select(std::string_view name, int* key)
{
    struct anim_t
    {
        bool active{ false };
        bool waiting_release{ false };
    };

    widget_t data = gui->register_item(std::string("action_key_") + std::string(name), SCALE(elements->keybind.key_select_size));
    anim_t* anim = gui->anim_container<anim_t>(data->id);
    normalize_action_key(key);

    const ImRect plus_rect{ data->rect.GetTR() - SCALE(elements->keybind.key_select_size.y,0), data->rect.Max };
    const ImRect main_rect{ data->rect.Min, plus_rect.GetBL() };
    const bool clicked = (main_rect.Contains(gui->mouse_pos()) || plus_rect.Contains(gui->mouse_pos())) &&
        (gui->mouse_clicked(mouse_button_left) || gui->mouse_released(mouse_button_left)) && gui->is_window_hovered(0);

    if (clicked)
    {
        anim->active = true;
        anim->waiting_release = true;
    }

    data->value_changed = false;

    if (anim->active)
    {
        if (anim->waiting_release)
        {
            if (!action_any_down())
                anim->waiting_release = false;
        }
        else
        {
            if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0)
            {
                *key = 0;
                anim->active = false;
                data->value_changed = true;
            }
            else
            {
                const int pressed = action_pressed_vk();
                if (pressed > 0 && pressed != VK_ESCAPE)
                {
                    *key = pressed;
                    anim->active = false;
                    data->value_changed = true;
                }
            }
        }
    }

    const char* display = anim->active ? "-" : get_action_key_name(*key);
    draw->rect_filled(gui->window_drawlist(), data->rect.Min, data->rect.Max, draw->get_clr(clr->widgets.widgets_active), SCALE(elements->keybind.rounding_widgets));
    draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 12), data->rect.Min + SCALE(elements->keybind.indent_text, 0), data->rect.Max, draw->get_clr(clr->widgets.text), display, nullptr, nullptr, ImVec2(0.f, 0.5f));
    draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 16), plus_rect.Min, plus_rect.Max, draw->get_clr(clr->widgets.text), "+", nullptr, nullptr, ImVec2(0.5f, 0.5f));
    return data;
}

widget_t c_widgets::mode_select(std::string_view name, int* mode)
{
    struct anim_t
    {
        float alpha[2]{ 0, 0 };
        ImVec4 text[2]{ clr->widgets.text, clr->widgets.text };
    };

    widget_t data = gui->register_item(name, ImVec2(SCALE(elements->keybind.mode_select_size)));
    anim_t* anim = gui->anim_container<anim_t>(data->id);

    const ImRect toggle{ data->rect.Min, ImVec2(data->rect.GetCenter().x - var->style.item_spacing.x / 2, data->rect.Max.y) };
    const ImRect hold{ ImVec2(data->rect.GetCenter().x + var->style.item_spacing.x / 2, data->rect.Min.y), data->rect.Max };

    if (toggle.Contains(gui->mouse_pos()) && gui->mouse_clicked(mouse_button_left) && gui->is_window_hovered(0))
        *mode = 0;
    if (hold.Contains(gui->mouse_pos()) && gui->mouse_clicked(mouse_button_left) && gui->is_window_hovered(0))
        *mode = 1;

    gui->easing(anim->alpha[0], *mode == 0 ? 1.f : 0.f, 8.f, static_easing);
    gui->easing(anim->alpha[1], *mode == 1 ? 1.f : 0.f, 8.f, static_easing);

    gui->easing(anim->text[0], *mode == 0 ? clr->widgets.black.Value : clr->widgets.text.Value, 16.f, dynamic_easing);
    gui->easing(anim->text[1], *mode == 1 ? clr->widgets.black.Value : clr->widgets.text.Value, 16.f, dynamic_easing);


    draw->rect_filled(gui->window_drawlist(), toggle.Min, toggle.Max, draw->get_clr(clr->widgets.widgets_active), SCALE(elements->keybind.rounding_widgets));
    draw->rect_filled(gui->window_drawlist(), hold.Min, hold.Max, draw->get_clr(clr->widgets.widgets_active), SCALE(elements->keybind.rounding_widgets));
    draw->rect_filled(gui->window_drawlist(), toggle.Min, toggle.Max, draw->get_clr(clr->accent, anim->alpha[0]), SCALE(elements->keybind.rounding_widgets));
    draw->rect_filled(gui->window_drawlist(), hold.Min, hold.Max, draw->get_clr(clr->accent, anim->alpha[1]), SCALE(elements->keybind.rounding_widgets));

    draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 12), toggle.Min, toggle.Max, draw->get_clr(anim->text[0]), "Toggle", NULL, NULL, ImVec2(0.5f, 0.5f));
    draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 12), hold.Min, hold.Max, draw->get_clr(anim->text[1]), "Hold", NULL, NULL, ImVec2(0.5f, 0.5f));

    return data;
}

widget_t c_widgets::checkbox_keybind(std::string_view name, bool* callback)
{
    struct anim_t
    {
        float alpha;
        float offset;
        ImVec4 off_on_circle{ clr->accent };

        ImVec4 text[2]{ clr->widgets.text, clr->widgets.text };
    };

    widget_t data = gui->register_item(name, ImVec2(SCALE(elements->keybind.checbox_keybind_size)));
    anim_t* anim = gui->anim_container<anim_t>(data->id);
    const ImRect rect{ data->rect.GetTR() - SCALE(elements->keybind.size_rect_chec,0), data->rect.Max };

    data->value_changed = data->hovered && gui->mouse_clicked(mouse_button_left) && gui->is_window_hovered(0);

    if (data->value_changed)
        *callback = !*callback;
    gui->easing(anim->alpha, *callback ? 1.f : 0.f, 8.f, static_easing);
    gui->easing(anim->offset, *callback ? rect.GetWidth() - SCALE(elements->checkbox.indent_circle * 3 + elements->checkbox.size_circle / 2) : 0.f, 16.f, dynamic_easing);
    gui->easing(anim->off_on_circle, *callback ? clr->accent.Value : clr->widgets.widgets_inactive.Value, 16.f, dynamic_easing);



    draw->rect_filled(gui->window_drawlist(), rect.Min, rect.Max, draw->get_clr(clr->widgets.widgets_background), SCALE(elements->keybind.rounding_checbox));
    draw->rect_filled(gui->window_drawlist(), rect.Min, rect.Max, draw->get_clr(clr->accent, anim->alpha * 0.10f), SCALE(elements->keybind.rounding_checbox));
    draw->circle_filled(gui->window_drawlist(), ImVec2(rect.Min.x + SCALE(elements->keybind.indent_circle + elements->keybind.size_circle / 2) + anim->offset, rect.GetCenter().y), SCALE(elements->keybind.size_circle / 2), draw->get_clr(anim->off_on_circle));

    draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 12), data->rect.Min, data->rect.Max, draw->get_clr(clr->widgets.text), name.data(), NULL, NULL, ImVec2(0.f, 0.5f));

    return data;
}


widget_t c_widgets::button_keybind(std::string_view name, std::string_view icon, int index, int& count)
{
    struct anim_t
    {
        float active{ false };
        float alpha;
        ImVec4 offset{ clr->widgets.widgets_active };

    };

    widget_t data = gui->register_item(name, ImVec2(gui->content_avail().x, SCALE(elements->keybind.size_main_widgets)));
    anim_t* anim = gui->anim_container<anim_t>(data->id);

    bool active = index == count;

    data->value_changed = data->hovered && gui->mouse_clicked(mouse_button_left);
    if (data->value_changed)
        count = index;

    gui->easing(anim->alpha, active ? 1.f : 0.f, 6.f, static_easing);
    gui->easing(anim->offset, anim->active ? clr->widgets.widgets_active.Value : clr->window.background.Value, 16.f, dynamic_easing);

    if (anim->alpha >= 0.99f)
        anim->active = false;

    draw->rect_filled(gui->window_drawlist(), data->rect.Min, data->rect.Max, draw->get_clr(clr->widgets.widgets_inactive, anim->alpha), SCALE(elements->keybind.rounding_widgets));
    draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 12), data->rect.Min + SCALE(elements->keybind.padding_main_name, 0), data->rect.Max, draw->get_clr(clr->widgets.text), name.data(), NULL, NULL, ImVec2(0.f, 0.5f));
    draw->text_clipped(gui->window_drawlist(), font->get(icon_data, 12), data->rect.Min + SCALE(elements->keybind.padding_main_icon, 0), data->rect.Max, draw->get_clr(clr->widgets.text), icon.data(), NULL, NULL, ImVec2(0.f, 0.5f));

    return data;
}

widget_t c_widgets::button_keybind_reset(std::string_view name, std::string_view icon)
{
    struct anim_t
    {
        float active{ false };
        float alpha;
        ImVec4 offset{ clr->widgets.widgets_active };

    };

    widget_t data = gui->register_item(name, ImVec2(gui->content_avail().x, SCALE(elements->keybind.size_main_widgets)));
    anim_t* anim = gui->anim_container<anim_t>(data->id);

    data->value_changed = data->hovered && gui->mouse_clicked(mouse_button_left);
    if (data->value_changed)
        anim->active = true;

    gui->easing(anim->alpha, anim->active ? 1.f : 0.f, 6.f, static_easing);
    if (anim->alpha >= 0.99f)
        anim->active = false;

    draw->rect_filled(gui->window_drawlist(), data->rect.Min, data->rect.Max, draw->get_clr(clr->widgets.widgets_inactive, anim->alpha), SCALE(elements->keybind.rounding_widgets));

    draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 12), data->rect.Min + SCALE(elements->keybind.padding_main_name, 0), data->rect.Max, draw->get_clr(clr->widgets.reb), name.data(), NULL, NULL, ImVec2(0.f, 0.5f));
    draw->text_clipped(gui->window_drawlist(), font->get(icon_data, 12), data->rect.Min + SCALE(elements->keybind.padding_main_icon, 0), data->rect.Max, draw->get_clr(clr->widgets.reb), icon.data(), NULL, NULL, ImVec2(0.f, 0.5f));

    return data;
}

widget_t c_widgets::button_keybind_added(std::string_view name, int key)
{
    struct anim_t
    {

    };

    widget_t data = gui->register_item(name, ImVec2(gui->content_avail().x, SCALE(elements->keybind.size_main_widgets)));
    anim_t* anim = gui->anim_container<anim_t>(data->id);

    data->value_changed = data->hovered && gui->mouse_clicked(mouse_button_left) && gui->is_window_hovered();

    draw->rect_filled(gui->window_drawlist(), data->rect.Min, data->rect.Max, draw->get_clr(clr->widgets.widgets_active), SCALE(4));
    draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 12), data->rect.Min, data->rect.Max, draw->get_clr(clr->widgets.text), get_key_name((ImGuiKey)key), NULL, NULL, ImVec2(0.5f, 0.5f));

    return data;
}
