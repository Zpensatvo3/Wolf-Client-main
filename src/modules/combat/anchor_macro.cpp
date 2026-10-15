#include <imgui/imgui.h>
#include "../../cache/cache.hpp"

#include "anchor_engine.hpp"
#include "anchor_macro.hpp"
#include "../ui_redirect.hpp"
void modules::anchor_macro::run(::cache&)
{
	// World interactions run from Minecraft's render/client thread.
}

void modules::anchor_macro::render_overlay()
{
	const anchor_engine::profile mode = blatant_mode
		? anchor_engine::profile::blatant
		: anchor_engine::profile::legit;
	if (!anchor_engine::run_overlay(settings, state, mode))
		enabled = false;
}

void modules::anchor_macro::render_options()
{
	anchor_engine::render_options(settings,
		blatant_mode ? anchor_engine::profile::blatant : anchor_engine::profile::legit,
		blatant_mode ? "blatant_anchor_v2" : "anchor_v2");
}

void modules::anchor_macro::on_disable(::cache& cache)
{
	anchor_engine::stop(cache, state);
}
