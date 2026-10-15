#include "block_esp.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../ui_redirect.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <vector>

#include <imgui/imgui.h>

#include "../gui/gui.hpp"


namespace
{
	struct scanned_block
	{
		double x = 0.0;
		double y = 0.0;
		double z = 0.0;
		std::string key;
	};

	std::uint64_t block_key(int x, int y, int z)
	{
		return (static_cast<std::uint64_t>(x) & 0x3FFFFFFULL) << 38 |
			(static_cast<std::uint64_t>(z) & 0x3FFFFFFULL) << 12 |
			(static_cast<std::uint64_t>(y) & 0xFFFULL);
	}

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

	std::string display_block_name(const std::string& identifier)
	{
		std::string name = identifier;
		const std::size_t separator = name.find(':');
		if (separator != std::string::npos) name.erase(0, separator + 1);
		bool capitalize = true;
		for (char& character : name)
		{
			if (character == '_') { character = ' '; capitalize = true; continue; }
			if (capitalize && character >= 'a' && character <= 'z') character = static_cast<char>(character - 'a' + 'A');
			capitalize = false;
		}
		return name;
	}
}

void modules::block_esp::run(::cache& cache)
{
	if (!cache.is_valid() || !cache.player || !cache.world || !jni::get_env()) return;
	try
	{
		jni::frame frame{ 4096 };
		maps::DefaultedRegistry block_registry = maps::Registries{}.BLOCK.get();
		if (!block_registry || platform::jni_context::clear_exception()) return;
		if (!registry_loaded)
		{
			std::vector<std::string> catalog;
			maps::Iterator iterator = block_registry.iterator.call();
			while (iterator && iterator.hasNext.call() == JNI_TRUE && !platform::jni_context::clear_exception())
			{
				maps::Object block = iterator.next.call();
				maps::Object block_object(block.object_instance, false);
				maps::Identifier identifier = block ? block_registry.getId.call(block_object) : maps::Identifier{};
				maps::String text = identifier && !platform::jni_context::clear_exception() ? identifier.toString.call() : maps::String{};
				if (text && !platform::jni_context::clear_exception()) catalog.push_back(text.to_string());
				platform::jni_context::clear_exception();
			}
			std::sort(catalog.begin(), catalog.end());
			catalog.erase(std::unique(catalog.begin(), catalog.end()), catalog.end());
			{
				std::scoped_lock lock(marker_mutex);
				available_blocks = std::move(catalog);
				registry_loaded = true;
			}
		}

		std::vector<std::string> selected_snapshot;
		{
			std::scoped_lock lock(marker_mutex);
			selected_snapshot = selected_blocks;
		}
		const maths::vector3d player_pos = cache.player.get_position();
		const int px = static_cast<int>(std::floor(player_pos.x));
		const int py = static_cast<int>(std::floor(player_pos.y));
		const int pz = static_cast<int>(std::floor(player_pos.z));
		const int r = std::clamp(range, 3, 12);
		const int side = r * 2 + 1;
		const int vertical = r * 2 + 1;
		const int total = side * side * vertical;

		{
			std::scoped_lock lock(marker_mutex);
			if (px != origin_x || py != origin_y || pz != origin_z)
			{
				origin_x = px; origin_y = py; origin_z = pz; scan_index = 0;
			}
		}

		std::vector<scanned_block> scanned;
		const int start = scan_index;
		const int end = std::min(total, start + 128);
		scanned.reserve(static_cast<std::size_t>(end - start));
		for (int index = start; index < end; ++index)
		{
			const int x_offset = index % side - r;
			const int temporary = index / side;
			const int z_offset = temporary % side - r;
			const int y_offset = temporary / side - r;
			const int x = px + x_offset;
			const int y = py + y_offset;
			const int z = pz + z_offset;
			maps::BlockPos pos = maps::BlockPos::new_object(&maps::BlockPos::constructor,
				static_cast<jint>(x), static_cast<jint>(y), static_cast<jint>(z));
			maps::BlockState state = pos ? cache.world.getBlockState.call(pos) : maps::BlockState{};
			std::string selected_key;
			if (state && !platform::jni_context::clear_exception())
			{
				maps::Block block = state.getBlock.call();
				maps::Object block_object(block.object_instance, false);
				maps::Identifier identifier = block ? block_registry.getId.call(block_object) : maps::Identifier{};
				maps::String text = identifier && !platform::jni_context::clear_exception() ? identifier.toString.call() : maps::String{};
				const std::string key = text && !platform::jni_context::clear_exception() ? text.to_string() : std::string{};
				platform::jni_context::clear_exception();
				if (std::find(selected_snapshot.begin(), selected_snapshot.end(), key) != selected_snapshot.end())
					selected_key = key;
			}
			scanned.push_back({ static_cast<double>(x), static_cast<double>(y), static_cast<double>(z), std::move(selected_key) });
		}

		std::unordered_set<std::uint64_t> scanned_keys;
		scanned_keys.reserve(scanned.size() * 2);
		for (const scanned_block& cell : scanned)
			scanned_keys.insert(block_key(static_cast<int>(cell.x), static_cast<int>(cell.y), static_cast<int>(cell.z)));

		std::scoped_lock lock(marker_mutex);
		markers.erase(std::remove_if(markers.begin(), markers.end(), [&](const marker& old)
		{
			return scanned_keys.contains(block_key(
				static_cast<int>(old.x), static_cast<int>(old.y), static_cast<int>(old.z)));
		}), markers.end());
		for (const scanned_block& cell : scanned)
			if (!cell.key.empty() && markers.size() < 10000)
				markers.push_back({ cell.x, cell.y, cell.z, cell.key });
		markers.erase(std::remove_if(markers.begin(), markers.end(), [&](const marker& old)
		{
			return std::abs(old.x - px) > r || std::abs(old.y - py) > r || std::abs(old.z - pz) > r;
		}), markers.end());
		scan_index = end >= total ? 0 : end;
	}
	catch (...) { platform::jni_context::clear_exception(); }
}

