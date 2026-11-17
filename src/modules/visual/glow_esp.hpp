#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/glow_esp_settings.hpp"
#include "../../platform/adapter/target_version.hpp"

namespace modules
{
class glow_esp : public module
	{
	public:
		glow_esp() : module("glow esp", "Visual") {}

		void run(::cache& cache) override;
		void on_disable(::cache& cache) override;
        void maintenance(::cache&) override;
        void on_worker_shutdown(::cache&) override;
		void render_overlay() override;
        bool render_when_disabled() const override { return mc::target.build_id == 12111; }
        void on_render_shutdown() override;
		void render_options() override;
		float get_options_height() override { return 160.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::glow_esp::schema()); }

	private:
        std::atomic_bool reset_requested_{true}, native_fallback_{false};
        modules::settings::store<module_settings::glow_esp> settings_;
	};
}
