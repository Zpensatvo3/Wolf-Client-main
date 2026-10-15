#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <algorithm>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

#include <imgui/imgui.h>

#include "trajectories.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../ui_redirect.hpp"
#include "../gui/gui.hpp"

namespace
{
	constexpr double pi = 3.14159265358979323846;
	constexpr double near_plane = 0.05;

	struct trajectory_point
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
	};

	struct trajectory_snapshot
	{
		std::vector<trajectory_point> points;
		bool hit = false;
		int flight_ticks = 0;
		double distance = 0.0;
		float aim_yaw = 0.0f;
		float aim_pitch = 0.0f;
		std::string item_name;
	};

	struct projectile_profile
	{
		double speed = 0.0;
		double gravity = 0.0;
		double drag = 0.99;
		double pitch_offset = 0.0;
		bool valid = false;
		bool needs_charge = false;
		bool bow = false;
		bool crossbow = false;
		const char* name = "";
	};

	struct view_point
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
	};

	std::mutex snapshot_mutex;
	trajectory_snapshot snapshot;
	std::chrono::steady_clock::time_point next_update{};

	std::atomic<bool> camera_valid{ false };
	std::atomic<double> camera_x{ 0.0 };
	std::atomic<double> camera_y{ 0.0 };
	std::atomic<double> camera_z{ 0.0 };
	std::atomic<float> camera_yaw{ 0.0f };
	std::atomic<float> camera_pitch{ 0.0f };
	std::atomic<float> camera_fov{ 70.0f };
	std::atomic<bool> camera_third_person{ false };
