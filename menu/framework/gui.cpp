#include "headers/includes.h"
#include "headers/blur.h"

#include <algorithm>

namespace
{
	void render_status_text(std::string_view title, std::string_view text)
	{
		widgets->begin_child(title);
		{
			ImVec2 min = gui->get_screen_pos();
			ImVec2 max = min + ImVec2(gui->content_avail().x, SCALE(34));
			draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 14), min, max, draw->get_clr(clr->widgets.text_inactive), text.data(), nullptr, nullptr, ImVec2(0.f, 0.5f));
			gui->dummy(SCALE(0, 34));
		}
		widgets->end_child();
	}

	void sync_style_color_from_settings()
	{
		color_edit_t& style = cfg->get<color_edit_t>("Styles");
		static bool initialized{ false };
		if (!initialized)
		{
			const bool old_white_default = style.color[0] >= 0.99f && style.color[1] >= 0.99f && style.color[2] >= 0.99f;
			if (old_white_default)
			{
				style.color[0] = 103.f / 255.f;
				style.color[1] = 100.f / 255.f;
				style.color[2] = 1.f;
				style.color[3] = 1.f;
			}
			initialized = true;
		}

		clr->accent.Value.x = style.color[0];
		clr->accent.Value.y = style.color[1];
		clr->accent.Value.z = style.color[2];
		clr->accent.Value.w = style.color[3];

		clr->widgets.section_on = ImColor(
			std::clamp(style.color[0] * 0.35f + 0.18f, 0.f, 1.f),
			std::clamp(style.color[1] * 0.35f + 0.18f, 0.f, 1.f),
			std::clamp(style.color[2] * 0.35f + 0.18f, 0.f, 1.f),
			1.f
		);
		clr->widgets.widgets_active = ImColor(
			std::clamp(style.color[0] * 0.18f + 0.10f, 0.f, 1.f),
			std::clamp(style.color[1] * 0.18f + 0.10f, 0.f, 1.f),
			std::clamp(style.color[2] * 0.18f + 0.10f, 0.f, 1.f),
			1.f
		);
	}

	void render_action_key(std::string_view label, std::string_view key)
	{
		slider_int_t& value = cfg->get<slider_int_t>(std::string(key));
		gui->dummy(SCALE(0, 12));
		ImVec2 min = gui->get_screen_pos();
		ImVec2 max = min + ImVec2(gui->content_avail().x, SCALE(22));
		draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 14), min, max, draw->get_clr(clr->widgets.text_inactive), label.data(), nullptr, nullptr, ImVec2(0.f, 0.5f));
		gui->dummy(SCALE(0, 22));
		widgets->action_key_select(key, &value.callback);
		gui->dummy(SCALE(0, 14));
	}

	void render_combat_tab()
	{
		gui->begin_group();
		{
			widgets->begin_child("HITBOX");
			{
				widgets->checkbox("HitBox");
				widgets->slider_float("HitBox X Position");
			}
			widgets->end_child();

			widgets->begin_child("SHIFT TAP");
			{
				widgets->checkbox("ShiftTap");
			}
			widgets->end_child();
		}
		gui->end_group();

		gui->sameline();

		gui->begin_group();
		{
			widgets->begin_child("TRIGGERBOT");
			{
				widgets->checkbox("TriggerBot");
				widgets->slider_float("TriggerBot Distance");
				widgets->multi_dropdown("TriggerBot Targets");
				widgets->multi_dropdown("TriggerBot No Hit");
				widgets->checkbox("Smart Crits");
			}
			widgets->end_child();

			widgets->begin_child("NO FRIEND DAMAGE");
			{
				widgets->checkbox("NoFriendDamage");
			}
			widgets->end_child();
		}
		gui->end_group();
	}

	void render_visual_tab()
	{
		gui->begin_group();
		{
			widgets->begin_child("ESP");
			{
				widgets->checkbox("ESP");
				widgets->multi_dropdown("ESP Elements");
				widgets->dropdown("ESP Box Style");
				if (cfg->get<dropdown_t>("ESP Box Style").callback == 2)
				{
					gui->dummy(SCALE(0, 4));
					widgets->color_edit("ESP 3D Box Color");
				}
			}
			widgets->end_child();

			widgets->begin_child("WORLD TWEAKS");
			{
				widgets->checkbox("WorldTweaks");
				widgets->checkbox("FullBright");
				widgets->checkbox("No Rain");
			}
			widgets->end_child();

			widgets->begin_child("STORAGE ESP");
			{
				widgets->checkbox("StorageESP");
				widgets->dropdown("StorageESP Mode");
				gui->dummy(SCALE(0, 4));
				widgets->color_edit("StorageESP Chest");
				widgets->color_edit("StorageESP EnderChest");
				widgets->color_edit("StorageESP Shulker");
				widgets->color_edit("StorageESP Barrel");
				widgets->color_edit("StorageESP Furnace");
				widgets->color_edit("StorageESP Dispenser");
				widgets->color_edit("StorageESP Dropper");
				widgets->color_edit("StorageESP Hopper");
			}
			widgets->end_child();
		}
		gui->end_group();

		gui->sameline();

		gui->begin_group();
		{
			widgets->begin_child("NO RENDER");
			{
				widgets->checkbox("NoRender");
				widgets->multi_dropdown("NoRender Targets");
			}
			widgets->end_child();

			widgets->begin_child("WORLD TIME");
			{
				widgets->checkbox("World Time");
				widgets->slider_float("World Time Hour");
			}
			widgets->end_child();

			widgets->begin_child("HUD");
			{
				widgets->checkbox("WaterMark");
				widgets->checkbox("KeyBinds");
				widgets->checkbox("TargetHUD");
				widgets->checkbox("Potions");
			}
			widgets->end_child();
		}
		gui->end_group();
	}

	void render_movement_tab()
	{
		gui->begin_group();
		{
			widgets->begin_child("SPRINT");
			{
				widgets->checkbox("Sprint");
			}
			widgets->end_child();

			widgets->begin_child("GUI MOVE");
			{
				widgets->checkbox("GuiMove");
			}
			widgets->end_child();
		}
		gui->end_group();
	}

	void render_misc_tab()
	{
		gui->begin_group();
		{
			widgets->begin_child("ITEM SCROLLER");
			{
				widgets->checkbox("ItemScroller");
				widgets->slider_int("ItemScroller Delay");
			}
			widgets->end_child();

			widgets->begin_child("MCF");
			{
				widgets->checkbox("MCF");
			}
			widgets->end_child();

			widgets->begin_child("DISCORD RPC");
			{
				widgets->checkbox("DiscordRPC");
			}
			widgets->end_child();
		}
		gui->end_group();

		gui->sameline();

		gui->begin_group();
		{
			widgets->begin_child("NAME PROTECT");
			{
				widgets->checkbox("NameProtect");
				widgets->checkbox("NameProtect On Friends");
			}
			widgets->end_child();

			widgets->begin_child("AUTO RESPAWN");
			{
				widgets->checkbox("AutoRespawn");
			}
			widgets->end_child();

			widgets->begin_child("KEYBINDS");
			{
				std::vector<active_bind_t> binds = cfg->active_keybinds();
				if (binds.empty())
				{
					ImVec2 min = gui->get_screen_pos();
					ImVec2 max = min + ImVec2(gui->content_avail().x, SCALE(34));
					draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 14), min, max, draw->get_clr(clr->widgets.text_inactive), "No active binds", nullptr, nullptr, ImVec2(0.f, 0.5f));
					gui->dummy(SCALE(0, 34));
				}
				else
				{
					for (const active_bind_t& bind : binds)
					{
						ImVec2 min = gui->get_screen_pos();
						ImVec2 max = min + ImVec2(gui->content_avail().x, SCALE(30));
						draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 14), min, max, draw->get_clr(clr->widgets.text), bind.name.data(), nullptr, nullptr, ImVec2(0.f, 0.5f));
						draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 12), min, max, draw->get_clr(clr->accent), bind.key.data(), nullptr, nullptr, ImVec2(1.f, 0.5f));
						gui->dummy(SCALE(0, 30));
					}
				}
			}
			widgets->end_child();
		}
		gui->end_group();
	}

	void render_empty_tab(std::string_view title)
	{
		gui->begin_group();
		{
			render_status_text(title, "No modules in this category yet.");
		}
		gui->end_group();
	}

	void render_hud_overlay(const ImVec2& base)
	{
		sync_style_color_from_settings();

		ImDrawList* list = gui->window_drawlist();
		static float watermark_alpha = 0.f;
		static float keybinds_alpha = 0.f;
		static std::vector<active_bind_t> shown_binds;

		std::vector<active_bind_t> binds = cfg->active_keybinds();
		if (!binds.empty())
			shown_binds = binds;

		const bool watermark_enabled = cfg->get<checkbox_t>("WaterMark").callback;
		const bool keybinds_enabled = cfg->get<checkbox_t>("KeyBinds").callback && !binds.empty();

		gui->easing(watermark_alpha, watermark_enabled ? 1.f : 0.f, 7.f, static_easing);
		gui->easing(keybinds_alpha, keybinds_enabled ? 1.f : 0.f, 7.f, static_easing);

		if (watermark_alpha <= 0.01f && keybinds_alpha <= 0.01f)
		{
			if (!keybinds_enabled)
				shown_binds.clear();
			return;
		}

		if (watermark_alpha > 0.01f)
		{
			const ImVec2 size = SCALE(154, 36);
			draw->rect_filled(list, base, base + size, draw->get_clr(clr->window.child, 0.92f * watermark_alpha), SCALE(8));
			draw->rect(list, base, base + size, draw->get_clr(clr->accent, 0.48f * watermark_alpha), SCALE(8));

			const ImVec2 logo_min = base + SCALE(8, 7);
			const ImVec2 logo_max = logo_min + SCALE(22, 22);
			draw->rect_filled(list, logo_min, logo_max, draw->get_clr(clr->widgets.section_on, 0.20f * watermark_alpha), SCALE(5));
			draw->text_clipped(list, font->get(icon_data, 18), logo_min, logo_max, draw->get_clr(clr->accent, watermark_alpha), "A", nullptr, nullptr, ImVec2(0.5f, 0.5f));

			draw->text_clipped(list, font->get(inter_medium_data, 15), base + SCALE(38, 0), base + size, draw->get_clr(clr->widgets.text, watermark_alpha), "Luma Legit", nullptr, nullptr, ImVec2(0.f, 0.5f));
		}

		if (keybinds_alpha > 0.01f && !shown_binds.empty())
		{
			float width = SCALE(145);
			for (const active_bind_t& bind : shown_binds)
			{
				width = ImMax(width, gui->text_size(font->get(inter_medium_data, 13), bind.name.data()).x + gui->text_size(font->get(inter_medium_data, 12), bind.key.data()).x + SCALE(44));
			}

			const ImVec2 pos = base + SCALE(0, 44);
			const float height = SCALE(30 + 24 * static_cast<float>(shown_binds.size()));
			draw->rect_filled(list, pos, pos + ImVec2(width, height), draw->get_clr(clr->window.child, 0.92f * keybinds_alpha), SCALE(8));
			draw->rect(list, pos, pos + ImVec2(width, height), draw->get_clr(clr->window.rect, 0.8f * keybinds_alpha), SCALE(8));
			draw->text_clipped(list, font->get(inter_medium_data, 13), pos + SCALE(12, 0), pos + ImVec2(width, SCALE(30)), draw->get_clr(clr->widgets.text, keybinds_alpha), "KeyBinds", nullptr, nullptr, ImVec2(0.f, 0.5f));

			float y = SCALE(30);
			for (const active_bind_t& bind : shown_binds)
			{
				draw->text_clipped(list, font->get(inter_medium_data, 13), pos + ImVec2(SCALE(12), y), pos + ImVec2(width - SCALE(12), y + SCALE(22)), draw->get_clr(clr->widgets.text_inactive, keybinds_alpha), bind.name.data(), nullptr, nullptr, ImVec2(0.f, 0.5f));
				draw->text_clipped(list, font->get(inter_medium_data, 12), pos + ImVec2(SCALE(12), y), pos + ImVec2(width - SCALE(12), y + SCALE(22)), draw->get_clr(clr->accent, keybinds_alpha), bind.key.data(), nullptr, nullptr, ImVec2(1.f, 0.5f));
				y += SCALE(24);
			}
		}

		if (!keybinds_enabled && keybinds_alpha <= 0.02f)
			shown_binds.clear();
	}
}

