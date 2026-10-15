#include <imgui/imgui.h>
#include "../../cache/cache.hpp"

#include "anchor_engine.hpp"
#include "safe_anchor.hpp"
#include "../ui_redirect.hpp"
void modules::safe_anchor::run(::cache&)
{
	// World interactions run from Minecraft's render/client thread.
}

void modules::safe_anchor::render_overlay()
{
	if (!anchor_engine::run_overlay(settings, state, anchor_engine::profile::safe))
		enabled = false;
}

void modules::safe_anchor::render_options()
{
	anchor_engine::render_options(settings, anchor_engine::profile::safe, "safe_anchor_v2");
}

void modules::safe_anchor::on_disable(::cache& cache)
{
	anchor_engine::stop(cache, state);
}
