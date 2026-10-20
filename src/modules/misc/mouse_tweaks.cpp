#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include <algorithm>
#include <imgui/imgui.h>

#include "mouse_tweaks.hpp"
#include "mouse_tweaks_logic.hpp"
#include "../../services/game/inventory_bridge.hpp"
#include "../../services/input/input_service.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"

namespace
{
	struct mouse_tweaks_state
	{
		jobject screen = nullptr;
		int rmb_slot = -1;
		int lmb_slot = -1;
		bool rmb_drag_active = false;
		bool lmb_drag_active = false;
		bool rmb_left_original_slot = false;
		bool last_rmb_down = false;
		bool last_lmb_down = false;
		bool cursor_was_empty = true;
		bool cursor_state_known = false;
	};

	mouse_tweaks_state state{};

	bool key_down(int virtual_key)
	{
		return (GetAsyncKeyState(virtual_key) & 0x8000) != 0;
	}
void reset_state(bool release_screen)
	{
		JNIEnv* env = jni::get_env();
		if (release_screen && state.screen && env)
			env->DeleteGlobalRef(state.screen);

		if (release_screen)
			state.screen = nullptr;

		state.rmb_slot = -1;
		state.lmb_slot = -1;
		state.rmb_drag_active = false;
		state.lmb_drag_active = false;
		state.rmb_left_original_slot = false;
		state.last_rmb_down = false;
		state.last_lmb_down = false;
		state.cursor_was_empty = true;
		state.cursor_state_known = false;
	}

	bool track_screen(jobject screen)
	{
		JNIEnv* env = jni::get_env();
		if (!env || !screen)
			return false;

		if (state.screen && env->IsSameObject(state.screen, screen) == JNI_TRUE)
			return true;

		reset_state(true);
		state.screen = env->NewGlobalRef(screen);
		return state.screen && !platform::jni_context::clear_exception();
	}

	bool get_cursor_stack(
		const maps::ScreenHandler& handler,
		maps::ItemStack& stack,
		bool& empty)
	{
		stack = handler.getCursorStack.call();
		if (!stack || platform::jni_context::clear_exception())
		{
			empty = true;
			return false;
		}

		empty = stack.isEmpty.call() == JNI_TRUE;
		return !platform::jni_context::clear_exception();
	}

	bool slot_has_item(const maps::Slot& slot)
	{
		if (!slot || slot.isEnabled.call() != JNI_TRUE || platform::jni_context::clear_exception())
			return false;

		const bool has_stack = slot.hasStack.call() == JNI_TRUE;
		return !platform::jni_context::clear_exception() && has_stack;
	}

	maps::Slot get_slot_by_id(const maps::ScreenHandler& handler, int slot_id)
	{
		if (!handler || slot_id < 0)
			return {};
		maps::DefaultedList slots = handler.slots.get();
		jni::array<maps::Object> array = slots
			? slots.toArray.call()
			: jni::array<maps::Object>{ nullptr };
		if (!slots || !array || platform::jni_context::clear_exception())
			return {};
		for (const maps::Object& object : array.to_vector())
		{
			maps::Slot slot(object.object_instance, false);
			if (slot && slot.id.get() == slot_id && !platform::jni_context::clear_exception())
				return slot;
			platform::jni_context::clear_exception();
		}
		return {};
	}

	bool can_insert_one(const maps::Slot& slot, const maps::ItemStack& cursor_stack)
	{
		if (!slot || !cursor_stack ||
			slot.isEnabled.call() != JNI_TRUE ||
			slot.canInsert.call(cursor_stack) != JNI_TRUE ||
			platform::jni_context::clear_exception())
		{
			return false;
		}

		maps::ItemStack target_stack = slot.getStack.call();
		if (!target_stack || platform::jni_context::clear_exception())
			return false;

		if (target_stack.isEmpty.call() == JNI_TRUE)
			return !platform::jni_context::clear_exception();

		if (platform::jni_context::clear_exception() ||
			cursor_stack.areItemsAndComponentsEqual.call(cursor_stack, target_stack) != JNI_TRUE ||
			platform::jni_context::clear_exception())
		{
			return false;
		}

		const int slot_limit = slot.getMaxItemCount.call(cursor_stack);
		const int item_limit = cursor_stack.getMaxCount.call();
		const int current_count = target_stack.getCount.call();
		if (platform::jni_context::clear_exception())
			return false;

		return current_count < std::min(slot_limit, item_limit);
	}