void c_gui::render()
{
	gui->initialize();
	sync_style_color_from_settings();
	gui->set_next_window_size(SCALE(var->window.size));
	gui->begin("menu", nullptr, var->window.flags);
	{
		gui->set_style();
		gui->draw_decorations();

		{
			gui->set_pos(SCALE(elements->section.distance_rect), pos_y);
			gui->begin_content("section", SCALE(elements->section.size), SCALE(elements->section.padding), SCALE(elements->section.spacing), window_flags_no_move);
			{
				for (int i = 0; i < static_cast<int>(var->gui.sections.size()); i++)
					widgets->section(var->gui.sections[i].text, var->gui.sections[i].icon, i, var->gui.section_index);
			}
			gui->end_content();

			gui->set_pos(ImVec2(SCALE(elements->section.padding.x), gui->window_height() - SCALE(elements->section.padding.y + elements->button.settings_size.y)), pos_all);
			if (widgets->settings_button(var->gui.image_texture).value_changed)
				var->gui.settings_opened = true;

			gui->easing(var->gui.settings_alpha, var->gui.settings_opened ? 1.f : 0.f, 8.f, static_easing);

			if (var->gui.settings_alpha > 0.f)
			{
				gui->set_next_window_pos(gui->window_pos() + ImVec2(SCALE(elements->section.padding.x), gui->window_height() - SCALE(elements->section.padding.y + elements->button.settings_size.y)));
				gui->set_next_window_size(SCALE(var->gui.settings_size));
				gui->push_var(style_var_alpha, var->gui.settings_alpha);
				gui->push_var(style_var_window_padding, SCALE(0, 0));
				gui->push_var(style_var_item_spacing, SCALE(0, 1));
				gui->begin("settings_window", nullptr, window_flags_tooltip | window_flags_no_decoration | window_flags_no_scrollbar | window_flags_no_scroll_with_mouse | window_flags_no_background | window_flags_no_nav);
				{
					var->gui.settings_hovered = gui->get_window()->Rect().Contains(gui->mouse_pos()) || (GImGui->HoveredWindow && (strstr(GImGui->HoveredWindow->Name, "coloredit_window") || strstr(GImGui->HoveredWindow->Name, "dropdown_window")));
					if (!var->gui.settings_hovered && gui->mouse_clicked(mouse_button_left))
						var->gui.settings_opened = false;

					gui->begin_content("blur_s", SCALE(var->gui.settings_size));
					{
						draw_background_blur(gui->window_drawlist(), var->device, var->context, SCALE(var->window.rounding));
						draw->rect_filled(gui->window_drawlist(), gui->window_pos(), gui->window_pos() + gui->window_size(), draw->get_clr(clr->window.background, 0.9f), SCALE(var->window.rounding));
					}
					gui->end_content();

					gui->set_pos(ImVec2(0, 0), pos_all);
					gui->begin_content("top", ImVec2(0, SCALE(var->gui.settings_top)));
					{
						draw->image_rounded(gui->window_drawlist(), var->gui.image_texture, gui->window_pos() + SCALE(elements->section.padding), gui->window_pos() + SCALE(elements->section.padding + elements->button.settings_size_opened), ImVec2(0, 0), ImVec2(1, 1), draw->get_clr({ 1.f, 1.f, 1.f, 1.f }), SCALE(elements->button.settings_size_opened.x / 2));
						draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 14), gui->window_pos() + SCALE(elements->section.padding.x * 2 + elements->button.settings_size_opened.x, elements->section.padding.y), gui->window_pos() + gui->window_size(), draw->get_clr(clr->widgets.text), "Past Owl", NULL, NULL, ImVec2(0.f, 0.f));
						draw->text_clipped(gui->window_drawlist(), font->get(inter_medium_data, 12), gui->window_pos() + SCALE(elements->section.padding.x * 2 + elements->button.settings_size_opened.x, 0), gui->window_pos() + ImVec2(gui->window_width(), gui->window_height() - SCALE(elements->section.padding.y)), draw->get_clr(clr->widgets.text_inactive), "Till: 1 Jan 2025", NULL, NULL, ImVec2(0.f, 1.f));
					}
					gui->end_content();

					gui->begin_content("bottom", ImVec2(0, 0), SCALE(elements->section.padding), SCALE(elements->section.padding), window_flags_no_scrollbar | window_flags_no_scroll_with_mouse);
					{
						widgets->dropdown_settings("Language");
						widgets->dropdown_settings("DPI Menu");
						const int dpi_callback = cfg->get<settings_dropdown_t>("DPI Menu").callback;
						const int wanted_dpi =
							dpi_callback == 0 ? 75 :
							dpi_callback == 1 ? 100 :
							dpi_callback == 2 ? 150 : 200;
						if (var->gui.stored_dpi != wanted_dpi)
						{
							var->gui.stored_dpi = wanted_dpi;
							var->gui.dpi_changed = true;
						}
						widgets->color_edit("Styles");
						sync_style_color_from_settings();

					}
					gui->end_content();

					draw->line(gui->window_drawlist(), gui->window_pos() + SCALE(elements->section.padding.x, var->gui.settings_top), gui->window_pos() + ImVec2(gui->window_width() - SCALE(elements->section.padding.x), SCALE(var->gui.settings_top)), draw->get_clr(clr->widgets.widgets_active), SCALE(1));
				}
				gui->end();
				gui->pop_var(3);
			}
		}

		{
			gui->set_pos(SCALE(elements->section.size.x, 0), pos_all);
			gui->begin_content("bar", SCALE(elements->bar.size), SCALE(elements->bar.margin, elements->bar.margin), SCALE(elements->bar.padding, elements->bar.padding));
			{
				widgets->search_field("search", search->search_buf, sizeof search->search_buf);
				search->search_element = search->search_buf;

				gui->sameline();

				std::vector<std::string> items = cfg->config_names();
				static int selected_cfg{ 0 };
				selected_cfg = cfg->active_config_index();
				if (!items.empty() && widgets->dropdown_cfg("drpdwn_cfg", &selected_cfg, items).value_changed)
					cfg->load_config(items[selected_cfg]);

				gui->sameline();
				if (widgets->button_page("X").value_changed && cfg->active_config_name() != "Default")
				{
					cfg->delete_config(cfg->active_config_name());
					selected_cfg = cfg->active_config_index();
				}

				gui->sameline();

				static bool create_cfg_open{ false };
				static char new_cfg[48]{ };
				if (widgets->button_page("+").value_changed)
					create_cfg_open = !create_cfg_open;

				if (create_cfg_open)
				{
					gui->sameline();
					widgets->config_name_field("cfg_new", new_cfg, sizeof new_cfg);
					gui->sameline();
					if (widgets->button_page("Save").value_changed && strlen(new_cfg) > 0)
					{
						if (cfg->create_config(new_cfg))
						{
							memset(new_cfg, 0, sizeof new_cfg);
							create_cfg_open = false;
						}
					}
				}
			}
			gui->end_content();
		}

		if (var->gui.section_index != var->gui.active_section)
		{
			gui->easing(var->gui.section_alpha, 0.f, 10.f, static_easing);
			if (var->gui.section_alpha <= 0.02f)
				var->gui.active_section = var->gui.section_index;
		}
		else
		{
			gui->easing(var->gui.section_alpha, 1.f, 10.f, static_easing);
		}

		{
			gui->set_pos(SCALE(65, 80), pos_all);
			gui->begin_content("child", SCALE(0, 0), SCALE(elements->child.margin, 0), SCALE(elements->child.margin, elements->child.margin), window_flags_no_scrollbar);
			{
				gui->push_var(style_var_alpha, var->gui.section_alpha);
				if (strlen(search->search_buf) > 0)
				{
					widgets->begin_child("SEARCH");
					{
						search->search();
					}
					widgets->end_child();
				}
				else if (var->gui.active_section == 0)
				{
					render_combat_tab();
				}
				else if (var->gui.active_section == 1)
				{
					render_movement_tab();
				}
				else if (var->gui.active_section == 4)
				{
					render_misc_tab();
				}
				else if (var->gui.active_section == 3)
				{
					render_empty_tab("RENDER");
				}
				else
				{
					render_visual_tab();
				}
				gui->pop_var();

				gui->dummy(ImVec2(0, 0));
			}
			gui->end_content();
		}
	}
	gui->end();
}

void c_gui::render_hud()
{
	render_hud_overlay(SCALE(20, 20));
}

std::string c_gui::lang(std::string ru, std::string en) {
	if (cfg->get<settings_dropdown_t>("Language").callback == 1) return ru;
	return en;
}
