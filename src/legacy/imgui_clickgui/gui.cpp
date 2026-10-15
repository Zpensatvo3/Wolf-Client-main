#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "gui.hpp"
#include "ui_state.hpp"
#include <Windows.h>
#include <gl/GL.h>
#include <wincodec.h>
#include "../imgui/imgui_impl_opengl3.h"
#include "../imgui/imgui_impl_win32.h"
#include "../modules/core/module_registry.hpp"
#include "../modules/core/public_config.hpp"
#include "../modules/combat/mace_swap.hpp"
#include "../modules/combat/shield_breaker.hpp"
#include "../modules/misc/fast_exp.hpp"
#include "../modules/misc/left_clicker.hpp"
#include "../modules/misc/right_clicker.hpp"
#include "../friends/friends.hpp"
#include "../configs/configs.hpp"
#include "../loader/loader_ui.hpp"
#include "../platform/jni/client_dispatcher.hpp"
#include "../../resource.h"
#include <MinHook.h>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <array>
#include <cfloat>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <unordered_set>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#pragma comment(lib, "windowscodecs.lib")

namespace
{
	typedef BOOL(WINAPI* wglSwapBuffers_t)(HDC);

	wglSwapBuffers_t wglSwapBuffers = nullptr;
	wglSwapBuffers_t original_wglSwapBuffers = nullptr;
	WNDPROC original_WndProc = nullptr;

	HWND window = nullptr;
	ImGuiContext* imGuiContext = nullptr;
	HGLRC original_context = nullptr;
	HGLRC new_context = nullptr;

	std::atomic_bool request_shutdown{ false };
	std::atomic_bool shut_down{ false };
	volatile bool run_once = true;

	int last_pressed_key = 0;
	bool panic_animation_active = false;
	std::chrono::steady_clock::time_point panic_animation_started{};
	modules::module* selected_settings_module = nullptr;
	ImVector<ImWchar> language_glyph_ranges{};
	ImVector<ImWchar> icon_glyph_ranges{};
	std::vector<unsigned char> ui_font_data{};
	std::vector<unsigned char> ui_bold_font_data{};
	std::vector<unsigned char> icon_font_data{};
	std::array<GLuint, 1> icon_textures{};
	std::array<bool, 1> icon_texture_attempted{};

	int icon_resource(gui::icon_asset asset)
	{
		switch (asset)
		{
		case gui::icon_asset::helmet: return IDR_ICON_HELMET;
		default: return 0;
		}
	}

	HMODULE current_module()
	{
		HMODULE module = nullptr;
		GetModuleHandleExW(
			GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			reinterpret_cast<LPCWSTR>(&gui::get_icon_texture),
			&module);
		return module;
	}

	bool read_resource_bytes(int resource_id, std::vector<unsigned char>& output)
	{
		output.clear();
		const HMODULE module = current_module();
		const HRSRC resource = module && resource_id
			? FindResourceW(module, MAKEINTRESOURCEW(resource_id), RT_RCDATA)
			: nullptr;
		if (!resource) return false;
		const DWORD size = SizeofResource(module, resource);
		const HGLOBAL loaded = LoadResource(module, resource);
		const void* data = loaded ? LockResource(loaded) : nullptr;
		if (!data || size == 0 || size > 16u * 1024u * 1024u) return false;
		const auto* begin = static_cast<const unsigned char*>(data);
		output.assign(begin, begin + size);
		return true;
	}

