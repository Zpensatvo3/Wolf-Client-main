#include "anchor_macro.hpp"
#include "../../services/game/world_bridge.hpp"
#include "../../ui/settings_renderer.hpp"

void modules::anchor_macro::render_overlay()
{
    if (lifecycle_reset_.exchange(false,std::memory_order_acq_rel)) cancel_on_client_thread();
    const auto expected_generation = generation_.load(std::memory_order_acquire);
    const auto config = settings_.read();
    services::game::world_bridge::tick_anchor(config,enabled,generation_,expected_generation);
}
void modules::anchor_macro::cancel_on_client_thread()
{
    services::game::world_bridge::cancel_anchor();
}
void modules::anchor_macro::render_options()
{
    if (gui::settings::render(settings_,module_settings::anchor_macro::schema()))
        generation_.fetch_add(1,std::memory_order_release);
}
