#pragma once

#include <chrono>
#include <mutex>

#include "../../cache/cache.hpp"
#include "anchor_config.hpp"
#include "../../services/game/overlay_context.hpp"

namespace anchor_engine
{
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
		services::game::overlay_session session;
		int owned_slot = -1;
		phase current = phase::idle;
		int old_slot = -1;
		int anchor_slot = -1;
		int glowstone_slot = -1;
		int totem_slot = -1;
		int shield_slot = -1;
		int shield_x = 0, shield_y = 0, shield_z = 0;
		bool shield_planned = false;
		int last_action_tick = -1;
		std::chrono::steady_clock::time_point sequence_started{};
		const char* status = "Aim at a block or anchor, then enable";
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
		int retry_interval_ms = 65;
		std::chrono::steady_clock::time_point next_action{};
		std::chrono::steady_clock::time_point phase_started{};
		std::chrono::steady_clock::time_point last_attempt{};
		std::chrono::steady_clock::time_point next_camera_move{};
	};

	// Returns false after a one-shot sequence completes or aborts; the module
	// wrapper then turns itself off.
	bool tick(config& settings, runtime& state, profile mode);
	void cancel_on_client_thread(runtime& state);
}
