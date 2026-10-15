#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "gui.hpp"
#include "ui_state.hpp"
#include "theme_palette.hpp"
#include "surface_style.hpp"
#include "control_draw.hpp"
#include "../ui/grim_ui.hpp"
#include "../ui/hud_editor.hpp"
#include "../ui/config_page.hpp"
#include <Windows.h>
#include <gl/GL.h>
#include <wincodec.h>
#include "../imgui/imgui_impl_opengl3.h"
#include "../imgui/imgui_impl_win32.h"
#include "../modules/core/module_registry.hpp"
#include "../modules/core/public_config.hpp"
#include "../modules/combat/mace_swap.hpp"
#include "../modules/combat/shield_breaker.hpp"
#include "../modules/combat/jump_reset.hpp"
#include "../modules/misc/fast_exp.hpp"
#include "../modules/misc/left_clicker.hpp"
#include "../modules/misc/right_clicker.hpp"
#include "../friends/friends.hpp"
#include "../configs/configs.hpp"
#include "../loader/loader_ui.hpp"
#include "../platform/jni/client_dispatcher.hpp"
#include "../services/input/input_service.hpp"
#include "../services/game/attack_service.hpp"
#include "../utils/version_manager.hpp"
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
	modules::module* selected_settings_module = nullptr;
	ImVector<ImWchar> language_glyph_ranges{};
	ImVector<ImWchar> icon_glyph_ranges{};
	std::vector<unsigned char> ui_font_data{};
	std::vector<unsigned char> ui_bold_font_data{};
	std::vector<unsigned char> icon_font_data{};
	std::array<GLuint, 3> icon_textures{};
	std::array<bool, 3> icon_texture_attempted{};

	int icon_resource(gui::icon_asset asset)
	{
		switch (asset)
		{
		case gui::icon_asset::helmet: return IDR_ICON_HELMET;
		case gui::icon_asset::background: return IDR_BACKGROUND_MAIN;
		case gui::icon_asset::spotify: return IDR_ICON_SPOTIFY;
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

#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_LINEAR_MIPMAP_LINEAR
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#endif

		glGenTextures(1, &texture);
		glBindTexture(GL_TEXTURE_2D, texture);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
		glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
		glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
			0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

		typedef void (APIENTRY* PFNGLGENERATEMIPMAPPROC)(GLenum target);
		static PFNGLGENERATEMIPMAPPROC pfn_glGenerateMipmap = nullptr;
		static bool pfn_resolved = false;
		if (!pfn_resolved)
		{
			pfn_glGenerateMipmap = reinterpret_cast<PFNGLGENERATEMIPMAPPROC>(wglGetProcAddress("glGenerateMipmap"));
			pfn_resolved = true;
		}

		if (pfn_glGenerateMipmap)
		{
			pfn_glGenerateMipmap(GL_TEXTURE_2D);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else
		{
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}

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
				0);
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

		const char* runtime_label = mc::get_runtime_label();
		const ImVec2 runtime_size = ImGui::CalcTextSize(runtime_label);
		draw_list->AddText(
			ImVec2(card_max.x - runtime_size.x - 26.0f, card_min.y + 29.0f),
			IM_COL32(105, 107, 118, static_cast<int>(225.0f * alpha)), runtime_label);

		const ImVec2 emblem_center(card_min.x + card_width * 0.5f, card_min.y + 119.0f);
		const float rotation = elapsed * 3.4f;
		draw_list->AddCircleFilled(emblem_center, 41.0f + ambient_pulse * 2.0f,
			color_with_alpha(accent, alpha * (0.055f + ambient_pulse * 0.02f)), 0);
		draw_list->AddCircle(emblem_center, 31.0f,
			IM_COL32(255, 255, 255, static_cast<int>(18.0f * alpha)), 0, 1.0f);

		if (loader_state.failed)
		{
			draw_list->AddCircle(emblem_center, 26.0f, color_with_alpha(accent, alpha), 0, 2.5f);
			draw_list->AddLine(ImVec2(emblem_center.x - 8.0f, emblem_center.y - 8.0f),
				ImVec2(emblem_center.x + 8.0f, emblem_center.y + 8.0f), color_with_alpha(accent, alpha), 2.5f);
			draw_list->AddLine(ImVec2(emblem_center.x + 8.0f, emblem_center.y - 8.0f),
				ImVec2(emblem_center.x - 8.0f, emblem_center.y + 8.0f), color_with_alpha(accent, alpha), 2.5f);
		}
		else if (displayed_progress >= 0.985f)
		{
			draw_list->AddCircle(emblem_center, 26.0f, color_with_alpha(accent, alpha), 0, 2.5f);
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
				reached ? color_with_alpha(accent, alpha) : IM_COL32(65, 66, 76, static_cast<int>(255.0f * alpha)), 0);
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
			{ "click gui", "Controls the client language", "Client dilini ayarlar" },
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
			{ "shield breaker", "Switches to an axe against blocking players", "Kalkan kullanan oyunculara karşı baltaya geçer" },
			{ "auto totem", "Moves available totems to protected slots", "Mevcut totemleri korumalı slotlara taşır" },
			{ "legit auto totem", "Refills totems with human-like inventory input", "Totemleri doğal envanter hareketleriyle doldurur" },
			{ "watermark", "Shows the Thedorius Lite status overlay", "Thedorius Lite durum göstergesini ekrana çizer" },
			{ "arraylist", "Shows enabled modules on the game overlay", "Etkin modülleri oyun ekranında gösterir" },
			{ "glow esp", "Highlights visible players with a glow effect", "Görünür oyuncuları parlama efektiyle vurgular" },
			{ "chams", "Colored player model overlay", "Oyuncu modeli renkli kaplamasi" },
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
		draw_list->AddCircleFilled(center, 7.0f, color, 0);
		draw_list->AddCircleFilled(center, 3.2f, IM_COL32(18, 18, 20, 255), 0);
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
		{
			*value = !*value;
		}

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

		// Snap the knob travel to whole pixels so the two rounded caps stay on the
		// same raster grid as the track and the edges do not stair-step mid-slide.
		const float travel = std::max(0.0f, width - height);
		const float knob_center = position.x + radius + std::floor(animation * travel + 0.5f);
		const float knob_y = std::floor(position.y + radius + 0.5f);
		const float knob_radius = radius - 3.0f;

		const ImVec2 track_min(std::floor(position.x), std::floor(position.y));
		const ImVec2 track_max(std::ceil(position.x + width), std::ceil(position.y + height));

		draw_list->AddRectFilled(track_min, track_max, track_color, radius);
		draw_list->AddRect(track_min, track_max, IM_COL32(255, 255, 255, hovered ? 28 : 14),
			radius, 0, 1.0f);
		// num_segments <= 0 lets ImGui pick the segment count, keeping the cap smooth.
		draw_list->AddCircleFilled(ImVec2(knob_center, knob_y), knob_radius,
			IM_COL32(210, 212, 218, 255), 0);
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

		draw_list->AddCircle(center, 5.0f, color, 0, 1.7f);
		draw_list->AddCircleFilled(center, 1.7f, color, 0);
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
		{
			selected_settings_module = module;
		}

		const ImVec2 gear_min(card_max.x - 96.0f, card_min.y + 22.0f);
		bool gear_hovered = false;
		if (has_options)
		{
			ImGui::SetCursorScreenPos(gear_min);
			ImGui::InvisibleButton("##module_settings", ImVec2(28.0f, 28.0f));
			gear_hovered = ImGui::IsItemHovered();
			if (ImGui::IsItemClicked())
			{
				selected_settings_module = module;
			}
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
		if (clicked && !selected)
		{
			gui::active_theme = index;
			gui::aegis::mark_preferences_dirty();
		}

		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImU32 border = selected
			? color_with_alpha(theme.color, 0.88f)
			: IM_COL32(255, 255, 255, hovered ? 22 : 9);
		const ImU32 background = hovered ? IM_COL32(25, 28, 35, 255) : IM_COL32(18, 21, 27, 255);

		draw_list->AddRectFilled(position, ImVec2(position.x + size.x, position.y + size.y), background, 10.0f);
		draw_list->AddRect(position, ImVec2(position.x + size.x, position.y + size.y), border, 10.0f, 0, selected ? 1.6f : 1.0f);
		ImFont* bold = gui::ui_bold_font ? gui::ui_bold_font : ImGui::GetFont();
		draw_list->AddText(bold, 14.0f,
			ImVec2(position.x + 14.0f, position.y + size.y - 28.0f),
			IM_COL32(232, 234, 241, 255), theme.name);
		if (selected)
		{
			const ImVec2 check(position.x + size.x - 22.0f, position.y + size.y - 21.0f);
			draw_list->AddCircleFilled(check, 8.0f, color_with_alpha(theme.color, 0.96f), 0);
			draw_list->AddLine(ImVec2(check.x - 3.0f, check.y), ImVec2(check.x - 0.5f, check.y + 2.5f),
				IM_COL32(250, 250, 252, 255), 1.5f);
			draw_list->AddLine(ImVec2(check.x - 0.5f, check.y + 2.5f), ImVec2(check.x + 4.0f, check.y - 3.0f),
				IM_COL32(250, 250, 252, 255), 1.5f);
		}

		const auto preview_color = [index](int stop, float alpha)
		{
			const auto c = gui::theme::palettes[index].stops[stop];
			return ImGui::ColorConvertFloat4ToU32(ImVec4(c.r, c.g, c.b, alpha));
		};

		// A miniature workspace previews all three palette stops without
		// changing the live theme on hover or adding explanatory paragraphs.
		const ImVec2 mini(position.x + 12.0f, position.y + 12.0f);
		const ImVec2 mini_max(position.x + size.x - 12.0f, position.y + size.y - 43.0f);
		const float side_width = (mini_max.x - mini.x) * 0.25f;
		draw_list->AddRectFilled(mini, mini_max, IM_COL32(9, 10, 15, 255), 7.0f);
		draw_list->AddRectFilled(mini, ImVec2(mini.x + side_width, mini_max.y),
			preview_color(0, 0.09f), 7.0f, ImDrawFlags_RoundCornersLeft);
		draw_list->AddLine(ImVec2(mini.x + side_width, mini.y + 7.0f),
			ImVec2(mini.x + side_width, mini_max.y - 7.0f), IM_COL32(255, 255, 255, 12));
		for (int row = 0; row < 4; ++row)
		{
			const ImVec2 nav(mini.x + 6.0f, mini.y + 12.0f + row * 17.0f);
			draw_list->AddRectFilled(nav, ImVec2(mini.x + side_width - 6.0f, nav.y + 7.0f),
				row == 1 ? preview_color(0, 0.9f) : IM_COL32(255, 255, 255, 23), 2.0f);
		}
		const float content_x = mini.x + side_width + 9.0f;
		draw_list->AddRectFilled(ImVec2(content_x, mini.y + 10.0f),
			ImVec2(content_x + (mini_max.x - content_x) * 0.46f, mini.y + 14.0f), IM_COL32(225, 225, 235, 155), 2.0f);
		for (int row = 0; row < 3; ++row)
		{
			const ImVec2 tile(content_x, mini.y + 23.0f + row * 22.0f);
			draw_list->AddRectFilled(tile, ImVec2(mini_max.x - 8.0f, tile.y + 17.0f),
				preview_color(row, 0.09f), 4.0f);
			draw_list->AddRectFilled(ImVec2(tile.x + 5.0f, tile.y + 6.0f),
				ImVec2(tile.x + (mini_max.x - tile.x) * 0.5f, tile.y + 10.0f), preview_color(row, 0.95f), 2.0f);
			draw_list->AddRectFilled(ImVec2(mini_max.x - 28.0f, tile.y + 5.0f),
				ImVec2(mini_max.x - 13.0f, tile.y + 12.0f), preview_color(row, 0.82f), 3.5f);
		}
		draw_list->AddRect(mini, mini_max, IM_COL32(255, 255, 255, 12), 7.0f);

		ImGui::PopID();
		return clicked;
	}

	void settings_theme_grid()
	{
		std::array<theme_preview, gui::theme::palettes.size()> themes{};
		for (std::size_t index = 0; index < themes.size(); ++index)
		{
			const auto c = gui::theme::sample(static_cast<int>(index), 0.0);
			themes[index] = { gui::theme::palettes[index].name, ImColor(c.r, c.g, c.b) };
		}

		const ImVec2 start = ImGui::GetCursorScreenPos();
		const float available_width = ImGui::GetContentRegionAvail().x;
		const float gap = 12.0f;
		const int columns = std::clamp(static_cast<int>((available_width + gap) / 212.0f), 1, 4);
		const ImVec2 card_size(std::max(1.0f, (available_width - gap * (columns - 1)) / columns), 150.0f);
		for (int index = 0; index < static_cast<int>(themes.size()); ++index)
		{
			const int column = index % columns;
			const int row = index / columns;
			theme_card(themes[index], index, ImVec2(start.x + column * (card_size.x + gap), start.y + row * (card_size.y + gap)), card_size);
		}
		const int rows = (static_cast<int>(themes.size()) + columns - 1) / columns;
		ImGui::SetCursorScreenPos(ImVec2(start.x, start.y + card_size.y * rows + gap * (rows - 1)));
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
		{
			gui::active_language = language;
			gui::aegis::mark_preferences_dirty();
		}

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
			draw_list->AddCircleFilled(moon, 7.0f, IM_COL32(255, 255, 255, 255), 0);
			draw_list->AddCircleFilled(ImVec2(moon.x + 3.0f, moon.y), 5.8f, IM_COL32(220, 32, 45, 255), 0);
			draw_list->AddCircleFilled(ImVec2(flag_min.x + 29.0f, flag_min.y + 13.0f), 2.2f, IM_COL32(255, 255, 255, 255), 5);
		}

		const char* title = language == 0 ? "English" : "Türkçe";
		draw_list->AddText(ImVec2(position.x + 65.0f, position.y + 18.0f), IM_COL32(225, 225, 232, 255), title);
		const ImVec2 selection(position.x + size.x - 18.0f, position.y + size.y * 0.5f);
		draw_list->AddCircle(selection, 5.5f, selected ? color_with_alpha(accent, 0.95f) : IM_COL32(89, 90, 101, 255), 0, 1.2f);
		if (selected)
			draw_list->AddCircleFilled(selection, 2.6f, color_with_alpha(accent, 0.95f), 0);
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
			if (ImGui::IsItemClicked())
			{
				gui::active_language = language;
				gui::aegis::mark_preferences_dirty();
			}
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

	void render_configs_page(bool embedded = false)
	{
		gui::config_page::render(embedded);
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
		bool search_open = false;
		float search_animation = 0.0f;
		int palette_selection = 0;
		float layout_transition = 1.0f;
		gui::aegis::layout_mode previous_layout = gui::aegis::layout_mode::vertical_columns;
	};

	aegis_runtime_state aegis_runtime{};
	std::unordered_map<std::string, float> measured_option_heights;

	constexpr const char* aegis_categories[] = { "Combat", "Movement", "Player", "World", "Render", "Visual", "Client", "Vape" };
	constexpr sidebar_icon aegis_category_icons[] = {
		sidebar_icon::combat, sidebar_icon::misc, sidebar_icon::friends,
		sidebar_icon::configs, sidebar_icon::render, sidebar_icon::visual, sidebar_icon::settings, sidebar_icon::combat
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
		if (std::strstr(name, "totem")) return "offhand hotbar health delay inventory cursor slot";
		if (std::strstr(name, "anchor")) return "delay charge jitter slot input movement air";
		if (std::strstr(name, "clicker")) return "cps jitter random delay drop spike input";
		if (std::strstr(name, "esp")) return "color box outline fill distance blocks storage";
		if (std::strcmp(name, "watermark") == 0 || std::strcmp(name, "arraylist") == 0)
			return "scale position opacity bar glow shadow keybind";
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

	const char* category_description(const char* category)
	{
		if (!category) return "";
		if (std::strcmp(category, "Combat") == 0) return gui::text("Combat assistance and timing", "Savaş yardımı ve zamanlama");
		if (std::strcmp(category, "Movement") == 0) return gui::text("Movement and bridging tools", "Hareket ve köprü araçları");
		if (std::strcmp(category, "Player") == 0) return gui::text("Inventory and player automation", "Envanter ve oyuncu otomasyonu");
		if (std::strcmp(category, "World") == 0) return gui::text("World interaction utilities", "Dünya etkileşim araçları");
		if (std::strcmp(category, "Render") == 0) return gui::text("HUD and screen overlays", "HUD ve ekran katmanları");
		if (std::strcmp(category, "Visual") == 0) return gui::text("World visualization modules", "Dünya görselleştirme modülleri");
		if (std::strcmp(category, "Client") == 0) return gui::text("Client interface and safety", "Client arayüzü ve güvenlik");
		return "";
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
				draw_list->AddCircleFilled(points[node], 1.7f, color_with_alpha(accent, 0.18f), 0);
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
					module->enabled ? color_with_alpha(gui::get_dynamic_color(0)) : IM_COL32(77, 81, 92, 255), 0);
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
		if (ImGui::IsItemClicked() && has_options)
		{
			module->display_options = !module->display_options;
		}
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
		if (ImGui::Button(gui::text("Panic##aegis", "Panik##aegis"), ImVec2(67.0f, 30.0f))) gui::request_panic();
		ImGui::PopStyleColor(3);

		render_command_palette(window_position, window_size, search_x, search_width);
	}

	void render_reference_horizontal_card(modules::module* module, float width)
	{
		if (!module || width < 190.0f) return;
		ImGui::PushID(module->get_id());
		const ImVec2 minimum = ImGui::GetCursorScreenPos();
		const ImVec2 maximum(minimum.x + width, minimum.y + 48.0f);
		ImGui::InvisibleButton("##reference_card", ImVec2(width, 48.0f));
		const bool hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked())
		{
			aegis_runtime.horizontal_module = module_has_options(module) ? module : nullptr;
		}
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(0);
		const bool selected = aegis_runtime.horizontal_module == module;
		const ImU32 background = selected
			? color_with_alpha(accent, 0.18f)
			: (hovered ? IM_COL32(34, 31, 40, 242) : IM_COL32(25, 23, 30, 230));
		draw_list->AddRectFilled(minimum, maximum, background, 8.0f);
		draw_list->AddRect(minimum, maximum,
			selected ? color_with_alpha(accent, 0.45f) : IM_COL32(255, 255, 255, 7), 8.0f);
		if (hovered || selected)
		{
			const float glow_width = width * (selected ? 0.72f : 0.42f);
			draw_list->AddRectFilledMultiColor(minimum, ImVec2(minimum.x + glow_width, maximum.y),
				color_with_alpha(accent, selected ? 0.10f : 0.055f), IM_COL32(0, 0, 0, 0),
				IM_COL32(0, 0, 0, 0), color_with_alpha(accent, selected ? 0.10f : 0.055f));
		}
		const std::string title = display_name(module->get_name());
		const ImVec4 title_clip(minimum.x + 12.0f, minimum.y + 5.0f, maximum.x - 12.0f, minimum.y + 25.0f);
		draw_list->AddText(gui::ui_bold_font, 14.5f, ImVec2(minimum.x + 12.0f, minimum.y + 6.0f),
			module->enabled ? color_with_alpha(accent) : IM_COL32(237, 238, 243, 255),
			title.c_str(), nullptr, 0.0f, &title_clip);
		const ImVec4 description_clip(minimum.x + 12.0f, minimum.y + 26.0f, maximum.x - 12.0f, maximum.y - 4.0f);
		draw_list->AddText(ImGui::GetFont(), 12.0f, ImVec2(minimum.x + 12.0f, minimum.y + 27.0f),
			IM_COL32(126, 128, 137, 255), module_description(module->get_name()), nullptr, 0.0f, &description_clip);
		ImGui::SetCursorScreenPos(ImVec2(minimum.x, maximum.y + 8.0f));
		ImGui::PopID();
	}

	void render_reference_inspector_animated(modules::module* requested_module, const ImVec2& minimum, const ImVec2& size)
	{
		static modules::module* displayed_module = nullptr;
		static float animation = 0.0f;
		if (requested_module && module_has_options(requested_module) && displayed_module != requested_module)
		{
			displayed_module = requested_module;
			animation = 0.0f;
		}
		const float target = requested_module && module_has_options(requested_module) ? 1.0f : 0.0f;
		animation = gui::aegis::get_preferences().reduce_motion
			? target : smooth_value(animation, target, 17.0f);
		if (std::abs(animation - target) < 0.003f) animation = target;
		if (!displayed_module || (target == 0.0f && animation <= 0.003f))
		{
			if (target == 0.0f) displayed_module = nullptr;
			return;
		}

		modules::module* module = displayed_module;
		const float eased = animation * animation * (3.0f - 2.0f * animation);
		const float visible_height = 54.0f + (size.y - 54.0f) * eased;
		const ImVec2 maximum(minimum.x + size.x, minimum.y + visible_height);
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(0);
		draw_list->PushClipRect(minimum, maximum, true);
		draw_list->AddRectFilled(minimum, maximum,
			IM_COL32(22, 17, 29, static_cast<int>(244.0f * eased)), 15.0f);
		draw_list->AddRect(minimum, maximum, color_with_alpha(accent, 0.18f * eased), 15.0f);

		ImGui::PushID(module->get_id());
		const std::string title = display_name(module->get_name());
		draw_list->AddText(gui::ui_bold_font, 18.0f, ImVec2(minimum.x + 15.0f, minimum.y + 14.0f),
			IM_COL32(244, 242, 247, 255), title.c_str());
		ImGui::SetCursorScreenPos(ImVec2(minimum.x + size.x - 34.0f, minimum.y + 10.0f));
		if (ImGui::InvisibleButton("##close_reference_inspector_animated", ImVec2(24.0f, 24.0f)))
			aegis_runtime.horizontal_module = nullptr;
		const ImVec2 close_center(minimum.x + size.x - 22.0f, minimum.y + 22.0f);
		draw_list->AddCircleFilled(close_center, 12.0f, IM_COL32(255, 255, 255, 9));
		gui::control_draw::close_icon(draw_list, close_center, IM_COL32(183, 180, 190, 255));
		draw_list->AddLine(ImVec2(minimum.x + 14.0f, minimum.y + 50.0f),
			ImVec2(minimum.x + size.x - 14.0f, minimum.y + 50.0f), IM_COL32(255, 255, 255, 13));

		draw_list->AddText(ImVec2(minimum.x + 14.0f, minimum.y + 65.0f),
			IM_COL32(214, 211, 220, 255), gui::text("Enabled", "Etkin"));
		toggle_switch("##reference_enabled_animated", &module->enabled,
			ImVec2(minimum.x + size.x - 49.0f, minimum.y + 59.0f), 35.0f, 21.0f);
		draw_list->AddText(ImVec2(minimum.x + 14.0f, minimum.y + 96.0f),
			IM_COL32(160, 157, 169, 255), gui::text("Keybind", "Tuş"));
		ImGui::SetCursorScreenPos(ImVec2(minimum.x + size.x - 88.0f, minimum.y + 88.0f));
		module->key_bind_selector();

		if (visible_height > 142.0f)
		{
			ImGui::SetCursorScreenPos(ImVec2(minimum.x + 10.0f, minimum.y + 126.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));
			ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarSize, 4.0f);
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.075f, 0.077f, 0.09f, 0.0f));
			if (ImGui::BeginChild("##reference_inspector_options_animated",
				ImVec2(size.x - 20.0f, visible_height - 136.0f)))
				module->render_options();
			ImGui::EndChild();
			ImGui::PopStyleColor();
			ImGui::PopStyleVar(2);
		}
		ImGui::PopID();
		draw_list->PopClipRect();
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
		ImGui::InvisibleButton("##sidebar_entry", ImVec2(176.0f, 35.0f));
		const bool hovered = ImGui::IsItemHovered();
		if (ImGui::IsItemClicked())
		{
			aegis_runtime.page = page;
			if (category >= 0) aegis_runtime.active_category = category;
			if (page != aegis_page::modules) aegis_runtime.horizontal_module = nullptr;
		}
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const ImColor accent = gui::get_dynamic_color(0);
		if (selected || hovered)
			draw_list->AddRectFilled(position, ImVec2(position.x + 176.0f, position.y + 35.0f),
				selected ? color_with_alpha(accent, 0.82f) : IM_COL32(255, 255, 255, 8), 7.0f);
		const ImU32 color = selected ? IM_COL32(250, 250, 252, 255) : IM_COL32(177, 179, 187, 255);
		draw_sidebar_icon(draw_list, icon, ImVec2(position.x + 17.0f, position.y + 17.5f), color);
		draw_list->AddText(gui::ui_bold_font, 14.0f, ImVec2(position.x + 36.0f, position.y + 9.0f), color, label);
		ImGui::PopID();
	}

	void render_reference_search_popover(const ImVec2& content_min, float content_width)
	{
		const float target = aegis_runtime.search_open ? 1.0f : 0.0f;
		aegis_runtime.search_animation = gui::aegis::get_preferences().reduce_motion
			? target : smooth_value(aegis_runtime.search_animation, target, 18.0f);
		if (std::abs(aegis_runtime.search_animation - target) < 0.003f)
			aegis_runtime.search_animation = target;
		if (!aegis_runtime.search_open && aegis_runtime.search_animation <= 0.003f) return;
		if (aegis_runtime.search_open && ImGui::IsKeyPressed(ImGuiKey_Escape))
			aegis_runtime.search_open = false;

		std::vector<modules::module*> results;
		for (modules::module* candidate : modules::get_modules())
			if (candidate && std::strcmp(candidate->get_category(), "Friends") != 0 &&
				module_matches(candidate, aegis_runtime.search)) results.push_back(candidate);
		if (results.size() > 6) results.resize(6);

		const float eased = aegis_runtime.search_animation * aegis_runtime.search_animation *
			(3.0f - 2.0f * aegis_runtime.search_animation);
		const float width = std::min(390.0f, content_width - 28.0f);
		const float full_height = 54.0f + (aegis_runtime.search[0] ? results.size() * 39.0f + 8.0f : 0.0f);
		const float height = 44.0f + (full_height - 44.0f) * eased;
		const ImVec2 position(content_min.x + std::max(0.0f, (content_width - width) * 0.5f), content_min.y - 7.0f);

		ImGui::SetCursorScreenPos(position);
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, std::max(0.03f, eased));
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(9.0f, 8.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 12.0f);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.045f, 0.070f, 0.98f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.60f, 0.27f, 1.0f, 0.24f));
		if (ImGui::BeginChild("##reference_search_popover", ImVec2(width, height), ImGuiChildFlags_Border,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			if (aegis_runtime.focus_search)
			{
				ImGui::SetKeyboardFocusHere();
				aegis_runtime.focus_search = false;
			}
			ImGui::SetNextItemWidth(-1.0f);
			ImGui::InputTextWithHint("##reference_search_input", gui::text("Search modules", "Modül ara"),
				aegis_runtime.search, IM_ARRAYSIZE(aegis_runtime.search));
			if (aegis_runtime.search[0] && eased > 0.60f)
			{
				for (modules::module* result : results)
				{
					ImGui::PushID(result->get_id());
					const ImVec2 row_min = ImGui::GetCursorScreenPos();
					ImGui::InvisibleButton("##search_result", ImVec2(ImGui::GetContentRegionAvail().x, 34.0f));
					const bool hovered = ImGui::IsItemHovered();
					if (hovered)
						ImGui::GetWindowDrawList()->AddRectFilled(row_min,
							ImVec2(row_min.x + ImGui::GetContentRegionAvail().x, row_min.y + 34.0f),
							IM_COL32(255, 255, 255, 9), 6.0f);
					ImGui::GetWindowDrawList()->AddText(gui::ui_bold_font, 13.5f,
						ImVec2(row_min.x + 8.0f, row_min.y + 4.0f), IM_COL32(238, 236, 242, 255),
						display_name(result->get_name()).c_str());
					ImGui::GetWindowDrawList()->AddText(ImVec2(row_min.x + 8.0f, row_min.y + 19.0f),
						IM_COL32(129, 124, 137, 255), display_category(result->get_category()));
					if (ImGui::IsItemClicked())
					{
						aegis_runtime.page = aegis_page::modules;
						aegis_runtime.active_category = category_index(result->get_category());
						aegis_runtime.horizontal_module = module_has_options(result) ? result : nullptr;
						aegis_runtime.search_open = false;
						aegis_runtime.search[0] = '\0';
					}
					ImGui::PopID();
				}
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor(2);
		ImGui::PopStyleVar(3);
	}

	void render_reference_horizontal(const ImGuiViewport* viewport)
	{
		auto& preferences = gui::aegis::get_preferences();
		static bool positioned = false;
		const ImVec2 desired(std::min(1280.0f, viewport->Size.x - 32.0f),
			std::min(620.0f, viewport->Size.y - 40.0f));
		ImGui::SetNextWindowSizeConstraints(ImVec2(980.0f, 520.0f),
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
		if (ImGui::Begin("##onestepHorizontalReference", nullptr,
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoSavedSettings))
		{
			const ImVec2 position = ImGui::GetWindowPos();
			const ImVec2 size = ImGui::GetWindowSize();
			const float gap = 14.0f;
			const float inspector_width = std::clamp(size.x * 0.24f, 270.0f, 304.0f);
			const float main_width = size.x - inspector_width - gap;
			ImDrawList* draw_list = ImGui::GetWindowDrawList();
			draw_list->AddRectFilled(position, ImVec2(position.x + main_width, position.y + size.y),
				IM_COL32(15, 16, 20, 239), 14.0f);
			draw_list->AddRect(position, ImVec2(position.x + main_width, position.y + size.y),
				IM_COL32(255, 255, 255, 16), 14.0f);
			const float sidebar_width = 204.0f;
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
			const float logo_font_size = 19.0f;
			const ImVec2 one_sz = gui::ui_bold_font->CalcTextSizeA(logo_font_size, FLT_MAX, 0.0f, "one");
			draw_list->AddText(gui::ui_bold_font, logo_font_size, ImVec2(position.x + 59.0f, position.y + 15.0f),
				IM_COL32(245, 245, 248, 255), "one");
			draw_list->AddText(gui::ui_bold_font, logo_font_size, ImVec2(position.x + 59.0f + one_sz.x, position.y + 15.0f),
				gui::get_dynamic_color(0), "step");
			draw_list->AddText(ImVec2(position.x + 60.0f, position.y + 36.0f), IM_COL32(116, 117, 126, 255), "v1.0");

			const float nav_x = position.x + 14.0f;
			draw_list->AddText(gui::ui_bold_font, 10.0f, ImVec2(nav_x + 4.0f, position.y + 72.0f),
				IM_COL32(111, 108, 119, 255), "FEATURES");
			float nav_y = position.y + 88.0f;
			for (int category = 0; category < IM_ARRAYSIZE(aegis_categories); ++category)
			{
				reference_sidebar_entry(aegis_categories[category], aegis_category_icons[category],
					display_category(aegis_categories[category]), ImVec2(nav_x, nav_y),
					aegis_runtime.page == aegis_page::modules && aegis_runtime.active_category == category,
					aegis_page::modules, category);
				nav_y += 41.0f;
			}
			nav_y += 4.0f;
			draw_list->AddText(gui::ui_bold_font, 10.0f, ImVec2(nav_x + 4.0f, nav_y),
				IM_COL32(111, 108, 119, 255), "OTHER");
			nav_y += 18.0f;
			reference_sidebar_entry("friends", sidebar_icon::friends, gui::text("Friends", "Arkadaşlar"),
				ImVec2(nav_x, nav_y), aegis_runtime.page == aegis_page::friends, aegis_page::friends); nav_y += 41.0f;
			reference_sidebar_entry("configs", sidebar_icon::configs, "Configs",
				ImVec2(nav_x, nav_y), aegis_runtime.page == aegis_page::configs, aegis_page::configs); nav_y += 41.0f;
			reference_sidebar_entry("settings", sidebar_icon::settings, gui::text("Settings", "Ayarlar"),
				ImVec2(nav_x, nav_y), aegis_runtime.page == aegis_page::settings, aegis_page::settings);

			const ImVec2 profile_min(position.x + 13.0f, position.y + size.y - 58.0f);
			draw_list->AddRectFilled(profile_min, ImVec2(position.x + sidebar_width - 13.0f, position.y + size.y - 12.0f),
				IM_COL32(255, 255, 255, 7), 9.0f);
			draw_sidebar_icon(draw_list, sidebar_icon::friends,
				ImVec2(profile_min.x + 18.0f, profile_min.y + 23.0f), IM_COL32(210, 207, 216, 255));
			std::string profile_name = friends::local_player_name();
			if (profile_name.empty()) profile_name = "Player";
			profile_name = ellipsize_text(gui::ui_bold_font, 13.0f, profile_name.c_str(), sidebar_width - 76.0f);
			draw_list->AddText(gui::ui_bold_font, 13.0f, ImVec2(profile_min.x + 38.0f, profile_min.y + 8.0f),
				IM_COL32(226, 224, 231, 255), profile_name.c_str());
			draw_list->AddText(ImVec2(profile_min.x + 39.0f, profile_min.y + 27.0f),
				IM_COL32(118, 115, 126, 255), "UID: session");

			const ImVec2 content_min(position.x + sidebar_width + 18.0f, position.y + 16.0f);
			const float content_width = main_width - sidebar_width - 34.0f;
			if (aegis_runtime.page == aegis_page::modules)
			{
				const int category = aegis_runtime.active_category;
				draw_sidebar_icon(draw_list, aegis_category_icons[category],
					ImVec2(content_min.x + 10.0f, content_min.y + 12.0f), color_with_alpha(gui::get_dynamic_color(0)));
				draw_list->AddText(gui::ui_bold_font, 17.0f, ImVec2(content_min.x + 27.0f, content_min.y + 2.0f),
					IM_COL32(239, 240, 245, 255), display_category(aegis_categories[category]));
				const float category_text_width = gui::ui_bold_font->CalcTextSizeA(17.0f, FLT_MAX, 0.0f,
					display_category(aegis_categories[category])).x;
				draw_list->AddCircleFilled(ImVec2(content_min.x + 36.0f + category_text_width, content_min.y + 12.0f),
					1.8f, IM_COL32(137, 131, 145, 255));
				draw_list->AddText(ImVec2(content_min.x + 45.0f + category_text_width, content_min.y + 5.0f),
					IM_COL32(128, 124, 136, 255), category_description(aegis_categories[category]));

				const ImVec2 search_button(content_min.x + content_width - 35.0f, content_min.y - 2.0f);
				ImGui::SetCursorScreenPos(search_button);
				ImGui::InvisibleButton("##reference_search_button", ImVec2(34.0f, 34.0f));
				const bool search_hovered = ImGui::IsItemHovered();
				if (ImGui::IsItemClicked())
				{
					aegis_runtime.search_open = !aegis_runtime.search_open;
					aegis_runtime.focus_search = aegis_runtime.search_open;
				}
				if (search_hovered || aegis_runtime.search_open)
					draw_list->AddRectFilled(search_button, ImVec2(search_button.x + 34.0f, search_button.y + 34.0f),
						IM_COL32(255, 255, 255, search_hovered ? 10 : 7), 8.0f);
				const ImVec2 search_center(search_button.x + 16.0f, search_button.y + 15.0f);
				draw_list->AddCircle(search_center, 5.5f, IM_COL32(171, 168, 179, 255), 0, 1.6f);
				draw_list->AddLine(ImVec2(search_center.x + 4.0f, search_center.y + 4.0f),
					ImVec2(search_center.x + 8.0f, search_center.y + 8.0f), IM_COL32(171, 168, 179, 255), 1.6f);
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
							(aegis_runtime.search_open && !module_matches(module, aegis_runtime.search))) continue;
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

			render_reference_search_popover(content_min, content_width);
			render_reference_inspector_animated(aegis_runtime.horizontal_module,
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
	// This is deliberately fixed and has no keybind-capture control in the UI.
	gui::menu_key = VK_RSHIFT;
	static const char* languages[] = { "English", "T\xC3\xBCrk\xC3\xA7" "e" };
	const int prev_lang = gui::active_language;
	gui::option_combo(gui::text("Language", "Dil"), &gui::active_language, languages, IM_ARRAYSIZE(languages));
	if (gui::active_language != prev_lang) gui::aegis::mark_preferences_dirty();

}

void gui::render_theme_selection()
{
	ImGui::PushID("theme_selection");
	settings_theme_grid();
	ImGui::PopID();
}

void gui::request_panic()
{
	for (modules::module* module : modules::get_modules())
		module->enabled = false;
	gui::draw = false;
	gui::should_unload = true;
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
		physical_left_down = physical_right_down = false;
		services::input::reset_physical_buttons();
		services::input::cancel_trigger_click();
		modules::left_clicker::notify_manual_click(false);
		modules::right_clicker::notify_manual_click(false);
		release_common_keys(hWnd);
	}

	auto update_physical_button = [&](bool left, bool pressed)
	{
		bool& previous = left ? physical_left_down : physical_right_down;
		if (previous == pressed)
			return;
		previous = pressed;
		services::input::observe_physical(left ? services::input::mouse_button::left : services::input::mouse_button::right, pressed);
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
	if (msg == WM_KILLFOCUS)
	{
		physical_left_down = physical_right_down = false;
		services::input::reset_physical_buttons();
		if (auto* jump = dynamic_cast<modules::jump_reset*>(modules::find_module_by_id("jump_reset")))
			jump->cancel_on_client_thread();
		services::input::cancel_trigger_click();
		modules::left_clicker::notify_manual_click(false);
		modules::right_clicker::notify_manual_click(false);
	}
	if (msg == WM_SETFOCUS)
		services::input::reset_physical_keys();
	if (msg == WM_INPUT)
	{
		RAWINPUT raw{};
		UINT raw_size = sizeof(raw);
		if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lParam), RID_INPUT,
			&raw, &raw_size, sizeof(RAWINPUTHEADER)) == sizeof(raw) &&
			raw.header.dwType == RIM_TYPEMOUSE)
		{
			const USHORT flags = raw.data.mouse.usButtonFlags;
			const auto tag = static_cast<std::uintptr_t>(raw.data.mouse.ulExtraInformation);
			if (services::input::restore_tag(tag))
			{
				if (services::input::accept_restore_event(tag))
					services::attack::restore_left_hold(!gui::draw && services::input::physical_button_down(services::input::mouse_button::left));
				return DefWindowProcA(hWnd, msg, wParam, lParam);
			}
			if (services::input::tracked_tag(tag))
			{
				bool allow = true;
				if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) allow &= services::input::observe_tracked_event(tag, true, services::input::event_path::raw);
				if (flags & RI_MOUSE_LEFT_BUTTON_UP) allow &= services::input::observe_tracked_event(tag, false, services::input::event_path::raw);
				if (!allow) return DefWindowProcA(hWnd, msg, wParam, lParam);
			}
			else if (!gui::draw && !services::input::is_synthetic_tag(tag))
			{
				if (flags & RI_MOUSE_LEFT_BUTTON_DOWN) update_physical_button(true, true);
				if (flags & RI_MOUSE_LEFT_BUTTON_UP) update_physical_button(true, false);
				if (flags & RI_MOUSE_RIGHT_BUTTON_DOWN) update_physical_button(false, true);
				if (flags & RI_MOUSE_RIGHT_BUTTON_UP) update_physical_button(false, false);
			}
		}
	}
	const auto mouse_tag = static_cast<std::uintptr_t>(GetMessageExtraInfo());
	const bool key_down = msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN;
	const bool key_up = msg == WM_KEYUP || msg == WM_SYSKEYUP;
	if ((key_down || key_up) && services::input::jump_key_tag(mouse_tag))
	{
		if (wParam != VK_SPACE || !services::input::observe_jump_space_event(mouse_tag, key_down)) return 0;
	}
	else if ((key_down || key_up) && !services::input::is_synthetic_tag(mouse_tag))
		services::input::observe_physical_key(static_cast<unsigned>(wParam), static_cast<unsigned>((lParam >> 16) & 0x1ff), key_down);
	if ((msg == WM_CHAR || msg == WM_SYSCHAR) && services::input::jump_key_tag(mouse_tag)) return 0;
	if (!services::input::is_synthetic_tag(mouse_tag))
	{
		if (msg == WM_MBUTTONDOWN || msg == WM_MBUTTONUP)
			services::input::observe_physical_key(VK_MBUTTON, 0, msg == WM_MBUTTONDOWN && !gui::draw);
		if (msg == WM_XBUTTONDOWN || msg == WM_XBUTTONUP)
			services::input::observe_physical_key(GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2,
				0, msg == WM_XBUTTONDOWN && !gui::draw);
	}
	if ((msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) && services::input::restore_tag(mouse_tag))
	{
		if (services::input::accept_restore_event(mouse_tag))
			services::attack::restore_left_hold(!gui::draw && services::input::physical_button_down(services::input::mouse_button::left));
		return 0; // State restoration must not enqueue another Minecraft attack.
	}
	if ((msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) && services::input::tracked_tag(mouse_tag) &&
		!services::input::observe_tracked_event(mouse_tag, msg == WM_LBUTTONDOWN, services::input::event_path::window)) return 0;

	if ((msg == WM_KEYDOWN || msg == WM_SYSKEYDOWN) && !services::input::is_synthetic_tag(mouse_tag))
		last_pressed_key = (int)wParam;
	else if (msg == WM_MBUTTONDOWN)
		last_pressed_key = VK_MBUTTON;
	else if (msg == WM_XBUTTONDOWN)
		last_pressed_key = GET_XBUTTON_WPARAM(wParam) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;

	if (gui::draw)
	{
		if (msg == WM_SETCURSOR)
		{
			SetCursor(arrow_cursor);
			return TRUE;
		}

		if (msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP)
		{
			modules::fast_exp::notify_manual_right_click(msg == WM_RBUTTONDOWN);
		}

		// Pass key release events through to original WndProc so GLFW and Minecraft register key releases while UI is open
		if (msg == WM_KEYUP || msg == WM_SYSKEYUP)
		{
			CallWindowProcA(original_WndProc, hWnd, msg, wParam, lParam);
		}

		ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam);
		SetCursor(arrow_cursor);
		return 0;
	}

	// Legacy mouse messages remain as a fallback. The transition guard above
	// prevents a raw-input + legacy-input pair from firing the modules twice.
	if ((msg == WM_LBUTTONDOWN || msg == WM_LBUTTONUP) &&
		!services::input::is_synthetic_tag(mouse_tag))
	{
		update_physical_button(true, msg == WM_LBUTTONDOWN);
	}
	if ((msg == WM_RBUTTONDOWN || msg == WM_RBUTTONUP) &&
		!services::input::is_synthetic_tag(mouse_tag))
	{
		update_physical_button(false, msg == WM_RBUTTONDOWN);
	}

	return CallWindowProcA(original_WndProc, hWnd, msg, wParam, lParam);
}

