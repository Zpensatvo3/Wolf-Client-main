#pragma once

#include "../module_support.hpp"

namespace modules
{
class glow_esp : public module
	{
	public:
		glow_esp() : module("glow esp", "Visual") {}

		void run(::cache& cache) override;
		void on_disable(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		float get_options_height() override { return 160.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("box_thickness", box_thickness); v.field("show_names", show_names);
			v.field("show_health", show_health); v.field("show_distance", show_distance);
			v.field("fov", fov_value); v.field("use_game_fov", use_game_fov);
		}

	private:
		float box_thickness = 1.5f;
		bool show_names = true;
		bool show_health = true;
		bool show_distance = true;
		float fov_value = 70.f;
		bool use_game_fov = true;
	};
}
