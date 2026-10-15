#pragma once

#include "../module_support.hpp"

namespace modules
{
class trajectories : public module
	{
	public:
		trajectories() : module("trajectories", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 245.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("show_bows", show_bows); v.field("show_throwables", show_throwables);
			v.field("show_potions", show_potions); v.field("show_landing_marker", show_landing_marker);
			v.field("show_flight_time", show_flight_time); v.field("line_thickness", line_thickness);
			v.field("max_ticks", max_ticks);
		}

	private:
		bool show_bows = true;
		bool show_throwables = true;
		bool show_potions = true;
		bool show_landing_marker = true;
		bool show_flight_time = true;
		float line_thickness = 2.0f;
		int max_ticks = 160;
	};
}
