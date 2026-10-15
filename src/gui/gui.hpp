#pragma once
#include <Windows.h>
#include <imgui/imgui.h>
#include "widgets.hpp"

namespace gui
{
	enum class icon_asset
	{
		helmet,
		background,
		spotify
	};

	inline bool draw = false;
	inline bool should_unload = false;
	inline bool loader_visible = true;
	// Fixed shortcut: Click GUI is always opened with Right Shift.
	inline int menu_key = VK_RSHIFT;
	inline bool menu_key_scanning = false;
	inline bool suppress_menu_key_until_release = false;
	inline int active_theme = 0; // 0: onestep, 1: Crimson, 2: Emerald, 3: Ocean, 4: Rose, 5: Cyber, 6: Sunset, 7: Mono
	inline float hud_scale = 1.0f;
	inline float arraylist_scale = 1.12f;
	inline int hud_style = 0; // 0: Glass, 1: Minimal, 2: Compact
	inline int active_language = 0; // 0: English, 1: Turkish
	inline float hud_pos_x = 10.0f;
	inline float hud_pos_y = 10.0f;
	inline float arraylist_pos_x = 10.0f;
	inline float arraylist_pos_y = 10.0f;
	inline bool hud_editor_active = false;
	inline bool hud_layout_editor_active = false;
	inline ImFont* arraylist_font = nullptr;
	inline ImFont* icon_font = nullptr;
	inline ImFont* ui_font = nullptr;
	inline ImFont* ui_bold_font = nullptr;
	inline ImFont* watermark_font = nullptr;

	inline bool is_turkish()
	{
		return active_language == 1;
	}

	inline const char* text(const char* english, const char* turkish)
	{
		return is_turkish() ? turkish : english;
	}

	bool init();
	void shutdown();

	int get_last_pressed_key();
	void reset_last_pressed_key();
	HWND get_window();

	ImColor get_dynamic_color(int offset = 0);
	ImTextureID get_icon_texture(icon_asset asset);
	void render_click_gui_options();
	void render_theme_selection();
	void request_panic_animation();
	void settings_stepper(const char* label, float* value, float minimum, float maximum, float step = 0.05f, const char* format = "%.2f");
	void settings_coordinate_row(float* x_value, float* y_value);
	bool centered_text_button(const char* label, const ImVec2& size);
}
