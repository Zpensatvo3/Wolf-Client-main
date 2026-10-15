#include <imgui/imgui.h>
#include "../../cache/cache.hpp"

#include "legit_aim_engine.hpp"
#include "clickaimassist.hpp"
#include "../ui_redirect.hpp"
void modules::clickaimassist::run(cache&)
{
	// Shared camera engine executes only on the render/client thread.
}

void modules::clickaimassist::render_overlay()
{
	legit_aim_engine::run(settings, state, enabled, false);
}

void modules::clickaimassist::on_enable(cache&)
{
	legit_aim_engine::request_reset(state);
}

void modules::clickaimassist::on_disable(cache&)
{
	legit_aim_engine::request_reset(state);
}

void modules::clickaimassist::render_options()
{
	legit_aim_engine::render_options(settings, "aim_assist_v2");
}
