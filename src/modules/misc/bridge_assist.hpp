#pragma once

#include "../module_support.hpp"

namespace modules
{
class bridge_assist : public module
	{
	public:
		bridge_assist() : module("bridge assist", "Movement") {}

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 230.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("edge_offset", edge_offset); v.field("unsneak_delay", unsneak_delay);
			v.field("require_block_item", require_block_item); v.field("only_while_moving", only_while_moving);
		}

	private:
		float edge_offset = 0.08f;
		int unsneak_delay = 55;
		bool require_block_item = true;
		bool only_while_moving = true;
		bool forced_sneak = false;
		bool sneak_state_before_force = false;
		int edge_samples = 0;
		int safe_samples = 0;
		std::chrono::steady_clock::time_point release_deadline{};

		void release_sneak(::cache& cache);
	};
}
