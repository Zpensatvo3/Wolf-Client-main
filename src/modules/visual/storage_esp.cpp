#include "storage_esp.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../ui_redirect.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include <imgui/imgui.h>

#include "../gui/gui.hpp"


namespace
{
	struct scan_result
	{
		double x = 0.0, y = 0.0, z = 0.0;
		std::string label;
	};

	bool camera_snapshot(double& x, double& y, double& z,
		float& yaw, float& pitch, float& fov)
	{
		maps::MinecraftClient instance = maps::MinecraftClient{}.instance.get();
		if (!instance) return false;
		maps::ClientPlayerEntity player = instance.player.get();
		auto& adapter = platform::adapter::get();
		float tick_delta = adapter.get_tick_delta(instance);
		auto cam = adapter.get_camera_data(instance, player, tick_delta);
		if (!cam.valid) return false;
		x = cam.pos.x;
		y = cam.pos.y;
		z = cam.pos.z;
		yaw = cam.yaw;
		pitch = cam.pitch;
		fov = cam.fov;
		return true;
	}

	bool project(double wx, double wy, double wz, double cx, double cy, double cz,
		float yaw, float pitch, float width, float height, float fov, float& sx, float& sy)
	{
		constexpr double pi = 3.14159265358979323846;
		const double rx = wx - cx, ry = wy - cy, rz = wz - cz;
		const double yaw_r = yaw * pi / 180.0, pitch_r = pitch * pi / 180.0;
		const double syaw = std::sin(yaw_r), cyaw = std::cos(yaw_r);
		const double sp = std::sin(pitch_r), cp = std::cos(pitch_r);
		const double vx = rx * -cyaw + rz * -syaw;
		const double vy = rx * -syaw * sp + ry * cp + rz * cyaw * sp;
		const double vz = rx * -syaw * cp - ry * sp + rz * cyaw * cp;
		if (vz < 0.08) return false;
		const double focal = (height * .5) / std::tan(std::clamp(fov, 1.0f, 179.0f) * pi / 360.0);
		sx = static_cast<float>(width * .5 + vx * focal / vz);
		sy = static_cast<float>(height * .5 - vy * focal / vz);
		return std::isfinite(sx) && std::isfinite(sy);
	}

	const char* storage_label(const std::string& key)
	{
		if (key.find("ender_chest") != std::string::npos) return "Ender Chest";
		if (key.find("trapped_chest") != std::string::npos) return "Trapped Chest";
		if (key.find("shulker_box") != std::string::npos) return "Shulker Box";
		if (key.find("barrel") != std::string::npos) return "Barrel";
		return "Chest";
	}

	bool is_storage_key(const std::string& key)
	{
		return key.find("trapped_chest") != std::string::npos ||
			key.find("ender_chest") != std::string::npos ||
			key.find("shulker_box") != std::string::npos ||
			key.find("barrel") != std::string::npos ||
			key.ends_with(".chest") || key.ends_with("_chest");
	}

	ImU32 storage_color(const std::string& label)
	{
		if (label == "Ender Chest") return IM_COL32(166, 104, 255, 245);
		if (label == "Trapped Chest") return IM_COL32(255, 102, 105, 245);
		if (label == "Shulker Box") return IM_COL32(218, 116, 255, 245);
		if (label == "Barrel") return IM_COL32(235, 174, 92, 245);
		return IM_COL32(255, 194, 92, 245);
	}
}

void modules::storage_esp::run(::cache& cache)
{
	if (!cache.is_valid() || !cache.player || !cache.world || !jni::get_env()) return;
	try
	{
		jni::frame frame{ 768 };
		const maths::vector3d player_pos = cache.player.get_position();
		const int px = static_cast<int>(std::floor(player_pos.x));
		const int py = static_cast<int>(std::floor(player_pos.y));
		const int pz = static_cast<int>(std::floor(player_pos.z));
		const int horizontal = std::clamp(range, 4, 24);
		const int vertical_radius = std::min(horizontal, 12);
		const int side = horizontal * 2 + 1;
		const int vertical = vertical_radius * 2 + 1;
		const int total = side * side * vertical;
		{
			std::scoped_lock lock(marker_mutex);
			if (px != origin_x || py != origin_y || pz != origin_z)
			{
				origin_x = px; origin_y = py; origin_z = pz; scan_index = 0;
			}
		}

		const int start = scan_index;
		const int end = std::min(total, start + 160);
		std::vector<scan_result> scanned;
		scanned.reserve(static_cast<std::size_t>(end - start));
		for (int index = start; index < end; ++index)
		{
			const int x_offset = index % side - horizontal;
			const int temporary = index / side;
			const int z_offset = temporary % side - horizontal;
			const int y_offset = temporary / side - vertical_radius;
			const int x = px + x_offset, y = py + y_offset, z = pz + z_offset;
			scan_result result{ static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), {} };
			maps::BlockPos pos = maps::BlockPos::new_object(&maps::BlockPos::constructor,
				static_cast<jint>(x), static_cast<jint>(y), static_cast<jint>(z));
			maps::BlockState state = pos ? cache.world.getBlockState.call(pos) : maps::BlockState{};
			if (state && !platform::jni_context::clear_exception() && state.isAir.call() != JNI_TRUE && !platform::jni_context::clear_exception())
			{
				maps::Block block = state.getBlock.call();
				maps::Item item = block ? block.asItem.call() : maps::Item{};
				maps::String name = item && !platform::jni_context::clear_exception() ? item.getTranslationKey.call() : maps::String{};
				const std::string key = name && !platform::jni_context::clear_exception() ? name.to_string() : std::string{};
				platform::jni_context::clear_exception();
				if (is_storage_key(key)) result.label = storage_label(key);
			}
			scanned.push_back(std::move(result));
		}

		std::scoped_lock lock(marker_mutex);
		for (const scan_result& cell : scanned)
		{
			markers.erase(std::remove_if(markers.begin(), markers.end(), [&](const marker& old)
			{
				return old.x == cell.x && old.y == cell.y && old.z == cell.z;
			}), markers.end());
			if (!cell.label.empty()) markers.push_back({ cell.x, cell.y, cell.z, cell.label });
		}
		markers.erase(std::remove_if(markers.begin(), markers.end(), [&](const marker& old)
		{
			return std::abs(old.x - px) > horizontal || std::abs(old.y - py) > vertical_radius ||
				std::abs(old.z - pz) > horizontal;
		}), markers.end());
		scan_index = end >= total ? 0 : end;
	}
	catch (...) { platform::jni_context::clear_exception(); }
}

