#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/auto_armor_settings.hpp"

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
		{ settings_.visit(v, module_settings::auto_armor::schema()); }

	private:
		modules::settings::store<module_settings::auto_armor> settings_;
		enum class state { idle, pickup_source, pickup_armor, return_old };
		state current_state = state::idle;
		int source_screen_slot = -1;
		int armor_screen_slot = -1;
		std::chrono::steady_clock::time_point next_action{};
		void reset_runtime();
	};
}
