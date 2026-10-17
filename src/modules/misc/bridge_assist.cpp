#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>

#include <imgui/imgui.h>

#include "bridge_assist.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"
#include "../../mappings.hpp"

namespace
{
	using steady_clock = std::chrono::steady_clock;
bool is_holding_block(const maps::ClientPlayerEntity& player)
	{
		maps::ItemStack stack = player.getMainHandStack.call();
		if (!stack || platform::jni_context::clear_exception() ||
			stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception() ||
			stack.getCount.call() <= 0 || platform::jni_context::clear_exception())
		{
			return false;
		}

		maps::Item item = stack.item.get();
		return item && !platform::jni_context::clear_exception() && item.is_instance_of<maps::BlockItem>() &&
			!platform::jni_context::clear_exception();
	}
}

void modules::bridge_assist::release_sneak(::cache& cache)
{
	if (forced_sneak)
	{
		try
		{
			maps::GameOptions options = cache.options
				? cache.options
				: (cache.instance ? cache.instance.options.get() : maps::GameOptions{});
			maps::KeyBinding sneak_key = options ? options.sneakKey.get() : maps::KeyBinding{};
			if (sneak_key && !platform::jni_context::clear_exception())
				sneak_key.setPressed.call(sneak_state_before_force ? JNI_TRUE : JNI_FALSE);
			platform::jni_context::clear_exception();
		}
		catch (...)
		{
			platform::jni_context::clear_exception();
		}
	}

	forced_sneak = false;
	sneak_state_before_force = false;
	edge_samples = 0;
	safe_samples = 0;
	release_deadline = {};
}

void modules::bridge_assist::run(::cache& cache)
{
	const auto settings_snapshot = settings_.read();
	const HWND minecraft_window = gui::get_window();
	if (!cache.is_valid() || !cache.instance || !cache.world || !cache.player ||
		!cache.options || !minecraft_window || GetForegroundWindow() != minecraft_window ||
		gui::draw)
	{
		release_sneak(cache);
		return;
	}

	try
	{
		jni::frame frame{ 96 };
		if (cache.instance.currentScreen.get() || platform::jni_context::clear_exception() ||
			cache.player.isOnGround.call() != JNI_TRUE || platform::jni_context::clear_exception())
		{
			release_sneak(cache);
			return;
		}

		if (settings_snapshot.require_block_item && !is_holding_block(cache.player))
		{
			release_sneak(cache);
			return;
		}

		double velocity_x = 0.0;
		double velocity_z = 0.0;
		maps::Vec3d velocity = cache.player.getVelocity.call();
		if (!velocity || platform::jni_context::clear_exception())
		{
			release_sneak(cache);
			return;
		}
		velocity_x = velocity.x.get();
		velocity_z = velocity.z.get();
		const bool moving = !platform::jni_context::clear_exception() &&
			velocity_x * velocity_x + velocity_z * velocity_z >= 0.0004;
		if (platform::jni_context::clear_exception() || (settings_snapshot.only_while_moving && !moving && !forced_sneak))
		{
			release_sneak(cache);
			return;
		}

		maps::Box bounds = cache.player.get_bounding_box();
		if (!bounds || platform::jni_context::clear_exception())
		{
			release_sneak(cache);
			return;
		}

		const double min_x = bounds.minX.get();
		const double min_y = bounds.minY.get();
		const double min_z = bounds.minZ.get();
		const double max_x = bounds.maxX.get();
		const double max_z = bounds.maxZ.get();
		if (platform::jni_context::clear_exception())
		{
			release_sneak(cache);
			return;
		}

		const double center_x = (min_x + max_x) * 0.5;
		const double center_z = (min_z + max_z) * 0.5;
		const double half_x = std::max(0.025, (max_x - min_x) * 0.5 -
			static_cast<double>(std::clamp(settings_snapshot.edge_offset, 0.0f, 0.27f)));
		const double half_z = std::max(0.025, (max_z - min_z) * 0.5 -
			static_cast<double>(std::clamp(settings_snapshot.edge_offset, 0.0f, 0.27f)));
		const int support_y = static_cast<int>(std::floor(min_y - 0.05));
		const auto unsupported_corners = [&](double sample_center_x, double sample_center_z)
		{
			const double sample_x[2] = { sample_center_x - half_x, sample_center_x + half_x };
			const double sample_z[2] = { sample_center_z - half_z, sample_center_z + half_z };
			int unsupported = 0;
			for (double x : sample_x)
			{
				for (double z : sample_z)
				{
					maps::BlockPos position = maps::BlockPos::new_object(
						&maps::BlockPos::constructor,
						static_cast<jint>(std::floor(x)),
						static_cast<jint>(support_y),
						static_cast<jint>(std::floor(z)));
					maps::BlockState state = position
						? cache.world.getBlockState.call(position)
						: maps::BlockState{};
					if (!position || !state || platform::jni_context::clear_exception())
						return -1;
					if (state.isAir.call() == JNI_TRUE && !platform::jni_context::clear_exception())
						++unsupported;
					else if (platform::jni_context::clear_exception())
						return -1;
				}
			}
			return unsupported;
		};

		const int current_unsupported = unsupported_corners(center_x, center_z);
		// A short directional look-ahead catches fast backward/diagonal bridging
		// before the input tick can carry the hitbox beyond the safe edge.
		const int predicted_unsupported = unsupported_corners(
			center_x + velocity_x * 0.35,
			center_z + velocity_z * 0.35);
		if (current_unsupported < 0 || predicted_unsupported < 0)
		{
			release_sneak(cache);
			return;
		}

		const bool raw_edge = current_unsupported > 0 || predicted_unsupported > 0;
		if (raw_edge)
		{
			edge_samples = std::min(edge_samples + 1, 4);
			safe_samples = 0;
		}
		else
		{
			edge_samples = 0;
			safe_samples = std::min(safe_samples + 1, 5);
		}
		const bool confirmed_edge = raw_edge && (forced_sneak || edge_samples >= 2);

		const steady_clock::time_point now = steady_clock::now();
		if (confirmed_edge)
		{
			release_deadline = now + std::chrono::milliseconds(
				std::clamp(settings_snapshot.unsneak_delay, 0, 300));
			maps::KeyBinding sneak_key = cache.options.sneakKey.get();
			if (!sneak_key || platform::jni_context::clear_exception())
			{
				release_sneak(cache);
				return;
			}

			if (!forced_sneak)
			{
				sneak_state_before_force = sneak_key.isPressed.call() == JNI_TRUE;
				if (platform::jni_context::clear_exception())
				{
					release_sneak(cache);
					return;
				}
				// Manual sneak already protects the edge; do not take ownership of it.
				if (sneak_state_before_force)
					return;
				forced_sneak = true;
			}

			sneak_key.setPressed.call(JNI_TRUE);
			platform::jni_context::clear_exception();
			return;
		}

		if (forced_sneak && (now < release_deadline || safe_samples < 3))
		{
			maps::KeyBinding sneak_key = cache.options.sneakKey.get();
			if (sneak_key && !platform::jni_context::clear_exception())
				sneak_key.setPressed.call(JNI_TRUE);
			platform::jni_context::clear_exception();
			return;
		}

		if (raw_edge && !forced_sneak)
			return;

		release_sneak(cache);
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
		release_sneak(cache);
	}
}

void modules::bridge_assist::on_disable(::cache& cache)
{
	release_sneak(cache);
}

void modules::bridge_assist::render_options()
{
	gui::settings::render(settings_, module_settings::bridge_assist::schema());
	ImGui::TextDisabled("%s", "Sneaks at unsupported hitbox corners; placement stays manual.");
}
