#pragma once

#include "../module_support.hpp"

namespace modules
{
class radar : public module
	{
	public:
		radar() : module("radar", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 490.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("size", radar_size); v.field("range", radar_range);
			v.field("position_x", position_x); v.field("position_y", position_y);
			v.field("background_opacity", background_opacity); v.field("rotate_with_player", rotate_with_player);
			v.field("show_names", show_names); v.field("show_distance_rings", show_distance_rings);
			v.field("show_cardinals", show_cardinals); v.field("show_player_count", show_player_count);
		}

	private:
		float radar_size = 150.0f;
		float radar_range = 48.0f;
		float position_x = 18.0f;
		float position_y = 92.0f;
		float background_opacity = 0.78f;
		bool rotate_with_player = true;
		bool show_names = false;
		bool show_distance_rings = true;
		bool show_cardinals = true;
		bool show_player_count = true;
	};
}
