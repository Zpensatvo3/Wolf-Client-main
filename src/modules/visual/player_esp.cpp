#include "../modules.hpp"
#include "../../services/game/anti_bot_service.hpp"
#include "../../services/game/name_protect.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../../services/game/client_context.hpp"
#include "../../services/game/overlay_context.hpp"
#include "../../gui/ui_state.hpp"
#include "visual_effects_logic.hpp"
#include "../../platform/jni/jni_context.hpp"
#include <imgui/imgui.h>
#include <algorithm>
#include <cmath>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <unordered_map>

extern JavaVM* jvm;

struct PlayerBox
{
	int entityId = -1;
    double minX, minY, minZ;
    double maxX, maxY, maxZ;
    std::string name;
    float health;
    float maxHealth;
    float distance;
};

static bool ensure_render_jni_env()
{
	if (jni::get_env()) return true;
	JNIEnv* env = nullptr;
	if (!jvm || jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_8) != JNI_OK || !env)
		return false;
	jni::set_thread_env(env);
	return jni::get_env() != nullptr;
}

static bool clear_render_exception()
{
	JNIEnv* env = jni::get_env();
	if (!env || env->ExceptionCheck() != JNI_TRUE) return false;
	env->ExceptionClear();
	return true;
}

static bool read_render_camera(const services::game::client_context& context, double& x, double& y, double& z,
	float& yaw, float& pitch, float& fov)
{
	jni::frame frame{ 64 };
	const auto& instance = context.instance;

	// Camera position and angles come from the shared adapter, which already
	// resolves the render side FOV and the correct Camera member for this build.
	auto& adapter = platform::adapter::get();
	const auto& player = context.player;
	const float delta = adapter.get_tick_delta(instance);
	const auto data = adapter.get_camera_data(instance, player, delta);
	if (!data.valid || clear_render_exception()) return false;

	x = data.pos.x; y = data.pos.y; z = data.pos.z;
	yaw = data.yaw; pitch = data.pitch;
	if (std::isfinite(data.fov) && data.fov >= 1.0f && data.fov <= 179.0f)
		fov = data.fov;
	return true;
}

static bool read_render_players(const services::game::client_context& context, std::vector<PlayerBox>& output, bool names)
{
	jni::frame frame{ 1024 };
	const auto& instance = context.instance;
	const auto& world = context.world;
	const auto& local = context.player;
	maps::RenderTickCounter counter = instance.renderTickCounter.get();
	const float tick_delta = counter ? std::clamp(counter.tickDelta.get(), 0.0f, 1.0f) : 1.0f;
	const maths::vector3d local_position = local.get_position();
	// Prefer the player collection; retain the entity fallback for adapters
	// whose mappings cannot expose it. Camera/positions are still read fresh.
	maps::List players = platform::adapter::get().get_players(world);
	const bool players_failed = clear_render_exception();
	maps::Iterable entities = players && !players_failed
		? maps::Iterable(players.object_instance, false) : world.getEntities.call();
	maps::Iterator iterator = entities ? entities.iterator.call() : maps::Iterator{};
	if (!iterator || clear_render_exception()) return false;

	output.clear();
	while (iterator.hasNext.call() == JNI_TRUE && !clear_render_exception())
	{
		maps::Object object = iterator.next.call();
		maps::Entity entity(object.object_instance, false);
		if (!entity || entity.is_same_object(local) ||
			!entity.is_instance_of<maps::PlayerEntity>() ||
			entity.isAlive.call() != JNI_TRUE || clear_render_exception())
			continue;
		if (services::anti_bot::is_bot(entity)) continue;
		maps::Box bounds = entity.boundingBox.get();
		maps::Vec3d current = entity.pos.get();
		maps::Vec3d lerped = entity.getLerpedPos.call(tick_delta);
		if (!bounds || !current || !lerped || clear_render_exception()) continue;
		const double shift_x = lerped.x.get() - current.x.get();
		const double shift_y = lerped.y.get() - current.y.get();
		const double shift_z = lerped.z.get() - current.z.get();
		if (clear_render_exception()) continue;

		PlayerBox box{};
		box.entityId = entity.getId.call();
		box.minX = bounds.minX.get() + shift_x; box.maxX = bounds.maxX.get() + shift_x;
		box.minY = bounds.minY.get() + shift_y; box.maxY = bounds.maxY.get() + shift_y;
		box.minZ = bounds.minZ.get() + shift_z; box.maxZ = bounds.maxZ.get() + shift_z;
		maps::LivingEntity living(entity.object_instance, false);
		box.health = living.getHealth.call();
		box.maxHealth = living.getMaxHealth.call();
		if (clear_render_exception() || !std::isfinite(box.health) ||
			!std::isfinite(box.maxHealth) || box.maxHealth <= 0.0f)
		{
			box.health = 20.0f; box.maxHealth = 20.0f;
		}
			if (names) {
				maps::String name = entity.getEntityName.call();
				if (name && !clear_render_exception()) box.name = name.to_string();
			}
		clear_render_exception();
		const double dx = (box.minX + box.maxX) * 0.5 - local_position.x;
		const double dy = (box.minY + box.maxY) * 0.5 - local_position.y;
		const double dz = (box.minZ + box.maxZ) * 0.5 - local_position.z;
		box.distance = static_cast<float>(std::sqrt(dx * dx + dy * dy + dz * dz));
		output.push_back(std::move(box));
	}
	return !clear_render_exception();
}

