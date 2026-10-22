#include <imgui/imgui.h>
#include "../../cache/cache.hpp"

#include "anchor_engine.hpp"
#include "safe_anchor.hpp"
#include "../../services/game/world_bridge.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../ui_redirect.hpp"

modules::safe_anchor::safe_anchor()
    : module("safe anchor", "Combat"), settings_(defaults())
{
    instance_ = this;
}

modules::safe_anchor::~safe_anchor()
{
    if (instance_ == this)
        instance_ = nullptr;
}

void modules::safe_anchor::run(::cache&)
{
	// World interactions run from Minecraft's pre-tick via client_tick().
}

void modules::safe_anchor::render_overlay()
{
	if (reset_requested_.exchange(false, std::memory_order_acq_rel)) cancel_on_client_thread();

	// Keep WorldBridge synchronized with our enabled state:
	services::game::world_bridge::set_safe_anchor_active(enabled);

	if (!enabled) return;

	// SafeAnchor MUST NEVER execute interactions from the DirectX render frame.
	// Dispatching block placements or held item changes from the swapchain frame
	// causes packets to arrive post-flying, which GrimAC flags as Post.
	// All interactions run exclusively on Minecraft client ticks via client_tick().
}

void modules::safe_anchor::client_tick()
{
	if (!instance_) return;
	if (instance_->reset_requested_.exchange(false, std::memory_order_acq_rel))
	{
		instance_->cancel_on_client_thread();
	}
	if (!instance_->enabled)
	{
		services::game::world_bridge::set_safe_anchor_active(false);
		return;
	}

	auto settings = instance_->settings_.read();
	if (!anchor_engine::tick(settings, instance_->state, anchor_engine::profile::safe))
	{
		instance_->enabled = false;
		services::game::world_bridge::set_safe_anchor_active(false);
	}
}

void modules::safe_anchor::render_options()
{
	if (gui::settings::render(settings_, anchor_engine::schema(anchor_engine::profile::safe)))
		reset_requested_.store(true, std::memory_order_release);
	std::lock_guard lock(state.mutex);
	ImGui::TextWrapped("%s", state.status);
	ImGui::TextWrapped("%s", "Uses obsidian or glowstone as a low protective block. Stand still and aim at a block or anchor before enabling. Cover reduces blast exposure; it does not guarantee zero damage.");
}

void modules::safe_anchor::on_enable(::cache&)
{
	reset_requested_.store(true, std::memory_order_release);
	services::game::world_bridge::set_safe_anchor_active(true);
}

void modules::safe_anchor::on_disable(::cache& cache)
{
	(void)cache;
	services::game::world_bridge::set_safe_anchor_active(false);
	reset_requested_.store(true, std::memory_order_release);
	cancel_on_client_thread();
}

void modules::safe_anchor::cancel_on_client_thread()
{
	services::game::world_bridge::set_safe_anchor_active(false);
	anchor_engine::cancel_on_client_thread(state);
}
