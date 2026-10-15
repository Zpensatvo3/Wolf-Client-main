#pragma once

#include "../module_support.hpp"

namespace modules
{
class nametag : public module
	{
	public:
		nametag() : module("nametag", "Visual") {}

		void run(::cache& cache) override;
		void on_disable(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		float get_options_height() override { return 230.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("scale", tag_scale); v.field("max_distance", max_distance);
			v.field("show_health", show_health); v.field("show_armor", show_armor);
		}

	private:
		float tag_scale = 1.0f;
		float max_distance = 96.0f;
		bool show_health = true;
		bool show_armor = true;
	};
}
