#pragma once

#include "../module_support.hpp"

namespace modules
{
class inventory_clicker : public module
	{
	public:
		inventory_clicker() : module("inventory clicker", "Player") {}

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 200.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("min_cps", min_cps); v.field("max_cps", max_cps); v.field("click_delay", click_delay);
		}

	private:
		int min_cps = 4;
		int max_cps = 7;
		long long last_click_time = 0;
		int click_delay = 150;
	};
}
