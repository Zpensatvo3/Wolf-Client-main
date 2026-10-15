#include <imgui/imgui.h>
#include "../../cache/cache.hpp"

#include "blatant_aim_engine.hpp"
#include "blatant_clickaimassist.hpp"
#include "../ui_redirect.hpp"
#include "../../gui/gui.hpp"

void modules::blatant_clickaimassist::run(cache& cache)
{
	blatant_aim_engine::run(cache, settings, state);
}

void modules::blatant_clickaimassist::on_disable(cache&)
{
	state = {};
}

void modules::blatant_clickaimassist::render_options()
{
	blatant_aim_engine::render_options(settings, "blatant_click_aimassist");
}
