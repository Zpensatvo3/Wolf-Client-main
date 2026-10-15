#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/hotbar_service.hpp"
#include "../../services/game/client_context.hpp"
#include "../../services/game/inventory_service.hpp"
#include "../../services/game/target_service.hpp"
#include <algorithm>
#include <chrono>
#include <string>

#include <imgui/imgui.h>

#include "mace_swap.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include "../../mappings.hpp"


namespace
{
}

bool modules::mace_swap::prepare_current_thread()
{
	if (gui::draw || gui::loader_visible)
		return false;

	services::game::client_context context{};
	if (!services::game::capture_client_context(context, true, true))
		return false;
	auto& instance = context.instance;
	auto& player = context.player;
	auto& manager = context.interaction;
	if (waiting_for_release)
	{
		const int selected = services::hotbar::selected_slot(player.getInventory.call());
		if (selected == mace_slot && trigger_world && trigger_world.is_same_object(context.world) &&
			trigger_player.is_same_object(player) && !platform::jni_context::clear_exception()) return true;
		// A manual slot change or a new world invalidates the old lease.
		old_slot = -1; mace_slot = -1; waiting_for_release = false; trigger_prepared = false;
		trigger_world.clear_ref(); trigger_player.clear_ref();
	}
	const HWND window = gui::get_window();
	if (!window || GetForegroundWindow() != window || context.screen ||
		player.isUsingItem.call() == JNI_TRUE || platform::jni_context::clear_exception() ||
		!services::target::living_crosshair_target(instance))
		return false;

	maps::PlayerInventory inventory = player.getInventory();
	maps::DefaultedList items = inventory ? inventory.main.get() : maps::DefaultedList{};
	const int selected_slot = inventory ? inventory.selectedSlot.get() : -1;
	if (!inventory || !items || selected_slot < 0 || selected_slot > 8 ||
		platform::jni_context::clear_exception() || !services::inventory::slot_matches(items, selected_slot, services::inventory::hotbar_item_kind::sword))
		return false;

	const int selected_mace_slot = services::inventory::find_hotbar_slot(items, services::inventory::hotbar_item_kind::mace, preferred_mace_slot - 1);
	if (selected_mace_slot < 0 || selected_mace_slot == selected_slot || platform::jni_context::clear_exception())
		return false;

	old_slot = selected_slot;
	mace_slot = selected_mace_slot;
	if (!services::hotbar::select(player, manager, mace_slot))
	{
		old_slot = -1;
		mace_slot = -1;
		return false;
	}

	switched_at = std::chrono::steady_clock::now();
	waiting_for_release = true;
	trigger_world = context.world; trigger_player = player;
	return true;
}

bool modules::mace_swap::prepare_for_synthetic_attack()
{
	mace_swap* instance = active_instance.load(std::memory_order_acquire);
	if (!instance || !instance->enabled || !platform::jni_context::bind_current_thread())
		return false;
	std::lock_guard lock(instance->state_mutex);
	jni::frame frame{ 128 };
	try
	{
		return instance->prepare_current_thread();
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		return false;
	}
}

void modules::mace_swap::gate_left_click()
{
	// WndProc runs on Minecraft's window/render thread. Switching here is both
	// earlier and safer than blocking this thread while a 5 ms worker catches up.
	prepare_for_synthetic_attack();
	auto* instance = active_instance.load(std::memory_order_acquire);
	if (instance)
	{
		std::lock_guard lock(instance->state_mutex);
		if (instance->trigger_prepared)
		{
			instance->trigger_prepared = false;
			instance->switched_at = std::chrono::steady_clock::now();
		}
	}
}