// Shared state - protected by mutex
static std::mutex              s_mutex;
static services::game::overlay_session s_visual_session;

static void clear_player_snapshot()
{
    std::lock_guard lock(s_mutex);
    if (jni::get_env()) s_visual_session.reset(); else s_visual_session.invalidate();
}

void modules::player_esp::on_disable(::cache&)
{
    clear_player_snapshot();
}

// Correct Minecraft 1.21.1 world-to-screen projection.
// Derived from Minecraft's camera coordinate system:
//   forward = { -sin(yaw)*cos(pitch), -sin(pitch), cos(yaw)*cos(pitch) }
//   right   = { cos(yaw), 0, sin(yaw) }
//   up      = cross(right, forward)
// View space = dot product of world-offset with each axis.
struct ViewPoint
{
	double x = 0.0;
	double y = 0.0;
	double z = 0.0;
};

static ViewPoint to_view(double wx, double wy, double wz,
	double cx, double cy, double cz, float yaw, float pitch)
{
	const double rx = wx - cx;
	const double ry = wy - cy;
	const double rz = wz - cz;

    const double pi = 3.14159265358979323846;
    const double yaw_rad   = static_cast<double>(yaw) * pi / 180.0;
    const double pitch_rad = static_cast<double>(pitch) * pi / 180.0;

    const double sy_yaw = std::sin(yaw_rad), cy_yaw = std::cos(yaw_rad);
    const double sp = std::sin(pitch_rad), cp = std::cos(pitch_rad);

    // Minecraft look/right/up basis vectors
    // Forward (Z)
    const double fz_x = -sy_yaw * cp;
    const double fz_y = -sp;
    const double fz_z = cy_yaw * cp;

    // Right (X)
    const double rx_x = cy_yaw;
    const double rx_z = sy_yaw;

    // Up (Y)
    const double uy_x = -sy_yaw * sp;
    const double uy_y = cp;
    const double uy_z = cy_yaw * sp;

	return {
		rx * -rx_x + rz * -rx_z,
		rx * uy_x + ry * uy_y + rz * uy_z,
		rx * fz_x + ry * fz_y + rz * fz_z
	};
}

static bool project_view(const ViewPoint& point, float sw, float sh, float fov_deg,
	float& sx, float& sy, bool clamp_extremes = false)
{
	constexpr double near_plane = 0.05;
	if (point.z < near_plane)
		return false;
	const double pi = 3.14159265358979323846;
	const double focal = (sh * 0.5) / std::tan(static_cast<double>(fov_deg) * pi / 360.0);
	sx = static_cast<float>(sw * 0.5 + point.x * focal / point.z);
	sy = static_cast<float>(sh * 0.5 - point.y * focal / point.z);
	if (clamp_extremes)
	{
		// Near-plane intersections can legitimately project far beyond the
		// viewport. Capping them avoids float overflow in ImGui tessellation.
		sx = std::clamp(sx, -sw * 4.0f, sw * 5.0f);
		sy = std::clamp(sy, -sh * 4.0f, sh * 5.0f);
	}
	return std::isfinite(sx) && std::isfinite(sy);
}

