#include "anchor_engine.hpp"
#include "anchor_timing.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../../services/input/input_service.hpp"
#include "../../services/game/hotbar_service.hpp"

#include <Windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <random>


#include "../../gui/gui.hpp"
#include "../../mappings.hpp"
#include "../core/module.hpp"

namespace
{
	using steady_clock = std::chrono::steady_clock;
	std::mt19937_64 random_engine(std::random_device{}());

	void reset_runtime(anchor_engine::runtime& state)
	{
		state.session.reset();
		state.owned_slot = -1;
		state.current = anchor_engine::runtime::phase::idle;
		state.old_slot = -1;
		state.anchor_slot = -1;
		state.glowstone_slot = -1;
		state.totem_slot = -1;
		state.shield_slot = -1;
		state.shield_x = state.shield_y = state.shield_z = 0;
		state.shield_planned = false;
		state.last_action_tick = -1;
		state.sequence_started = {};
		state.support_x = state.support_y = state.support_z = 0;
		state.face_x = 0;
		state.face_y = 1;
		state.face_z = 0;
		state.anchor_x = state.anchor_y = state.anchor_z = 0;
		state.retries = 0;
		state.confirmation_timeout_ms = 0;
		state.retry_interval_ms = 65;
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
		(void)anchor; (void)totem;
		// A random BlockItem may be a flower, slab or cobweb. Use a full cube.
		auto obsidian = maps::Blocks{}.OBSIDIAN.get();
		auto obsidian_item = obsidian ? obsidian.asItem.call() : maps::Item{};
		if (platform::jni_context::clear_exception()) return -1;
		const int slot = find_item_slot(items,obsidian_item,"obsidian");
		return slot >= 0 ? slot : find_item_slot(items,glowstone,"glowstone");
	}

	maps::BlockState state_at(const maps::ClientWorld& world, int x, int y, int z)
	{
		maps::BlockPos position = maps::BlockPos::new_object(
			&maps::BlockPos::constructor, static_cast<jint>(x), static_cast<jint>(y), static_cast<jint>(z));
		if (!position || platform::jni_context::clear_exception())
			return {};
		return world.getBlockState.call(position);
	}

	bool is_replaceable(const maps::ClientWorld& world, int x, int y, int z)
	{
		maps::BlockState state = state_at(world, x, y, z);
		// Fire is not air, but vanilla allows a placed block to replace it.
		return state && state.isReplaceable.call() == JNI_TRUE && !platform::jni_context::clear_exception();
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
		auto result = manager.interactBlock.call(player, hand, hit);
		if (!result || platform::jni_context::clear_exception() ||
			result.isAccepted.call() != JNI_TRUE || platform::jni_context::clear_exception())
			return false;
		player.swingHand.call(hand);
		return !platform::jni_context::clear_exception();
	}

	bool ray_to(const maps::ClientWorld& world, const maps::ClientPlayerEntity& player,
		const maths::vector3d& from, const maths::vector3d& to, maps::BlockHitResult& hit)
	{
		auto start = maps::Vec3d::new_object(&maps::Vec3d::constructor,from.x,from.y,from.z);
		auto end = maps::Vec3d::new_object(&maps::Vec3d::constructor,to.x,to.y,to.z);
		auto shape = maps::RaycastContext$ShapeType{}.COLLIDER.get();
		auto fluid = maps::RaycastContext$FluidHandling{}.NONE.get();
		auto ray = maps::RaycastContext::new_object(&maps::RaycastContext::constructor,start,end,shape,fluid,
			maps::Entity(player.object_instance,false));
		if (!start || !end || !shape || !fluid || !ray || platform::jni_context::clear_exception()) return false;
		hit = world.raycast.call(ray);
		auto type = hit ? maps::HitResult(hit.object_instance,false).getType.call() : maps::HitResult$Type{};
		auto block = maps::HitResult$Type{}.BLOCK.get();
		return hit && type && block && type.is_same_object(block) && !platform::jni_context::clear_exception();
	}

