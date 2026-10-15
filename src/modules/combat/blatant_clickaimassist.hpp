#pragma once

#include "../module_support.hpp"
#include "blatant_aim_engine.hpp"

namespace modules
{
class blatant_clickaimassist : public module
	{
	public:
		blatant_clickaimassist() : module("blatant click aim assist") {}

		void run(::cache& cache) override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 370.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("fov", settings.fov); v.field("max_distance", settings.max_distance);
			v.field("strength", settings.strength); v.enum_field("aim_point", settings.point);
			v.field("only_weapon", settings.only_weapon); v.field("last_target", settings.last_target);
		}

	private:
		blatant_aim_engine::config settings{};
		blatant_aim_engine::runtime state{};
	};
}
