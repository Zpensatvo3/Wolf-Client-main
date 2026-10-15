#include <Windows.h>
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"

#include <imgui/imgui.h>

#include "sprint.hpp"
#include "../ui_redirect.hpp"
#include "../gui/gui.hpp"
#include "../mappings.hpp"

namespace
{
}

void modules::sprint::run(::cache& cache)
{
	const HWND game_window = gui::get_window();
	if (gui::draw || !game_window || GetForegroundWindow() != game_window ||
		!cache.is_valid() || !cache.instance || !cache.player || !jni::get_env())
		return;

	try
	{
		jni::frame frame{ 32 };
		if (cache.instance.currentScreen.get() || platform::jni_context::clear_exception())
			return;

		if (stop_while_using_item &&
			cache.player.isUsingItem.call() == JNI_TRUE && !platform::jni_context::clear_exception())
			return;
		platform::jni_context::clear_exception();

		if (!platform::adapter::get().can_start_sprinting(cache.player, cache.options))
			return;

		if (cache.player.isSprinting.call() != JNI_TRUE && !platform::jni_context::clear_exception())
		{
			cache.player.setSprinting.call(JNI_TRUE);
			if (!platform::jni_context::clear_exception())
				started_by_module = true;
		}
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}
}

void modules::sprint::on_disable(::cache& cache)
{
	if (!started_by_module)
		return;

	try
	{
		if (cache.player && jni::get_env() &&
			cache.player.isSprinting.call() == JNI_TRUE && !platform::jni_context::clear_exception())
			cache.player.setSprinting.call(JNI_FALSE);
		platform::jni_context::clear_exception();
	}
	catch (...)
	{
		platform::jni_context::clear_exception();
	}
	started_by_module = false;
}

void modules::sprint::render_options()
{
	ImGui::Checkbox(gui::text("Stop While Using Items", "Eşya Kullanırken Dur"),
		&stop_while_using_item);
	ImGui::TextDisabled("%s", gui::text(
		"Uses Minecraft's native 1.21.1 sprint eligibility checks.",
		"Minecraft 1.21.1'in kendi sprint uygunluk kontrolünü kullanır."));
}