	bool hit_at(const maps::BlockHitResult& hit, int x, int y, int z)
	{
		auto pos = hit ? hit.getBlockPos.call() : maps::BlockPos{};
		return pos && pos.getX.call() == x && pos.getY.call() == y && pos.getZ.call() == z &&
			!platform::jni_context::clear_exception();
	}

	bool reachable_hit(const maps::ClientWorld& world, const maps::ClientPlayerEntity& player,
		int x, int y, int z, int dx, int dy, int dz, maps::BlockHitResult& hit)
	{
		const auto eye = platform::adapter::get().get_eye_position(player);
		const double range = player.getBlockInteractionRange.call();
		const maths::vector3d point{x+.5+dx*.499,y+.5+dy*.499,z+.5+dz*.499};
		const double ex = point.x-eye.x, ey = point.y-eye.y, ez = point.z-eye.z;
		if (platform::jni_context::clear_exception() || !std::isfinite(range) || range <= 0 ||
			!std::isfinite(ex+ey+ez) || ex*ex+ey*ey+ez*ez > range*range) return false;
		maps::BlockHitResult visible{};
		if (!ray_to(world,player,eye,point,visible) || !hit_at(visible,x,y,z)) return false;
		auto side = visible.getSide.call();
		if (!side || side.getOffsetX.call() != dx || side.getOffsetY.call() != dy || side.getOffsetZ.call() != dz ||
			platform::jni_context::clear_exception()) return false;
		return make_hit(x,y,z,dx,dy,dz,hit);
	}

	bool solid_cover(const maps::ClientWorld& world, int x, int y, int z)
	{
		auto state = state_at(world,x,y,z);
		if (!state || is_anchor(world,x,y,z) || platform::jni_context::clear_exception()) return false;
		if (state.isOpaque.call() == JNI_TRUE && !platform::jni_context::clear_exception()) return true;
		// Glowstone is a full collision cube even on profiles with transparent lighting.
		auto block = state.getBlock.call();
		auto item = block ? block.asItem.call() : maps::Item{};
		auto glowstone = maps::Items{}.GLOWSTONE.get();
		return item && glowstone && item.is_same_object(glowstone) && !platform::jni_context::clear_exception();
	}

	bool shield_between(const maps::ClientPlayerEntity& player, const anchor_engine::runtime& state)
	{
		const auto p = platform::adapter::get().get_position(player);
		if (!std::isfinite(p.x+p.y+p.z)) return false;
		const auto box = platform::adapter::get().get_bounding_box(player);
		if (!box || platform::jni_context::clear_exception()) return false;
		if (box.minX.get() < state.shield_x+1 && box.maxX.get() > state.shield_x &&
			box.minY.get() < state.shield_y+1 && box.maxY.get() > state.shield_y &&
			box.minZ.get() < state.shield_z+1 && box.maxZ.get() > state.shield_z) return false;
		if (platform::jni_context::clear_exception()) return false;
		const double from[]{p.x,p.y+.1,p.z}, to[]{state.anchor_x+.5,state.anchor_y+.5,state.anchor_z+.5};
		const int cell[]{state.shield_x,state.shield_y,state.shield_z};
		double enter=0, leave=1;
		for (int axis=0;axis<3;++axis) {
			const double delta=to[axis]-from[axis];
			if (std::abs(delta)<1e-9) { if (from[axis]<=cell[axis] || from[axis]>=cell[axis]+1) return false; }
			else {
				double a=(cell[axis]-from[axis])/delta, b=(cell[axis]+1-from[axis])/delta;
				if (a>b) std::swap(a,b);
				enter=std::max(enter,a); leave=std::min(leave,b);
				if (enter>=leave) return false;
			}
		}
		return enter>0 && enter<1 && leave>enter;
	}

	bool shield_ready(const maps::ClientWorld& world, const maps::ClientPlayerEntity& player,
		const anchor_engine::runtime& state)
	{
		if (!state.shield_planned || !shield_between(player,state) ||
			!solid_cover(world,state.shield_x,state.shield_y,state.shield_z)) return false;
		const auto p=platform::adapter::get().get_position(player);
		maps::BlockHitResult hit{};
		return ray_to(world,player,{p.x,p.y+.1,p.z},{state.anchor_x+.5,state.anchor_y+.5,state.anchor_z+.5},hit) &&
			hit_at(hit,state.shield_x,state.shield_y,state.shield_z);
	}

