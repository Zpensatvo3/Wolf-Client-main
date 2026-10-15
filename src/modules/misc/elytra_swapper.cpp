#include "elytra_swapper.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../ui_redirect.hpp"
#include <algorithm>
#include <chrono>
#include <string>

#include <imgui/imgui.h>

#include "../gui/gui.hpp"


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
		return mode == 0
			? (key.ends_with("_elytra") || key.ends_with(".elytra"))
			: (key.ends_with("_chestplate") || key.ends_with(".chestplate"));
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
}

void modules::elytra_swapper::run(::cache&)
{
	// Inventory interaction is performed from render_overlay(), which runs on
	// Minecraft's render/client thread. The module worker remains read-only.
}

void modules::elytra_swapper::render_overlay()
{
	if (!platform::jni_context::bind_current_thread())
		return;

	try
	{
		jni::frame frame{ 80 };
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
		// The player's own ScreenHandler exists even while the inventory GUI is
		// closed, so a bound one-shot swap can work directly during gameplay.
		// Reject only unrelated open screens whose slot layout is different.
		if (screen && !screen.is_instance_of<maps::InventoryScreen>())
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
			maps::DefaultedList armor = inventory.armor.get();
			maps::DefaultedList main = inventory.main.get();
			if (!cursor || !armor || !main || platform::jni_context::clear_exception() ||
				cursor.isEmpty.call() != JNI_TRUE || platform::jni_context::clear_exception())
			{
				finish();
				return;
			}

			// PlayerInventory armor index 2 is the chest equipment slot.
			maps::Object chest_object = armor.get.call(2);
			if (chest_object && !platform::jni_context::clear_exception() &&
				stack_matches_mode(maps::ItemStack(chest_object.object_instance, false), target_mode))
			{
				finish();
				return;
			}

			const int inventory_slot = find_target_slot(main, target_mode, use_hotbar);
			source_screen_slot = screen_slot_from_inventory_slot(inventory_slot);
			if (source_screen_slot < 0 || !click_slot(instance, player, handler, source_screen_slot))
			{
				finish();
				return;
			}
			current_state = state::pickup_source;
			next_action = steady_clock::now() + std::chrono::milliseconds(
				std::clamp(swap_delay_ms, 20, 300));
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
				std::clamp(swap_delay_ms, 20, 300));
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
	current_state = state::idle;
	source_screen_slot = -1;
	next_action = {};
}

void modules::elytra_swapper::render_options()
{
	const char* modes[] = {
		gui::text("Equip Elytra", "Elytra Tak"),
		gui::text("Equip Chestplate", "Gogusluk Tak")
	};
	ImGui::Combo(gui::text("Swap Target", "Takas Hedefi"), &target_mode, modes, IM_ARRAYSIZE(modes));
	ImGui::Checkbox(gui::text("Use Hotbar", "Hotbar Kullan"), &use_hotbar);
	ImGui::SliderInt(gui::text("Click Delay", "Tik Gecikmesi"), &swap_delay_ms, 20, 300, "%d ms");
	ImGui::TextDisabled("%s", gui::text(
		"One use: swaps the chest item and turns itself off.",
		"Tek kullanim: gogus esyasini degistirir ve kendini kapatir."));
}
