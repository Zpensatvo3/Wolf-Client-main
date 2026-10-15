#pragma once

#include "../module_support.hpp"
#include "anchor_engine.hpp"

namespace modules
{
class anchor_macro : public module
	{
	public:
		explicit anchor_macro(bool blatant = false)
			: module(blatant ? "blatant anchor macro" : "anchor macro", "Combat"),
			blatant_mode(blatant)
		{
			if (blatant_mode)
			{
				settings.camera_movement = false;
				settings.switch_delay = 0;
				settings.action_delay = 0;
				settings.return_delay = 0;
				settings.timing_variance = 0;
			}
		}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return blatant_mode ? 100.0f : 360.0f; }
		void visit_config(config_visitor& v) override
		{
            modules::settings::visit(v, settings, anchor_engine::schema(blatant_mode ? anchor_engine::profile::blatant : anchor_engine::profile::legit));
}

	private:
		const bool blatant_mode = false;
		anchor_engine::config settings{};
		anchor_engine::runtime state{};
	};
}
