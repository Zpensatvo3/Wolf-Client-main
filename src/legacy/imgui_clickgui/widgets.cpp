#include "widgets.hpp"
#include "gui.hpp"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <type_traits>
#include <unordered_map>
#include <string>

namespace
{
    struct animation_state
    {
        float toggle = 0.0f;
        float hover = 0.0f;
        float popup = 0.0f;
        bool combo_open = false;
        bool combo_scroll_pending = false;
    };

    std::unordered_map<ImGuiID, animation_state> animations;
    int combo_tracking_frame = -1;
    bool previous_frame_mouse_click = false;
    bool current_frame_combo_interaction = false;

    constexpr float option_right_gutter = 0.0f;
    constexpr float toggle_height = 34.0f;
    constexpr float slider_height = 48.0f;

    struct toast_state
    {
        std::string message;
        bool success = true;
        double created_at = 0.0;
    };

    toast_state workspace_toast;

    const char* visible_end(const char* label)
    {
        const char* marker = std::strstr(label, "##");
        return marker ? marker : label + std::strlen(label);
    }

    float approach(float current, float target, float speed = 14.0f)
    {
        const float amount = 1.0f - std::exp(-speed * ImGui::GetIO().DeltaTime);
        return current + (target - current) * amount;
    }

    ImU32 with_alpha(const ImVec4& color, float alpha)
    {
        ImVec4 value = color;
        value.w *= alpha;
        return ImGui::ColorConvertFloat4ToU32(value);
    }

    void draw_text_right(ImDrawList* draw, ImFont* font, float size, const ImVec2& right,
        ImU32 color, const char* text)
    {
        const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
        draw->AddText(font, size, ImVec2(right.x - extent.x, right.y), color, text);
    }

    void draw_label_clipped(ImDrawList* draw, ImFont* font, float size, const ImVec2& position,
        float maximum_x, ImU32 color, const char* label)
    {
        const ImVec4 clip(position.x, position.y - 2.0f, maximum_x, position.y + size + 4.0f);
        draw->AddText(font, size, position, color, label, visible_end(label), 0.0f, &clip);
    }

