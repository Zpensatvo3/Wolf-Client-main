#include <algorithm>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/input/input_service.hpp"
#include <chrono>
#include <cmath>
#include <random>

#include <imgui/imgui.h>

#include "auto_totem.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"

namespace
{
    using steady_clock = std::chrono::steady_clock;
    std::mt19937_64 random_engine(std::random_device{}());

    int humanized_delay(int base_ms, int randomness_ms)
    {
        const int base = std::clamp(base_ms, 0, 1000);
        const int randomness = std::clamp(randomness_ms, 0, 250);
        if (randomness == 0)
            return base;

        std::normal_distribution<double> core(0.0, randomness * 0.45);
        std::uniform_int_distribution<int> fine(-randomness, randomness);
        std::bernoulli_distribution short_pause(0.13);
        std::uniform_int_distribution<int> pause(0, randomness);
        int result = base + static_cast<int>(std::lround(core(random_engine))) + fine(random_engine) / 3;
        if (short_pause(random_engine))
            result += pause(random_engine);
        return std::clamp(result, 20, 1250);
    }

    void pulse_key(WORD vk)
    {
        services::input::key_pulse(vk);
    }
bool stack_is_totem(const maps::ItemStack& stack, const maps::Item& totem_item)
    {
        if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
            return false;

        maps::Item item = stack.item.get();
        if (!item || platform::jni_context::clear_exception())
            return false;

        if (totem_item && item.is_same_object(totem_item))
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

    bool list_slot_is_totem(
        const maps::DefaultedList& items,
        int inventory_index,
        const maps::Item& totem_item)
    {
        if (!items || inventory_index < 0)
            return false;

        maps::Object object = items.get.call(static_cast<jint>(inventory_index));
        if (!object || platform::jni_context::clear_exception())
            return false;

        return stack_is_totem(maps::ItemStack(object.object_instance, false), totem_item);
    }

    int find_totem_source(
        const maps::DefaultedList& main_items,
        const maps::Item& totem_item,
        int excluded_index)
    {
        // Prefer the main inventory so existing hotbar arrangements remain
        // untouched whenever a spare totem is available there.
        for (int index = 9; index < 36; ++index)
        {
            if (index != excluded_index && list_slot_is_totem(main_items, index, totem_item))
                return index;
        }

        for (int index = 0; index < 9; ++index)
        {
            if (index != excluded_index && list_slot_is_totem(main_items, index, totem_item))
                return index;
        }

        return -1;
    }

    int inventory_index_to_screen_slot(int inventory_index)
    {
        if (inventory_index >= 0 && inventory_index < 9)
            return 36 + inventory_index;
        if (inventory_index >= 9 && inventory_index < 36)
            return inventory_index;
        return -1;
    }

    bool swap_from_inventory(
        ::cache& cache,
        const maps::ScreenHandler& handler,
        int source_inventory_index,
        int swap_button)
    {
        const int source_screen_slot = inventory_index_to_screen_slot(source_inventory_index);
        if (source_screen_slot < 0 || !handler || !cache.instance || !cache.player)
            return false;

        maps::ClientPlayerInteractionManager interaction_manager =
            cache.instance.interactionManager.get();
        maps::SlotActionType swap_action = maps::SlotActionType{}.SWAP.get();
        if (!interaction_manager || !swap_action || platform::jni_context::clear_exception())
            return false;

        interaction_manager.clickSlot.call(
            handler.syncId.get(),
            static_cast<jint>(source_screen_slot),
            static_cast<jint>(swap_button),
            swap_action,
            cache.player.toPlayerEntity());

        return !platform::jni_context::clear_exception();
    }
}

void modules::auto_totem::run(::cache& cache)
{
    const auto settings_snapshot = settings_.read();
    if (!applied_settings_ || *applied_settings_ != settings_snapshot)
    {
        reset_runtime();
        applied_settings_ = settings_snapshot;
    }
    if (settings_snapshot.mode == 1) run_legit(cache, settings_snapshot);
    else run_normal(cache, settings_snapshot);
}

void modules::auto_totem::reset_runtime()
{
    inventory_was_open = false;
    inventory_opened_by_module = false;
    pending_destination = -1;
    next_action_time = {};
    pending_action_time = {};
}

void modules::auto_totem::run_normal(::cache& cache, const module_settings::auto_totem& settings_snapshot)
{
    const auto reset_inventory_state = [this]() { reset_runtime(); };

    if (gui::draw || !cache.is_valid() || !cache.instance || !cache.player || !jni::get_env())
    {
        reset_inventory_state();
        return;
    }

    try
    {
        jni::frame frame{ 64 };
        const auto now = steady_clock::now();

        const auto schedule_action = [this, now, &settings_snapshot](int base_delay)
        {
            next_action_time = now + std::chrono::milliseconds(
                humanized_delay(base_delay, settings_snapshot.timing_randomness_ms));
        };

        maps::Screen current_screen = cache.instance.currentScreen.get();
        const bool in_inventory = current_screen && !platform::jni_context::clear_exception() &&
            current_screen.is_instance_of<maps::InventoryScreen>();

        maps::PlayerInventory inventory = cache.player.getInventory.call();
        maps::Item totem_item = maps::Items{}.TOTEM_OF_UNDYING.get();
        if (!inventory || !totem_item || platform::jni_context::clear_exception())
            return;

        maps::DefaultedList main_items = inventory.main.get();
        maps::ItemStack offhand_item = inventory.offhand_stack();
        if (!main_items || !offhand_item || platform::jni_context::clear_exception())
            return;

        const int target_hotbar_index = std::clamp(settings_snapshot.hotbar_slot, 1, 9) - 1;
		const bool need_offhand = settings_snapshot.fill_offhand && !stack_is_totem(offhand_item, totem_item);
		const bool need_hotbar = settings_snapshot.fill_hotbar && !list_slot_is_totem(main_items, target_hotbar_index, totem_item);

		// clickSlot is server-authoritative. Wait for the inventory update before
		// scheduling another swap, otherwise a delayed update can turn a second
		// SWAP into a swap-back.
		if (pending_destination >= 0)
		{
			const bool confirmed = pending_destination == 40
				? !need_offhand
				: (pending_destination == target_hotbar_index && !need_hotbar);
			if (confirmed)
			{
				pending_destination = -1;
				pending_action_time = {};
				schedule_action(settings_snapshot.delay_ms);
				return;
			}

			if (pending_action_time != steady_clock::time_point{} &&
				now - pending_action_time < std::chrono::milliseconds(
					std::clamp(settings_snapshot.confirmation_timeout_ms, 80, 1200)))
			{
				return;
			}

			// The server did not confirm this action. Release the pending state and
			// rescan after a full delay instead of issuing duplicate packets.
			pending_destination = -1;
			pending_action_time = {};
			schedule_action(settings_snapshot.initial_reaction_ms);
			return;
		}

        if (!need_offhand && !need_hotbar)
        {
            // Only close an inventory that this module opened itself. A manually
            // opened inventory is always left under the user's control.
            if (in_inventory && inventory_opened_by_module && now >= next_action_time)
            {
                pulse_key('E');
                reset_inventory_state();
            }
            return;
        }

		if (!in_inventory)
        {
            inventory_was_open = false;

            // Allow a short window for the E key event to create InventoryScreen.
            if (inventory_opened_by_module && now < next_action_time)
                return;

            if (inventory_opened_by_module && now >= next_action_time)
                inventory_opened_by_module = false;

            if (settings_snapshot.auto_open_inventory && !current_screen && now >= next_action_time)
            {
                const int source_offhand = need_offhand ? find_totem_source(main_items, totem_item, -1) : -1;
                const int source_hotbar = need_hotbar ? find_totem_source(main_items, totem_item, target_hotbar_index) : -1;

                if (source_offhand >= 0 || source_hotbar >= 0)
                {
                    pulse_key('E');
                    inventory_opened_by_module = true;
                    schedule_action(settings_snapshot.initial_reaction_ms);
                }
            }
			return;
		}

        if (!inventory_was_open)
        {
            inventory_was_open = true;

            // Auto-open already scheduled its reaction delay. Manual inventory
            // opening gets its own small decision pause here.
            if (!inventory_opened_by_module)
                schedule_action(settings_snapshot.initial_reaction_ms);
            return;
        }

        if (now < next_action_time)
            return;

        maps::ScreenHandler handler = cache.player.currentScreenHandler.get();
        if (!handler || platform::jni_context::clear_exception())
            return;

        if (settings_snapshot.pause_with_cursor_item)
        {
            maps::ItemStack cursor_stack = handler.getCursorStack.call();
            if (!cursor_stack || platform::jni_context::clear_exception())
                return;
            if (cursor_stack.isEmpty.call() != JNI_TRUE && !platform::jni_context::clear_exception())
            {
                schedule_action(settings_snapshot.delay_ms);
                return;
            }
        }

		int performed_destination = -1;
		const auto try_offhand = [&]()
		{
			if (!need_offhand)
				return false;
			const int source = find_totem_source(main_items, totem_item, -1);
			if (source < 0 || !swap_from_inventory(cache, handler, source, 40))
				return false;
			performed_destination = 40;
			return true;
		};

        const auto try_hotbar = [&]()
        {
			if (!need_hotbar)
				return false;
			const int source = find_totem_source(main_items, totem_item, target_hotbar_index);
			if (source < 0 || !swap_from_inventory(cache, handler, source, target_hotbar_index))
				return false;
			performed_destination = target_hotbar_index;
			return true;
        };

        // One inventory action per scheduled interval keeps swaps ordered and
        // prevents two clickSlot packets from landing in the same client tick.
		const bool action_performed = settings_snapshot.offhand_first
			? (try_offhand() || try_hotbar())
			: (try_hotbar() || try_offhand());
		if (action_performed)
		{
			pending_destination = performed_destination;
			pending_action_time = now;
			return;
		}

		// No spare totem was found. Do not repeatedly scan the same inventory on
		// every worker iteration while the player is still looking at it.
		schedule_action(settings_snapshot.initial_reaction_ms);

		if (inventory_opened_by_module)
        {
            pulse_key('E');
            reset_inventory_state();
        }
    }
    catch (...)
    {
        platform::jni_context::clear_exception();
        reset_inventory_state();
    }
}

void modules::auto_totem::on_disable(::cache& cache)
{
    (void)cache;
    reset_runtime();
    applied_settings_.reset();
}

void modules::auto_totem::render_options()
{
    gui::settings::render(settings_, module_settings::auto_totem::schema());
}
