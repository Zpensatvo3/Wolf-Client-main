#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "hud_editor.hpp"
#include "hud_editor_layout.hpp"
#include "../gui/gui.hpp"
#include "../gui/ui_state.hpp"
#include "../gui/watermark_draw.hpp"
#include "../modules/core/module_registry.hpp"
#include "../modules/render/arraylist.hpp"
#include "../modules/visual/spotify_hud.hpp"
#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <vector>

namespace
{
    namespace layout = gui::hud_editor_layout;
    using snapshot = std::array<gui::aegis::hud_placement, 3>;
    constexpr const char* ids[] = { "watermark", "arraylist", "spotify_hud" };
    constexpr const char* names[] = { "Watermark", "ArrayList", "Spotify" };
    int selected = 0, dragging = -1;
    bool grid = false, snap = true, restored = false;
    ImVec2 drag_offset{};
    std::vector<snapshot> history;

    modules::spotify_hud* spotify_module()
    {
        return dynamic_cast<modules::spotify_hud*>(modules::find_module_by_id(ids[2]));
    }
    snapshot capture()
    {
        snapshot result{{ {gui::hud_pos_x, gui::hud_pos_y, gui::hud_scale},
            {gui::arraylist_pos_x, gui::arraylist_pos_y, gui::arraylist_scale}, {22, 60, 1.2f} }};
        if (auto* spotify = spotify_module()) { result[2].x = spotify->get_pos_x(); result[2].y = spotify->get_pos_y(); }
        for (int index = 0; index < 3; ++index)
            if (auto* module = modules::find_module_by_id(ids[index])) result[index].visible = module->enabled;
        return result;
    }
    void apply(const snapshot& value)
    {
        gui::hud_pos_x = value[0].x; gui::hud_pos_y = value[0].y; gui::hud_scale = value[0].scale;
        gui::arraylist_pos_x = value[1].x; gui::arraylist_pos_y = value[1].y; gui::arraylist_scale = value[1].scale;
        if (auto* spotify = spotify_module()) { spotify->set_pos_x(value[2].x); spotify->set_pos_y(value[2].y); }
        for (int index = 0; index < 3; ++index)
            if (auto* module = modules::find_module_by_id(ids[index])) module->enabled = value[index].visible;
    }
    void save()
    {
        auto& preferences = gui::aegis::get_preferences();
        preferences.hud_layout = capture(); preferences.has_hud_layout = true;
        gui::aegis::mark_preferences_dirty();
    }
    void remember()
    {
        if (history.size() >= 32) history.erase(history.begin());
        history.push_back(capture());
    }
    void undo()
    {
        if (history.empty()) return;
        apply(history.back()); history.pop_back(); save(); dragging = -1;
    }
    void set_position(int index, layout::point position, ImVec2 size, ImVec2 canvas)
    {
        position = layout::clamp(position, {size.x, size.y}, {canvas.x, canvas.y});
        if (index == 0) { gui::hud_pos_x = position.x; gui::hud_pos_y = position.y; }
        if (index == 1) { gui::arraylist_pos_x = std::max(8.f, canvas.x - position.x - size.x); gui::arraylist_pos_y = position.y; }
        if (index == 2) if (auto* spotify = spotify_module()) { spotify->set_pos_x(position.x); spotify->set_pos_y(position.y); }
        save();
    }
    void draw_editor_text(ImDrawList* draw, const ImVec2& position, float size, ImU32 color, const char* value)
    {
        ImFont* font = gui::ui_font ? gui::ui_font : ImGui::GetFont();
        draw->AddText(font, size, position, color, value);
    }
    bool button(const char* id, const char* label, ImVec2 position, ImVec2 size, bool active = false, bool disabled = false)
    {
        ImGui::SetCursorScreenPos(position);
        ImGui::BeginDisabled(disabled);
        const bool pressed = ImGui::InvisibleButton(id, size);
        const bool hovered = ImGui::IsItemHovered();
        const ImVec4 tint = gui::get_dynamic_color().Value;
        ImDrawList* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y),
            active ? ImGui::GetColorU32(ImVec4(tint.x, tint.y, tint.z, .22f))
                : ImGui::GetColorU32(ImVec4(1, 1, 1, hovered ? .08f : .035f)), 6.f);
        if (active) draw->AddRect(position, ImVec2(position.x + size.x, position.y + size.y),
            ImGui::GetColorU32(ImVec4(tint.x, tint.y, tint.z, .5f)), 6.f);
        ImFont* font = gui::ui_font ? gui::ui_font : ImGui::GetFont();
        const float font_size = size.x < 65 ? 10.f : 12.f;
        const ImVec2 extent = font->CalcTextSizeA(font_size, FLT_MAX, 0, label);
        const ImVec4 clip(position.x + 2, position.y, position.x + size.x - 2, position.y + size.y);
        draw->AddText(font, font_size, ImVec2(position.x + (size.x - extent.x) * .5f, position.y + (size.y - extent.y) * .5f),
            ImGui::GetColorU32(ImGuiCol_Text), label, nullptr, 0, &clip);
        ImGui::EndDisabled();
        return pressed;
    }
}

