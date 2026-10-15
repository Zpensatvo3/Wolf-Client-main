#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/auto_totem_settings.hpp"

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
		{ settings_.visit(v, module_settings::auto_totem::schema()); }

	private:
        modules::settings::store<module_settings::auto_totem> settings_;
		bool inventory_was_open = false;
		bool inventory_opened_by_module = false;
		int pending_destination = -1;
		std::chrono::steady_clock::time_point next_action_time{};
		std::chrono::steady_clock::time_point pending_action_time{};
	};
}
