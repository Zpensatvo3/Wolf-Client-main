#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "auth_screen.hpp"
#include "gui.hpp"
#include "../auth/client.hpp"
#include <imgui/imgui_internal.h>

#include <algorithm>
#include <array>

namespace
{
    std::array<char, 129> license{};
    ImGuiID input_id = 0;
}

void gui::clear_auth_screen()
{
    SecureZeroMemory(license.data(), license.size());
    if (!ImGui::GetCurrentContext() || !input_id) return;
    auto& context = *ImGui::GetCurrentContext();
    auto& input = context.InputTextState;
    if (input.ID == input_id)
    {
        if (context.ActiveId == input_id) ImGui::ClearActiveID();
        if (input.TextW.Data) SecureZeroMemory(input.TextW.Data, input.TextW.Capacity * sizeof(ImWchar));
        if (input.TextA.Data) SecureZeroMemory(input.TextA.Data, input.TextA.Capacity);
        if (input.InitialTextA.Data) SecureZeroMemory(input.InitialTextA.Data, input.InitialTextA.Capacity);
        SecureZeroMemory(&input.Stb, sizeof(input.Stb));
        input.ClearFreeMemory();
        input.ID = 0;
        input.CurLenA = input.CurLenW = 0;
    }
    // ImGui 1.90 also keeps a separate deactivation copy on focus changes.
    auto& deactivated = context.InputTextDeactivatedState;
    if (deactivated.ID == input_id)
    {
        if (deactivated.TextA.Data) SecureZeroMemory(deactivated.TextA.Data, deactivated.TextA.Capacity);
        deactivated.ClearFreeMemory();
    }
}

void gui::render_auth_screen()
{
    if (!auth::client::login_required()) { clear_auth_screen(); return; }
    const auto state = auth::client::snapshot();
    const bool busy = state.status == auth::client::phase::verifying;
    const auto* viewport = ImGui::GetMainViewport();
    const float width = std::max(220.f, std::min(380.f, viewport->WorkSize.x - 32.f));
    const ImVec2 center(viewport->WorkPos.x + viewport->WorkSize.x * .5f,
        viewport->WorkPos.y + viewport->WorkSize.y * .5f);
    ImGui::GetBackgroundDrawList()->AddRectFilled(viewport->Pos,
        ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y), IM_COL32(4, 5, 9, 175));

    ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(.5f, .5f));
    ImGui::SetNextWindowSize(ImVec2(width, 0.f));
    ImGui::SetNextWindowSizeConstraints(ImVec2(width, 0.f), ImVec2(width, std::max(180.f, viewport->WorkSize.y - 24.f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(24.f, 22.f));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12.f, 10.f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.f, 12.f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(.045f, .047f, .063f, .98f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(.5f, .5f, .7f, .18f));
    constexpr auto flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar;
    if (ImGui::Begin("##onestep_auth", nullptr, flags))
    {
        if (ui_bold_font) ImGui::PushFont(ui_bold_font);
        ImGui::TextUnformatted("onestep");
        if (ui_bold_font) ImGui::PopFont();
        ImGui::TextDisabled("%s", "License login");
        ImGui::Spacing();
        ImGui::BeginDisabled(busy || !state.configured);
        if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-1.f);
        input_id = ImGui::GetID("##license");
        const bool enter = ImGui::InputTextWithHint("##license", "License key",
            license.data(), license.size(), ImGuiInputTextFlags_Password | ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_NoUndoRedo);
        const bool clicked = centered_text_button("Sign in", ImVec2(ImGui::GetContentRegionAvail().x, 38.f));
        if (enter || clicked)
        {
            auth::client::submit_license(license.data());
            clear_auth_screen();
        }
        ImGui::EndDisabled();

        if (busy) ImGui::TextDisabled("%s", "Verifying license...");
        else if (!state.message.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, .5f, .53f, 1.f));
            ImGui::PushTextWrapPos(0.f);
            ImGui::TextUnformatted(state.message.c_str());
            ImGui::PopTextWrapPos();
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
        if (centered_text_button("Close client", ImVec2(ImGui::GetContentRegionAvail().x, 30.f)))
        {
            clear_auth_screen();
            gui::should_unload.store(true, std::memory_order_release);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(5);
}
