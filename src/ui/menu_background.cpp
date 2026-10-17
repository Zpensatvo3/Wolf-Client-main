#include "menu_background.hpp"
#include "../gui/ui_state.hpp"

bool gui::background::render(ImDrawList* draw, ImVec2 minimum, ImVec2 size, const options& raw)
{
    options value = raw;
    normalize(value);
    if (!draw || value.selected == style::classic || value.opacity == 0) return false;
    const auto image = texture(value.selected);
    if (!image.id) return false;
    const auto uv = cover_uv(image.size, size);
    const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
    draw->AddImageRounded(image.id, minimum, maximum, uv.minimum, uv.maximum,
        IM_COL32(255, 255, 255, static_cast<int>(255 * value.opacity)), 20.f);
    draw->AddRectFilled(minimum, maximum,
        IM_COL32(3, 3, 5, static_cast<int>(255 * value.dimming)), 20.f);
    return true;
}

void gui::background::render_settings(ImU32 accent)
{
    ImGui::PushID("menu_background");
    auto& value = aegis::get_preferences().menu_background;
    ImGui::TextUnformatted("Menu background");
    ImGui::TextDisabled("%s", "Local only. Your gameplay configs are unchanged.");
    ImGui::Spacing();

    const char* labels[] = {
        "Classic", "Citadel",
        "Candy Dusk", "Nebula", "Aurora",
        "My image"
    };
    const float gap = 10.f, width = ImGui::GetContentRegionAvail().x;
    const int columns = std::clamp(static_cast<int>((width + gap) / 170.f), 1, 3);
    const ImVec2 card(std::max(1.f, (width - gap * (columns - 1)) / columns), 104.f);
    const ImVec2 start = ImGui::GetCursorScreenPos();
    for (int index = 0; index < static_cast<int>(style::count); ++index)
    {
        ImGui::PushID(index);
        const auto selected = static_cast<style>(index);
        const auto image = selected == style::classic ? texture_view{} : texture(selected);
        const bool available = selected == style::classic || image.id;
        const ImVec2 minimum(start.x + (index % columns) * (card.x + gap),
            start.y + (index / columns) * (card.y + gap));
        const ImVec2 maximum(minimum.x + card.x, minimum.y + card.y);
        ImGui::SetCursorScreenPos(minimum);
        ImGui::BeginDisabled(!available);
        if (ImGui::InvisibleButton("##select", card))
        {
            value.selected = selected;
            aegis::mark_preferences_dirty();
        }
        const bool hovered = ImGui::IsItemHovered();
        ImGui::EndDisabled();
        auto* draw = ImGui::GetWindowDrawList();
        draw->AddRectFilled(minimum, maximum, IM_COL32(14, 15, 21, 245), 9.f);
        if (image.id)
        {
            const auto uv = cover_uv(image.size, card);
            draw->AddImageRounded(image.id, minimum, maximum, uv.minimum, uv.maximum,
                IM_COL32_WHITE, 9.f);
            draw->AddRectFilled(minimum, maximum, IM_COL32(0, 0, 0, 55), 9.f);
        }
        draw->AddRectFilled(ImVec2(minimum.x, maximum.y - 29.f), maximum,
            IM_COL32(5, 6, 10, 210), 9.f, ImDrawFlags_RoundCornersBottom);
        draw->AddRect(minimum, maximum, value.selected == selected ? accent :
            IM_COL32(255, 255, 255, hovered ? 50 : 20), 9.f, 0, value.selected == selected ? 2.f : 1.f);
        draw->AddText(ImGui::GetFont(), 12.f,
            ImVec2(minimum.x + 10.f, maximum.y - 23.f), available ? IM_COL32_WHITE : IM_COL32(140, 140, 150, 255), labels[index]);
        if (value.selected == selected)
            draw->AddCircleFilled(ImVec2(maximum.x - 13.f, minimum.y + 13.f), 4.f, accent);
        ImGui::PopID();
    }
    const int rows = (static_cast<int>(style::count) + columns - 1) / columns;
    ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + rows * (card.y + gap)));

    const auto state = status();
    ImGui::BeginDisabled(state == import_state::choosing);
    if (ImGui::Button("Add image...")) begin_import();
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", "PNG / JPG · up to 16 MB");
    if (state == import_state::choosing)
        ImGui::TextDisabled("%s", "Choose an image in the file window.");
    else if (state == import_state::failed)
        ImGui::TextWrapped("%s", "Image could not be loaded. Use PNG/JPG, at most 8192 px per side and 16 megapixels.");
    else if (state == import_state::save_failed)
        ImGui::TextWrapped("%s", "Image could not be saved. Your previous background is unchanged.");
    else if (state == import_state::imported)
        ImGui::TextDisabled("%s", "Image saved on this computer.");
    else if (value.selected == style::custom && !texture(style::custom).id)
        ImGui::TextWrapped("%s", "Saved image is missing. Classic background is used until you add an image again.");

    ImGui::BeginDisabled(value.selected == style::classic);
    if (ImGui::SliderFloat("Image opacity", &value.opacity, 0.f, 1.f, "%.2f"))
        aegis::mark_preferences_dirty();
    if (ImGui::SliderFloat("Darkening", &value.dimming, .25f, .85f, "%.2f"))
        aegis::mark_preferences_dirty();
    ImGui::EndDisabled();
    ImGui::PopID();
}
