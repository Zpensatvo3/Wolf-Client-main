#pragma once

#include "../module_support.hpp"
#include "../../gui/gui.hpp"

namespace modules
{
class hud : public module
	{
	public:
		hud() : module("watermark", "Visual") { enabled = true; } // enable by default

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		std::pair<std::string, int> status_snapshot() const;
		float get_options_height() override { return 86.0f; }
		void visit_config(config_visitor& v) override
		{
			v.field("scale", gui::hud_scale); v.field("position_x", gui::hud_pos_x); v.field("position_y", gui::hud_pos_y);
		}

	private:
		mutable std::mutex status_mutex{};
		std::string player_name{ "Player" };
		int latency_ms = 0;
		std::chrono::steady_clock::time_point next_status_refresh{};
	};
}
