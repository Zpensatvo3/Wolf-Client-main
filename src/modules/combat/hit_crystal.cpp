#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/hotbar_service.hpp"
#include "../../services/game/attack_service.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>

#include <imgui/imgui.h>

#include "hit_crystal.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include "../../mappings.hpp"


namespace
{
	using steady_clock = std::chrono::steady_clock;
bool stack_is_item(const maps::DefaultedList& items, int slot, const maps::Item& wanted)
	{
		if (!items || !wanted || slot < 0 || slot > 8)
			return false;
		maps::Object object = items.get.call(static_cast<jint>(slot));
		if (!object || platform::jni_context::clear_exception())
			return false;
		maps::ItemStack stack(object.object_instance, false);
		if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
			return false;
		maps::Item item = stack.item.get();
		return item && !platform::jni_context::clear_exception() && item.is_same_object(wanted);
	}

	int find_item_slot(const maps::DefaultedList& items, const maps::Item& wanted)
	{
		for (int slot = 0; slot < 9; ++slot)
			if (stack_is_item(items, slot, wanted))
				return slot;
		return -1;
	}
maps::BlockState state_at(const maps::ClientWorld& world, int x, int y, int z)
	{
		maps::BlockPos position = maps::BlockPos::new_object(
			&maps::BlockPos::constructor, static_cast<jint>(x), static_cast<jint>(y), static_cast<jint>(z));
		return position && !platform::jni_context::clear_exception()
			? world.getBlockState.call(position)
			: maps::BlockState{};
	}

	bool is_obsidian(const maps::ClientWorld& world, int x, int y, int z)
	{
		maps::BlockState state = state_at(world, x, y, z);
		maps::Block obsidian = maps::Blocks{}.OBSIDIAN.get();
		return state && obsidian && !platform::jni_context::clear_exception() &&
			state.isOf.call(obsidian) == JNI_TRUE && !platform::jni_context::clear_exception();
	}

	bool is_air(const maps::ClientWorld& world, int x, int y, int z)
	{
		maps::BlockState state = state_at(world, x, y, z);
		return state && state.isAir.call() == JNI_TRUE && !platform::jni_context::clear_exception();
	}

	bool capture_obsidian(
		const maps::MinecraftClient& instance,
		const maps::ClientWorld& world,
		int& x, int& y, int& z)
	{
		maps::HitResult generic = instance.crosshairTarget.get();
		maps::HitResult$Type block_type = maps::HitResult$Type{}.BLOCK.get();
		if (!generic || !block_type || platform::jni_context::clear_exception() ||
			!generic.getType.call().is_same_object(block_type) || platform::jni_context::clear_exception())
			return false;

		maps::BlockHitResult hit(generic.object_instance, false);
		maps::BlockPos position = hit.getBlockPos.call();
		if (!position || platform::jni_context::clear_exception())
			return false;
		x = position.getX.call();
		y = position.getY.call();
		z = position.getZ.call();
		return !platform::jni_context::clear_exception() && is_obsidian(world, x, y, z);
	}

	bool make_top_hit(int x, int y, int z, maps::BlockHitResult& result)
	{
		maps::Direction up = maps::Direction{}.UP.get();
		maps::BlockPos position = maps::BlockPos::new_object(
			&maps::BlockPos::constructor, static_cast<jint>(x), static_cast<jint>(y), static_cast<jint>(z));
		maps::Vec3d point = maps::Vec3d::new_object(
			&maps::Vec3d::constructor, x + 0.5, y + 1.0, z + 0.5);
		if (!up || !position || !point || platform::jni_context::clear_exception())
			return false;
		result = maps::BlockHitResult::new_object(
			&maps::BlockHitResult::constructor,
			point, up, position, static_cast<jboolean>(JNI_FALSE));
		return result && !platform::jni_context::clear_exception();
	}

