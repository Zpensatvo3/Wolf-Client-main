#pragma once
#include <imgui/imgui.h>
#include <algorithm>

namespace gui::surface_style
{
    inline void configure(ImGuiStyle& style)
    {
        style.AntiAliasedLines = true;
        style.AntiAliasedLinesUseTex = true;
        style.AntiAliasedFill = true;
        style.ScrollbarSize = 0.f;
        style.CurveTessellationTol = .15f;
        style.CircleTessellationMaxError = .08f;
    }

    inline ImU32 mix(ImU32 a, ImU32 b, float t)
    {
        const ImVec4 first = ImGui::ColorConvertU32ToFloat4(a), second = ImGui::ColorConvertU32ToFloat4(b);
        t = std::clamp(t, 0.f, 1.f);
        return ImGui::ColorConvertFloat4ToU32(ImVec4(first.x + (second.x - first.x) * t,
            first.y + (second.y - first.y) * t, first.z + (second.z - first.z) * t,
            first.w + (second.w - first.w) * t));
    }

    inline ImU32 selected_fill(ImU32 accent)
    {
        ImVec4 color = ImGui::ColorConvertU32ToFloat4(mix(IM_COL32(20, 23, 29, 255), accent, .16f));
        color.w = 225.f / 255.f;
        return ImGui::ColorConvertFloat4ToU32(color);
    }

    inline void module_card(ImDrawList* draw, ImVec2 minimum, ImVec2 maximum, ImU32 accent, float hover, float selected)
    {
        ImU32 fill = mix(IM_COL32(18, 20, 25, 170), IM_COL32(32, 35, 42, 210), hover);
        fill = mix(fill, selected_fill(accent), selected);
        const int first_vertex = draw->VtxBuffer.Size;
        draw->AddRectFilled(minimum, maximum, fill, 9.f);
        // Shade the existing rounded mesh; a rectangular overlay would spill into its corners.
        const ImVec4 base = ImGui::ColorConvertU32ToFloat4(fill);
        for (int index = first_vertex; index < draw->VtxBuffer.Size; ++index)
        {
            ImDrawVert& vertex = draw->VtxBuffer[index];
            const float t = std::clamp((vertex.pos.y - minimum.y) / std::max(1.f, maximum.y - minimum.y), 0.f, 1.f);
            const float shade = 1.02f - .12f * t;
            const ImU32 rgb = ImGui::ColorConvertFloat4ToU32(ImVec4(base.x * shade, base.y * shade, base.z * shade, 1));
            vertex.col = (vertex.col & IM_COL32_A_MASK) | (rgb & ~IM_COL32_A_MASK);
        }
        ImU32 border = mix(IM_COL32(255, 255, 255, 12), IM_COL32(255, 255, 255, 28), hover);
        ImVec4 tint = ImGui::ColorConvertU32ToFloat4(accent); tint.w = .65f;
        border = mix(border, ImGui::ColorConvertFloat4ToU32(tint), selected);
        draw->AddRect(minimum, maximum, border, 8.5f, 0, selected > .1f ? 1.2f : 1.f);
    }
}