	bool load_png_texture(const void* encoded_data, std::size_t encoded_size, GLuint& texture)
	{
		IWICImagingFactory* factory = nullptr;
		IWICStream* stream = nullptr;
		IWICBitmapDecoder* decoder = nullptr;
		IWICBitmapFrameDecode* frame = nullptr;
		IWICFormatConverter* converter = nullptr;
		UINT width = 0;
		UINT height = 0;
		std::vector<BYTE> pixels;
		bool success = false;

		if (!encoded_data || encoded_size == 0 || encoded_size > static_cast<std::size_t>((std::numeric_limits<DWORD>::max)()))
			goto cleanup;
		if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
			IID_PPV_ARGS(&factory))) || !factory)
			goto cleanup;
		if (FAILED(factory->CreateStream(&stream)) || !stream ||
			FAILED(stream->InitializeFromMemory(
				const_cast<BYTE*>(static_cast<const BYTE*>(encoded_data)), static_cast<DWORD>(encoded_size))))
			goto cleanup;
		if (FAILED(factory->CreateDecoderFromStream(stream, nullptr,
			WICDecodeMetadataCacheOnLoad, &decoder)) || !decoder)
			goto cleanup;
		if (FAILED(decoder->GetFrame(0, &frame)) || !frame ||
			FAILED(factory->CreateFormatConverter(&converter)) || !converter ||
			FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
				nullptr, 0.0, WICBitmapPaletteTypeCustom)))
			goto cleanup;

		if (FAILED(converter->GetSize(&width, &height)) || width == 0 || height == 0)
			goto cleanup;

		pixels.resize(static_cast<size_t>(width) * static_cast<size_t>(height) * 4u);
		if (FAILED(converter->CopyPixels(nullptr, width * 4u, static_cast<UINT>(pixels.size()), pixels.data())))
			goto cleanup;

		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
			0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
		glBindTexture(GL_TEXTURE_2D, 0);
		success = texture != 0;

	cleanup:
		if (converter) converter->Release();
		if (frame) frame->Release();
		if (decoder) decoder->Release();
		if (stream) stream->Release();
		if (factory) factory->Release();
		return success;
	}

	// Virtual mouse position for GLFW cursor-disabled raw-input tracking
	static float virt_mouse_x = 0.0f;
	static float virt_mouse_y = 0.0f;

	float smooth_value(float current, float target, float speed)
	{
		const float delta_time = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.10f);
		return current + (target - current) * (1.0f - std::exp(-speed * delta_time));
	}

	ImU32 color_with_alpha(const ImColor& color, float alpha = 1.0f)
	{
		ImVec4 value = color.Value;
		value.w *= std::clamp(alpha, 0.0f, 1.0f);
		return ImGui::ColorConvertFloat4ToU32(value);
	}

	ImU32 mix_color(const ImVec4& from, const ImVec4& to, float amount)
	{
		amount = std::clamp(amount, 0.0f, 1.0f);
		return ImGui::ColorConvertFloat4ToU32(ImVec4(
			from.x + (to.x - from.x) * amount,
			from.y + (to.y - from.y) * amount,
			from.z + (to.z - from.z) * amount,
			from.w + (to.w - from.w) * amount));
	}

	bool centered_text_button(const char* label, const ImVec2& size)
	{
		return gui::centered_text_button(label, size);
	}

	void focus_minecraft_window(HWND minecraft_window)
	{
		if (!minecraft_window || !IsWindow(minecraft_window))
			return;

		if (IsIconic(minecraft_window))
			ShowWindow(minecraft_window, SW_RESTORE);

		const HWND foreground_window = GetForegroundWindow();
		const DWORD current_thread = GetCurrentThreadId();
		const DWORD foreground_thread = foreground_window
			? GetWindowThreadProcessId(foreground_window, nullptr)
			: 0;
		const bool attached = foreground_thread != 0 && foreground_thread != current_thread &&
			AttachThreadInput(current_thread, foreground_thread, TRUE) != FALSE;

		BringWindowToTop(minecraft_window);
		SetForegroundWindow(minecraft_window);
		SetActiveWindow(minecraft_window);
		SetFocus(minecraft_window);

		if (attached)
			AttachThreadInput(current_thread, foreground_thread, FALSE);
	}

	void begin_panic_animation()
	{
		if (panic_animation_active)
			return;
		panic_animation_active = true;
		panic_animation_started = std::chrono::steady_clock::now();

		// Stop gameplay behavior immediately while the visual shutdown finishes.
		for (modules::module* module : modules::get_modules())
			module->enabled = false;
	}

	void render_panic_animation(
		ImDrawList* draw_list,
		const ImVec2& window_position,
		const ImVec2& window_size,
		float rounding)
	{
		if (!panic_animation_active || !draw_list)
			return;

		using clock = std::chrono::steady_clock;
		const float elapsed = std::chrono::duration<float>(
			clock::now() - panic_animation_started).count();
		const float linear_progress = std::clamp(elapsed / 2.60f, 0.0f, 1.0f);
		const float progress = 1.0f - std::pow(1.0f - linear_progress, 1.35f);
		const float entrance = std::clamp(elapsed / 0.20f, 0.0f, 1.0f);
		const float eased_entrance = 1.0f - std::pow(1.0f - entrance, 3.0f);
		const ImColor accent = gui::get_dynamic_color(static_cast<int>(elapsed * 90.0f));

		const ImVec2 window_end(
			window_position.x + window_size.x,
			window_position.y + window_size.y);
		draw_list->AddRectFilled(
			window_position,
			window_end,
			IM_COL32(5, 5, 8, static_cast<int>(255.0f * eased_entrance)),
			rounding);

		const ImVec2 card_size(430.0f, 276.0f);
		const ImVec2 card_min(
			window_position.x + (window_size.x - card_size.x) * 0.5f,
			window_position.y + (window_size.y - card_size.y) * 0.5f +
				(1.0f - eased_entrance) * 18.0f);
		const ImVec2 card_max(card_min.x + card_size.x, card_min.y + card_size.y);

		for (int layer = 5; layer >= 1; --layer)
		{
			const float spread = static_cast<float>(layer) * 3.0f;
			draw_list->AddRectFilled(
				ImVec2(card_min.x - spread, card_min.y - spread),
				ImVec2(card_max.x + spread, card_max.y + spread),
				color_with_alpha(accent, 0.018f * static_cast<float>(6 - layer) * eased_entrance),
				18.0f + spread);
		}

		draw_list->AddRectFilled(card_min, card_max, IM_COL32(16, 16, 20, 252), 18.0f);
		draw_list->AddRect(card_min, card_max, color_with_alpha(accent, 0.34f), 18.0f, 0, 1.0f);

		const ImVec2 icon_center(card_min.x + card_size.x * 0.5f, card_min.y + 61.0f);
		const float pulse = 0.5f + 0.5f * std::sin(elapsed * 5.0f);
		draw_list->AddCircleFilled(icon_center, 35.0f + pulse * 3.0f,
			color_with_alpha(accent, 0.055f + pulse * 0.025f), 48);

		if (linear_progress < 0.91f)
		{
			const float rotation = elapsed * 4.4f;
			draw_list->PathArcTo(icon_center, 27.0f, rotation, rotation + 4.75f, 40);
			draw_list->PathStroke(color_with_alpha(accent, 0.95f), 0, 3.0f);
			draw_list->AddCircle(icon_center, 13.0f, color_with_alpha(accent, 0.92f), 32, 2.2f);
			draw_list->AddLine(
				ImVec2(icon_center.x, icon_center.y - 17.0f),
				ImVec2(icon_center.x, icon_center.y - 3.0f),
				color_with_alpha(accent, 1.0f), 3.0f);
		}
		else
		{
			draw_list->AddCircle(icon_center, 27.0f, color_with_alpha(accent, 0.95f), 40, 3.0f);
			draw_list->AddLine(
				ImVec2(icon_center.x - 11.0f, icon_center.y),
				ImVec2(icon_center.x - 2.0f, icon_center.y + 9.0f),
				color_with_alpha(accent, 1.0f), 3.0f);
			draw_list->AddLine(
				ImVec2(icon_center.x - 2.0f, icon_center.y + 9.0f),
				ImVec2(icon_center.x + 14.0f, icon_center.y - 9.0f),
				color_with_alpha(accent, 1.0f), 3.0f);
		}

		auto centered_text = [&](float y, ImU32 color, const char* text)
		{
			const ImVec2 text_size = ImGui::CalcTextSize(text);
			draw_list->AddText(
				ImVec2(card_min.x + (card_size.x - text_size.x) * 0.5f, y),
				color, text);
		};

		centered_text(card_min.y + 104.0f, IM_COL32(240, 240, 245, 255), "Closing Thedorius Lite");

		const char* status = "Disabling active modules";
		if (linear_progress >= 0.25f) status = "Releasing input hooks";
		if (linear_progress >= 0.50f) status = "Clearing runtime state";
		if (linear_progress >= 0.76f) status = "Finalizing safe shutdown";
		if (linear_progress >= 0.96f) status = "Shutdown complete";
		centered_text(card_min.y + 132.0f, IM_COL32(132, 132, 143, 255), status);

		const float bar_left = card_min.x + 42.0f;
		const float bar_right = card_max.x - 42.0f;
		const float bar_top = card_min.y + 169.0f;
		const float bar_bottom = bar_top + 8.0f;
		draw_list->AddRectFilled(
			ImVec2(bar_left, bar_top), ImVec2(bar_right, bar_bottom),
			IM_COL32(39, 39, 47, 255), 4.0f);
		const float fill_right = bar_left + (bar_right - bar_left) * progress;
		if (fill_right > bar_left + 1.0f)
		{
			draw_list->AddRectFilled(
				ImVec2(bar_left, bar_top - 3.0f), ImVec2(fill_right, bar_bottom + 3.0f),
				color_with_alpha(accent, 0.12f), 7.0f);
			draw_list->AddRectFilled(
				ImVec2(bar_left, bar_top), ImVec2(fill_right, bar_bottom),
				color_with_alpha(accent, 1.0f), 4.0f);
		}

		char percentage[16]{};
		sprintf_s(percentage, "%d%%", static_cast<int>(std::round(progress * 100.0f)));
		centered_text(card_min.y + 188.0f, color_with_alpha(accent, 0.94f), percentage);

		const char* step_labels[] = { "Modules", "Input", "Memory", "Unload" };
		const float step_thresholds[] = { 0.08f, 0.34f, 0.60f, 0.84f };
		for (int step = 0; step < 4; ++step)
		{
			const float x = card_min.x + 69.0f + static_cast<float>(step) * 97.0f;
			const bool complete = linear_progress >= step_thresholds[step];
			draw_list->AddCircleFilled(
				ImVec2(x, card_min.y + 226.0f), 4.0f,
				complete ? color_with_alpha(accent, 1.0f) : IM_COL32(62, 62, 72, 255), 18);
			const ImVec2 label_size = ImGui::CalcTextSize(step_labels[step]);
			draw_list->AddText(
				ImVec2(x - label_size.x * 0.5f, card_min.y + 239.0f),
				complete ? IM_COL32(185, 185, 194, 255) : IM_COL32(82, 82, 91, 255),
				step_labels[step]);
		}

		if (elapsed >= 2.78f)
			gui::should_unload = true;
	}

	void render_loader_overlay()
	{
		if (!gui::loader_visible)
			return;

		using clock = std::chrono::steady_clock;
		static bool started = false;
		static clock::time_point started_at{};
		static float displayed_progress = 0.0f;

		const loader_ui::state loader_state = loader_ui::snapshot();
		if (loader_state.closing)
		{
			gui::loader_visible = false;
			return;
		}

		if (!started)
		{
			started = true;
			started_at = clock::now();
			displayed_progress = 0.0f;
		}

		const float elapsed = std::chrono::duration<float>(clock::now() - started_at).count();
		const float entrance_linear = std::clamp(elapsed / 0.24f, 0.0f, 1.0f);
		const float entrance = 1.0f - std::pow(1.0f - entrance_linear, 3.0f);

		float target_progress = std::clamp(
			(static_cast<float>(loader_state.stage) + 0.72f) / 5.0f,
			0.08f,
			0.94f);
		if (loader_state.completed)
		{
			const float timeline = std::clamp(elapsed / 3.35f, 0.0f, 1.0f);
			target_progress = 1.0f - std::pow(1.0f - timeline, 1.28f);
		}
		displayed_progress = smooth_value(displayed_progress, target_progress, 7.5f);
		if (loader_state.completed && elapsed >= 3.35f)
			displayed_progress = 1.0f;

		float exit_alpha = 1.0f;
		if (loader_state.completed && elapsed > 3.55f)
			exit_alpha = 1.0f - std::clamp((elapsed - 3.55f) / 0.45f, 0.0f, 1.0f);
		const float alpha = entrance * exit_alpha;

		if (loader_state.completed && elapsed >= 4.0f)
		{
			gui::loader_visible = false;
			return;
		}

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImDrawList* draw_list = ImGui::GetForegroundDrawList();
		if (!viewport || !draw_list)
			return;

		const ImVec2 screen_min = viewport->Pos;
		const ImVec2 screen_max(
			viewport->Pos.x + viewport->Size.x,
			viewport->Pos.y + viewport->Size.y);
		const ImVec2 screen_center(
			viewport->Pos.x + viewport->Size.x * 0.5f,
			viewport->Pos.y + viewport->Size.y * 0.5f);

		const ImColor accent = loader_state.failed
			? ImColor(235, 66, 84)
			: gui::get_dynamic_color(static_cast<int>(elapsed * 75.0f));

		// A fully opaque native canvas replaces the allocated debug console. The
		// layered accent haze gives the panel a soft glass-like edge without a
		// framebuffer blur shader.
		draw_list->AddRectFilled(
			screen_min,
			screen_max,
			IM_COL32(5, 6, 9, static_cast<int>(250.0f * alpha)));

		const float ambient_pulse = 0.5f + 0.5f * std::sin(elapsed * 2.4f);
		for (int layer = 8; layer >= 1; --layer)
		{
			const float radius = 115.0f + static_cast<float>(layer) * 29.0f + ambient_pulse * 6.0f;
			draw_list->AddCircleFilled(
				ImVec2(screen_center.x, screen_center.y - 6.0f),
				radius,
				color_with_alpha(accent, alpha * (0.004f + static_cast<float>(9 - layer) * 0.0025f)),
				64);
		}

		const float card_width = std::min(590.0f, std::max(420.0f, viewport->Size.x - 42.0f));
		const float card_height = std::min(344.0f, std::max(310.0f, viewport->Size.y - 42.0f));
		const ImVec2 card_min(
			screen_center.x - card_width * 0.5f,
			screen_center.y - card_height * 0.5f + (1.0f - entrance) * 18.0f);
		const ImVec2 card_max(card_min.x + card_width, card_min.y + card_height);

		for (int layer = 6; layer >= 1; --layer)
		{
			const float spread = static_cast<float>(layer) * 3.0f;
			draw_list->AddRectFilled(
				ImVec2(card_min.x - spread, card_min.y - spread),
				ImVec2(card_max.x + spread, card_max.y + spread),
				color_with_alpha(accent, alpha * 0.012f * static_cast<float>(7 - layer)),
				22.0f + spread);
		}

		draw_list->AddRectFilled(card_min, card_max,
			IM_COL32(14, 15, 19, static_cast<int>(252.0f * alpha)), 20.0f);
		draw_list->AddRect(card_min, card_max,
			color_with_alpha(accent, alpha * 0.40f), 20.0f, 0, 1.15f);
		draw_list->AddLine(
			ImVec2(card_min.x + 26.0f, card_min.y + 65.0f),
			ImVec2(card_max.x - 26.0f, card_min.y + 65.0f),
			IM_COL32(255, 255, 255, static_cast<int>(18.0f * alpha)));

		ImFont* font = ImGui::GetFont();
		const char* brand_left = "THEDORIUS";
		const char* brand_right = " LITE";
		const float brand_size = 21.0f;
		const ImVec2 left_size = font->CalcTextSizeA(brand_size, FLT_MAX, 0.0f, brand_left);
		const ImVec2 right_size = font->CalcTextSizeA(brand_size, FLT_MAX, 0.0f, brand_right);
		const float brand_x = card_min.x + 26.0f;
		draw_list->AddText(font, brand_size, ImVec2(brand_x, card_min.y + 23.0f),
			color_with_alpha(accent, alpha), brand_left);
		draw_list->AddText(font, brand_size, ImVec2(brand_x + left_size.x, card_min.y + 23.0f),
			IM_COL32(238, 239, 244, static_cast<int>(255.0f * alpha)), brand_right);

		const char* runtime_label = "NATIVE RUNTIME  //  1.21.1";
		const ImVec2 runtime_size = ImGui::CalcTextSize(runtime_label);
		draw_list->AddText(
			ImVec2(card_max.x - runtime_size.x - 26.0f, card_min.y + 29.0f),
			IM_COL32(105, 107, 118, static_cast<int>(225.0f * alpha)), runtime_label);

		const ImVec2 emblem_center(card_min.x + card_width * 0.5f, card_min.y + 119.0f);
		const float rotation = elapsed * 3.4f;
		draw_list->AddCircleFilled(emblem_center, 41.0f + ambient_pulse * 2.0f,
			color_with_alpha(accent, alpha * (0.055f + ambient_pulse * 0.02f)), 64);
		draw_list->AddCircle(emblem_center, 31.0f,
			IM_COL32(255, 255, 255, static_cast<int>(18.0f * alpha)), 48, 1.0f);

		if (loader_state.failed)
		{
			draw_list->AddCircle(emblem_center, 26.0f, color_with_alpha(accent, alpha), 48, 2.5f);
			draw_list->AddLine(ImVec2(emblem_center.x - 8.0f, emblem_center.y - 8.0f),
				ImVec2(emblem_center.x + 8.0f, emblem_center.y + 8.0f), color_with_alpha(accent, alpha), 2.5f);
			draw_list->AddLine(ImVec2(emblem_center.x + 8.0f, emblem_center.y - 8.0f),
				ImVec2(emblem_center.x - 8.0f, emblem_center.y + 8.0f), color_with_alpha(accent, alpha), 2.5f);
		}
		else if (displayed_progress >= 0.985f)
		{
			draw_list->AddCircle(emblem_center, 26.0f, color_with_alpha(accent, alpha), 48, 2.5f);
			draw_list->AddLine(ImVec2(emblem_center.x - 11.0f, emblem_center.y),
				ImVec2(emblem_center.x - 2.0f, emblem_center.y + 9.0f), color_with_alpha(accent, alpha), 3.0f);
			draw_list->AddLine(ImVec2(emblem_center.x - 2.0f, emblem_center.y + 9.0f),
				ImVec2(emblem_center.x + 14.0f, emblem_center.y - 9.0f), color_with_alpha(accent, alpha), 3.0f);
		}
		else
		{
			draw_list->PathArcTo(emblem_center, 31.0f, rotation, rotation + 4.75f, 48);
			draw_list->PathStroke(color_with_alpha(accent, alpha), 0, 3.0f);


			if (const ImTextureID helmet = gui::get_icon_texture(gui::icon_asset::helmet))
			{
				const float helmet_size = 43.0f;
				draw_list->AddImage(helmet,
					ImVec2(emblem_center.x - helmet_size * 0.5f, emblem_center.y - helmet_size * 0.5f),
					ImVec2(emblem_center.x + helmet_size * 0.5f, emblem_center.y + helmet_size * 0.5f),
					ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), IM_COL32(255, 255, 255, static_cast<int>(255.0f * alpha)));
			}
			else
			{
			// Offline fallback: the original Thedorius mark remains available.
			const ImVec2 crest_points[] = {
				ImVec2(emblem_center.x - 17.0f, emblem_center.y - 15.0f),
				ImVec2(emblem_center.x + 17.0f, emblem_center.y - 15.0f),
				ImVec2(emblem_center.x + 13.0f, emblem_center.y + 12.0f),
				ImVec2(emblem_center.x, emblem_center.y + 20.0f),
				ImVec2(emblem_center.x - 13.0f, emblem_center.y + 12.0f)
			};
			draw_list->AddConvexPolyFilled(crest_points, IM_ARRAYSIZE(crest_points), color_with_alpha(accent, alpha * 0.15f));
			draw_list->AddPolyline(crest_points, IM_ARRAYSIZE(crest_points), color_with_alpha(accent, alpha * 0.96f), ImDrawFlags_Closed, 1.8f);
			draw_list->AddLine(ImVec2(emblem_center.x - 9.0f, emblem_center.y - 7.0f), ImVec2(emblem_center.x + 9.0f, emblem_center.y - 7.0f), IM_COL32(239, 240, 245, static_cast<int>(240.0f * alpha)), 2.5f);
			draw_list->AddLine(ImVec2(emblem_center.x, emblem_center.y - 7.0f), ImVec2(emblem_center.x, emblem_center.y + 10.0f), IM_COL32(239, 240, 245, static_cast<int>(240.0f * alpha)), 2.5f);
			}
		}

		const char* stage_names[] = {
			"Locating Minecraft runtime",
			"Connecting to the JVM",
			"Building native class cache",
			"Installing renderer hooks",
			"Registering client modules"
		};
		const int visual_stage = std::clamp(
			static_cast<int>(displayed_progress * 5.0f), 0, 4);
		const char* status_title = loader_state.failed
			? "Initialization failed"
			: (displayed_progress >= 0.985f ? "Thedorius Lite is ready" : stage_names[visual_stage]);
		const ImVec2 status_size = ImGui::CalcTextSize(status_title);
		draw_list->AddText(
			ImVec2(card_min.x + (card_width - status_size.x) * 0.5f, card_min.y + 166.0f),
			IM_COL32(228, 229, 235, static_cast<int>(255.0f * alpha)), status_title);

		const char* detail = loader_state.failed
			? loader_state.detail.c_str()
			: "Preparing a clean and stable Minecraft session";
		const ImVec2 detail_size = ImGui::CalcTextSize(detail);
		draw_list->AddText(
			ImVec2(card_min.x + (card_width - detail_size.x) * 0.5f, card_min.y + 190.0f),
			IM_COL32(104, 106, 117, static_cast<int>(235.0f * alpha)), detail);

		const float bar_left = card_min.x + 42.0f;
		const float bar_right = card_max.x - 42.0f;
		const float bar_top = card_min.y + 226.0f;
		draw_list->AddRectFilled(ImVec2(bar_left, bar_top), ImVec2(bar_right, bar_top + 7.0f),
			IM_COL32(37, 38, 46, static_cast<int>(255.0f * alpha)), 4.0f);
		const float bar_fill = bar_left + (bar_right - bar_left) * displayed_progress;
		if (bar_fill > bar_left + 1.0f)
		{
			draw_list->AddRectFilled(ImVec2(bar_left, bar_top - 3.0f), ImVec2(bar_fill, bar_top + 10.0f),
				color_with_alpha(accent, alpha * 0.12f), 7.0f);
			draw_list->AddRectFilled(ImVec2(bar_left, bar_top), ImVec2(bar_fill, bar_top + 7.0f),
				color_with_alpha(accent, alpha), 4.0f);
		}

		char percentage[16]{};
		sprintf_s(percentage, "%d%%", static_cast<int>(std::round(displayed_progress * 100.0f)));
		const ImVec2 percentage_size = ImGui::CalcTextSize(percentage);
		draw_list->AddText(ImVec2(card_max.x - 42.0f - percentage_size.x, card_min.y + 242.0f),
			color_with_alpha(accent, alpha * 0.92f), percentage);

		const char* step_labels[] = { "Runtime", "JNI", "JVMTI", "Renderer", "Modules" };
		const float step_left = card_min.x + 54.0f;
		const float step_width = card_width - 108.0f;
		for (int step = 0; step < 5; ++step)
		{
			const float x = step_left + step_width * (static_cast<float>(step) / 4.0f);
			const bool reached = displayed_progress >= static_cast<float>(step) / 5.0f;
			if (step < 4)
			{
				const float next_x = step_left + step_width * (static_cast<float>(step + 1) / 4.0f);
				draw_list->AddLine(ImVec2(x + 7.0f, card_min.y + 282.0f),
					ImVec2(next_x - 7.0f, card_min.y + 282.0f),
					IM_COL32(48, 49, 58, static_cast<int>(220.0f * alpha)), 1.0f);
			}
			draw_list->AddCircleFilled(ImVec2(x, card_min.y + 282.0f), 4.5f,
				reached ? color_with_alpha(accent, alpha) : IM_COL32(65, 66, 76, static_cast<int>(255.0f * alpha)), 20);
			const ImVec2 label_size = ImGui::CalcTextSize(step_labels[step]);
			draw_list->AddText(ImVec2(x - label_size.x * 0.5f, card_min.y + 297.0f),
				reached ? IM_COL32(171, 173, 184, static_cast<int>(245.0f * alpha))
					: IM_COL32(79, 80, 90, static_cast<int>(230.0f * alpha)), step_labels[step]);
		}
	}

	std::string display_name(const char* name)
	{
		std::string result = name ? name : "Unknown";
		bool capitalize = true;
		for (char& character : result)
		{
			if (capitalize && character >= 'a' && character <= 'z')
				character = static_cast<char>(character - 'a' + 'A');
			capitalize = character == ' ';
		}
		return result;
	}

	const char* module_description(const char* name)
	{
		if (!name) return "";
		struct description_entry { const char* name; const char* english; const char* turkish; };
		static const description_entry entries[] = {
			{ "click gui", "Controls the client language and menu key", "Client dilini ve menu tusunu ayarlar" },
			{ "panic", "Safely closes the client", "Clienti guvenli bicimde kapatir" },
			{ "triggerbot", "Attacks valid targets under the crosshair", "Nişangâhtaki geçerli hedeflere saldırır" },
			{ "mace swap", "Swaps to a mace for the impact, then returns", "Vuruş anında topuza geçip önceki slota döner" },
			{ "anchor macro", "Places, charges and detonates a respawn anchor", "Yeniden doğma çapasını kurar, şarj eder ve patlatır" },
			{ "blatant anchor macro", "Executes the anchor sequence at maximum speed", "Çapa dizisini maksimum hızda uygular" },
			{ "safe anchor", "Runs anchor actions with additional safety checks", "Çapa işlemlerini ek güvenlik kontrolleriyle uygular" },
			{ "hit crystal", "Places and breaks crystals after a manual hit", "Manuel vuruştan sonra kristal koyup kırar" },
			{ "legit click aim assist", "Smooth aim assistance while clicking", "Tıklarken yumuşak nişan desteği sağlar" },
			{ "blatant click aim assist", "Strong target tracking while clicking", "Tıklarken güçlü hedef takibi sağlar" },
			{ "aim assist", "Smoothly guides the crosshair toward targets", "Nişangâhı hedeflere yumuşakça yönlendirir" },
			{ "jump reset", "Times jumps to reduce incoming knockback", "Geri tepmeyi azaltmak için zıplamayı zamanlar" },
			{ "velocity", "Controls horizontal and vertical knockback", "Yatay ve dikey geri tepme miktarını kontrol eder" },
			{ "shield breaker", "Switches to an axe against blocking players", "Kalkan kullanan oyunculara karşı baltaya geçer" },
			{ "auto totem", "Moves available totems to protected slots", "Mevcut totemleri korumalı slotlara taşır" },
			{ "legit auto totem", "Refills totems with human-like inventory input", "Totemleri doğal envanter hareketleriyle doldurur" },
			{ "watermark", "Shows the Thedorius Lite status overlay", "Thedorius Lite durum göstergesini ekrana çizer" },
			{ "arraylist", "Shows enabled modules on the game overlay", "Etkin modülleri oyun ekranında gösterir" },
			{ "glow esp", "Highlights visible players with a glow effect", "Görünür oyuncuları parlama efektiyle vurgular" },
			{ "player esp", "Displays configurable player highlights", "Oyuncuları ayarlanabilir vurgularla gösterir" },
			{ "storage esp", "Highlights nearby storage blocks", "Yakındaki depolama bloklarını vurgular" },
			{ "block esp", "Highlights only the block types you select", "Yalnızca seçtiğin blok türlerini vurgular" },
			{ "radar", "Shows nearby players on a compact radar", "Yakındaki oyuncuları kompakt radarda gösterir" },
			{ "full bright", "Brightens dark areas without changing the world", "Dünyayı değiştirmeden karanlık alanları aydınlatır" },
			{ "nametag", "Replaces player labels with detailed nametags", "Oyuncu etiketlerini ayrıntılı nametaglerle değiştirir" },
			{ "pearl macro", "Throws a pearl and returns to the previous slot", "İnci atıp önceki slota geri döner" },
			{ "chest stealer", "Moves chest contents into the inventory", "Sandık içeriğini envantere taşır" },
			{ "elytra swapper", "Switches between an elytra and chestplate", "Elytra ile göğüslük arasında geçiş yapar" },
			{ "auto tool", "Selects the best tool for the targeted block", "Bakılan blok için en uygun aleti seçer" },
			{ "auto armor", "Equips the strongest available armor", "Mevcut en güçlü zırhı kuşanır" },
			{ "bridge assist", "Sneaks automatically near block edges", "Blok kenarlarında otomatik olarak eğilir" },
			{ "sprint", "Keeps sprint active while moving", "Hareket ederken koşmayı etkin tutar" },
			{ "trajectories", "Previews projectile flight paths", "Fırlatılabilir eşyaların uçuş yolunu gösterir" },
			{ "scaffold", "Automatically places bridge blocks beneath you", "Altınıza otomatik olarak köprü bloğu yerleştirir" },
			{ "right clicker", "Generates configurable right clicks", "Ayarlanabilir sağ tıklamalar üretir" },
			{ "left clicker", "Generates configurable left clicks", "Ayarlanabilir sol tıklamalar üretir" },
			{ "inventory clicker", "Accelerates repeated inventory clicks", "Tekrarlanan envanter tıklamalarını hızlandırır" },
			{ "fast place", "Reduces delay between block placements", "Blok yerleştirmeleri arasındaki gecikmeyi azaltır" },
			{ "fast exp", "Throws experience bottles at adjustable speed", "Tecrübe şişelerini ayarlanabilir hızda kullanır" },
			{ "mouse tweaks", "Improves inventory dragging and item movement", "Envanter sürükleme ve eşya taşıma işlemlerini iyileştirir" },
			{ "pot refill", "Refills the hotbar with Instant Health potions", "Hotbarı Anında Sağlık iksirleriyle doldurur" }
		};
		for (const description_entry& entry : entries)
			if (std::strcmp(entry.name, name) == 0)
				return gui::text(entry.english, entry.turkish);
		return gui::text("Client module", "Client modülü");
	}

	std::string ellipsize_text(ImFont* font, float size, const char* text, float maximum_width)
	{
		if (!font || !text || maximum_width <= 4.0f) return {};
		if (font->CalcTextSizeA(size, FLT_MAX, 0.0f, text).x <= maximum_width) return text;
		constexpr const char* ellipsis = "...";
		const float ellipsis_width = font->CalcTextSizeA(size, FLT_MAX, 0.0f, ellipsis).x;
		const char* remaining = text;
		font->CalcTextSizeA(size, std::max(1.0f, maximum_width - ellipsis_width), 0.0f,
			text, nullptr, &remaining);
		return std::string(text, remaining) + ellipsis;
	}

	const char* display_category(const char* category)
	{
		if (!category) return "";
		if (std::strcmp(category, "Movement") == 0) return gui::text("Movement", "Hareket");
		if (std::strcmp(category, "Player") == 0) return gui::text("Player", "Oyuncu");
		if (std::strcmp(category, "World") == 0) return gui::text("World", "Dunya");
		if (std::strcmp(category, "Combat") == 0) return gui::text("Combat", "Savaş");
		if (std::strcmp(category, "Misc") == 0) return gui::text("Misc", "Diğer");
		if (std::strcmp(category, "Render") == 0) return gui::text("Render", "Görüntüleme");
		if (std::strcmp(category, "Visual") == 0) return gui::text("Visuals", "Görseller");
		if (std::strcmp(category, "Friends") == 0) return gui::text("Friends", "Arkadaşlar");
		if (std::strcmp(category, "Client") == 0) return "Client";
		return category;
	}

	enum class sidebar_icon
	{
		combat,
		friends,
		render,
		visual,
		misc,
		configs,
		settings
	};

	const char* sidebar_icon_glyph(sidebar_icon icon)
	{
		switch (icon)
		{
		case sidebar_icon::combat:   return "\xEF\x81\x9B"; // Font Awesome: crosshairs (f05b)
		case sidebar_icon::friends:  return "\xEF\x83\x80"; // users (f0c0)
		case sidebar_icon::render:   return "\xEE\x85\xA3"; // display (e163)
		case sidebar_icon::visual:   return "\xEF\x81\xAE"; // eye (f06e)
		case sidebar_icon::misc:     return "\xEF\x84\xA1"; // code (f121)
		case sidebar_icon::configs:  return "\xEF\x81\xBC"; // folder-open (f07c)
		case sidebar_icon::settings: return "\xEF\x87\x9E"; // sliders (f1de)
		default:                     return "";
		}
	}

	void draw_sidebar_icon(ImDrawList* draw_list, sidebar_icon icon, const ImVec2& center, ImU32 color)
	{
		if (gui::icon_font)
		{
			const char* glyph = sidebar_icon_glyph(icon);
			const float icon_size = 20.0f;
			const ImVec2 glyph_size = gui::icon_font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, glyph);
			draw_list->AddText(
				gui::icon_font,
				icon_size,
				ImVec2(std::floor(center.x - glyph_size.x * 0.5f), std::floor(center.y - glyph_size.y * 0.5f)),
				color,
				glyph);
			return;
		}

		// Font yüklenemezse sade ve düzgün bir fallback işareti çiz.
		draw_list->AddCircleFilled(center, 7.0f, color, 32);
		draw_list->AddCircleFilled(center, 3.2f, IM_COL32(18, 18, 20, 255), 24);
	}

	bool sidebar_button(const char* label, sidebar_icon icon, bool selected, const ImVec2& size)
	{
		static std::unordered_map<ImGuiID, float> animations;
		const ImVec2 position = ImGui::GetCursorScreenPos();
		const std::string id = std::string("##sidebar_") + label;
		ImGui::InvisibleButton(id.c_str(), size);
		const bool hovered = ImGui::IsItemHovered();
		const bool clicked = ImGui::IsItemClicked();
		const ImGuiID item_id = ImGui::GetItemID();
		float& animation = animations[item_id];
		animation = smooth_value(animation, selected ? 1.0f : 0.0f, 13.0f);

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(0);
		if (animation > 0.001f)
		{
			draw_list->AddRectFilled(
				position,
				ImVec2(position.x + size.x, position.y + size.y),
				color_with_alpha(accent, animation),
				size.y * 0.48f);
		}

		const ImVec4 idle_color(0.48f, 0.48f, 0.52f, hovered ? 1.0f : 0.88f);
		const ImVec4 selected_color(0.10f, 0.09f, 0.12f, 1.0f);
		const ImU32 content_color = mix_color(idle_color, selected_color, animation);
		draw_sidebar_icon(draw_list, icon, ImVec2(position.x + 21.0f, position.y + size.y * 0.5f), content_color);
		const ImVec2 text_size = ImGui::CalcTextSize(label);
		draw_list->AddText(ImVec2(position.x + 42.0f, position.y + (size.y - text_size.y) * 0.5f), content_color, label);
		return clicked;
	}

	bool toggle_switch(const char* id, bool* value, const ImVec2& position, float width = 43.0f, float height = 23.0f)
	{
		static std::unordered_map<ImGuiID, float> animations;
		ImGui::SetCursorScreenPos(position);
		ImGui::InvisibleButton(id, ImVec2(width, height));
		const bool changed = ImGui::IsItemClicked();
		if (changed)
			*value = !*value;

		const bool hovered = ImGui::IsItemHovered();
		const ImGuiID item_id = ImGui::GetItemID();
		const auto animation_result = animations.try_emplace(item_id, *value ? 1.0f : 0.0f);
		float& animation = animation_result.first->second;
		animation = smooth_value(animation, *value ? 1.0f : 0.0f, 16.0f);

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImVec4 off_color(0.12f, 0.12f, 0.14f, 1.0f);
		ImVec4 on_color = gui::get_dynamic_color(80).Value;
		on_color.w = 1.0f;
		const ImU32 track_color = mix_color(off_color, on_color, animation);
		const float radius = height * 0.5f;
		draw_list->AddRectFilled(position, ImVec2(position.x + width, position.y + height), track_color, radius);
		draw_list->AddRect(position, ImVec2(position.x + width, position.y + height), IM_COL32(255, 255, 255, hovered ? 28 : 14), radius, 0, 1.0f);
		const float knob_x = position.x + radius + (width - height) * animation;
		draw_list->AddCircleFilled(ImVec2(knob_x, position.y + radius), radius - 3.0f, IM_COL32(210, 212, 218, 255), 24);
		return changed;
	}

	void draw_gear(ImDrawList* draw_list, const ImVec2& center, ImU32 color)
	{
		if (gui::icon_font)
		{
			const char* glyph = sidebar_icon_glyph(sidebar_icon::settings);
			const float icon_size = 17.0f;
			const ImVec2 glyph_size = gui::icon_font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, glyph);
			draw_list->AddText(
				gui::icon_font,
				icon_size,
				ImVec2(std::floor(center.x - glyph_size.x * 0.5f), std::floor(center.y - glyph_size.y * 0.5f)),
				color,
				glyph);
			return;
		}

		draw_list->AddCircle(center, 5.0f, color, 20, 1.7f);
		draw_list->AddCircleFilled(center, 1.7f, color, 14);
		for (int i = 0; i < 8; ++i)
		{
			const float angle = static_cast<float>(i) * 3.14159265f * 0.25f;
			const ImVec2 direction(std::cos(angle), std::sin(angle));
			draw_list->AddLine(
				ImVec2(center.x + direction.x * 5.2f, center.y + direction.y * 5.2f),
				ImVec2(center.x + direction.x * 7.2f, center.y + direction.y * 7.2f),
				color,
				1.8f);
		}
	}

	bool config_icon_button(
		const char* id,
		const char* glyph,
		const char* fallback,
		const char* tooltip,
		bool danger = false)
	{
		ImGui::PushID(id);
		const ImVec2 position = ImGui::GetCursorScreenPos();
		const ImVec2 size(30.0f, 30.0f);
		ImGui::InvisibleButton("##config_action", size);
		const bool hovered = ImGui::IsItemHovered();
		const bool held = ImGui::IsItemActive();
		const bool clicked = ImGui::IsItemClicked();
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(20);

		if (hovered || held)
			draw_list->AddRectFilled(
				position,
				ImVec2(position.x + size.x, position.y + size.y),
				danger ? IM_COL32(126, 32, 48, held ? 115 : 78)
					: color_with_alpha(accent, held ? 0.22f : 0.13f),
				7.0f);

		ImFont* font = gui::icon_font ? gui::icon_font : ImGui::GetFont();
		const char* icon = gui::icon_font ? glyph : fallback;
		const float icon_size = gui::icon_font ? 16.0f : 14.0f;
		const ImVec2 extent = font->CalcTextSizeA(icon_size, FLT_MAX, 0.0f, icon);
		const ImU32 color = danger
			? (hovered ? IM_COL32(247, 87, 103, 255) : IM_COL32(137, 138, 148, 255))
			: (hovered ? color_with_alpha(accent, 1.0f) : IM_COL32(137, 138, 148, 255));
		draw_list->AddText(
			font,
			icon_size,
			ImVec2(
				position.x + std::floor((size.x - extent.x) * 0.5f),
				position.y + std::floor((size.y - extent.y) * 0.5f)),
			color,
			icon);

		if (hovered && tooltip && *tooltip)
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(tooltip);
			ImGui::EndTooltip();
		}
		ImGui::PopID();
		return clicked;
	}

	void render_module_card(modules::module* module, float width)
	{
		if (!module || width < 180.0f)
			return;

		ImGui::PushID(module);
		const bool has_options = module->get_options_height() > 45.0f;
		const float card_height = 72.0f;
		const float card_gap = 8.0f;
		const ImVec2 card_min = ImGui::GetCursorScreenPos();
		const ImVec2 card_max(card_min.x + width, card_min.y + card_height);

		ImGui::SetCursorScreenPos(card_min);
		ImGui::InvisibleButton("##module_card", ImVec2(width - (has_options ? 106.0f : 68.0f), card_height));
		const bool card_hovered = ImGui::IsItemHovered();
		if (has_options && ImGui::IsItemClicked())
			selected_settings_module = module;

		const ImVec2 gear_min(card_max.x - 96.0f, card_min.y + 22.0f);
		bool gear_hovered = false;
		if (has_options)
		{
			ImGui::SetCursorScreenPos(gear_min);
			ImGui::InvisibleButton("##module_settings", ImVec2(28.0f, 28.0f));
			gear_hovered = ImGui::IsItemHovered();
			if (ImGui::IsItemClicked())
				selected_settings_module = module;
		}

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(40);
		const ImU32 card_color = card_hovered || gear_hovered
			? IM_COL32(31, 31, 36, 250)
			: IM_COL32(25, 25, 29, 247);

		draw_list->AddRectFilled(
			ImVec2(card_min.x + 1.0f, card_min.y + 3.0f),
			ImVec2(card_max.x + 1.0f, card_max.y + 3.0f),
			IM_COL32(0, 0, 0, 38), 13.0f);
		draw_list->AddRectFilled(card_min, card_max, card_color, 13.0f);
		draw_list->AddRect(
			card_min, card_max,
			module->enabled ? color_with_alpha(accent, 0.22f) : IM_COL32(255, 255, 255, 10),
			13.0f, 0, 1.0f);
		const std::string title = display_name(module->get_name());
		ImFont* title_font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
		draw_list->AddText(
			title_font,
			17.0f,
			ImVec2(card_min.x + 20.0f, card_min.y + 14.0f),
			IM_COL32(235, 235, 241, 255), title.c_str());
		const char* description = module_description(module->get_name());
		const float description_right = card_max.x - (has_options ? 111.0f : 70.0f);
		const ImVec4 description_clip(
			card_min.x + 20.0f, card_min.y + 36.0f,
			description_right, card_min.y + 61.0f);
		draw_list->AddText(
			ImGui::GetFont(), 14.0f,
			ImVec2(card_min.x + 20.0f, card_min.y + 39.0f),
			IM_COL32(119, 120, 130, 255), description, nullptr, 0.0f, &description_clip);

		if (has_options)
			draw_gear(
				draw_list,
				ImVec2(gear_min.x + 14.0f, gear_min.y + 14.0f),
				gear_hovered ? color_with_alpha(accent) : IM_COL32(104, 104, 114, 255));

		const ImVec2 toggle_position(card_max.x - 55.0f, card_min.y + 24.5f);
		toggle_switch("##module_toggle", &module->enabled, toggle_position, 43.0f, 23.0f);

		ImGui::SetCursorScreenPos(ImVec2(card_min.x, card_max.y + card_gap));
		ImGui::PopID();
	}

	void render_module_settings(modules::module* module, float width, float height)
	{
		if (!module)
			return;

		ImGui::PushID(module);
		const float panel_width = std::min(620.0f, width);
		const float panel_height = std::max(300.0f, height - 4.0f);
		const ImVec2 content_min = ImGui::GetCursorScreenPos();
		const ImVec2 panel_min(content_min.x + std::max(0.0f, (width - panel_width) * 0.5f), content_min.y);
		const ImVec2 panel_max(panel_min.x + panel_width, panel_min.y + panel_height);
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(25);

		constexpr ImU32 settings_surface = IM_COL32(17, 18, 21, 253);
		draw_list->AddRectFilled(panel_min, panel_max, settings_surface, 12.0f);
		draw_list->AddRect(panel_min, panel_max, IM_COL32(255, 255, 255, 13), 12.0f, 0, 1.0f);
		const float header_height = 96.0f;
		draw_list->AddLine(
			ImVec2(panel_min.x + 18.0f, panel_min.y + 55.0f),
			ImVec2(panel_max.x - 18.0f, panel_min.y + 55.0f),
			IM_COL32(255, 255, 255, 14));

		const ImVec2 close_min(panel_max.x - 39.0f, panel_min.y + 12.0f);
		ImGui::SetCursorScreenPos(close_min);
		ImGui::InvisibleButton("##settings_close", ImVec2(27.0f, 27.0f));
		const bool close_hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked())
			selected_settings_module = nullptr;
		draw_list->AddRectFilled(
			close_min, ImVec2(close_min.x + 27.0f, close_min.y + 27.0f),
			close_hovered ? color_with_alpha(accent, 0.18f) : IM_COL32(34, 35, 40, 255), 13.5f);
		const ImU32 close_color = close_hovered ? color_with_alpha(accent) : IM_COL32(155, 156, 165, 255);
		draw_list->AddLine(ImVec2(close_min.x + 9.0f, close_min.y + 9.0f), ImVec2(close_min.x + 18.0f, close_min.y + 18.0f), close_color, 1.6f);
		draw_list->AddLine(ImVec2(close_min.x + 18.0f, close_min.y + 9.0f), ImVec2(close_min.x + 9.0f, close_min.y + 18.0f), close_color, 1.6f);

		const std::string title = display_name(module->get_name());
		ImFont* title_font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
		draw_list->AddText(
			title_font, 19.0f,
			ImVec2(panel_min.x + 18.0f, panel_min.y + 16.0f),
			IM_COL32(239, 240, 245, 255), title.c_str());

		const ImVec2 toggle_position(panel_max.x - 58.0f, panel_min.y + 65.5f);
		const char* enabled_label = gui::text("Enabled", "Etkin");
		const ImVec2 enabled_label_size = ImGui::CalcTextSize(enabled_label);
		draw_list->AddText(
			ImVec2(panel_min.x + 18.0f, toggle_position.y + (21.0f - enabled_label_size.y) * 0.5f),
			IM_COL32(203, 204, 212, 255), enabled_label);
		toggle_switch("##settings_module_toggle", &module->enabled, toggle_position, 40.0f, 21.0f);
		const ImVec2 bind_position(panel_max.x - 140.0f, panel_min.y + 63.0f);
		ImGui::SetCursorScreenPos(bind_position);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(36, 37, 42, 255));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, color_with_alpha(accent, 0.22f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, color_with_alpha(accent, 0.32f));
		module->key_bind_selector();
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();

		ImGui::SetCursorScreenPos(ImVec2(panel_min.x + 9.0f, panel_min.y + header_height + 6.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 10.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 9.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 7.0f);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(17.0f / 255.0f, 18.0f / 255.0f, 21.0f / 255.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.105f, 0.107f, 0.126f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.14f, 0.142f, 0.17f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.16f, 0.162f, 0.19f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, color_with_alpha(accent, 0.92f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, color_with_alpha(accent));
		ImGui::PushStyleColor(ImGuiCol_CheckMark, color_with_alpha(accent));
		if (ImGui::BeginChild(
			"##module_settings_scroll",
			ImVec2(panel_width - 18.0f, panel_height - header_height - 12.0f),
			ImGuiChildFlags_None))
		{
			ImGui::PushItemWidth(-150.0f);
			ImGui::PushTextWrapPos(0.0f);
			module->render_options();
			ImGui::PopTextWrapPos();
			ImGui::PopItemWidth();
			ImGui::Dummy(ImVec2(0.0f, 8.0f));
		}
		ImGui::EndChild();
		ImGui::PopStyleColor(8);
		ImGui::PopStyleVar(4);

		ImGui::SetCursorScreenPos(ImVec2(content_min.x, panel_max.y));
		ImGui::PopID();
	}

	void render_module_category(const char* category)
	{
		if (category && std::strcmp(category, "Friends") == 0)
		{
			selected_settings_module = nullptr;
			friends::render_manager();
			return;
		}

		const float width = ImGui::GetContentRegionAvail().x - 2.0f;
		const float height = ImGui::GetContentRegionAvail().y;
		if (selected_settings_module &&
			std::strcmp(selected_settings_module->get_category(), category) == 0)
		{
			ImGui::SetScrollY(0.0f);
			render_module_settings(selected_settings_module, width, height);
			return;
		}
		selected_settings_module = nullptr;

		for (modules::module* module : modules::get_modules())
		{
			if (module && std::strcmp(module->get_category(), category) == 0)
				render_module_card(module, width);
		}
	}

	struct theme_preview
	{
		const char* name;
		ImU32 color;
	};

	bool theme_card(const theme_preview& theme, int index, const ImVec2& position, const ImVec2& size)
	{
		ImGui::PushID(index);
		ImGui::SetCursorScreenPos(position);
		ImGui::InvisibleButton("##theme_card", size);
		const bool hovered = ImGui::IsItemHovered();
		const bool selected = gui::active_theme == index;
		const bool clicked = ImGui::IsItemClicked();
		if (clicked)
			gui::active_theme = index;

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImU32 border = selected
			? color_with_alpha(gui::get_dynamic_color(0), 0.88f)
			: IM_COL32(255, 255, 255, hovered ? 22 : 9);
		const ImU32 background = hovered ? IM_COL32(25, 28, 35, 255) : IM_COL32(18, 21, 27, 255);

		draw_list->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y), background, 10.0f);
		draw_list->AddRect(position, ImVec2(position.x + size.x, position.y + size.y), border, 10.0f, 0, selected ? 1.6f : 1.0f);
		ImFont* bold = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
		const ImVec2 label_size = bold->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, theme.name);
		draw_list->AddText(bold, 14.0f,
			ImVec2(position.x + (size.x - label_size.x) * 0.5f, position.y + size.y - 25.0f),
			IM_COL32(232, 234, 241, 255), theme.name);
		if (selected)
		{
			const ImVec2 check(position.x + size.x - 14.0f, position.y + 14.0f);
			draw_list->AddCircleFilled(check, 7.0f, color_with_alpha(gui::get_dynamic_color(0), 0.96f), 18);
			draw_list->AddLine(ImVec2(check.x - 3.0f, check.y), ImVec2(check.x - 0.5f, check.y + 2.5f),
				IM_COL32(250, 250, 252, 255), 1.5f);
			draw_list->AddLine(ImVec2(check.x - 0.5f, check.y + 2.5f), ImVec2(check.x + 4.0f, check.y - 3.0f),
				IM_COL32(250, 250, 252, 255), 1.5f);
		}

		const ImVec4 full = ImGui::ColorConvertU32ToFloat4(theme.color);
		const auto faded = [&full](float alpha)
		{
			return ImGui::ColorConvertFloat4ToU32(ImVec4(full.x, full.y, full.z, alpha));
		};

		// Three concentric tonal chips make the palette recognizable without imitating a module card.
		const ImVec2 center(position.x + size.x * 0.5f, position.y + 31.0f);
		draw_list->AddCircleFilled(ImVec2(center.x - 17.0f, center.y + 2.0f), 12.0f, faded(0.30f), 28);
		draw_list->AddCircleFilled(center, 14.0f, faded(0.62f), 32);
		draw_list->AddCircleFilled(ImVec2(center.x + 18.0f, center.y + 1.0f), 11.0f, theme.color, 28);
		draw_list->AddCircle(ImVec2(center.x + 18.0f, center.y + 1.0f), 11.0f,
			IM_COL32(255, 255, 255, selected ? 80 : 28), 28, 1.0f);

		ImGui::PopID();
		return clicked;
	}

	void settings_theme_grid()
	{
		const theme_preview themes[] =
		{
			{ "Thedorius", IM_COL32(122, 105, 255, 255) },
			{ "Crimson", IM_COL32(235, 68, 92, 255) },
			{ "Emerald", IM_COL32(62, 207, 128, 255) },
			{ "Ocean", IM_COL32(61, 145, 244, 255) },
			{ "Rose", IM_COL32(238, 90, 157, 255) },
			{ "Cyber", IM_COL32(24, 204, 211, 255) },
			{ "Sunset", IM_COL32(248, 145, 52, 255) },
			{ "Mono", IM_COL32(207, 208, 217, 255) }
		};

		const ImVec2 start = ImGui::GetCursorScreenPos();
		const float available_width = ImGui::GetContentRegionAvail().x;
		const float gap = 8.0f;
		const int columns = 4;
		const ImVec2 card_size((available_width - gap * (columns - 1)) / columns, 76.0f);
		for (int index = 0; index < IM_ARRAYSIZE(themes); ++index)
		{
			const int column = index % columns;
			const int row = index / columns;
			theme_card(themes[index], index, ImVec2(start.x + column * (card_size.x + gap), start.y + row * (card_size.y + gap)), card_size);
		}
		ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + card_size.y * 2.0f + gap));
	}

	bool language_card(int language, const ImVec2& position, const ImVec2& size)
	{
		ImGui::PushID(language);
		ImGui::SetCursorScreenPos(position);
		ImGui::InvisibleButton("##language_card", size);
		const bool hovered = ImGui::IsItemHovered();
		const bool selected = gui::active_language == language;
		const bool clicked = ImGui::IsItemClicked();
		if (clicked)
			gui::active_language = language;

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(language * 35);
		draw_list->AddRectFilled(
			position,
			ImVec2(position.x + size.x, position.y + size.y),
			hovered ? IM_COL32(28, 28, 34, 255) : IM_COL32(20, 20, 25, 255),
			9.0f);
		draw_list->AddRect(
			position,
			ImVec2(position.x + size.x, position.y + size.y),
			selected ? color_with_alpha(accent, 0.95f) : IM_COL32(255, 255, 255, 14),
			9.0f, 0, selected ? 1.6f : 1.0f);

		const ImVec2 flag_min(position.x + 12.0f, position.y + 13.0f);
		const ImVec2 flag_max(flag_min.x + 42.0f, flag_min.y + 26.0f);
		if (language == 0)
		{
			draw_list->AddRectFilled(flag_min, flag_max, IM_COL32(28, 54, 112, 255), 3.0f);
			draw_list->AddLine(flag_min, flag_max, IM_COL32(245, 245, 245, 255), 7.0f);
			draw_list->AddLine(ImVec2(flag_max.x, flag_min.y), ImVec2(flag_min.x, flag_max.y), IM_COL32(245, 245, 245, 255), 7.0f);
			draw_list->AddLine(flag_min, flag_max, IM_COL32(205, 38, 55, 255), 3.0f);
			draw_list->AddLine(ImVec2(flag_max.x, flag_min.y), ImVec2(flag_min.x, flag_max.y), IM_COL32(205, 38, 55, 255), 3.0f);
			draw_list->AddRectFilled(ImVec2(flag_min.x, flag_min.y + 9.0f), ImVec2(flag_max.x, flag_min.y + 17.0f), IM_COL32(245, 245, 245, 255));
			draw_list->AddRectFilled(ImVec2(flag_min.x + 16.0f, flag_min.y), ImVec2(flag_min.x + 26.0f, flag_max.y), IM_COL32(245, 245, 245, 255));
			draw_list->AddRectFilled(ImVec2(flag_min.x, flag_min.y + 11.0f), ImVec2(flag_max.x, flag_min.y + 15.0f), IM_COL32(205, 38, 55, 255));
			draw_list->AddRectFilled(ImVec2(flag_min.x + 19.0f, flag_min.y), ImVec2(flag_min.x + 23.0f, flag_max.y), IM_COL32(205, 38, 55, 255));
		}
		else
		{
			draw_list->AddRectFilled(flag_min, flag_max, IM_COL32(220, 32, 45, 255), 3.0f);
			const ImVec2 moon(flag_min.x + 18.0f, flag_min.y + 13.0f);
			draw_list->AddCircleFilled(moon, 7.0f, IM_COL32(255, 255, 255, 255), 28);
			draw_list->AddCircleFilled(ImVec2(moon.x + 3.0f, moon.y), 5.8f, IM_COL32(220, 32, 45, 255), 28);
			draw_list->AddCircleFilled(ImVec2(flag_min.x + 29.0f, flag_min.y + 13.0f), 2.2f, IM_COL32(255, 255, 255, 255), 5);
		}

		const char* title = language == 0 ? "English" : "Türkçe";
		draw_list->AddText(ImVec2(position.x + 65.0f, position.y + 18.0f), IM_COL32(225, 225, 232, 255), title);
		const ImVec2 selection(position.x + size.x - 18.0f, position.y + size.y * 0.5f);
		draw_list->AddCircle(selection, 5.5f, selected ? color_with_alpha(accent, 0.95f) : IM_COL32(89, 90, 101, 255), 18, 1.2f);
		if (selected)
			draw_list->AddCircleFilled(selection, 2.6f, color_with_alpha(accent, 0.95f), 14);
		ImGui::PopID();
		return clicked;
	}

	void settings_language_row()
	{
		const ImVec2 start = ImGui::GetCursorScreenPos();
		const float available_width = ImGui::GetContentRegionAvail().x;
		const float height = 36.0f;
		const float segment_width = available_width * 0.5f;
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		draw_list->AddRectFilled(start, ImVec2(start.x + available_width, start.y + height),
			IM_COL32(255, 255, 255, 7), 8.0f);
		const char* labels[] = { "English", "T\xC3\xBCrk\xC3\xA7" "e" };
		for (int language = 0; language < 2; ++language)
		{
			ImGui::PushID(language);
			const ImVec2 minimum(start.x + segment_width * language, start.y);
			ImGui::SetCursorScreenPos(minimum);
			ImGui::InvisibleButton("##language_segment", ImVec2(segment_width, height));
			const bool hovered = ImGui::IsItemHovered();
			if (ImGui::IsItemClicked()) gui::active_language = language;
			const bool selected = gui::active_language == language;
			if (selected || hovered)
				draw_list->AddRectFilled(ImVec2(minimum.x + 2.0f, minimum.y + 2.0f),
					ImVec2(minimum.x + segment_width - 2.0f, minimum.y + height - 2.0f),
					selected ? color_with_alpha(gui::get_dynamic_color(0), 0.20f) : IM_COL32(255, 255, 255, 7), 7.0f);
			const ImVec2 extent = ImGui::CalcTextSize(labels[language]);
			draw_list->AddText(ImVec2(minimum.x + (segment_width - extent.x) * 0.5f,
				minimum.y + (height - extent.y) * 0.5f), selected ? color_with_alpha(gui::get_dynamic_color(0))
				: IM_COL32(193, 196, 206, 255), labels[language]);
			ImGui::PopID();
		}
		ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + height));
	}

	std::string menu_key_name(int virtual_key)
	{
		switch (virtual_key)
		{
		case VK_INSERT: return "INSERT";
		case VK_DELETE: return "DELETE";
		case VK_HOME: return "HOME";
		case VK_END: return "END";
		case VK_PRIOR: return "PAGE UP";
		case VK_NEXT: return "PAGE DOWN";
		case VK_SPACE: return "SPACE";
		case VK_TAB: return "TAB";
		case VK_SHIFT: case VK_LSHIFT: return "LEFT SHIFT";
		case VK_RSHIFT: return "RIGHT SHIFT";
		case VK_CONTROL: case VK_LCONTROL: return "LEFT CTRL";
		case VK_RCONTROL: return "RIGHT CTRL";
		case VK_MENU: case VK_LMENU: return "LEFT ALT";
		case VK_RMENU: return "RIGHT ALT";
		case VK_MBUTTON: return "MOUSE 3";
		case VK_XBUTTON1: return "MOUSE 4";
		case VK_XBUTTON2: return "MOUSE 5";
		default: break;
		}

		UINT scan_code = MapVirtualKeyW(static_cast<UINT>(virtual_key), MAPVK_VK_TO_VSC);
		if (virtual_key == VK_LEFT || virtual_key == VK_UP || virtual_key == VK_RIGHT ||
			virtual_key == VK_DOWN || virtual_key == VK_PRIOR || virtual_key == VK_NEXT ||
			virtual_key == VK_END || virtual_key == VK_HOME || virtual_key == VK_INSERT ||
			virtual_key == VK_DELETE || virtual_key == VK_DIVIDE || virtual_key == VK_NUMLOCK)
			scan_code |= 0x100;
		char name[64]{};
		if (GetKeyNameTextA(static_cast<LONG>(scan_code << 16), name, static_cast<int>(sizeof(name))) > 0)
			return name;
		char fallback[16]{};
		sprintf_s(fallback, "KEY %d", virtual_key);
		return fallback;
	}

	void settings_menu_key_row()
	{
		const ImVec2 start = ImGui::GetCursorScreenPos();
		const float width = ImGui::GetContentRegionAvail().x;
		const bool compact = width < 360.0f;
		const float height = compact ? 78.0f : 48.0f;
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		draw_list->AddRectFilled(start, ImVec2(start.x + width, start.y + height),
			IM_COL32(20, 20, 25, 255), 9.0f);
		draw_list->AddRect(start, ImVec2(start.x + width, start.y + height),
			IM_COL32(255, 255, 255, 14), 9.0f);
		draw_list->AddText(ImVec2(start.x + 13.0f, start.y + 8.0f),
			IM_COL32(226, 227, 234, 255), gui::text("Menu Key", "Menü Tuşu"));
		if (!compact)
			draw_list->AddText(ImVec2(start.x + 13.0f, start.y + 27.0f),
			IM_COL32(115, 116, 128, 255), gui::text("Open or close the client UI", "Client arayüzünü aç veya kapat"));

		const ImVec2 button_size(116.0f, 30.0f);
		const ImVec2 button_min(start.x + width - button_size.x - 9.0f, start.y + (compact ? 39.0f : 9.0f));
		ImGui::SetCursorScreenPos(button_min);
		ImGui::InvisibleButton("##menu_key_bind", button_size);
		const bool hovered = ImGui::IsItemHovered();
		const bool clicked = ImGui::IsItemClicked();
		const ImU32 button_color = gui::menu_key_scanning
			? color_with_alpha(gui::get_dynamic_color(0), 0.22f)
			: (hovered ? IM_COL32(39, 39, 47, 255) : IM_COL32(31, 31, 38, 255));
		draw_list->AddRectFilled(button_min,
			ImVec2(button_min.x + button_size.x, button_min.y + button_size.y), button_color, 7.0f);
		draw_list->AddRect(button_min,
			ImVec2(button_min.x + button_size.x, button_min.y + button_size.y),
			gui::menu_key_scanning ? color_with_alpha(gui::get_dynamic_color(0), 0.85f) : IM_COL32(255, 255, 255, 13), 7.0f);

		if (clicked)
		{
			gui::menu_key_scanning = true;
			gui::reset_last_pressed_key();
		}
		if (gui::menu_key_scanning)
		{
			const int pressed = gui::get_last_pressed_key();
			if (pressed == VK_ESCAPE)
			{
				gui::menu_key_scanning = false;
				gui::reset_last_pressed_key();
			}
			else if (pressed > 0 && pressed <= 255 && pressed != VK_LBUTTON && pressed != VK_RBUTTON)
			{
				gui::menu_key = pressed;
				gui::menu_key_scanning = false;
				gui::suppress_menu_key_until_release = true;
				gui::reset_last_pressed_key();
			}
		}

		const std::string value = gui::menu_key_scanning
			? gui::text("PRESS A KEY", "TUŞA BAS")
			: menu_key_name(gui::menu_key);
		const ImVec2 text_size = ImGui::CalcTextSize(value.c_str());
		draw_list->AddText(ImVec2(
			button_min.x + (button_size.x - text_size.x) * 0.5f,
			button_min.y + (button_size.y - text_size.y) * 0.5f),
			gui::menu_key_scanning ? color_with_alpha(gui::get_dynamic_color(0), 0.98f) : IM_COL32(218, 219, 227, 255),
			value.c_str());
		ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + height));
	}

	void render_settings_page()
	{
		gui::page_header(
			gui::text("Settings", "Ayarlar"),
			gui::text("Personalize Thedorius Lite", "Thedorius Lite görünümünü kişiselleştir"));
		if (gui::begin_surface("##settings_surface", ImVec2(0.0f, 0.0f), ImVec2(15.0f, 13.0f)))
		{
			ImFont* heading_font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
			auto& preferences = gui::aegis::get_preferences();
			ImGui::GetWindowDrawList()->AddText(
				heading_font, 16.0f, ImGui::GetCursorScreenPos(), IM_COL32(229, 230, 237, 255),
				gui::text("Interface", "Arayüz"));
			ImGui::Dummy(ImVec2(0.0f, 26.0f));
			const bool old_reduce_motion = preferences.reduce_motion;
			gui::option_toggle(gui::text("Reduce Motion", "Hareketi Azalt"), &preferences.reduce_motion);
			if (preferences.reduce_motion != old_reduce_motion) gui::aegis::mark_preferences_dirty();
			if (gui::action_button(gui::text("Reset UI preferences", "Arayüz tercihlerini sıfırla"),
				ImVec2(176.0f, 29.0f)))
			{
				gui::aegis::reset_preferences();
			}
			ImGui::Dummy(ImVec2(0.0f, 12.0f));
			const ImVec2 interface_separator = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddLine(interface_separator,
				ImVec2(interface_separator.x + ImGui::GetContentRegionAvail().x, interface_separator.y),
				IM_COL32(255, 255, 255, 13));
			ImGui::Dummy(ImVec2(0.0f, 15.0f));
			ImGui::GetWindowDrawList()->AddText(
				heading_font, 16.0f, ImGui::GetCursorScreenPos(), IM_COL32(229, 230, 237, 255),
				gui::text("Theme", "Tema"));
			ImGui::Dummy(ImVec2(0.0f, 25.0f));
			settings_theme_grid();
			ImGui::Dummy(ImVec2(0.0f, 12.0f));
			const ImVec2 separator = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddLine(
				separator,
				ImVec2(separator.x + ImGui::GetContentRegionAvail().x, separator.y),
				IM_COL32(255, 255, 255, 13));
			ImGui::Dummy(ImVec2(0.0f, 16.0f));
			ImGui::GetWindowDrawList()->AddText(
				heading_font, 16.0f, ImGui::GetCursorScreenPos(), IM_COL32(229, 230, 237, 255),
				gui::text("Language", "Dil"));
			ImGui::Dummy(ImVec2(0.0f, 25.0f));
			settings_language_row();
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			const ImVec2 input_separator = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddLine(
				input_separator,
				ImVec2(input_separator.x + ImGui::GetContentRegionAvail().x, input_separator.y),
				IM_COL32(255, 255, 255, 13));
			ImGui::Dummy(ImVec2(0.0f, 10.0f));
			settings_menu_key_row();
		}
		gui::end_surface();
	}

	void render_theme_page()
	{
		gui::page_header(
			gui::text("Themes", "Temalar"),
			gui::text("Select an accent palette", "Bir vurgu paleti sec"));
		if (gui::begin_surface("##themes_only_surface", ImVec2(0.0f, 0.0f), ImVec2(14.0f, 14.0f)))
		{
			settings_theme_grid();
			ImGui::Dummy(ImVec2(0.0f, 8.0f));
			ImGui::TextDisabled("%s", gui::text("Accent colors apply instantly across the interface.",
				"Vurgu renkleri arayuze aninda uygulanir."));
		}
		gui::end_surface();
	}

	std::string lowercase(std::string value);

	void render_configs_page_v2()
	{
		static bool requested_initial_list = false;
		static int selected_id = 0;
		static int editing_id = 0;
		static bool editor_mode = false;
		static char config_name[49]{};
		static char config_description[181]{};
		static char search[64]{};
		static std::string last_service_status;
		static int pending_delete_id = 0;

		std::string downloaded_payload;
		std::string downloaded_name;
		if (configs::consume_pending_payload(downloaded_payload, downloaded_name))
		{
			std::string result;
			const bool applied = modules::apply_public_config(downloaded_payload, result);
			gui::push_workspace_toast(downloaded_name + (applied ? ": " : " - ") + result, applied);
		}

		configs::snapshot library = configs::get_snapshot();
		if (!requested_initial_list && !library.busy)
		{
			configs::refresh();
			requested_initial_list = true;
			library = configs::get_snapshot();
		}
		if (!library.status.empty() && library.status != last_service_status)
		{
			if (!last_service_status.empty())
				gui::push_workspace_toast(library.status,
					library.status.find("failed") == std::string::npos && library.status.find("Unable") == std::string::npos);
			last_service_status = library.status;
		}

		const ImVec2 header = gui::page_header(gui::text("Configs", "Configs"),
			gui::text("Browse and manage community setups", "Topluluk ayarlarini yonet"));
		const ImVec2 content_start = ImGui::GetCursorScreenPos();
		const float header_right = content_start.x + ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorScreenPos(ImVec2(header_right - 72.0f, header.y + 4.0f));
		if (gui::icon_action("config_publish_top", "+", "+", gui::text("Publish config", "Config yayinla"), false, true))
		{
			editor_mode = true;
			editing_id = 0;
			config_name[0] = '\0';
			config_description[0] = '\0';
		}
		ImGui::SameLine(0.0f, 6.0f);
		ImGui::BeginDisabled(library.busy);
		if (gui::icon_action("config_refresh_top", "\xEF\x80\xA1", "R", gui::text("Refresh library", "Kutuphane yenile")))
			configs::refresh();
		ImGui::EndDisabled();
		ImGui::SetCursorScreenPos(content_start);

		const float full_width = ImGui::GetContentRegionAvail().x;
		const float height = ImGui::GetContentRegionAvail().y;
		const float gap = 10.0f;
		const float list_width = std::floor((full_width - gap) * 0.42f);
		const float detail_width = full_width - gap - list_width;

		if (gui::begin_surface("##config_library_v2", ImVec2(list_width, height), ImVec2(14.0f, 12.0f)))
		{
			ImFont* bold = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
			const ImVec2 title = ImGui::GetCursorScreenPos();
			ImGui::GetWindowDrawList()->AddText(bold, 16.0f, title, IM_COL32(233, 235, 242, 255),
				gui::text("Public Library", "Public Kutuphane"));
			const std::string count = std::to_string(library.entries.size());
			const ImVec2 count_size = ImGui::CalcTextSize(count.c_str());
			ImGui::GetWindowDrawList()->AddText(ImVec2(title.x + ImGui::GetContentRegionAvail().x - count_size.x, title.y + 1.0f),
				IM_COL32(126, 130, 142, 255), count.c_str());
			ImGui::Dummy(ImVec2(0.0f, 29.0f));
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##config_search_v2", gui::text("Search configs", "Config ara"), search, IM_ARRAYSIZE(search));
			ImGui::Dummy(ImVec2(0.0f, 5.0f));
			if (ImGui::BeginChild("##config_rows_v2", ImVec2(0.0f, 0.0f)))
			{
				int visible = 0;
				for (const configs::public_config& entry : library.entries)
				{
					const std::string searchable = lowercase(entry.name + " " + entry.author_name);
					if (search[0] && searchable.find(lowercase(search)) == std::string::npos) continue;
					++visible;
					ImGui::PushID(entry.id);
					const ImVec2 row_min = ImGui::GetCursorScreenPos();
					const float row_width = ImGui::GetContentRegionAvail().x - 4.0f;
					const ImVec2 row_max(row_min.x + row_width, row_min.y + 58.0f);
					ImGui::InvisibleButton("##select_config", ImVec2(row_width, 58.0f));
					const bool hovered = ImGui::IsItemHovered();
					if (ImGui::IsItemClicked())
					{
						selected_id = entry.id;
						editor_mode = false;
					}
					gui::list_row_background(row_min, row_max, hovered, selected_id == entry.id);
					const float badge_space = entry.mine ? 54.0f : 10.0f;
					const ImVec4 title_clip(row_min.x + 10.0f, row_min.y + 5.0f, row_max.x - badge_space, row_min.y + 28.0f);
					ImGui::GetWindowDrawList()->AddText(bold, 15.0f, ImVec2(row_min.x + 10.0f, row_min.y + 8.0f),
						IM_COL32(232, 234, 241, 255), entry.name.c_str(), nullptr, 0.0f, &title_clip);
					if (entry.mine)
					{
						const char* mine = gui::text("YOURS", "SENIN");
						const ImVec2 mine_size = ImGui::CalcTextSize(mine);
						const ImVec2 badge(row_max.x - mine_size.x - 16.0f, row_min.y + 7.0f);
						ImGui::GetWindowDrawList()->AddRectFilled(badge,
							ImVec2(badge.x + mine_size.x + 10.0f, badge.y + 19.0f),
							color_with_alpha(gui::get_dynamic_color(0), 0.16f), 9.0f);
						ImGui::GetWindowDrawList()->AddText(ImVec2(badge.x + 5.0f, badge.y + 2.0f),
							color_with_alpha(gui::get_dynamic_color(0)), mine);
					}
					const std::string meta = entry.author_name + "  |  " + std::to_string(entry.module_count) +
						gui::text(" modules", " modul") + "  |  " + std::to_string(entry.downloads) + gui::text(" loads", " yukleme");
					const ImVec4 meta_clip(row_min.x + 10.0f, row_min.y + 30.0f, row_max.x - 10.0f, row_max.y - 5.0f);
					ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), 13.0f, ImVec2(row_min.x + 10.0f, row_min.y + 34.0f),
						IM_COL32(127, 131, 144, 255), meta.c_str(), nullptr, 0.0f, &meta_clip);
					ImGui::SetCursorScreenPos(ImVec2(row_min.x, row_max.y + 5.0f));
					ImGui::PopID();
				}
				if (library.entries.empty() && !library.busy)
				{
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 38.0f);
					ImGui::TextDisabled("%s", gui::text("No public configs yet", "Henuz public config yok"));
				}
				else if (visible == 0)
				{
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 38.0f);
					ImGui::TextDisabled("%s", gui::text("No matching configs", "Eslesen config yok"));
				}
			}
			ImGui::EndChild();
		}
		gui::end_surface();

		ImGui::SetCursorScreenPos(ImVec2(content_start.x + list_width + gap, content_start.y));
		if (gui::begin_surface("##config_detail_v2", ImVec2(detail_width, height), ImVec2(16.0f, 14.0f)))
		{
			const configs::public_config* selected = nullptr;
			for (const configs::public_config& entry : library.entries)
				if (entry.id == selected_id) { selected = &entry; break; }
			ImFont* bold = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
			if (editor_mode)
			{
				const ImVec2 title = ImGui::GetCursorScreenPos();
				ImGui::GetWindowDrawList()->AddText(bold, 17.0f, title, IM_COL32(236, 238, 244, 255),
					editing_id > 0 ? gui::text("Edit Config", "Config Duzenle") : gui::text("Publish Config", "Config Yayinla"));
				ImGui::SetCursorScreenPos(ImVec2(title.x + ImGui::GetContentRegionAvail().x - 30.0f, title.y - 5.0f));
				if (gui::icon_action("close_config_editor", "\xEF\x80\x8D", "X", gui::text("Close editor", "Editoru kapat")))
					editor_mode = false;
				ImGui::SetCursorScreenPos(ImVec2(title.x, title.y + 42.0f));
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputTextWithHint("##config_name_v2", gui::text("Config name", "Config adi"),
					config_name, IM_ARRAYSIZE(config_name));
				ImGui::Dummy(ImVec2(0.0f, 8.0f));
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputTextWithHint("##config_description_v2", gui::text("Short description", "Kisa aciklama"),
					config_description, IM_ARRAYSIZE(config_description));
				ImGui::Dummy(ImVec2(0.0f, 14.0f));
				const std::string author = friends::local_player_name();
				if (author.empty())
				{
					ImGui::TextDisabled("%s", gui::text("Join a world before publishing.", "Yayinlamadan once bir dunyaya gir."));
					ImGui::Dummy(ImVec2(0.0f, 8.0f));
				}
				ImGui::BeginDisabled(library.busy || author.empty() || !config_name[0]);
				if (gui::action_button(editing_id > 0 ? gui::text("Save changes", "Degisiklikleri kaydet")
					: gui::text("Publish publicly", "Public yayinla"), ImVec2(154.0f, 32.0f), gui::action_tone::accent))
				{
					configs::publish(editing_id, config_name, config_description, author,
						modules::export_public_config(), modules::enabled_module_count());
					editor_mode = false;
				}
				ImGui::EndDisabled();
			}
			else if (!selected)
			{
				const char* title = gui::text("Select a config", "Bir config sec");
				const char* body = gui::text("Choose an entry from the library to inspect or load it.",
					"Detaylari gormek veya yuklemek icin kutuphaneden sec.");
				const ImVec2 title_size = bold->CalcTextSizeA(17.0f, FLT_MAX, 0.0f, title);
				const ImVec2 body_size = ImGui::CalcTextSize(body);
				const ImVec2 center(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x * 0.5f,
					ImGui::GetCursorScreenPos().y + ImGui::GetContentRegionAvail().y * 0.42f);
				ImGui::GetWindowDrawList()->AddText(bold, 17.0f, ImVec2(center.x - title_size.x * 0.5f, center.y - 16.0f),
					IM_COL32(220, 223, 231, 255), title);
				ImGui::GetWindowDrawList()->AddText(ImVec2(center.x - body_size.x * 0.5f, center.y + 10.0f),
					IM_COL32(122, 126, 139, 255), body);
			}
			else
			{
				const ImVec2 title = ImGui::GetCursorScreenPos();
				const ImVec4 clip(title.x, title.y, title.x + ImGui::GetContentRegionAvail().x - 118.0f, title.y + 28.0f);
				ImGui::GetWindowDrawList()->AddText(bold, 18.0f, title, IM_COL32(236, 238, 244, 255),
					selected->name.c_str(), nullptr, 0.0f, &clip);
				ImGui::SetCursorScreenPos(ImVec2(title.x + ImGui::GetContentRegionAvail().x - (selected->mine ? 108.0f : 34.0f), title.y - 5.0f));
				ImGui::BeginDisabled(library.busy);
				if (gui::icon_action("detail_load", "\xEF\x80\x99", "L", gui::text("Load config", "Config yukle"), false, true))
					configs::load(selected->id);
				if (selected->mine)
				{
					ImGui::SameLine(0.0f, 6.0f);
					if (gui::icon_action("detail_edit", "\xEF\x83\x87", "E", gui::text("Edit config", "Config duzenle")))
					{
						editor_mode = true;
						editing_id = selected->id;
						strncpy_s(config_name, selected->name.c_str(), _TRUNCATE);
						strncpy_s(config_description, selected->description.c_str(), _TRUNCATE);
					}
					ImGui::SameLine(0.0f, 6.0f);
					if (gui::icon_action("detail_delete", "\xEF\x87\xB8", "X", gui::text("Delete config", "Config sil"), true))
					{
						pending_delete_id = selected->id;
						ImGui::OpenPopup("##confirm_config_delete");
					}
				}
				ImGui::EndDisabled();
				ImGui::SetCursorScreenPos(ImVec2(title.x, title.y + 42.0f));
				ImGui::TextColored(ImVec4(0.56f, 0.58f, 0.65f, 1.0f), "%s", selected->author_name.c_str());
				ImGui::Dummy(ImVec2(0.0f, 12.0f));
				ImGui::TextWrapped("%s", selected->description.empty()
					? gui::text("No description provided.", "Aciklama eklenmemis.") : selected->description.c_str());
				ImGui::Dummy(ImVec2(0.0f, 18.0f));
				const std::string stats = std::to_string(selected->module_count) + gui::text(" modules", " modul") +
					"  |  " + std::to_string(selected->downloads) + gui::text(" loads", " yukleme");
				ImGui::TextDisabled("%s", stats.c_str());
			}

			ImGui::SetNextWindowPos(ImVec2(ImGui::GetMainViewport()->Pos.x + ImGui::GetMainViewport()->Size.x * 0.5f,
				ImGui::GetMainViewport()->Pos.y + ImGui::GetMainViewport()->Size.y * 0.5f), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
			if (ImGui::BeginPopupModal("##confirm_config_delete", nullptr,
				ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
			{
				ImGui::TextUnformatted(gui::text("Delete this config permanently?", "Bu config kalici olarak silinsin mi?"));
				ImGui::Dummy(ImVec2(0.0f, 8.0f));
				if (gui::action_button(gui::text("Cancel", "Vazgec"), ImVec2(92.0f, 30.0f)))
				{
					pending_delete_id = 0;
					ImGui::CloseCurrentPopup();
				}
				ImGui::SameLine(0.0f, 8.0f);
				if (gui::action_button(gui::text("Delete", "Sil"), ImVec2(92.0f, 30.0f), gui::action_tone::danger))
				{
					if (pending_delete_id > 0) configs::remove(pending_delete_id);
					if (selected_id == pending_delete_id) selected_id = 0;
					pending_delete_id = 0;
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}
		gui::end_surface();
		ImGui::SetCursorScreenPos(ImVec2(content_start.x, content_start.y + height));
	}


	void render_configs_page_v3()
	{
		static bool requested_initial_list = false;
		static int selected_id = 0;
		static int editing_id = 0;
		static int pending_delete_id = 0;
		static bool editor_open = false;
		static char config_name[49]{};
		static char config_description[181]{};
		static char search[64]{};

		std::string downloaded_payload;
		std::string downloaded_name;
		if (configs::consume_pending_payload(downloaded_payload, downloaded_name))
		{
			std::string result;
			const bool applied = modules::apply_public_config(downloaded_payload, result);
			gui::push_workspace_toast(downloaded_name + (applied ? ": " : " - ") + result, applied);
		}

		configs::snapshot library = configs::get_snapshot();
		if (!requested_initial_list && !library.busy)
		{
			configs::refresh();
			requested_initial_list = true;
			library = configs::get_snapshot();
		}

		const ImVec2 header = gui::page_header(gui::text("Configs", "Configler"),
			gui::text("Community setups", "Topluluk ayarlari"));
		const ImVec2 content_start = ImGui::GetCursorScreenPos();
		const float header_right = content_start.x + ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorScreenPos(ImVec2(header_right - 72.0f, header.y + 4.0f));
		if (gui::icon_action("config_publish_v3", "+", "+", gui::text("Publish config", "Config yayinla"), false, true))
		{
			editor_open = true;
			editing_id = 0;
			config_name[0] = '\0';
			config_description[0] = '\0';
		}
		ImGui::SameLine(0.0f, 6.0f);
		ImGui::BeginDisabled(library.busy);
		if (gui::icon_action("config_refresh_v3", "\xEF\x80\xA1", "R", gui::text("Refresh", "Yenile"))) configs::refresh();
		ImGui::EndDisabled();
		ImGui::SetCursorScreenPos(content_start);

		if (editor_open)
		{
			if (gui::begin_surface("##config_editor_v3", ImVec2(0.0f, 128.0f), ImVec2(13.0f, 11.0f)))
			{
				ImFont* bold = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
				const ImVec2 top = ImGui::GetCursorScreenPos();
				ImGui::GetWindowDrawList()->AddText(bold, 15.0f, top, IM_COL32(234, 236, 242, 255),
					editing_id ? gui::text("Edit config", "Config duzenle") : gui::text("Publish config", "Config yayinla"));
				ImGui::SetCursorScreenPos(ImVec2(top.x + ImGui::GetContentRegionAvail().x - 30.0f, top.y - 5.0f));
				if (gui::icon_action("close_config_editor_v3", "\xEF\x80\x8D", "X", gui::text("Close", "Kapat"))) editor_open = false;
				ImGui::SetCursorScreenPos(ImVec2(top.x, top.y + 29.0f));
				const float available = ImGui::GetContentRegionAvail().x;
				ImGui::SetNextItemWidth(available * 0.38f);
				ImGui::InputTextWithHint("##config_name_v3", gui::text("Config name", "Config adi"), config_name, IM_ARRAYSIZE(config_name));
				ImGui::SameLine(0.0f, 8.0f);
				ImGui::SetNextItemWidth(-1.0f);
				ImGui::InputTextWithHint("##config_description_v3", gui::text("Short description", "Kisa aciklama"),
					config_description, IM_ARRAYSIZE(config_description));
				ImGui::Dummy(ImVec2(0.0f, 8.0f));
				const std::string author = friends::local_player_name();
				ImGui::BeginDisabled(library.busy || author.empty() || !config_name[0]);
				if (gui::action_button(editing_id ? gui::text("Save changes", "Degisiklikleri kaydet")
					: gui::text("Publish publicly", "Public yayinla"), ImVec2(150.0f, 30.0f), gui::action_tone::accent))
				{
					configs::publish(editing_id, config_name, config_description, author,
						modules::export_public_config(), modules::enabled_module_count());
					editor_open = false;
				}
				ImGui::EndDisabled();
				if (author.empty())
				{
					ImGui::SameLine(0.0f, 10.0f);
					ImGui::TextDisabled("%s", gui::text("Join a world to publish", "Yayinlamak icin dunyaya gir"));
				}
			}
			gui::end_surface();
			ImGui::Dummy(ImVec2(0.0f, 8.0f));
		}

		const float list_height = ImGui::GetContentRegionAvail().y;
		if (gui::begin_surface("##config_library_v3", ImVec2(0.0f, list_height), ImVec2(13.0f, 11.0f)))
		{
			ImFont* bold = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
			const ImVec2 bar = ImGui::GetCursorScreenPos();
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 74.0f);
			ImGui::InputTextWithHint("##config_search_v3", gui::text("Search public configs", "Public config ara"), search, IM_ARRAYSIZE(search));
			const std::string count = std::to_string(library.entries.size()) + gui::text(" configs", " config");
			const ImVec2 count_size = ImGui::CalcTextSize(count.c_str());
			ImGui::GetWindowDrawList()->AddText(ImVec2(bar.x + ImGui::GetContentRegionAvail().x - count_size.x, bar.y + 8.0f),
				IM_COL32(126, 130, 142, 255), count.c_str());
			ImGui::Dummy(ImVec2(0.0f, 7.0f));
			if (ImGui::BeginChild("##config_rows_v3", ImVec2(0.0f, 0.0f)))
			{
				int visible = 0;
				for (const configs::public_config& entry : library.entries)
				{
					if (search[0] && lowercase(entry.name + " " + entry.author_name).find(lowercase(search)) == std::string::npos) continue;
					++visible;
					ImGui::PushID(entry.id);
					const bool expanded = selected_id == entry.id;
					const float row_height = expanded ? 121.0f : 58.0f;
					const ImVec2 row_min = ImGui::GetCursorScreenPos();
					const float row_width = ImGui::GetContentRegionAvail().x - 4.0f;
					const ImVec2 row_max(row_min.x + row_width, row_min.y + row_height);
					ImGui::InvisibleButton("##config_row_v3", ImVec2(row_width, row_height));
					if (ImGui::IsItemClicked()) selected_id = expanded ? 0 : entry.id;
					gui::list_row_background(row_min, row_max, ImGui::IsItemHovered(), expanded);
					const ImVec4 title_clip(row_min.x + 12.0f, row_min.y + 5.0f, row_max.x - 80.0f, row_min.y + 28.0f);
					ImGui::GetWindowDrawList()->AddText(bold, 15.0f, ImVec2(row_min.x + 12.0f, row_min.y + 8.0f),
						IM_COL32(234, 236, 242, 255), entry.name.c_str(), nullptr, 0.0f, &title_clip);
					const std::string meta = entry.author_name + "  /  " + std::to_string(entry.module_count) + gui::text(" modules", " modul") +
						"  /  " + std::to_string(entry.downloads) + gui::text(" loads", " yukleme");
					ImGui::GetWindowDrawList()->AddText(ImVec2(row_min.x + 12.0f, row_min.y + 33.0f),
						IM_COL32(126, 130, 142, 255), meta.c_str());
					if (entry.mine)
					{
						ImGui::SetCursorScreenPos(ImVec2(row_max.x - 66.0f, row_min.y + 7.0f));
						gui::status_badge(gui::text("YOURS", "SENIN"), true);
					}
					if (expanded)
					{
						ImGui::GetWindowDrawList()->AddLine(ImVec2(row_min.x + 12.0f, row_min.y + 61.0f),
							ImVec2(row_max.x - 12.0f, row_min.y + 61.0f), IM_COL32(255, 255, 255, 13));
						const char* description = entry.description.empty() ? gui::text("No description", "Aciklama yok") : entry.description.c_str();
						const ImVec4 description_clip(row_min.x + 12.0f, row_min.y + 65.0f, row_max.x - 150.0f, row_max.y - 8.0f);
						ImGui::GetWindowDrawList()->AddText(ImGui::GetFont(), 13.0f, ImVec2(row_min.x + 12.0f, row_min.y + 75.0f),
							IM_COL32(158, 162, 174, 255), description, nullptr, 0.0f, &description_clip);
						float action_x = row_max.x - (entry.mine ? 108.0f : 34.0f);
						ImGui::SetCursorScreenPos(ImVec2(action_x, row_min.y + 74.0f));
						ImGui::BeginDisabled(library.busy);
						if (gui::icon_action("load_v3", "\xEF\x80\x99", "L", gui::text("Load config", "Config yukle"), false, true)) configs::load(entry.id);
						if (entry.mine)
						{
							ImGui::SameLine(0.0f, 6.0f);
							if (gui::icon_action("edit_v3", "\xEF\x83\x87", "E", gui::text("Edit config", "Config duzenle")))
							{
								editor_open = true; editing_id = entry.id;
								strncpy_s(config_name, entry.name.c_str(), _TRUNCATE);
								strncpy_s(config_description, entry.description.c_str(), _TRUNCATE);
							}
							ImGui::SameLine(0.0f, 6.0f);
							if (gui::icon_action("delete_v3", "\xEF\x87\xB8", "X", gui::text("Delete config", "Config sil"), true))
							{
								pending_delete_id = entry.id;
								ImGui::OpenPopup("##confirm_config_delete_v3");
							}
						}
						ImGui::EndDisabled();
					}
					ImGui::SetCursorScreenPos(ImVec2(row_min.x, row_max.y + 6.0f));
					ImGui::PopID();
				}
				if (!visible && !library.busy) ImGui::TextDisabled("%s", gui::text("No configs found", "Config bulunamadi"));
			}
			ImGui::EndChild();
		}
		gui::end_surface();

		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal("##confirm_config_delete_v3", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoTitleBar))
		{
			ImGui::TextUnformatted(gui::text("Delete this config permanently?", "Bu config kalici olarak silinsin mi?"));
			ImGui::Dummy(ImVec2(0.0f, 8.0f));
			if (gui::action_button(gui::text("Cancel", "Vazgec"), ImVec2(90.0f, 30.0f))) { pending_delete_id = 0; ImGui::CloseCurrentPopup(); }
			ImGui::SameLine(0.0f, 8.0f);
			if (gui::action_button(gui::text("Delete", "Sil"), ImVec2(90.0f, 30.0f), gui::action_tone::danger))
			{
				if (pending_delete_id) configs::remove(pending_delete_id);
				if (selected_id == pending_delete_id) selected_id = 0;
				pending_delete_id = 0;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void render_configs_page()
	{
		render_configs_page_v3();
		return;
		static bool requested_initial_list = false;
		static bool editor_open = false;
		static float editor_animation = 0.0f;
		static int editing_id = 0;
		static char config_name[49] = "";
		static char config_description[181] = "";
		static std::string apply_status;

		std::string downloaded_payload;
		std::string downloaded_name;
		if (configs::consume_pending_payload(downloaded_payload, downloaded_name))
		{
			std::string result;
			const bool applied = modules::apply_public_config(downloaded_payload, result);
			apply_status = downloaded_name + (applied ? ": " : " - ") + result;
		}

		configs::snapshot library = configs::get_snapshot();
		if (!requested_initial_list && !library.busy)
		{
			configs::refresh();
			requested_initial_list = true;
			library = configs::get_snapshot();
		}

		const char* subtitle = apply_status.empty()
			? gui::text("Discover, publish and load community setups", "Topluluk ayarlarını keşfet, yayınla ve yükle")
			: apply_status.c_str();
		const ImVec2 header_start = gui::page_header(gui::text("Configs", "Configs"), subtitle);
		const ImVec2 after_header = ImGui::GetCursorScreenPos();
		const float action_width = 92.0f;
		const float action_gap = 7.0f;
		const float content_width = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorScreenPos(ImVec2(
			header_start.x + content_width - action_width * 2.0f - action_gap,
			header_start.y + 2.0f));
		if (gui::action_button(
			editor_open ? gui::text("Close editor", "Editörü kapat") : gui::text("Publish", "Yayınla"),
			ImVec2(action_width, 29.0f), gui::action_tone::accent))
		{
			editor_open = !editor_open;
		}
		ImGui::SameLine(0.0f, action_gap);
		ImGui::BeginDisabled(library.busy);
		if (gui::action_button(gui::text("Refresh", "Yenile"), ImVec2(action_width, 29.0f)))
			configs::refresh();
		ImGui::EndDisabled();
		ImGui::SetCursorScreenPos(after_header);

		const float editor_delta = std::clamp(ImGui::GetIO().DeltaTime, 0.0f, 0.05f);
		const float editor_duration = editor_open ? 0.22f : 0.20f;
		editor_animation = std::clamp(
			editor_animation + (editor_open ? editor_delta : -editor_delta) / editor_duration,
			0.0f,
			1.0f);
		if (!editor_open && editor_animation < 0.002f)
		{
			editor_animation = 0.0f;
			editing_id = 0;
			config_name[0] = '\0';
			config_description[0] = '\0';
		}
		const float editor_eased = editor_animation * editor_animation * (3.0f - 2.0f * editor_animation);
		if (editor_eased > 0.0f)
		{
			const float editor_height = 139.0f * editor_eased;
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(0.02f, editor_eased));
			if (gui::begin_surface("##config_editor", ImVec2(0.0f, editor_height), ImVec2(12.0f, 10.0f)))
			{
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (1.0f - editor_eased) * 7.0f);
				const std::string author = friends::local_player_name();
				ImFont* heading_font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
				const ImVec2 editor_title = ImGui::GetCursorScreenPos();
				ImGui::GetWindowDrawList()->AddText(
					heading_font, 16.0f, editor_title, IM_COL32(231, 232, 238, 255),
					editing_id > 0 ? gui::text("Edit public config", "Public configi düzenle")
						: gui::text("Publish current setup", "Mevcut ayarları yayınla"));
				const std::string author_line = gui::text("Author  ", "Yayınlayan  ") +
					(author.empty() ? std::string(gui::text("Join a world", "Bir dünyaya gir")) : author);
				const ImVec2 author_size = ImGui::CalcTextSize(author_line.c_str());
				ImGui::GetWindowDrawList()->AddText(
					ImVec2(editor_title.x + ImGui::GetContentRegionAvail().x - author_size.x, editor_title.y + 1.0f),
					IM_COL32(111, 112, 123, 255), author_line.c_str());
				ImGui::Dummy(ImVec2(0.0f, 24.0f));

				const float field_width = ImGui::GetContentRegionAvail().x;
				ImGui::SetNextItemWidth(field_width * 0.35f);
				ImGui::InputTextWithHint("##config_name", gui::text("Config name", "Config adı"), config_name, IM_ARRAYSIZE(config_name));
				ImGui::SameLine(0.0f, 7.0f);
				ImGui::SetNextItemWidth(field_width * 0.65f - 7.0f);
				ImGui::InputTextWithHint(
					"##config_description",
					gui::text("Short description", "Kısa açıklama"),
					config_description,
					IM_ARRAYSIZE(config_description));
				ImGui::Dummy(ImVec2(0.0f, 3.0f));

				ImGui::BeginDisabled(library.busy || author.empty() || config_name[0] == '\0');
				if (gui::action_button(
					editing_id > 0 ? gui::text("Save changes", "Değişiklikleri kaydet")
						: gui::text("Publish publicly", "Herkese açık yayınla"),
					ImVec2(142.0f, 28.0f), gui::action_tone::accent))
				{
					configs::publish(
						editing_id,
						config_name,
						config_description,
						author,
						modules::export_public_config(),
						modules::enabled_module_count());
				}
				ImGui::EndDisabled();
				ImGui::SameLine(0.0f, 9.0f);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 5.0f);
				ImGui::TextColored(
					ImVec4(0.45f, 0.46f, 0.51f, 1.0f),
					"%s",
					gui::text("Module settings, states and keybinds included", "Modül ayarları, durumları ve tuşlar dahil"));
			}
			gui::end_surface();
			ImGui::PopStyleVar();
			ImGui::Dummy(ImVec2(0.0f, 7.0f * editor_eased));
		}

		const ImVec2 library_header = ImGui::GetCursorScreenPos();
		ImFont* heading_font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
		ImGui::GetWindowDrawList()->AddText(
			heading_font, 16.0f, library_header, IM_COL32(226, 227, 234, 255),
			gui::text("Public library", "Public kütüphane"));
		ImGui::SetCursorScreenPos(ImVec2(library_header.x, library_header.y + 29.0f));

		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.0f);
		if (gui::begin_surface("##config_library", ImVec2(0.0f, 0.0f), ImVec2(8.0f, 8.0f)))
		{
			if (library.entries.empty() && !library.busy)
			{
				const char* empty_title = gui::text("No configs published yet", "Henüz yayınlanmış config yok");
				const char* empty_body = gui::text("Publish the first community setup.", "İlk topluluk ayarını sen yayınla.");
				const ImVec2 title_size = ImGui::CalcTextSize(empty_title);
				const ImVec2 body_size = ImGui::CalcTextSize(empty_body);
				const float width = ImGui::GetContentRegionAvail().x;
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 45.0f);
				ImGui::SetCursorPosX(std::max(0.0f, (width - title_size.x) * 0.5f));
				ImGui::TextColored(ImVec4(0.64f, 0.65f, 0.71f, 1.0f), "%s", empty_title);
				ImGui::SetCursorPosX(std::max(0.0f, (width - body_size.x) * 0.5f));
				ImGui::TextColored(ImVec4(0.40f, 0.41f, 0.46f, 1.0f), "%s", empty_body);
			}

			for (const configs::public_config& entry : library.entries)
			{
				ImGui::PushID(entry.id);
				const ImVec2 row_min = ImGui::GetCursorScreenPos();
				const float row_width = ImGui::GetContentRegionAvail().x - 5.0f;
				const ImVec2 row_max(row_min.x + row_width, row_min.y + 82.0f);
				ImGui::Dummy(ImVec2(row_width, 82.0f));
				const bool hovered = ImGui::IsMouseHoveringRect(row_min, row_max, true);
				gui::list_row_background(row_min, row_max, hovered, entry.mine);

				const float actions_width = entry.mine ? 108.0f : 36.0f;
				const float text_right = row_max.x - actions_width - 10.0f;
				const ImVec4 title_clip(row_min.x + 12.0f, row_min.y + 6.0f, text_right, row_min.y + 25.0f);
				ImGui::GetWindowDrawList()->AddText(
					heading_font, 15.0f, ImVec2(row_min.x + 12.0f, row_min.y + 9.0f),
					IM_COL32(234, 235, 241, 255), entry.name.c_str(), nullptr, 0.0f, &title_clip);

				if (entry.mine)
				{
					const char* mine = gui::text("YOURS", "SENİN");
					const ImVec2 mine_size = ImGui::CalcTextSize(mine);
					const float badge_x = std::min(text_right - mine_size.x - 12.0f,
						row_min.x + 18.0f + ImGui::CalcTextSize(entry.name.c_str()).x);
					ImGui::GetWindowDrawList()->AddRectFilled(
						ImVec2(badge_x, row_min.y + 8.0f),
						ImVec2(badge_x + mine_size.x + 12.0f, row_min.y + 27.0f),
						color_with_alpha(gui::get_dynamic_color(0), 0.15f), 9.5f);
					ImGui::GetWindowDrawList()->AddText(
						ImVec2(badge_x + 6.0f, row_min.y + 10.0f),
						color_with_alpha(gui::get_dynamic_color(0), 0.95f), mine);
				}

				const std::string meta = entry.author_name + "  •  " +
					std::to_string(entry.module_count) + gui::text(" modules", " modül") + "  •  " +
					std::to_string(entry.downloads) + gui::text(" loads", " yükleme");
				const ImVec4 meta_clip(row_min.x + 12.0f, row_min.y + 29.0f, text_right, row_min.y + 48.0f);
				ImGui::GetWindowDrawList()->AddText(
					ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(row_min.x + 12.0f, row_min.y + 32.0f),
					entry.mine ? color_with_alpha(gui::get_dynamic_color(0), 0.92f) : IM_COL32(131, 132, 143, 255),
					meta.c_str(), nullptr, 0.0f, &meta_clip);
				const ImVec4 description_clip(row_min.x + 12.0f, row_min.y + 50.0f, text_right, row_min.y + 74.0f);
				ImGui::GetWindowDrawList()->AddText(
					ImGui::GetFont(), ImGui::GetFontSize(), ImVec2(row_min.x + 12.0f, row_min.y + 54.0f),
					IM_COL32(102, 103, 114, 255), entry.description.c_str(), nullptr, 0.0f, &description_clip);

				ImGui::BeginDisabled(library.busy);
				if (entry.mine)
				{
					ImGui::SetCursorScreenPos(ImVec2(row_max.x - 104.0f, row_min.y + 26.0f));
					if (config_icon_button(
						"load", "\xEF\x80\x99", "L", gui::text("Load config", "Configi yükle")))
						configs::load(entry.id);
					ImGui::SameLine(0.0f, 4.0f);
					if (config_icon_button(
						"edit", "\xEF\x83\x87", "E", gui::text("Edit config", "Configi düzenle")))
					{
						editing_id = entry.id;
						editor_open = true;
						strncpy_s(config_name, entry.name.c_str(), _TRUNCATE);
						strncpy_s(config_description, entry.description.c_str(), _TRUNCATE);
					}
					ImGui::SameLine(0.0f, 4.0f);
					if (config_icon_button(
						"delete", "\xEF\x87\xB8", "X", gui::text("Delete config", "Configi sil"), true))
						configs::remove(entry.id);
				}
				else
				{
					ImGui::SetCursorScreenPos(ImVec2(row_max.x - 34.0f, row_min.y + 26.0f));
					if (config_icon_button(
						"load", "\xEF\x80\x99", "L", gui::text("Load config", "Configi yükle")))
						configs::load(entry.id);
				}
				ImGui::EndDisabled();
				ImGui::SetCursorScreenPos(ImVec2(row_min.x, row_max.y + 6.0f));
				ImGui::PopID();
			}
		}
		gui::end_surface();
		ImGui::PopStyleVar();
	}

	enum class aegis_page
	{
		modules,
		friends,
		configs,
		settings
	};

	struct aegis_runtime_state
	{
		aegis_page page = aegis_page::modules;
		int active_category = 0;
		modules::module* horizontal_module = nullptr;
		char search[96]{};
		bool focus_search = false;
		int palette_selection = 0;
		float layout_transition = 1.0f;
		gui::aegis::layout_mode previous_layout = gui::aegis::layout_mode::vertical_columns;
	};

	aegis_runtime_state aegis_runtime{};
	std::unordered_map<std::string, float> measured_option_heights;

	constexpr const char* aegis_categories[] = { "Combat", "Movement", "Player", "World", "Render", "Visual", "Client" };
	constexpr sidebar_icon aegis_category_icons[] = {
		sidebar_icon::combat, sidebar_icon::misc, sidebar_icon::friends,
		sidebar_icon::configs, sidebar_icon::render, sidebar_icon::visual, sidebar_icon::settings
	};

	bool module_has_options(modules::module* module)
	{
		return module && module->get_options_height() > 45.0f;
	}

	float vertical_options_full_height(modules::module* module)
	{
		if (!module_has_options(module)) return 0.0f;
		const auto measured = measured_option_heights.find(module->get_id());
		const float content = measured != measured_option_heights.end()
			? measured->second
			: std::max(52.0f, module->get_options_height());
		const float viewport_limit = std::max(210.0f, ImGui::GetMainViewport()->Size.y * 0.65f);
		return std::min(content + 18.0f, viewport_limit);
	}

	int category_index(const char* category)
	{
		for (int index = 0; index < IM_ARRAYSIZE(aegis_categories); ++index)
			if (category && std::strcmp(category, aegis_categories[index]) == 0) return index;
		return 0;
	}

	std::string lowercase(std::string value)
	{
		std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character)
		{
			return static_cast<char>(std::tolower(character));
		});
		return value;
	}

	const char* module_option_keywords(const char* name)
	{
		if (!name) return "";
		if (std::strcmp(name, "triggerbot") == 0) return "target cooldown cps delay pattern critical weapon players mobs animals click";
		if (std::strstr(name, "aim assist")) return "fov distance strength target priority adaptive body random weapon";
		if (std::strcmp(name, "velocity") == 0) return "horizontal vertical chance lag delay mode knockback";
		if (std::strstr(name, "totem")) return "offhand hotbar health delay inventory cursor slot";
		if (std::strstr(name, "anchor")) return "delay charge jitter slot input movement air";
		if (std::strstr(name, "clicker")) return "cps jitter random delay drop spike input";
		if (std::strstr(name, "esp")) return "color box outline fill distance blocks storage";
		if (std::strcmp(name, "watermark") == 0 || std::strcmp(name, "arraylist") == 0)
			return "style scale position design color";
		return "enabled bind settings options";
	}

	bool module_matches(modules::module* module, const char* query)
	{
		if (!module || !query || !*query) return true;
		const std::string needle = lowercase(query);
		const std::string haystack = lowercase(
			std::string(module->get_name()) + " " + module->get_category() + " " +
			module_description(module->get_name()) + " " + module_option_keywords(module->get_name()));
		return haystack.find(needle) != std::string::npos;
	}

	void draw_tactical_energy(ImDrawList* draw_list, const ImVec2& minimum, const ImVec2& maximum)
	{
		const auto& preferences = gui::aegis::get_preferences();
		const int layers = static_cast<int>(preferences.energy) + 1;
		const float time = static_cast<float>(ImGui::GetTime());
		const ImColor accent = gui::get_dynamic_color(0);
		draw_list->PushClipRect(minimum, maximum, true);
		for (int layer = 0; layer < layers; ++layer)
		{
			const float phase = time * (0.12f + layer * 0.035f) + layer * 1.73f;
			const float span = maximum.x - minimum.x;
			const float base_y = minimum.y + 76.0f + layer * 67.0f;
			const float travel = std::fmod(phase * 83.0f, span + 260.0f) - 130.0f;
			const ImVec2 points[] = {
				ImVec2(minimum.x + travel - 180.0f, base_y + 18.0f),
				ImVec2(minimum.x + travel - 90.0f, base_y + 18.0f),
				ImVec2(minimum.x + travel - 58.0f, base_y - 14.0f),
				ImVec2(minimum.x + travel + 62.0f, base_y - 14.0f),
				ImVec2(minimum.x + travel + 88.0f, base_y + 12.0f),
				ImVec2(minimum.x + travel + 210.0f, base_y + 12.0f)
			};
			if (preferences.energy == gui::aegis::energy_quality::high)
				draw_list->AddPolyline(points, IM_ARRAYSIZE(points), color_with_alpha(accent, 0.045f), 0, 5.0f);
			draw_list->AddPolyline(points, IM_ARRAYSIZE(points), color_with_alpha(accent, 0.10f + layer * 0.025f), 0, 1.0f);
			for (int node = 1; node < IM_ARRAYSIZE(points) - 1; ++node)
				draw_list->AddCircleFilled(points[node], 1.7f, color_with_alpha(accent, 0.18f), 12);
		}
		draw_list->PopClipRect();
	}

	bool aegis_icon_button(const char* id, sidebar_icon icon, const ImVec2& position, bool selected, const char* tooltip)
	{
		ImGui::PushID(id);
		ImGui::SetCursorScreenPos(position);
		ImGui::InvisibleButton("##aegis_icon", ImVec2(34.0f, 34.0f));
		const bool hovered = ImGui::IsItemHovered();
		const bool clicked = ImGui::IsItemClicked();
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(0);
		if (selected || hovered)
			draw_list->AddRectFilled(position, ImVec2(position.x + 34.0f, position.y + 34.0f),
				selected ? color_with_alpha(accent, 0.20f) : IM_COL32(255, 255, 255, 9), 9.0f);
		if (selected)
			draw_list->AddRect(position, ImVec2(position.x + 34.0f, position.y + 34.0f),
				color_with_alpha(accent, 0.55f), 9.0f, 0, 1.0f);
		draw_sidebar_icon(draw_list, icon, ImVec2(position.x + 17.0f, position.y + 17.0f),
			selected ? color_with_alpha(accent) : IM_COL32(139, 143, 155, hovered ? 255 : 210));
		if (hovered && tooltip)
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(tooltip);
			ImGui::EndTooltip();
		}
		ImGui::PopID();
		return clicked;
	}

	bool layout_button(const char* id, const ImVec2& position, bool vertical, bool selected)
	{
		ImGui::PushID(id);
		ImGui::SetCursorScreenPos(position);
		ImGui::InvisibleButton("##layout", ImVec2(34.0f, 34.0f));
		const bool hovered = ImGui::IsItemHovered();
		const bool clicked = ImGui::IsItemClicked();
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(0);
		draw_list->AddRectFilled(position, ImVec2(position.x + 34.0f, position.y + 34.0f),
			selected ? color_with_alpha(accent, 0.24f) : IM_COL32(24, 27, 34, hovered ? 255 : 224), 8.0f);
		const ImU32 color = selected ? color_with_alpha(accent) : IM_COL32(135, 140, 153, 255);
		if (vertical)
		{
			for (int column = 0; column < 3; ++column)
				draw_list->AddRectFilled(ImVec2(position.x + 8.0f + column * 7.0f, position.y + 9.0f),
					ImVec2(position.x + 12.0f + column * 7.0f, position.y + 25.0f), color, 1.5f);
		}
		else
		{
			for (int row = 0; row < 3; ++row)
				draw_list->AddRectFilled(ImVec2(position.x + 9.0f, position.y + 7.0f + row * 7.0f),
					ImVec2(position.x + 27.0f, position.y + 11.0f + row * 7.0f), color, 1.5f);
		}
		ImGui::PopID();
		return clicked;
	}

	bool panic_icon_button(const char* id, const ImVec2& position)
	{
		ImGui::PushID(id);
		ImGui::SetCursorScreenPos(position);
		ImGui::InvisibleButton("##panic_icon", ImVec2(34.0f, 34.0f));
		const bool hovered = ImGui::IsItemHovered();
		const bool clicked = ImGui::IsItemClicked();
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		draw_list->AddRectFilled(position, ImVec2(position.x + 34.0f, position.y + 34.0f),
			hovered ? IM_COL32(118, 31, 47, 235) : IM_COL32(49, 25, 33, 225), 8.0f);
		const ImU32 color = hovered ? IM_COL32(255, 103, 121, 255) : IM_COL32(196, 87, 103, 255);
		const ImVec2 center(position.x + 17.0f, position.y + 17.0f);
		draw_list->AddLine(ImVec2(center.x - 5.0f, center.y - 5.0f), ImVec2(center.x + 5.0f, center.y + 5.0f), color, 1.8f);
		draw_list->AddLine(ImVec2(center.x + 5.0f, center.y - 5.0f), ImVec2(center.x - 5.0f, center.y + 5.0f), color, 1.8f);
		if (hovered)
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(gui::text("Panic", "Panik"));
			ImGui::EndTooltip();
		}
		ImGui::PopID();
		return clicked;
	}

	void select_module_from_palette(modules::module* module)
	{
		if (!module) return;
		aegis_runtime.page = aegis_page::modules;
		aegis_runtime.active_category = category_index(module->get_category());
		if (gui::aegis::get_preferences().layout == gui::aegis::layout_mode::horizontal_workspace)
			aegis_runtime.horizontal_module = module;
		else if (module_has_options(module))
		{
			for (modules::module* other : modules::get_modules())
				if (other && other != module && std::strcmp(other->get_category(), module->get_category()) == 0)
					other->display_options = false;
			module->display_options = true;
		}
		aegis_runtime.search[0] = '\0';
	}

	void render_command_palette(const ImVec2& window_position, const ImVec2& window_size, float search_x, float search_width)
	{
		if (!aegis_runtime.search[0]) return;
		std::vector<modules::module*> results;
		for (modules::module* module : modules::get_modules())
			if (module && std::strcmp(module->get_category(), "Friends") != 0 && module_matches(module, aegis_runtime.search))
				results.push_back(module);
		if (results.size() > 7) results.resize(7);
		aegis_runtime.palette_selection = std::clamp(aegis_runtime.palette_selection, 0,
			std::max(0, static_cast<int>(results.size()) - 1));
		if (!results.empty())
		{
			if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
				aegis_runtime.palette_selection = (aegis_runtime.palette_selection + 1) % static_cast<int>(results.size());
			if (ImGui::IsKeyPressed(ImGuiKey_UpArrow))
				aegis_runtime.palette_selection = (aegis_runtime.palette_selection - 1 + static_cast<int>(results.size())) %
					static_cast<int>(results.size());
			modules::module* selected = results[aegis_runtime.palette_selection];
			if (ImGui::IsKeyPressed(ImGuiKey_Enter)) select_module_from_palette(selected);
			if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_Space)) selected->enabled = !selected->enabled;
			if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_B)) selected->begin_keybind_capture();
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Escape)) aegis_runtime.search[0] = '\0';

		const float palette_height = results.empty() ? 66.0f : 12.0f + results.size() * 46.0f;
		ImGui::SetCursorScreenPos(ImVec2(search_x, window_position.y + 48.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 10.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 6.0f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.045f, 0.052f, 0.068f, 0.99f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.10f));
		if (ImGui::BeginChild("##aegis_palette", ImVec2(search_width, palette_height), ImGuiChildFlags_Border))
		{
			if (results.empty())
			{
				ImGui::SetCursorPosY(21.0f);
				const char* empty = gui::text("No modules found", "Modül bulunamadı");
				ImGui::SetCursorPosX((search_width - ImGui::CalcTextSize(empty).x) * 0.5f);
				ImGui::TextDisabled("%s", empty);
			}
			for (int index = 0; index < static_cast<int>(results.size()); ++index)
			{
				modules::module* module = results[index];
				ImGui::PushID(module);
				const ImVec2 row_min = ImGui::GetCursorScreenPos();
				const float row_width = ImGui::GetContentRegionAvail().x;
				ImGui::InvisibleButton("##result", ImVec2(row_width, 40.0f));
				const bool hovered = ImGui::IsItemHovered();
				if (ImGui::IsItemClicked()) select_module_from_palette(module);
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				if (hovered || index == aegis_runtime.palette_selection)
					draw_list->AddRectFilled(row_min, ImVec2(row_min.x + row_width, row_min.y + 40.0f),
						color_with_alpha(gui::get_dynamic_color(0), hovered ? 0.14f : 0.08f), 7.0f);
				const std::string title = display_name(module->get_name());
				draw_list->AddText(gui::ui_bold_font, 14.5f, ImVec2(row_min.x + 10.0f, row_min.y + 5.0f),
					IM_COL32(238, 240, 246, 255), title.c_str());
				draw_list->AddText(ImVec2(row_min.x + 10.0f, row_min.y + 23.0f), IM_COL32(115, 120, 133, 255),
					display_category(module->get_category()));
				draw_list->AddCircleFilled(ImVec2(row_min.x + row_width - 16.0f, row_min.y + 20.0f), 3.0f,
					module->enabled ? color_with_alpha(gui::get_dynamic_color(0)) : IM_COL32(77, 81, 92, 255), 12);
				ImGui::SetCursorScreenPos(ImVec2(row_min.x, row_min.y + 42.0f));
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(2);
	}

	void render_vertical_module(modules::module* module)
	{
		if (!module) return;
		ImGui::PushID(module->get_id());
		const bool has_options = module_has_options(module);
		const float width = ImGui::GetContentRegionAvail().x;
		const ImVec2 card_min = ImGui::GetCursorScreenPos();
		const ImVec2 card_max(card_min.x + width, card_min.y + 60.0f);
		ImGui::InvisibleButton("##vertical_module", ImVec2(width - 58.0f, 60.0f));
		const bool hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked() && has_options) module->display_options = !module->display_options;
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(0);
		if (module->enabled && gui::aegis::get_preferences().energy == gui::aegis::energy_quality::high)
			draw_list->AddRectFilled(ImVec2(card_min.x - 2.0f, card_min.y - 2.0f),
				ImVec2(card_max.x + 2.0f, card_max.y + 2.0f), color_with_alpha(accent, 0.06f), 11.0f);
		draw_list->AddRectFilled(card_min, card_max, hovered ? IM_COL32(25, 29, 37, 250) : IM_COL32(20, 23, 30, 248), 9.0f);
		draw_list->AddRect(card_min, card_max,
			module->enabled ? color_with_alpha(accent, 0.40f) : IM_COL32(255, 255, 255, 12), 9.0f, 0, 1.0f);
		if (module->enabled)
			draw_list->AddRectFilled(card_min, ImVec2(card_min.x + 2.0f, card_max.y), color_with_alpha(accent), 9.0f,
				ImDrawFlags_RoundCornersLeft);
		const std::string title = display_name(module->get_name());
		const ImVec4 title_clip(card_min.x + 12.0f, card_min.y + 8.0f, card_max.x - 64.0f, card_min.y + 28.0f);
		draw_list->AddText(gui::ui_bold_font, 14.5f, ImVec2(card_min.x + 12.0f, card_min.y + 8.0f),
			IM_COL32(238, 240, 246, 255), title.c_str(), nullptr, 0.0f, &title_clip);
		const ImVec4 description_clip(card_min.x + 12.0f, card_min.y + 31.0f, card_max.x - 64.0f, card_min.y + 51.0f);
		draw_list->AddText(ImGui::GetFont(), 12.5f, ImVec2(card_min.x + 12.0f, card_min.y + 33.0f),
			IM_COL32(123, 128, 141, 255), module_description(module->get_name()), nullptr, 0.0f, &description_clip);
		if (has_options)
		{
			const ImVec2 chevron(card_max.x - 68.0f, card_min.y + 20.0f);
			ImGui::SetCursorScreenPos(chevron);
			ImGui::InvisibleButton("##accordion", ImVec2(24.0f, 24.0f));
			if (ImGui::IsItemClicked()) module->display_options = !module->display_options;
			const float direction = module->display_options_progress;
			const ImVec2 center(chevron.x + 12.0f, chevron.y + 12.0f);
			const float angle = direction * 1.5707963f;
			const ImVec2 a(std::cos(angle), std::sin(angle));
			const ImVec2 b(-std::sin(angle), std::cos(angle));
			draw_list->AddLine(ImVec2(center.x - a.x * 4.0f - b.x * 3.0f, center.y - a.y * 4.0f - b.y * 3.0f), center,
				color_with_alpha(accent, 0.9f), 1.4f);
			draw_list->AddLine(center, ImVec2(center.x - a.x * 4.0f + b.x * 3.0f, center.y - a.y * 4.0f + b.y * 3.0f),
				color_with_alpha(accent, 0.9f), 1.4f);
		}
		toggle_switch("##vertical_toggle", &module->enabled, ImVec2(card_max.x - 39.0f, card_min.y + 19.5f), 32.0f, 20.0f);

		const auto& preferences = gui::aegis::get_preferences();
		const float target = module->display_options ? 1.0f : 0.0f;
		module->display_options_progress = preferences.reduce_motion
			? target : smooth_value(module->display_options_progress, target, 14.0f);
		if (std::abs(module->display_options_progress - target) < 0.002f)
			module->display_options_progress = target;
		const float progress = module->display_options_progress;
		const float full_height = std::max(78.0f, module->get_options_height() - 32.0f);
		const float options_height = full_height * progress;
		ImGui::SetCursorScreenPos(ImVec2(card_min.x, card_max.y + 6.0f));
		if (has_options && options_height > 0.5f)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(0.04f, progress));
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 9.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.064f, 0.082f, 0.98f));
			ImVec4 accordion_border = gui::get_dynamic_color(0).Value;
			accordion_border.w = 0.24f;
			ImGui::PushStyleColor(ImGuiCol_Border, accordion_border);
			if (ImGui::BeginChild("##vertical_options", ImVec2(width, options_height), ImGuiChildFlags_Border,
				ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
			{
				const ImVec2 bind_start = ImGui::GetCursorScreenPos();
				ImGui::TextColored(ImVec4(0.55f, 0.57f, 0.63f, 1.0f), "%s", gui::text("Bind", "Tuş"));
				ImGui::SetCursorScreenPos(ImVec2(bind_start.x + ImGui::GetContentRegionAvail().x - 72.0f, bind_start.y - 3.0f));
				module->key_bind_selector();
				ImGui::SetCursorScreenPos(ImVec2(bind_start.x, bind_start.y + 35.0f));
				module->render_options();
			}
			ImGui::EndChild();
			ImGui::PopStyleColor(2);
			ImGui::PopStyleVar(3);
			ImGui::SetCursorScreenPos(ImVec2(card_min.x, card_max.y + options_height + 12.0f));
		}
		else
		{
			ImGui::SetCursorScreenPos(ImVec2(card_min.x, card_max.y + 8.0f));
		}
		ImGui::PopID();
	}

	void render_vertical_columns(const ImVec2& size)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.0f);
		if (ImGui::BeginChild("##vertical_workspace", size, ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar))
		{
			const float available = ImGui::GetContentRegionAvail().x;
			const float gap = 10.0f;
			float column_width = std::floor((available - gap * 3.0f) / 4.0f);
			column_width = std::clamp(column_width, 224.0f, 310.0f);
			const float total_width = column_width * 4.0f + gap * 3.0f;
			const float start_x = ImGui::GetCursorPosX() + std::max(0.0f, (available - total_width) * 0.5f);
			const float start_y = ImGui::GetCursorPosY();
			for (int category = 0; category < IM_ARRAYSIZE(aegis_categories); ++category)
			{
				ImGui::PushID(category);
				ImGui::SetCursorPos(ImVec2(start_x + category * (column_width + gap), start_y));
				ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 11.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.052f, 0.060f, 0.076f, 0.96f));
				ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.055f));
				if (ImGui::BeginChild("##category_column", ImVec2(column_width, size.y - 16.0f), ImGuiChildFlags_Border))
				{
					int enabled = 0;
					for (modules::module* module : modules::get_modules())
						if (module && module->enabled && std::strcmp(module->get_category(), aegis_categories[category]) == 0) ++enabled;
					const ImVec2 header = ImGui::GetCursorScreenPos();
					draw_sidebar_icon(ImGui::GetWindowDrawList(), aegis_category_icons[category],
						ImVec2(header.x + 13.0f, header.y + 15.0f), color_with_alpha(gui::get_dynamic_color(category * 35)));
					ImGui::GetWindowDrawList()->AddText(gui::ui_bold_font, 16.0f, ImVec2(header.x + 30.0f, header.y + 5.0f),
						IM_COL32(238, 240, 246, 255), display_category(aegis_categories[category]));
					const std::string count = std::to_string(enabled) + " / " + std::to_string(std::count_if(
						modules::get_modules().begin(), modules::get_modules().end(), [category](modules::module* module)
						{ return module && std::strcmp(module->get_category(), aegis_categories[category]) == 0; }));
					const ImVec2 count_size = ImGui::CalcTextSize(count.c_str());
					ImGui::GetWindowDrawList()->AddText(ImVec2(header.x + column_width - count_size.x - 18.0f, header.y + 7.0f),
						IM_COL32(104, 110, 124, 255), count.c_str());
					ImGui::GetWindowDrawList()->AddLine(ImVec2(header.x, header.y + 37.0f),
						ImVec2(header.x + column_width - 16.0f, header.y + 37.0f), IM_COL32(255, 255, 255, 13));
					ImGui::Dummy(ImVec2(0.0f, 43.0f));
					for (modules::module* module : modules::get_modules())
						if (module && std::strcmp(module->get_category(), aegis_categories[category]) == 0 &&
							module_matches(module, aegis_runtime.search)) render_vertical_module(module);
				}
				ImGui::EndChild();
				ImGui::PopStyleColor(2);
				ImGui::PopStyleVar(2);
				ImGui::PopID();
			}
			ImGui::Dummy(ImVec2(total_width, 1.0f));
		}
		ImGui::EndChild();
		ImGui::PopStyleVar();
	}

	void render_horizontal_workspace(const ImVec2& size)
	{
		const float rail_width = 64.0f;
		const float list_width = std::clamp(size.x * 0.30f, 280.0f, 340.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 11.0f);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.052f, 0.060f, 0.076f, 0.97f));
		if (ImGui::BeginChild("##horizontal_rail", ImVec2(rail_width, size.y), ImGuiChildFlags_Border))
		{
			const ImVec2 rail = ImGui::GetWindowPos();
			for (int category = 0; category < IM_ARRAYSIZE(aegis_categories); ++category)
				if (aegis_icon_button(aegis_categories[category], aegis_category_icons[category],
					ImVec2(rail.x + 14.0f, rail.y + 14.0f + category * 45.0f),
					aegis_runtime.active_category == category, display_category(aegis_categories[category])))
					aegis_runtime.active_category = category;
		}
		ImGui::EndChild();
		ImGui::SameLine(0.0f, 10.0f);
		if (ImGui::BeginChild("##horizontal_modules", ImVec2(list_width, size.y), ImGuiChildFlags_Border))
		{
			const char* category = aegis_categories[aegis_runtime.active_category];
			ImGui::TextColored(ImVec4(0.94f, 0.95f, 0.98f, 1.0f), "%s", display_category(category));
			ImGui::SameLine();
			ImGui::TextDisabled("%s", gui::text("modules", "modüller"));
			ImGui::Dummy(ImVec2(0.0f, 5.0f));
			selected_settings_module = aegis_runtime.horizontal_module;
			for (modules::module* module : modules::get_modules())
				if (module && std::strcmp(module->get_category(), category) == 0 && module_matches(module, aegis_runtime.search))
					render_module_card(module, ImGui::GetContentRegionAvail().x - 3.0f);
			if (selected_settings_module != aegis_runtime.horizontal_module)
				aegis_runtime.horizontal_module = selected_settings_module;
		}
		ImGui::EndChild();
		ImGui::SameLine(0.0f, 10.0f);
		const float inspector_width = std::max(260.0f, size.x - rail_width - list_width - 20.0f);
		if (ImGui::BeginChild("##horizontal_inspector", ImVec2(inspector_width, size.y), ImGuiChildFlags_None))
		{
			if (aegis_runtime.horizontal_module && module_has_options(aegis_runtime.horizontal_module))
			{
				selected_settings_module = aegis_runtime.horizontal_module;
				render_module_settings(aegis_runtime.horizontal_module, ImGui::GetContentRegionAvail().x,
					ImGui::GetContentRegionAvail().y);
				if (!selected_settings_module) aegis_runtime.horizontal_module = nullptr;
			}
			else
			{
				const char* title = gui::text("Select a module", "Bir modül seç");
				const char* body = gui::text("Its controls will appear in this inspector.", "Kontrolleri bu panelde görünecek.");
				const ImVec2 available = ImGui::GetContentRegionAvail();
				ImGui::SetCursorPosY(available.y * 0.42f);
				ImGui::SetCursorPosX((available.x - ImGui::CalcTextSize(title).x) * 0.5f);
				ImGui::TextColored(ImVec4(0.72f, 0.74f, 0.80f, 1.0f), "%s", title);
				ImGui::SetCursorPosX((available.x - ImGui::CalcTextSize(body).x) * 0.5f);
				ImGui::TextDisabled("%s", body);
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}

	void render_aegis_page(const ImVec2& size)
	{
		switch (aegis_runtime.page)
		{
		case aegis_page::friends:
			friends::render_manager();
			break;
		case aegis_page::configs:
			render_configs_page();
			break;
		case aegis_page::settings:
			render_settings_page();
			break;
		case aegis_page::modules:
		default:
			if (gui::aegis::get_preferences().layout == gui::aegis::layout_mode::vertical_columns)
				render_vertical_columns(size);
			else
				render_horizontal_workspace(size);
			break;
		}
	}

	void render_aegis_header(const ImVec2& window_position, const ImVec2& window_size)
	{
		auto& preferences = gui::aegis::get_preferences();
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(0);
		const ImVec2 end(window_position.x + window_size.x, window_position.y + 52.0f);
		draw_list->AddRectFilled(window_position, end, IM_COL32(14, 17, 23, 250), 14.0f, ImDrawFlags_RoundCornersTop);
		draw_list->AddLine(ImVec2(window_position.x + 12.0f, end.y), ImVec2(window_position.x + window_size.x - 12.0f, end.y),
			color_with_alpha(accent, 0.24f));

		ImGui::SetCursorScreenPos(ImVec2(window_position.x + 10.0f, window_position.y + 8.0f));
		ImGui::InvisibleButton("##aegis_drag", ImVec2(166.0f, 36.0f));
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
			ImGui::SetWindowPos(ImVec2(window_position.x + ImGui::GetIO().MouseDelta.x,
				window_position.y + ImGui::GetIO().MouseDelta.y));
		if (ImTextureID helmet = gui::get_icon_texture(gui::icon_asset::helmet))
			draw_list->AddImage(helmet, ImVec2(window_position.x + 15.0f, window_position.y + 10.0f),
				ImVec2(window_position.x + 47.0f, window_position.y + 42.0f));
		draw_list->AddText(gui::ui_bold_font, 18.0f, ImVec2(window_position.x + 55.0f, window_position.y + 11.0f),
			IM_COL32(242, 244, 249, 255), "THEDORIUS");
		draw_list->AddText(ImVec2(window_position.x + 56.0f, window_position.y + 30.0f),
			color_with_alpha(accent, 0.82f), "AEGIS MATRIX");

		const float controls_right = window_position.x + window_size.x - 14.0f;
		const float search_width = std::clamp(window_size.x * 0.26f, 210.0f, 330.0f);
		const float search_x = std::min(window_position.x + 190.0f,
			controls_right - search_width - 320.0f);
		if (ImGui::IsKeyChordPressed(ImGuiMod_Ctrl | ImGuiKey_K)) aegis_runtime.focus_search = true;
		ImGui::SetCursorScreenPos(ImVec2(search_x, window_position.y + 10.0f));
		ImGui::SetNextItemWidth(search_width);
		if (aegis_runtime.focus_search)
		{
			ImGui::SetKeyboardFocusHere();
			aegis_runtime.focus_search = false;
		}
		ImGui::InputTextWithHint("##aegis_search", gui::text("Search modules and settings  Ctrl+K", "Modül ve ayar ara  Ctrl+K"),
			aegis_runtime.search, IM_ARRAYSIZE(aegis_runtime.search));

		const float layout_x = controls_right - 300.0f;
		if (layout_button("vertical", ImVec2(layout_x, window_position.y + 11.0f), true,
			preferences.layout == gui::aegis::layout_mode::vertical_columns))
		{
			preferences.layout = gui::aegis::layout_mode::vertical_columns;
			gui::aegis::mark_preferences_dirty();
		}
		if (layout_button("horizontal", ImVec2(layout_x + 40.0f, window_position.y + 11.0f), false,
			preferences.layout == gui::aegis::layout_mode::horizontal_workspace))
		{
			preferences.layout = gui::aegis::layout_mode::horizontal_workspace;
			gui::aegis::mark_preferences_dirty();
		}

		const float utility_x = layout_x + 89.0f;
		if (aegis_icon_button("friends", sidebar_icon::friends, ImVec2(utility_x, window_position.y + 9.0f),
			aegis_runtime.page == aegis_page::friends, gui::text("Friends", "Arkadaşlar")))
			aegis_runtime.page = aegis_runtime.page == aegis_page::friends ? aegis_page::modules : aegis_page::friends;
		if (aegis_icon_button("configs", sidebar_icon::configs, ImVec2(utility_x + 39.0f, window_position.y + 9.0f),
			aegis_runtime.page == aegis_page::configs, "Configs"))
			aegis_runtime.page = aegis_runtime.page == aegis_page::configs ? aegis_page::modules : aegis_page::configs;
		if (aegis_icon_button("settings", sidebar_icon::settings, ImVec2(utility_x + 78.0f, window_position.y + 9.0f),
			aegis_runtime.page == aegis_page::settings, gui::text("Settings", "Ayarlar")))
			aegis_runtime.page = aegis_runtime.page == aegis_page::settings ? aegis_page::modules : aegis_page::settings;

		ImGui::SetCursorScreenPos(ImVec2(controls_right - 67.0f, window_position.y + 11.0f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.38f, 0.035f, 0.075f, 0.95f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.68f, 0.06f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.82f, 0.08f, 0.15f, 1.0f));
		if (ImGui::Button(gui::text("Panic##aegis", "Panik##aegis"), ImVec2(67.0f, 30.0f))) begin_panic_animation();
		ImGui::PopStyleColor(3);

		render_command_palette(window_position, window_size, search_x, search_width);
	}

	void render_reference_horizontal_card(modules::module* module, float width)
	{
		if (!module || width < 190.0f) return;
		ImGui::PushID(module->get_id());
		const ImVec2 minimum = ImGui::GetCursorScreenPos();
		const ImVec2 maximum(minimum.x + width, minimum.y + 55.0f);
		ImGui::InvisibleButton("##reference_card", ImVec2(width - 44.0f, 55.0f));
		const bool hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked())
		{
			aegis_runtime.horizontal_module = module_has_options(module) ? module : nullptr;
		}
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(0);
		const ImU32 background = module->enabled
			? color_with_alpha(accent, hovered ? 0.23f : 0.15f)
			: (hovered ? IM_COL32(31, 33, 39, 238) : IM_COL32(23, 25, 30, 230));
		draw_list->AddRectFilled(minimum, maximum, background, 8.0f);
		draw_list->AddRect(minimum, maximum,
			module->enabled ? color_with_alpha(accent, 0.38f) : IM_COL32(255, 255, 255, 8), 8.0f);
		const std::string title = display_name(module->get_name());
		const ImVec4 title_clip(minimum.x + 12.0f, minimum.y + 7.0f, maximum.x - 48.0f, minimum.y + 27.0f);
		draw_list->AddText(gui::ui_bold_font, 14.5f, ImVec2(minimum.x + 12.0f, minimum.y + 7.0f),
			module->enabled ? color_with_alpha(accent) : IM_COL32(237, 238, 243, 255),
			title.c_str(), nullptr, 0.0f, &title_clip);
		const ImVec4 description_clip(minimum.x + 12.0f, minimum.y + 29.0f, maximum.x - 48.0f, maximum.y - 5.0f);
		draw_list->AddText(ImGui::GetFont(), 12.0f, ImVec2(minimum.x + 12.0f, minimum.y + 31.0f),
			IM_COL32(126, 128, 137, 255), module_description(module->get_name()), nullptr, 0.0f, &description_clip);
		toggle_switch("##reference_toggle", &module->enabled,
			ImVec2(maximum.x - 39.0f, minimum.y + 17.5f), 32.0f, 20.0f);
		ImGui::SetCursorScreenPos(ImVec2(minimum.x, maximum.y + 7.0f));
		ImGui::PopID();
	}

	void render_reference_inspector(modules::module* module, const ImVec2& minimum, const ImVec2& size)
	{
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(0);
		const ImVec2 maximum(minimum.x + size.x, minimum.y + size.y);
		draw_list->AddRectFilled(minimum, maximum, IM_COL32(16, 17, 21, 239), 13.0f);
		draw_list->AddRect(minimum, maximum, IM_COL32(255, 255, 255, 18), 13.0f);
		if (!module || !module_has_options(module))
		{
			const char* empty = gui::text("Select a module", "Bir modül seç");
			const ImVec2 extent = ImGui::CalcTextSize(empty);
			draw_list->AddText(ImVec2(minimum.x + (size.x - extent.x) * 0.5f,
				minimum.y + size.y * 0.46f), IM_COL32(125, 127, 136, 255), empty);
			return;
		}

		ImGui::PushID(module->get_id());
		const std::string title = display_name(module->get_name());
		draw_list->AddText(gui::ui_bold_font, 18.0f, ImVec2(minimum.x + 15.0f, minimum.y + 14.0f),
			IM_COL32(242, 243, 247, 255), title.c_str());
		ImGui::SetCursorScreenPos(ImVec2(maximum.x - 34.0f, minimum.y + 10.0f));
		if (ImGui::InvisibleButton("##close_reference_inspector", ImVec2(24.0f, 24.0f)))
			aegis_runtime.horizontal_module = nullptr;
		const ImVec2 close_center(maximum.x - 22.0f, minimum.y + 22.0f);
		draw_list->AddLine(ImVec2(close_center.x - 4.0f, close_center.y - 4.0f),
			ImVec2(close_center.x + 4.0f, close_center.y + 4.0f), IM_COL32(180, 181, 188, 255), 1.4f);
		draw_list->AddLine(ImVec2(close_center.x + 4.0f, close_center.y - 4.0f),
			ImVec2(close_center.x - 4.0f, close_center.y + 4.0f), IM_COL32(180, 181, 188, 255), 1.4f);
		draw_list->AddLine(ImVec2(minimum.x + 14.0f, minimum.y + 50.0f),
			ImVec2(maximum.x - 14.0f, minimum.y + 50.0f), IM_COL32(255, 255, 255, 13));

		const ImVec2 enabled_position(minimum.x + 14.0f, minimum.y + 63.0f);
		draw_list->AddText(enabled_position, IM_COL32(210, 211, 217, 255), gui::text("Enabled", "Etkin"));
		toggle_switch("##reference_enabled", &module->enabled,
			ImVec2(maximum.x - 49.0f, minimum.y + 59.0f), 35.0f, 21.0f);
		const ImVec2 bind_position(minimum.x + 14.0f, minimum.y + 94.0f);
		draw_list->AddText(bind_position, IM_COL32(160, 162, 171, 255), gui::text("Keybind", "Tuş"));
		ImGui::SetCursorScreenPos(ImVec2(maximum.x - 88.0f, minimum.y + 88.0f));
		module->key_bind_selector();

		ImGui::SetCursorScreenPos(ImVec2(minimum.x + 10.0f, minimum.y + 126.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.0f);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.075f, 0.077f, 0.09f, 0.0f));
		if (ImGui::BeginChild("##reference_inspector_options", ImVec2(size.x - 20.0f, size.y - 136.0f)))
			module->render_options();
		ImGui::EndChild();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);
		ImGui::PopID();
	}

	void reference_sidebar_entry(const char* id, sidebar_icon icon, const char* label, const ImVec2& position,
		bool selected, aegis_page page, int category = -1)
	{
		ImGui::PushID(id);
		ImGui::SetCursorScreenPos(position);
		ImGui::InvisibleButton("##sidebar_entry", ImVec2(126.0f, 35.0f));
		const bool hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked())
		{
			aegis_runtime.page = page;
			if (category >= 0) aegis_runtime.active_category = category;
		}
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(0);
		if (selected || hovered)
			draw_list->AddRectFilled(position, ImVec2(position.x + 126.0f, position.y + 35.0f),
				selected ? color_with_alpha(accent, 0.82f) : IM_COL32(255, 255, 255, 8), 7.0f);
		const ImU32 color = selected ? IM_COL32(250, 250, 252, 255) : IM_COL32(177, 179, 187, 255);
		draw_sidebar_icon(draw_list, icon, ImVec2(position.x + 17.0f, position.y + 17.5f), color);
		draw_list->AddText(gui::ui_bold_font, 14.0f, ImVec2(position.x + 36.0f, position.y + 9.0f), color, label);
		ImGui::PopID();
	}

	void render_reference_horizontal(const ImGuiViewport* viewport)
	{
		auto& preferences = gui::aegis::get_preferences();
		static bool positioned = false;
		const ImVec2 desired(std::min(1040.0f, viewport->Size.x - 40.0f),
			std::min(620.0f, viewport->Size.y - 40.0f));
		ImGui::SetNextWindowSizeConstraints(ImVec2(900.0f, 520.0f),
			ImVec2(viewport->Size.x - 24.0f, viewport->Size.y - 24.0f));
		if (!positioned)
		{
			ImGui::SetNextWindowSize(preferences.has_geometry ? preferences.window_size : desired, ImGuiCond_Always);
			ImGui::SetNextWindowPos(preferences.has_geometry ? preferences.window_position
				: ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f),
				ImGuiCond_Always, preferences.has_geometry ? ImVec2(0.0f, 0.0f) : ImVec2(0.5f, 0.5f));
			positioned = true;
		}
		ImGui::SetNextWindowBgAlpha(0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		if (ImGui::Begin("##ThedoriusHorizontalReference", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoSavedSettings))
		{
			const ImVec2 position = ImGui::GetWindowPos();
			const ImVec2 size = ImGui::GetWindowSize();
			const float gap = 14.0f;
			const float inspector_width = std::clamp(size.x * 0.245f, 230.0f, 285.0f);
			const float main_width = size.x - inspector_width - gap;
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			draw_list->AddRectFilled(position, ImVec2(position.x + main_width, position.y + size.y),
				IM_COL32(15, 16, 20, 239), 14.0f);
			draw_list->AddRect(position, ImVec2(position.x + main_width, position.y + size.y),
				IM_COL32(255, 255, 255, 16), 14.0f);
			const float sidebar_width = 154.0f;
			draw_list->AddRectFilled(position, ImVec2(position.x + sidebar_width, position.y + size.y),
				IM_COL32(20, 20, 25, 246), 14.0f, ImDrawFlags_RoundCornersLeft);
			draw_list->AddLine(ImVec2(position.x + sidebar_width, position.y + 12.0f),
				ImVec2(position.x + sidebar_width, position.y + size.y - 12.0f), IM_COL32(255, 255, 255, 10));

			ImGui::SetCursorScreenPos(ImVec2(position.x + 10.0f, position.y + 8.0f));
			ImGui::InvisibleButton("##horizontal_drag", ImVec2(main_width - 20.0f, 42.0f));
			if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
				ImGui::SetWindowPos(ImVec2(position.x + ImGui::GetIO().MouseDelta.x,
					position.y + ImGui::GetIO().MouseDelta.y));
			if (ImTextureID helmet = gui::get_icon_texture(gui::icon_asset::helmet))
				draw_list->AddImage(helmet, ImVec2(position.x + 16.0f, position.y + 15.0f),
					ImVec2(position.x + 51.0f, position.y + 50.0f));
			draw_list->AddText(gui::ui_bold_font, 16.0f, ImVec2(position.x + 59.0f, position.y + 16.0f),
				IM_COL32(242, 243, 247, 255), "Thedorius");
			draw_list->AddText(ImVec2(position.x + 60.0f, position.y + 36.0f), IM_COL32(116, 117, 126, 255), "Version 0.3");

			const float nav_x = position.x + 14.0f;
			float nav_y = position.y + 84.0f;
			for (int category = 0; category < IM_ARRAYSIZE(aegis_categories); ++category)
			{
				reference_sidebar_entry(aegis_categories[category], aegis_category_icons[category],
					display_category(aegis_categories[category]), ImVec2(nav_x, nav_y),
					aegis_runtime.page == aegis_page::modules && aegis_runtime.active_category == category,
					aegis_page::modules, category);
				nav_y += 41.0f;
			}
			nav_y += 13.0f;
			reference_sidebar_entry("friends", sidebar_icon::friends, gui::text("Friends", "Arkadaşlar"),
				ImVec2(nav_x, nav_y), aegis_runtime.page == aegis_page::friends, aegis_page::friends); nav_y += 41.0f;
			reference_sidebar_entry("configs", sidebar_icon::configs, "Configs",
				ImVec2(nav_x, nav_y), aegis_runtime.page == aegis_page::configs, aegis_page::configs); nav_y += 41.0f;
			reference_sidebar_entry("settings", sidebar_icon::settings, gui::text("Settings", "Ayarlar"),
				ImVec2(nav_x, nav_y), aegis_runtime.page == aegis_page::settings, aegis_page::settings);

			const ImVec2 content_min(position.x + sidebar_width + 18.0f, position.y + 16.0f);
			const float content_width = main_width - sidebar_width - 34.0f;
			if (aegis_runtime.page == aegis_page::modules)
			{
				const int category = aegis_runtime.active_category;
				draw_sidebar_icon(draw_list, aegis_category_icons[category],
					ImVec2(content_min.x + 10.0f, content_min.y + 12.0f), color_with_alpha(gui::get_dynamic_color(0)));
				draw_list->AddText(gui::ui_bold_font, 17.0f, ImVec2(content_min.x + 27.0f, content_min.y + 2.0f),
					IM_COL32(239, 240, 245, 255), display_category(aegis_categories[category]));
				ImGui::SetCursorScreenPos(ImVec2(content_min.x + content_width - 205.0f, content_min.y - 3.0f));
				ImGui::SetNextItemWidth(205.0f);
				ImGui::InputTextWithHint("##reference_search", gui::text("Search modules", "Modül ara"),
					aegis_runtime.search, IM_ARRAYSIZE(aegis_runtime.search));
				ImGui::SetCursorScreenPos(ImVec2(content_min.x, content_min.y + 45.0f));
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
				if (ImGui::BeginChild("##reference_grid", ImVec2(content_width, size.y - 62.0f)))
				{
					const float card_gap = 8.0f;
					const float card_width = (ImGui::GetContentRegionAvail().x - card_gap) * 0.5f;
					int visible_index = 0;
					for (modules::module* module : modules::get_modules())
					{
						if (!module || std::strcmp(module->get_category(), aegis_categories[category]) != 0 ||
							!module_matches(module, aegis_runtime.search)) continue;
						if ((visible_index & 1) != 0) ImGui::SameLine(0.0f, card_gap);
						render_reference_horizontal_card(module, card_width);
						++visible_index;
					}
				}
				ImGui::EndChild();
				ImGui::PopStyleVar();
			}
			else
			{
				ImGui::SetCursorScreenPos(ImVec2(content_min.x, content_min.y));
				ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
				if (ImGui::BeginChild("##reference_utility", ImVec2(content_width, size.y - 30.0f)))
					render_aegis_page(ImGui::GetContentRegionAvail());
				ImGui::EndChild();
				ImGui::PopStyleVar();
			}

			render_reference_inspector(aegis_runtime.horizontal_module,
				ImVec2(position.x + main_width + gap, position.y), ImVec2(inspector_width, size.y));
			const ImVec2 current_position = ImGui::GetWindowPos();
			const ImVec2 current_size = ImGui::GetWindowSize();
			if (std::abs(preferences.window_position.x - current_position.x) > 0.5f ||
				std::abs(preferences.window_position.y - current_position.y) > 0.5f ||
				std::abs(preferences.window_size.x - current_size.x) > 0.5f ||
				std::abs(preferences.window_size.y - current_size.y) > 0.5f)
			{
				preferences.has_geometry = true;
				preferences.window_position = current_position;
				preferences.window_size = current_size;
				gui::aegis::mark_preferences_dirty();
			}
		}
		ImGui::End();
		ImGui::PopStyleVar(2);
	}

	void render_reference_vertical_module(modules::module* module)
	{
		if (!module) return;
		ImGui::PushID(module->get_id());
		const float width = ImGui::GetContentRegionAvail().x;
		constexpr float row_height = 56.0f;
		const ImVec2 minimum = ImGui::GetCursorScreenPos();
		const bool has_options = module_has_options(module);
		const float settings_lane = has_options ? 31.0f : 0.0f;
		ImGui::InvisibleButton("##classic_module", ImVec2(width - settings_lane, row_height));
		const bool hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) module->enabled = !module->enabled;
		bool settings_hovered = false;
		if (has_options)
		{
			ImGui::SetCursorScreenPos(ImVec2(minimum.x + width - settings_lane, minimum.y));
			ImGui::InvisibleButton("##classic_settings", ImVec2(settings_lane, row_height));
			settings_hovered = ImGui::IsItemHovered();
			bool settings_clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
			if (ImGui::IsMouseHoveringRect(minimum, ImVec2(minimum.x + width, minimum.y + row_height), true) &&
				ImGui::IsMouseClicked(ImGuiMouseButton_Right)) settings_clicked = true;
			if (settings_clicked)
			{
				const bool open = !module->display_options;
				for (modules::module* other : modules::get_modules())
					if (other && other != module &&
						std::strcmp(other->get_category(), module->get_category()) == 0)
						other->display_options = false;
				module->display_options = open;
			}
		}
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(0);
		if (module->enabled || hovered || settings_hovered || module->display_options)
			draw_list->AddRectFilled(minimum, ImVec2(minimum.x + width, minimum.y + row_height),
				module->enabled ? color_with_alpha(accent, 0.20f) : IM_COL32(255, 255, 255, 8), 7.0f);
		ImFont* normal_font = ImGui::GetFont();
		ImFont* bold_font = gui::ui_bold_font ? gui::ui_bold_font : normal_font;
		const float text_width = std::max(12.0f, width - settings_lane - 21.0f);
		const std::string title = ellipsize_text(bold_font, 15.5f, display_name(module->get_name()).c_str(), text_width);
		const ImVec4 title_clip(minimum.x + 11.0f, minimum.y + 5.0f,
			minimum.x + width - settings_lane - 5.0f, minimum.y + 28.0f);
		draw_list->AddText(bold_font, 15.5f,
			ImVec2(minimum.x + 11.0f, minimum.y + 8.0f),
			module->enabled ? IM_COL32(250, 250, 252, 255) : IM_COL32(207, 210, 218, 255),
			title.c_str(), nullptr, 0.0f, &title_clip);
		const std::string description = ellipsize_text(normal_font, 13.0f,
			module_description(module->get_name()), text_width);
		const ImVec4 description_clip(minimum.x + 11.0f, minimum.y + 29.0f,
			minimum.x + width - settings_lane - 5.0f, minimum.y + 50.0f);
		draw_list->AddText(normal_font, 13.0f, ImVec2(minimum.x + 11.0f, minimum.y + 32.0f),
			IM_COL32(132, 136, 148, 255), description.c_str(), nullptr, 0.0f, &description_clip);
		if (has_options)
		{
			const ImVec2 center(minimum.x + width - 15.5f, minimum.y + row_height * 0.5f);
			const ImU32 icon_color = module->display_options || settings_hovered
				? color_with_alpha(accent) : IM_COL32(137, 141, 153, 255);
			const float direction = module->display_options ? -1.0f : 1.0f;
			draw_list->AddLine(ImVec2(center.x - 4.0f, center.y - direction * 2.0f),
				ImVec2(center.x, center.y + direction * 2.0f), icon_color, 1.7f);
			draw_list->AddLine(ImVec2(center.x, center.y + direction * 2.0f),
				ImVec2(center.x + 4.0f, center.y - direction * 2.0f), icon_color, 1.7f);
		}
		const auto& preferences = gui::aegis::get_preferences();
		const float target = module->display_options ? 1.0f : 0.0f;
		module->display_options_progress = preferences.reduce_motion ? target
			: smooth_value(module->display_options_progress, target, 22.0f);
		if (std::abs(module->display_options_progress - target) < 0.002f)
			module->display_options_progress = target;
		const float full_options_height = vertical_options_full_height(module);
		const float options_height = full_options_height * module->display_options_progress;
		ImGui::SetCursorScreenPos(ImVec2(minimum.x, minimum.y + row_height + 2.0f));
		if (has_options && options_height > 0.5f)
		{
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(0.04f, module->display_options_progress));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 9.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 5.0f));
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.063f, 0.081f, 0.96f));
			const auto measured = measured_option_heights.find(module->get_id());
			const float wanted_height = (measured != measured_option_heights.end()
				? measured->second : module->get_options_height()) + 18.0f;
			const bool content_is_long = wanted_height > full_options_height + 1.0f;
			if (ImGui::BeginChild("##classic_options", ImVec2(width, options_height), ImGuiChildFlags_None,
				content_is_long ? ImGuiWindowFlags_AlwaysVerticalScrollbar : ImGuiWindowFlags_None))
			{
				const float content_start_y = ImGui::GetCursorPosY();
				const ImVec2 bind = ImGui::GetCursorScreenPos();
				const float bind_width = ImGui::GetContentRegionAvail().x;
				const bool show_module_bind = std::strcmp(module->get_name(), "click gui") != 0;
				if (show_module_bind)
				{
					ImGui::GetWindowDrawList()->AddRectFilled(bind,
						ImVec2(bind.x + bind_width, bind.y + 32.0f),
						IM_COL32(255, 255, 255, 6), 7.0f);
					ImGui::SetCursorScreenPos(ImVec2(bind.x + 9.0f, bind.y + 6.0f));
					ImGui::TextColored(ImVec4(0.55f, 0.57f, 0.63f, 1.0f), "%s", gui::text("Keybind", "Tuş"));
					ImGui::SetCursorScreenPos(ImVec2(bind.x + bind_width - 72.0f, bind.y + 3.0f));
					module->key_bind_selector();
				}
				ImGui::SetCursorScreenPos(ImVec2(bind.x, bind.y + (show_module_bind ? 33.0f : 0.0f)));
				ImGui::PushItemWidth(-1.0f);
				module->render_options();
				ImGui::PopItemWidth();
				ImGui::Dummy(ImVec2(0.0f, 8.0f));
				const float observed = std::max(52.0f, ImGui::GetCursorPosY() - content_start_y);
				float& stored = measured_option_heights[module->get_id()];
				if (stored <= 0.0f || std::abs(stored - observed) > 1.0f) stored = observed;
			}
			ImGui::EndChild();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(4);
			ImGui::SetCursorScreenPos(ImVec2(minimum.x, minimum.y + row_height + 6.0f + options_height));
		}
		else
		{
			ImGui::SetCursorScreenPos(ImVec2(minimum.x, minimum.y + row_height + 2.0f));
		}
		ImGui::PopID();
	}

	void render_reference_vertical(const ImGuiViewport* viewport)
	{
		constexpr int category_count = IM_ARRAYSIZE(aegis_categories);
		const float gap = 8.0f;
		const float fitted_width = (viewport->Size.x - 20.0f - gap * (category_count - 1)) / category_count;
		const float column_width = std::clamp(fitted_width, 205.0f, 246.0f);
		const float total_width = column_width * category_count + gap * (category_count - 1);
		const float start_x = viewport->Pos.x + std::max(8.0f, (viewport->Size.x - total_width) * 0.5f);
		for (int category = 0; category < IM_ARRAYSIZE(aegis_categories); ++category)
		{
			float content_height = 42.0f;
			for (modules::module* module : modules::get_modules())
				if (module && std::strcmp(module->get_category(), aegis_categories[category]) == 0)
					content_height += 58.0f + (module_has_options(module)
						? vertical_options_full_height(module) * module->display_options_progress : 0.0f);
			const float height = std::clamp(content_height + 10.0f, 120.0f, viewport->Size.y - 100.0f);
			ImGui::SetNextWindowPos(ImVec2(start_x + category * (column_width + gap), viewport->Pos.y + 74.0f),
				ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowSize(ImVec2(column_width, height), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.86f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(7.0f, 7.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 9.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
			ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.0f);
			ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.075f, 0.086f, 0.11f, 0.86f));
			ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.11f));
			const std::string window_id = std::string("##ThedoriusColumn_") + aegis_categories[category];
			if (ImGui::Begin(window_id.c_str(), nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
				ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
			{
				const ImVec2 window_position = ImGui::GetWindowPos();
				const ImVec2 header = ImGui::GetCursorScreenPos();
				ImGui::InvisibleButton("##column_drag", ImVec2(column_width - 14.0f, 32.0f));
				if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
					ImGui::SetWindowPos(ImVec2(window_position.x + ImGui::GetIO().MouseDelta.x,
						window_position.y + ImGui::GetIO().MouseDelta.y));
				ImDrawList* draw_list = ImGui::GetWindowDrawList();
				draw_list->AddText(gui::ui_bold_font, 18.0f, ImVec2(header.x + 4.0f, header.y + 4.0f),
					IM_COL32(242, 243, 248, 255), display_category(aegis_categories[category]));
				draw_sidebar_icon(draw_list, aegis_category_icons[category],
					ImVec2(header.x + column_width - 29.0f, header.y + 15.0f), IM_COL32(206, 210, 220, 255));
				draw_list->AddLine(ImVec2(header.x, header.y + 34.0f),
					ImVec2(header.x + column_width - 14.0f, header.y + 34.0f), IM_COL32(255, 255, 255, 18));
				ImGui::SetCursorScreenPos(ImVec2(header.x, header.y + 38.0f));
				for (modules::module* module : modules::get_modules())
					if (module && std::strcmp(module->get_category(), aegis_categories[category]) == 0)
						render_reference_vertical_module(module);
			}
			ImGui::End();
			ImGui::PopStyleColor(2);
			ImGui::PopStyleVar(4);
		}

		ImGui::SetNextWindowPos(ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + 18.0f),
			ImGuiCond_Always, ImVec2(0.5f, 0.0f));
		ImGui::SetNextWindowSize(ImVec2(170.0f, 44.0f), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.94f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 12.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(7.0f, 7.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.045f, 0.052f, 0.068f, 0.94f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.09f));
		if (ImGui::Begin("##ThedoriusColumnToolbar", nullptr, ImGuiWindowFlags_NoTitleBar |
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			const ImVec2 bar = ImGui::GetWindowPos();
			if (layout_button("column_modules", ImVec2(bar.x + 8.0f, bar.y + 5.0f), true,
				aegis_runtime.page == aegis_page::modules)) aegis_runtime.page = aegis_page::modules;
			if (aegis_icon_button("column_friends", sidebar_icon::friends, ImVec2(bar.x + 48.0f, bar.y + 5.0f),
				aegis_runtime.page == aegis_page::friends, gui::text("Friends", "Arkadaşlar"))) aegis_runtime.page = aegis_page::friends;
			if (aegis_icon_button("column_configs", sidebar_icon::configs, ImVec2(bar.x + 88.0f, bar.y + 5.0f),
				aegis_runtime.page == aegis_page::configs, "Configs")) aegis_runtime.page = aegis_page::configs;
			if (aegis_icon_button("column_settings", sidebar_icon::settings, ImVec2(bar.x + 128.0f, bar.y + 5.0f),
				aegis_runtime.page == aegis_page::settings, gui::text("Themes", "Temalar"))) aegis_runtime.page = aegis_page::settings;
		}
		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(3);
	}

	void render_vertical_utility(const ImGuiViewport* viewport)
	{
		static aegis_page displayed_page = aegis_page::friends;
		static float animation = 0.0f;
		const bool requested = aegis_runtime.page != aegis_page::modules;
		if (requested && displayed_page != aegis_runtime.page)
		{
			displayed_page = aegis_runtime.page;
			animation = 0.0f;
		}
		const float target = requested ? 1.0f : 0.0f;
		if (requested && ImGui::IsKeyPressed(ImGuiKey_Escape))
			aegis_runtime.page = aegis_page::modules;
		animation = gui::aegis::get_preferences().reduce_motion
			? target : smooth_value(animation, target, 22.0f);
		if (!requested && animation < 0.002f)
		{
			animation = 0.0f;
			return;
		}
		const float eased = animation * animation * (3.0f - 2.0f * animation);

		ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
		ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.0f);
		if (ImGui::Begin("##ThedoriusWorkspaceBackdrop", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground))
		{
			ImGui::GetWindowDrawList()->AddRectFilled(viewport->Pos,
				ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y),
				IM_COL32(3, 4, 7, static_cast<int>(31.0f * eased)));
			ImGui::SetCursorScreenPos(viewport->Pos);
			ImGui::InvisibleButton("##close_workspace_backdrop", viewport->Size);
			if (requested && ImGui::IsItemClicked()) aegis_runtime.page = aegis_page::modules;
		}
		ImGui::End();

		const bool theme_page = displayed_page == aegis_page::settings;
		const float desired_width = theme_page ? 610.0f : 780.0f;
		const float desired_height = theme_page ? 310.0f : 500.0f;
		const float minimum_width = theme_page ? 520.0f : 620.0f;
		const float minimum_height = theme_page ? 300.0f : 420.0f;
		const float width = std::clamp(desired_width, minimum_width, std::max(minimum_width, viewport->Size.x - 56.0f));
		const float height = std::clamp(desired_height, minimum_height, std::max(minimum_height, viewport->Size.y - 72.0f));
		const char* id = displayed_page == aegis_page::friends ? "##ThedoriusVerticalFriends"
			: (displayed_page == aegis_page::configs ? "##ThedoriusVerticalConfigs" : "##ThedoriusVerticalThemes");
		const float scale = 0.975f + eased * 0.025f;
		ImGui::SetNextWindowPos(
			ImVec2(viewport->Pos.x + viewport->Size.x * 0.5f, viewport->Pos.y + viewport->Size.y * 0.5f),
			ImGuiCond_Always, ImVec2(0.5f, 0.5f));
		ImGui::SetNextWindowSize(ImVec2(width * scale, height * scale), ImGuiCond_Always);
		ImGui::SetNextWindowBgAlpha(0.96f * eased);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 14.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 14.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 5.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(0.02f, eased));
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.043f, 0.049f, 0.063f, 0.96f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 1.0f, 1.0f, 0.08f));
		if (ImGui::Begin(id, nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
			ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
		{
			const ImVec2 window_position = ImGui::GetWindowPos();
			const ImVec2 window_size = ImGui::GetWindowSize();
			ImGui::SetCursorScreenPos(ImVec2(window_position.x + 16.0f, window_position.y + 16.0f));
			if (ImGui::BeginChild("##vertical_utility_content", ImVec2(window_size.x - 32.0f, window_size.y - 32.0f),
				ImGuiChildFlags_None))
			{
				if (displayed_page == aegis_page::friends) friends::render_manager();
				else if (displayed_page == aegis_page::configs) render_configs_page();
				else render_theme_page();
			}
			ImGui::EndChild();
			gui::render_workspace_toast(window_position,
				ImVec2(window_position.x + window_size.x, window_position.y + window_size.y));
		}
		ImGui::End();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(5);
	}

} // Closing anonymous namespace

