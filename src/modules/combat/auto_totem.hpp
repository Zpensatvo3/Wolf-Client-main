#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/auto_totem_settings.hpp"
#include <chrono>
#include <optional>

namespace modules
{
class auto_totem : public module
	{
	public:
		auto_totem() : module("auto totem", "Player", "auto_totem") {}

		void run(::cache& cache) override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return settings_.read().mode == 1 ? 610.f : 460.f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::auto_totem::schema()); }

	private:
        modules::settings::store<module_settings::auto_totem> settings_;
		std::optional<module_settings::auto_totem> applied_settings_;
		void reset_runtime();
		void run_normal(::cache& cache, const module_settings::auto_totem& settings_snapshot);
		void run_legit(::cache& cache, const module_settings::auto_totem& settings_snapshot);
		bool inventory_was_open = false;
		bool inventory_opened_by_module = false;
		int pending_destination = -1;
		std::chrono::steady_clock::time_point next_action_time{};
		std::chrono::steady_clock::time_point pending_action_time{};
	};
}
