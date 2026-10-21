#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/hit_crystal_settings.hpp"

namespace modules
{
class hit_crystal : public module
	{
	public:
		hit_crystal() : module("hit crystal", "Combat") {}

		void run(::cache&) override {};
		void render_overlay() override;
		bool render_when_disabled() const override { return true; }
		void render_options() override;
		void on_enable(::cache&) override { reset_requested_.store(true, std::memory_order_release); }
		void on_disable(::cache&) override { reset_requested_.store(true, std::memory_order_release); }
		void on_render_shutdown() override;
		void cancel_on_client_thread();
		float get_options_height() override { return 300.0f; }
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::hit_crystal::schema()); }

	private:
        modules::settings::store<module_settings::hit_crystal> settings_;
        std::atomic_bool reset_requested_{false};
	};
}
