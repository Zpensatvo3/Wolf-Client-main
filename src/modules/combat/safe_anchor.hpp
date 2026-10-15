#pragma once

#include "../module_support.hpp"
#include "anchor_engine.hpp"

namespace modules
{
class safe_anchor : public module
	{
	public:
		safe_anchor() : module("safe anchor", "Combat")
		{
			settings.camera_strength = 0.24f;
			settings.switch_delay = 30;
			settings.action_delay = 42;
			settings.return_delay = 25;
		}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 410.0f; }
		void visit_config(config_visitor& v) override
		{
            modules::settings::visit(v, settings, anchor_engine::schema(anchor_engine::profile::safe));
}

	private:
		anchor_engine::config settings{};
		anchor_engine::runtime state{};
	};
}