	bool place_crystal(
		const maps::ClientPlayerEntity& player,
		const maps::ClientPlayerInteractionManager& manager,
		int x, int y, int z)
	{
		maps::BlockHitResult hit{};
		maps::Hand hand = maps::Hand{}.MAIN_HAND.get();
		if (!make_top_hit(x, y, z, hit) || !hand || platform::jni_context::clear_exception())
			return false;
		manager.interactBlock.call(player, hand, hit);
		if (platform::jni_context::clear_exception())
			return false;
		player.swingHand.call(hand);
		return !platform::jni_context::clear_exception();
	}

	maps::Entity find_target_crystal(
		const maps::ClientWorld& world,
		const maps::ClientPlayerEntity& player,
		double target_x, double target_y, double target_z,
		int required_id = -1)
	{
		maps::Entity best{};
		double best_distance_sq = 1.75;
		maps::Iterable entities = world.getEntities.call();
		maps::Iterator iterator = entities ? entities.iterator.call() : maps::Iterator{};
		if (!entities || !iterator || platform::jni_context::clear_exception())
			return best;

		while (iterator.hasNext.call() == JNI_TRUE)
		{
			maps::Object object = iterator.next.call();
			maps::Entity entity(object.object_instance, false);
			if (!entity || !entity.is_instance_of<maps::EndCrystalEntity>() || platform::jni_context::clear_exception())
				continue;
			const int id = entity.getId.call();
			if (platform::jni_context::clear_exception() || (required_id >= 0 && id != required_id))
				continue;
			const maths::vector3d position = entity.get_position();
			const double dx = position.x - target_x;
			const double dy = position.y - target_y;
			const double dz = position.z - target_z;
			const double distance_sq = dx * dx + dy * dy + dz * dz;
			if (distance_sq < best_distance_sq &&
				player.squaredDistanceTo.call(entity) <= 36.0 && !platform::jni_context::clear_exception())
			{
				best = entity;
				best_distance_sq = distance_sq;
			}
		}
		platform::jni_context::clear_exception();
		return best;
	}

	maths::vector3d eye_position(maps::ClientPlayerEntity player)
	{
		const maths::vector3d position = player.get_position();
		maps::EntityPose pose = player.getPose.call();
		const float height = pose && !platform::jni_context::clear_exception()
			? player.getEyeHeight.call(pose)
			: 1.62f;
		platform::jni_context::clear_exception();
		return { position.x, position.y + height, position.z };
	}

	bool visible_to_crystal(
		const maps::ClientWorld& world,
		maps::ClientPlayerEntity player,
		maps::Entity crystal)
	{
		const maths::vector3d from = eye_position(player);
		maths::vector3d to = crystal.get_position();
		to.y += 0.45;
		maps::Vec3d start = maps::Vec3d::new_object(
			&maps::Vec3d::constructor, from.x, from.y, from.z);
		maps::Vec3d end = maps::Vec3d::new_object(
			&maps::Vec3d::constructor, to.x, to.y, to.z);
		maps::RaycastContext$ShapeType shape = maps::RaycastContext$ShapeType{}.COLLIDER.get();
		maps::RaycastContext$FluidHandling fluid = maps::RaycastContext$FluidHandling{}.NONE.get();
		if (!start || !end || !shape || !fluid || platform::jni_context::clear_exception())
			return false;
		maps::RaycastContext context = maps::RaycastContext::new_object(
			&maps::RaycastContext::constructor,
			start, end, shape, fluid, maps::Entity(player.object_instance, false));
		maps::BlockHitResult hit = context ? world.raycast.call(context) : maps::BlockHitResult{};
		if (!hit || platform::jni_context::clear_exception())
			return false;
		maps::HitResult$Type type = maps::HitResult(hit.object_instance, false).getType.call();
		maps::HitResult$Type block = maps::HitResult$Type{}.BLOCK.get();
		return type && block && !platform::jni_context::clear_exception() && !type.is_same_object(block);
	}