void modules::block_esp::render_overlay()
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
	const ImColor accent = gui::get_dynamic_color();
	ImDrawList* draw = ImGui::GetBackgroundDrawList();
	for (const marker& entry : draw_markers)
	{
		const double dx = entry.x + 0.5 - cx;
		const double dy = entry.y + 0.5 - cy;
		const double dz = entry.z + 0.5 - cz;
		const float distance = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
		const float alpha = std::clamp(1.15f - distance / (std::clamp(range, 3, 12) * 1.55f), 0.12f, 0.92f);
		ImVec4 line_value = accent.Value;
		line_value.w = alpha;
		ImVec4 fill_value = accent.Value;
		fill_value.w = std::clamp(fill_opacity, 0.0f, 0.35f) * alpha;

		float min_x = (std::numeric_limits<float>::max)();
		float min_y = (std::numeric_limits<float>::max)();
		float max_x = (std::numeric_limits<float>::lowest)();
		float max_y = (std::numeric_limits<float>::lowest)();
		int projected = 0;
		for (int x = 0; x <= 1; ++x) for (int y = 0; y <= 1; ++y) for (int z = 0; z <= 1; ++z)
		{
			float sx = 0.0f, sy = 0.0f;
			if (project(entry.x + x, entry.y + y, entry.z + z,
				cx, cy, cz, yaw, pitch, display.x, display.y, fov, sx, sy))
			{
				min_x = std::min(min_x, sx); min_y = std::min(min_y, sy);
				max_x = std::max(max_x, sx); max_y = std::max(max_y, sy); ++projected;
			}
		}
		if (projected < 4 || max_x - min_x < 1.0f || max_y - min_y < 1.0f ||
			max_x < 0.0f || min_x > display.x || max_y < 0.0f || min_y > display.y)
			continue;
		draw->AddRectFilled(ImVec2(min_x, min_y), ImVec2(max_x, max_y),
			ImGui::ColorConvertFloat4ToU32(fill_value), 1.0f);
		draw->AddRect(ImVec2(min_x, min_y), ImVec2(max_x, max_y),
			ImGui::ColorConvertFloat4ToU32(line_value), 1.0f, 0, line_thickness);
	}
}

