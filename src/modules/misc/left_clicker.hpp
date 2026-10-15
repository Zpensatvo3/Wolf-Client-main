#pragma once

#include "../module_support.hpp"

namespace modules
{
class left_clicker : public module
	{
	public:
		left_clicker() : module("left clicker", "Player") { active_instance.store(this); }
		~left_clicker() override
		{
			left_clicker* expected = this;
			active_instance.compare_exchange_strong(expected, nullptr);
		}

		void run(::cache& cache) override;
		void render_overlay() override;
		bool render_when_disabled() const override { return true; }
		void render_options() override;
		void on_disable(::cache& cache) override;
		float get_options_height() override { return 270.0f; }
		static void notify_manual_click(bool pressed);
		void visit_config(config_visitor& v) override
		{
			v.field("min_cps", min_cps); v.field("max_cps", max_cps);
			v.field("jitter_percent", jitter_percent); v.field("jitter_camera", jitter_camera);
			v.field("jitter_amount", jitter_amount); v.field("fatigue_enabled", fatigue_enabled);
			v.field("fatigue_factor", fatigue_factor); v.field("glfw_input", glfw_input);
		}

	private:
		inline static std::atomic<left_clicker*> active_instance{ nullptr };
		std::atomic<bool> manual_down{ false };
		std::atomic<bool> reset_timing{ true };
		int min_cps = 10;
		int max_cps = 14;
		int jitter_percent = 8;
		bool jitter_camera = false;
		float jitter_amount = 0.25f;
		bool fatigue_enabled = true;
		float fatigue_factor = 0.35f;
		bool glfw_input = true;
		bool was_held = false;
		std::chrono::steady_clock::time_point next_click{};
		std::chrono::steady_clock::time_point hold_start{};
		void schedule_next_click(std::chrono::steady_clock::time_point now);
	};
}
