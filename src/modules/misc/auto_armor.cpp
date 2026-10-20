#include "auto_armor.hpp"
#include "gear_policy.hpp"
#include "../../services/game/inventory_bridge.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../ui_redirect.hpp"
#include <algorithm>
#include <chrono>
#include <string>

#include <imgui/imgui.h>

#include "../../gui/gui.hpp"

namespace
{
	using steady_clock = std::chrono::steady_clock;
int armor_type(const maps::ItemStack& stack)
	{
		if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception()) return -1;
		maps::Item item = stack.item.get();
		maps::String name = item && !platform::jni_context::clear_exception() ? item.getTranslationKey.call() : maps::String{};
		const std::string key = name && !platform::jni_context::clear_exception() ? name.to_string() : std::string{};
		platform::jni_context::clear_exception();
        return modules::gear_logic::armor_type(key);
	}

	bool is_elytra(const maps::ItemStack& stack)
	{
		if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception()) return false;
		maps::Item item = stack.item.get();
		maps::String name = item && !platform::jni_context::clear_exception() ? item.getTranslationKey.call() : maps::String{};
		const std::string key = name && !platform::jni_context::clear_exception() ? name.to_string() : std::string{};
		platform::jni_context::clear_exception();
        return modules::gear_logic::elytra(key);
	}

	int armor_score(const maps::ItemStack& stack)
	{
		if (armor_type(stack) < 0) return -1;
		maps::Item item = stack.item.get();
		maps::String name = item && !platform::jni_context::clear_exception() ? item.getTranslationKey.call() : maps::String{};
		const std::string key = name && !platform::jni_context::clear_exception() ? name.to_string() : std::string{};
		platform::jni_context::clear_exception();
        return modules::gear_logic::armor_score(key);
	}

	int screen_slot_from_inventory_slot(int slot)
	{
		if (slot >= 9 && slot < 36) return slot;
		if (slot >= 0 && slot < 9) return 36 + slot;
		return -1;
	}

	int armor_screen_slot(int armor_index)
	{
		// PlayerScreenHandler has head/chest/legs/boots at 5/6/7/8 while
		// PlayerInventory armor indexes are boots/legs/chest/head.
		return 8 - armor_index;
	}

	bool click_slot(::cache& cache, const maps::ScreenHandler& handler, int slot)
	{
		maps::ClientPlayerInteractionManager manager = cache.instance.interactionManager.get();
		maps::SlotActionType pickup = maps::SlotActionType{}.PICKUP.get();
		if (!manager || !pickup || platform::jni_context::clear_exception()) return false;
		manager.clickSlot.call(handler.syncId.get(), slot, 0, pickup, cache.player.toPlayerEntity());
		return !platform::jni_context::clear_exception();
	}
}

void modules::auto_armor::reset_runtime()
{
	current_state = state::idle;
	source_screen_slot = armor_screen_slot = -1;
	next_action = {};
}

void modules::auto_armor::run(::cache& cache)
{
    if constexpr (mc::target.build_id == 12111)
        if (!native_fallback_.load(std::memory_order_acquire)) return;
	const auto settings_snapshot = settings_.read();
	if (gui::draw || !cache.is_valid() || !cache.instance || !cache.player || !jni::get_env())
	{
		reset_runtime();
		return;
	}

	try
	{
		jni::frame frame{ 112 };
		maps::Screen screen = cache.instance.currentScreen.get();
		if (platform::jni_context::clear_exception() ||
			(screen && !screen.is_instance_of<maps::InventoryScreen>()) || platform::jni_context::clear_exception() ||
			(!screen && !settings_snapshot.work_without_inventory))
		{
			reset_runtime();
			return;
		}

		maps::ScreenHandler handler = cache.player.currentScreenHandler.get();
		maps::PlayerInventory inventory = cache.player.getInventory.call();
		if (!handler || !inventory || platform::jni_context::clear_exception()) { reset_runtime(); return; }

		if (current_state == state::idle)
		{
			maps::ItemStack cursor = handler.getCursorStack.call();
			maps::DefaultedList main = inventory.main.get();
			if (!cursor || !main || platform::jni_context::clear_exception() || cursor.isEmpty.call() != JNI_TRUE || platform::jni_context::clear_exception())
				return;

			int selected_source = -1;
			int selected_armor_index = -1;
			int highest_gain = 0;
			for (int source = 0; source < 36; ++source)
			{
				maps::Object object = main.get.call(source);
				if (!object || platform::jni_context::clear_exception()) continue;
				maps::ItemStack candidate(object.object_instance, false);
				const int type = armor_type(candidate);
				const int candidate_score = armor_score(candidate);
				if (type < 0 || candidate_score < 0) continue;

				maps::ItemStack worn = inventory.armor_stack(type);
				if (settings_snapshot.protect_elytra && type == 2 && is_elytra(worn)) continue;
				const int worn_score = armor_score(worn);
				const int gain = candidate_score - worn_score;
				if (gain > highest_gain)
				{
					highest_gain = gain;
					selected_source = source;
					selected_armor_index = type;
				}
			}

			if (selected_source < 0) return;
			source_screen_slot = screen_slot_from_inventory_slot(selected_source);
			armor_screen_slot = ::armor_screen_slot(selected_armor_index);
			if (source_screen_slot < 0 || armor_screen_slot < 0 || !click_slot(cache, handler, source_screen_slot))
			{
				reset_runtime();
				return;
			}
			current_state = state::pickup_source;
			next_action = steady_clock::now() + std::chrono::milliseconds(std::clamp(settings_snapshot.action_delay_ms, 40, 500));
			return;
		}

		if (steady_clock::now() < next_action) return;
		if (current_state == state::pickup_source)
		{
			if (!click_slot(cache, handler, armor_screen_slot)) { reset_runtime(); return; }
			current_state = state::pickup_armor;
			next_action = steady_clock::now() + std::chrono::milliseconds(std::clamp(settings_snapshot.action_delay_ms, 40, 500));
			return;
		}
		if (current_state == state::pickup_armor)
		{
			if (!click_slot(cache, handler, source_screen_slot)) { reset_runtime(); return; }
			current_state = state::return_old;
			next_action = steady_clock::now() + std::chrono::milliseconds(60);
			return;
		}
		if (current_state == state::return_old) reset_runtime();
	}
	catch (...) { platform::jni_context::clear_exception(); reset_runtime(); }
}

