#pragma once

#include "../module_support.hpp"
#include <cstdint>

namespace modules
{
class player_esp : public module
	{
	public:
		player_esp() : module("player esp", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		float get_options_height() override { return 300.0f; }
		struct preview_settings
		{
			int mode = 0;
			float thickness = 1.5f;
			float fill_opacity = 0.16f;
			bool names = true;
			bool health = true;
			bool distance = true;
			bool theme_color = false;
			float color[4]{ 0.35f, 0.75f, 1.0f, 1.0f };
		};
		preview_settings get_preview_settings() const
		{
			preview_settings result{};
			result.mode = esp_mode;
			result.thickness = box_thickness;
			result.fill_opacity = fill_opacity;
			result.names = show_names;
			result.health = show_health;
			result.distance = show_distance;
			result.theme_color = use_theme_color;
			for (int i = 0; i < 4; ++i) result.color[i] = esp_color[i];
			return result;
		}
		static std::uintptr_t local_skin_texture_id();
		static void refresh_local_skin_texture();
		void visit_config(config_visitor& v) override
		{
			v.field("esp_mode", esp_mode); v.field("box_thickness", box_thickness);
			v.field("fill_opacity", fill_opacity); v.field("show_names", show_names);
			v.field("show_health", show_health); v.field("show_distance", show_distance);
			v.field("fov", fov_value); v.field("use_game_fov", use_game_fov);
			v.field("use_theme_color", use_theme_color); v.field("esp_color", esp_color);
		}

	private:
		int esp_mode = 0; // 0 = 2D Box, 1 = Corners, 2 = 3D Box
		float box_thickness = 1.5f;
		float fill_opacity = 0.16f;
		bool show_names = true;
		bool show_health = true;
		bool show_distance = true;
		float fov_value = 70.f;
		bool use_game_fov = true;
		bool use_theme_color = false;
		float esp_color[4] = { 0.35f, 0.75f, 1.0f, 1.0f };
	};
}