	bool stacks_are_compatible(
		const maps::ItemStack& first,
		const maps::ItemStack& second)
	{
		if (!first || !second)
			return false;
		const bool first_empty = first.isEmpty.call() == JNI_TRUE;
		const bool second_empty = second.isEmpty.call() == JNI_TRUE;
		if (platform::jni_context::clear_exception())
			return false;
		if (first_empty || second_empty)
			return true;
		return first.areItemsAndComponentsEqual.call(first, second) == JNI_TRUE &&
			!platform::jni_context::clear_exception();
	}

	bool can_merge_entire_stack(
		const maps::Slot& slot,
		const maps::ItemStack& cursor_stack,
		bool& crafting_output)
	{
		crafting_output = false;
		if (!slot || !cursor_stack || slot.isEnabled.call() != JNI_TRUE ||
			platform::jni_context::clear_exception())
			return false;

		maps::ItemStack target_stack = slot.getStack.call();
		if (!target_stack || platform::jni_context::clear_exception() ||
			target_stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception() ||
			!stacks_are_compatible(target_stack, cursor_stack))
			return false;

		const int cursor_count = cursor_stack.getCount.call();
		const int target_count = target_stack.getCount.call();
		const int cursor_limit = cursor_stack.getMaxCount.call();
		if (platform::jni_context::clear_exception() || cursor_count <= 0 || target_count <= 0 ||
			cursor_count + target_count > cursor_limit)
			return false;

		// Output slots reject insertion. They need only the first pickup click;
		// regular inventory slots use a second click to put the merged stack back
		// onto the cursor, matching Mouse Tweaks' LMB-with-item behavior.
		crafting_output = slot.canInsert.call(cursor_stack) != JNI_TRUE;
		platform::jni_context::clear_exception();
		return true;
	}

	bool click_slot(
		const maps::ClientPlayerInteractionManager& interaction_manager,
		const maps::ScreenHandler& handler,
		const maps::PlayerEntity& player,
		int slot_id,
		int button,
		const maps::SlotActionType& action_type)
	{
		if (!interaction_manager || !handler || !player || !action_type || slot_id < 0)
			return false;

		interaction_manager.clickSlot.call(
			handler.syncId.get(),
			static_cast<jint>(slot_id),
			static_cast<jint>(button),
			action_type,
			player);

		return !platform::jni_context::clear_exception();
	}
}

struct modules::mouse_tweaks::runtime
{
    maps::Object player{nullptr, true}, world{nullptr, true}, screen{nullptr, true}, handler{nullptr, true};
    int sync = -1;
    mouse_tweaks_logic::gestures gestures;
    void reset() { player = {}; world = {}; screen = {}; handler = {}; sync = -1; gestures = {}; }
    bool matches(const services::game::inventory_bridge::container_snapshot& s) const {
        auto env = jni::get_env();
        return sync == s.sync_id && player && env->IsSameObject(player.object_instance, s.player) &&
            env->IsSameObject(world.object_instance, s.world) && env->IsSameObject(screen.object_instance, s.screen) &&
            env->IsSameObject(handler.object_instance, s.handler);
    }
    void remember(const services::game::inventory_bridge::container_snapshot& s) {
        player = maps::Object{s.player, false}; world = maps::Object{s.world, false};
        screen = maps::Object{s.screen, false}; handler = maps::Object{s.handler, false}; sync = s.sync_id;
    }
};

modules::mouse_tweaks::mouse_tweaks() : module("mouse tweaks", "Player"), runtime_(std::make_unique<runtime>()) {}
modules::mouse_tweaks::~mouse_tweaks() = default;

