#include "../../ui/settings_renderer.hpp"
#include "anchor_engine.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../../services/input/input_service.hpp"
#include "../../services/game/hotbar_service.hpp"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <random>

#include <imgui/imgui.h>

#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include "../../mappings.hpp"
#include "../core/module.hpp"

namespace
{
	using steady_clock = std::chrono::steady_clock;
	std::mt19937_64 random_engine(std::random_device{}());

	void reset_runtime(anchor_engine::runtime& state)
	{
		state.current = anchor_engine::runtime::phase::idle;
		state.old_slot = -1;
		state.anchor_slot = -1;
		state.glowstone_slot = -1;
		state.totem_slot = -1;
		state.shield_slot = -1;
		state.support_x = state.support_y = state.support_z = 0;
		state.face_x = 0;
		state.face_y = 1;
		state.face_z = 0;
		state.anchor_x = state.anchor_y = state.anchor_z = 0;
		state.retries = 0;
		state.confirmation_timeout_ms = 0;
		state.next_action = {};
		state.phase_started = {};
		state.last_attempt = {};
		state.next_camera_move = {};
	}

	int sample_delay(int base, int variance, int maximum)
	{
		base = std::clamp(base, 0, maximum);
		variance = std::clamp(variance, 0, 80);
		if (variance == 0)
			return base;
		std::normal_distribution<double> distribution(0.0, (std::max)(1.0, variance * 0.38));
		return std::clamp(base + static_cast<int>(std::lround(distribution(random_engine))), 0, maximum);
	}

	bool elapsed(steady_clock::time_point from, steady_clock::time_point now, int milliseconds)
	{
		return from.time_since_epoch().count() != 0 &&
			now - from >= std::chrono::milliseconds((std::max)(0, milliseconds));
	}

	bool select_hotbar_slot(
		maps::ClientPlayerEntity player,
		maps::ClientPlayerInteractionManager manager,
		int slot,
		bool hybrid_input)
	{
		if (!player || !manager || slot < 0 || slot > 8)
			return false;
		maps::PlayerInventory inventory = player.getInventory();
		if (!inventory || platform::jni_context::clear_exception())
			return false;
		const int current = inventory.selectedSlot.get();
		if (platform::jni_context::clear_exception())
			return false;
		if (current == slot)
			return true;
		if (hybrid_input)
			services::input::key_pulse(static_cast<UINT>('1' + slot));
		return services::hotbar::select(inventory, manager, slot);
	}

	bool stack_is_item(const maps::DefaultedList& items, int slot, const maps::Item& wanted, const char* name_hint = nullptr)
	{
		if (!items || slot < 0 || slot > 8)
			return false;
		maps::Object object = items.get.call(static_cast<jint>(slot));
		if (!object || platform::jni_context::clear_exception())
			return false;
		maps::ItemStack stack(object.object_instance, false);
		if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
			return false;
		maps::Item item = stack.item.get();
		if (!item || platform::jni_context::clear_exception())
			return false;
		if (wanted && item.is_same_object(wanted))
			return true;
		if (name_hint && item.getTranslationKey.is_valid())
		{
			maps::String key = item.getTranslationKey.call();
			if (key && !platform::jni_context::clear_exception())
			{
				std::string s = key.to_string();
				if (s.find(name_hint) != std::string::npos)
					return true;
			}
		}
		return false;
	}

	int find_item_slot(const maps::DefaultedList& items, const maps::Item& wanted, const char* name_hint = nullptr)
	{
		for (int slot = 0; slot < 9; ++slot)
			if (stack_is_item(items, slot, wanted, name_hint))
				return slot;
		return -1;
	}

