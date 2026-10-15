#include "legit_auto_totem.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../ui_redirect.hpp"
#include <Windows.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>
#include <thread>

#include <imgui/imgui.h>

#include "../../gui/gui.hpp"

namespace
{
	using steady_clock = std::chrono::steady_clock;
	std::mt19937_64 legit_totem_random(std::random_device{}());
int varied_delay(int base_ms, int variation_ms)
	{
		const int base = std::clamp(base_ms, 40, 1200);
		const int variation = std::clamp(variation_ms, 0, 250);
		if (variation == 0)
			return base;

		std::normal_distribution<double> distribution(0.0, variation * 0.42);
		return std::clamp(base + static_cast<int>(std::lround(distribution(legit_totem_random))),
			40, 1450);
	}

	bool is_totem(const maps::ItemStack& stack, const maps::Item& totem)
	{
		if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
			return false;
		maps::Item item = stack.item.get();
		if (!item || platform::jni_context::clear_exception())
			return false;
		if (totem && item.is_same_object(totem))
			return true;
		maps::String key = item.getTranslationKey.call();
		if (key && !platform::jni_context::clear_exception())
		{
			std::string s = key.to_string();
			if (s.find("totem") != std::string::npos)
				return true;
		}
		return false;
	}

	bool item_at_is_totem(const maps::DefaultedList& items, int index, const maps::Item& totem)
	{
		if (!items || index < 0)
			return false;
		maps::Object object = items.get.call(static_cast<jint>(index));
		if (!object || platform::jni_context::clear_exception())
			return false;
		return is_totem(maps::ItemStack(object.object_instance, false), totem);
	}

	int find_source(const maps::DefaultedList& items, const maps::Item& totem,
		bool preserve_hotbar, int excluded_slot = -1)
	{
		for (int slot = 9; slot < 36; ++slot)
			if (slot != excluded_slot && item_at_is_totem(items, slot, totem))
				return slot;

		if (!preserve_hotbar)
		{
			for (int slot = 0; slot < 9; ++slot)
				if (slot != excluded_slot && item_at_is_totem(items, slot, totem))
					return slot;
		}
		return -1;
	}

	int screen_slot_from_inventory_slot(int slot)
	{
		return slot >= 0 && slot < 9 ? slot + 36 : (slot >= 9 && slot < 36 ? slot : -1);
	}

	bool screen_slot_center(const maps::Screen& screen, int screen_slot, POINT& output)
	{
		if (!screen || screen_slot < 9 || screen_slot > 45)
			return false;

		const int gui_width = screen.width.get();
		const int gui_height = screen.height.get();
		if (platform::jni_context::clear_exception() || gui_width <= 0 || gui_height <= 0)
			return false;

		const int left = (gui_width - 176) / 2;
		const int top = (gui_height - 166) / 2;
		int gui_x = 0;
		int gui_y = 0;
		if (screen_slot >= 9 && screen_slot <= 35)
		{
			const int index = screen_slot - 9;
			gui_x = left + 16 + (index % 9) * 18;
			gui_y = top + 92 + (index / 9) * 18;
		}
		else if (screen_slot >= 36 && screen_slot <= 44)
		{
			gui_x = left + 16 + (screen_slot - 36) * 18;
			gui_y = top + 150;
		}
		else
		{
			// PlayerScreenHandler offhand slot: background coordinate (77, 62).
			gui_x = left + 85;
			gui_y = top + 70;
		}

		const HWND window = gui::get_window();
		RECT client{};
		if (!window || !GetClientRect(window, &client))
			return false;
		const int client_width = client.right - client.left;
		const int client_height = client.bottom - client.top;
		if (client_width <= 0 || client_height <= 0)
			return false;

		POINT point{
			static_cast<LONG>(std::lround(gui_x * (static_cast<double>(client_width) / gui_width))),
			static_cast<LONG>(std::lround(gui_y * (static_cast<double>(client_height) / gui_height)))
		};
		if (!ClientToScreen(window, &point))
			return false;
		output = point;
		return true;
	}

