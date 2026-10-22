#include "elytra_swapper.hpp"
#include "gear_policy.hpp"
#include "../../services/game/inventory_bridge.hpp"
#include "../../services/game/client_bridge.hpp"
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
	constexpr int chest_armor_screen_slot = 6;
bool stack_matches_mode(const maps::ItemStack& stack, int mode)
	{
		if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
			return false;
		maps::Item item = stack.item.get();
		if (!item || platform::jni_context::clear_exception())
			return false;

		maps::String translation = item.getTranslationKey.call();
		const std::string key = translation && !platform::jni_context::clear_exception()
			? translation.to_string()
			: std::string{};
		platform::jni_context::clear_exception();
        return modules::gear_logic::matches(key, mode);
	}

	int screen_slot_from_inventory_slot(int inventory_slot)
	{
		if (inventory_slot >= 9 && inventory_slot < 36)
			return inventory_slot;
		if (inventory_slot >= 0 && inventory_slot < 9)
			return 36 + inventory_slot;
		return -1;
	}

	int find_target_slot(const maps::DefaultedList& inventory, int target_mode, bool use_hotbar)
	{
		for (int slot = 9; slot < 36; ++slot)
		{
			maps::Object object = inventory.get.call(slot);
			if (object && !platform::jni_context::clear_exception() &&
				stack_matches_mode(maps::ItemStack(object.object_instance, false), target_mode))
				return slot;
		}

		if (use_hotbar)
		{
			for (int slot = 0; slot < 9; ++slot)
			{
				maps::Object object = inventory.get.call(slot);
				if (object && !platform::jni_context::clear_exception() &&
					stack_matches_mode(maps::ItemStack(object.object_instance, false), target_mode))
					return slot;
			}
		}
		return -1;
	}

	bool click_slot(const maps::MinecraftClient& instance,
		const maps::ClientPlayerEntity& player,
		const maps::ScreenHandler& handler,
		int slot)
	{
		maps::ClientPlayerInteractionManager manager = instance.interactionManager.get();
		maps::SlotActionType pickup = maps::SlotActionType{}.PICKUP.get();
		if (!manager || !pickup || platform::jni_context::clear_exception())
			return false;
		if (player && player.isSprinting.call() == JNI_TRUE && !platform::jni_context::clear_exception())
			player.setSprinting.call(JNI_FALSE);
		platform::jni_context::clear_exception();
		manager.clickSlot.call(handler.syncId.get(), slot, 0, pickup, player.toPlayerEntity());
		return !platform::jni_context::clear_exception();
	}
}

void modules::elytra_swapper::finish()
{
	current_state = state::idle;
	source_screen_slot = -1;
	next_action = {};
	enabled = false;
	services::game::client_bridge::set_inventory_busy(false);
}

void modules::elytra_swapper::run(::cache&)
{
	// Inventory interaction is performed from render_overlay(), which runs on
	// Minecraft's render/client thread. The module worker remains read-only.
}