void modules::auto_armor::on_disable(::cache&)
{
    if constexpr (mc::target.build_id == 12111) {
        reset_requested_.store(true, std::memory_order_release);
        if (!native_fallback_.load(std::memory_order_acquire)) return;
    }
	reset_runtime();
}

void modules::auto_armor::render_overlay()
{
    if constexpr (mc::target.build_id != 12111) return;
    if (native_fallback_.load(std::memory_order_acquire) || !platform::jni_context::bind_current_thread()) return;
    namespace bridge = services::game::inventory_bridge;
    if (reset_requested_.exchange(false, std::memory_order_acq_rel) || !enabled) {
        reset_runtime(); bridge::cancel(bridge::feature::armor);
        if (!enabled) return;
    }
    jni::frame frame{96};
    const auto settings = settings_.read();
    bridge::snapshot observed;
    const auto backend = bridge::capture(bridge::feature::armor, settings.work_without_inventory, observed);
    if (backend == services::game::hybrid::result::native_fallback) {
        reset_runtime(); native_fallback_.store(true, std::memory_order_release); return;
    }
    const int expected = current_state == state::idle ? 0 : current_state == state::pickup_source ? 1 :
        current_state == state::pickup_armor ? 2 : 3;
    if (backend != services::game::hybrid::result::ready || observed.stage != expected) {
        reset_runtime(); bridge::cancel(bridge::feature::armor); return;
    }
    if (current_state == state::idle) {
        if (!observed.cursor_empty) return;
        const auto [source, type] = gear_logic::choose_armor(observed.keys, settings.protect_elytra);
        if (source < 0) return;
        source_screen_slot = gear_logic::screen_slot(source); armor_screen_slot = 8 - type;
        if (!bridge::act(observed, source_screen_slot, armor_screen_slot, 0, settings.work_without_inventory)) {
            reset_runtime(); bridge::cancel(bridge::feature::armor); return;
        }
        current_state = state::pickup_source;
        next_action = steady_clock::now() + std::chrono::milliseconds(std::clamp(settings.action_delay_ms, 40, 500));
        return;
    }
    if (steady_clock::now() < next_action) return;
    if (current_state == state::return_old) { reset_runtime(); bridge::cancel(bridge::feature::armor); return; }
    if (!bridge::act(observed, source_screen_slot, armor_screen_slot, expected, settings.work_without_inventory)) {
        reset_runtime(); bridge::cancel(bridge::feature::armor); return;
    }
    current_state = current_state == state::pickup_source ? state::pickup_armor : state::return_old;
    next_action = steady_clock::now() + std::chrono::milliseconds(current_state == state::return_old ? 60 :
        std::clamp(settings.action_delay_ms, 40, 500));
}

void modules::auto_armor::on_render_shutdown()
{
    if constexpr (mc::target.build_id == 12111) {
        services::game::inventory_bridge::cancel(services::game::inventory_bridge::feature::armor);
        if (!native_fallback_.load(std::memory_order_acquire)) reset_runtime();
    }
}

void modules::auto_armor::render_options()
{
	gui::settings::render(settings_, module_settings::auto_armor::schema());
}
