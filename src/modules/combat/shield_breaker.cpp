#include "shield_breaker.hpp"
#include "../../services/game/world_bridge.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"
void modules::shield_breaker::render_overlay() {
    services::game::world_bridge::tick_shield(settings_.read(),enabled,generation_);
}
void modules::shield_breaker::cancel_on_client_thread() { services::game::world_bridge::cancel_shield(); }
void modules::shield_breaker::on_render_shutdown() { cancel_on_client_thread(); }
bool modules::shield_breaker::gate_left_click() {
    auto* current=active_instance.load(std::memory_order_acquire);
    return current && current->enabled && services::game::world_bridge::prepare_shield_manual();
}
void modules::shield_breaker::render_options() {
    if (gui::settings::render(settings_,module_settings::shield_breaker::schema()))
        generation_.fetch_add(1,std::memory_order_acq_rel);
}
