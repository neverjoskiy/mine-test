#define IMGUI_DEFINE_MATH_OPERATORS
#include "../headers/config.h"
#include "../headers/includes.h"
#include "input_utils.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace {
    std::vector<std::string> split_tab(const std::string& line) {
        std::vector<std::string> out;
        std::stringstream stream(line);
        std::string part;
        while (std::getline(stream, part, '\t')) {
            out.push_back(part);
        }
        return out;
    }

    bool as_bool(const std::string& value) {
        return value == "1" || value == "true" || value == "True";
    }

    int as_int(const std::string& value, int fallback = 0) {
        try {
            return std::stoi(value);
        } catch (...) {
            return fallback;
        }
    }

    float as_float(const std::string& value, float fallback = 0.f) {
        try {
            return std::stof(value);
        } catch (...) {
            return fallback;
        }
    }

    bool is_action_key_option(const std::string& name) {
        return name.find("Action Key") != std::string::npos;
    }

    std::string migrated_option_name(const std::string& name) {
        if (name == "ClickFriend") return "MCF";
        return name;
    }

    bool is_one_shot_module(const std::string& name) {
        return name == "MCF";
    }
}

void c_config::init_config()
{
    static bool init = false;

    if (!init)
    {
        add_option<checkbox_t>("HitBox", false, true);
        add_option<slider_float_t>("HitBox X Position", 0.3f, 0.3f, 1.5f, "%.2f");
        add_option<checkbox_t>("TriggerBot", false, true);
        add_option<slider_float_t>("TriggerBot Distance", 2.9f, 1.0f, 6.0f, "%.1f");
        add_option<multi_dropdown_t>("TriggerBot Targets", bool_t{ true, false, false, false }, string_t{ "Players", "Hostiles", "Mobs", "Invisibles" });
        add_option<multi_dropdown_t>("TriggerBot No Hit", bool_t{ true, true }, string_t{ "Eat", "Block" });
        add_option<checkbox_t>("Smart Crits", true);
        add_option<checkbox_t>("AimAssist", false, true);
        add_option<slider_float_t>("AimAssist Distance", 4.0f, 1.0f, 6.0f, "%.1f");
        add_option<slider_float_t>("AimAssist FOV", 90.0f, 20.0f, 360.0f, "%.1f");
        add_option<slider_float_t>("AimAssist Speed", 2.0f, 0.1f, 10.0f, "%.1f");
        add_option<checkbox_t>("AimAssist Only Players", true);
        add_option<checkbox_t>("ShiftTap", false, true);
        add_option<checkbox_t>("NoFriendDamage", false, true);

        add_option<settings_dropdown_t>("Language", "K", 0, string_t{ "English", "Russian" });

        add_option<checkbox_t>("Sprint", false, true);
        add_option<checkbox_t>("GuiMove", false, true);

        add_option<checkbox_t>("ESP", false, true);
        add_option<multi_dropdown_t>("ESP Elements", bool_t{ true, true, true, true }, string_t{ "Box", "Name", "Distance", "Health" });
        add_option<dropdown_t>("ESP Box Style", 1, string_t{ "Full", "Corner", "3D Box" });
        add_option<color_edit_t>("ESP 3D Box Color", col_t{ 0.45f, 0.74f, 1.0f, 0.45f }, true, "", "");

        add_option<checkbox_t>("WorldTweaks", false, true);
        add_option<checkbox_t>("FullBright", false);
        add_option<checkbox_t>("No Rain", false);
        add_option<checkbox_t>("World Time", false, true);
        add_option<slider_float_t>("World Time Hour", 12.0f, 0.0f, 24.0f, "%.2f");

        add_option<checkbox_t>("NoRender", false, true);
        add_option<multi_dropdown_t>("NoRender Targets", bool_t{ false, false, false, false, false }, string_t{ "Fire", "Potions", "Particles", "Totem", "Armor Stands" });
        add_option<checkbox_t>("StorageESP", false, true);
        add_option<dropdown_t>("StorageESP Mode", 0, string_t{ "Default", "Cached" });
        add_option<color_edit_t>("StorageESP Chest", col_t{ 1.0f, 0.54f, 0.12f, 0.80f }, true, "", "");
        add_option<color_edit_t>("StorageESP EnderChest", col_t{ 0.50f, 0.22f, 1.0f, 0.80f }, true, "", "");
        add_option<color_edit_t>("StorageESP Shulker", col_t{ 0.92f, 0.36f, 1.0f, 0.80f }, true, "", "");
        add_option<color_edit_t>("StorageESP Barrel", col_t{ 0.74f, 0.46f, 0.20f, 0.80f }, true, "", "");
        add_option<color_edit_t>("StorageESP Furnace", col_t{ 0.58f, 0.62f, 0.68f, 0.80f }, true, "", "");
        add_option<color_edit_t>("StorageESP Dispenser", col_t{ 0.38f, 0.78f, 1.0f, 0.80f }, true, "", "");
        add_option<color_edit_t>("StorageESP Dropper", col_t{ 0.38f, 1.0f, 0.70f, 0.80f }, true, "", "");
        add_option<color_edit_t>("StorageESP Hopper", col_t{ 1.0f, 0.90f, 0.35f, 0.80f }, true, "", "");

        add_option<checkbox_t>("ItemScroller", false, true);
        add_option<slider_int_t>("ItemScroller Delay", 80, 10, 500, "%d ms");
        add_option<checkbox_t>("MCF", false, true);
        add_option<checkbox_t>("NameProtect", false, true);
        add_option<checkbox_t>("NameProtect On Friends", false);
        add_option<checkbox_t>("AutoRespawn", false, true);
        add_option<checkbox_t>("DiscordRPC", false, true);

        add_option<checkbox_t>("WaterMark", true);
        add_option<checkbox_t>("KeyBinds", true);
        add_option<checkbox_t>("TargetHUD", true);
        add_option<checkbox_t>("Potions", true);
        add_option<slider_float_t>("TargetHUD X Position", 0.f, 0.f, 3840.f, "%.0f");
        add_option<slider_float_t>("TargetHUD Y Position", 0.f, 0.f, 2160.f, "%.0f");
        add_option<slider_float_t>("Potions X Position", 20.f, 0.f, 3840.f, "%.0f");
        add_option<slider_float_t>("Potions Y Position", 110.f, 0.f, 2160.f, "%.0f");

        add_option<settings_dropdown_t>("DPI Menu", "L", 1, string_t{ "75%", "100%", "150%", "200%" });
        add_option<color_edit_t>("Styles", col_t{ 103.f / 255.f, 100.f / 255.f, 1.f, 1.f }, true, "M", "N");

        ensure_config_dir();
        refresh_config_names();
        if (!load_config(active_config)) {
            save_active_config();
        }

        init = true;
    }
}

