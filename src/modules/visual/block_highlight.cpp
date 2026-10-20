#include "block_highlight.hpp"
#include "../../ui/settings_renderer.hpp"
void modules::block_highlight::render_overlay()
{
    const auto generation = reset_.load(std::memory_order_acquire);
    if (!enabled || generation != applied_) { state_.clear(); applied_ = generation; }
    if (enabled) cosmetics::draw_block_highlight(settings_.read(), state_);
}
void modules::block_highlight::render_options() { gui::settings::render(settings_, module_settings::block_highlight::schema()); }
