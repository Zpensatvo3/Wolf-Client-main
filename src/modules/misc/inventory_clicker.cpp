#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/item_bridge.hpp"
#include "../../services/input/input_service.hpp"
#include <algorithm>
#include <chrono>
#include <random>
#include <imgui/imgui.h>

#include "inventory_clicker.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"

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

struct modules::inventory_clicker::runtime
{
    maps::Screen screen{nullptr, true};
    maps::ScreenHandler handler{nullptr, true};
    bool down = false;
    void reset() { screen = {}; handler = {}; down = false; }
};

modules::inventory_clicker::inventory_clicker()
    : module("inventory clicker", "Player"), runtime_(std::make_unique<runtime>()) {}
modules::inventory_clicker::~inventory_clicker() = default;

void modules::inventory_clicker::run(::cache& cache)
{
    if constexpr (mc::target.build_id == 12111)
        if (!native_fallback_.load(std::memory_order_acquire)) return;
    auto settings_snapshot = settings_.read();
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
			settings_snapshot.click_delay = random_click_delay(settings_snapshot.min_cps, settings_snapshot.max_cps);
			settings_.update([&](auto& current) { current.click_delay = settings_snapshot.click_delay; return true; });
			return;
		}

		if (now - last_click_time < settings_snapshot.click_delay)
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
		settings_snapshot.click_delay = random_click_delay(settings_snapshot.min_cps, settings_snapshot.max_cps);
		settings_.update([&](auto& current) { current.click_delay = settings_snapshot.click_delay; return true; });
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
    if constexpr (mc::target.build_id == 12111)
    {
        reset_requested_.store(true, std::memory_order_release);
        if (!native_fallback_.load(std::memory_order_acquire)) return;
    }
    auto settings_snapshot = settings_.read();
	(void)cache;
	previous_lmb_down = false;
	last_click_time = 0;
	settings_snapshot.click_delay = 150;
	settings_.update([&](auto& current) { current.click_delay = settings_snapshot.click_delay; return true; });
}

void modules::inventory_clicker::render_overlay()
{
    if constexpr (mc::target.build_id != 12111) return;
    if (native_fallback_.load(std::memory_order_acquire) || !platform::jni_context::bind_current_thread()) return;
    auto& state = *runtime_;
    if (reset_requested_.exchange(false, std::memory_order_acq_rel)) {
        state.reset(); last_click_time = 0;
        settings_.update([](auto& settings) { settings.click_delay = 150; return true; });
    }
    if (!enabled || !services::input::physical_button_down(services::input::mouse_button::left)) {
        state.reset(); last_click_time = 0; return;
    }
    struct cleanup { ~cleanup() { platform::jni_context::clear_exception(); } } cleanup;
    jni::frame frame{32};
    services::game::items::inventory_snapshot observed;
    const auto backend = services::game::items::capture_inventory(observed);
    if (backend == services::game::hybrid::result::native_fallback) {
        state.reset(); last_click_time = 0; previous_lmb_down = false;
        native_fallback_.store(true, std::memory_order_release);
        return;
    }
    if (backend != services::game::hybrid::result::ready) { state.reset(); last_click_time = 0; return; }
    const maps::Screen screen{observed.screen, false};
    const maps::ScreenHandler handler{observed.handler, false};
    if (!state.screen || !state.screen.is_same_object(screen) || !state.handler.is_same_object(handler)) {
        state.reset();
        state.screen = screen;
        state.handler = handler;
        last_click_time = 0;
    }
    const auto settings = settings_.read();
    const auto now = now_milliseconds();
    if (!state.down) {
        state.down = true; last_click_time = now;
        const int delay = random_click_delay(settings.min_cps, settings.max_cps);
        settings_.update([&](auto& current) { current.click_delay = delay; return true; });
        return; // Keep the original physical click first, including a new screen.
    }
    if (now - last_click_time < settings.click_delay) return;
    if (observed.slot < 0) { last_click_time = now; return; }
    if (!enabled || reset_requested_.load(std::memory_order_acquire) || !services::game::items::pickup(observed)) {
        state.reset(); last_click_time = 0; return;
    }
    last_click_time = now;
    const int delay = random_click_delay(settings.min_cps, settings.max_cps);
    settings_.update([&](auto& current) { current.click_delay = delay; return true; });
}

void modules::inventory_clicker::on_render_shutdown()
{
    if constexpr (mc::target.build_id == 12111) {
        if (!native_fallback_.load(std::memory_order_acquire)) { runtime_->reset(); last_click_time = 0; }
    }
}

void modules::inventory_clicker::render_options()
{
    gui::settings::render(settings_, module_settings::inventory_clicker::schema());
}
