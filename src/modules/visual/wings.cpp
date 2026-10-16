#include "wings.hpp"
#include "cosmetic_renderer.hpp"
#include "../../ui/settings_renderer.hpp"

void modules::wings::render_overlay()
{
    if (enabled) cosmetics::draw_wings(settings_.read());
}
void modules::wings::render_options()
{
    gui::settings::render(settings_, module_settings::wings::schema());
}