static bool w2s(double wx, double wy, double wz,
                double cx, double cy, double cz,
                float yaw, float pitch,
                float sw, float sh, float fov_deg,
                float& sx, float& sy)
{
	return project_view(to_view(wx, wy, wz, cx, cy, cz, yaw, pitch),
		sw, sh, fov_deg, sx, sy);
}

static std::vector<ViewPoint> clip_face_to_near_plane(const ViewPoint* vertices, int count)
{
	constexpr double near_plane = 0.05;
	std::vector<ViewPoint> output;
	output.reserve(static_cast<std::size_t>(count) + 2);
	for (int index = 0; index < count; ++index)
	{
		const ViewPoint& current = vertices[index];
		const ViewPoint& previous = vertices[(index + count - 1) % count];
		const bool current_inside = current.z >= near_plane;
		const bool previous_inside = previous.z >= near_plane;
		if (current_inside != previous_inside)
		{
			const double denominator = current.z - previous.z;
			if (std::abs(denominator) > 1.0e-9)
			{
				const double amount = (near_plane - previous.z) / denominator;
				output.push_back({
					previous.x + (current.x - previous.x) * amount,
					previous.y + (current.y - previous.y) * amount,
					near_plane
				});
			}
		}
		if (current_inside)
			output.push_back(current);
	}
	return output;
}

static bool clip_edge_to_near_plane(ViewPoint& from, ViewPoint& to)
{
	constexpr double near_plane = 0.05;
	const bool from_inside = from.z >= near_plane;
	const bool to_inside = to.z >= near_plane;
	if (!from_inside && !to_inside)
		return false;
	if (from_inside && to_inside)
		return true;
	ViewPoint& outside = from_inside ? to : from;
	const ViewPoint& inside = from_inside ? from : to;
	const double denominator = outside.z - inside.z;
	if (std::abs(denominator) <= 1.0e-9)
		return false;
	const double amount = (near_plane - inside.z) / denominator;
	outside.x = inside.x + (outside.x - inside.x) * amount;
	outside.y = inside.y + (outside.y - inside.y) * amount;
	outside.z = near_plane;
	return true;
}