void c_config::apply_keybinds()
{
    if (var->gui.menu_opened || var->gui.menu_alpha > 0.03f) {
        return;
    }

    static std::unordered_map<std::string, bool> hold_active;

    for (auto& [name, option] : options)
    {
        checkbox_t* checkbox = std::get_if<checkbox_t>(&option);
        if (!checkbox || !checkbox->keybind)
            continue;

        std::vector<int> handled_keys;
        for (const key_data_t& bind : checkbox->state)
        {
            if (bind.key <= 0)
                continue;
            if (std::find(handled_keys.begin(), handled_keys.end(), bind.key) != handled_keys.end())
                continue;
            handled_keys.push_back(bind.key);

            const bool oneShot = is_one_shot_module(name);
            if (bind.mode == 0 && Features::Input::Pressed(bind.key, false)) {
                checkbox->callback = oneShot ? true : !checkbox->callback;
            } else if (bind.mode == 1) {
                const bool down = Features::Input::Down(bind.key);
                const std::string holdKey = name + ":" + std::to_string(bind.key);
                bool& wasDown = hold_active[holdKey];
                if (down) {
                    checkbox->callback = oneShot ? true : bind.value;
                    wasDown = true;
                } else if (wasDown) {
                    checkbox->callback = oneShot ? false : !bind.value;
                    wasDown = false;
                }
            }
        }
    }
}

std::vector<active_bind_t> c_config::active_keybinds() const
{
    std::vector<active_bind_t> binds;

    for (const auto& [name, option] : options)
    {
        const checkbox_t* checkbox = std::get_if<checkbox_t>(&option);
        if (!checkbox || !checkbox->callback || checkbox->state.empty())
            continue;

        for (const key_data_t& bind : checkbox->state)
        {
            if (bind.key <= 0 || !bind.show)
                continue;

            const char* key_name = ImGui::GetKeyName(static_cast<ImGuiKey>(bind.key));
            binds.push_back({ checkbox->name, key_name && key_name[0] ? key_name : "-" });
            break;
        }
    }

    return binds;
}