void gui::render_click_gui_options()
{
	ImFont* heading_font = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
	ImGui::GetWindowDrawList()->AddText(
		heading_font, 16.0f, ImGui::GetCursorScreenPos(), IM_COL32(226, 228, 236, 255),
		gui::text("Language", "Dil"));
	ImGui::Dummy(ImVec2(0.0f, 27.0f));
	settings_language_row();
	ImGui::Dummy(ImVec2(0.0f, 10.0f));
	settings_menu_key_row();
}

void gui::request_panic_animation()
{
	begin_panic_animation();
}

void gui::settings_stepper(
	const char* label,
	float* value,
	float minimum,
	float maximum,
	float step,
	const char* format)
{
	ImGui::PushID(label);

	const float row_x = ImGui::GetCursorPosX();
	const float row_y = ImGui::GetCursorPosY();
	const float available_width = ImGui::GetContentRegionAvail().x;
	ImGui::SetCursorPosY(row_y + 5.0f);
	ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.76f, 1.0f), "%s", label);

	const float controls_width = 142.0f;
	const float right_edge = row_x + available_width;
	ImGui::SetCursorPos(ImVec2(right_edge - controls_width, row_y));

	if (ImGui::Button("-", ImVec2(28.0f, 28.0f)))
		*value = std::clamp(*value - step, minimum, maximum);

	ImGui::SameLine(0.0f, 4.0f);
	ImGui::SetNextItemWidth(78.0f);
	ImGui::DragFloat("##value", value, step * 0.10f, minimum, maximum, format, ImGuiSliderFlags_AlwaysClamp);

	ImGui::SameLine(0.0f, 4.0f);
	if (ImGui::Button("+", ImVec2(28.0f, 28.0f)))
		*value = std::clamp(*value + step, minimum, maximum);

	ImGui::SetCursorPosY(row_y + 32.0f);
	ImGui::PopID();
}

