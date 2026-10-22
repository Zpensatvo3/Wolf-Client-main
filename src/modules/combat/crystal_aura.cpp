#include "crystal_aura.hpp"
#include "../../services/game/world_bridge.hpp"
#include "../../services/game/attack_coordinator.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"

modules::crystal_aura::crystal_aura() : module("crystal aura","Combat","crystal_aura") {}
modules::crystal_aura::~crystal_aura() { services::combat::release_crystal_aura(); }
void modules::crystal_aura::render_overlay()
{
    if (reset_requested_.exchange(false,std::memory_order_acq_rel)) cancel_on_client_thread();
    const auto expected = generation_.load(std::memory_order_acquire);
    const auto config = settings_.read();
    status_ = services::game::world_bridge::tick_aura(config,enabled,generation_,expected);
}
void modules::crystal_aura::cancel_on_client_thread()
{
    services::game::world_bridge::cancel_aura();
    status_ = {-1,0,0};
}
void modules::crystal_aura::on_render_shutdown() { cancel_on_client_thread(); }

void modules::crystal_aura::render_options()
{
    if (gui::settings::render(settings_,module_settings::crystal_aura::schema()))
        generation_.fetch_add(1,std::memory_order_release);
    const auto state = status_;
    ImGui::TextWrapped("%s","Places crystals by estimated damage and breaks nearby crystals independently. Obsidian support is optional. Visible rotations and vanilla reach apply.");
    const char* messages[]{
        "Waiting for a target or action delay", "No crystals in hotbar or offhand", "No valid base / placement in range",
        "Paused: using item or Triggerbot owns attack", "Java controller unavailable; see game log",
        "Aiming / waiting for slot sync", "Waiting for server confirmation", "Self / friend damage limit blocks action",
        "Select the required item or enable Auto Switch", "Paused after manual slot change",
        "Action not accepted or confirmation timed out", "Crystal retry limit reached", "Weakness: select a sword or axe",
        "Estimated target damage is below the minimum"};
    const int code=state[2];
    ImGui::TextWrapped("%s",code>=0 && code<int(std::size(messages))?messages[code]:"Waiting");
    if (state[0]>=0) {
        const char* phases[]{"Planning","Aiming at support","Waiting for obsidian","Aiming at crystal base",
            "Waiting for crystal spawn","Aiming at crystal","Waiting for crystal removal"};
        const int phase=state[1];
        ImGui::TextWrapped("Target: %d | %s",state[0],phase>=0 && phase<int(std::size(phases))?phases[phase]:"Planning");
    }
}
