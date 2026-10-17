#include "module.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/control_draw.hpp"
#include <imgui/imgui.h>
#include <algorithm>
#include <cctype>
#include <cmath>

modules::module::module(const char* name, const char* category, const char* id)
    : id_storage(id && *id ? id : make_stable_id(name)), name(name), category(category)
{
}

void modules::module::run(::cache&) {}
void modules::module::on_enable(::cache&) {}
void modules::module::on_disable(::cache&) {}
void modules::module::render_options() {}

std::string modules::module::export_config_settings() const
{
    return serialize_config_fields(const_cast<module&>(*this));
}

bool modules::module::apply_config_settings(const std::string& object)
{
    return deserialize_config_fields(*this, object);
}

bool modules::module::validate_config_settings(const std::string& object) const
{
    return validate_config_fields(const_cast<module&>(*this), object);
}

int modules::module::get_keybind() const { return keybind; }
void modules::module::set_keybind(int value) { keybind = value >= 0 && value <= 255 ? value : 0; }
const char* modules::module::get_id() const { return id_storage.c_str(); }
const char* modules::module::get_name() const { return name; }
const char* modules::module::get_category() const { return category; }

void modules::module::begin_keybind_capture()
{
    scanning = true;
    gui::reset_last_pressed_key();
}

std::string modules::module::make_stable_id(const char* value)
{
    std::string id;
    bool underscore_pending = false;
    for (const unsigned char character : std::string(value ? value : "module"))
    {
        if (std::isalnum(character))
        {
            if (underscore_pending && !id.empty()) id.push_back('_');
            id.push_back(static_cast<char>(std::tolower(character)));
            underscore_pending = false;
        }
        else
        {
            underscore_pending = !id.empty();
        }
    }
    return id.empty() ? "module" : id;
}

void modules::module::key_bind_selector()
{
    utils::null_string display = (scanning ? "LISTENING" : keycode_to_string(keybind));
    const ImVec2 button_size(72.0f, 26.0f);
    const ImVec2 button_min = ImGui::GetCursorScreenPos();
    ImGui::PushID(get_id());
    ImGui::InvisibleButton("##keybind", button_size);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool pressed = ImGui::IsItemClicked();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddRectFilled(button_min, ImVec2(button_min.x + button_size.x, button_min.y + button_size.y),
        ImGui::GetColorU32(held ? ImGuiCol_ButtonActive : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button)),
        ImGui::GetStyle().FrameRounding);
    ImFont* label_font = gui::ui_font ? gui::ui_font : ImGui::GetFont();
    gui::control_draw::centered_label(draw_list, label_font, 14.5f, button_min,
        ImVec2(button_min.x + button_size.x, button_min.y + button_size.y),
        ImGui::GetColorU32(ImGuiCol_Text), display.data);
    ImGui::PopID();
    if (pressed) begin_keybind_capture();
    if (scanning)
    {
        int last_pressed = gui::get_last_pressed_key();
        if (!last_pressed) return;
        keybind = last_pressed == VK_ESCAPE ? 0 : last_pressed;
        scanning = false;
    }
}

utils::null_string modules::module::keycode_to_string(int keycode) const
{
    if (!keycode) return "none";
    switch (keycode)
    {
    case VK_MENU: case VK_LMENU: return "LALT";
    case VK_RMENU: return "RALT";
    case VK_SPACE: return "SPACE";
    case VK_LBUTTON: return "LBUTTON";
    case VK_RBUTTON: return "RBUTTON";
    case VK_MBUTTON: return "MBUTTON";
    case VK_TAB: return "TAB";
    }
    const char character = static_cast<char>(MapVirtualKeyA(keycode, MAPVK_VK_TO_CHAR));
    if (!character) return "NONE";
    utils::null_string result{"a"};
    result.data[0] = character;
    return result;
}