void gui::settings_coordinate_row(float* x_value, float* y_value)
{
	const float row_x = ImGui::GetCursorPosX();
	const float row_y = ImGui::GetCursorPosY();
	const float available_width = ImGui::GetContentRegionAvail().x;

	ImGui::SetCursorPosY(row_y + 5.0f);
	ImGui::TextColored(ImVec4(0.72f, 0.72f, 0.76f, 1.0f), "Position");

	const float controls_width = 184.0f;
	const float right_edge = row_x + available_width;
	ImGui::SetCursorPos(ImVec2(right_edge - controls_width, row_y + 1.0f));

	ImGui::TextColored(ImVec4(0.48f, 0.48f, 0.53f, 1.0f), "X");
	ImGui::SameLine(0.0f, 5.0f);
	ImGui::SetNextItemWidth(66.0f);
	ImGui::DragFloat("##position_x", x_value, 0.5f, 0.0f, 1000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);

	ImGui::SameLine(0.0f, 10.0f);
	ImGui::TextColored(ImVec4(0.48f, 0.48f, 0.53f, 1.0f), "Y");
	ImGui::SameLine(0.0f, 5.0f);
	ImGui::SetNextItemWidth(66.0f);
	ImGui::DragFloat("##position_y", y_value, 0.5f, 0.0f, 1000.0f, "%.0f", ImGuiSliderFlags_AlwaysClamp);

	ImGui::SetCursorPosY(row_y + 32.0f);
}

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
static LRESULT CALLBACK detour_WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HCURSOR arrow_cursor = LoadCursorW(nullptr, IDC_ARROW);
	static bool physical_left_down = false;
	static bool physical_right_down = false;
	static bool prev_gui_draw = false;

	auto release_common_keys = [&](HWND window_handle)
	{
		static const int keys_to_reset[] = {
			'W', 'A', 'S', 'D',
			VK_SPACE, VK_SHIFT, VK_CONTROL, VK_MENU,
			VK_LBUTTON, VK_RBUTTON, VK_MBUTTON
		};

		for (int vk : keys_to_reset)
		{
			if ((GetAsyncKeyState(vk) & 0x8000) == 0)
			{
				CallWindowProcA(original_WndProc, window_handle, WM_KEYUP, static_cast<WPARAM>(vk), 0xC0000001);
			}
		}
	};

	if (prev_gui_draw != gui::draw)
	{
		prev_gui_draw = gui::draw;
		release_common_keys(hWnd);
	}

	auto update_physical_button = [&](bool left, bool pressed)
	{
		bool& previous = left ? physical_left_down : physical_right_down;
		if (previous == pressed)
			return;
		previous = pressed;
		if (left)
		{
			modules::left_clicker::notify_manual_click(pressed);
			if (pressed)
			{
				// Shield Breaker has priority when the crosshair is on a blocking
				// player. The slot swap happens before this input reaches GLFW, so
				// Minecraft processes the original physical click with the axe.
				if (!modules::shield_breaker::gate_left_click())
					modules::mace_swap::gate_left_click();
			}
		}
		else
		{
			modules::right_clicker::notify_manual_click(pressed);
			modules::fast_exp::notify_manual_right_click(pressed);
		}
	};

	// GLFW commonly receives hardware mouse buttons through raw input. Reading
	// WM_INPUT here keeps manual clicks separate from our tagged SendInput pulses.
	if (!gui::draw && msg == WM_INPUT)
	{
		RAWINPUT raw{};
		UINT raw_size = sizeof(raw);
		if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT,
			&raw, &raw_size, sizeof(RAWINPUTHEADER)) == sizeof(raw) &&
			raw.header.dwType == RIM_TYPEMOUSE &&
			raw.data.mouse.ulExtraInformation !=
				static_cast<ULONG>(modules::synthetic_mouse_tag))
		{
			const USHORT flags = raw.data.mouse.usButtonFlags;
			if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) update_physical_button(true, true);
			if (flags & RI_MOUSE_LEFT_BUTTON_UP) update_physical_button(true, false);
			if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) update_physical_button(false, true);
			if (flags & RI_MOUSE_RIGHT_BUTTON_UP) update_physical_button(false, false);
		}
	}

	if (msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN)
		last_pressed_key = (int)wParam;
	else if (msg == WM_MBUTTONDOWN)
		last_pressed_key = VK_MBUTTON;
	else if (msg == WM_XBUTTONDOWN)
		last_pressed_key = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;

	if (gui::draw)
	{
		// Force standard OS cursor pos mapping to client coords
		POINT mouse_pos;
		if (GetCursorPos(&mouse_pos))
		{
			ScreenToClient(hWnd, &mouse_pos);
			ImGui::GetIO().AddMousePosEvent((float)mouse_pos.x, (float)mouse_pos.y);
		}

		// Read mouse buttons at OS hardware level since GLFW blocks click messages from being posted
		ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
		ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);

		// Pass key release events through to original WndProc so GLFW and Minecraft register key releases while UI is open
		if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
		{
			CallWindowProcA(original_WndProc, hWnd, msg, wParam, lParam);
		}

		// Don't let backend process default hover/move messages since GLFW lacks focus
		if (msg == WM_MOUSEMOVE || msg == WM_NCMOUSEMOVE || msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP || msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP)
		{
			SetCursor(arrow_cursor);
			return 0;
		}

		ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
		SetCursor(arrow_cursor);
		return 0;
	}

	// Legacy mouse messages remain as a fallback. The transition guard above
	// prevents a raw-input + legacy-input pair from firing the modules twice.
	if ((msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) &&
		static_cast<std::uintptr_t>(GetMessageExtraInfo()) !=
		modules::synthetic_mouse_tag)
	{
		update_physical_button(true, msg == WM_LBUTTONDOWN);
	}
	if ((msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) &&
		static_cast<std::uintptr_t>(GetMessageExtraInfo()) !=
		modules::synthetic_mouse_tag)
	{
		update_physical_button(false, msg == WM_RBUTTONDOWN);
	}

	return CallWindowProcA(original_WndProc, hWnd, msg, wParam, lParam);
}