bool modules::mace_swap::prepare_for_trigger_attack()
{
    auto* instance = active_instance.load(std::memory_order_acquire);
    if (!instance || !instance->enabled || !platform::jni_context::bind_current_thread()) return false;
    std::lock_guard lock(instance->state_mutex);
    jni::frame frame{128};
    try {
        if (!instance->prepare_current_thread()) return false;
        if (instance->waiting_for_release) {
            services::game::client_context context{};
            if (!services::game::capture_client_context(context, true, true)) {
                instance->old_slot = -1; instance->mace_slot = -1; instance->waiting_for_release = false;
                return false;
            }
            instance->trigger_world = context.world; instance->trigger_player = context.player;
            instance->trigger_prepared = true;
        }
        return !platform::jni_context::clear_exception();
    } catch (...) { platform::jni_context::clear_exception(); return false; }
}
int modules::mace_swap::trigger_prepared_slot()
{
    auto* instance = active_instance.load(std::memory_order_acquire);
    if (!instance) return -1;
    std::lock_guard lock(instance->state_mutex);
    return instance->enabled && instance->trigger_prepared ? instance->mace_slot : -1;
}
int modules::mace_swap::starting_slot_if_owned(const maps::ClientPlayerEntity& player,
    const maps::ClientWorld& world, int selected)
{
    auto* instance = active_instance.load(std::memory_order_acquire);
    if (!instance) return -1;
    std::lock_guard lock(instance->state_mutex);
    if (!instance->enabled || !instance->waiting_for_release || selected != instance->mace_slot ||
        !instance->trigger_world || !instance->trigger_player.is_same_object(player) ||
        !instance->trigger_world.is_same_object(world)) return -1;
    return instance->old_slot;
}

void modules::mace_swap::finish_trigger_preparation(const maps::ClientPlayerEntity& player,
    const maps::ClientPlayerInteractionManager& manager, bool consumed)
{
    auto* instance = active_instance.load(std::memory_order_acquire);
    if (!instance) return;
    std::lock_guard lock(instance->state_mutex);
    if (!instance->trigger_prepared) return;
    instance->trigger_prepared = false;
    if (consumed && instance->enabled) {
        instance->switched_at = std::chrono::steady_clock::now();
        return;
    }
    if (player && manager && instance->trigger_player.is_same_object(player))
        services::hotbar::restore_if_owned(player, manager, instance->old_slot, instance->mace_slot);
    instance->old_slot = -1; instance->mace_slot = -1; instance->waiting_for_release = false;
    instance->trigger_world.clear_ref(); instance->trigger_player.clear_ref();
    platform::jni_context::clear_exception();
}

void modules::mace_swap::restore(::cache& cache)
{
	if (trigger_world && (!cache.instance || !cache.player || !trigger_player.is_same_object(cache.player) ||
		!trigger_world.is_same_object(cache.instance.world.get())))
	{
		old_slot = -1; mace_slot = -1; waiting_for_release = false; trigger_prepared = false;
		trigger_world.clear_ref(); trigger_player.clear_ref(); return;
	}
	if (old_slot >= 0 && old_slot <= 8 && cache.player && cache.instance)
	{
		try
		{
			maps::PlayerInventory inventory = cache.player.getInventory();
			maps::ClientPlayerInteractionManager manager = cache.instance.interactionManager.get();
			if (inventory && manager && !platform::jni_context::clear_exception() &&
				inventory.selectedSlot.get() == mace_slot && !platform::jni_context::clear_exception())
				services::hotbar::select(cache.player, manager, old_slot);
		}
		catch (...) { platform::jni_context::clear_exception(); }
	}
	old_slot = -1;
	mace_slot = -1;
	waiting_for_release = false;
	trigger_prepared = false;
	trigger_world.clear_ref(); trigger_player.clear_ref();
	switched_at = {};
}

void modules::mace_swap::run(::cache& cache)
{
	std::lock_guard lock(state_mutex);
	if (trigger_prepared) return;
	if (!waiting_for_release)
		return;

	const auto now = std::chrono::steady_clock::now();
	const int safe_delay = (std::max)(18, std::clamp(return_delay, 0, 75));
	const bool delay_elapsed = now - switched_at >= std::chrono::milliseconds(safe_delay);
	const HWND window = gui::get_window();
	if (!cache.is_valid() || !window || GetForegroundWindow() != window || gui::draw ||
		delay_elapsed || now - switched_at >= std::chrono::milliseconds(160))
		restore(cache);
}

void modules::mace_swap::on_disable(::cache& cache)
{
	std::lock_guard lock(state_mutex);
	if (trigger_prepared) return;
	restore(cache);
}

void modules::mace_swap::render_options()
{
	gui::option_slider(gui::text("Preferred Mace Slot", "Tercih Edilen Mace Slotu"),
		&preferred_mace_slot, 1, 9, "Slot %d");
	gui::option_slider(gui::text("Return Delay", "Geri Donus Gecikmesi"),
		&return_delay, 0, 75, "%d ms");
}
