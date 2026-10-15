#include "auto_tool.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../cache/cache.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/hotbar_service.hpp"
#include "../ui_redirect.hpp"
#include <Windows.h>
#include <algorithm>
#include <string>

#include <imgui/imgui.h>

#include "../../gui/gui.hpp"

namespace
{
struct mining_score
	{
		float speed = 1.0f;
		bool suitable = false;
	};

	mining_score score_stack(const maps::ItemStack& stack, const maps::BlockState& state)
	{
		mining_score result{};
		if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception())
			return result;
		result.speed = stack.getMiningSpeedMultiplier.call(state);
		if (platform::jni_context::clear_exception())
			return {};
		result.suitable = stack.isSuitableFor.call(state) == JNI_TRUE;
		platform::jni_context::clear_exception();
		return result;
	}

	bool is_better_score(const mining_score& candidate, const mining_score& current)
	{
		if (candidate.suitable != current.suitable)
			return candidate.suitable;
		return candidate.speed > current.speed + 0.01f;
	}

}

void modules::auto_tool::run(::cache& cache)
{
	const auto settings_snapshot = settings_.read();
	if (gui::draw || !cache.is_valid() || !cache.instance || !cache.player || !cache.world || !jni::get_env() ||
		(settings_snapshot.require_left_click && (GetAsyncKeyState(VK_LBUTTON) & 0x8000) == 0))
		return;

	try
	{
		jni::frame frame{ 80 };
		if (cache.instance.currentScreen.get() || platform::jni_context::clear_exception())
			return;
		maps::HitResult hit = cache.instance.crosshairTarget.get();
		maps::HitResult$Type block_type = maps::HitResult$Type{}.BLOCK.get();
		if (!hit || !block_type || platform::jni_context::clear_exception() ||
			!hit.getType.call().is_same_object(block_type) || platform::jni_context::clear_exception())
			return;

		maps::BlockHitResult block_hit(hit.object_instance, false);
		maps::BlockPos position = block_hit.getBlockPos.call();
		maps::BlockState state = position ? cache.world.getBlockState.call(position) : maps::BlockState{};
		if (!state || state.isAir.call() == JNI_TRUE || platform::jni_context::clear_exception())
			return;

		maps::PlayerInventory inventory = cache.player.getInventory.call();
		maps::DefaultedList items = inventory ? inventory.main.get() : maps::DefaultedList{};
		if (!inventory || !items || platform::jni_context::clear_exception())
			return;

		const int selected = std::clamp(static_cast<int>(inventory.selectedSlot.get()), 0, 8);
		if (platform::jni_context::clear_exception())
			return;
		int best_slot = selected;
		mining_score best_score{};
		maps::Object selected_object = items.get.call(selected);
		if (selected_object && !platform::jni_context::clear_exception())
			best_score = score_stack(maps::ItemStack(selected_object.object_instance, false), state);
		for (int slot = 0; slot < 9; ++slot)
		{
			if (slot == selected)
				continue;
			maps::Object object = items.get.call(slot);
			if (!object || platform::jni_context::clear_exception())
				continue;
			maps::ItemStack stack(object.object_instance, false);
			const mining_score score = score_stack(stack, state);
			if (is_better_score(score, best_score))
			{
				best_score = score;
				best_slot = slot;
			}
		}

		if (best_slot != selected)
		{
			if (services::hotbar::select(cache, best_slot))
				last_selected_slot = best_slot;
		}
	}
	catch (...) { platform::jni_context::clear_exception(); }
}

void modules::auto_tool::render_options()
{
	gui::settings::render(settings_, module_settings::auto_tool::schema());
	ImGui::TextDisabled("%s", gui::text(
		"Chooses the strongest matching pickaxe, axe, or shovel from your hotbar.",
		"Hotbardaki en güçlü uygun kazma, balta veya küreği seçer."));
}