	int find_shield_slot(
		const maps::DefaultedList& items,
		const maps::Item& anchor,
		const maps::Item& glowstone,
		const maps::Item& totem)
	{
		for (int slot = 0; slot < 9; ++slot)
		{
			if (stack_is_item(items, slot, anchor, "anchor") ||
				stack_is_item(items, slot, glowstone, "glowstone") ||
				stack_is_item(items, slot, totem, "totem"))
				continue;
			maps::Object object = items.get.call(static_cast<jint>(slot));
			maps::ItemStack stack = object ? maps::ItemStack(object.object_instance, false) : maps::ItemStack{};
			maps::Item item = stack && stack.isEmpty.call() != JNI_TRUE ? stack.item.get() : maps::Item{};
			if (platform::jni_context::clear_exception())
				continue;
			if (item && item.is_instance_of<maps::BlockItem>() && !platform::jni_context::clear_exception())
				return slot;
			if (item && item.getTranslationKey.is_valid())
			{
				maps::String key = item.getTranslationKey.call();
				if (key && !platform::jni_context::clear_exception())
				{
					std::string s = key.to_string();
					if (s.find("obsidian") != std::string::npos || s.find("block") != std::string::npos ||
						s.find("slab") != std::string::npos || s.find("cobweb") != std::string::npos)
						return slot;
				}
			}
			platform::jni_context::clear_exception();
		}
		return -1;
	}

	maps::BlockState state_at(const maps::ClientWorld& world, int x, int y, int z)
	{
		maps::BlockPos position = maps::BlockPos::new_object(
			&maps::BlockPos::constructor, static_cast<jint>(x), static_cast<jint>(y), static_cast<jint>(z));
		if (!position || platform::jni_context::clear_exception())
			return {};
		return world.getBlockState.call(position);
	}

	bool is_air(const maps::ClientWorld& world, int x, int y, int z)
	{
		maps::BlockState state = state_at(world, x, y, z);
		return state && state.isAir.call() == JNI_TRUE && !platform::jni_context::clear_exception();
	}

	bool is_anchor(const maps::ClientWorld& world, int x, int y, int z)
	{
		maps::BlockState state = state_at(world, x, y, z);
		if (!state || platform::jni_context::clear_exception())
			return false;
		maps::Block anchor = maps::Blocks{}.RESPAWN_ANCHOR.get();
		if (anchor && state.isOf.call(anchor) == JNI_TRUE && !platform::jni_context::clear_exception())
			return true;
		platform::jni_context::clear_exception();
		if (state.getBlock.is_valid())
		{
			maps::Block b = state.getBlock.call();
			if (b && b.asItem.is_valid())
			{
				maps::Item it = b.asItem.call();
				if (it && it.getTranslationKey.is_valid())
				{
					maps::String key = it.getTranslationKey.call();
					if (key && !platform::jni_context::clear_exception())
					{
						std::string s = key.to_string();
						if (s.find("anchor") != std::string::npos)
							return true;
					}
				}
			}
		}
		platform::jni_context::clear_exception();
		return false;
	}

	int anchor_charges(const maps::ClientWorld& world, int x, int y, int z)
	{
		maps::BlockState state = state_at(world, x, y, z);
		if (!state || platform::jni_context::clear_exception())
			return -1;
		if (!is_anchor(world, x, y, z))
			return -1;
		try
		{
			maps::IntProperty property = maps::RespawnAnchorBlock{}.CHARGES.get();
			if (property && !platform::jni_context::clear_exception())
			{
				maps::Comparable value = state.get.call(static_cast<maps::Property>(property));
				if (value && !platform::jni_context::clear_exception())
				{
					const int charges = maps::Integer(value.object_instance, false).intValue.call();
					if (!platform::jni_context::clear_exception() && charges >= 0 && charges <= 4)
						return charges;
				}
			}
		}
		catch (...)
		{
			platform::jni_context::clear_exception();
		}
		return -1;
	}

	maps::Direction direction_for(int x, int y, int z)
	{
		if (y < 0) return maps::Direction{}.DOWN.get();
		if (y > 0) return maps::Direction{}.UP.get();
		if (z < 0) return maps::Direction{}.NORTH.get();
		if (z > 0) return maps::Direction{}.SOUTH.get();
		if (x < 0) return maps::Direction{}.WEST.get();
		if (x > 0) return maps::Direction{}.EAST.get();
		return {};
	}

	bool make_hit(
		int block_x, int block_y, int block_z,
		int face_x, int face_y, int face_z,
		maps::BlockHitResult& result)
	{
		maps::Direction face = direction_for(face_x, face_y, face_z);
		maps::BlockPos position = maps::BlockPos::new_object(
			&maps::BlockPos::constructor,
			static_cast<jint>(block_x), static_cast<jint>(block_y), static_cast<jint>(block_z));
		maps::Vec3d hit_position = maps::Vec3d::new_object(
			&maps::Vec3d::constructor,
			block_x + 0.5 + face_x * 0.5,
			block_y + 0.5 + face_y * 0.5,
			block_z + 0.5 + face_z * 0.5);
		if (!face || !position || !hit_position || platform::jni_context::clear_exception())
			return false;
		result = maps::BlockHitResult::new_object(
			&maps::BlockHitResult::constructor,
			hit_position, face, position, static_cast<jboolean>(JNI_FALSE));
		return result && !platform::jni_context::clear_exception();
	}