static void uninit(HWND hWnd, HDC device)
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext(imGuiContext);
	SetWindowLongPtrA(hWnd, GWLP_WNDPROC, (LONG_PTR)original_WndProc);
	wglMakeCurrent(device, original_context);
	wglDeleteContext(new_context);
}

static BOOL WINAPI detour_wglSwapBuffers(HDC device)
{
	HWND current_window = WindowFromDC(device);


	if (request_shutdown.load(std::memory_order_acquire))
	{
		request_shutdown.store(false, std::memory_order_release);
		MH_DisableHook(wglSwapBuffers);
		uninit(current_window, device);

		// Finish the current swap through MinHook's trampoline before telling
		// the worker thread that the render side is safe to remove. Signalling
		// before this call allowed the DLL to unload while this detour was active.
		const BOOL result = original_wglSwapBuffers
			? original_wglSwapBuffers(device)
			: TRUE;
		shut_down.store(true, std::memory_order_release);
		return result;
	}

	HGLRC last_context = wglGetCurrentContext();

	// if already init, and window changed, cleanup and reinit
	if (!run_once && window != current_window)
	{
		uninit(current_window, device);
		run_once = true;
	}

	if (run_once)
	{
		window = current_window;
		if (!window) {
			loader_ui::fail("Minecraft render window was not available");
		}
		else
		{
			// Injection happens from another process, so explicitly return the
			// user to Minecraft before the first loader frame is presented.
			focus_minecraft_window(window);
		}

		original_context = last_context;
		new_context = wglCreateContext(device);
		if (last_context && new_context)
		{
			wglShareLists(last_context, new_context);
		}
		wglMakeCurrent(device, new_context);

		original_WndProc = (WNDPROC)SetWindowLongPtrA(window, GWLP_WNDPROC, (LONG_PTR)detour_WndProc);
		if (!original_WndProc) {
			loader_ui::fail("Minecraft input window hook failed");
		}

		imGuiContext = ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		// Do NOT set NavEnableKeyboard – it intercepts every key including INSERT
		io.IniFilename = nullptr;
		io.LogFilename = nullptr;

		ImFontGlyphRangesBuilder language_range_builder;
		language_range_builder.AddRanges(io.Fonts->GetGlyphRangesDefault());
		language_range_builder.AddText("ÇĞİÖŞÜçğıöşü");
		language_range_builder.BuildRanges(&language_glyph_ranges);
		ImFontConfig ui_font_config{};
		ui_font_config.OversampleH = 3;
		ui_font_config.OversampleV = 2;
		ui_font_config.PixelSnapH = false;
		ui_font_config.RasterizerMultiply = 1.07f;
		ui_font_config.FontDataOwnedByAtlas = false;
		if (read_resource_bytes(IDR_FONT_GOOGLE_SANS_SEMIBOLD, ui_font_data))
			gui::ui_font = io.Fonts->AddFontFromMemoryTTF(
				ui_font_data.data(), static_cast<int>(ui_font_data.size()), 18.0f,
				&ui_font_config, language_glyph_ranges.Data);
		if (!gui::ui_font) {
			gui::ui_font = io.Fonts->AddFontDefault();
		}
		ImFontConfig bold_font_config = ui_font_config;
		bold_font_config.RasterizerMultiply = 1.10f;
		if (read_resource_bytes(IDR_FONT_GOOGLE_SANS_BOLD, ui_bold_font_data))
			gui::ui_bold_font = io.Fonts->AddFontFromMemoryTTF(
				ui_bold_font_data.data(), static_cast<int>(ui_bold_font_data.size()), 21.0f,
				&bold_font_config, language_glyph_ranges.Data);
		if (!gui::ui_bold_font)
			gui::ui_bold_font = gui::ui_font;
		io.FontDefault = gui::ui_font;

		ImFontConfig icon_font_config{};
		icon_font_config.OversampleH = 3;
		icon_font_config.OversampleV = 2;
		icon_font_config.PixelSnapH = true;
		icon_font_config.RasterizerMultiply = 1.08f;
		icon_font_config.FontDataOwnedByAtlas = false;
		ImFontGlyphRangesBuilder icon_range_builder;
		icon_range_builder.AddChar(0xF05B);
		icon_range_builder.AddChar(0xF121);
		icon_range_builder.AddChar(0xE163);
		icon_range_builder.AddChar(0xF06E);
		icon_range_builder.AddChar(0xF0C0);
		icon_range_builder.AddChar(0xF07C);
		icon_range_builder.AddChar(0xF1DE);
		icon_range_builder.AddChar(0xF019);
		icon_range_builder.AddChar(0xF0C7);
		icon_range_builder.AddChar(0xF1F8);
		icon_range_builder.AddChar(0xF00D);
		icon_range_builder.AddChar(0xF021);
		icon_range_builder.AddChar(0xF234);
		icon_range_builder.AddChar(0xF503);
		icon_range_builder.AddChar(0x002B);
		icon_range_builder.BuildRanges(&icon_glyph_ranges);
		if (read_resource_bytes(IDR_FONT_FONTAWESOME_SOLID, icon_font_data))
			gui::icon_font = io.Fonts->AddFontFromMemoryTTF(
				icon_font_data.data(), static_cast<int>(icon_font_data.size()), 20.0f,
				&icon_font_config, icon_glyph_ranges.Data);

		ImFontConfig arraylist_font_config = ui_font_config;
		gui::arraylist_font = ui_font_data.empty() ? nullptr : io.Fonts->AddFontFromMemoryTTF(
			ui_font_data.data(), static_cast<int>(ui_font_data.size()), 16.5f,
			&arraylist_font_config, language_glyph_ranges.Data);
		if (!gui::arraylist_font) {
			gui::arraylist_font = gui::ui_font;
		}
		
		// Timewarp-inspired dark card theme.
		{
			auto& style = ImGui::GetStyle();
			ImVec4* colors = style.Colors;

			style.WindowRounding = 18.0f;
			style.FrameRounding = 8.0f;
			style.GrabRounding = 8.0f;
			style.PopupRounding = 10.0f;
			style.ChildRounding = 13.0f;
			style.ScrollbarRounding = 8.0f;
			style.FramePadding = ImVec2(10, 7);
			style.WindowPadding = ImVec2(14, 14);
			style.ItemSpacing = ImVec2(10, 9);
			style.ItemInnerSpacing = ImVec2(8, 8);
			style.WindowBorderSize = 1.0f;
			style.ChildBorderSize = 1.0f;
			style.PopupBorderSize = 1.0f;
			style.FrameBorderSize = 0.0f;

			// Deep charcoal palette from the reference layout.
			colors[ImGuiCol_Text]                   = ImVec4(0.92f, 0.92f, 0.95f, 1.00f);
			colors[ImGuiCol_TextDisabled]           = ImVec4(0.43f, 0.43f, 0.47f, 1.00f);
			colors[ImGuiCol_WindowBg]               = ImVec4(0.05f, 0.05f, 0.06f, 0.98f);
			colors[ImGuiCol_ChildBg]                = ImVec4(0.105f, 0.105f, 0.118f, 1.00f);
			colors[ImGuiCol_PopupBg]                = ImVec4(0.075f, 0.075f, 0.085f, 0.99f);
			colors[ImGuiCol_Border]                 = ImVec4(1.00f, 1.00f, 1.00f, 0.055f);
			colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
			
			colors[ImGuiCol_FrameBg]                = ImVec4(0.13f, 0.13f, 0.145f, 1.00f);
			colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.17f, 0.17f, 0.19f, 1.00f);
			colors[ImGuiCol_FrameBgActive]          = ImVec4(0.20f, 0.20f, 0.22f, 1.00f);
			
			colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
			colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
			colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
			
			colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.08f, 0.08f, 0.09f, 0.00f);
			colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.22f, 0.22f, 0.27f, 1.00f);
			colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.28f, 0.27f, 0.33f, 1.00f);
			colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.34f, 0.33f, 0.40f, 1.00f);
			
			// Rose accent used by the selected navigation pill.
			colors[ImGuiCol_CheckMark]              = ImVec4(0.94f, 0.31f, 0.50f, 1.00f);
			colors[ImGuiCol_SliderGrab]             = ImVec4(0.94f, 0.31f, 0.50f, 1.00f);
			colors[ImGuiCol_SliderGrabActive]       = ImVec4(1.00f, 0.43f, 0.60f, 1.00f);
			
			colors[ImGuiCol_Button]                 = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
			colors[ImGuiCol_ButtonHovered]          = ImVec4(0.66f, 0.36f, 0.96f, 0.85f);
			colors[ImGuiCol_ButtonActive]           = ImVec4(0.66f, 0.36f, 0.96f, 1.00f);
			
			colors[ImGuiCol_Header]                 = ImVec4(0.18f, 0.17f, 0.22f, 1.00f);
			colors[ImGuiCol_HeaderHovered]          = ImVec4(0.66f, 0.36f, 0.96f, 0.40f);
			colors[ImGuiCol_HeaderActive]           = ImVec4(0.66f, 0.36f, 0.96f, 0.80f);
			
			colors[ImGuiCol_Separator]              = ImVec4(0.18f, 0.17f, 0.22f, 1.00f);
			colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.66f, 0.36f, 0.96f, 0.80f);
			colors[ImGuiCol_SeparatorActive]        = ImVec4(0.66f, 0.36f, 0.96f, 1.00f);
			
			colors[ImGuiCol_Tab]                    = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
			colors[ImGuiCol_TabHovered]             = ImVec4(0.66f, 0.36f, 0.96f, 0.80f);
			colors[ImGuiCol_TabActive]              = ImVec4(0.16f, 0.15f, 0.20f, 1.00f);
			colors[ImGuiCol_TabUnfocused]           = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
			colors[ImGuiCol_TabUnfocusedActive]     = ImVec4(0.16f, 0.15f, 0.20f, 1.00f);
		}

		ImGui_ImplOpenGL3_Init();
		ImGui_ImplWin32_Init(window);
		run_once = false;
	}

	wglMakeCurrent(device, new_context);

	// Poll the configurable menu key every frame. GLFW does not reliably expose
	// every Windows virtual key through the hooked WndProc path.
	{
		static bool previous_menu_key = false;
		const bool current_menu_key = gui::menu_key > 0 &&
			(GetAsyncKeyState(gui::menu_key) & 0x8000) != 0;
		if (gui::suppress_menu_key_until_release)
		{
			if (!current_menu_key)
				gui::suppress_menu_key_until_release = false;
		}
		else if (current_menu_key && !previous_menu_key && !gui::menu_key_scanning &&
			!panic_animation_active && !gui::loader_visible)
		{
			gui::draw = !gui::draw;
			ClipCursor(NULL);
			if (gui::draw)
			{
				// Initialise virtual cursor at client centre
				RECT client{};
				GetClientRect(window, &client);
				virt_mouse_x = (float)(client.right  / 2);
				virt_mouse_y = (float)(client.bottom / 2);
				POINT pt{ (LONG)virt_mouse_x, (LONG)virt_mouse_y };
				ClientToScreen(window, &pt);
				SetCursorPos(pt.x, pt.y);
			}
		}
		previous_menu_key = current_menu_key;
	}

	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	platform::client_dispatcher::drain();


	// Update dynamic accent colors in style
	{
		auto& style = ImGui::GetStyle();
		ImColor accent = gui::get_dynamic_color();
		ImColor accent_hover = gui::get_dynamic_color(100);
		ImColor accent_active = gui::get_dynamic_color(200);

		style.Colors[ImGuiCol_CheckMark]        = accent;
		style.Colors[ImGuiCol_SliderGrab]       = accent;
		style.Colors[ImGuiCol_SliderGrabActive] = accent_active;
		style.Colors[ImGuiCol_ButtonHovered]    = ImVec4(accent.Value.x, accent.Value.y, accent.Value.z, 0.85f);
		style.Colors[ImGuiCol_ButtonActive]     = accent_active;
		style.Colors[ImGuiCol_HeaderHovered]    = ImVec4(accent.Value.x, accent.Value.y, accent.Value.z, 0.40f);
		style.Colors[ImGuiCol_HeaderActive]     = ImVec4(accent.Value.x, accent.Value.y, accent.Value.z, 0.80f);
		style.Colors[ImGuiCol_TabHovered]       = ImVec4(accent.Value.x, accent.Value.y, accent.Value.z, 0.80f);
	}

	render_loader_overlay();

	// Render HUD and ArrayList overlays
	if (!gui::loader_visible)
	{
		for (modules::module* module : modules::get_modules())
		{
			if (module->enabled)
				module->render_overlay();
		}
	}

	if (gui::draw && !gui::loader_visible)
	{
		const ImGuiViewport* reference_viewport = ImGui::GetMainViewport();
		auto& reference_preferences = gui::aegis::get_preferences();
		if (reference_preferences.layout != gui::aegis::layout_mode::vertical_columns)
		{
			reference_preferences.layout = gui::aegis::layout_mode::vertical_columns;
			gui::aegis::mark_preferences_dirty();
		}
		ImDrawList* background_draw_list = ImGui::GetBackgroundDrawList();
		if (background_draw_list)
		{
			background_draw_list->AddRectFilled(
				ImVec2(0.0f, 0.0f),
				ImGui::GetIO().DisplaySize,
				reference_preferences.layout == gui::aegis::layout_mode::vertical_columns
					? IM_COL32(5, 6, 9, 28)
					: IM_COL32(5, 6, 9, 96));
		}

		render_reference_vertical(reference_viewport);
		render_vertical_utility(reference_viewport);
		gui::aegis::flush_preferences();


		// Panic is rendered on the foreground viewport rather than inside the
		// menu window, so the regular UI disappears completely underneath it.
		if (panic_animation_active)
		{
			const ImGuiViewport* panic_viewport = ImGui::GetMainViewport();
			render_panic_animation(
				ImGui::GetForegroundDrawList(),
				panic_viewport->Pos,
				panic_viewport->Size,
				0.0f);
		}
	}
	ImGui::EndFrame();
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	if (last_context)
	{
		wglMakeCurrent(device, last_context);
	}
	else if (original_context)
	{
		wglMakeCurrent(device, original_context);
	}
	return original_wglSwapBuffers(device);
}

