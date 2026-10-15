#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/right_clicker_settings.hpp"

namespace modules
{
class right_clicker : public module
	{
	public:
		right_clicker() : module("right clicker", "Player") { active_instance.store(this); }
		~right_clicker() override
		{
			right_clicker* expected = this;
			active_instance.compare_exchange_strong(expected, nullptr);
		}

		void run(::cache& cache) override;
		void render_overlay() override;
		bool render_when_disabled() const override { return true; }
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 290.0f; }
		static void notify_manual_click(bool pressed);
		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::right_clicker::schema()); }

	private:
        modules::settings::store<module_settings::right_clicker> settings_;
		inline static std::atomic<right_clicker*> active_instance{ nullptr };
		std::atomic<bool> manual_down{ false };
		std::atomic<bool> reset_timing{ true };
		bool was_held = false;
		std::chrono::steady_clock::time_point next_click{};
		std::chrono::steady_clock::time_point hold_start{};
		void schedule_next_click(std::chrono::steady_clock::time_point now);
	};
}
