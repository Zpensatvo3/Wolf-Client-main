#pragma once

#include <chrono>
#include <mutex>

#include "../cache/cache.hpp"

namespace anchor_engine
{
	enum class profile
	{
		legit,
		blatant,
		safe
	};

	struct config
	{
		bool hybrid_slot_input = false;
		bool camera_movement = true;
		int switch_delay = 28;
		int action_delay = 38;
		int shield_delay = 45;
		int return_delay = 24;
		int timing_variance = 14;
		float camera_strength = 0.28f;
	};

	struct runtime
	{
		enum class phase
		{
			idle,
			place_anchor,
			wait_anchor,
			charge_anchor,
			wait_charge,
			place_shield,
			wait_shield,
			detonate,
			wait_detonation,
			restore
		};

		std::mutex mutex{};
		phase current = phase::idle;
		int old_slot = -1;
		int anchor_slot = -1;
		int glowstone_slot = -1;
		int totem_slot = -1;
		int shield_slot = -1;
		int support_x = 0;
		int support_y = 0;
		int support_z = 0;
		int face_x = 0;
		int face_y = 1;
		int face_z = 0;
		int anchor_x = 0;
		int anchor_y = 0;
		int anchor_z = 0;
		int retries = 0;
		int confirmation_timeout_ms = 0;
		std::chrono::steady_clock::time_point next_action{};
		std::chrono::steady_clock::time_point phase_started{};
		std::chrono::steady_clock::time_point last_attempt{};
		std::chrono::steady_clock::time_point next_camera_move{};
	};

	// Returns false after a one-shot sequence completes or aborts; the module
	// wrapper then turns itself off.
	bool run_overlay(config& settings, runtime& state, profile mode);
	void stop(::cache& cache, runtime& state);
	void render_options(config& settings, profile mode, const char* id_suffix);
}
