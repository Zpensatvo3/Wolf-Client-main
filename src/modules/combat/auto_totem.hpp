#pragma once

#include "../module_support.hpp"

namespace modules
{
class auto_totem : public module
	{
	public:
		auto_totem() : module("auto totem", "Player") {}

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 450.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("fill_offhand", fill_offhand); v.field("fill_hotbar", fill_hotbar);
			v.field("auto_open_inventory", auto_open_inventory); v.field("offhand_first", offhand_first);
			v.field("pause_with_cursor_item", pause_with_cursor_item); v.field("hotbar_slot", hotbar_slot);
			v.field("delay_ms", delay_ms); v.field("timing_randomness_ms", timing_randomness_ms);
			v.field("initial_reaction_ms", initial_reaction_ms); v.field("confirmation_timeout_ms", confirmation_timeout_ms);
		}

	private:
		bool fill_offhand = true;
		bool fill_hotbar = true;
		bool auto_open_inventory = true;
		bool offhand_first = true;
		bool pause_with_cursor_item = true;
		int hotbar_slot = 9;
		int delay_ms = 75;
		int timing_randomness_ms = 30;
		int initial_reaction_ms = 110;
		int confirmation_timeout_ms = 260;
		bool inventory_was_open = false;
		bool inventory_opened_by_module = false;
		int pending_destination = -1;
		std::chrono::steady_clock::time_point next_action_time{};
		std::chrono::steady_clock::time_point pending_action_time{};
	};
}