std::filesystem::path c_config::config_dir() const {
    return std::filesystem::path("C:\\Nvidia\\Legit");
}

std::filesystem::path c_config::config_path(const std::string& name) const {
    return config_dir() / (sanitize_config_name(name) + ".cfg");
}

std::string c_config::sanitize_config_name(const std::string& name) const {
    std::string value;
    value.reserve(name.size());

    for (unsigned char ch : name) {
        if (std::isalnum(ch) || ch == ' ' || ch == '_' || ch == '-') {
            value.push_back(static_cast<char>(ch));
        }
    }

    while (!value.empty() && value.front() == ' ') {
        value.erase(value.begin());
    }
    while (!value.empty() && value.back() == ' ') {
        value.pop_back();
    }
    if (value.empty()) {
        value = "Default";
    }
    if (value.size() > 48) {
        value.resize(48);
    }

    return value;
}

void c_config::ensure_config_dir() const {
    std::error_code ignored;
    std::filesystem::create_directories(config_dir(), ignored);
}

void c_config::refresh_config_names() {
    ensure_config_dir();
    configs.clear();

    std::error_code ignored;
    for (const auto& entry : std::filesystem::directory_iterator(config_dir(), ignored)) {
        if (!entry.is_regular_file(ignored) || entry.path().extension() != ".cfg") {
            continue;
        }

        configs.push_back(entry.path().stem().string());
    }

    if (std::find(configs.begin(), configs.end(), "Default") == configs.end()) {
        configs.push_back("Default");
    }

    std::sort(configs.begin(), configs.end());
    configs.erase(std::unique(configs.begin(), configs.end()), configs.end());

    auto defaultIt = std::find(configs.begin(), configs.end(), "Default");
    if (defaultIt != configs.end() && defaultIt != configs.begin()) {
        std::rotate(configs.begin(), defaultIt, defaultIt + 1);
    }

    if (std::find(configs.begin(), configs.end(), active_config) == configs.end()) {
        active_config = "Default";
    }
}

std::vector<std::string> c_config::config_names() const {
    return configs.empty() ? std::vector<std::string>{ "Default" } : configs;
}

int c_config::active_config_index() const {
    auto it = std::find(configs.begin(), configs.end(), active_config);
    if (it == configs.end()) {
        return 0;
    }
    return static_cast<int>(std::distance(configs.begin(), it));
}

const std::string& c_config::active_config_name() const {
    return active_config;
}

bool c_config::save_active_config() const {
    ensure_config_dir();

    std::ofstream file(config_path(active_config), std::ios::trunc);
    if (!file.is_open()) {
        return false;
    }

    for (const auto& [name, type] : order) {
        auto it = options.find(name);
        if (it == options.end()) {
            continue;
        }

        if (const auto* option = std::get_if<checkbox_t>(&it->second)) {
            file << "checkbox\t" << option->name << '\t' << (option->callback ? 1 : 0) << '\t';
            for (size_t i = 0; i < option->state.size(); ++i) {
                const key_data_t& bind = option->state[i];
                if (i > 0) {
                    file << ';';
                }
                file << bind.key << ',' << bind.mode << ',' << (bind.value ? 1 : 0) << ',' << (bind.show ? 1 : 0);
            }
            file << '\n';
        } else if (const auto* option = std::get_if<slider_int_t>(&it->second)) {
            file << "slider_int\t" << option->name << '\t' << option->callback << '\n';
        } else if (const auto* option = std::get_if<slider_float_t>(&it->second)) {
            file << "slider_float\t" << option->name << '\t' << option->callback << '\n';
        } else if (const auto* option = std::get_if<dropdown_t>(&it->second)) {
            file << "dropdown\t" << option->name << '\t' << option->callback << '\n';
        } else if (const auto* option = std::get_if<multi_dropdown_t>(&it->second)) {
            file << "multi\t" << option->name << '\t';
            for (bool selected : option->callback) {
                file << (selected ? '1' : '0');
            }
            file << '\n';
        } else if (const auto* option = std::get_if<color_edit_t>(&it->second)) {
            file << "color\t" << option->name;
            for (float channel : option->color) {
                file << '\t' << channel;
            }
            file << '\n';
        } else if (const auto* option = std::get_if<settings_dropdown_t>(&it->second)) {
            file << "settings_dropdown\t" << option->name << '\t' << option->callback << '\n';
        }
    }

    return true;
}