    template <typename T>
    bool scalar_slider(const char* label, T* value, T minimum, T maximum, const char* format)
    {
        ImGui::PushID(label);
        const ImGuiID id = ImGui::GetID("##option_slider");
        animation_state& anim = animations[id];
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float width = std::max(120.0f, ImGui::GetContentRegionAvail().x - option_right_gutter);
        const float height = slider_height;
        ImGui::InvisibleButton("##option_slider", ImVec2(width, height));
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        anim.hover = approach(anim.hover, hovered || held ? 1.0f : 0.0f);

        const ImVec2 mouse = ImGui::GetIO().MousePos;
        const float track_left = start.x + 10.0f;
        const float track_right = start.x + width - 10.0f;
        const float track_y = start.y + 37.0f;
        bool changed = false;

        if (held && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            if (mouse.y >= start.y + 24.0f)
            {
                if (mouse.x < track_left)
                {
                    const T step = std::is_integral_v<T> ? T(1) : T((maximum - minimum) / T(100));
                    const T next = std::max(minimum, T(*value - step));
                    changed = next != *value;
                    *value = next;
                }
                else if (mouse.x > track_right)
                {
                    const T step = std::is_integral_v<T> ? T(1) : T((maximum - minimum) / T(100));
                    const T next = std::min(maximum, T(*value + step));
                    changed = next != *value;
                    *value = next;
                }
                else
                {
                    const float ratio = std::clamp((mouse.x - track_left) / (track_right - track_left), 0.0f, 1.0f);
                    T next = T(minimum + T((maximum - minimum) * ratio));
                    if constexpr (std::is_integral_v<T>)
                        next = T(std::lround(static_cast<double>(minimum) + static_cast<double>(maximum - minimum) * ratio));
                    next = std::clamp(next, minimum, maximum);
                    changed = next != *value;
                    *value = next;
                }
            }
        }

        char value_text[64]{};
        if constexpr (std::is_integral_v<T>)
            std::snprintf(value_text, sizeof(value_text), format ? format : "%d", static_cast<int>(*value));
        else
            std::snprintf(value_text, sizeof(value_text), format ? format : "%.3f", static_cast<double>(*value));

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const float font_size = ImGui::GetFontSize();
        const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrabActive);
        const ImU32 text_color = ImGui::GetColorU32(ImGuiCol_Text);
        const ImVec2 value_extent = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, value_text);
        const ImU32 surface = IM_COL32(255, 255, 255, hovered || held ? 10 : 6);
        draw->AddRectFilled(start, ImVec2(start.x + width, start.y + height), surface, 7.0f);
        draw_label_clipped(draw, font, 15.0f, ImVec2(start.x + 10.0f, start.y + 7.0f),
            start.x + width - value_extent.x - 24.0f, text_color, label);
        draw_text_right(draw, font, 14.0f, ImVec2(start.x + width - 10.0f, start.y + 7.0f), text_color, value_text);
		draw->AddRectFilled(ImVec2(track_left, track_y), ImVec2(track_right, track_y + 2.0f),
			IM_COL32(55, 59, 70, 220), 1.0f);
        const float ratio = std::clamp(static_cast<float>(*value - minimum) / static_cast<float>(maximum - minimum), 0.0f, 1.0f);
        const float thumb_x = track_left + (track_right - track_left) * ratio;
        draw->AddRectFilled(ImVec2(track_left, track_y), ImVec2(thumb_x, track_y + 2.0f), with_alpha(accent, 0.88f), 1.0f);
        draw->AddCircleFilled(ImVec2(thumb_x, track_y + 1.0f), 3.4f + anim.hover * 0.45f, with_alpha(accent, 1.0f));
        ImGui::PopID();
        return changed;
    }

    template <typename T>
    bool range_slider(const char* label, T value[2], T minimum, T maximum, const char* format)
    {
        ImGui::PushID(label);
        const ImGuiID id = ImGui::GetID("##option_range");
        animation_state& anim = animations[id];
        const ImVec2 start = ImGui::GetCursorScreenPos();
        const float width = std::max(120.0f, ImGui::GetContentRegionAvail().x - option_right_gutter);
        ImGui::InvisibleButton("##option_range", ImVec2(width, slider_height));
        const bool hovered = ImGui::IsItemHovered();
        const bool held = ImGui::IsItemActive();
        anim.hover = approach(anim.hover, hovered || held ? 1.0f : 0.0f);
        const float track_left = start.x + 10.0f;
        const float track_right = start.x + width - 10.0f;
        const float track_y = start.y + 37.0f;
        bool changed = false;

        if (held && ImGui::IsMouseDown(ImGuiMouseButton_Left))
        {
            const float ratio = std::clamp((ImGui::GetIO().MousePos.x - track_left) / (track_right - track_left), 0.0f, 1.0f);
            T next = T(minimum + T((maximum - minimum) * ratio));
            if constexpr (std::is_integral_v<T>)
                next = T(std::lround(static_cast<double>(minimum) + static_cast<double>(maximum - minimum) * ratio));
            next = std::clamp(next, minimum, maximum);
            const float low_ratio = static_cast<float>(value[0] - minimum) / static_cast<float>(maximum - minimum);
            const float high_ratio = static_cast<float>(value[1] - minimum) / static_cast<float>(maximum - minimum);
            const int selected = std::abs(ratio - low_ratio) <= std::abs(ratio - high_ratio) ? 0 : 1;
            if (selected == 0) next = std::min(next, value[1]);
            else next = std::max(next, value[0]);
            changed = next != value[selected];
            value[selected] = next;
        }

        char first[48]{}, second[48]{}, combined[104]{};
        if constexpr (std::is_integral_v<T>)
        {
            std::snprintf(first, sizeof(first), format ? format : "%d", static_cast<int>(value[0]));
            std::snprintf(second, sizeof(second), format ? format : "%d", static_cast<int>(value[1]));
        }
        else
        {
            std::snprintf(first, sizeof(first), format ? format : "%.3f", static_cast<double>(value[0]));
            std::snprintf(second, sizeof(second), format ? format : "%.3f", static_cast<double>(value[1]));
        }
        std::snprintf(combined, sizeof(combined), "%s  -  %s", first, second);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        ImFont* font = ImGui::GetFont();
        const float font_size = ImGui::GetFontSize();
        const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrabActive);
        const ImVec2 value_extent = font->CalcTextSizeA(font_size, FLT_MAX, 0.0f, combined);
        draw->AddRectFilled(start, ImVec2(start.x + width, start.y + slider_height),
            IM_COL32(255, 255, 255, hovered || held ? 10 : 6), 7.0f);
        draw_label_clipped(draw, font, 15.0f, ImVec2(start.x + 10.0f, start.y + 7.0f), start.x + width - value_extent.x - 24.0f,
            ImGui::GetColorU32(ImGuiCol_Text), label);
        draw_text_right(draw, font, 14.0f, ImVec2(start.x + width - 10.0f, start.y + 7.0f), ImGui::GetColorU32(ImGuiCol_Text), combined);
        draw->AddRectFilled(ImVec2(track_left, track_y), ImVec2(track_right, track_y + 2.0f), IM_COL32(55, 59, 70, 220), 1.0f);
        const float low_x = track_left + (track_right - track_left) * std::clamp(static_cast<float>(value[0] - minimum) / static_cast<float>(maximum - minimum), 0.0f, 1.0f);
        const float high_x = track_left + (track_right - track_left) * std::clamp(static_cast<float>(value[1] - minimum) / static_cast<float>(maximum - minimum), 0.0f, 1.0f);
        draw->AddRectFilled(ImVec2(low_x, track_y), ImVec2(high_x, track_y + 2.0f), with_alpha(accent, 0.88f), 1.0f);
        draw->AddCircleFilled(ImVec2(low_x, track_y + 1.0f), 3.4f + anim.hover * 0.45f, with_alpha(accent, 1.0f));
        draw->AddCircleFilled(ImVec2(high_x, track_y + 1.0f), 3.4f + anim.hover * 0.45f, with_alpha(accent, 1.0f));
        ImGui::PopID();
        return changed;
    }
}

