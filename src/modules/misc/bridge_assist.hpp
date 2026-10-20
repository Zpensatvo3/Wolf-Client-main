#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/bridge_assist_settings.hpp"
#include "../../platform/adapter/target_version.hpp"
#include <memory>

namespace modules
{
class bridge_assist : public module
	{
	public:
		bridge_assist();
		~bridge_assist() override;

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
        void render_overlay() override;
        void on_render_shutdown() override;
        bool render_when_disabled() const override { return mc::target.build_id == 12111; }
		float get_options_height() override { return 230.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::bridge_assist::schema()); }

	private:
        struct runtime;
        std::unique_ptr<runtime> runtime_;
        std::atomic_bool native_fallback_{false}, reset_requested_{true};
		modules::settings::store<module_settings::bridge_assist> settings_;
		bool forced_sneak = false;
		bool sneak_state_before_force = false;
		int edge_samples = 0;
		int safe_samples = 0;
		std::chrono::steady_clock::time_point release_deadline{};

		void release_sneak(::cache& cache);
	};
}