void modules::storage_esp::render_overlay()
{
	if (!platform::jni_context::bind_current_thread()) return;
	double cx = 0.0, cy = 0.0, cz = 0.0;
	float yaw = 0.0f, pitch = 0.0f, fov = 70.0f;
	{
		jni::frame frame{ 48 };
		if (!camera_snapshot(cx, cy, cz, yaw, pitch, fov)) return;
	}
	std::vector<marker> draw_markers;
	{
		std::scoped_lock lock(marker_mutex);
		draw_markers = markers;
	}
	if (draw_markers.empty()) return;

	const ImVec2 display = ImGui::GetIO().DisplaySize;
	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	for (const marker& entry : draw_markers)
	{
		float corners_x[8]{}, corners_y[8]{};
		bool corner_valid[8]{};
		int projected = 0;
		float min_x = 1.0e9f, min_y = 1.0e9f, max_x = -1.0e9f, max_y = -1.0e9f;
		for (int x = 0; x <= 1; ++x) for (int y = 0; y <= 1; ++y) for (int z = 0; z <= 1; ++z)
		{
			const int corner = x * 4 + y * 2 + z;
			float sx = 0.0f, sy = 0.0f;
			if (!project(entry.x + x, entry.y + y, entry.z + z,
				cx, cy, cz, yaw, pitch, display.x, display.y, fov, sx, sy)) continue;
			min_x = std::min(min_x, sx); min_y = std::min(min_y, sy);
			max_x = std::max(max_x, sx); max_y = std::max(max_y, sy);
			corners_x[corner] = sx; corners_y[corner] = sy; corner_valid[corner] = true; ++projected;
		}
		if (projected < 4 || max_x < 0.0f || min_x > display.x || max_y < 0.0f || min_y > display.y)
			continue;

		const ImU32 color = storage_color(entry.label);
		if (box_style == 0)
		{
			draw->AddRectFilled(ImVec2(min_x, min_y), ImVec2(max_x, max_y),
				(color & 0x00FFFFFFu) | 0x1E000000u, 2.0f);
			draw->AddRect(ImVec2(min_x, min_y), ImVec2(max_x, max_y), color, 2.0f, 0, 1.4f);
		}
		else
		{
			static constexpr int edges[12][2] = {
				{ 0, 1 }, { 0, 2 }, { 0, 4 }, { 1, 3 }, { 1, 5 }, { 2, 3 },
				{ 2, 6 }, { 3, 7 }, { 4, 5 }, { 4, 6 }, { 5, 7 }, { 6, 7 }
			};
			for (const auto& edge : edges)
			{
				if (!corner_valid[edge[0]] || !corner_valid[edge[1]]) continue;
				draw->AddLine(ImVec2(corners_x[edge[0]], corners_y[edge[0]]),
					ImVec2(corners_x[edge[1]], corners_y[edge[1]]), IM_COL32(0, 0, 0, 190), 3.4f);
				draw->AddLine(ImVec2(corners_x[edge[0]], corners_y[edge[0]]),
					ImVec2(corners_x[edge[1]], corners_y[edge[1]]), color, 1.5f);
			}
		}

		const double dx = entry.x + .5 - cx, dy = entry.y + .5 - cy, dz = entry.z + .5 - cz;
		const int distance = static_cast<int>(std::sqrt(dx * dx + dy * dy + dz * dz));
		char text[96]{};
		if (show_labels && show_distance) std::snprintf(text, sizeof(text), "%s  %dm", entry.label.c_str(), distance);
		else if (show_labels) std::snprintf(text, sizeof(text), "%s", entry.label.c_str());
		else if (show_distance) std::snprintf(text, sizeof(text), "%dm", distance);
		if (text[0] == '\0') continue;
		const ImVec2 text_size = ImGui::CalcTextSize(text);
		const float text_x = (min_x + max_x - text_size.x) * 0.5f;
		const float text_y = min_y - text_size.y - 5.0f;
		draw->AddRectFilled(ImVec2(text_x - 5.0f, text_y - 2.0f),
			ImVec2(text_x + text_size.x + 5.0f, text_y + text_size.y + 2.0f),
			IM_COL32(7, 9, 13, 210), 4.0f);
		draw->AddText(ImVec2(text_x, text_y), color, text);
	}
}

void modules::storage_esp::on_disable(::cache&)
{
	std::scoped_lock lock(marker_mutex);
	markers.clear(); scan_index = 0;
}

void modules::storage_esp::render_options()
{
	const char* styles[] = { "2D Box", "3D Box" };
	ImGui::Combo(gui::text("Box Style", "Kutu Stili"), &box_style, styles, IM_ARRAYSIZE(styles));
	ImGui::SliderInt(gui::text("Range", "Menzil"), &range, 4, 24, "%d blocks");
	ImGui::Checkbox(gui::text("Show Labels", "Etiketleri Goster"), &show_labels);
	ImGui::Checkbox(gui::text("Show Distance", "Mesafeyi Goster"), &show_distance);
}