bool c_config::load_config(const std::string& name) {
    const std::string safeName = sanitize_config_name(name);
    const std::filesystem::path path = config_path(safeName);

    std::ifstream file(path);
    if (!file.is_open()) {
        active_config = safeName;
        refresh_config_names();
        return false;
    }

    std::string line;
    while (std::getline(file, line)) {
        std::vector<std::string> parts = split_tab(line);
        if (parts.size() < 3) {
            continue;
        }

        const std::string& type = parts[0];
        const std::string optionName = migrated_option_name(parts[1]);

        if (type == "checkbox") {
            checkbox_t* option = find<checkbox_t>(optionName);
            if (!option) {
                continue;
            }

            option->callback = is_one_shot_module(optionName) ? false : as_bool(parts[2]);
            option->state.clear();
            if (parts.size() > 3 && !parts[3].empty()) {
                std::stringstream stream(parts[3]);
                std::string bind;
                while (std::getline(stream, bind, ';')) {
                    std::vector<std::string> values;
                    std::stringstream bindStream(bind);
                    std::string value;
                    while (std::getline(bindStream, value, ',')) {
                        values.push_back(value);
                    }
                    if (values.size() == 4) {
                        const key_data_t bind{ as_int(values[0]), as_int(values[1]), as_bool(values[2]), as_bool(values[3]) };
                        const bool exists = std::any_of(option->state.begin(), option->state.end(), [&](const key_data_t& value) {
                            return value.key == bind.key && value.mode == bind.mode && value.value == bind.value && value.show == bind.show;
                        });
                        if (bind.key > 0 && !exists) {
                            option->state.push_back(bind);
                        }
                    }
                }
            }
        } else if (type == "slider_int") {
            if (slider_int_t* option = find<slider_int_t>(optionName)) {
                int value = as_int(parts[2], option->callback);
                if (is_action_key_option(optionName)) {
                    value = Features::Input::VkFromKeyOrVk(value);
                }
                option->callback = std::clamp(value, option->min, option->max);
            }
        } else if (type == "slider_float") {
            if (slider_float_t* option = find<slider_float_t>(optionName)) {
                option->callback = std::clamp(as_float(parts[2], option->callback), option->min, option->max);
            }
        } else if (type == "dropdown") {
            if (dropdown_t* option = find<dropdown_t>(optionName)) {
                const int maxIndex = option->items.empty() ? 0 : static_cast<int>(option->items.size()) - 1;
                option->callback = std::clamp(as_int(parts[2], option->callback), 0, maxIndex);
            }
        } else if (type == "multi") {
            if (multi_dropdown_t* option = find<multi_dropdown_t>(optionName)) {
                for (size_t i = 0; i < option->callback.size() && i < parts[2].size(); ++i) {
                    option->callback[i] = parts[2][i] == '1';
                }
            }
        } else if (type == "color") {
            if (color_edit_t* option = find<color_edit_t>(optionName)) {
                for (size_t i = 0; i < option->color.size() && i + 2 < parts.size(); ++i) {
                    option->color[i] = std::clamp(as_float(parts[i + 2], option->color[i]), 0.f, 1.f);
                }
            }
        } else if (type == "settings_dropdown") {
            if (settings_dropdown_t* option = find<settings_dropdown_t>(optionName)) {
                const int maxIndex = option->items.empty() ? 0 : static_cast<int>(option->items.size()) - 1;
                option->callback = std::clamp(as_int(parts[2], option->callback), 0, maxIndex);
            }
        }
    }

    active_config = safeName;
    refresh_config_names();
    return true;
}

bool c_config::create_config(const std::string& name) {
    active_config = sanitize_config_name(name);
    if (!save_active_config()) {
        return false;
    }

    refresh_config_names();
    return true;
}

bool c_config::delete_config(const std::string& name) {
    const std::string safeName = sanitize_config_name(name);
    if (safeName == "Default") {
        return false;
    }

    std::error_code ignored;
    const bool removed = std::filesystem::remove(config_path(safeName), ignored);
    if (!removed) {
        refresh_config_names();
        return false;
    }

    if (active_config == safeName) {
        active_config = "Default";
        if (!load_config(active_config)) {
            save_active_config();
        }
    } else {
        refresh_config_names();
    }

    return true;
}

void c_config::auto_save() {
    if (!ImGui::GetCurrentContext()) {
        return;
    }

    const double now = ImGui::GetTime();
    if (now - last_save_time < 1.25) {
        return;
    }

    save_active_config();
    last_save_time = now;
}
