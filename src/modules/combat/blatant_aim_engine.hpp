#pragma once

#include <chrono>
#include <string>

#include "../cache/cache.hpp"

namespace blatant_aim_engine
{
	enum class aim_point
	{
		// Keep the old serialized values for Body and Random so existing public
		// configs continue to load predictably. Old Head/Legs values are migrated
		// to Body/Adaptive by sanitize_point().
		body = 1,
		random = 3,
		adaptive = 4
	};

	struct config
	{
		float fov = 60.0f;
		float max_distance = 4.5f;
		float strength = 0.30f;
		aim_point point = aim_point::body;
		bool only_weapon = false;
		bool last_target = false;
	};

	struct runtime
	{
		bool active = false;
		bool left_button_was_down = false;
		int locked_target_id = -1;
		std::string locked_target_name{};
		int random_target_id = -1;
		float random_ratio = 0.58f;
		float random_goal_ratio = 0.58f;
		std::chrono::steady_clock::time_point random_next_shift{};
		std::chrono::steady_clock::time_point last_update{};
	};

	void run(::cache& cache, config& settings, runtime& state);
	void reset(runtime& state);
	void render_options(config& settings, const char* id_suffix);
}