void clear_snapshot()
	{
		std::lock_guard lock(snapshot_mutex);
		snapshot = {};
	}

	bool same_item(const maps::Item& held, const maps::Item& wanted)
	{
		return held && wanted && held.is_same_object(wanted);
	}

	bool update_camera_state(const ::cache& cache)
	{
		try
		{
			float tick_delta = platform::adapter::get().get_tick_delta(cache.instance);
			auto cam = platform::adapter::get().get_camera_data(cache.instance, cache.player, tick_delta);
			if (!cam.valid)
			{
				camera_valid.store(false, std::memory_order_release);
				return false;
			}

			camera_x.store(cam.pos.x, std::memory_order_relaxed);
			camera_y.store(cam.pos.y, std::memory_order_relaxed);
			camera_z.store(cam.pos.z, std::memory_order_relaxed);
			camera_yaw.store(cam.yaw, std::memory_order_relaxed);
			camera_pitch.store(cam.pitch, std::memory_order_relaxed);
			camera_fov.store(cam.fov, std::memory_order_relaxed);

			bool is_third_person = false;
			try
			{
				maps::EntityRenderDispatcher dispatcher = cache.instance.entityRenderDispatcher.get();
				maps::Camera camera = dispatcher ? dispatcher.camera.get() : maps::Camera{};
				if (camera && camera.isThirdPerson.is_valid())
					is_third_person = camera.isThirdPerson.call() == JNI_TRUE;
			}
			catch (...)
			{
				platform::jni_context::clear_exception();
			}

			camera_third_person.store(is_third_person, std::memory_order_relaxed);
			camera_valid.store(true, std::memory_order_release);
			return true;
		}
		catch (...)
		{
			platform::jni_context::clear_exception();
			camera_valid.store(false, std::memory_order_release);
			return false;
		}
	}

	projectile_profile profile_for_item(const maps::ItemStack& stack,
		const maps::Items& items, bool using_item)
	{
		projectile_profile profile{};
		if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
			return profile;

		maps::Item held = stack.item.get();
		if (!held || platform::jni_context::clear_exception())
			return profile;

		if (same_item(held, items.BOW.get()))
			return { 3.0, 0.05, 0.99, 0.0, true, true, true, false, "Bow" };

		if (same_item(held, items.CROSSBOW.get()))
		{
			maps::ComponentType type = maps::DataComponentTypes{}.CHARGED_PROJECTILES.get();
			maps::Object value = type ? stack.getComponent.call(type) : maps::Object{};
			maps::ChargedProjectilesComponent charged(value.object_instance, false);
			if (!charged || platform::jni_context::clear_exception() || charged.isEmpty.call() == JNI_TRUE ||
				platform::jni_context::clear_exception())
				return profile;

			maps::Item rocket = items.FIREWORK_ROCKET.get();
			if (rocket && charged.contains.call(rocket) == JNI_TRUE && !platform::jni_context::clear_exception())
				return profile;
			platform::jni_context::clear_exception();
			return { 3.15, 0.05, 0.99, 0.0, true, false, false, true, "Crossbow" };
		}

		if (same_item(held, items.TRIDENT.get()))
			return { 2.5, 0.05, 0.99, 0.0, true, true, false, false, "Trident" };
		if (same_item(held, items.ENDER_PEARL.get()))
			return { 1.5, 0.03, 0.99, 0.0, true, false, false, false, "Ender Pearl" };
		if (same_item(held, items.SNOWBALL.get()))
			return { 1.5, 0.03, 0.99, 0.0, true, false, false, false, "Snowball" };
		if (same_item(held, items.EGG.get()))
			return { 1.5, 0.03, 0.99, 0.0, true, false, false, false, "Egg" };
		if (same_item(held, items.SPLASH_POTION.get()))
			return { 0.5, 0.05, 0.99, -20.0, true, false, false, false, "Splash Potion" };
		if (same_item(held, items.LINGERING_POTION.get()))
			return { 0.5, 0.05, 0.99, -20.0, true, false, false, false, "Lingering Potion" };
		if (same_item(held, items.EXPERIENCE_BOTTLE.get()))
			return { 0.7, 0.07, 0.99, -20.0, true, false, false, false, "Experience Bottle" };

		return profile;
	}

	bool block_raycast(const maps::ClientWorld& world, const maps::ClientPlayerEntity& player,
		const trajectory_point& from, const trajectory_point& to, trajectory_point& impact)
	{
		try
		{
			jni::frame segment_frame{ 24 };
			maps::Vec3d start = maps::Vec3d::new_object(&maps::Vec3d::constructor,
				from.x, from.y, from.z);
			maps::Vec3d end = maps::Vec3d::new_object(&maps::Vec3d::constructor,
				to.x, to.y, to.z);
			maps::RaycastContext$ShapeType shape =
				maps::RaycastContext$ShapeType{}.COLLIDER.get();
			maps::RaycastContext$FluidHandling fluid =
				maps::RaycastContext$FluidHandling{}.NONE.get();
			if (!start || !end || !shape || !fluid || platform::jni_context::clear_exception())
				return false;

			maps::RaycastContext context = maps::RaycastContext::new_object(
				&maps::RaycastContext::constructor, start, end, shape, fluid,
				maps::Entity(player.object_instance, false));
			maps::BlockHitResult hit = context ? world.raycast.call(context) : maps::BlockHitResult{};
			if (!hit || platform::jni_context::clear_exception())
				return false;

			maps::HitResult$Type type = maps::HitResult(hit.object_instance, false).getType.call();
			maps::HitResult$Type block = maps::HitResult$Type{}.BLOCK.get();
			if (!type || !block || platform::jni_context::clear_exception() || !type.is_same_object(block))
				return false;

			maps::Vec3d position = maps::HitResult(hit.object_instance, false).getPos.call();
			if (!position || platform::jni_context::clear_exception())
				return false;
			impact = { position.x.get(), position.y.get(), position.z.get() };
			return !platform::jni_context::clear_exception();
		}
		catch (...)
		{
			platform::jni_context::clear_exception();
			return false;
		}
	}

	view_point to_view(const trajectory_point& point, double cx, double cy, double cz,
		float yaw, float pitch)
	{
		const double relative_x = point.x - cx;
		const double relative_y = point.y - cy;
		const double relative_z = point.z - cz;
		const double yaw_radians = static_cast<double>(yaw) * pi / 180.0;
		const double pitch_radians = static_cast<double>(pitch) * pi / 180.0;
		const double yaw_sine = std::sin(yaw_radians);
		const double yaw_cosine = std::cos(yaw_radians);
		const double pitch_sine = std::sin(pitch_radians);
		const double pitch_cosine = std::cos(pitch_radians);

		const double forward_x = -yaw_sine * pitch_cosine;
		const double forward_y = -pitch_sine;
		const double forward_z = yaw_cosine * pitch_cosine;
		const double right_x = yaw_cosine;
		const double right_z = yaw_sine;
		const double up_x = -yaw_sine * pitch_sine;
		const double up_y = pitch_cosine;
		const double up_z = yaw_cosine * pitch_sine;

		return {
			relative_x * -right_x + relative_z * -right_z,
			relative_x * up_x + relative_y * up_y + relative_z * up_z,
			relative_x * forward_x + relative_y * forward_y + relative_z * forward_z
		};
	}

	bool clip_to_near_plane(view_point& from, view_point& to)
	{
		const bool from_inside = from.z >= near_plane;
		const bool to_inside = to.z >= near_plane;
		if (!from_inside && !to_inside)
			return false;
		if (from_inside && to_inside)
			return true;

		view_point& outside = from_inside ? to : from;
		const view_point& inside = from_inside ? from : to;
		const double denominator = outside.z - inside.z;
		if (std::abs(denominator) < 1.0e-9)
			return false;
		const double amount = (near_plane - inside.z) / denominator;
		outside.x = inside.x + (outside.x - inside.x) * amount;
		outside.y = inside.y + (outside.y - inside.y) * amount;
		outside.z = near_plane;
		return true;
	}

	bool project(const view_point& point, float width, float height, float fov,
		ImVec2& screen)
	{
		if (point.z < near_plane)
			return false;
		const double focal = (height * 0.5) /
			std::tan(static_cast<double>(fov) * pi / 360.0);
		screen.x = static_cast<float>(width * 0.5 + point.x * focal / point.z);
		screen.y = static_cast<float>(height * 0.5 - point.y * focal / point.z);
		screen.x = std::clamp(screen.x, -width * 4.0f, width * 5.0f);
		screen.y = std::clamp(screen.y, -height * 4.0f, height * 5.0f);
		return std::isfinite(screen.x) && std::isfinite(screen.y);
	}
}

