#include "notification_renderer.hpp"
#include <cfloat>
#include <string>
#include <unordered_set>

namespace
{
    constexpr float card_rounding = 4.f;

    ImU32 alpha(ImVec4 color, float amount)
    {
        color.w *= amount; return ImGui::ColorConvertFloat4ToU32(color);
    }
    ImVec4 tint(ImVec4 a, ImVec4 b, float amount)
    {
        return {a.x + (b.x - a.x) * amount, a.y + (b.y - a.y) * amount,
            a.z + (b.z - a.z) * amount, a.w};
    }

    std::string fit_text(ImFont* font, float size, float width, std::string value)
    {
        for (char& c : value)
            if (c == '\n' || c == '\r' || c == '\t') c = ' ';
        if (font->CalcTextSizeA(size, FLT_MAX, 0.f, value.c_str()).x <= width) return value;
        const float dots = font->CalcTextSizeA(size, FLT_MAX, 0.f, "...").x;
        if (width <= dots) return {};
        const char* remaining = value.c_str();
        font->CalcTextSizeA(size, width - dots, 0.f, value.c_str(), nullptr, &remaining);
        value.resize(static_cast<std::size_t>(remaining - value.c_str()));
        return value + "...";
    }

    std::string fit_body(ImFont* font, float size, float width, const std::string& value)
    {
        if (value.empty()) return {};
        const char* begin = value.c_str();
        const char* end = begin + value.size();
        const char* split = font->CalcWordWrapPositionA(size / font->FontSize, begin, end, width);
        const auto newline = value.find('\n');
        if (newline != std::string::npos) split = std::min(split, begin + newline);
        if (split >= end) return fit_text(font, size, width, value);
        std::string first(begin, split);
        while (split < end && (*split == ' ' || *split == '\t' || *split == '\r' || *split == '\n')) ++split;
        if (split == end) return first;
        return first + '\n' + fit_text(font, size, width, std::string(split, end));
    }

    void glass_fill(ImDrawList* draw, ImVec2 minimum, ImVec2 maximum, ImVec4 color,
        ImVec4 accent, float opacity)
    {
        const int first = draw->VtxBuffer.Size;
        draw->AddRectFilled(minimum, maximum, alpha(color, opacity), card_rounding);
        const ImVec4 upper = tint(color, accent, .075f);
        for (int index = first; index < draw->VtxBuffer.Size; ++index)
        {
            auto& vertex = draw->VtxBuffer[index];
            const float t = std::clamp((vertex.pos.y - minimum.y) / (maximum.y - minimum.y), 0.f, 1.f);
            const ImU32 rgb = ImGui::ColorConvertFloat4ToU32(tint(upper, color, t));
            // Retain the rounded mesh's antialiasing instead of covering its corners.
            vertex.col = (vertex.col & IM_COL32_A_MASK) | (rgb & ~IM_COL32_A_MASK);
        }
    }

    void card_glow(ImDrawList* draw, ImVec2 minimum, ImVec2 maximum, ImVec4 color, float opacity)
    {
        // Overlapping low-alpha fills form a soft halo without visible outline rings.
        for (int layer = 12; layer >= 1; --layer)
        {
            const float spread = static_cast<float>(layer) * .8f;
            const float falloff = 1.f - static_cast<float>(layer) / 13.f;
            draw->AddRectFilled(ImVec2(minimum.x - spread, minimum.y - spread),
                ImVec2(maximum.x + spread, maximum.y + spread),
                alpha(color, (.008f + .024f * falloff * falloff) * opacity), card_rounding + spread);
        }
    }
}