// Live client-thread reads only: disconnects must never fall back to old boxes.
void modules::player_esp::render_overlay()
{
    auto settings_snapshot = settings_.read();
    if (!platform::jni_context::bind_current_thread()) { clear_player_snapshot(); return; }
    jni::frame render_frame{96};
    services::game::client_context context;
    if (!services::game::capture_client_context(context, true, false)) { clear_player_snapshot(); return; }
    const float sw = ImGui::GetIO().DisplaySize.x;
    const float sh = ImGui::GetIO().DisplaySize.y;
    if (sw < 1.f || sh < 1.f) return;

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl) return;

    // Fresh client-thread data is the only source used by the renderer.
    double cx=0., cy=0., cz=0.;
    float yaw=0.f, pitch=0.f;
    float fov=settings_snapshot.fov_value;
	float render_fov = fov;
	if (!read_render_camera(context, cx, cy, cz, yaw, pitch, render_fov)) { clear_player_snapshot(); return; }
    if (settings_snapshot.use_game_fov) fov = render_fov;

	ImVec4 active_color = settings_snapshot.use_theme_color
		? gui::get_dynamic_color().Value
		: ImVec4(settings_snapshot.esp_color[0], settings_snapshot.esp_color[1], settings_snapshot.esp_color[2], settings_snapshot.esp_color[3]);
	active_color.w = std::clamp(active_color.w, 0.05f, 1.0f);
	static thread_local std::vector<PlayerBox> boxes;
    struct release_boxes {
        std::vector<PlayerBox>& value;
        ~release_boxes() {
            value.clear();
            if (value.capacity()>2048) std::vector<PlayerBox>{}.swap(value);
        }
    } release{boxes};
	if (!read_render_players(context, boxes, settings_snapshot.show_names)) { clear_player_snapshot(); return; }

    struct health_visual { visual_effects_logic::health_animation animation; int frame=-1; };
    struct visual_runtime {
        std::unordered_map<int,health_visual> health;
        ImVec4 color{};
        std::uint64_t generation=0;
        double time=-1.;
    };
    static visual_runtime visual; // Only the render thread owns animation state.
    std::uint64_t generation;
    {
        std::lock_guard lock(s_mutex);
        s_visual_session.observe({context.world,context.player});
        generation=s_visual_session.ticket();
    }
    const double now=ImGui::GetTime();
    if (visual.generation!=generation || visual.time<0. || now<visual.time || now-visual.time>.5) {
        visual.health.clear(); visual.color=active_color; visual.generation=generation;
    }
    visual.time=now;
    const bool reduced=gui::ui_state::get_preferences().reduce_motion;
    const float dt=ImGui::GetIO().DeltaTime;
    const float blend=reduced ? 1.f : visual_effects_logic::response(dt,10.);
    visual.color={visual.color.x+(active_color.x-visual.color.x)*blend,
        visual.color.y+(active_color.y-visual.color.y)*blend,
        visual.color.z+(active_color.z-visual.color.z)*blend,
        visual.color.w+(active_color.w-visual.color.w)*blend};
    active_color=visual.color;
    ImColor acc(active_color);
    const ImU32 col=ImGui::ColorConvertFloat4ToU32(active_color);
    const int frame=ImGui::GetFrameCount();
    for (const auto& box:boxes) {
        auto& entry=visual.health[box.entityId];
        entry.animation.update(box.maxHealth>0.f ? box.health/box.maxHealth : 0.f,dt,reduced);
        entry.frame=frame;
    }
    std::erase_if(visual.health,[&](const auto& entry) { return entry.second.frame!=frame; });

    for (const PlayerBox& b : boxes)
    {
        // Project all 8 corners of the bounding box
        double bx[8] = { b.minX, b.maxX, b.minX, b.maxX, b.minX, b.maxX, b.minX, b.maxX };
        double by[8] = { b.minY, b.minY, b.maxY, b.maxY, b.minY, b.minY, b.maxY, b.maxY };
        double bz[8] = { b.minZ, b.minZ, b.minZ, b.minZ, b.maxZ, b.maxZ, b.maxZ, b.maxZ };
		static constexpr int box_edges[12][2] = {
			{ 0, 1 }, { 1, 3 }, { 3, 2 }, { 2, 0 },
			{ 4, 5 }, { 5, 7 }, { 7, 6 }, { 6, 4 },
			{ 0, 4 }, { 1, 5 }, { 2, 6 }, { 3, 7 }
		};

        float px[8], py_[8];
        bool  ok[8];
		ViewPoint view_points[8];
        int   vis = 0;
        for (int i = 0; i < 8; i++)
        {
			view_points[i] = to_view(bx[i], by[i], bz[i], cx, cy, cz, yaw, pitch);
			ok[i] = project_view(view_points[i], sw, sh, fov, px[i], py_[i]);
            if (ok[i]) vis++;
        }
		if (settings_snapshot.esp_mode == 2 && vis < 2) continue;

        // Find 2D bounding rectangle
        float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
        for (int i = 0; i < 8; i++)
        {
            if (!ok[i]) continue;
            if (px[i]  < x0) x0 = px[i];
            if (py_[i] < y0) y0 = py_[i];
            if (px[i]  > x1) x1 = px[i];
            if (py_[i] > y1) y1 = py_[i];
        }
		if (settings_snapshot.esp_mode != 2)
		{
			// A centre-axis box does not change width when different 3D corners
			// cross the near plane. This is substantially steadier while either
			// the camera or the remote player is moving.
			const double center_x = (b.minX + b.maxX) * 0.5;
			const double center_z = (b.minZ + b.maxZ) * 0.5;
			float top_x = 0.0f, top_y = 0.0f, bottom_x = 0.0f, bottom_y = 0.0f;
			if (!w2s(center_x, b.maxY + 0.04, center_z, cx, cy, cz,
				yaw, pitch, sw, sh, fov, top_x, top_y) ||
				!w2s(center_x, b.minY - 0.03, center_z, cx, cy, cz,
					yaw, pitch, sw, sh, fov, bottom_x, bottom_y))
				continue;
			const float height = std::abs(bottom_y - top_y);
			if (!std::isfinite(height) || height < 3.0f || height > sh * 3.0f)
				continue;
			const double world_height = std::max(0.1, b.maxY - b.minY);
			const double world_width = std::max(0.1, b.maxX - b.minX);
			const float width = height * static_cast<float>(
				std::clamp(world_width / world_height * 1.14, 0.28, 0.58));
			const float center_screen_x = (top_x + bottom_x) * 0.5f;
			x0 = center_screen_x - width * 0.5f;
			x1 = center_screen_x + width * 0.5f;
			y0 = std::min(top_y, bottom_y);
			y1 = std::max(top_y, bottom_y);

		}
		else
		{
			// Include near-plane-clipped edge endpoints in the screen bounds. This
			// keeps the box alive even when the camera crosses one of its faces.
			for (const auto& edge : box_edges)
			{
				ViewPoint from = view_points[edge[0]];
				ViewPoint to = view_points[edge[1]];
				if (!clip_edge_to_near_plane(from, to))
					continue;
				float edge_x[2]{};
				float edge_y[2]{};
				if (!project_view(from, sw, sh, fov, edge_x[0], edge_y[0], true) ||
					!project_view(to, sw, sh, fov, edge_x[1], edge_y[1], true))
					continue;
				for (int point = 0; point < 2; ++point)
				{
					x0 = std::min(x0, edge_x[point]);
					y0 = std::min(y0, edge_y[point]);
					x1 = std::max(x1, edge_x[point]);
					y1 = std::max(y1, edge_y[point]);
				}
			}
		}
		if (x0 > x1 || y0 > y1) continue;
        if (x1 < 0 || x0 > sw || y1 < 0 || y0 > sh) continue;

        if (settings_snapshot.esp_mode == 0) // Modern 2D Box
        {
            // Outer subtle glow / shadow
            dl->AddRect(ImVec2(x0 - 1.5f, y0 - 1.5f), ImVec2(x1 + 1.5f, y1 + 1.5f),
                IM_COL32(0, 0, 0, 160), 2.0f, 0, 1.0f);

            // Main ESP box
            dl->AddRect(ImVec2(x0, y0), ImVec2(x1, y1), col, 2.0f, 0, settings_snapshot.box_thickness);

            // Inner outline
            dl->AddRect(ImVec2(x0 + 1.0f, y0 + 1.0f), ImVec2(x1 - 1.0f, y1 - 1.0f),
                IM_COL32(0, 0, 0, 120), 1.0f, 0, 1.0f);
        }
        else if (settings_snapshot.esp_mode == 1) // Modern Corners
        {
            const float bxs = (x1 - x0) * 0.28f;
            const float bys = (y1 - y0) * 0.28f;
            const float bt = settings_snapshot.box_thickness;
            const ImU32 shadow = IM_COL32(0, 0, 0, 190);

            // Outer dark drop lines
            dl->AddLine(ImVec2(x0 - 1.0f, y0), ImVec2(x0 + bxs, y0), shadow, bt + 1.5f);
            dl->AddLine(ImVec2(x0, y0 - 1.0f), ImVec2(x0, y0 + bys), shadow, bt + 1.5f);
            dl->AddLine(ImVec2(x1 + 1.0f, y0), ImVec2(x1 - bxs, y0), shadow, bt + 1.5f);
            dl->AddLine(ImVec2(x1, y0 - 1.0f), ImVec2(x1, y0 + bys), shadow, bt + 1.5f);
            dl->AddLine(ImVec2(x0 - 1.0f, y1), ImVec2(x0 + bxs, y1), shadow, bt + 1.5f);
            dl->AddLine(ImVec2(x0, y1 + 1.0f), ImVec2(x0, y1 - bys), shadow, bt + 1.5f);
            dl->AddLine(ImVec2(x1 + 1.0f, y1), ImVec2(x1 - bxs, y1), shadow, bt + 1.5f);
            dl->AddLine(ImVec2(x1, y1 + 1.0f), ImVec2(x1, y1 - bys), shadow, bt + 1.5f);

            // Dynamic color corner lines
            dl->AddLine(ImVec2(x0, y0), ImVec2(x0 + bxs, y0), col, bt);
            dl->AddLine(ImVec2(x0, y0), ImVec2(x0, y0 + bys), col, bt);
            dl->AddLine(ImVec2(x1, y0), ImVec2(x1 - bxs, y0), col, bt);
            dl->AddLine(ImVec2(x1, y0), ImVec2(x1, y0 + bys), col, bt);
            dl->AddLine(ImVec2(x0, y1), ImVec2(x0 + bxs, y1), col, bt);
            dl->AddLine(ImVec2(x0, y1), ImVec2(x0, y1 - bys), col, bt);
            dl->AddLine(ImVec2(x1, y1), ImVec2(x1 - bxs, y1), col, bt);
            dl->AddLine(ImVec2(x1, y1), ImVec2(x1, y1 - bys), col, bt);
        }
		else if (settings_snapshot.esp_mode == 2) // 3D Box
		{
			static constexpr int faces[6][4] = {
				{ 0, 1, 3, 2 }, { 4, 5, 7, 6 },
				{ 0, 2, 6, 4 }, { 1, 3, 7, 5 },
				{ 0, 1, 5, 4 }, { 2, 3, 7, 6 }
			};

			ImVec4 fill_value = acc.Value;
			fill_value.w = std::clamp(settings_snapshot.fill_opacity, 0.02f, 0.55f);
			const ImU32 fill_color = ImGui::ColorConvertFloat4ToU32(fill_value);
			for (const auto& face : faces)
			{
				const ViewPoint input[4] = {
					view_points[face[0]], view_points[face[1]],
					view_points[face[2]], view_points[face[3]]
				};
				const std::vector<ViewPoint> clipped = clip_face_to_near_plane(input, 4);
				if (clipped.size() < 3)
					continue;
				std::vector<ImVec2> points;
				points.reserve(clipped.size());
				for (const ViewPoint& point : clipped)
				{
					float screen_x = 0.0f;
					float screen_y = 0.0f;
					if (project_view(point, sw, sh, fov, screen_x, screen_y, true))
						points.emplace_back(screen_x, screen_y);
				}
				if (points.size() >= 3)
					dl->AddConvexPolyFilled(points.data(), static_cast<int>(points.size()), fill_color);
			}

			const auto draw_edges = [&](ImU32 color, float thickness)
			{
				for (const auto& edge : box_edges)
				{
					ViewPoint from = view_points[edge[0]];
					ViewPoint to = view_points[edge[1]];
					if (!clip_edge_to_near_plane(from, to))
						continue;
					float from_x = 0.0f;
					float from_y = 0.0f;
					float to_x = 0.0f;
					float to_y = 0.0f;
					if (!project_view(from, sw, sh, fov, from_x, from_y, true) ||
						!project_view(to, sw, sh, fov, to_x, to_y, true))
						continue;
					dl->AddLine(
						ImVec2(from_x, from_y),
						ImVec2(to_x, to_y),
						color,
						thickness);
				}
			};

			draw_edges(ImColor(0, 0, 0, 190), settings_snapshot.box_thickness + 2.0f);
			draw_edges(col, settings_snapshot.box_thickness);
		}

        // ---- Modern Health bar (left side, vertical) ----
        if (settings_snapshot.show_health)
        {
            const float bar_w = 4.0f;
            const float bar_x = x0 - bar_w - 4.0f;
            const float bar_h = y1 - y0;
            const auto& health=visual.health.at(b.entityId).animation;
            const float hp_ratio=health.shown;

            const float filled_h = bar_h * hp_ratio;
            const float empty_top = y0 + (bar_h - filled_h);

            // Dark backdrop
            dl->AddRectFilled(ImVec2(bar_x - 1.0f, y0 - 1.0f), ImVec2(bar_x + bar_w + 1.0f, y1 + 1.0f),
                IM_COL32(10, 12, 16, 220), 2.0f);

            // Health color calculation
            const float healthy=static_cast<float>(visual_effects_logic::smoothstep(hp_ratio));
            const int red = static_cast<int>(245.f-healthy*170.f);
            const int green = static_cast<int>(80.f+healthy*146.f);
            const ImU32 hp_color = IM_COL32(red, green, 112, 240);

            if (health.trailing>hp_ratio+.002f)
                dl->AddRectFilled(ImVec2(bar_x,y1-bar_h*health.trailing),ImVec2(bar_x+bar_w,y1),
                    IM_COL32(255,169,102,120),1.5f);
            if (hp_ratio>.001f)
                dl->AddRectFilled(ImVec2(bar_x, empty_top), ImVec2(bar_x + bar_w, y1), hp_color, 1.5f);
        }

        // ---- Sleek Name tag (above box) ----
        if (settings_snapshot.show_names && !b.name.empty())
        {
            const auto display_name=services::privacy::display_name(b.name);
            const ImVec2 ts = ImGui::CalcTextSize(display_name.c_str());
            const float tx = (x0 + x1) * 0.5f - ts.x * 0.5f;
            const float ty = y0 - ts.y - 5.0f;

            dl->AddRectFilled(ImVec2(tx - 4.0f, ty - 2.0f), ImVec2(tx + ts.x + 4.0f, ty + ts.y + 1.0f),
                IM_COL32(12, 14, 18, 200), 3.0f);
            dl->AddText(ImVec2(tx, ty), IM_COL32(240, 242, 248, 255), display_name.c_str());
        }

        // ---- Sleek Distance indicator (below box) ----
        if (settings_snapshot.show_distance)
        {
            char dist_str[16];
            std::snprintf(dist_str, sizeof(dist_str), "%.1fm", b.distance);
            const ImVec2 ds = ImGui::CalcTextSize(dist_str);
            const float dx = (x0 + x1) * 0.5f - ds.x * 0.5f;
            const float dy = y1 + 4.0f;

            dl->AddRectFilled(ImVec2(dx - 3.0f, dy - 1.0f), ImVec2(dx + ds.x + 3.0f, dy + ds.y + 1.0f),
                IM_COL32(12, 14, 18, 180), 3.0f);
            dl->AddText(ImVec2(dx, dy), IM_COL32(200, 205, 215, 230), dist_str);
        }
	}

}