	bool attack_crystal(
		const maps::ClientPlayerEntity& player,
		const maps::ClientPlayerInteractionManager& manager,
		const maps::Entity& crystal)
	{
		return services::attack::attack_entity(manager, player, crystal);
	}
}

void modules::hit_crystal::stop_clicking(::cache& cache, bool restore)
{
    auto settings_snapshot = settings_.read();
	if (restore && settings_snapshot.restore_slot && old_slot >= 0 && old_slot <= 8 &&
		cache.player && cache.instance)
	{
		maps::ClientPlayerInteractionManager manager = cache.instance.interactionManager.get();
		if (manager && !platform::jni_context::clear_exception())
			services::hotbar::restore_if_owned(cache.player, manager, old_slot, crystal_slot);
		platform::jni_context::clear_exception();
	}

	old_slot = -1;
	crystal_slot = -1;
	target_crystal_id = -1;
	retries = 0;
	state = State::SEEK_PLACE;
	base_x = base_y = base_z = 0;
	target_x = target_y = target_z = 0.0;
	next_action_time = {};
	state_started = {};
}

void modules::hit_crystal::run(::cache&)
{
	// Block/entity interactions are synchronized with Minecraft's render thread.
}

void modules::hit_crystal::render_overlay()
{
    auto settings_snapshot = settings_.read();
	if (!platform::jni_context::bind_current_thread())
		return;
	std::lock_guard lock(state_mutex);
	jni::frame frame{ 320 };

	maps::MinecraftClient instance = maps::MinecraftClient{}.instance.get();
	maps::ClientWorld world = instance ? instance.world.get() : maps::ClientWorld{};
	maps::ClientPlayerEntity player = instance ? instance.player.get() : maps::ClientPlayerEntity{};
	maps::ClientPlayerInteractionManager manager = instance
		? instance.interactionManager.get()
		: maps::ClientPlayerInteractionManager{};
	const HWND window = gui::get_window();

	auto reset_cycle = [&](bool restore)
	{
		if (restore && settings_snapshot.restore_slot && old_slot >= 0 && player && manager)
			services::hotbar::restore_if_owned(player, manager, old_slot, crystal_slot);
		old_slot = -1;
		crystal_slot = -1;
		target_crystal_id = -1;
		retries = 0;
		state = State::SEEK_PLACE;
		base_x = base_y = base_z = 0;
		target_x = target_y = target_z = 0.0;
		next_action_time = {};
		state_started = {};
		platform::jni_context::clear_exception();
	};

	if (!instance || !world || !player || !manager || platform::jni_context::clear_exception() ||
		!window || GetForegroundWindow() != window || gui::draw || gui::loader_visible ||
		instance.currentScreen.get() || platform::jni_context::clear_exception())
	{
		reset_cycle(true);
		return;
	}

	auto now = steady_clock::now();
	settings_snapshot.place_delay = std::clamp(settings_snapshot.place_delay, 0, 250);
	settings_snapshot.break_delay = std::clamp(settings_snapshot.break_delay, 0, 200);

	try
	{
		if (state == State::WAIT_BREAK_CONFIRM)
		{
			maps::Entity crystal = find_target_crystal(
				world, player, target_x, target_y, target_z, target_crystal_id);
			if (!crystal)
			{
				if (settings_snapshot.restore_slot && old_slot >= 0)
					services::hotbar::restore_if_owned(player, manager, old_slot, crystal_slot);
				old_slot = -1;
				crystal_slot = -1;
				target_crystal_id = -1;
				retries = 0;
				state = State::WAIT_REPLACE;
				next_action_time = now + std::chrono::milliseconds(settings_snapshot.place_delay);
				return;
			}

			if (now >= next_action_time)
			{
				if (retries >= 3 || now - state_started > std::chrono::milliseconds(360) ||
					!visible_to_crystal(world, player, crystal))
				{
					reset_cycle(true);
					return;
				}
				if (attack_crystal(player, manager, crystal))
					++retries;
				next_action_time = now + std::chrono::milliseconds(
					(std::max)(35, settings_snapshot.break_delay));
			}
			return;
		}

		if (state == State::WAIT_CRYSTAL)
		{
			maps::Entity crystal = find_target_crystal(
				world, player, target_x, target_y, target_z);
			if (crystal && now >= next_action_time)
			{
				if (!visible_to_crystal(world, player, crystal) ||
					!attack_crystal(player, manager, crystal))
				{
					reset_cycle(true);
					return;
				}
				target_crystal_id = crystal.getId.call();
				if (platform::jni_context::clear_exception())
				{
					reset_cycle(true);
					return;
				}
				state = State::WAIT_BREAK_CONFIRM;
				state_started = now;
				retries = 0;
				next_action_time = now + std::chrono::milliseconds(
					(std::max)(35, settings_snapshot.break_delay));
				return;
			}

			if (now - state_started > std::chrono::milliseconds(480))
			{
				if (retries < 2 && is_obsidian(world, base_x, base_y, base_z) &&
					is_air(world, base_x, base_y + 1, base_z))
				{
					++retries;
					state = State::WAIT_PLACE;
					next_action_time = now + std::chrono::milliseconds(35);
				}
				else
					reset_cycle(true);
			}
			return;
		}

		if (state == State::WAIT_REPLACE)
		{
			if (now < next_action_time)
				return;
			state = State::SEEK_PLACE;
		}

		if (state == State::WAIT_PLACE)
		{
			if (now < next_action_time)
				return;
			if (!is_obsidian(world, base_x, base_y, base_z) ||
				!is_air(world, base_x, base_y + 1, base_z) ||
				!place_crystal(player, manager, base_x, base_y, base_z))
			{
				reset_cycle(true);
				return;
			}
			state = State::WAIT_CRYSTAL;
			state_started = now;
			next_action_time = now + std::chrono::milliseconds(settings_snapshot.break_delay);
			return;
		}

		if (state != State::SEEK_PLACE || now < next_action_time)
			return;

		if ((GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0 ||
			!capture_obsidian(instance, world, base_x, base_y, base_z) ||
			!is_air(world, base_x, base_y + 1, base_z))
		{
			reset_cycle(true);
			return;
		}

		target_x = base_x + 0.5;
		target_y = base_y + 1.0;
		target_z = base_z + 0.5;

		maps::PlayerInventory inventory = player.getInventory();
		maps::DefaultedList items = inventory ? inventory.main.get() : maps::DefaultedList{};
		maps::Item crystal_item = maps::Items{}.END_CRYSTAL.get();
		if (!inventory || !items || !crystal_item || platform::jni_context::clear_exception())
		{
			reset_cycle(true);
			return;
		}

		const int selected = inventory.selectedSlot.get();
		if (selected < 0 || selected > 8 || platform::jni_context::clear_exception())
		{
			reset_cycle(true);
			return;
		}

		if (settings_snapshot.auto_switch)
		{
			crystal_slot = find_item_slot(items, crystal_item);
			if (crystal_slot < 0)
			{
				reset_cycle(true);
				return;
			}
			if (selected != crystal_slot)
			{
				old_slot = selected;
				if (!services::hotbar::select(player, manager, crystal_slot))
				{
					reset_cycle(true);
					return;
				}
				next_action_time = now + std::chrono::milliseconds(25);
			}
			else
				next_action_time = now;
		}
		else
		{
			crystal_slot = selected;
			if (!stack_is_item(items, selected, crystal_item))
			{
				reset_cycle(false);
				return;
			}
			next_action_time = now;
		}

		retries = 0;
		state = State::WAIT_PLACE;
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		reset_cycle(true);
	}
}

void modules::hit_crystal::on_disable(::cache& cache)
{
	std::lock_guard lock(state_mutex);
	stop_clicking(cache, true);
}

void modules::hit_crystal::render_options()
{
    gui::settings::render(settings_, module_settings::hit_crystal::schema());
}
