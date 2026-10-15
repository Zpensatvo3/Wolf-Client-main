#pragma once

#include "../module_support.hpp"
#include "../../gui/gui.hpp"

struct ImDrawList;

namespace modules
{
	class arraylist : public module
	{
	public:
		arraylist() : module("arraylist", "Visual") { enabled = true; } // enable by default

		// Screen rectangle the overlay actually occupies. The HUD editor uses it as
		// its drag handle so the preview and the real overlay can never drift apart.
		struct overlay_bounds
		{
			float minimum_x = 0.0f;
			float minimum_y = 0.0f;
			float maximum_x = 0.0f;
			float maximum_y = 0.0f;

			float width() const { return maximum_x - minimum_x; }
			float height() const { return maximum_y - minimum_y; }
		};

		// Single drawing path shared by the live overlay and the HUD editor preview.
		// right_margin is the distance to the right screen edge, top the distance to
		// the upper edge. Returns whether anything was drawn.
		static bool draw_overlay(ImDrawList* draw_list, float right_margin, float top,
			bool preview_mode, overlay_bounds* out_bounds);

		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;

		// Six controls: scale, background opacity, bar width, bar glow, text shadow
		// and the keybind badge.
		float get_options_height() override { return 270.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("scale", gui::arraylist_scale);
			v.field("position_x", gui::arraylist_pos_x);
			v.field("position_y", gui::arraylist_pos_y);
			v.field("bar_glow", bar_glow);
			v.field("text_shadow", text_shadow);
			v.field("bg_opacity", bg_opacity);
			v.field("bar_width", bar_width);
			v.field("show_keybinds", show_keybinds);
		}

		inline static bool bar_glow = true;
		inline static bool text_shadow = true;
		inline static bool show_keybinds = true;
		inline static float bg_opacity = 0.72f;
		inline static float bar_width = 2.0f;
	};
}
