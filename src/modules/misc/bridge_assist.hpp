#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/bridge_assist_settings.hpp"

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
		{ settings_.visit(v, module_settings::bridge_assist::schema()); }

	private:
		modules::settings::store<module_settings::bridge_assist> settings_;
		bool forced_sneak = false;
		bool sneak_state_before_force = false;
		int edge_samples = 0;
		int safe_samples = 0;
		std::chrono::steady_clock::time_point release_deadline{};

		void release_sneak(::cache& cache);
	};
}