void gui::hud_editor::restore_saved_layout()
{
    if (restored || !modules::find_module_by_id(ids[2])) return;
    restored = true;
    const auto& preferences = gui::aegis::get_preferences();
    if (preferences.has_hud_layout) apply(preferences.hud_layout);
}

void gui::hud_editor::close()
{
    dragging = -1;
    gui::hud_layout_editor_active = false; gui::hud_editor_active = false;
    gui::aegis::flush_preferences(true);
}

void gui::hud_editor::render(const ImGuiViewport* viewport)
{
    if (!viewport || viewport->Size.x < 1 || viewport->Size.y < 1) return;
    restore_saved_layout();
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { close(); return; }
    if (ImGui::GetIO().KeyCtrl && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && ImGui::IsKeyPressed(ImGuiKey_Z, false)) undo();
    const ImVec2 canvas = viewport->Size, origin = viewport->Pos;
    const float toolbar_width = std::min(720.f, std::max(1.f, canvas.x - 24.f));
    const ImVec2 toolbar(origin.x + (canvas.x - toolbar_width) * .5f, origin.y + std::max(0.f, canvas.y - 124.f));
    const ImVec2 toolbar_max(toolbar.x + toolbar_width, toolbar.y + 112.f);
    ImGui::SetNextWindowPos(origin); ImGui::SetNextWindowSize(canvas); ImGui::SetNextWindowBgAlpha(0);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0);
    if (ImGui::Begin("##HudLayoutEditor", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImU32 accent = gui::get_dynamic_color();
        draw->AddRectFilled(origin, ImVec2(origin.x + canvas.x, origin.y + canvas.y), IM_COL32(0, 0, 0, 42));
        if (grid)
            for (float y = 8; y < canvas.y; y += 16)
                for (float x = 8; x < canvas.x; x += 16)
                    draw->AddCircleFilled(ImVec2(origin.x + x, origin.y + y), .65f, IM_COL32(255, 255, 255, 24), 4);
        const float header_width = std::max(1.f, std::min(440.f, canvas.x - 16.f));
        const ImVec2 header(origin.x + (canvas.x - header_width) * .5f, origin.y + 12);
        draw->AddRectFilled(header, ImVec2(header.x + header_width, header.y + 47), IM_COL32(14, 17, 23, 225), 9);
        draw_editor_text(draw, ImVec2(header.x + 14, header.y + 7), 14, IM_COL32(240, 243, 249, 255), gui::text("HUD Editor", "HUD Editörü"));
        draw_editor_text(draw, ImVec2(header.x + 14, header.y + 28), 10.5f, IM_COL32(154, 161, 176, 255),
            gui::text("Drag / arrows: 1px / Shift: 10px / Alt: free / Ctrl+Z: undo", "Sürükle / oklar: 1px / Shift: 10px / Alt: serbest / Ctrl+Z: geri al"));

        struct item { ImVec2 position{}, size{}; int first = 0, last = 0; modules::module* module = nullptr; };
        std::array<item, 3> items{};
        // Capture actual drawing bounds once. The editor never substitutes mock shapes.
        for (int index = 0; index < 3; ++index)
        {
            auto& item = items[index]; item.module = modules::find_module_by_id(ids[index]);
            item.first = draw->VtxBuffer.Size;
            if (!item.module) continue;
            if (index == 0)
            {
                item.size = gui::watermark_draw::size();
                item.position = {gui::hud_pos_x, gui::hud_pos_y};
                gui::watermark_draw::draw(draw, item.position.x, item.position.y);
            }
            if (index == 1)
            {
                modules::arraylist::overlay_bounds bounds{};
                modules::arraylist::draw_overlay(draw, gui::arraylist_pos_x, gui::arraylist_pos_y, true, &bounds);
                item.position = {bounds.minimum_x, bounds.minimum_y}; item.size = {bounds.width(), bounds.height()};
            }
            if (index == 2) if (auto* spotify = spotify_module())
            {
                const auto bounds = spotify->get_bounds(canvas.x, canvas.y);
                item.position = {bounds.x, bounds.y}; item.size = {bounds.w, bounds.h};
                spotify->render_hud_preview(draw, item.position.x, item.position.y);
            }
            item.last = draw->VtxBuffer.Size;
        }
        for (int index = 0; index < 3; ++index)
        {
            auto& item = items[index];
            if (!item.module || item.size.x < 1 || item.size.y < 1) continue;
            const ImVec2 before = item.position;
            auto bounded = layout::clamp({before.x, before.y}, {item.size.x, item.size.y}, {canvas.x, canvas.y});
            item.position = {bounded.x, bounded.y};
            ImGui::SetCursorScreenPos(ImVec2(origin.x + item.position.x, origin.y + item.position.y));
            const ImVec2 mouse = ImGui::GetIO().MousePos;
            const bool over_toolbar = mouse.x >= toolbar.x && mouse.x <= toolbar_max.x && mouse.y >= toolbar.y && mouse.y <= toolbar_max.y;
            const bool over_header = mouse.x >= header.x && mouse.x <= header.x + header_width && mouse.y >= header.y && mouse.y <= header.y + 47;
            ImGui::PushID(index);
            ImGui::BeginDisabled(dragging < 0 && (over_toolbar || over_header));
            ImGui::InvisibleButton("##move_overlay", item.size);
            const bool hovered = ImGui::IsItemHovered();
            if (ImGui::IsItemActivated())
            {
                selected = index; dragging = index; remember();
                drag_offset = {mouse.x - origin.x - item.position.x, mouse.y - origin.y - item.position.y};
            }
            float guide_x = -1, guide_y = -1;
            if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            {
                std::vector<layout::rect> neighbours;
                for (int other = 0; other < 3; ++other) if (other != index && items[other].size.x > 0)
                    neighbours.push_back({items[other].position.x, items[other].position.y, items[other].size.x, items[other].size.y});
                const auto placed = layout::place({mouse.x - origin.x - drag_offset.x, mouse.y - origin.y - drag_offset.y},
                    {item.size.x, item.size.y}, {canvas.x, canvas.y}, snap, grid, neighbours, ImGui::GetIO().KeyAlt);
                item.position = {placed.position.x, placed.position.y}; guide_x = placed.guide_x; guide_y = placed.guide_y;
            }
            if (dragging == index && !ImGui::IsMouseDown(ImGuiMouseButton_Left)) dragging = -1;
            ImGui::EndDisabled(); ImGui::PopID();
            if (selected == index && dragging < 0 && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().WantTextInput)
            {
                ImVec2 delta{};
                const float step = ImGui::GetIO().KeyShift ? 10.f : 1.f;
                if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) delta.x -= step;
                if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) delta.x += step;
                if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) delta.y -= step;
                if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) delta.y += step;
                if (delta.x || delta.y)
                {
                    remember(); bounded = layout::clamp({item.position.x + delta.x, item.position.y + delta.y},
                        {item.size.x, item.size.y}, {canvas.x, canvas.y}); item.position = {bounded.x, bounded.y};
                }
            }
            if (item.position.x != before.x || item.position.y != before.y)
                set_position(index, {item.position.x, item.position.y}, item.size, canvas);
            const float dx = origin.x + item.position.x - before.x, dy = origin.y + item.position.y - before.y;
            for (int vertex = item.first; vertex < item.last; ++vertex)
            {
                auto& v = draw->VtxBuffer[vertex]; v.pos.x += dx; v.pos.y += dy;
                if (!item.module->enabled)
                {
                    const unsigned alpha = (v.col & IM_COL32_A_MASK) >> IM_COL32_A_SHIFT;
                    v.col = (v.col & ~IM_COL32_A_MASK) | (static_cast<unsigned>(alpha * .45f) << IM_COL32_A_SHIFT);
                }
            }
            const ImVec2 minimum(origin.x + item.position.x, origin.y + item.position.y);
            if (selected == index || hovered)
            {
                draw->AddRect(ImVec2(minimum.x - 3, minimum.y - 3), ImVec2(minimum.x + item.size.x + 3, minimum.y + item.size.y + 3),
                    selected == index ? accent : IM_COL32(255, 255, 255, 90), 5, 0, selected == index ? 1.5f : 1.f);
                if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
            if (guide_x >= 0) draw->AddLine(ImVec2(origin.x + guide_x, origin.y), ImVec2(origin.x + guide_x, origin.y + canvas.y), accent);
            if (guide_y >= 0) draw->AddLine(ImVec2(origin.x, origin.y + guide_y), ImVec2(origin.x + canvas.x, origin.y + guide_y), accent);
        }
        // Fixed controls are drawn last and cannot be covered by a dragged overlay.
        draw->AddRectFilled(toolbar, toolbar_max, IM_COL32(14, 17, 23, 244), 12);
        draw->AddRect(toolbar, toolbar_max, IM_COL32(255, 255, 255, 26), 12);
        const float slot = std::max(1.f, (toolbar_width - 24 - 30) / 6);
        auto location = [&](int column, int row) { return ImVec2(toolbar.x + 12 + column * (slot + 6), toolbar.y + 10 + row * 34); };
        const ImVec2 button_size(slot, 28);
        for (int index = 0; index < 3; ++index)
            if (button(ids[index], names[index], location(index, 0), button_size, selected == index, !items[index].module)) selected = index;
        if (button("##grid", gui::text("Grid", "Izgara"), location(3, 0), button_size, grid)) grid = !grid;
        if (button("##snap", gui::text("Snap", "Hizala"), location(4, 0), button_size, snap)) snap = !snap;
        if (button("##done", gui::text("Done", "Tamam"), location(5, 0), button_size, true)) close();
        auto* selected_module = items[selected].module;
        const bool scalable = selected < 2 && selected_module;
        float* scale = selected == 0 ? &gui::hud_scale : &gui::arraylist_scale;
        if (button("##visible", selected_module && selected_module->enabled ? gui::text("Visible", "Görünür") : gui::text("Hidden", "Gizli"),
            location(0, 1), button_size, selected_module && selected_module->enabled, !selected_module))
        { remember(); selected_module->enabled = !selected_module->enabled; save(); }
        if (button("##scale_down", "-", location(1, 1), button_size, false, !scalable || *scale <= .5f))
        { remember(); *scale = std::max(.5f, *scale - .05f); save(); }
        char scale_text[24]{};
        std::snprintf(scale_text, sizeof(scale_text), selected == 2 ? "1.20x" : "%.2fx", *scale);
        button("##scale_value", scale_text, location(2, 1), button_size, false, true);
        if (button("##scale_up", "+", location(3, 1), button_size, false, !scalable || *scale >= 2.f))
        { remember(); *scale = std::min(2.f, *scale + .05f); save(); }
        if (button("##undo", gui::text("Undo", "Geri al"), location(4, 1), button_size, false, history.empty())) undo();
        if (button("##reset", gui::text("Reset", "Sıfırla"), location(5, 1), button_size, false, !selected_module))
        {
            remember(); auto value = capture(); const bool visible = value[selected].visible;
            value[selected] = gui::aegis::preferences{}.hud_layout[selected]; value[selected].visible = visible;
            apply(value); save();
        }
        char status[192]{};
        std::snprintf(status, sizeof(status), "%s  |  X %.0f  Y %.0f  |  %s", names[selected], items[selected].position.x,
            items[selected].position.y, gui::text("Auto-save on / Esc: return", "Otomatik kayıt açık / Esc: geri dön"));
        draw_editor_text(draw, ImVec2(toolbar.x + 14, toolbar.y + 84), 11, IM_COL32(158, 166, 182, 255), status);
    }
    ImGui::End(); ImGui::PopStyleVar(2);
}
