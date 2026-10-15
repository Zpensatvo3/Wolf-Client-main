#pragma once

#include "../module_support.hpp"

namespace modules
{
class block_esp : public module
	{
	public:
		block_esp() : module("block esp", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 500.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("range", range); v.field("line_thickness", line_thickness);
			v.field("fill_opacity", fill_opacity); v.field("selected_blocks", selected_blocks);
		}

	private:
		struct marker { double x, y, z; std::string key; };
		int range = 8;
		float line_thickness = 1.6f;
		float fill_opacity = 0.08f;
		int scan_index = 0;
		int origin_x = 0, origin_y = 0, origin_z = 0;
		std::vector<marker> markers{};
		std::mutex marker_mutex{};
		std::vector<std::string> available_blocks{};
		std::vector<std::string> selected_blocks{
			"minecraft:diamond_ore", "minecraft:deepslate_diamond_ore",
			"minecraft:ancient_debris"
		};
		bool registry_loaded = false;
		char block_search[64]{};
	};
}