	bool guide_cursor_through_slots(const maps::Screen& screen, int source_slot,
		int destination_slot, int travel_ms)
	{
		const HWND window = gui::get_window();
		if (!window || GetForegroundWindow() != window)
			return false;

		POINT source{};
		POINT destination{};
		if (!screen_slot_center(screen, source_slot, source) ||
			!screen_slot_center(screen, destination_slot, destination))
			return false;

		POINT start{};
		if (!GetCursorPos(&start))
			return false;

		const int step_delay = std::clamp(travel_ms, 16, 180) / 6;
		auto glide = [&](const POINT& from, const POINT& to)
		{
			for (int step = 1; step <= 3; ++step)
			{
				const double progress = static_cast<double>(step) / 3.0;
				SetCursorPos(
					static_cast<int>(std::lround(from.x + (to.x - from.x) * progress)),
					static_cast<int>(std::lround(from.y + (to.y - from.y) * progress)));
				std::this_thread::sleep_for(std::chrono::milliseconds((std::max)(1, step_delay)));
			}
		};

		glide(start, source);
		glide(source, destination);
		return true;
	}
}

void modules::legit_auto_totem::run(::cache& cache)
{
    auto settings_snapshot = settings_.read();
	const auto reset = [this]()
	{
		inventory_seen = false;
		pending_destination = -1;
		next_action_time = {};
		pending_action_time = {};
	};

	if (gui::draw || !cache.is_valid() || !cache.instance || !cache.player || !jni::get_env())
	{
		reset();
		return;
	}

	try
	{
		jni::frame frame{ 64 };
		maps::Screen screen = cache.instance.currentScreen.get();
		const bool inventory_open = screen && !platform::jni_context::clear_exception() &&
			screen.is_instance_of<maps::InventoryScreen>();
		if (!inventory_open)
		{
			reset();
			return;
		}

		const auto now = steady_clock::now();
		if (!inventory_seen)
		{
			inventory_seen = true;
			next_action_time = now + std::chrono::milliseconds(
				varied_delay(settings_snapshot.reaction_delay_ms, settings_snapshot.timing_variation_ms));
			return;
		}
		if (now < next_action_time)
			return;

		maps::PlayerInventory inventory = cache.player.getInventory.call();
		maps::Item totem = maps::Items{}.TOTEM_OF_UNDYING.get();
		maps::ScreenHandler handler = cache.player.currentScreenHandler.get();
		if (!inventory || !totem || !handler || platform::jni_context::clear_exception())
			return;

		maps::DefaultedList offhand = inventory.offHand.get();
		maps::DefaultedList main = inventory.main.get();
		if (!offhand || !main || platform::jni_context::clear_exception())
			return;

		const int target_hotbar_index = std::clamp(settings_snapshot.hotbar_slot, 1, 9) - 1;
		const bool offhand_has_totem = item_at_is_totem(offhand, 0, totem);
		const bool hotbar_has_totem = item_at_is_totem(main, target_hotbar_index, totem);
		const float health = cache.player.getHealth.call();
		if (platform::jni_context::clear_exception())
			return;

		const bool need_offhand = settings_snapshot.fill_offhand && !offhand_has_totem &&
			(!settings_snapshot.low_health_only || health <= std::clamp(settings_snapshot.health_threshold, 1.0f, 20.0f));
		const bool need_hotbar = settings_snapshot.fill_hotbar && !hotbar_has_totem;

		if (pending_destination >= 0)
		{
			const bool confirmed = pending_destination == 40
				? offhand_has_totem
				: (pending_destination == target_hotbar_index && hotbar_has_totem);
			if (confirmed)
			{
				pending_destination = -1;
				pending_action_time = {};
				next_action_time = now + std::chrono::milliseconds(
					varied_delay(settings_snapshot.action_delay_ms, settings_snapshot.timing_variation_ms));
				return;
			}

			if (pending_action_time != steady_clock::time_point{} &&
				now - pending_action_time < std::chrono::milliseconds(
					std::clamp(settings_snapshot.confirmation_timeout_ms, 120, 1800)))
			{
				return;
			}

			// The server has not accepted the swap yet. Rescan after a natural
			// pause instead of sending the same offhand action again immediately.
			pending_destination = -1;
			pending_action_time = {};
			next_action_time = now + std::chrono::milliseconds(
				varied_delay(settings_snapshot.reaction_delay_ms, settings_snapshot.timing_variation_ms));
			return;
		}

		if (!need_offhand && !need_hotbar)
			return;

		if (settings_snapshot.pause_with_cursor_item)
		{
			maps::ItemStack cursor = handler.getCursorStack.call();
			if (!cursor || platform::jni_context::clear_exception() || cursor.isEmpty.call() != JNI_TRUE)
			{
				next_action_time = now + std::chrono::milliseconds(
					varied_delay(settings_snapshot.action_delay_ms, settings_snapshot.timing_variation_ms));
				return;
			}
		}

		maps::ClientPlayerInteractionManager manager = cache.instance.interactionManager.get();
		maps::SlotActionType swap = maps::SlotActionType{}.SWAP.get();
		if (!manager || !swap || platform::jni_context::clear_exception())
			return;

		int performed_destination = -1;
		const auto try_swap = [&](bool for_offhand)
		{
			const int destination = for_offhand ? 40 : target_hotbar_index;
			const int excluded = for_offhand ? -1 : target_hotbar_index;
			const int source = find_source(main, totem, settings_snapshot.preserve_hotbar, excluded);
			const int screen_slot = screen_slot_from_inventory_slot(source);
			if (screen_slot < 0)
				return false;
			if (settings_snapshot.cursor_guided_swap)
			{
				const int target_screen_slot = for_offhand ? 45 : 36 + target_hotbar_index;
				if (!guide_cursor_through_slots(screen, screen_slot, target_screen_slot, settings_snapshot.cursor_travel_ms))
					return false;
			}

			manager.clickSlot.call(handler.syncId.get(), static_cast<jint>(screen_slot),
				static_cast<jint>(destination), swap, cache.player.toPlayerEntity());
			if (platform::jni_context::clear_exception())
				return false;

			performed_destination = destination;
			return true;
		};

		const bool action_sent = settings_snapshot.offhand_first
			? (need_offhand && try_swap(true)) || (need_hotbar && try_swap(false))
			: (need_hotbar && try_swap(false)) || (need_offhand && try_swap(true));
		if (action_sent)
		{
			pending_destination = performed_destination;
			pending_action_time = now;
			return;
		}

		next_action_time = now + std::chrono::milliseconds(
			varied_delay(settings_snapshot.reaction_delay_ms, settings_snapshot.timing_variation_ms));
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		inventory_seen = false;
		pending_destination = -1;
		next_action_time = {};
		pending_action_time = {};
	}
}

void modules::legit_auto_totem::on_disable(::cache&)
{
	inventory_seen = false;
	pending_destination = -1;
	next_action_time = {};
	pending_action_time = {};
}

void modules::legit_auto_totem::render_options()
{
    gui::settings::edit(settings_, module_settings::legit_auto_totem::schema(), [&](auto& candidate) {
        const auto schema = module_settings::legit_auto_totem::schema();
        ImGui::TextDisabled("Operates only while your Inventory screen is visibly open.");
        gui::option_section("Destination");
        gui::settings::render(candidate, schema.first(4));
        gui::option_section("Safety");
        gui::settings::render(candidate, schema.subspan(4, 7));
        ImGui::TextDisabled("Every swap waits for visible inventory confirmation before another action.");
        gui::option_section(gui::text("Human Timing", "Insan Zamanlamasi"));
        gui::settings::render(candidate, schema.subspan(11));
        ImGui::TextDisabled("It never opens or closes inventory and performs one confirmed swap at a time.");
    });
}