bool gui::centered_text_button(const char* label, const ImVec2& size)
{
    const ImVec2 button_min = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(label, size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool pressed = ImGui::IsItemClicked();
    ImGui::GetWindowDrawList()->AddRectFilled(button_min, ImVec2(button_min.x + size.x, button_min.y + size.y),
        ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button)),
        ImGui::GetStyle().FrameRounding);
    const char* visible_end = std::strstr(label, "##");
    if (!visible_end) visible_end = label + std::strlen(label);
    const ImVec2 text_size = ImGui::CalcTextSize(label, visible_end);
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
        ImVec2(button_min.x + std::floor((size.x - text_size.x) * 0.5f), button_min.y + std::floor((size.y - text_size.y) * 0.5f)),
        ImGui::GetColorU32(ImGuiCol_Text), label, visible_end);
    return pressed;
}

ImVec2 gui::page_header(const char* title, const char* subtitle)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImFont* title_font = ui_bold_font ? ui_bold_font : ImGui::GetFont();
    const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    draw->AddRectFilled(start, ImVec2(start.x + 3.0f, start.y + 43.0f), with_alpha(accent, 0.88f), 1.5f);
    draw->AddText(title_font, 20.0f, ImVec2(start.x + 12.0f, start.y), IM_COL32(239, 240, 245, 255), title);
    if (subtitle && *subtitle)
        draw->AddText(ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(start.x + 12.0f, start.y + 24.0f),
            IM_COL32(116, 117, 128, 255), subtitle);
    ImGui::Dummy(ImVec2(ImGui::GetContentRegionAvail().x, 52.0f));
    return start;
}

bool gui::begin_surface(const char* id, const ImVec2& size, const ImVec2& padding)
{
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padding);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.067f, 0.076f, 0.097f, 0.96f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    return ImGui::BeginChild(id, size, ImGuiChildFlags_None);
}

void gui::end_surface()
{
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

bool gui::action_button(const char* label, const ImVec2& size, action_tone tone)
{
    ImVec4 normal(0.14f, 0.142f, 0.165f, 1.0f);
    ImVec4 hovered(0.18f, 0.182f, 0.21f, 1.0f);
    ImVec4 active(0.20f, 0.202f, 0.235f, 1.0f);
    if (tone == action_tone::accent)
    {
        const ImVec4 accent_color = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
        normal = ImVec4(accent_color.x, accent_color.y, accent_color.z, 0.86f);
        hovered = ImVec4(accent_color.x, accent_color.y, accent_color.z, 1.0f);
        active = ImVec4(accent_color.x * 0.82f, accent_color.y * 0.82f, accent_color.z * 0.82f, 1.0f);
    }
    else if (tone == action_tone::danger)
    {
        normal = ImVec4(0.43f, 0.075f, 0.11f, 0.90f);
        hovered = ImVec4(0.58f, 0.09f, 0.14f, 1.0f);
        active = ImVec4(0.36f, 0.055f, 0.085f, 1.0f);
    }
    ImGui::PushStyleColor(ImGuiCol_Button, normal);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hovered);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, active);
    const bool clicked = centered_text_button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