void modules::block_esp::on_disable(::cache&)
{
	std::scoped_lock lock(marker_mutex);
	markers.clear(); scan_index = 0;
}

void modules::block_esp::render_options()
{
	bool needs_catalog = false;
	{
		std::scoped_lock lock(marker_mutex);
		needs_catalog = !registry_loaded;
	}
	if (needs_catalog && platform::jni_context::bind_current_thread())
	{
		try
		{
			jni::frame frame{ 2048 };
			maps::DefaultedRegistry block_registry = maps::Registries{}.BLOCK.get();
			std::vector<std::string> catalog;
			maps::Iterator iterator = block_registry ? block_registry.iterator.call() : maps::Iterator{};
			while (iterator && iterator.hasNext.call() == JNI_TRUE && !platform::jni_context::clear_exception())
			{
				maps::Object block = iterator.next.call();
				maps::Identifier identifier = block ? block_registry.getId.call(block) : maps::Identifier{};
				maps::String text = identifier && !platform::jni_context::clear_exception() ? identifier.toString.call() : maps::String{};
				if (text && !platform::jni_context::clear_exception()) catalog.push_back(text.to_string());
				platform::jni_context::clear_exception();
			}
			if (!catalog.empty())
			{
				std::sort(catalog.begin(), catalog.end());
				catalog.erase(std::unique(catalog.begin(), catalog.end()), catalog.end());
				std::scoped_lock lock(marker_mutex);
				available_blocks = std::move(catalog);
				registry_loaded = true;
			}
		}
		catch (...) { platform::jni_context::clear_exception(); }
	}

	ImGui::SliderInt(gui::text("Range", "Menzil"), &range, 3, 12, "%d blocks");
	ImGui::SliderFloat(gui::text("Line Thickness", "Cizgi Kalinligi"),
		&line_thickness, 0.5f, 3.0f, "%.1f");
	ImGui::SliderFloat(gui::text("Fill Opacity", "Dolgu Saydamligi"),
		&fill_opacity, 0.0f, 0.35f, "%.2f");

	std::vector<std::string> catalog;
	std::vector<std::string> selected;
	{
		std::scoped_lock lock(marker_mutex);
		catalog = available_blocks;
		selected = selected_blocks;
	}
	ImGui::InputTextWithHint("##block_search", gui::text("Search blocks...", "Blok ara..."),
		block_search, sizeof(block_search));
	ImGui::SameLine();
	if (ImGui::Button(gui::text("Clear", "Temizle")))
	{
		std::scoped_lock lock(marker_mutex);
		selected_blocks.clear(); markers.clear(); scan_index = 0;
		selected.clear();
	}
	ImGui::TextDisabled("%zu / %zu", selected.size(), catalog.size());
	std::string query = block_search;
	std::transform(query.begin(), query.end(), query.begin(), [](unsigned char value)
		{ return static_cast<char>(std::tolower(value)); });
	ImGui::BeginChild("##block_catalog", ImVec2(0.0f, 255.0f), true);
	for (const std::string& identifier : catalog)
	{
		const std::string label = display_block_name(identifier);
		std::string searchable = identifier + " " + label;
		std::transform(searchable.begin(), searchable.end(), searchable.begin(), [](unsigned char value)
			{ return static_cast<char>(std::tolower(value)); });
		if (!query.empty() && searchable.find(query) == std::string::npos) continue;
		const bool is_selected = std::find(selected.begin(), selected.end(), identifier) != selected.end();
		if (ImGui::Selectable((label + "##" + identifier).c_str(), is_selected))
		{
			std::scoped_lock lock(marker_mutex);
			auto found = std::find(selected_blocks.begin(), selected_blocks.end(), identifier);
			if (found == selected_blocks.end()) selected_blocks.push_back(identifier);
			else selected_blocks.erase(found);
			markers.clear(); scan_index = 0;
		}
	}
	ImGui::EndChild();
}
