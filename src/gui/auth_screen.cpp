#include "auth_screen.hpp"
#include "gui.hpp"
#include "ui_typography.hpp"
#include "../auth/client.hpp"
#include <algorithm>
void gui::clear_auth_screen() {}
void gui::render_auth_screen() {
    if(!auth::client::login_required())return;
    const auto state=auth::client::snapshot();
    const auto* viewport=ImGui::GetMainViewport();
    const float width=(std::max)(1.f,(std::min)(408.f,viewport->WorkSize.x-24.f));
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x+viewport->WorkSize.x*.5f,
        viewport->WorkPos.y+viewport->WorkSize.y*.5f),ImGuiCond_Always,ImVec2(.5f,.5f));
    ImGui::SetNextWindowSize(ImVec2(width,0));
    ImGui::SetNextWindowSizeConstraints(ImVec2(width,0),ImVec2(width,(std::max)(1.f,viewport->WorkSize.y-24.f)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,ImVec2(24,24));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,16.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,ImVec2(8,14));
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize,6.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg,ImVec4(.045f,.047f,.065f,.99f));
    constexpr auto flags=ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|
        ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings;
    if(ImGui::Begin("##onestep_auth",nullptr,flags)){
        {typography::scoped_font heading(ui_bold_font,24.f);ImGui::TextUnformatted("onestep");}
        typography::scoped_font controls(ui_font,17.f);
        ImGui::TextWrapped("%s",state.message.empty()?"Open the current OneStep launcher to start this client.":state.message.c_str());
        if(centered_text_button("Close client",ImVec2(ImGui::GetContentRegionAvail().x,38.f)))
            gui::should_unload.store(true,std::memory_order_release);
    }
    ImGui::End();ImGui::PopStyleColor();ImGui::PopStyleVar(4);
}
