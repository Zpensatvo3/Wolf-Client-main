#pragma once

#include "../module_support.hpp"

namespace modules
{
	// Now-playing overlay built on top of spotify::source.
	class spotify_hud : public module
	{
	public:
		static constexpr float fixed_scale = 1.2f;
		spotify_hud() : module("spotify hud", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_enable(::cache& cache) override;
		void on_disable(::cache& cache) override;

		float get_options_height() override { return show_lyrics ? 422.0f : 384.0f; }

		float get_pos_x() const { return position_x; }
		float get_pos_y() const { return position_y; }
		void set_pos_x(float x) { position_x = x; }
		void set_pos_y(float y) { position_y = y; }
		float get_scale() const { return fixed_scale; }

		struct bounds
		{
			float x = 0.0f;
			float y = 0.0f;
			float w = 0.0f;
			float h = 0.0f;
		};
		bounds get_bounds(float display_w, float display_h) const;
		void render_hud_preview(ImDrawList* draw, float x, float y);

		void visit_config(config_visitor& v) override
		{
			v.field("position_x", position_x); v.field("position_y", position_y);
			v.field("scale", scale); v.field("background_opacity", background_opacity);
			v.field("show_artwork", show_artwork); v.field("show_progress", show_progress);
			v.field("show_visualizer", show_visualizer); v.field("show_brand", show_brand);
			v.field("show_lyrics", show_lyrics); v.field("lyric_lines", lyric_lines);
			v.field("enable_lyrics", enable_lyrics);
			v.field("show_when_idle", show_when_idle);
			// Retain the config keys while enforcing the compact HUD's fixed layout.
			scale = fixed_scale;
			lyric_lines = 1;
		}

	private:
		float position_x = 22.0f;
		float position_y = 60.0f;
		float scale = fixed_scale;
		float background_opacity = 0.72f;

		bool show_artwork = true;
		bool show_progress = true;
		bool show_visualizer = true;
		bool show_brand = true;
		bool show_lyrics = true;
		int lyric_lines = 1;
		bool enable_lyrics = true;
		bool show_when_idle = true;

		bool applied_lyrics = false;
	};
}