// ---------------------------------------------------------------------------
// Local skin texture lookup used by the inspector preview.
//
// Minecraft already owns a GPU texture for the local player skin, so the
// preview reuses it instead of decoding and uploading its own copy.
// ---------------------------------------------------------------------------
namespace
{
	std::atomic<std::uintptr_t> s_local_skin_texture{ 0 };
	std::mutex s_local_skin_mutex{};

	// Returns the cached GL id, or 0 while it has not been resolved yet.
	std::uintptr_t resolve_local_skin_texture()
	{
		if (!ensure_render_jni_env()) return 0;
		jni::frame frame{ 64 };

		try
		{
			maps::MinecraftClient instance = maps::MinecraftClient{}.instance.get();
			if (!instance) return 0;
			maps::ClientPlayerEntity player = instance.player.get();
			if (!player) return 0;

			maps::SkinTextures skin = player.getSkinTextures.call();
			if (!skin || clear_render_exception()) return 0;
			maps::Identifier identifier = skin.texture.call();
			if (!identifier || clear_render_exception()) return 0;

			maps::TextureManager manager = instance.getTextureManager.call();
			if (!manager || clear_render_exception()) return 0;
			maps::Texture texture = manager.getTexture.call(identifier);
			if (!texture || clear_render_exception()) return 0;

			const jint gl_id = texture.getGlId.call();
			if (clear_render_exception() || gl_id <= 0) return 0;
			return static_cast<std::uintptr_t>(gl_id);
		}
		catch (...)
		{
			clear_render_exception();
			return 0;
		}
	}
}

std::uintptr_t modules::player_esp::local_skin_texture_id()
{
	const std::uintptr_t cached = s_local_skin_texture.load(std::memory_order_acquire);
	return cached;
}

void modules::player_esp::refresh_local_skin_texture()
{
	std::lock_guard lock(s_local_skin_mutex);
	const std::uintptr_t resolved = resolve_local_skin_texture();
	if (resolved)
		s_local_skin_texture.store(resolved, std::memory_order_release);
}

void modules::player_esp::render_options()
{
    gui::settings::render(settings_, module_settings::player_esp::schema());
}
