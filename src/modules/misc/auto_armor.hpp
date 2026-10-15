#pragma once

#include "../module_support.hpp"

namespace modules
{
class auto_armor : public module
	{
	public:
		auto_armor() : module("auto armor", "Player") {}

		void run(::cache& cache) override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 180.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("action_delay_ms", action_delay_ms); v.field("protect_elytra", protect_elytra);
		}

	private:
		enum class state { idle, pickup_source, pickup_armor, return_old };
		int action_delay_ms = 120;
		bool protect_elytra = true;
		state current_state = state::idle;
		int source_screen_slot = -1;
		int armor_screen_slot = -1;
		std::chrono::steady_clock::time_point next_action{};
		void reset_runtime();
	};
}