bool gui::init()
{
	HMODULE opengl = GetModuleHandleA("opengl32.dll");
	if (!opengl) 
	{
		loader_ui::fail("OpenGL runtime was not loaded");
		return false;
	}
	wglSwapBuffers = (wglSwapBuffers_t)GetProcAddress(opengl, "wglSwapBuffers");

	if (!wglSwapBuffers)
	{
		loader_ui::fail("wglSwapBuffers export was not found");
		return false;
	}

	if (MH_Initialize() != MH_OK) 
	{
		loader_ui::fail("Native hook engine initialization failed");
		return false;
	}

	if (MH_CreateHook(wglSwapBuffers, detour_wglSwapBuffers, (void**)&original_wglSwapBuffers) != MH_OK) 
	{
		loader_ui::fail("OpenGL hook creation failed");
		return false;
	}

	if (MH_EnableHook(wglSwapBuffers) != MH_OK) 
	{
		loader_ui::fail("OpenGL hook activation failed");
		return false;
	}

	return true;
}

ImTextureID gui::get_icon_texture(icon_asset asset)
{
	const size_t index = static_cast<size_t>(asset);
	if (index >= icon_textures.size())
		return nullptr;

	if (!icon_texture_attempted[index])
	{
		icon_texture_attempted[index] = true;
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		std::vector<unsigned char> encoded_icon;
		if (read_resource_bytes(icon_resource(asset), encoded_icon))
			load_png_texture(encoded_icon.data(), encoded_icon.size(), icon_textures[index]);
	}

	return reinterpret_cast<ImTextureID>(static_cast<intptr_t>(icon_textures[index]));
}

