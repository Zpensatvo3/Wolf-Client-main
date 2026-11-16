#include "click_aim_assist.hpp"
#include "../../ui/settings_renderer.hpp"

void modules::click_aim_assist::run(::cache& game)
{
    std::lock_guard lock(mutex);
    const auto revision = generation.load(std::memory_order_acquire);
    if (worker_generation != revision || !enabled || settings.mode != 1) {
        blatant_aim_engine::reset(blatant_state);
        worker_generation = revision;
    }
    if (enabled && settings.mode == 1) {
        auto selected = settings.blatant();
        blatant_aim_engine::run(game, selected, blatant_state);
    }
}
void modules::click_aim_assist::render_overlay()
{
    std::lock_guard lock(mutex);
    const auto revision = generation.load(std::memory_order_acquire);
    if (render_generation != revision) {
        legit_aim_engine::request_reset(legit_state);
        render_generation = revision;
    }
    legit_aim_engine::run(settings.legit(), legit_state, enabled && settings.mode == 0, true);
}
void modules::click_aim_assist::on_render_shutdown()
{
    std::lock_guard lock(mutex);
    legit_aim_engine::reset(legit_state);
}
void modules::click_aim_assist::render_options()
{
    std::lock_guard lock(mutex);
    const auto before = settings;
    gui::settings::render(settings, click_aim::config::schema());
    if (!(settings == before)) request_reset();
}
void modules::click_aim_assist::visit_config(config_visitor& visitor)
{
    std::lock_guard lock(mutex);
    if (modules::settings::visit(visitor, settings, click_aim::config::schema())) request_reset();
}