void modules::elytra_swapper::render_overlay()
{
	const auto settings_snapshot = settings_.read();
	if (!platform::jni_context::bind_current_thread())
		return;
    if constexpr (mc::target.build_id == 12111) {
        if (reset_requested_.exchange(false, std::memory_order_acq_rel) || !enabled) {
            current_state = state::idle; source_screen_slot = -1; next_action = {};
            services::game::inventory_bridge::cancel(services::game::inventory_bridge::feature::elytra);
            if (!enabled) return;
        }
    }

	try
	{
		jni::frame frame{ 80 };
        namespace bridge = services::game::inventory_bridge;
        bridge::snapshot observed;
        const auto backend = bridge::capture(bridge::feature::elytra, settings_snapshot.work_without_inventory, observed);
        if (backend != services::game::hybrid::result::native_fallback) {
            const auto stop = [&] { bridge::cancel(bridge::feature::elytra); finish(); };
            if (backend != services::game::hybrid::result::ready) { stop(); return; }
            if (observed.stage == -2 && current_state == state::idle) return; // Armed until inventory opens.
            const int expected = current_state == state::idle ? 0 : current_state == state::pickup_source ? 1 :
                current_state == state::pickup_chest ? 2 : 3;
            if (observed.stage != expected) { stop(); return; }
            if (current_state == state::idle) {
                if (!observed.cursor_empty || gear_logic::matches(observed.keys[38], settings_snapshot.target_mode)) { stop(); return; }
                const int source = gear_logic::choose_swap(observed.keys, settings_snapshot.target_mode, settings_snapshot.use_hotbar);
                source_screen_slot = gear_logic::screen_slot(source);
                services::game::client_bridge::set_inventory_busy(true);
                if (source < 0 || !bridge::act(observed, source_screen_slot, 6, 0, settings_snapshot.work_without_inventory)) { stop(); return; }
                current_state = state::pickup_source;
                next_action = steady_clock::now() + std::chrono::milliseconds(std::clamp(settings_snapshot.swap_delay_ms, 20, 300));
                return;
            }
            if (steady_clock::now() < next_action) return;
            if (current_state == state::return_old) { stop(); return; }
            if (!bridge::act(observed, source_screen_slot, 6, expected, settings_snapshot.work_without_inventory)) { stop(); return; }
            current_state = current_state == state::pickup_source ? state::pickup_chest : state::return_old;
            next_action = steady_clock::now() + std::chrono::milliseconds(current_state == state::return_old ? 40 :
                std::clamp(settings_snapshot.swap_delay_ms, 20, 300));
            return;
        }
		maps::MinecraftClient instance = maps::MinecraftClient{}.instance.get();
		maps::ClientPlayerEntity player = instance ? instance.player.get() : maps::ClientPlayerEntity{};
		if (!instance || !player || platform::jni_context::clear_exception())
		{
			finish();
			return;
		}

		maps::Screen screen = instance.currentScreen.get();
		if (platform::jni_context::clear_exception())
		{
			finish();
			return;
		}
		// The player's own ScreenHandler exists with the inventory GUI closed.
		// Other open screens have a different slot layout and are always rejected.
		if (!screen && !settings_snapshot.work_without_inventory)
			return; // Keep the one-shot armed until the inventory is opened.
		if ((screen && !screen.is_instance_of<maps::InventoryScreen>()) || platform::jni_context::clear_exception())
		{
			platform::jni_context::clear_exception();
			finish();
			return;
		}

		maps::ScreenHandler handler = player.currentScreenHandler.get();
		maps::PlayerInventory inventory = player.getInventory.call();
		if (!handler || !inventory || platform::jni_context::clear_exception())
		{
			finish();
			return;
		}

		if (current_state == state::idle)
		{
			maps::ItemStack cursor = handler.getCursorStack.call();
			maps::DefaultedList main = inventory.main.get();
			if (!cursor || !main || platform::jni_context::clear_exception() ||
				cursor.isEmpty.call() != JNI_TRUE || platform::jni_context::clear_exception())
			{
				finish();
				return;
			}

			// PlayerInventory armor index 2 is the chest equipment slot.
			maps::ItemStack chest_object = inventory.armor_stack(2);
			if (chest_object && !platform::jni_context::clear_exception() &&
				stack_matches_mode(maps::ItemStack(chest_object.object_instance, false), settings_snapshot.target_mode))
			{
				finish();
				return;
			}

			const int inventory_slot = find_target_slot(main, settings_snapshot.target_mode, settings_snapshot.use_hotbar);
			source_screen_slot = screen_slot_from_inventory_slot(inventory_slot);
			if (source_screen_slot < 0 || !click_slot(instance, player, handler, source_screen_slot))
			{
				finish();
				return;
			}
			current_state = state::pickup_source;
			next_action = steady_clock::now() + std::chrono::milliseconds(
				std::clamp(settings_snapshot.swap_delay_ms, 20, 300));
			return;
		}

		if (steady_clock::now() < next_action)
			return;

		if (current_state == state::pickup_source)
		{
			if (!click_slot(instance, player, handler, chest_armor_screen_slot))
			{
				finish();
				return;
			}
			current_state = state::pickup_chest;
			next_action = steady_clock::now() + std::chrono::milliseconds(
				std::clamp(settings_snapshot.swap_delay_ms, 20, 300));
			return;
		}

		if (current_state == state::pickup_chest)
		{
			if (!click_slot(instance, player, handler, source_screen_slot))
			{
				finish();
				return;
			}
			current_state = state::return_old;
			next_action = steady_clock::now() + std::chrono::milliseconds(40);
			return;
		}

		if (current_state == state::return_old)
			finish();
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		finish();
	}
}

void modules::elytra_swapper::on_disable(::cache&)
{
    services::game::client_bridge::set_inventory_busy(false);
    if constexpr (mc::target.build_id == 12111) { reset_requested_.store(true, std::memory_order_release); return; }
	current_state = state::idle;
	source_screen_slot = -1;
	next_action = {};
}

void modules::elytra_swapper::on_render_shutdown()
{
    if constexpr (mc::target.build_id == 12111) {
        services::game::inventory_bridge::cancel(services::game::inventory_bridge::feature::elytra);
        current_state = state::idle; source_screen_slot = -1; next_action = {};
    }
}

void modules::elytra_swapper::render_options()
{
	gui::settings::render(settings_, module_settings::elytra_swapper::schema());
}