	bool plan_shield(const maps::ClientWorld& world, const maps::ClientPlayerEntity& player,
		anchor_engine::runtime& state)
	{
		const auto p=platform::adapter::get().get_position(player);
		if (!std::isfinite(p.y) || std::abs(p.y-state.anchor_y)>2) return false;
		const int floor_y=static_cast<int>(std::floor(p.y));
		constexpr int offsets[][2]{{0,-1},{0,1},{-1,0},{1,0}};
		double best=1e30; std::array<int,3> chosen{}; bool found=false;
		for (const auto& offset:offsets) for (int y:std::array{floor_y,state.anchor_y}) {
			state.shield_x=state.anchor_x+offset[0]; state.shield_y=y; state.shield_z=state.anchor_z+offset[1];
			state.shield_planned=true;
			if (!shield_between(player,state)) continue;
			if (shield_ready(world,player,state)) return true;
			maps::BlockHitResult hit{};
			if (state.shield_slot<0 || !is_replaceable(world,state.shield_x,y,state.shield_z) ||
				!solid_cover(world,state.shield_x,y-1,state.shield_z) ||
				!reachable_hit(world,player,state.shield_x,y-1,state.shield_z,0,1,0,hit)) continue;
			const double dx=p.x-(state.shield_x+.5), dz=p.z-(state.shield_z+.5), score=dx*dx+dz*dz;
			if (score<best) {best=score;chosen={state.shield_x,y,state.shield_z};found=true;}
		}
		state.shield_planned=found;
		if (found) {state.shield_x=chosen[0];state.shield_y=chosen[1];state.shield_z=chosen[2];}
		return found;
	}

	bool capture_target(const maps::MinecraftClient& instance, const maps::ClientWorld& world, anchor_engine::runtime& state)
	{
		maps::HitResult generic = instance.crosshairTarget.get();
		if (!generic || platform::jni_context::clear_exception())
			return false;
		if (!generic.is_instance_of<maps::BlockHitResult>())
			return false;
		auto type = generic.getType.call();
		auto block_type = maps::HitResult$Type{}.BLOCK.get();
		if (!type || !block_type || !type.is_same_object(block_type) || platform::jni_context::clear_exception())
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
		(void)settings;
		return state.old_slot < 0 || services::hotbar::restore_if_owned(player,manager,state.old_slot,state.owned_slot);
	}
}

