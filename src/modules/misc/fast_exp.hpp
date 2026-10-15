#pragma once

#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/fast_exp_settings.hpp"
#include <atomic>
#include <chrono>
#include <random>

namespace modules
{
	class fast_exp : public module
	{
	public:
		fast_exp() : module("fast exp", "World") { active_instance.store(this); }
		~fast_exp() override
		{
			fast_exp* expected = this;
			active_instance.compare_exchange_strong(expected, nullptr);
		}

		void run(::cache& cache) override;
		void render_overlay() override;
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 140.0f; }

		static void notify_manual_right_click(bool pressed);

		void visit_config(config_visitor& v) override
		{ settings_.visit(v, module_settings::fast_exp::schema()); }

	private:
        modules::settings::store<module_settings::fast_exp> settings_;
		inline static std::atomic<fast_exp*> active_instance{ nullptr };
		std::atomic<bool> manual_right_down{ false };
		bool was_held = false;
		bool blocked_until_release = false;

		std::chrono::steady_clock::time_point next_click{};
		std::mt19937_64 gen{ std::random_device{}() };

		void stop();
		void schedule_next_click(std::chrono::steady_clock::time_point now);
	};
}
