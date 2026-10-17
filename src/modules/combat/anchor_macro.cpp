#include "anchor_macro.hpp"
#include "anchor_engine.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../ui_redirect.hpp"
#include <optional>

struct modules::anchor_macro::runtime
{
    anchor_engine::runtime engine;
    std::optional<module_settings::anchor_macro> config;
};
modules::anchor_macro::anchor_macro() : module("anchor macro","Combat","anchor_macro"), state_(std::make_unique<runtime>()) {}
modules::anchor_macro::~anchor_macro() = default;
void modules::anchor_macro::run(::cache&)
{
	// World interactions run from Minecraft's render/client thread.
}

void modules::anchor_macro::render_overlay()
{
    if (!platform::jni_context::bind_current_thread())
    { reset_requested_.store(true,std::memory_order_release); return; }
    try
    {
        const auto config = settings_.read();
        if (reset_requested_.exchange(false,std::memory_order_acq_rel) || !state_->config || *state_->config != config)
        {
            anchor_engine::cancel_on_client_thread(state_->engine);
            state_->config = config;
        }
        if (!enabled) { anchor_engine::cancel_on_client_thread(state_->engine); return; }
        auto selected = config.selected_config();
        if (!anchor_engine::run_overlay(selected,state_->engine,config.selected_profile())) enabled = false;
    }
    catch (...)
    {
        platform::jni_context::clear_exception();
        reset_requested_.store(true,std::memory_order_release);
        enabled = false;
    }
}

void modules::anchor_macro::render_options()
{
    gui::settings::render(settings_,module_settings::anchor_macro::schema());
    ImGui::TextWrapped("%s","Each mode remembers its settings. Slot switch and action delays are added. Random variation is sampled once per scheduled step, not a guarantee of server acceptance. One sequence per activation.");
}

void modules::anchor_macro::on_disable(::cache& cache)
{
    (void)cache;
    reset_requested_.store(true,std::memory_order_release);
}
