#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "hud.hpp"
#include "../../gui/watermark_draw.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../ui_redirect.hpp"
#include "../gui/gui.hpp"
#include "../friends/friends.hpp"

#include <Windows.h>
#include <imgui/imgui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>
#include <string>

namespace
{
ImU32 color_with_alpha(const ImColor& color, float alpha)
    {
        ImVec4 value = color.Value;
        value.w *= std::clamp(alpha, 0.0f, 1.0f);
        return ImGui::ColorConvertFloat4ToU32(value);
    }

    void draw_glow_border(
        ImDrawList* draw_list,
        const ImVec2& minimum,
        const ImVec2& maximum,
        float rounding,
        float scale)
    {
        if (!draw_list)
            return;

        const ImColor accent = gui::get_dynamic_color(25);

        // Several low-alpha outlines approximate a soft blur without requiring
        // an extra OpenGL shader or render target.
        for (int layer = 7; layer >= 1; --layer)
        {
            const float expansion = static_cast<float>(layer) * 1.35f * scale;
            const float alpha = 0.012f + static_cast<float>(8 - layer) * 0.010f;
            draw_list->AddRect(
                ImVec2(minimum.x - expansion, minimum.y - expansion),
                ImVec2(maximum.x + expansion, maximum.y + expansion),
                color_with_alpha(accent, alpha),
                rounding + expansion,
                ImDrawFlags_RoundCornersAll,
                std::max(1.0f, 2.4f * scale));
        }

        draw_list->AddRect(
            minimum,
            maximum,
            color_with_alpha(accent, 0.78f),
            rounding,
            ImDrawFlags_RoundCornersAll,
            std::max(1.0f, 1.15f * scale));
    }

    void draw_separator(
        ImDrawList* draw_list,
        float x,
        float center_y,
        float height,
        float scale)
    {
        draw_list->AddRectFilled(
            ImVec2(std::floor(x), std::floor(center_y - height * 0.5f)),
            ImVec2(std::ceil(x + std::max(1.0f, scale)), std::ceil(center_y + height * 0.5f)),
            IM_COL32(255, 255, 255, 42),
            1.0f);
    }

    void draw_thedorius_mark(ImDrawList* draw_list, const ImVec2& center, float size, const ImColor& accent)
    {
        const ImVec2 shield[] = {
            ImVec2(center.x - size, center.y - size * 0.82f),
            ImVec2(center.x + size, center.y - size * 0.82f),
            ImVec2(center.x + size * 0.70f, center.y + size * 0.64f),
            ImVec2(center.x, center.y + size),
            ImVec2(center.x - size * 0.70f, center.y + size * 0.64f)
        };
        draw_list->AddConvexPolyFilled(shield, IM_ARRAYSIZE(shield), color_with_alpha(accent, 0.16f));
        draw_list->AddPolyline(shield, IM_ARRAYSIZE(shield), color_with_alpha(accent, 0.96f), ImDrawFlags_Closed, std::max(1.0f, size * 0.13f));
        draw_list->AddLine(ImVec2(center.x - size * 0.48f, center.y - size * 0.34f), ImVec2(center.x + size * 0.48f, center.y - size * 0.34f), IM_COL32(242, 242, 247, 245), std::max(1.0f, size * 0.16f));
        draw_list->AddLine(ImVec2(center.x, center.y - size * 0.34f), ImVec2(center.x, center.y + size * 0.52f), IM_COL32(242, 242, 247, 245), std::max(1.0f, size * 0.16f));
    }
}


void modules::hud::run(::cache& cache)
{
    const auto now = std::chrono::steady_clock::now();
    if (now < next_status_refresh) return;
    next_status_refresh = now + std::chrono::milliseconds(500);

    std::string current_name = friends::local_player_name();
    int current_latency = 0;
    {
        std::lock_guard lock(status_mutex);
        current_latency = latency_ms;
    }

    bool latency_resolved = false;
    try
    {
        if (cache.is_valid() && cache.player)
        {
            maps::GameProfile local_profile = cache.player.getGameProfile.call();
            maps::String local_name = local_profile ? local_profile.getName.call() : maps::String{};
            if (local_name && !platform::jni_context::clear_exception()) current_name = local_name.to_string();

            maps::ClientPlayNetworkHandler handler = cache.instance.getNetworkHandler.call();
            maps::PlayerListEntry local_entry =
                (handler && local_name) ? handler.getPlayerListEntryByName.call(local_name) : maps::PlayerListEntry{};

            if (!platform::jni_context::clear_exception() && local_entry)
            {
                const int measured_latency = static_cast<int>(local_entry.getLatency.call());
                if (!platform::jni_context::clear_exception())
                {
                    current_latency = std::clamp(measured_latency, 0, 9999);
                    latency_resolved = true;
                }
            }

            // Some proxy/mod combinations briefly return no direct entry while the
            // player list is being rebuilt. Keep the collection lookup as a safe
            // fallback, but never replace the last valid ping on a failed refresh.
            if (!latency_resolved && handler)
            {
                maps::Collection players = handler.getPlayerList.call();
                jni::array<maps::Object> entries = players ? players.toArray.call() : jni::array<maps::Object>{ nullptr };
                if (entries && !platform::jni_context::clear_exception())
                {
                    for (const maps::Object& object : entries.to_vector())
                    {
                        maps::PlayerListEntry entry(object.object_instance, false);
                        maps::GameProfile profile = entry ? entry.getProfile.call() : maps::GameProfile{};
                        maps::String name = profile ? profile.getName.call() : maps::String{};
                        if (!name || platform::jni_context::clear_exception() || name.to_string() != current_name) continue;

                        const int measured_latency = static_cast<int>(entry.getLatency.call());
                        if (!platform::jni_context::clear_exception())
                        {
                            current_latency = std::clamp(measured_latency, 0, 9999);
                            latency_resolved = true;
                        }
                        break;
                    }
                }
            }
        }
    }
    catch (...)
    {
        platform::jni_context::clear_exception();
    }

    if (current_name.empty()) current_name = "Player";
    std::lock_guard lock(status_mutex);
    player_name = std::move(current_name);
    latency_ms = current_latency;
}

void modules::hud::render_overlay()
{
	if (gui::hud_editor_active) return;
	const ImVec2 size = gui::watermark_draw::size();
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float x = std::clamp(std::round(gui::hud_pos_x), 8.f, std::max(8.f, display.x - size.x - 8.f));
	const float y = std::clamp(std::round(gui::hud_pos_y), 8.f, std::max(8.f, display.y - size.y - 8.f));
	gui::watermark_draw::draw(ImGui::GetForegroundDrawList(), x, y);
}

void modules::hud::render_options()
{
	ImGui::TextDisabled("%s", gui::text("Displays onestep logo watermark", "onestep logo watermark gostergesini ekrana cizer"));
	ImGui::Spacing();
	ImGui::SliderFloat(gui::text("Scale##watermark", "Olcek##watermark"), &gui::hud_scale, 0.50f, 2.0f, "%.2f");
}

std::pair<std::string, int> modules::hud::status_snapshot() const
{
	std::lock_guard lock(status_mutex);
	return { player_name, latency_ms };
}
