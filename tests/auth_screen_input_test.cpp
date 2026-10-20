// The DLL now has a launcher-only status screen: no license input or submit action.
#include "../src/gui/auth_screen.cpp"
#include "../src/gui/surface_style.hpp"
#include <cassert>
#include <filesystem>
#include <iostream>
namespace {bool login_open=true;auth::client::view_state auth_state{auth::client::phase::verifying,"Waiting for launcher...","",true};}
namespace auth::client {
    bool login_required(){return login_open;}
    view_state snapshot(){return auth_state;}
}
namespace gui {bool centered_text_button(const char* text,const ImVec2& size){return ImGui::Button(text,size);}}
int main(){
    ImGui::CreateContext();gui::surface_style::configure(ImGui::GetStyle());
    auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.DisplaySize={900,600};io.DeltaTime=1.f/60.f;
    const auto fonts=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/fonts";
    gui::ui_font=io.Fonts->AddFontFromFileTTF((fonts/"Montserrat-SemiBold.ttf").string().c_str(),19.5f);
    gui::ui_bold_font=io.Fonts->AddFontFromFileTTF((fonts/"Montserrat-Bold.ttf").string().c_str(),22.5f);
    assert(gui::ui_font&&gui::ui_bold_font);io.FontDefault=gui::ui_font;
    unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
    auto frame=[] {ImGui::NewFrame();gui::render_auth_screen();ImGui::Render();};
    frame();frame();assert(GImGui->InputTextState.ID==0);
    io.AddKeyEvent(ImGuiKey_Enter,true);frame();io.AddKeyEvent(ImGuiKey_Enter,false);frame();
    assert(GImGui->InputTextState.ID==0&&!gui::should_unload.load());
    io.DisplaySize={320,300};auth_state.status=auth::client::phase::error;
    auth_state.message="Start from the launcher. "+std::string(240,'W');
    frame();frame();frame();
    auto window=ImGui::FindWindowByName("##onestep_auth");
    assert(window&&window->Pos.x>=0&&window->Pos.y>=0);
    assert(window->Pos.x+window->Size.x<=io.DisplaySize.x+.1f);
    assert(window->Pos.y+window->Size.y<=io.DisplaySize.y+.1f);
    gui::clear_auth_screen();assert(GImGui->InputTextState.ID==0);
    ImGui::DestroyContext();std::cout<<"PASS launcher-only status, no credential input, compact layout\\n";
}
