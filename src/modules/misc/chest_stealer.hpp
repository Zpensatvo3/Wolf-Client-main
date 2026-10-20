#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/chest_stealer_settings.hpp"
#include "../../platform/adapter/target_version.hpp"
#include <memory>

namespace modules
{
class chest_stealer : public module
	{
	public:
		chest_stealer();
		~chest_stealer() override;

		void run(::cache& cache) override;
		void render_overlay() override;
		bool render_when_disabled() const override { return mc::target.build_id == 12111; }
		void on_render_shutdown() override;
		void on_disable(::cache& cache) override;
		void render_options() override;
		float get_options_height() override { return 215.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::chest_stealer::schema()); }

	private:
		struct runtime;
		std::unique_ptr<runtime> runtime_;
		modules::settings::store<module_settings::chest_stealer> settings_;
		bool chest_seen = false;
		std::chrono::steady_clock::time_point next_action_time{};
		std::atomic_bool reset_requested{ true };
	};
}