void gui::shutdown()
{
	gui::aegis::flush_preferences(true);
	request_shutdown.store(true, std::memory_order_release);
	while (!shut_down.load(std::memory_order_acquire))
		std::this_thread::sleep_for(std::chrono::milliseconds(1));

	// Give the detour epilogue time to return completely before MinHook data
	// and the DLL image can be released by the worker thread.
	std::this_thread::sleep_for(std::chrono::milliseconds(50));
	MH_RemoveHook(wglSwapBuffers);
	MH_Uninitialize();
}

int gui::get_last_pressed_key()
{
	return last_pressed_key;
}

void gui::reset_last_pressed_key()
{
	last_pressed_key = 0;
}

HWND gui::get_window()
{
	return window;
}

ImColor gui::get_dynamic_color(int offset)
{
	struct palette
	{
		ImVec4 stops[3];
	};
	static const palette palettes[] =
	{
		{ ImVec4(0.40f, 0.31f, 0.94f, 1.0f), ImVec4(0.59f, 0.43f, 1.00f, 1.0f), ImVec4(0.35f, 0.48f, 0.98f, 1.0f) },
		{ ImVec4(0.86f, 0.18f, 0.30f, 1.0f), ImVec4(1.00f, 0.34f, 0.46f, 1.0f), ImVec4(0.91f, 0.25f, 0.58f, 1.0f) },
		{ ImVec4(0.12f, 0.66f, 0.40f, 1.0f), ImVec4(0.28f, 0.89f, 0.61f, 1.0f), ImVec4(0.18f, 0.73f, 0.68f, 1.0f) },
		{ ImVec4(0.12f, 0.45f, 0.91f, 1.0f), ImVec4(0.27f, 0.68f, 1.00f, 1.0f), ImVec4(0.24f, 0.55f, 0.95f, 1.0f) },
		{ ImVec4(0.86f, 0.27f, 0.56f, 1.0f), ImVec4(1.00f, 0.48f, 0.72f, 1.0f), ImVec4(0.91f, 0.33f, 0.75f, 1.0f) },
		{ ImVec4(0.03f, 0.66f, 0.73f, 1.0f), ImVec4(0.10f, 0.88f, 0.87f, 1.0f), ImVec4(0.13f, 0.72f, 0.94f, 1.0f) },
		{ ImVec4(0.94f, 0.39f, 0.13f, 1.0f), ImVec4(1.00f, 0.66f, 0.24f, 1.0f), ImVec4(0.96f, 0.46f, 0.29f, 1.0f) },
		{ ImVec4(0.62f, 0.64f, 0.70f, 1.0f), ImVec4(0.88f, 0.89f, 0.94f, 1.0f), ImVec4(0.72f, 0.75f, 0.83f, 1.0f) }
	};

	const ULONGLONG tick = GetTickCount64();
	const float time = static_cast<float>(tick) / 1000.0f;
	static int source_theme = std::clamp(active_theme, 0, 7);
	static int target_theme = source_theme;
	static ULONGLONG transition_started = tick;
	const int requested_theme = std::clamp(active_theme, 0, 7);
	if (requested_theme != target_theme)
	{
		source_theme = target_theme;
		target_theme = requested_theme;
		transition_started = tick;
	}

	auto sample_palette = [&](int theme_index)
	{
		// A slow three-stop loop avoids the flat two-color sine pulse used by
		// the previous system. Offset adds a stable spatial phase for lists.
		float phase = std::fmod(time * 0.055f + static_cast<float>(offset) * 0.0028f, 1.0f);
		if (phase < 0.0f) phase += 1.0f;
		const float scaled = phase * 3.0f;
		const int first = static_cast<int>(std::floor(scaled)) % 3;
		const int second = (first + 1) % 3;
		float local = scaled - std::floor(scaled);
		local = local * local * (3.0f - 2.0f * local);
		const ImVec4& a = palettes[theme_index].stops[first];
		const ImVec4& b = palettes[theme_index].stops[second];
		return ImVec4(a.x + (b.x - a.x) * local,
			a.y + (b.y - a.y) * local,
			a.z + (b.z - a.z) * local, 1.0f);
	};

	float transition = std::clamp(static_cast<float>(tick - transition_started) / 520.0f, 0.0f, 1.0f);
	// Smootherstep gives the palette change zero velocity at both ends.
	transition = transition * transition * transition * (transition * (transition * 6.0f - 15.0f) + 10.0f);
	const ImVec4 from = sample_palette(source_theme);
	const ImVec4 to = sample_palette(target_theme);
	return ImColor(from.x + (to.x - from.x) * transition,
		from.y + (to.y - from.y) * transition,
		from.z + (to.z - from.z) * transition, 1.0f);
}
