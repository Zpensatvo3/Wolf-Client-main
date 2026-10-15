#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/trajectories_settings.hpp"

namespace modules
{
class trajectories : public module
	{
	public:
		trajectories() : module("trajectories", "Visual") {}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 245.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::trajectories::schema()); }

	private:
        modules::settings::store<module_settings::trajectories> settings_;
	};
}
