#include "notification_renderer.hpp"
#include <cfloat>
#include <unordered_set>

namespace
{
    ImU32 alpha(ImVec4 color, float amount)
    {
        color.w *= amount; return ImGui::ColorConvertFloat4ToU32(color);
    }
    ImVec4 tint(ImVec4 a, ImVec4 b, float amount)
    {
        return {a.x + (b.x - a.x) * amount, a.y + (b.y - a.y) * amount,
            a.z + (b.z - a.z) * amount, a.w};
    }
}

void gui::notification_ui::renderer::render(const services::notifications::snapshot& snapshot,
    const ImGuiViewport& viewport, const appearance& style)
{
    namespace model = services::notifications;
    if (snapshot.entries.empty()) { clear(); return; }
    const float width = std::min(344.f, viewport.Size.x - 32.f);
    if (width < 100 || viewport.Size.y < 60 || !std::isfinite(width)) { clear(); return; }
    const int position = std::clamp(snapshot.settings.position, 0, 3);
    if (position_ != position) { animations_.clear(); position_ = position; }
    const bool left = position >= 2, top = position == 1 || position == 3;
    ImFont* font = style.font ? style.font : ImGui::GetFont();
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec4 background = tint(ImGui::GetStyleColorVec4(ImGuiCol_WindowBg), style.accent, .07f);
    const ImVec4 text = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    const ImVec4 muted = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    const float dt = ImGui::GetIO().DeltaTime;
    std::unordered_set<std::uint64_t> alive;
    float offset = 0;
    draw->PushClipRect(viewport.Pos, ImVec2(viewport.Pos.x + viewport.Size.x, viewport.Pos.y + viewport.Size.y), true);
    for (const auto& item : snapshot.entries)
    {
        const auto motion = model::animation(item, snapshot.now, snapshot.settings.duration, style.reduce_motion);
        if (motion.remaining <= 0) continue;
        const bool module = item.type == model::kind::module_on || item.type == model::kind::module_off;
        const bool error = item.type == model::kind::error;
        const bool off = item.type == model::kind::module_off;
        const char* title = item.title.empty()
            ? (error ? (style.turkish ? "Islem basarisiz" : "Operation failed")
                     : (style.turkish ? "Islem tamamlandi" : "Operation complete")) : item.title.c_str();
        const char* body = module
            ? (off ? (style.turkish ? "Modul kapatildi" : "Module disabled")
                   : (style.turkish ? "Modul acildi" : "Module enabled")) : item.body.c_str();
        const float wrap = width - 65.f;
        const ImVec2 extent = font->CalcTextSizeA(12.5f, FLT_MAX, wrap, body);
        const float body_height = std::min(extent.y, 12.5f * 3);
        const float height = std::max(66.f, 38.f + body_height + 12.f);
        if (offset + height > viewport.Size.y - 32.f) break;
        alive.insert(item.id);
        auto [it, inserted] = animations_.try_emplace(item.id, state{offset, 0});
        auto& anim = it->second;
        anim.offset = model::approach(anim.offset, offset, dt, style.reduce_motion);
        // A refreshed duplicate may revive a fading card. Ease that increase,
        // but keep exit opacity analytic so removal never cuts a visible tail.
        anim.alpha = motion.alpha < anim.alpha ? motion.alpha :
            model::approach(anim.alpha, motion.alpha, dt, style.reduce_motion);
        const float opacity = anim.alpha;
        const float slide = style.reduce_motion ? 0.f : motion.slide * (width + 24.f);
        const float x = viewport.Pos.x + (left ? 16.f - slide : viewport.Size.x - width - 16.f + slide);
        const float y = viewport.Pos.y + (top ? 16.f + anim.offset : viewport.Size.y - 16.f - height - anim.offset);
        const ImVec2 minimum(x, y), maximum(x + width, y + height);
        ImVec4 semantic = error ? tint(style.accent, ImVec4(.96f, .31f, .39f, 1.f), .8f) : style.accent;
        if (off) semantic = tint(style.accent, muted, .55f);
        draw->AddRectFilled(ImVec2(x - 2, y + 4), ImVec2(maximum.x + 2, maximum.y + 6),
            IM_COL32(0, 0, 0, static_cast<int>(65 * opacity)), 13.f);
        draw->AddRectFilled(minimum, maximum, alpha(background, .97f * opacity), 11.f);
        draw->AddRect(minimum, maximum, alpha(style.accent, .25f * opacity), 11.f, 0, 1.f);
        draw->AddRectFilled(ImVec2(x + 1, y + 13), ImVec2(x + 3, maximum.y - 13), alpha(semantic, opacity), 1.f);
        const ImVec2 center(x + 25, y + 31);
        draw->AddCircleFilled(center, 12.f, alpha(semantic, .13f * opacity));
        const ImU32 icon = alpha(semantic, opacity);
        if (error)
        {
            draw->AddLine(ImVec2(center.x - 3.5f, center.y - 3.5f), ImVec2(center.x + 3.5f, center.y + 3.5f), icon, 1.8f);
            draw->AddLine(ImVec2(center.x - 3.5f, center.y + 3.5f), ImVec2(center.x + 3.5f, center.y - 3.5f), icon, 1.8f);
        }
        else if (off) draw->AddLine(ImVec2(center.x - 4, center.y), ImVec2(center.x + 4, center.y), icon, 1.8f);
        else
        {
            draw->AddLine(ImVec2(center.x - 5, center.y), ImVec2(center.x - 1, center.y + 3.5f), icon, 1.8f);
            draw->AddLine(ImVec2(center.x - 1, center.y + 3.5f), ImVec2(center.x + 5, center.y - 4), icon, 1.8f);
        }
        const ImVec4 clip(x + 47, y + 10, maximum.x - 13, maximum.y - 9);
        draw->AddText(font, 14.f, ImVec2(x + 47, y + 12), alpha(text, opacity), title, nullptr, 0, &clip);
        draw->AddText(font, 12.5f, ImVec2(x + 47, y + 33), alpha(muted, opacity), body, nullptr, wrap, &clip);
        draw->AddRectFilled(ImVec2(x + 14, maximum.y - 5), ImVec2(maximum.x - 14, maximum.y - 3), alpha(style.accent, .12f * opacity), 1.f);
        const float end = x + 14 + (width - 28) * motion.remaining;
        draw->AddRectFilledMultiColor(ImVec2(x + 14, maximum.y - 5), ImVec2(end, maximum.y - 3),
            alpha(style.accent, opacity), alpha(style.secondary, opacity),
            alpha(style.secondary, opacity), alpha(style.accent, opacity));
        offset += height + 9.f;
    }
    draw->PopClipRect();
    std::erase_if(animations_, [&](const auto& pair) { return !alive.contains(pair.first); });
}