bool anchor_engine::tick(config& settings, runtime& state, profile mode)
{
	if (!platform::jni_context::bind_current_thread())
		return true; // Keep enabled, try next frame
	std::lock_guard lock(state.mutex);
	jni::frame frame{ 256 };

	settings.switch_delay = std::clamp(settings.switch_delay, 0, settings.separate_delays ? 500 : 180);
	settings.action_delay = std::clamp(settings.action_delay, 0, 200);
	settings.shield_delay = std::clamp(settings.shield_delay, 0, 220);
	settings.return_delay = std::clamp(settings.return_delay, 0, settings.separate_delays ? 500 : 160);
	settings.timing_variance = std::clamp(settings.timing_variance, 0, settings.separate_delays ? 150 : 80);
	settings.place_delay = std::clamp(settings.place_delay,0,500);
	settings.charge_delay = std::clamp(settings.charge_delay,0,500);
	settings.detonate_delay = std::clamp(settings.detonate_delay,0,500);
	settings.camera_strength = std::clamp(settings.camera_strength, 0.0f, 1.2f);

	maps::MinecraftClient instance = maps::MinecraftClient{}.instance.get();
	maps::ClientWorld world = instance ? instance.world.get() : maps::ClientWorld{};
	maps::ClientPlayerEntity player = instance ? instance.player.get() : maps::ClientPlayerEntity{};
	maps::ClientPlayerInteractionManager manager = instance
		? instance.interactionManager.get()
		: maps::ClientPlayerInteractionManager{};
	const HWND window = gui::get_window();

	maps::GameOptions options = instance ? instance.options.get() : maps::GameOptions{};
	maps::KeyBinding use_key = options ? options.useKey.get() : maps::KeyBinding{};
	if (use_key && state.current != runtime::phase::idle)
	{
		use_key.setPressed.call(JNI_FALSE);
	}

	auto finish = [&]()
	{
		if (player && manager && world && state.session.matches({world,player}))
			restore_slot(player, manager, settings, state);
		if (use_key)
		{
			const bool held = services::input::physical_button_down(services::input::mouse_button::right);
			use_key.setPressed.call(held ? JNI_TRUE : JNI_FALSE);
		}
		reset_runtime(state);
		platform::jni_context::clear_exception();
		return false; // Tells the caller module to disable itself
	};

	// If game or window is not ready, do not crash or abort permanently; stay ready
	if (!instance || !world || !player || !manager || platform::jni_context::clear_exception())
	{
		const bool active = state.current != runtime::phase::idle;
		reset_runtime(state);
		return !active;
	}
	if (state.current != runtime::phase::idle && !state.session.matches({world,player}))
	{ reset_runtime(state); return false; }
	state.session.observe({world,player});

	if (!window || GetForegroundWindow() != window || gui::draw || gui::loader_visible || gui::should_unload ||
		instance.currentScreen.get() || player.isAlive.call() != JNI_TRUE || platform::jni_context::clear_exception())
	{
		return finish();
	}

	const auto now = steady_clock::now();
	if (state.sequence_started.time_since_epoch().count() != 0 && now-state.sequence_started > std::chrono::seconds(10))
	{ state.status="Stopped: sequence timed out"; return finish(); }

	auto delay = [&](int base, int maximum)
	{
		return sample_delay(base, settings.timing_variance, maximum);
	};
	auto step_delay = [&](timing_stage stage, int legacy, int maximum)
	{
		return settings.separate_delays ? sequence_delay(settings,stage,random_engine) : delay(legacy,maximum);
	};
	auto retry_delay = [&](runtime::phase waiting)
	{
		const int minimum = mode == profile::blatant ? 35 : 65;
		if (!settings.separate_delays) return minimum;
		const auto stage = waiting == runtime::phase::wait_anchor ? timing_stage::place :
			waiting == runtime::phase::wait_charge ? timing_stage::charge : timing_stage::detonate;
		return std::max(minimum,sequence_delay(settings,stage,random_engine));
	};
	auto select = [&](int slot)
	{
		if (!select_hotbar_slot(player, manager, slot, settings.hybrid_slot_input)) return false;
		state.owned_slot = slot;
		return true;
	};
	if (state.current != runtime::phase::idle && state.owned_slot >= 0)
	{
		auto inventory = player.getInventory.call();
		const int selected = services::hotbar::selected_slot(inventory);
		if (selected != state.owned_slot) return finish();
	}
	auto anchor_hit = [&]()
	{
		auto inventory = player.getInventory.call();
		auto items = inventory ? inventory.main.get() : maps::DefaultedList{};
		auto glowstone = maps::Items{}.GLOWSTONE.get();
		if (!items || !glowstone || platform::jni_context::clear_exception()) return false;
		const bool charging = state.current == runtime::phase::charge_anchor || state.current == runtime::phase::wait_charge;
		const int selected = services::hotbar::selected_slot(inventory);
		const bool holding_glowstone = stack_is_item(items,selected,glowstone,"glowstone");
		if (platform::jni_context::clear_exception() || charging != holding_glowstone) return false;
		const int charges=anchor_charges(world,state.anchor_x,state.anchor_y,state.anchor_z);
		if (charging ? charges!=0 : charges<1) return false;
		if (!charging) {
			auto offhand=player.getOffHandStack.call();
			auto item=offhand && offhand.isEmpty.call()!=JNI_TRUE ? offhand.item.get() : maps::Item{};
			if (platform::jni_context::clear_exception()) return false;
			if (item && item.is_same_object(glowstone))
			{ state.status="Remove glowstone from offhand before detonating"; return false; }
		}
		if (mode==profile::safe && !shield_ready(world,player,state))
		{ state.status="Stopped: protective block is missing or no longer between you and the anchor"; return false; }
		const maths::vector3d player_position = player.get_position();
		const double x_distance = player_position.x - (state.anchor_x + 0.5);
		const double z_distance = player_position.z - (state.anchor_z + 0.5);
		const int face_x = std::abs(x_distance) >= std::abs(z_distance)
			? (x_distance >= 0.0 ? 1 : -1)
			: 0;
		const int face_z = face_x == 0 ? (z_distance >= 0.0 ? 1 : -1) : 0;
		maps::BlockHitResult hit{};
		// The top stays reachable above a low protective block; the near side may be hidden.
		if (!reachable_hit(world,player,state.anchor_x,state.anchor_y,state.anchor_z,0,1,0,hit) &&
			!reachable_hit(world,player,state.anchor_x,state.anchor_y,state.anchor_z,face_x,0,face_z,hit)) return false;
		return interact(player, manager, hit);
	};
	auto initial_place = [&]()
	{
		auto inventory = player.getInventory.call();
		auto items = inventory ? inventory.main.get() : maps::DefaultedList{};
		auto anchor = maps::Items{}.RESPAWN_ANCHOR.get();
		if (!items || !anchor || platform::jni_context::clear_exception() ||
			!stack_is_item(items,state.anchor_slot,anchor,"anchor")) return false;
		maps::BlockHitResult hit{};
		return is_replaceable(world,state.anchor_x,state.anchor_y,state.anchor_z) &&
			reachable_hit(world,player,state.support_x, state.support_y, state.support_z,
			state.face_x, state.face_y, state.face_z, hit) && interact(player, manager, hit);
	};
	auto shield_place = [&]()
	{
		if (!state.shield_planned || !shield_between(player,state) ||
			!is_replaceable(world,state.shield_x,state.shield_y,state.shield_z) ||
			!solid_cover(world,state.shield_x,state.shield_y-1,state.shield_z)) return false;
		auto inventory=player.getInventory.call();
		auto items=inventory ? inventory.main.get() : maps::DefaultedList{};
		auto anchor=maps::Items{}.RESPAWN_ANCHOR.get(), glowstone=maps::Items{}.GLOWSTONE.get(), totem=maps::Items{}.TOTEM_OF_UNDYING.get();
		if (!items || find_shield_slot(items,anchor,glowstone,totem)!=state.shield_slot ||
			platform::jni_context::clear_exception()) return false;
		maps::BlockHitResult hit{};
		// Click the ground next to the anchor. Clicking a charged anchor detonates it.
		return reachable_hit(world,player,state.shield_x,state.shield_y-1,state.shield_z,0,1,0,hit) &&
			interact(player, manager, hit);
	};
	auto begin_confirmation = [&](runtime::phase next, int timeout)
	{
		state.current = next;
		state.phase_started = now;
		state.last_attempt = now;
		state.retries = 0;
		state.confirmation_timeout_ms = timeout;
		state.retry_interval_ms = retry_delay(next);
		state.next_action = now;
		state.status=next==runtime::phase::wait_anchor ? "Waiting for anchor placement" :
			next==runtime::phase::wait_charge ? "Waiting for anchor charge" :
			next==runtime::phase::wait_shield ? "Waiting for protective block placement" : "Waiting for anchor disappearance";
	};
	auto schedule_anchor_action = [&]()
	{
		const int charges=anchor_charges(world,state.anchor_x,state.anchor_y,state.anchor_z);
		if (charges<0) return false;
		if (mode==profile::safe && !shield_ready(world,player,state)) {
			if (state.shield_slot<0 || !state.shield_planned || !select(state.shield_slot)) return false;
			state.current=runtime::phase::place_shield;
			state.status="Placing protective block between you and the anchor";
			state.next_action=now+std::chrono::milliseconds(delay(settings.shield_delay,220));
			return true;
		}
		const bool charging=charges==0;
		if (!select(charging ? state.glowstone_slot : state.totem_slot)) return false;
		state.current=charging ? runtime::phase::charge_anchor : runtime::phase::detonate;
		state.status=charging ? "Charging protected anchor" : "Detonating protected anchor";
		state.next_action=now+std::chrono::milliseconds(charging ?
			step_delay(timing_stage::charge,settings.switch_delay,180) :
			step_delay(timing_stage::detonate,settings.action_delay,200));
		return true;
	};

	if (state.current == runtime::phase::idle)
	{
		// Macro burst check: if conditions not met, disable module immediately
		if (!movement_allowed(player, mode))
		{ state.status="Stand still on the ground before enabling"; return finish(); }

		// Try to capture target block or existing anchor
		if (!capture_target(instance, world, state))
		{ state.status="Aim at a reachable block or anchor"; return finish(); }

		const bool target_is_existing_anchor = is_anchor(world, state.anchor_x, state.anchor_y, state.anchor_z);
		if (!target_is_existing_anchor && !is_replaceable(world, state.anchor_x, state.anchor_y, state.anchor_z))
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
		if (!services::hotbar::valid_slot(state.old_slot) || platform::jni_context::clear_exception()) return finish();
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
		state.sequence_started=now;
		if (mode==profile::safe && !plan_shield(world,player,state))
		{ state.status="No protective position: need obsidian or glowstone and solid ground between you and the anchor"; return finish(); }

		const int initial_charges=target_is_existing_anchor ? anchor_charges(world,state.anchor_x,state.anchor_y,state.anchor_z) : 0;
		if (mode==profile::safe && !shield_ready(world,player,state) && initial_charges<1 && state.shield_slot==state.glowstone_slot) {
			auto object=items.get.call(static_cast<jint>(state.glowstone_slot));
			auto stack=object ? maps::ItemStack(object.object_instance,false) : maps::ItemStack{};
			if (!stack || stack.getCount.call()<2 || platform::jni_context::clear_exception())
			{ state.status="Need two glowstone: one protective block and one anchor charge"; return finish(); }
		}
		if ((initial_charges<1 && state.glowstone_slot<0) || (!target_is_existing_anchor && state.anchor_slot<0))
		{ state.status="Missing anchor or charging glowstone in hotbar"; return finish(); }

		if (target_is_existing_anchor)
		{
			if (!schedule_anchor_action()) return finish();
		}
		else
		{
			if (!select(state.anchor_slot))
				return finish();
			state.current = runtime::phase::place_anchor;
			state.next_action = now + std::chrono::milliseconds(step_delay(timing_stage::place,settings.switch_delay,180));
		}

		if (mode != profile::blatant)
			return true;
	}

	if (!movement_allowed(player, mode))
	{ state.status="Stopped: player moved before the sequence finished"; return finish(); }

	if (settings.camera_movement &&
		state.current != runtime::phase::idle && now >= state.next_camera_move)
	{
		camera_move(settings.camera_strength);
		state.next_camera_move = now + std::chrono::milliseconds(
			std::uniform_int_distribution<int>(24, 58)(random_engine));
	}

	if (now < state.next_action)
		return true;

	const int transition_budget = 1; // Even zero-delay Blatant never bursts several interactions in one frame.
	const int minimum_confirmation = mode == profile::blatant ? 0 : (mode == profile::safe ? 45 : 25);
	// Never replay an unconfirmed safe placement/charge/explosion.
	const int maximum_retries = mode == profile::safe ? 0 : mode == profile::blatant ? 4 : 3;
	const int timeout = mode == profile::blatant ? 350 : 650;

	for (int transition = 0; transition < transition_budget; ++transition)
	{
		if (now < state.next_action) return true;
		const int tick=player.age.get();
		if (platform::jni_context::clear_exception()) return finish();
		const bool acts=state.current==runtime::phase::place_anchor || state.current==runtime::phase::charge_anchor ||
			state.current==runtime::phase::place_shield || state.current==runtime::phase::detonate;
		if (acts && tick==state.last_action_tick) return true;
		if (acts) state.last_action_tick=tick;
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
				if (!schedule_anchor_action()) return finish();
				if (mode == profile::blatant) continue;
				return true;
			}
			if (state.retries < maximum_retries && elapsed(state.last_attempt, now, state.retry_interval_ms))
			{
				if (!select(state.anchor_slot) || !initial_place()) return finish();
				++state.retries;
				state.last_attempt = now;
				state.retry_interval_ms = retry_delay(state.current);
			}
			if (elapsed(state.phase_started, now, state.confirmation_timeout_ms))
			{ state.status="Stopped: anchor placement was not confirmed"; return finish(); }
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
				if (charges >= 1 &&
					elapsed(state.phase_started, now, minimum_confirmation))
				{
					if (!schedule_anchor_action()) return finish();
					if (mode == profile::blatant) continue;
					return true;
				}
				if (state.retries < maximum_retries && elapsed(state.last_attempt, now, state.retry_interval_ms))
				{
					if (!select(state.glowstone_slot) || !anchor_hit()) return finish();
					++state.retries;
					state.last_attempt = now;
					state.retry_interval_ms = retry_delay(state.current);
				}
				if (elapsed(state.phase_started, now, state.confirmation_timeout_ms))
				{ state.status="Stopped: anchor charge was not confirmed"; return finish(); }
				return true;
			}

		case runtime::phase::place_shield:
			if (!select(state.shield_slot) || !shield_place())
				return finish();
			begin_confirmation(runtime::phase::wait_shield, timeout);
			return true;

		case runtime::phase::wait_shield:
			if (shield_ready(world,player,state) &&
				elapsed(state.phase_started, now, minimum_confirmation))
			{
				if (!schedule_anchor_action()) return finish();
				return true;
			}
			if (state.retries < maximum_retries && elapsed(state.last_attempt, now, state.retry_interval_ms))
			{
				if (!select(state.shield_slot) || !shield_place()) return finish();
				++state.retries;
				state.last_attempt = now;
				state.retry_interval_ms = retry_delay(state.current);
			}
			if (elapsed(state.phase_started, now, state.confirmation_timeout_ms))
			{ state.status="Stopped: protective block was not confirmed"; return finish(); }
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
				state.next_action = now + std::chrono::milliseconds(step_delay(timing_stage::restore,settings.return_delay,160));
				if (mode == profile::blatant) continue;
				return true;
			}
			if (state.retries < maximum_retries && elapsed(state.last_attempt, now, state.retry_interval_ms))
			{
				if (!select(state.totem_slot) || !anchor_hit()) return finish();
				++state.retries;
				state.last_attempt = now;
				state.retry_interval_ms = retry_delay(state.current);
			}
			if (elapsed(state.phase_started, now, state.confirmation_timeout_ms))
			{ state.status="Stopped: anchor did not disappear after use"; return finish(); }
			return true;

		case runtime::phase::restore:
			restore_slot(player, manager, settings, state);
			state.status="Completed; enable again for another sequence";
			reset_runtime(state);
			return false; // Successful sequence completed

		default:
			return finish();
		}
	}
	return true;
}

void anchor_engine::cancel_on_client_thread(runtime& state)
{
	if (!platform::jni_context::bind_current_thread()) return;
	std::lock_guard lock(state.mutex);
	jni::frame frame{48};
	services::game::client_context context;
	if (state.old_slot >= 0 && services::game::capture_client_context(context) &&
		state.session.matches({context.world,context.player}))
	{
		services::hotbar::restore_if_owned(context.player,context.interaction,state.old_slot,state.owned_slot);
	}
	maps::MinecraftClient instance = maps::MinecraftClient{}.instance.get();
	maps::GameOptions options = instance ? instance.options.get() : maps::GameOptions{};
	maps::KeyBinding use_key = options ? options.useKey.get() : maps::KeyBinding{};
	if (use_key)
	{
		const bool held = services::input::physical_button_down(services::input::mouse_button::right);
		use_key.setPressed.call(held ? JNI_TRUE : JNI_FALSE);
	}
	platform::jni_context::clear_exception();
	reset_runtime(state);
}