bool gui::icon_action(const char* id, const char* glyph, const char* fallback, const char* tooltip,
    bool danger, bool accent)
{
    ImGui::PushID(id);
    const ImVec2 position = ImGui::GetCursorScreenPos();
    const ImVec2 size(30.0f, 30.0f);
    ImGui::InvisibleButton("##icon_action", size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool clicked = ImGui::IsItemClicked();
    const ImVec4 accent_color = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    if (hovered || held || accent)
    {
        const ImU32 background = danger
            ? IM_COL32(126, 32, 48, held ? 118 : (hovered ? 82 : 44))
            : with_alpha(accent_color, held ? 0.24f : (hovered ? 0.15f : 0.09f));
        draw->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y), background, 7.0f);
    }
    ImFont* font = icon_font ? icon_font : ImGui::GetFont();
    const char* text = icon_font && glyph && *glyph ? glyph : fallback;
    const float text_size = icon_font ? 16.0f : 14.0f;
    const ImVec2 extent = font->CalcTextSizeA(text_size, FLT_MAX, 0.0f, text);
    const ImU32 color = danger
        ? (hovered ? IM_COL32(247, 87, 103, 255) : IM_COL32(147, 149, 159, 255))
        : (hovered || accent ? with_alpha(accent_color, 1.0f) : IM_COL32(149, 151, 163, 255));
    draw->AddText(font, text_size,
        ImVec2(position.x + std::floor((size.x - extent.x) * 0.5f),
            position.y + std::floor((size.y - extent.y) * 0.5f)), color, text);
    if (hovered && tooltip && *tooltip)
    {
        ImGui::BeginTooltip();
        ImGui::TextUnformatted(tooltip);
        ImGui::EndTooltip();
    }
    ImGui::PopID();
    return clicked;
}

void gui::push_workspace_toast(const std::string& message, bool success)
{
    workspace_toast.message = message;
    workspace_toast.success = success;
    workspace_toast.created_at = ImGui::GetTime();
}

void gui::render_workspace_toast(const ImVec2& window_min, const ImVec2& window_max)
{
    if (workspace_toast.message.empty()) return;
    const float age = static_cast<float>(ImGui::GetTime() - workspace_toast.created_at);
    if (age >= 3.4f)
    {
        workspace_toast.message.clear();
        return;
    }
    const float fade_in = std::clamp(age / 0.16f, 0.0f, 1.0f);
    const float fade_out = std::clamp((3.4f - age) / 0.28f, 0.0f, 1.0f);
    const float alpha = fade_in * fade_out;
    const ImVec2 text_extent = ImGui::CalcTextSize(workspace_toast.message.c_str());
    const ImVec2 size(std::min(360.0f, text_extent.x + 30.0f), 38.0f);
    const ImVec2 minimum(window_max.x - size.x - 16.0f,
        window_max.y - size.y - 16.0f + (1.0f - fade_in) * 8.0f);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(minimum, ImVec2(minimum.x + size.x, minimum.y + size.y),
        IM_COL32(20, 23, 30, static_cast<int>(242.0f * alpha)), 9.0f);
    const ImVec4 accent_color = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    const ImU32 indicator = workspace_toast.success
        ? with_alpha(accent_color, alpha)
        : IM_COL32(244, 79, 99, static_cast<int>(255.0f * alpha));
    draw->AddRectFilled(minimum, ImVec2(minimum.x + 3.0f, minimum.y + size.y), indicator, 9.0f,
        ImDrawFlags_RoundCornersLeft);
    const ImVec4 clip(minimum.x + 13.0f, minimum.y + 4.0f,
        minimum.x + size.x - 10.0f, minimum.y + size.y - 4.0f);
    draw->AddText(ImGui::GetFont(), 14.0f, ImVec2(minimum.x + 13.0f, minimum.y + 10.0f),
        IM_COL32(232, 234, 240, static_cast<int>(255.0f * alpha)), workspace_toast.message.c_str(),
        nullptr, 0.0f, &clip);
}

