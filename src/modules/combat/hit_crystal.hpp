#pragma once

#include "../module_support.hpp"

namespace modules
{
class hit_crystal : public module
	{
	public:
		hit_crystal() : module("hit crystal", "Combat") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 300.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("auto_switch", auto_switch); v.field("restore_slot", restore_slot);
			v.field("place_delay", place_delay); v.field("break_delay", break_delay);
		}

	private:
			enum class State
		{
			SEEK_PLACE,
			WAIT_PLACE,
			WAIT_CRYSTAL,
			WAIT_BREAK_CONFIRM,
			WAIT_REPLACE
		};

		bool auto_switch = true;
		bool restore_slot = true;
		int place_delay = 28;
		int break_delay = 24;
		int old_slot = -1;
		int crystal_slot = -1;
		int target_crystal_id = -1;
		int retries = 0;
		State state = State::SEEK_PLACE;
		int base_x = 0;
		int base_y = 0;
		int base_z = 0;
		double target_x = 0.0;
		double target_y = 0.0;
		double target_z = 0.0;
		std::chrono::steady_clock::time_point next_action_time{};
		std::chrono::steady_clock::time_point state_started{};
		std::mutex state_mutex{};

		void stop_clicking(::cache& cache, bool restore);
	};
}
