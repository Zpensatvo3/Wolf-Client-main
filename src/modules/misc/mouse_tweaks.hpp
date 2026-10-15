#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/mouse_tweaks_settings.hpp"

namespace modules
{
class mouse_tweaks : public module
	{
	public:
		mouse_tweaks() : module("mouse tweaks", "Player") {}

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 120.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::mouse_tweaks::schema()); }

	private:
		modules::settings::store<module_settings::mouse_tweaks> settings_;
	};
}
