#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/inventory_clicker_settings.hpp"

namespace modules
{
class inventory_clicker : public module
	{
	public:
		inventory_clicker() : module("inventory clicker", "Player") {}

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 200.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::inventory_clicker::schema()); }

	private:
        modules::settings::store<module_settings::inventory_clicker> settings_;
		long long last_click_time = 0;
	};
}
