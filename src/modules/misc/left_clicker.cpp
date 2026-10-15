#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/input/input_service.hpp"
#include "../../services/game/attack_coordinator.hpp"
#include "../../services/game/client_context.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

#include <imgui/imgui.h>

#include "left_clicker.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"
#include "../../mappings.hpp"

namespace
{
	using steady_clock = std::chrono::steady_clock;

	void apply_jitter_camera(maps::ClientPlayerEntity player, float amount, std::mt19937_64& gen)
	{
		if (!player || amount <= 0.0f)
			return;

		std::uniform_real_distribution<float> dist(-amount, amount);
		const float current_yaw = player.yaw.get();
		const float current_pitch = player.pitch.get();
		if (platform::jni_context::clear_exception())
			return;

		player.yaw.set(current_yaw + dist(gen));
		player.pitch.set(std::clamp(current_pitch + dist(gen), -90.0f, 90.0f));
		platform::jni_context::clear_exception();
	}
}

void modules::left_clicker::notify_manual_click(bool pressed)
{
	left_clicker* instance = active_instance.load();
	if (instance)
		instance->manual_down.store(pressed);
}

void modules::left_clicker::schedule_next_click(steady_clock::time_point now)
{
    auto settings_snapshot = settings_.read();
	const int low = std::clamp((std::min)(settings_snapshot.min_cps, settings_snapshot.max_cps), 4, 30);
	const int high = std::clamp((std::max)(settings_snapshot.min_cps, settings_snapshot.max_cps), low, 30);
	std::uniform_int_distribution<int> cps_distribution(low, high);
	double selected_cps = static_cast<double>(cps_distribution(gen));

	if (settings_snapshot.fatigue_enabled && hold_start != steady_clock::time_point{})
	{
		const double seconds_held = std::chrono::duration_cast<std::chrono::milliseconds>(now - hold_start).count() / 1000.0;
		const double fatigue_decay = (std::min)(0.35, seconds_held * 0.035 * static_cast<double>(settings_snapshot.fatigue_factor));
		selected_cps = (std::max)(4.0, selected_cps * (1.0 - fatigue_decay));
	}

	const double jitter = static_cast<double>(std::clamp(settings_snapshot.jitter_percent, 0, 35)) / 100.0;
	std::uniform_real_distribution<double> jitter_distribution(-jitter, jitter);
	const double interval = (1000.0 / selected_cps) * (1.0 + jitter_distribution(gen));

	next_click = now + std::chrono::milliseconds((std::max)(12, static_cast<int>(std::lround(interval))));
}

void modules::left_clicker::run(::cache&)
{
    // Game input and camera changes belong to the client/render thread.
}

void modules::left_clicker::render_overlay()
{
    auto settings_snapshot = settings_.read();
	if (reset_timing.exchange(false) || !enabled) {
		was_held = false; next_click = {}; hold_start = {};
	}
	if (!enabled) return;
	const HWND minecraft_window = gui::get_window();
	const bool held = services::input::physical_button_down(services::input::mouse_button::left);

	if (!held || gui::draw || gui::loader_visible || gui::should_unload || !minecraft_window ||
		GetForegroundWindow() != minecraft_window || !platform::jni_context::bind_current_thread())
	{
		was_held = false;
		next_click = {};
		hold_start = {};
		return;
	}

	try
	{
		jni::frame frame{ 32 };
		services::game::client_context context;
		if (!services::game::capture_client_context(context, true, false) || context.screen ||
			context.player.isAlive.call() != JNI_TRUE || context.player.isUsingItem.call() == JNI_TRUE ||
			platform::jni_context::clear_exception())
		{
			platform::jni_context::clear_exception();
			was_held = false;
			next_click = {};
			hold_start = {};
			return;
		}

		const steady_clock::time_point now = steady_clock::now();
		if (services::combat::trigger_owns_attack())
		{
			// Keep the physical hold intact; reacquire timing after ownership ends.
			was_held = false; next_click = {}; hold_start = {}; return;
		}
		if (!was_held)
		{
			was_held = true;
			hold_start = now;
			schedule_next_click(now);
			return;
		}
		if (now < next_click)
			return;

		bool clicked = false;
		clicked = services::combat::try_unowned_attack([&]() -> bool {
		if (!enabled || reset_timing.load() ||
			!services::input::physical_button_down(services::input::mouse_button::left)) return false;
		if (settings_snapshot.glfw_input)
		{
			return services::input::game_mouse_click(services::input::mouse_button::left);
		}
		else
		{
			if (!context.instance.doAttack.is_valid()) return false;
			context.instance.doAttack.call();
			const bool attacked = !platform::jni_context::clear_exception();
			return attacked;
		}
		});

		if (!clicked)
		{
			was_held = false;
			next_click = {};
			hold_start = {};
			return;
		}

		if (settings_snapshot.jitter_camera && context.player)
		{
			apply_jitter_camera(context.player, settings_snapshot.jitter_amount, gen);
		}

		schedule_next_click(now);
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		was_held = false;
		next_click = {};
		hold_start = {};
	}
}

void modules::left_clicker::on_disable(::cache&)
{
	manual_down.store(false);
	reset_timing.store(true);
}

void modules::left_clicker::render_options()
{
    gui::settings::render(settings_, module_settings::left_clicker::schema());
}