static void uninit(HWND hWnd, HDC device)
{
	// Release owned jump input while client-thread JNI and the input guard are
	// still available, before module destruction/unhooking on the worker.
	if (auto* jump = dynamic_cast<modules::jump_reset*>(modules::find_module_by_id("jump_reset")))
		jump->cancel_on_client_thread();
	// UI textures are owned by the injected OpenGL context. Release them while
	// that context is still valid so repeated window recreation and unloads do
	// not leak the embedded logo/background textures.
	if (new_context)
	{
		wglMakeCurrent(device, new_context);
		for (GLuint& texture : icon_textures)
		{
			if (texture)
			{
				glDeleteTextures(1, &texture);
				texture = 0;
			}
		}
		icon_texture_attempted.fill(false);
	}
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
		ui_font_config.OversampleV = 1;
		ui_font_config.PixelSnapH = false;
		ui_font_config.RasterizerMultiply = 1.0f;
		ui_font_config.FontDataOwnedByAtlas = false;
		if (read_resource_bytes(IDR_FONT_MONTSERRAT_SEMIBOLD, ui_font_data))
			gui::ui_font = io.Fonts->AddFontFromMemoryTTF(
				ui_font_data.data(), static_cast<int>(ui_font_data.size()), 19.5f,
				&ui_font_config, language_glyph_ranges.Data);
		if (!gui::ui_font) {
			gui::ui_font = io.Fonts->AddFontDefault();
		}
		ImFontConfig bold_font_config = ui_font_config;
		bold_font_config.RasterizerMultiply = 1.0f;
		if (read_resource_bytes(IDR_FONT_MONTSERRAT_BOLD, ui_bold_font_data))
			gui::ui_bold_font = io.Fonts->AddFontFromMemoryTTF(
				ui_bold_font_data.data(), static_cast<int>(ui_bold_font_data.size()), 22.5f,
				&bold_font_config, language_glyph_ranges.Data);
		if (!gui::ui_bold_font)
			gui::ui_bold_font = gui::ui_font;
		io.FontDefault = gui::ui_font;

		// High-resolution font for crisp, non-pixelated watermark rendering
		ImFontConfig wm_font_config = ui_font_config;
		wm_font_config.OversampleH = 2;
		wm_font_config.OversampleV = 1;
		wm_font_config.PixelSnapH = false;
		wm_font_config.RasterizerMultiply = 1.0f;
		if (!ui_bold_font_data.empty())
			gui::watermark_font = io.Fonts->AddFontFromMemoryTTF(
				ui_bold_font_data.data(), static_cast<int>(ui_bold_font_data.size()), 56.0f,
				&wm_font_config, language_glyph_ranges.Data);
		if (!gui::watermark_font)
			gui::watermark_font = gui::ui_bold_font;

		ImFontConfig icon_font_config{};
		icon_font_config.OversampleH = 3;
		icon_font_config.OversampleV = 1;
		icon_font_config.PixelSnapH = false;
		icon_font_config.RasterizerMultiply = 1.0f;
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
		icon_range_builder.AddChar(0xF554);
		icon_range_builder.AddChar(0xF007);
		icon_range_builder.AddChar(0xF013);
		icon_range_builder.AddChar(0xF07B);
		icon_range_builder.AddChar(0xF108);
		icon_range_builder.AddChar(0xF005);
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

			// Anti-aliased geometry with smooth curve tessellation
			gui::surface_style::configure(style);

			style.WindowRounding = 18.0f;
			style.FrameRounding = 8.0f;
			style.GrabRounding = 8.0f;
			style.PopupRounding = 10.0f;
			style.ChildRounding = 13.0f;
			style.ScrollbarRounding = 8.0f;
			style.ScrollbarSize = 1.0f;
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
			colors[ImGuiCol_WindowBg]               = ImVec4(0.015f, 0.015f, 0.021f, 0.74f);
			colors[ImGuiCol_ChildBg]                = ImVec4(0.024f, 0.024f, 0.032f, 0.60f);
			colors[ImGuiCol_PopupBg]                = ImVec4(0.020f, 0.018f, 0.027f, 0.92f);
			colors[ImGuiCol_Border]                 = ImVec4(0.62f, 0.32f, 1.00f, 0.12f);
			colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
			
			colors[ImGuiCol_FrameBg]                = ImVec4(0.055f, 0.052f, 0.068f, 0.68f);
			colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.090f, 0.082f, 0.110f, 0.80f);
			colors[ImGuiCol_FrameBgActive]          = ImVec4(0.120f, 0.105f, 0.150f, 0.88f);
			
			colors[ImGuiCol_TitleBg]                = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
			colors[ImGuiCol_TitleBgActive]          = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
			colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.08f, 0.08f, 0.09f, 1.00f);
			
			colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.08f, 0.08f, 0.09f, 0.00f);
			colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
			colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
			colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.0f, 0.0f, 0.0f, 0.00f);
			
			// Rose accent used by the selected navigation pill.
			colors[ImGuiCol_CheckMark]              = ImVec4(0.62f, 0.30f, 1.00f, 1.00f);
			colors[ImGuiCol_SliderGrab]             = ImVec4(0.62f, 0.30f, 1.00f, 1.00f);
			colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.76f, 0.47f, 1.00f, 1.00f);
			
			colors[ImGuiCol_Button]                 = ImVec4(0.085f, 0.050f, 0.120f, 0.72f);
			colors[ImGuiCol_ButtonHovered]          = ImVec4(0.66f, 0.36f, 0.96f, 0.85f);
			colors[ImGuiCol_ButtonActive]           = ImVec4(0.66f, 0.36f, 0.96f, 1.00f);
			
			colors[ImGuiCol_Header]                 = ImVec4(0.16f, 0.09f, 0.23f, 0.72f);
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

	// Poll the fixed Right Shift shortcut every frame. GLFW does not reliably
	// expose every Windows virtual key through the hooked WndProc path.
	{
		static bool previous_menu_key = false;
		const bool current_menu_key = (GetAsyncKeyState(VK_RSHIFT) & 0x8000) != 0;
		if (gui::suppress_menu_key_until_release)
		{
			if (!current_menu_key)
				gui::suppress_menu_key_until_release = false;
		}
		else if (current_menu_key && !previous_menu_key && !gui::menu_key_scanning &&
			!gui::should_unload && !gui::loader_visible)
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
	if (gui::draw)
	{
		ClipCursor(NULL);
		POINT mouse_pos{};
		if (GetCursorPos(&mouse_pos) && ScreenToClient(window, &mouse_pos))
		{
			ImGui::GetIO().AddMousePosEvent((float)mouse_pos.x, (float)mouse_pos.y);
		}
		ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
		ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Right, (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0);
	}
	ImGui::NewFrame();
	platform::client_dispatcher::drain();


	// Update dynamic accent colors in style
	{
		auto& style = ImGui::GetStyle();
		ImColor accent = gui::get_dynamic_color();
		const ImVec4 base = accent.Value;
		ImColor accent_hover(base.x + (1.0f - base.x) * 0.14f, base.y + (1.0f - base.y) * 0.14f,
			base.z + (1.0f - base.z) * 0.14f, 1.0f);
		ImColor accent_active(base.x * 0.84f, base.y * 0.84f, base.z * 0.84f, 1.0f);

		style.Colors[ImGuiCol_CheckMark]        = accent;
		style.Colors[ImGuiCol_SliderGrab]       = accent;
		style.Colors[ImGuiCol_SliderGrabActive] = accent_active;
		style.Colors[ImGuiCol_ButtonHovered]    = ImVec4(accent_hover.Value.x, accent_hover.Value.y, accent_hover.Value.z, 0.85f);
		style.Colors[ImGuiCol_ButtonActive]     = accent_active;
		style.Colors[ImGuiCol_HeaderHovered]    = ImVec4(accent_hover.Value.x, accent_hover.Value.y, accent_hover.Value.z, 0.40f);
		style.Colors[ImGuiCol_HeaderActive]     = ImVec4(accent.Value.x, accent.Value.y, accent.Value.z, 0.80f);
		style.Colors[ImGuiCol_TabHovered]       = ImVec4(accent_hover.Value.x, accent_hover.Value.y, accent_hover.Value.z, 0.80f);
		style.Colors[ImGuiCol_Header]           = ImVec4(base.x, base.y, base.z, 0.20f);
		style.Colors[ImGuiCol_TextSelectedBg]   = ImVec4(base.x, base.y, base.z, 0.30f);
		style.Colors[ImGuiCol_NavHighlight]     = accent;
		style.Colors[ImGuiCol_Border]           = ImVec4(base.x, base.y, base.z, 0.12f);
		style.Colors[ImGuiCol_FrameBgHovered]   = ImVec4(0.05f + base.x * 0.07f, 0.05f + base.y * 0.07f, 0.06f + base.z * 0.07f, 0.80f);
		style.Colors[ImGuiCol_FrameBgActive]    = ImVec4(0.05f + base.x * 0.12f, 0.05f + base.y * 0.12f, 0.06f + base.z * 0.12f, 0.88f);
		style.Colors[ImGuiCol_Button]           = ImVec4(0.04f + base.x * 0.06f, 0.04f + base.y * 0.06f, 0.05f + base.z * 0.06f, 0.72f);
		style.Colors[ImGuiCol_SeparatorHovered] = accent_hover;
		style.Colors[ImGuiCol_SeparatorActive]  = accent_active;
		style.Colors[ImGuiCol_ScrollbarGrab]    = ImVec4(0.72f, 0.74f, 0.80f, 0.18f);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(base.x, base.y, base.z, 0.55f);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = accent;
	}

	// The standalone Thedorius injector owns the visible loading experience.
	// Inside Minecraft we only keep a headless readiness gate so gameplay
	// modules cannot run before JNI/JVMTI initialization has finished.
	if (gui::loader_visible)
	{
		const loader_ui::state loader_state = loader_ui::snapshot();
		if (loader_state.completed || loader_state.closing)
			gui::loader_visible = false;
		else if (loader_state.failed)
		{
			gui::loader_visible = false;
			gui::should_unload = true;
		}
	}

	if (!gui::draw)
	{
		if (gui::hud_layout_editor_active) gui::hud_editor::close();
		gui::hud_editor_active = false;
		gui::hud_layout_editor_active = false;
	}

	// Render HUD and ArrayList overlays
	gui::hud_editor::restore_saved_layout();
	gui::hud_editor_active = gui::draw && gui::hud_layout_editor_active;
	if (!gui::loader_visible)
	{
		for (modules::module* module : modules::get_modules())
		{
			if (module->enabled || module->render_when_disabled())
				module->render_overlay();
		}
	}
	else if (auto* jump = dynamic_cast<modules::jump_reset*>(modules::find_module_by_id("jump_reset")))
		jump->cancel_on_client_thread();

	if (gui::draw && !gui::loader_visible)
	{
		const ImGuiViewport* reference_viewport = ImGui::GetMainViewport();

		gui::grim::page_callbacks callbacks{};
		callbacks.friends = []() { friends::render_manager(true); };
		callbacks.configs = []() { render_configs_page(true); };
		gui::grim::render(reference_viewport, callbacks);


	}
	gui::aegis::flush_preferences();
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
	gui::aegis::load_preferences();
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
	static gui::theme::transition animation;
	const double time = ImGui::GetCurrentContext() ? ImGui::GetTime()
		: static_cast<double>(GetTickCount64()) / 1000.0;
	const auto color = animation.evaluate(active_theme, time, offset, gui::aegis::get_preferences().reduce_motion);
	return ImColor(color.r, color.g, color.b, 1.0f);
}