void gui::status_badge(const char* label, bool accent)
{
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const ImVec2 text_size = ImGui::CalcTextSize(label);
    const ImVec2 size(text_size.x + 15.0f, 21.0f);
    const ImVec4 accent_color = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(start, ImVec2(start.x + size.x, start.y + size.y),
        accent ? with_alpha(accent_color, 0.16f) : IM_COL32(34, 35, 42, 235), 10.5f);
    draw->AddRect(start, ImVec2(start.x + size.x, start.y + size.y),
        accent ? with_alpha(accent_color, 0.38f) : IM_COL32(255, 255, 255, 12), 10.5f);
    draw->AddText(ImVec2(start.x + 7.5f, start.y + 3.0f),
        accent ? with_alpha(accent_color, 0.96f) : IM_COL32(150, 151, 162, 255), label);
    ImGui::Dummy(size);
}

void gui::list_row_background(const ImVec2& minimum, const ImVec2& maximum, bool hovered, bool selected)
{
    const ImVec4 accent_color = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(minimum, maximum,
        selected ? with_alpha(accent_color, hovered ? 0.16f : 0.10f)
            : (hovered ? IM_COL32(29, 32, 40, 255) : IM_COL32(255, 255, 255, 5)), 7.0f);
    if (selected)
        draw->AddRect(minimum, maximum, with_alpha(accent_color, 0.28f), 7.0f);
}

bool gui::option_toggle(const char* label, bool* value)
{
    ImGui::PushID(label);
    const ImGuiID id = ImGui::GetID("##option_toggle");
    animation_state& anim = animations[id];
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = std::max(120.0f, ImGui::GetContentRegionAvail().x - option_right_gutter);
    ImGui::InvisibleButton("##option_toggle", ImVec2(width, toggle_height));
    const bool pressed = ImGui::IsItemClicked();
    if (pressed) *value = !*value;
    anim.toggle = approach(anim.toggle, *value ? 1.0f : 0.0f, 18.0f);
    anim.hover = approach(anim.hover, ImGui::IsItemHovered() ? 1.0f : 0.0f);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    ImFont* font = ImGui::GetFont();
    const float font_size = ImGui::GetFontSize();
    draw->AddRectFilled(start, ImVec2(start.x + width, start.y + toggle_height),
        IM_COL32(255, 255, 255, ImGui::IsItemHovered() ? 10 : 6), 7.0f);
    draw_label_clipped(draw, font, 15.0f, ImVec2(start.x + 10.0f, start.y + 7.0f), start.x + width - 54.0f,
        ImGui::GetColorU32(ImGuiCol_Text), label);
    const ImVec2 toggle_min(start.x + width - 43.0f, start.y + 7.0f);
    const ImVec2 toggle_max(start.x + width - 8.0f, start.y + 27.0f);
    const ImVec4 off = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
    const ImVec4 on = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    const ImVec4 mixed(off.x + (on.x - off.x) * anim.toggle,
        off.y + (on.y - off.y) * anim.toggle,
        off.z + (on.z - off.z) * anim.toggle, 1.0f);
    draw->AddRectFilled(toggle_min, toggle_max, ImGui::ColorConvertFloat4ToU32(mixed), 10.0f);
    draw->AddCircleFilled(ImVec2(toggle_min.x + 10.0f + 15.0f * anim.toggle, toggle_min.y + 10.0f),
        7.0f + anim.hover * 0.4f, IM_COL32(238, 238, 244, 255));
    ImGui::PopID();
    return pressed;
}

bool gui::option_slider(const char* label, float* value, float minimum, float maximum, const char* format, ImGuiSliderFlags)
{
    return scalar_slider(label, value, minimum, maximum, format);
}

bool gui::option_slider(const char* label, int* value, int minimum, int maximum, const char* format, ImGuiSliderFlags)
{
    return scalar_slider(label, value, minimum, maximum, format);
}

bool gui::option_range(const char* label, float value[2], float minimum, float maximum, const char* format, ImGuiSliderFlags)
{
    return range_slider(label, value, minimum, maximum, format);
}

