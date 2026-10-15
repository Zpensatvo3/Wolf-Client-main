#pragma once

#include "../module_support.hpp"

namespace modules
{
class legit_auto_totem : public module
	{
	public:
		legit_auto_totem() : module("legit auto totem", "Player") {}

		void run(::cache& cache) override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 320.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("reaction_delay_ms", reaction_delay_ms); v.field("action_delay_ms", action_delay_ms);
			v.field("timing_variation_ms", timing_variation_ms); v.field("confirmation_timeout_ms", confirmation_timeout_ms);
			v.field("hotbar_slot", hotbar_slot); v.field("health_threshold", health_threshold);
			v.field("fill_offhand", fill_offhand); v.field("fill_hotbar", fill_hotbar);
			v.field("offhand_first", offhand_first); v.field("low_health_only", low_health_only);
			v.field("cursor_guided_swap", cursor_guided_swap); v.field("cursor_travel_ms", cursor_travel_ms);
			v.field("pause_with_cursor_item", pause_with_cursor_item); v.field("preserve_hotbar", preserve_hotbar);
		}

	private:
		int reaction_delay_ms = 135;
		int action_delay_ms = 165;
		int timing_variation_ms = 45;
		int confirmation_timeout_ms = 360;
		int hotbar_slot = 9;
		float health_threshold = 12.0f;
		bool fill_offhand = true;
		bool fill_hotbar = false;
		bool offhand_first = true;
		bool low_health_only = false;
		bool cursor_guided_swap = true;
		int cursor_travel_ms = 46;
		bool pause_with_cursor_item = true;
		bool preserve_hotbar = true;
		bool inventory_seen = false;
		int pending_destination = -1;
		std::chrono::steady_clock::time_point next_action_time{};
		std::chrono::steady_clock::time_point pending_action_time{};
	};
}
