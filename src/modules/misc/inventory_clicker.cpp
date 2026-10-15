#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include <algorithm>
#include <chrono>
#include <random>
#include <imgui/imgui.h>

#include "inventory_clicker.hpp"
#include "../ui_redirect.hpp"
#include "../gui/gui.hpp"

namespace
{
	using steady_clock = std::chrono::steady_clock;

	bool previous_lmb_down = false;
long long now_milliseconds()
	{
		return std::chrono::duration_cast<std::chrono::milliseconds>(
			steady_clock::now().time_since_epoch()).count();
	}

	int random_click_delay(int min_cps, int max_cps)
	{
		min_cps = std::clamp(min_cps, 1, 20);
		max_cps = std::clamp(max_cps, min_cps, 20);

		static thread_local std::mt19937 generator{ std::random_device{}() };
		std::uniform_int_distribution<int> cps_distribution(min_cps, max_cps);
		std::uniform_int_distribution<int> jitter_distribution(-10, 10);

		const int cps = cps_distribution(generator);
		return std::max(25, (1000 / cps) + jitter_distribution(generator));
	}
}

void modules::inventory_clicker::run(::cache& cache)
{
	const bool lmb_down = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
	HWND minecraft_window = gui::get_window();

	// gui::get_window() is the HWND captured from Minecraft's OpenGL context.
	// GetActiveWindow() cannot be used here because this module runs on a worker
	// thread and would normally return null for the game's UI thread.
	if (gui::draw || !minecraft_window ||
		GetForegroundWindow() != minecraft_window ||
		!cache.is_valid() || !cache.player || !cache.instance || !jni::get_env())
	{
		previous_lmb_down = lmb_down;
		last_click_time = 0;
		return;
	}

	try
	{
		jni::frame frame{ 32 };

		maps::Screen current_screen = cache.instance.currentScreen.get();
		if (!current_screen || platform::jni_context::clear_exception() ||
			!current_screen.is_instance_of<maps::HandledScreen>())
		{
			platform::jni_context::clear_exception();
			previous_lmb_down = lmb_down;
			last_click_time = 0;
			return;
		}

		if (!lmb_down)
		{
			previous_lmb_down = false;
			last_click_time = 0;
			return;
		}

		const long long now = now_milliseconds();
		if (!previous_lmb_down)
		{
			// Let Minecraft process the user's original physical click first.
			previous_lmb_down = true;
			last_click_time = now;
			click_delay = random_click_delay(min_cps, max_cps);
			return;
		}

		if (now - last_click_time < click_delay)
			return;

		maps::HandledScreen handled_screen(current_screen.object_instance, false);
		maps::ScreenHandler handler = handled_screen.handler.get();
		maps::Slot focused_slot = handled_screen.focusedSlot.get();
		if (!handler || !focused_slot || platform::jni_context::clear_exception())
		{
			last_click_time = now;
			return;
		}

		const int slot_id = focused_slot.id.get();
		maps::ClientPlayerInteractionManager interaction_manager =
			cache.instance.interactionManager.get();
		maps::PlayerEntity player = cache.player.toPlayerEntity();
		maps::SlotActionType pickup = maps::SlotActionType{}.PICKUP.get();

		if (slot_id < 0 || !interaction_manager || !player || !pickup ||
			platform::jni_context::clear_exception())
		{
			last_click_time = now;
			return;
		}

		// Use Minecraft's inventory action directly instead of global SendInput.
		// This cannot click desktop applications when the game loses focus.
		interaction_manager.clickSlot.call(
			handler.syncId.get(),
			static_cast<jint>(slot_id),
			0,
			pickup,
			player);

		platform::jni_context::clear_exception();
		last_click_time = now;
		click_delay = random_click_delay(min_cps, max_cps);
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		last_click_time = 0;
		previous_lmb_down = lmb_down;
	}
}

void modules::inventory_clicker::on_disable(::cache& cache)
{
	(void)cache;
	previous_lmb_down = false;
	last_click_time = 0;
	click_delay = 150;
}

void modules::inventory_clicker::render_options()
{
	ImGui::SliderInt(gui::text("Min CPS", "Minimum CPS"), &min_cps, 1, 20);
	ImGui::SliderInt(gui::text("Max CPS", "Maksimum CPS"), &max_cps, 1, 20);

	min_cps = std::clamp(min_cps, 1, 20);
	max_cps = std::clamp(max_cps, min_cps, 20);
}
