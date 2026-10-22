#pragma once

#include "../module_support.hpp"
#include "anchor_engine.hpp"
#include "../core/settings_store.hpp"

namespace modules
{
	class safe_anchor : public module
	{
	public:
		safe_anchor();
		~safe_anchor() override;

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		void on_enable(::cache&) override;
		void on_render_shutdown() override { cancel_on_client_thread(); }
		void cancel_on_client_thread();
		bool render_when_disabled() const override { return true; }
		float get_options_height() override { return 460.0f; }
		void visit_config(config_visitor& v) override
		{
			if (settings_.visit(v, anchor_engine::schema(anchor_engine::profile::safe)))
				reset_requested_.store(true,std::memory_order_release);
		}

		static void client_tick();

	private:
		static inline safe_anchor* instance_{nullptr};
		static anchor_engine::config defaults()
		{
			anchor_engine::config value{};
			value.camera_movement=false;
			value.camera_strength=.24f; value.switch_delay=30; value.action_delay=42; value.return_delay=25;
			return value;
		}
		settings::store<anchor_engine::config> settings_;
		std::atomic_bool reset_requested_{false};
		anchor_engine::runtime state{};
	};
}