	bool interact(
		maps::ClientPlayerEntity player,
		maps::ClientPlayerInteractionManager manager,
		const maps::BlockHitResult& hit)
	{
		maps::Hand hand = maps::Hand{}.MAIN_HAND.get();
		if (!player || !manager || !hit || !hand || platform::jni_context::clear_exception())
			return false;
		manager.interactBlock.call(player, hand, hit);
		if (platform::jni_context::clear_exception())
			return false;
		player.swingHand.call(hand);
		return !platform::jni_context::clear_exception();
	}

	bool capture_target(const maps::MinecraftClient& instance, const maps::ClientWorld& world, anchor_engine::runtime& state)
	{
		maps::HitResult generic = instance.crosshairTarget.get();
		if (!generic || platform::jni_context::clear_exception())
			return false;
		if (!generic.is_instance_of<maps::BlockHitResult>())
			return false;
		maps::BlockHitResult hit(generic.object_instance, false);
		maps::BlockPos support = hit.getBlockPos.call();
		maps::Direction side = hit.getSide.call();
		if (!support || !side || platform::jni_context::clear_exception())
			return false;
		state.support_x = support.getX.call();
		state.support_y = support.getY.call();
		state.support_z = support.getZ.call();
		state.face_x = side.getOffsetX.call();
		state.face_y = side.getOffsetY.call();
		state.face_z = side.getOffsetZ.call();
		if (platform::jni_context::clear_exception())
			return false;

		// If looking directly at an existing anchor, use that as the anchor position!
		if (is_anchor(world, state.support_x, state.support_y, state.support_z))
		{
			state.anchor_x = state.support_x;
			state.anchor_y = state.support_y;
			state.anchor_z = state.support_z;
			return true;
		}

		state.anchor_x = state.support_x + state.face_x;
		state.anchor_y = state.support_y + state.face_y;
		state.anchor_z = state.support_z + state.face_z;
		return true;
	}

	bool grounded(maps::ClientPlayerEntity player)
	{
		if (!player) return false;
		return platform::adapter::get().is_on_ground(player);
	}

	bool stationary_grounded(maps::ClientPlayerEntity player)
	{
		if (!grounded(player))
			return false;
		if ((GetAsyncKeyState('W') & 0x8000) != 0 || (GetAsyncKeyState('A') & 0x8000) != 0 ||
			(GetAsyncKeyState('S') & 0x8000) != 0 || (GetAsyncKeyState('D') & 0x8000) != 0)
			return false;
		maps::Vec3d velocity = player.getVelocity.call();
		if (!velocity || platform::jni_context::clear_exception())
			return false;
		const double x = velocity.x.get();
		const double z = velocity.z.get();
		return !platform::jni_context::clear_exception() && x * x + z * z <= 0.0025;
	}

	bool movement_allowed(maps::ClientPlayerEntity player, anchor_engine::profile mode)
	{
		if (mode == anchor_engine::profile::blatant)
			return true;
		return mode == anchor_engine::profile::safe
			? stationary_grounded(player)
			: grounded(player);
	}

	void camera_move(float strength)
	{
		strength = std::clamp(strength, 0.0f, 1.2f);
		if (strength <= 0.0f)
			return;
		std::normal_distribution<double> offset(0.0, 1.0 + strength * 2.2);
		const LONG x = static_cast<LONG>(std::lround(offset(random_engine)));
		const LONG y = static_cast<LONG>(std::lround(offset(random_engine) * 0.65));
		if (x != 0 || y != 0)
			services::input::move_relative(x, y);
	}

	bool restore_slot(
		maps::ClientPlayerEntity player,
		maps::ClientPlayerInteractionManager manager,
		anchor_engine::config& settings,
		anchor_engine::runtime& state)
	{
		return state.old_slot < 0 ||
			select_hotbar_slot(player, manager, state.old_slot, settings.hybrid_slot_input);
	}
}