void modules::trajectories::run(::cache& cache)
{
	if (!cache.is_valid() || !cache.instance || !cache.world || !cache.player ||
		!jni::get_env())
	{
		clear_snapshot();
		camera_valid.store(false, std::memory_order_release);
		return;
	}

	const auto now = std::chrono::steady_clock::now();
	if (now < next_update)
		return;
	next_update = now + std::chrono::milliseconds(40);

	try
	{
		jni::frame frame{ 256 };
		update_camera_state(cache);

		const bool using_item = cache.player.isUsingItem.call() == JNI_TRUE &&
			!platform::jni_context::clear_exception();
		maps::Items items{};
		maps::ItemStack active = using_item ? cache.player.getActiveItem.call() : maps::ItemStack{};
		maps::ItemStack main = cache.player.getMainHandStack.call();
		maps::ItemStack offhand = cache.player.getOffHandStack.call();
		if (platform::jni_context::clear_exception())
		{
			clear_snapshot();
			return;
		}

		projectile_profile profile = profile_for_item(active, items, using_item);
		maps::ItemStack selected = active;
		if (!profile.valid)
		{
			profile = profile_for_item(main, items, using_item);
			selected = main;
		}
		if (!profile.valid)
		{
			profile = profile_for_item(offhand, items, using_item);
			selected = offhand;
		}
		if (!profile.valid ||
			((profile.bow || profile.crossbow || std::strcmp(profile.name, "Trident") == 0) &&
				!show_bows) ||
			((std::string(profile.name).find("Potion") != std::string::npos ||
				std::string(profile.name).find("Bottle") != std::string::npos) && !show_potions) ||
			(!profile.bow && !profile.crossbow && std::strcmp(profile.name, "Trident") != 0 &&
				std::string(profile.name).find("Potion") == std::string::npos &&
				std::string(profile.name).find("Bottle") == std::string::npos && !show_throwables))
		{
			clear_snapshot();
			return;
		}

		int charge_ticks = 0;
		if (profile.needs_charge)
		{
			if (!using_item || !active || !selected || !active.is_same_object(selected))
			{
				clear_snapshot();
				return;
			}
			charge_ticks = cache.player.getItemUseTime.call();
			if (platform::jni_context::clear_exception())
			{
				clear_snapshot();
				return;
			}
			if (profile.bow)
			{
				const double pull = static_cast<double>(charge_ticks) / 20.0;
				const double progress = std::clamp((pull * pull + pull * 2.0) / 3.0, 0.0, 1.0);
				if (progress < 0.1)
				{
					clear_snapshot();
					return;
				}
				profile.speed *= progress;
			}
			else if (charge_ticks < 10)
			{
				clear_snapshot();
				return;
			}
		}

		const maths::vector3d player_position = cache.player.get_position();
		maps::EntityPose pose = cache.player.getPose.call();
		const double eye_height = pose
			? static_cast<double>(cache.player.getEyeHeight.call(pose))
			: 1.62;
		platform::jni_context::clear_exception();
		const maths::angles angles = cache.player.get_angles();
		const double yaw = static_cast<double>(angles.yaw) * pi / 180.0;
		const double pitch = (static_cast<double>(angles.pitch) + profile.pitch_offset) * pi / 180.0;
		const double cosine_pitch = std::cos(pitch);

		trajectory_point position{
			player_position.x,
			player_position.y + (std::isfinite(eye_height) ? eye_height : 1.62) - 0.10,
			player_position.z
		};
		trajectory_point velocity{
			-std::sin(yaw) * cosine_pitch * profile.speed,
			-std::sin(pitch) * profile.speed,
			 std::cos(yaw) * cosine_pitch * profile.speed
		};

		maps::Vec3d player_velocity = cache.player.getVelocity.call();
		if (player_velocity && !platform::jni_context::clear_exception())
		{
			velocity.x += player_velocity.x.get();
			velocity.z += player_velocity.z.get();
			if (cache.player.isOnGround.call() != JNI_TRUE && !platform::jni_context::clear_exception())
				velocity.y += player_velocity.y.get();
			platform::jni_context::clear_exception();
		}

		trajectory_snapshot updated;
		updated.points.reserve(static_cast<std::size_t>(max_ticks) + 1);
		updated.points.push_back(position);
		updated.item_name = profile.name;
		updated.aim_yaw = angles.yaw;
		updated.aim_pitch = angles.pitch;
		const trajectory_point origin = position;

		const int simulation_ticks = std::clamp(max_ticks, 40, 240);
		for (int tick = 1; tick <= simulation_ticks; ++tick)
		{
			trajectory_point next{
				position.x + velocity.x,
				position.y + velocity.y,
				position.z + velocity.z
			};

			trajectory_point impact{};
			if (block_raycast(cache.world, cache.player, position, next, impact))
			{
				updated.points.push_back(impact);
				updated.hit = true;
				updated.flight_ticks = tick;
				position = impact;
				break;
			}

			updated.points.push_back(next);
			position = next;
			velocity.x *= profile.drag;
			velocity.y = velocity.y * profile.drag - profile.gravity;
			velocity.z *= profile.drag;
			updated.flight_ticks = tick;

			if (position.y < -96.0)
				break;
		}

		const double dx = position.x - origin.x;
		const double dy = position.y - origin.y;
		const double dz = position.z - origin.z;
		updated.distance = std::sqrt(dx * dx + dy * dy + dz * dz);

		std::lock_guard lock(snapshot_mutex);
		snapshot = std::move(updated);
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		clear_snapshot();
	}
}

