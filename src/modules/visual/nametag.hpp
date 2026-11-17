#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/nametag_settings.hpp"

namespace modules
{
class nametag : public module
	{
	public:
		nametag() : module("nametag", "Visual") {}

		void run(::cache& cache) override;
		void on_disable(::cache& cache) override;
        void maintenance(::cache& cache) override;
        void on_worker_shutdown(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		float get_options_height() override { return 230.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::nametag::schema()); }

	private:
        modules::settings::store<module_settings::nametag> settings_;
	};
}
