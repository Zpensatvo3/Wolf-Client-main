#include "hit_crystal.hpp"
#include "../../services/game/world_bridge.hpp"
#include "../../ui/settings_renderer.hpp"

void modules::hit_crystal::render_overlay()
{
    if (reset_requested_.exchange(false, std::memory_order_acq_rel)) cancel_on_client_thread();
    const auto settings = settings_.read();
    services::game::world_bridge::tick_crystal(enabled, settings.auto_switch, settings.restore_slot,
        settings.place_delay, settings.break_delay);
}

void modules::hit_crystal::cancel_on_client_thread()
{
    services::game::world_bridge::cancel_crystal();
}

void modules::hit_crystal::on_render_shutdown()
{
    cancel_on_client_thread();
}

void modules::hit_crystal::render_options()
{
    gui::settings::render(settings_, module_settings::hit_crystal::schema());
}
