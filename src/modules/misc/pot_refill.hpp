#pragma once

#include "../module_support.hpp"

namespace modules
{
class pot_refill : public module
	{
	public:
		pot_refill() : module("pot refill", "Player") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 54.0f; }
		void visit_config(config_visitor& v) override { v.field("delay_ms", delay_ms); }

	private:
		int delay_ms = 100;
		std::chrono::steady_clock::time_point next_refill{};
		std::atomic_bool reset_requested{ true };
		int live_status = 0;
		int live_health_potions = 0;
		int live_empty_hotbar = 0;
	};
}