bool gui::option_range(const char* label, int value[2], int minimum, int maximum, const char* format, ImGuiSliderFlags)
{
    return range_slider(label, value, minimum, maximum, format);
}

bool gui::option_combo(const char* label, int* current_item, const char* const items[], int item_count, int popup_max_height_in_items)
{
    if (!current_item || !items || item_count <= 0) return false;
    *current_item = std::clamp(*current_item, 0, item_count - 1);

    const int frame = ImGui::GetFrameCount();
    if (combo_tracking_frame != frame)
    {
        if (combo_tracking_frame >= 0 && previous_frame_mouse_click && !current_frame_combo_interaction)
        {
            for (auto& [unused_id, state] : animations)
                state.combo_open = false;
        }
        previous_frame_mouse_click = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        current_frame_combo_interaction = false;
        combo_tracking_frame = frame;
    }

    ImGui::PushID(label);
    const ImGuiID id = ImGui::GetID("##option_combo");
    animation_state& anim = animations[id];
    const ImVec2 start = ImGui::GetCursorScreenPos();
    const float width = std::max(120.0f, ImGui::GetContentRegionAvail().x - option_right_gutter);
    ImGui::InvisibleButton("##option_combo", ImVec2(width, toggle_height));
    const bool header_hovered = ImGui::IsItemHovered();
    const bool header_clicked = ImGui::IsItemClicked();
    bool just_opened = false;
    if (header_clicked)
    {
        anim.combo_open = !anim.combo_open;
        just_opened = anim.combo_open;
        if (just_opened)
            anim.combo_scroll_pending = true;
        current_frame_combo_interaction = true;
    }
    const float combo_delta = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f);
    const float combo_duration = anim.combo_open ? 0.18f : 0.15f;
    anim.popup = std::clamp(
        anim.popup + (anim.combo_open ? combo_delta : -combo_delta) / combo_duration,
        0.0f,
        1.0f);
    if (!anim.combo_open && anim.popup < 0.002f)
        anim.popup = 0.0f;
    anim.hover = approach(anim.hover, header_hovered || anim.combo_open ? 1.0f : 0.0f);
    const float eased_popup = anim.popup * anim.popup * (3.0f - 2.0f * anim.popup);

    const int visible_items = popup_max_height_in_items > 0
        ? std::min(item_count, popup_max_height_in_items)
        : item_count;
    const float full_list_height = 6.0f + static_cast<float>(visible_items) * 31.0f;
    const float list_height = full_list_height * eased_popup;

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_CheckMark);
    const ImVec2 surface_max(start.x + width, start.y + toggle_height + list_height);
    draw->AddRectFilled(start, surface_max, IM_COL32(255, 255, 255,
        static_cast<int>(6.0f + anim.hover * 5.0f)), 7.0f);
    if (list_height > 1.0f)
        draw->AddLine(
            ImVec2(start.x + 8.0f, start.y + toggle_height),
            ImVec2(start.x + width - 8.0f, start.y + toggle_height),
            IM_COL32(255, 255, 255, static_cast<int>(12.0f * eased_popup)));
    const char* selected = items[*current_item];
    const float right_padding = 29.0f;
    const ImVec2 selected_extent = ImGui::GetFont()->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, selected);
    draw_label_clipped(draw, ImGui::GetFont(), 15.0f, ImVec2(start.x + 10.0f, start.y + 7.0f),
        start.x + width - right_padding - selected_extent.x - 12.0f, ImGui::GetColorU32(ImGuiCol_Text), label);
    draw_text_right(draw, ImGui::GetFont(), 14.0f,
        ImVec2(start.x + width - right_padding, start.y + 7.0f), ImGui::GetColorU32(ImGuiCol_TextDisabled), selected);
    const float chevron_x = start.x + width - 15.0f;
    const float chevron_y = start.y + 18.0f;
    const float direction = eased_popup;
    draw->AddTriangleFilled(
        ImVec2(chevron_x - 4.0f, chevron_y - 2.0f + 4.0f * direction),
        ImVec2(chevron_x + 4.0f, chevron_y - 2.0f + 4.0f * direction),
        ImVec2(chevron_x, chevron_y + 3.0f - 5.0f * direction),
        with_alpha(accent, 0.95f));

    bool changed = false;
    if (list_height > 0.5f)
    {
        ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + toggle_height));
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(0.02f, eased_popup));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(5.0f, 3.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
        const ImGuiWindowFlags child_flags = visible_items < item_count
            ? ImGuiWindowFlags_AlwaysVerticalScrollbar
            : ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        if (ImGui::BeginChild("##combo_inline", ImVec2(width, list_height), ImGuiChildFlags_None, child_flags))
        {
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (1.0f - eased_popup) * 8.0f);
            for (int index = 0; index < item_count; ++index)
            {
                ImGui::PushID(index);
                const ImVec2 item_min = ImGui::GetCursorScreenPos();
                const float item_width = ImGui::GetContentRegionAvail().x;
                ImGui::InvisibleButton("##combo_item", ImVec2(item_width, 31.0f));
                const bool item_hovered = ImGui::IsItemHovered();
                const bool item_clicked = ImGui::IsItemClicked();
                const bool item_selected = index == *current_item;
                ImDrawList* item_draw = ImGui::GetWindowDrawList();
                if (item_selected || item_hovered)
                {
                    item_draw->AddRectFilled(
                        ImVec2(item_min.x + 1.0f, item_min.y + 2.0f),
                        ImVec2(item_min.x + item_width - 1.0f, item_min.y + 29.0f),
                        item_selected ? with_alpha(accent, 0.16f) : IM_COL32(255, 255, 255, 10),
                        5.0f);
                }
                item_draw->AddText(ImGui::GetFont(), 14.0f,
                    ImVec2(item_min.x + 8.0f, item_min.y + 7.0f),
                    item_selected ? with_alpha(accent, 0.98f) : ImGui::GetColorU32(ImGuiCol_Text),
                    items[index]);
                if (item_selected)
                    item_draw->AddCircleFilled(
                        ImVec2(item_min.x + item_width - 11.0f, item_min.y + 15.5f),
                        2.5f, with_alpha(accent, 0.95f), 12);
                if (item_clicked)
                {
                    *current_item = index;
                    changed = true;
                    anim.combo_open = false;
                    current_frame_combo_interaction = true;
                }
                if (item_clicked || (item_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)))
                    current_frame_combo_interaction = true;
                ImGui::PopID();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(4);
        ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + toggle_height + list_height + 5.0f));
        if (anim.combo_scroll_pending && anim.popup >= 0.999f)
        {
            ImGui::SetScrollHereY(0.82f);
            anim.combo_scroll_pending = false;
        }
    }
    ImGui::PopID();
    return changed;
}

