#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include <algorithm>
#include <chrono>

#include <imgui/imgui.h>

#include "pot_refill.hpp"
#include "../ui_redirect.hpp"
#include "../gui/gui.hpp"
#include "../mappings.hpp"


namespace
{
	using steady_clock = std::chrono::steady_clock;
bool is_instant_health_potion(const maps::ItemStack& stack)
	{
		if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
			return false;

		maps::Item item = stack.item.get();
		if (!item || platform::jni_context::clear_exception() ||
			!item.is_instance_of<maps::PotionItem>() || platform::jni_context::clear_exception())
			return false;

		maps::ComponentType potion_type = maps::DataComponentTypes{}.POTION_CONTENTS.get();
		if (!potion_type || platform::jni_context::clear_exception())
			return false;

		maps::Object component = stack.getComponent.call(potion_type);
		if (!component || platform::jni_context::clear_exception())
			return false;

		maps::PotionContentsComponent contents(component.object_instance, false);
		maps::RegistryEntry healing = maps::Potions{}.HEALING.get();
		maps::RegistryEntry strong_healing = maps::Potions{}.STRONG_HEALING.get();
		if (!contents || !healing || !strong_healing || platform::jni_context::clear_exception())
			return false;

		const bool healing_one = contents.matches.call(healing) == JNI_TRUE;
		if (platform::jni_context::clear_exception())
			return false;
		const bool healing_two = contents.matches.call(strong_healing) == JNI_TRUE;
		return !platform::jni_context::clear_exception() && (healing_one || healing_two);
	}

	bool find_player_slots(const maps::ScreenHandler& handler,
		const maps::PlayerInventory& player_inventory,
		int& empty_hotbar_index,
		int& health_source_screen_slot,
		int& empty_count,
		int& potion_count)
	{
		empty_hotbar_index = -1;
		health_source_screen_slot = -1;
		empty_count = 0;
		potion_count = 0;

		maps::DefaultedList slots = handler ? handler.slots.get() : maps::DefaultedList{};
		jni::array<maps::Object> array = slots
			? slots.toArray.call()
			: jni::array<maps::Object>{ nullptr };
		if (!slots || !array || !player_inventory || platform::jni_context::clear_exception())
			return false;

		maps::Inventory player_inventory_view(player_inventory.object_instance, false);
		for (const maps::Object& object : array.to_vector())
		{
			maps::Slot slot(object.object_instance, false);
			maps::Inventory slot_inventory = slot ? slot.inventory.get() : maps::Inventory{};
			if (!slot || !slot_inventory || platform::jni_context::clear_exception() ||
				!slot_inventory.is_same_object(player_inventory_view))
				continue;

			const int inventory_index = slot.index.get();
			const int screen_slot = slot.id.get();
			maps::ItemStack stack = slot.getStack.call();
			if (!stack || platform::jni_context::clear_exception())
				continue;

			const bool empty = stack.isEmpty.call() == JNI_TRUE;
			if (platform::jni_context::clear_exception())
				continue;

			if (inventory_index >= 0 && inventory_index < 9)
			{
				if (empty)
				{
					++empty_count;
					if (empty_hotbar_index < 0)
						empty_hotbar_index = inventory_index;
				}
				continue;
			}

			if (inventory_index >= 9 && inventory_index < 36 &&
				!empty && is_instant_health_potion(stack))
			{
				++potion_count;
				if (health_source_screen_slot < 0)
					health_source_screen_slot = screen_slot;
			}
		}

		return true;
	}
}

void modules::pot_refill::run(::cache&)
{
	// Inventory packets are issued from render_overlay(), which runs on the
	// Minecraft client/render thread. The DLL worker thread stays read-only.
}

