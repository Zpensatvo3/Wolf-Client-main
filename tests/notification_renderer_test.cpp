#include "../src/ui/notification_renderer.hpp"
#include "../src/gui/theme_palette.hpp"
#include <cassert>
#include <iostream>

int main()
{
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(800, 600); io.DeltaTime = 1.f / 60;
    unsigned char* pixels = nullptr; int width = 0, height = 0;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
    ImGui::StyleColorsDark();
    gui::notification_ui::renderer renderer;
    gui::notification_ui::appearance style;
    style.reduce_motion = true;
    services::notifications::snapshot snapshot;
    snapshot.now = .5;
    snapshot.entries.push_back({1, "module:aim", "Aim Assist", {}, services::notifications::kind::module_on, 0, 0});
    auto render = [&] {
        ImGui::NewFrame();
        // No menu window is opened: the foreground notification must still draw.
        renderer.render(snapshot, *ImGui::GetMainViewport(), style);
        ImGui::Render();
        return ImGui::GetDrawData();
    };
    for (int theme = 0; theme < gui::theme::count; ++theme)
    {
        const auto accent = gui::theme::sample(theme, 0);
        style.accent = ImVec4(accent.r, accent.g, accent.b, 1);
        style.secondary = style.accent;
        const auto* data = render();
        assert(data && data->TotalVtxCount > 0);
        const auto expected = ImGui::ColorConvertFloat4ToU32(style.accent);
        bool themed = false;
        for (int list = 0; list < data->CmdListsCount; ++list)
            for (const auto& vertex : data->CmdLists[list]->VtxBuffer)
                if (vertex.col == expected) themed = true;
        assert(themed); // Existing live-theme accent actually reaches the draw vertices.
    }
    for (int position = 0; position < 4; ++position)
    {
        snapshot.settings.position = position;
        assert(render()->TotalVtxCount > 0);
    }
    for (auto type : {services::notifications::kind::success, services::notifications::kind::error,
                      services::notifications::kind::module_off})
    {
        snapshot.entries[0].type = type;
        snapshot.entries[0].body = "Config saved. This longer result should wrap inside the card without leaving its bounds.";
        assert(render()->TotalVtxCount > 0);
    }
    style.reduce_motion = false; renderer.clear(); snapshot.now = 0;
    assert(render()->TotalVtxCount == 0); // Invisible entrance starts outside the card anchor.
    snapshot.now = .3;
    assert(render()->TotalVtxCount > 0);
    snapshot.now = 5;
    assert(render()->TotalVtxCount == 0);
    snapshot.entries.clear();
    assert(render()->TotalVtxCount == 0);
    ImGui::DestroyContext();
    std::cout << "Notification renderer: menu-closed drawing, all eight themes, four corners, entrance and expiration passed\n";
}