void gui::option_section(const char* label)
{
    if (!label || !*label) return;
    ImGui::Dummy(ImVec2(0.0f, 4.0f));
    const ImVec2 position = ImGui::GetCursorScreenPos();
    ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), 13.0f, position,
        IM_COL32(132, 136, 149, 255), label, visible_end(label));
    ImGui::Dummy(ImVec2(0.0f, 17.0f));
}

bool ImGui::ThedoriusCheckbox(const char* label, bool* value) { return gui::option_toggle(label, value); }
bool ImGui::ThedoriusSliderFloat(const char* label, float* value, float minimum, float maximum, const char* format, ImGuiSliderFlags flags)
{ return gui::option_slider(label, value, minimum, maximum, format, flags); }
bool ImGui::ThedoriusSliderInt(const char* label, int* value, int minimum, int maximum, const char* format, ImGuiSliderFlags flags)
{ return gui::option_slider(label, value, minimum, maximum, format, flags); }
bool ImGui::ThedoriusSliderFloat2(const char* label, float value[2], float minimum, float maximum, const char* format, ImGuiSliderFlags flags)
{ return gui::option_range(label, value, minimum, maximum, format, flags); }
bool ImGui::ThedoriusSliderInt2(const char* label, int value[2], int minimum, int maximum, const char* format, ImGuiSliderFlags flags)
{ return gui::option_range(label, value, minimum, maximum, format, flags); }
bool ImGui::ThedoriusCombo(const char* label, int* current_item, const char* const items[], int item_count, int popup_max_height_in_items)
{ return gui::option_combo(label, current_item, items, item_count, popup_max_height_in_items); }
void ImGui::ThedoriusSeparatorText(const char* label) { gui::option_section(label); }
void ImGui::ThedoriusStaticDescription(const char*, ...) {}
