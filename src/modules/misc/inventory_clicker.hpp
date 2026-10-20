#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/inventory_clicker_settings.hpp"
#include "../../platform/adapter/target_version.hpp"
#include <memory>

namespace modules
{
class inventory_clicker : public module
	{
	public:
        inventory_clicker();
        ~inventory_clicker() override;

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
        void render_overlay() override;
        void on_render_shutdown() override;
        bool render_when_disabled() const override { return mc::target.build_id == 12111; }
		float get_options_height() override { return 200.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::inventory_clicker::schema()); }

	private:
        modules::settings::store<module_settings::inventory_clicker> settings_;
		long long last_click_time = 0;
        struct runtime;
        std::unique_ptr<runtime> runtime_;
        std::atomic_bool native_fallback_{false}, reset_requested_{true};
	};
}
