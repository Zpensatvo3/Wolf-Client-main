#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/legit_auto_totem_settings.hpp"

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
		{ settings_.visit(v, module_settings::legit_auto_totem::schema()); }

	private:
        modules::settings::store<module_settings::legit_auto_totem> settings_;
		bool inventory_seen = false;
		int pending_destination = -1;
		std::chrono::steady_clock::time_point next_action_time{};
		std::chrono::steady_clock::time_point pending_action_time{};
	};
}