void modules::trajectories::render_overlay()
{
	if (platform::jni_context::bind_current_thread())
	{
		jni::frame frame{ 48 };
		maps::MinecraftClient instance = maps::MinecraftClient{}.instance.get();
		if (instance)
		{
			maps::ClientPlayerEntity player = instance.player.get();
			auto& adapter = platform::adapter::get();
			const float tick_delta = adapter.get_tick_delta(instance);
			auto cam = adapter.get_camera_data(instance, player, tick_delta);
			if (cam.valid)
			{
				camera_x.store(cam.pos.x, std::memory_order_relaxed);
				camera_y.store(cam.pos.y, std::memory_order_relaxed);
				camera_z.store(cam.pos.z, std::memory_order_relaxed);
				camera_yaw.store(cam.yaw, std::memory_order_relaxed);
				camera_pitch.store(cam.pitch, std::memory_order_relaxed);
				camera_fov.store(cam.fov, std::memory_order_relaxed);
				camera_valid.store(true, std::memory_order_release);
			}
		}
	}

	if (!camera_valid.load(std::memory_order_acquire))
		return;

	trajectory_snapshot current;
	{
		std::lock_guard lock(snapshot_mutex);
		current = snapshot;
	}
	if (current.points.size() < 2)
		return;

	const ImVec2 display = ImGui::GetIO().DisplaySize;
	if (display.x < 10.0f || display.y < 10.0f)
		return;
	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	if (!draw)
		return;

	const double cx = camera_x.load(std::memory_order_relaxed);
	const double cy = camera_y.load(std::memory_order_relaxed);
	const double cz = camera_z.load(std::memory_order_relaxed);
	const float yaw = camera_yaw.load(std::memory_order_relaxed);
	const float pitch = camera_pitch.load(std::memory_order_relaxed);
	const float fov = camera_fov.load(std::memory_order_relaxed);
	const bool third_person = camera_third_person.load(std::memory_order_relaxed);
	const ImColor accent = gui::get_dynamic_color();
	const ImU32 line_color = ImGui::ColorConvertFloat4ToU32(ImVec4(
		accent.Value.x, accent.Value.y, accent.Value.z, 0.96f));
	const float thickness = std::clamp(line_thickness, 1.0f, 5.0f);

	auto draw_screen_segment = [&](const ImVec2& from, const ImVec2& to)
	{
		draw->AddLine(from, to, IM_COL32(0, 0, 0, 175), thickness + 2.0f);
		draw->AddLine(from, to, line_color, thickness);
	};

	auto angle_delta = [](float first, float second)
	{
		float delta = std::fmod(first - second, 360.0f);
		if (delta > 180.0f) delta -= 360.0f;
		if (delta < -180.0f) delta += 360.0f;
		return std::abs(delta);
	};

	// In normal first person the simulated position comes from the 20 TPS
	// entity state, while the camera is render-interpolated. Drawing that tiny
	// world-space offset near the near plane magnifies it into a distracting
	// jump. Anchor only the close portion to the crosshair. Freelook keeps the
	// world-space path because its camera intentionally differs from player aim.
	const bool crosshair_view = !third_person &&
		angle_delta(yaw, current.aim_yaw) < 1.0f &&
		angle_delta(pitch, current.aim_pitch) < 1.0f;
	std::size_t first_world_segment = 1;
	if (crosshair_view)
	{
		std::size_t anchor_index = 1;
		for (; anchor_index < current.points.size(); ++anchor_index)
		{
			const double dx = current.points[anchor_index].x - cx;
			const double dy = current.points[anchor_index].y - cy;
			const double dz = current.points[anchor_index].z - cz;
			if (dx * dx + dy * dy + dz * dz >= 0.85 * 0.85)
				break;
		}
		if (anchor_index < current.points.size())
		{
			const view_point anchor_view = to_view(current.points[anchor_index],
				cx, cy, cz, yaw, pitch);
			ImVec2 anchor_screen{};
			if (project(anchor_view, display.x, display.y, fov, anchor_screen))
				draw_screen_segment(ImVec2(display.x * 0.5f, display.y * 0.5f),
					anchor_screen);
			first_world_segment = anchor_index + 1;
		}
	}

	for (std::size_t index = first_world_segment; index < current.points.size(); ++index)
	{
		view_point from = to_view(current.points[index - 1], cx, cy, cz, yaw, pitch);
		view_point to = to_view(current.points[index], cx, cy, cz, yaw, pitch);
		if (!clip_to_near_plane(from, to))
			continue;

		ImVec2 screen_from{};
		ImVec2 screen_to{};
		if (!project(from, display.x, display.y, fov, screen_from) ||
			!project(to, display.x, display.y, fov, screen_to))
			continue;
		draw_screen_segment(screen_from, screen_to);
	}

	if (!current.hit || !show_landing_marker)
		return;

	const view_point landing_view = to_view(current.points.back(), cx, cy, cz, yaw, pitch);
	ImVec2 landing{};
	if (!project(landing_view, display.x, display.y, fov, landing))
		return;

	draw->AddCircleFilled(landing, 6.5f, IM_COL32(5, 6, 9, 205), 24);
	draw->AddCircle(landing, 6.5f, line_color, 24, 2.0f);
	draw->AddLine(ImVec2(landing.x - 3.5f, landing.y),
		ImVec2(landing.x + 3.5f, landing.y), line_color, 1.4f);
	draw->AddLine(ImVec2(landing.x, landing.y - 3.5f),
		ImVec2(landing.x, landing.y + 3.5f), line_color, 1.4f);

	if (show_flight_time)
	{
		char label[96]{};
		sprintf_s(label, "%s  %.2fs  %.1fm", current.item_name.c_str(),
			static_cast<double>(current.flight_ticks) / 20.0, current.distance);
		const ImVec2 text_size = ImGui::CalcTextSize(label);
		const ImVec2 text_position(landing.x - text_size.x * 0.5f, landing.y + 10.0f);
		draw->AddRectFilled(ImVec2(text_position.x - 5.0f, text_position.y - 3.0f),
			ImVec2(text_position.x + text_size.x + 5.0f, text_position.y + text_size.y + 3.0f),
			IM_COL32(8, 9, 13, 205), 4.0f);
		draw->AddText(ImVec2(text_position.x + 1.0f, text_position.y + 1.0f),
			IM_COL32(0, 0, 0, 210), label);
		draw->AddText(text_position, IM_COL32(238, 239, 246, 245), label);
	}
}

void modules::trajectories::render_options()
{
	ImGui::Checkbox(gui::text("Bows & Trident", "Yaylar ve Trident"), &show_bows);
	ImGui::Checkbox(gui::text("Throwables", "Fırlatılabilir Eşyalar"), &show_throwables);
	ImGui::Checkbox(gui::text("Potions & XP", "Potion ve XP"), &show_potions);
	ImGui::Checkbox(gui::text("Impact Marker", "Çarpma İşareti"), &show_landing_marker);
	ImGui::Checkbox(gui::text("Flight Info", "Uçuş Bilgisi"), &show_flight_time);
	ImGui::SetNextItemWidth(210.0f);
	ImGui::SliderFloat(gui::text("Line Thickness", "Çizgi Kalınlığı"),
		&line_thickness, 1.0f, 5.0f, "%.1f px");
	ImGui::SetNextItemWidth(210.0f);
	ImGui::SliderInt(gui::text("Prediction Time", "Tahmin Süresi"),
		&max_ticks, 40, 240, "%d ticks");
}

void modules::trajectories::on_disable(::cache& cache)
{
	clear_snapshot();
	camera_valid.store(false, std::memory_order_release);
	next_update = {};
}
