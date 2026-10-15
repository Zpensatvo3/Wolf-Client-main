#pragma once

#include "../module_support.hpp"

namespace modules
{
class elytra_swapper : public module
	{
	public:
		elytra_swapper() : module("elytra swapper", "Player") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 205.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("target_mode", target_mode); v.field("swap_delay_ms", swap_delay_ms);
			v.field("use_hotbar", use_hotbar);
		}

	private:
		enum class state { idle, pickup_source, pickup_chest, return_old };
		int target_mode = 0; // 0 = elytra, 1 = chestplate
		int swap_delay_ms = 70;
		bool use_hotbar = true;
		state current_state = state::idle;
		int source_screen_slot = -1;
		std::chrono::steady_clock::time_point next_action{};

		void finish();
	};
}
