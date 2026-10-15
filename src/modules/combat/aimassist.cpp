#include <imgui/imgui.h>
#include "../../cache/cache.hpp"

#include "legit_aim_engine.hpp"
#include "aimassist.hpp"
#include "../ui_redirect.hpp"
#include "../gui/gui.hpp"

void modules::aimassist::run(cache&)
{
	// Camera state is no longer read/written from the background module worker.
}

void modules::aimassist::render_overlay()
{
	legit_aim_engine::run(settings, state, enabled, true);
}

void modules::aimassist::on_enable(cache&)
{
	legit_aim_engine::request_reset(state);
}

void modules::aimassist::on_disable(cache&)
{
	legit_aim_engine::request_reset(state);
}

void modules::aimassist::render_options()
{
	legit_aim_engine::render_options(settings, "legit_click_aimassist_v2");
}
