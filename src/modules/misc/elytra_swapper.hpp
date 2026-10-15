#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/elytra_swapper_settings.hpp"

namespace modules
{
class elytra_swapper : public module
	{
	public:
		elytra_swapper() : module("elytra swapper", "Player") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 205.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::elytra_swapper::schema()); }

	private:
		modules::settings::store<module_settings::elytra_swapper> settings_;
		enum class state { idle, pickup_source, pickup_chest, return_old };
		state current_state = state::idle;
		int source_screen_slot = -1;
		std::chrono::steady_clock::time_point next_action{};

		void finish();
	};
}