void modules::mouse_tweaks::render_overlay()
{
    if constexpr (mc::target.build_id != 12111) return;
    if (native_fallback_.load(std::memory_order_acquire) || !platform::jni_context::bind_current_thread()) return;
    namespace bridge = services::game::inventory_bridge;
    if (reset_requested_.exchange(false, std::memory_order_acq_rel) || !enabled) {
        on_render_shutdown(); if (!enabled) return;
    }
    try {
        jni::frame frame{160};
        bridge::container_snapshot observed;
        const auto backend = bridge::capture_container(bridge::feature::mouse, observed);
        if (backend == services::game::hybrid::result::native_fallback) {
            on_render_shutdown(); native_fallback_.store(true, std::memory_order_release); return;
        }
        if (backend != services::game::hybrid::result::ready) { on_render_shutdown(); return; }
        if (!runtime_->matches(observed)) { runtime_->reset(); runtime_->remember(observed); }
        const bool left = services::input::physical_button_down(services::input::mouse_button::left);
        const bool right = services::input::physical_button_down(services::input::mouse_button::right);
        const bool shift = services::input::physical_key_down(VK_LSHIFT) || services::input::physical_key_down(VK_RSHIFT);
        const auto settings = settings_.read();
        const auto changes = runtime_->gestures.update(settings, {observed.hover, left, right, observed.cursor_empty});
        const auto refresh = [&] {
            bridge::container_snapshot fresh;
            if (bridge::capture_container(bridge::feature::mouse, fresh) != services::game::hybrid::result::ready ||
                !runtime_->matches(fresh)) return false;
            observed = std::move(fresh); return true;
        };
        for (int id : changes.right) {
            if (id < 0 || id >= static_cast<int>(observed.slots.size())) continue;
            const auto& slot = observed.slots[id];
            if (observed.cursor_empty || !slot.enabled || !slot.can_insert || !slot.compatible ||
                (!slot.empty && slot.count >= std::min(slot.limit, observed.cursor_limit))) continue;
            if (!bridge::container_act(observed, 1, id) || !refresh()) { on_render_shutdown(); return; }
        }
        const int id = changes.left;
        if (id < 0 || id >= static_cast<int>(observed.slots.size())) return;
        const auto& slot = observed.slots[id];
        if (!slot.enabled || slot.empty) return;
        int action = -1;
        if (observed.cursor_empty) {
            if (shift && settings.left_click_without_item) action = 2;
        } else if (settings.left_click_with_item && slot.compatible) {
            if (shift) action = 2;
            else if (observed.cursor_count > 0 && slot.count > 0 &&
                static_cast<long long>(observed.cursor_count) + slot.count <= observed.cursor_limit) action = 3;
        }
        if (action >= 0 && !bridge::container_act(observed, action, id)) on_render_shutdown();
    } catch (...) { platform::jni_context::clear_exception(); on_render_shutdown(); }
}

void modules::mouse_tweaks::on_render_shutdown()
{
    runtime_->reset();
    services::game::inventory_bridge::cancel(services::game::inventory_bridge::feature::mouse);
}

