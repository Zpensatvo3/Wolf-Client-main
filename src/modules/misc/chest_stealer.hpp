#pragma once

#include "../module_support.hpp"

namespace modules
{
class chest_stealer : public module
	{
	public:
		chest_stealer() : module("chest stealer", "World") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 215.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("min_delay_ms", min_delay_ms); v.field("max_delay_ms", max_delay_ms);
			v.field("open_reaction_ms", open_reaction_ms); v.field("random_slot_order", random_slot_order);
		}

	private:
		int min_delay_ms = 85;
		int max_delay_ms = 145;
		int open_reaction_ms = 95;
		bool random_slot_order = true;
		bool chest_seen = false;
		std::chrono::steady_clock::time_point next_action_time{};
		std::atomic_bool reset_requested{ true };
	};
}
