#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/glow_esp_settings.hpp"

namespace modules
{
class glow_esp : public module
	{
	public:
		glow_esp() : module("glow esp", "Visual") {}

		void run(::cache& cache) override;
		void on_disable(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		float get_options_height() override { return 160.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::glow_esp::schema()); }

	private:
        modules::settings::store<module_settings::glow_esp> settings_;
	};
}