void modules::mouse_tweaks::run(::cache& cache)
{
    if constexpr (mc::target.build_id == 12111)
        if (!native_fallback_.load(std::memory_order_acquire)) return;
	const auto settings_snapshot = settings_.read();
	if (gui::draw || !cache.is_valid() || !cache.player || !cache.instance || !jni::get_env())
	{
		reset_state(true);
		return;
	}

	try
	{
		jni::frame frame{ 64 };

		maps::Screen current_screen = cache.instance.currentScreen.get();
		if (!current_screen || platform::jni_context::clear_exception() ||
			!current_screen.is_instance_of<maps::HandledScreen>())
		{
			platform::jni_context::clear_exception();
			reset_state(true);
			return;
		}

		if (!track_screen(current_screen.object_instance))
		{
			reset_state(true);
			return;
		}

		maps::HandledScreen screen(current_screen.object_instance, false);
		maps::ScreenHandler handler = screen.handler.get();
		maps::Slot focused_slot = screen.focusedSlot.get();
		if (!handler || platform::jni_context::clear_exception())
		{
			reset_state(false);
			return;
		}

		int focused_slot_id = -1;
		if (focused_slot)
		{
			focused_slot_id = focused_slot.id.get();
			if (platform::jni_context::clear_exception())
			{
				reset_state(false);
				return;
			}
		}

		maps::ItemStack cursor_stack;
		bool cursor_empty = true;
		if (!get_cursor_stack(handler, cursor_stack, cursor_empty))
		{
			reset_state(false);
			return;
		}

		const bool rmb_down = key_down(VK_RBUTTON);
		const bool lmb_down = key_down(VK_LBUTTON);
		const bool shift_down = key_down(VK_LSHIFT) || key_down(VK_RSHIFT);
		if (!state.cursor_state_known)
		{
			state.cursor_was_empty = cursor_empty;
			state.cursor_state_known = true;
		}
		const bool rmb_pressed = rmb_down && !state.last_rmb_down;
		const bool lmb_pressed = lmb_down && !state.last_lmb_down;

		// RMB tweak only starts if an item was already held before pressing RMB.
		// This preserves vanilla's half-stack pickup when RMB starts on an item.
		if (!settings_snapshot.right_click_tweak || !rmb_down)
		{
			state.rmb_drag_active = false;
			state.rmb_slot = -1;
			state.rmb_left_original_slot = false;
		}
		else if (rmb_pressed && !state.cursor_was_empty)
		{
			state.rmb_drag_active = true;
			state.rmb_slot = focused_slot_id;
			state.rmb_left_original_slot = false;
		}

		// LMB tweak starts only when the cursor was empty before the initial click.
		// Once vanilla picks that stack up, dragging can collect compatible stacks.
		if ((!settings_snapshot.left_click_with_item && !settings_snapshot.left_click_without_item) || !lmb_down)
		{
			state.lmb_drag_active = false;
			state.lmb_slot = -1;
		}
		else if (lmb_pressed && state.cursor_was_empty)
		{
			state.lmb_drag_active = true;
			state.lmb_slot = focused_slot_id;
		}

		maps::ClientPlayerInteractionManager interaction_manager =
			cache.instance.interactionManager.get();
		maps::PlayerEntity player = cache.player.toPlayerEntity();
		if (!interaction_manager || !player || platform::jni_context::clear_exception())
		{
			reset_state(false);
			return;
		}

		maps::SlotActionType action_types{};

		// Slot transitions are processed immediately. There is no fixed timer: a
		// fast cursor cannot lose its current slot just because an 18 ms gate is
		// still active, and revisiting a slot places another item as in the mod.
		if (state.rmb_drag_active && focused_slot_id != state.rmb_slot)
		{
			if (!state.rmb_left_original_slot)
			{
				state.rmb_left_original_slot = true;
				if (state.rmb_slot >= 0 && !cursor_empty)
				{
					maps::Slot original_slot = get_slot_by_id(handler, state.rmb_slot);
					if (original_slot && can_insert_one(original_slot, cursor_stack))
					{
						maps::SlotActionType pickup = action_types.PICKUP.get();
						click_slot(interaction_manager, handler, player,
							state.rmb_slot, 1, pickup);
					}
				}
			}

			state.rmb_slot = focused_slot_id;
			if (focused_slot && can_insert_one(focused_slot, cursor_stack))
			{
				maps::SlotActionType pickup = action_types.PICKUP.get();
				click_slot(interaction_manager, handler, player,
					focused_slot_id, 1, pickup);
			}
		}

		if (state.lmb_drag_active && focused_slot_id != state.lmb_slot)
		{
			state.lmb_slot = focused_slot_id;
			if (focused_slot && slot_has_item(focused_slot))
			{
				if (cursor_empty)
				{
					if (shift_down && settings_snapshot.left_click_without_item)
					{
						maps::SlotActionType quick_move = action_types.QUICK_MOVE.get();
						click_slot(interaction_manager, handler, player,
							focused_slot_id, 0, quick_move);
					}
				}
				else if (settings_snapshot.left_click_with_item)
				{
					maps::ItemStack target_stack = focused_slot.getStack.call();
					if (target_stack && !platform::jni_context::clear_exception() &&
						stacks_are_compatible(target_stack, cursor_stack))
					{
						if (shift_down)
						{
							maps::SlotActionType quick_move = action_types.QUICK_MOVE.get();
							click_slot(interaction_manager, handler, player,
								focused_slot_id, 0, quick_move);
						}
						else
						{
							bool crafting_output = false;
							if (can_merge_entire_stack(
								focused_slot, cursor_stack, crafting_output))
							{
								maps::SlotActionType pickup = action_types.PICKUP.get();
								if (click_slot(interaction_manager, handler, player,
									focused_slot_id, 0, pickup) && !crafting_output)
									click_slot(interaction_manager, handler, player,
										focused_slot_id, 0, pickup);
							}
						}
					}
				}
			}
		}

		state.last_rmb_down = rmb_down;
		state.last_lmb_down = lmb_down;
		state.cursor_was_empty = cursor_empty;
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		reset_state(false);
	}
}

void modules::mouse_tweaks::on_disable(::cache& cache)
{
    if constexpr (mc::target.build_id == 12111) {
        reset_requested_.store(true, std::memory_order_release);
        if (!native_fallback_.load(std::memory_order_acquire)) return;
    }
	(void)cache;
	reset_state(true);
}

void modules::mouse_tweaks::render_options()
{
	gui::settings::render(settings_, module_settings::mouse_tweaks::schema());
	ImGui::TextDisabled("%s", "RMB revisits a slot; LMB collects matching stacks.");
	ImGui::TextDisabled("%s", "Hold Shift while dragging to quick-move hovered stacks.");
}
