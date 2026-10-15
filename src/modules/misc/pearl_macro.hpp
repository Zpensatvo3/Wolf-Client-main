#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/pearl_macro_settings.hpp"

namespace modules
{
class pearl_macro : public module
	{
	public:
		pearl_macro() : module("pearl macro", "World") {}

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 190.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::pearl_macro::schema()); }

	private:
		modules::settings::store<module_settings::pearl_macro> settings_;
		enum class State { IDLE, WAIT_SWITCH, HOLD_USE, WAIT_RETURN };
		State state = State::IDLE;
		int old_slot = -1;
		int pearl_slot = -1;
		bool right_click_down = false;
		std::chrono::steady_clock::time_point next_action{};

		void finish(::cache& cache, bool turn_off);
	};
}