void modules::pot_refill::render_overlay()
{
	if (!platform::jni_context::bind_current_thread())
		return;

	jni::frame frame{ 96 };
	if (reset_requested.exchange(false, std::memory_order_acq_rel))
	{
		next_refill = {};
		live_status = 0;
		live_health_potions = 0;
		live_empty_hotbar = 0;
	}

	const HWND game_window = gui::get_window();
	if (!game_window || GetForegroundWindow() != game_window || gui::draw || gui::loader_visible)
		return;

	maps::MinecraftClient instance = maps::MinecraftClient{}.instance.get();
	maps::ClientPlayerEntity player = instance ? instance.player.get() : maps::ClientPlayerEntity{};
	maps::Screen current_screen = instance ? instance.currentScreen.get() : maps::Screen{};
	if (!instance || !player || !current_screen || platform::jni_context::clear_exception() ||
		!current_screen.is_instance_of<maps::HandledScreen>() || platform::jni_context::clear_exception())
		return;

	const steady_clock::time_point now = steady_clock::now();
	if (next_refill != steady_clock::time_point{} && now < next_refill)
		return;

	maps::HandledScreen inventory_screen(current_screen.object_instance, false);
	maps::ScreenHandler handler = inventory_screen.handler.get();
	maps::ScreenHandler player_handler = player.currentScreenHandler.get();
	if (!handler || !player_handler || platform::jni_context::clear_exception() ||
		!handler.is_same_object(player_handler))
	{
		live_status = 1;
		return;
	}

	// Never disturb an item the user is already holding with the cursor.
	maps::ItemStack cursor_stack = handler.getCursorStack.call();
	if (!cursor_stack || platform::jni_context::clear_exception() ||
		cursor_stack.isEmpty.call() != JNI_TRUE || platform::jni_context::clear_exception())
	{
		live_status = 2;
		return;
	}

	maps::PlayerInventory inventory = player.getInventory.call();
	if (!inventory || platform::jni_context::clear_exception())
	{
		live_status = 1;
		return;
	}

	int hotbar_slot = -1;
	int source_screen_slot = -1;
	if (!find_player_slots(handler, inventory, hotbar_slot, source_screen_slot,
		live_empty_hotbar, live_health_potions))
	{
		live_status = 1;
		return;
	}
	if (hotbar_slot < 0)
	{
		live_status = 3;
		return;
	}
	if (source_screen_slot < 0)
	{
		live_status = 4;
		return;
	}

	maps::ClientPlayerInteractionManager interaction_manager = instance.interactionManager.get();
	maps::SlotActionType swap_action = maps::SlotActionType{}.SWAP.get();
	if (!interaction_manager || !swap_action || platform::jni_context::clear_exception())
	{
		live_status = 5;
		return;
	}

	// SWAP's button is the destination hotbar inventory index (0..8). The source
	// ID comes from the active handler's real Slot object, so custom layouts and
	// remapped clients do not depend on PlayerScreenHandler's visual slot order.
	interaction_manager.clickSlot.call(
		handler.syncId.get(),
		static_cast<jint>(source_screen_slot),
		static_cast<jint>(hotbar_slot),
		swap_action,
		player.toPlayerEntity());

	if (!platform::jni_context::clear_exception())
	{
		live_status = 6;
		next_refill = now + std::chrono::milliseconds(std::clamp(delay_ms, 50, 1000));
	}
	else
	{
		live_status = 5;
	}
}

void modules::pot_refill::on_disable(::cache&)
{
	reset_requested.store(true, std::memory_order_release);
}

void modules::pot_refill::render_options()
{
	ImGui::SliderInt(gui::text("Refill Delay", "Doldurma Gecikmesi"), &delay_ms, 50, 1000, "%d ms");
	return; // Runtime diagnostics are intentionally not part of the settings UI.
	static const char* english_status[] = {
		"Open an inventory", "Unsupported inventory", "Release cursor item",
		"Hotbar is full", "No Instant Health found", "Inventory action failed", "Refilling"
	};
	static const char* turkish_status[] = {
		"Bir envanter aÃ§", "Desteklenmeyen envanter", "Ä°mleÃ§teki eÅŸyayÄ± bÄ±rak",
		"Hotbar dolu", "Instant Health bulunamadÄ±", "Envanter iÅŸlemi baÅŸarÄ±sÄ±z", "Dolduruluyor"
	};
	const int status = std::clamp(live_status, 0, 6);
	ImGui::Separator();
	ImGui::Text("%s: %s", gui::text("Status", "Durum"),
		gui::is_turkish() ? turkish_status[status] : english_status[status]);
	ImGui::Text("%s: %d", gui::text("Instant Health", "Instant Health"), live_health_potions);
	ImGui::Text("%s: %d", gui::text("Empty hotbar slots", "BoÅŸ hotbar yuvasÄ±"), live_empty_hotbar);
}