void gui::notification_ui::renderer::render(const services::notifications::snapshot& snapshot,
    const ImGuiViewport& viewport, const appearance& style)
{
    namespace model = services::notifications;
    if (snapshot.entries.empty()) { clear(); return; }
    constexpr float margin = 16.f, title_size = 13.5f, body_size = 12.f, badge_size = 10.5f;
    const float maximum_width = std::min(264.f, viewport.Size.x - margin * 2.f);
    if (maximum_width < 120.f || viewport.Size.y < 64.f || !std::isfinite(maximum_width)) { clear(); return; }
    const int position = std::clamp(snapshot.settings.position, 0, 3);
    if (position_ != position) { animations_.clear(); position_ = position; }
    const bool left = position >= 2, top = position == 1 || position == 3;
    ImFont* font = style.font ? style.font : ImGui::GetFont();
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImVec4 background = tint(ImVec4(.035f, .037f, .048f, 1.f), style.accent, .025f);
    background.w = .985f;
    const ImVec4 text = ImGui::GetStyleColorVec4(ImGuiCol_Text);
    const ImVec4 muted = tint(ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled), text, .28f);
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
        const bool info = item.type == model::kind::info;
        const char* title = item.title.empty()
            ? (error ? (style.turkish ? "Başarısız" : "Failed")
                : info ? (style.turkish ? "Bildirim" : "Notification")
                       : (style.turkish ? "Tamamlandı" : "Completed")) : item.title.c_str();
        const char* badge = off ? (style.turkish ? "Kapalı" : "Off") : (style.turkish ? "Açık" : "On");
        const float badge_width = module ? font->CalcTextSizeA(badge_size, FLT_MAX, 0.f, badge).x + 16.f : 0.f;
        const float width = maximum_width;
        const float wrap = width - 58.f;
        const std::string heading = fit_text(font, title_size, wrap - (module ? badge_width + 12.f : 0.f), title);
        const std::string body = module ? std::string{} : fit_body(font, body_size, wrap, item.body);
        const float body_height = body.empty() ? 0.f : font->CalcTextSizeA(body_size, FLT_MAX, 0.f, body.c_str()).y;
        const float height = body.empty() ? 46.f : 40.f + body_height;
        if (offset + height > viewport.Size.y - margin * 2.f) break;
        alive.insert(item.id);
        auto [it, inserted] = animations_.try_emplace(item.id, state{offset, 0});
        auto& anim = it->second;
        anim.offset = model::approach(anim.offset, offset, dt, style.reduce_motion);
        // A refreshed duplicate may revive a fading card. Ease that increase,
        // but keep exit opacity analytic so removal never cuts a visible tail.
        anim.alpha = motion.alpha < anim.alpha ? motion.alpha :
            model::approach(anim.alpha, motion.alpha, dt, style.reduce_motion);
        const float opacity = anim.alpha;
        const float slide = style.reduce_motion ? 0.f : motion.slide * 18.f;
        const float x = viewport.Pos.x + (left ? margin - slide : viewport.Size.x - width - margin + slide);
        const float y = viewport.Pos.y + (top ? margin + anim.offset : viewport.Size.y - margin - height - anim.offset);
        offset += height + 10.f;
        if (opacity <= .001f) continue;
        const ImVec2 minimum(x, y), maximum(x + width, y + height);
        ImVec4 semantic = error ? tint(style.accent, ImVec4(.96f, .31f, .39f, 1.f), .8f) : style.accent;
        if (off) semantic = tint(style.accent, muted, .45f);
        draw->AddRectFilled(ImVec2(x - 2.f, y + 3.f), ImVec2(maximum.x + 2.f, maximum.y + 5.f),
            IM_COL32(0, 0, 0, static_cast<int>(80 * opacity)), card_rounding + 2.f);
        card_glow(draw, minimum, maximum, semantic, opacity);
        glass_fill(draw, minimum, maximum, background, semantic, opacity);
        draw->AddRect(minimum, maximum, alpha(semantic, .48f * opacity), card_rounding, 0, 1.f);
        draw->AddRect(ImVec2(x + 1.f, y + 1.f), ImVec2(maximum.x - 1.f, maximum.y - 1.f),
            IM_COL32(255, 255, 255, static_cast<int>(10 * opacity)), card_rounding - 1.f);
        const ImVec2 center(x + 24.f, y + (body.empty() ? height * .5f : 23.f));
        draw->AddRectFilled(ImVec2(center.x - 11.f, center.y - 11.f),
            ImVec2(center.x + 11.f, center.y + 11.f), alpha(semantic, .15f * opacity), 3.f);
        draw->AddRect(ImVec2(center.x - 11.f, center.y - 11.f),
            ImVec2(center.x + 11.f, center.y + 11.f), alpha(semantic, .24f * opacity), 3.f);
        const ImU32 icon = alpha(semantic, opacity);
        if (error)
        {
            draw->AddLine(ImVec2(center.x - 2.5f, center.y - 2.5f), ImVec2(center.x + 2.5f, center.y + 2.5f), icon, 1.5f);
            draw->AddLine(ImVec2(center.x - 2.5f, center.y + 2.5f), ImVec2(center.x + 2.5f, center.y - 2.5f), icon, 1.5f);
        }
        else if (off) draw->AddLine(ImVec2(center.x - 3, center.y), ImVec2(center.x + 3, center.y), icon, 1.5f);
        else if (info)
        {
            draw->AddCircleFilled(ImVec2(center.x, center.y - 3.f), 1.f, icon);
            draw->AddLine(ImVec2(center.x, center.y), ImVec2(center.x, center.y + 3.f), icon, 1.5f);
        }
        else
        {
            draw->AddLine(ImVec2(center.x - 3.5f, center.y), ImVec2(center.x - 1, center.y + 2.5f), icon, 1.5f);
            draw->AddLine(ImVec2(center.x - 1, center.y + 2.5f), ImVec2(center.x + 4, center.y - 3), icon, 1.5f);
        }
        const ImVec4 clip(x + 46.f, y + 7.f, maximum.x - 12.f, maximum.y - 7.f);
        const float title_y = body.empty() ? center.y - title_size * .5f : y + 11.f;
        draw->AddText(font, title_size, ImVec2(x + 46.f, title_y), alpha(text, opacity), heading.c_str(), nullptr, 0.f, &clip);
        if (!body.empty())
            draw->AddText(font, body_size, ImVec2(x + 46.f, y + 30.f), alpha(muted, opacity), body.c_str(), nullptr, 0.f, &clip);
        if (module)
        {
            const float badge_x = maximum.x - 12.f - badge_width;
            draw->AddRectFilled(ImVec2(badge_x, center.y - 9.f), ImVec2(maximum.x - 12.f, center.y + 9.f),
                alpha(semantic, .16f * opacity), 3.f);
            draw->AddText(font, badge_size, ImVec2(badge_x + 8.f, center.y - badge_size * .5f),
                alpha(semantic, opacity), badge);
        }
    }
    draw->PopClipRect();
    std::erase_if(animations_, [&](const auto& pair) { return !alive.contains(pair.first); });
}
