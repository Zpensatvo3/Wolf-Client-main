#pragma once

#include "../module_support.hpp"

namespace modules
{
class storage_esp : public module
	{
	public:
		storage_esp() : module("storage esp", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 250.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("range", range); v.field("show_labels", show_labels);
			v.field("show_distance", show_distance); v.field("box_style", box_style);
		}

	private:
		struct marker { double x, y, z; std::string label; };
		int range = 12;
		bool show_labels = true;
		bool show_distance = true;
		int box_style = 1; // 0 = 2D, 1 = 3D
		int scan_index = 0;
		int origin_x = 0, origin_y = 0, origin_z = 0;
		std::vector<marker> markers{};
		std::mutex marker_mutex{};
	};
}
