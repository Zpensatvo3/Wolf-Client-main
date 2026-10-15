#include "chest_stealer.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../ui_redirect.hpp"
#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <random>
#include <vector>

#include <imgui/imgui.h>

#include "../../gui/gui.hpp"


namespace
{
	using steady_clock = std::chrono::steady_clock;
	std::mt19937 chest_random(std::random_device{}());
int next_delay(int minimum, int maximum)
	{
		minimum = std::clamp(minimum, 25, 800);
		maximum = std::clamp(maximum, minimum, 1000);
		std::uniform_int_distribution<int> delay(minimum, maximum);
		return delay(chest_random);
	}
}

void modules::chest_stealer::run(::cache&)
{
	// Container clicks must run on Minecraft's client thread. render_overlay()
	// is called by the OpenGL/client hook and owns the complete click sequence.
}

void modules::chest_stealer::render_overlay()
{
	const auto settings_snapshot = settings_.read();
	if (!platform::jni_context::bind_current_thread())
		return;

	if (reset_requested.exchange(false, std::memory_order_acq_rel))
	{
		chest_seen = false;
		next_action_time = {};
	}

	const HWND game_window = gui::get_window();
	if (!game_window || GetForegroundWindow() != game_window || gui::draw || gui::loader_visible)
	{
		chest_seen = false;
		next_action_time = {};
		return;
	}

	try
	{
		jni::frame frame{ 144 };
		maps::MinecraftClient instance = maps::MinecraftClient{}.instance.get();
		maps::ClientPlayerEntity player = instance ? instance.player.get() : maps::ClientPlayerEntity{};
		maps::Screen screen = instance ? instance.currentScreen.get() : maps::Screen{};
		if (!instance || !player || !screen || platform::jni_context::clear_exception() ||
			!screen.is_instance_of<maps::GenericContainerScreen>() || platform::jni_context::clear_exception())
		{
			chest_seen = false;
			next_action_time = {};
			return;
		}

		const auto now = steady_clock::now();
		if (!chest_seen)
		{
			chest_seen = true;
			next_action_time = now + std::chrono::milliseconds(
				std::clamp(settings_snapshot.open_reaction_ms, 25, 800));
			return;
		}
		if (next_action_time != steady_clock::time_point{} && now < next_action_time)
			return;

		maps::HandledScreen handled(screen.object_instance, false);
		maps::ScreenHandler handler = handled.handler.get();
		maps::DefaultedList slots = handler ? handler.slots.get() : maps::DefaultedList{};
		jni::array<maps::Object> slot_array = slots
			? slots.toArray.call()
			: jni::array<maps::Object>{ nullptr };
		if (!handler || !slots || !slot_array || platform::jni_context::clear_exception())
			return;

		const std::vector<maps::Object> all_slots = slot_array.to_vector();
		// GenericContainerScreen always appends the player's 27 inventory slots
		// and 9 hotbar slots after the container slots. This remains reliable for
		// single, double and custom-row generic chests and avoids comparing backing
		// Inventory wrapper objects across JNI.
		const int container_slot_count = static_cast<int>(all_slots.size()) - 36;
		if (container_slot_count <= 0)
			return;

		std::vector<int> candidates;
		candidates.reserve(static_cast<std::size_t>(container_slot_count));
		for (int index = 0; index < container_slot_count; ++index)
		{
			const maps::Object& object = all_slots[static_cast<std::size_t>(index)];
			maps::Slot slot(object.object_instance, false);
			if (!slot || slot.hasStack.call() != JNI_TRUE || platform::jni_context::clear_exception())
				continue;
			const int slot_id = slot.id.get();
			if (!platform::jni_context::clear_exception() && slot_id >= 0)
				candidates.push_back(slot_id);
		}

		if (candidates.empty())
			return;

		int selected_slot = candidates.front();
		if (settings_snapshot.random_slot_order && candidates.size() > 1)
		{
			std::uniform_int_distribution<std::size_t> pick(0, candidates.size() - 1);
			selected_slot = candidates[pick(chest_random)];
		}

		maps::ClientPlayerInteractionManager manager = instance.interactionManager.get();
		maps::SlotActionType quick_move = maps::SlotActionType{}.QUICK_MOVE.get();
		if (!manager || !quick_move || platform::jni_context::clear_exception())
			return;

		manager.clickSlot.call(handler.syncId.get(), selected_slot, 0, quick_move,
			player.toPlayerEntity());
		if (!platform::jni_context::clear_exception())
			next_action_time = now + std::chrono::milliseconds(
				next_delay(settings_snapshot.min_delay_ms, settings_snapshot.max_delay_ms));
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		chest_seen = false;
		next_action_time = {};
	}
}

void modules::chest_stealer::on_disable(::cache&)
{
	reset_requested.store(true, std::memory_order_release);
}

void modules::chest_stealer::render_options()
{
	gui::settings::render(settings_, module_settings::chest_stealer::schema());
}