bool anchor_engine::run_overlay(config& settings, runtime& state, profile mode)
{
	if (!platform::jni_context::bind_current_thread())
		return true; // Keep enabled, try next frame
	std::lock_guard lock(state.mutex);
	jni::frame frame{ 256 };

	settings.switch_delay = std::clamp(settings.switch_delay, 0, 180);
	settings.action_delay = std::clamp(settings.action_delay, 0, 200);
	settings.shield_delay = std::clamp(settings.shield_delay, 0, 220);
	settings.return_delay = std::clamp(settings.return_delay, 0, 160);
	settings.timing_variance = std::clamp(settings.timing_variance, 0, 80);
	settings.camera_strength = std::clamp(settings.camera_strength, 0.0f, 1.2f);

	maps::MinecraftClient instance = maps::MinecraftClient{}.instance.get();
	maps::ClientWorld world = instance ? instance.world.get() : maps::ClientWorld{};
	maps::ClientPlayerEntity player = instance ? instance.player.get() : maps::ClientPlayerEntity{};
	maps::ClientPlayerInteractionManager manager = instance
		? instance.interactionManager.get()
		: maps::ClientPlayerInteractionManager{};
	const HWND window = gui::get_window();

	auto finish = [&]()
	{
		if (player && manager)
			restore_slot(player, manager, settings, state);
		reset_runtime(state);
		platform::jni_context::clear_exception();
		return false; // Tells the caller module to disable itself
	};

	// If game or window is not ready, do not crash or abort permanently; stay ready
	if (!instance || !world || !player || !manager || platform::jni_context::clear_exception())
		return true;

	if (!window || GetForegroundWindow() != window || gui::draw || gui::loader_visible || instance.currentScreen.get())
	{
		return finish();
	}

	const auto now = steady_clock::now();

	auto delay = [&](int base, int maximum)
	{
		return mode == profile::blatant ? 0 : sample_delay(base, settings.timing_variance, maximum);
	};
	auto select = [&](int slot)
	{
		return select_hotbar_slot(player, manager, slot, settings.hybrid_slot_input);
	};
	auto anchor_hit = [&]()
	{
		const maths::vector3d player_position = player.get_position();
		const double x_distance = player_position.x - (state.anchor_x + 0.5);
		const double z_distance = player_position.z - (state.anchor_z + 0.5);
		const int face_x = std::abs(x_distance) >= std::abs(z_distance)
			? (x_distance >= 0.0 ? 1 : -1)
			: 0;
		const int face_z = face_x == 0 ? (z_distance >= 0.0 ? 1 : -1) : 0;
		maps::BlockHitResult hit{};
		return make_hit(state.anchor_x, state.anchor_y, state.anchor_z, face_x, 0, face_z, hit) &&
			interact(player, manager, hit);
	};
	auto initial_place = [&]()
	{
		maps::BlockHitResult hit{};
		return make_hit(state.support_x, state.support_y, state.support_z,
			state.face_x, state.face_y, state.face_z, hit) && interact(player, manager, hit);
	};
	auto shield_place = [&]()
	{
		maps::BlockHitResult hit{};
		return make_hit(state.anchor_x, state.anchor_y, state.anchor_z, 0, 1, 0, hit) &&
			interact(player, manager, hit);
	};
	auto begin_confirmation = [&](runtime::phase next, int timeout)
	{
		state.current = next;
		state.phase_started = now;
		state.last_attempt = now;
		state.retries = 0;
		state.confirmation_timeout_ms = timeout;
		state.next_action = now;
	};

	if (state.current == runtime::phase::idle)
	{
		// Macro burst check: if conditions not met, disable module immediately
		if (!movement_allowed(player, mode))
			return finish();

		// Try to capture target block or existing anchor
		if (!capture_target(instance, world, state))
			return finish();

		const bool target_is_existing_anchor = is_anchor(world, state.anchor_x, state.anchor_y, state.anchor_z);
		if (!target_is_existing_anchor && !is_air(world, state.anchor_x, state.anchor_y, state.anchor_z))
			return finish();

		maps::PlayerInventory inventory = player.getInventory();
		maps::DefaultedList items = inventory ? inventory.main.get() : maps::DefaultedList{};
		maps::Items vanilla{};
		maps::Item anchor = vanilla.RESPAWN_ANCHOR.get();
		maps::Item glowstone = vanilla.GLOWSTONE.get();
		maps::Item totem = vanilla.TOTEM_OF_UNDYING.get();
		if (!inventory || !items || platform::jni_context::clear_exception())
			return finish();

		state.old_slot = inventory.selectedSlot.get();
		state.anchor_slot = find_item_slot(items, anchor, "anchor");
		state.glowstone_slot = find_item_slot(items, glowstone, "glowstone");
		state.totem_slot = find_item_slot(items, totem, "totem");

		// If no totem in hotbar, fallback to weapon, old_slot, anchor_slot, or any slot != glowstone
		if (state.totem_slot < 0)
		{
			if (state.old_slot >= 0 && state.old_slot != state.glowstone_slot)
				state.totem_slot = state.old_slot;
			else if (state.anchor_slot >= 0 && state.anchor_slot != state.glowstone_slot)
				state.totem_slot = state.anchor_slot;
			else
			{
				for (int s = 0; s < 9; ++s)
				{
					if (s != state.glowstone_slot) { state.totem_slot = s; break; }
				}
			}
		}

		state.shield_slot = mode == profile::safe
			? find_shield_slot(items, anchor, glowstone, totem)
			: -1;

		if (state.glowstone_slot < 0 || (!target_is_existing_anchor && state.anchor_slot < 0))
			return finish(); // Missing items in hotbar, abort

		if (target_is_existing_anchor)
		{
			int charges = anchor_charges(world, state.anchor_x, state.anchor_y, state.anchor_z);
			if (charges >= 1)
			{
				state.current = runtime::phase::detonate;
				state.next_action = now + std::chrono::milliseconds(delay(settings.action_delay, 200));
			}
			else
			{
				state.current = runtime::phase::charge_anchor;
				state.next_action = now + std::chrono::milliseconds(delay(settings.switch_delay, 180));
			}
		}
		else
		{
			if (!select_hotbar_slot(player, manager, state.anchor_slot, settings.hybrid_slot_input))
				return finish();
			state.current = runtime::phase::place_anchor;
			state.next_action = now + std::chrono::milliseconds(delay(settings.switch_delay, 180));
		}

		if (mode != profile::blatant)
			return true;
	}

	if (!movement_allowed(player, mode))
		return true; // Wait for grounded

	if (mode != profile::blatant && settings.camera_movement &&
		state.current != runtime::phase::idle && now >= state.next_camera_move)
	{
		camera_move(settings.camera_strength);
		state.next_camera_move = now + std::chrono::milliseconds(
			std::uniform_int_distribution<int>(24, 58)(random_engine));
	}

	if (now < state.next_action)
		return true;

	const int transition_budget = mode == profile::blatant ? 12 : 1;
	const int minimum_confirmation = mode == profile::blatant ? 0 : (mode == profile::safe ? 45 : 25);
	const int retry_interval = mode == profile::blatant ? 35 : 65;
	const int maximum_retries = mode == profile::blatant ? 4 : 3;
	const int timeout = mode == profile::blatant ? 350 : 650;

	for (int transition = 0; transition < transition_budget; ++transition)
	{
		switch (state.current)
		{
		case runtime::phase::place_anchor:
			if (!select(state.anchor_slot) || !initial_place())
				return finish();
			begin_confirmation(runtime::phase::wait_anchor, timeout);
			if (mode == profile::blatant) continue;
			return true;

		case runtime::phase::wait_anchor:
			if (is_anchor(world, state.anchor_x, state.anchor_y, state.anchor_z) &&
				elapsed(state.phase_started, now, minimum_confirmation))
			{
				if (!select(state.glowstone_slot)) return finish();
				state.current = runtime::phase::charge_anchor;
				state.next_action = now + std::chrono::milliseconds(delay(settings.switch_delay, 180));
				if (mode == profile::blatant) continue;
				return true;
			}
			if (state.retries < maximum_retries && elapsed(state.last_attempt, now, retry_interval))
			{
				if (!select(state.anchor_slot) || !initial_place()) return finish();
				++state.retries;
				state.last_attempt = now;
			}
			if (elapsed(state.phase_started, now, state.confirmation_timeout_ms)) return finish();
			return true;

		case runtime::phase::charge_anchor:
			if (!select(state.glowstone_slot) || !anchor_hit())
				return finish();
			begin_confirmation(runtime::phase::wait_charge, timeout);
			if (mode == profile::blatant) continue;
			return true;

		case runtime::phase::wait_charge:
			{
				int charges = anchor_charges(world, state.anchor_x, state.anchor_y, state.anchor_z);
				if ((charges >= 1 || (charges == -1 && elapsed(state.last_attempt, now, (std::max)(minimum_confirmation, 35)))) &&
					elapsed(state.phase_started, now, minimum_confirmation))
				{
					if (mode == profile::safe && state.shield_slot >= 0)
					{
						if (!select(state.shield_slot)) return finish();
						state.current = runtime::phase::place_shield;
						state.next_action = now + std::chrono::milliseconds(delay(settings.shield_delay, 220));
					}
					else
					{
						if (!select(state.totem_slot)) return finish();
						state.current = runtime::phase::detonate;
						state.next_action = now + std::chrono::milliseconds(delay(settings.action_delay, 200));
					}
					if (mode == profile::blatant) continue;
					return true;
				}
				if (state.retries < maximum_retries && elapsed(state.last_attempt, now, retry_interval))
				{
					if (!select(state.glowstone_slot) || !anchor_hit()) return finish();
					++state.retries;
					state.last_attempt = now;
				}
				if (elapsed(state.phase_started, now, state.confirmation_timeout_ms)) return finish();
				return true;
			}

		case runtime::phase::place_shield:
			if (!select(state.shield_slot) || !shield_place())
				return finish();
			begin_confirmation(runtime::phase::wait_shield, timeout);
			return true;

		case runtime::phase::wait_shield:
			if (!is_air(world, state.anchor_x, state.anchor_y + 1, state.anchor_z) &&
				elapsed(state.phase_started, now, minimum_confirmation))
			{
				if (!select(state.totem_slot)) return finish();
				state.current = runtime::phase::detonate;
				state.next_action = now + std::chrono::milliseconds(delay(settings.action_delay, 200));
				return true;
			}
			if (state.retries < maximum_retries && elapsed(state.last_attempt, now, retry_interval))
			{
				if (!select(state.shield_slot) || !shield_place()) return finish();
				++state.retries;
				state.last_attempt = now;
			}
			if (elapsed(state.phase_started, now, state.confirmation_timeout_ms)) return finish();
			return true;

		case runtime::phase::detonate:
			if (!select(state.totem_slot) || !anchor_hit())
				return finish();
			begin_confirmation(runtime::phase::wait_detonation, timeout);
			if (mode == profile::blatant) continue;
			return true;

		case runtime::phase::wait_detonation:
			if (!is_anchor(world, state.anchor_x, state.anchor_y, state.anchor_z))
			{
				state.current = runtime::phase::restore;
				state.next_action = now + std::chrono::milliseconds(delay(settings.return_delay, 160));
				if (mode == profile::blatant) continue;
				return true;
			}
			if (state.retries < maximum_retries && elapsed(state.last_attempt, now, retry_interval))
			{
				if (!select(state.totem_slot) || !anchor_hit()) return finish();
				++state.retries;
				state.last_attempt = now;
			}
			if (elapsed(state.phase_started, now, state.confirmation_timeout_ms)) return finish();
			return true;

		case runtime::phase::restore:
			restore_slot(player, manager, settings, state);
			reset_runtime(state);
			return false; // Successful sequence completed

		default:
			return finish();
		}
	}
	return true;
}

void anchor_engine::stop(::cache& cache, runtime& state)
{
	std::lock_guard lock(state.mutex);
	if (platform::jni_context::bind_current_thread() && state.old_slot >= 0 && cache.instance && cache.player)
	{
		jni::frame frame{ 48 };
		maps::ClientPlayerInteractionManager manager = cache.instance.interactionManager.get();
		if (manager && !platform::jni_context::clear_exception())
			select_hotbar_slot(cache.player, manager, state.old_slot, true);
		platform::jni_context::clear_exception();
	}
	reset_runtime(state);
}

void anchor_engine::render_options(config& settings, profile mode, const char* id_suffix)
{
    ImGui::PushID(id_suffix);
    gui::settings::render(settings, schema(mode));
    ImGui::PopID();
}
