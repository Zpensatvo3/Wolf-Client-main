#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <Windows.h>
#include <windowsx.h>
#include <TlHelp32.h>
#include <ShlObj.h>
#include <dwmapi.h>
#include <gl/GL.h>
#include <wincodec.h>

#include <imgui/imgui.h>
#include <imgui/imgui_impl_opengl3.h>
#include <imgui/imgui_impl_win32.h>

#include "resource.h"
#include "../src/gui/surface_style.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace
{
    struct texture
    {
        GLuint id = 0;
        int width = 0;
        int height = 0;
    };

    struct game_process
    {
        DWORD pid = 0;
        HWND window = nullptr;
        std::wstring title;
        bool confirmed_1211 = false;
    };

    enum class stage
    {
        scanning,
        ready,
        extracting,
        injecting,
        success,
        failed
    };

    HWND app_window = nullptr;
    HDC device_context = nullptr;
    HGLRC render_context = nullptr;
    bool running = true;
    texture background_texture{};
    texture logo_texture{};
    ImFont* ui_font = nullptr;
    ImFont* bold_font = nullptr;
    std::vector<unsigned char> font_memory;
    std::vector<unsigned char> bold_font_memory;
    std::vector<game_process> processes;
    int selected_process = 0;
    stage current_stage = stage::scanning;
    std::string status_detail = "Searching for a Fabric 1.21.1 Minecraft session";
    float visual_progress = 0.08f;
    std::chrono::steady_clock::time_point last_scan{};
    std::chrono::steady_clock::time_point success_time{};

    std::vector<unsigned char> resource_bytes(int id)
    {
        HMODULE module = GetModuleHandleW(nullptr);
        HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(id), RT_RCDATA);
        if (!resource) return {};
        HGLOBAL loaded = LoadResource(module, resource);
        const DWORD size = SizeofResource(module, resource);
        const void* data = loaded ? LockResource(loaded) : nullptr;
        if (!data || size == 0) return {};
        const auto* begin = static_cast<const unsigned char*>(data);
        return { begin, begin + size };
    }

    texture create_texture(const std::vector<unsigned char>& encoded)
    {
        texture output{};
        if (encoded.empty()) return output;
        IWICImagingFactory* factory = nullptr;
        IWICStream* stream = nullptr;
        IWICBitmapDecoder* decoder = nullptr;
        IWICBitmapFrameDecode* frame = nullptr;
        IWICFormatConverter* converter = nullptr;
        UINT width = 0, height = 0;
        std::vector<BYTE> pixels;

        if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&factory))) || !factory) goto cleanup;
        if (FAILED(factory->CreateStream(&stream)) || !stream ||
            FAILED(stream->InitializeFromMemory(const_cast<BYTE*>(encoded.data()),
                static_cast<DWORD>(encoded.size())))) goto cleanup;
        if (FAILED(factory->CreateDecoderFromStream(stream, nullptr, WICDecodeMetadataCacheOnLoad, &decoder)) ||
            !decoder || FAILED(decoder->GetFrame(0, &frame)) || !frame ||
            FAILED(factory->CreateFormatConverter(&converter)) || !converter) goto cleanup;
        if (FAILED(converter->Initialize(frame, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
            nullptr, 0.0, WICBitmapPaletteTypeCustom)) || FAILED(converter->GetSize(&width, &height)) ||
            width == 0 || height == 0) goto cleanup;
        pixels.resize(static_cast<size_t>(width) * height * 4u);
        if (FAILED(converter->CopyPixels(nullptr, width * 4u, static_cast<UINT>(pixels.size()), pixels.data())))
            goto cleanup;
        glGenTextures(1, &output.id);
        glBindTexture(GL_TEXTURE_2D, output.id);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
            0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
        glBindTexture(GL_TEXTURE_2D, 0);
        output.width = static_cast<int>(width);
        output.height = static_cast<int>(height);
    cleanup:
        if (converter) converter->Release();
        if (frame) frame->Release();
        if (decoder) decoder->Release();
        if (stream) stream->Release();
        if (factory) factory->Release();
        return output;
    }

    struct window_search { DWORD pid; HWND result; };
    BOOL CALLBACK find_window(HWND window, LPARAM value)
    {
        auto* search = reinterpret_cast<window_search*>(value);
        DWORD pid = 0;
        GetWindowThreadProcessId(window, &pid);
        if (pid != search->pid || !IsWindowVisible(window) || GetWindow(window, GW_OWNER)) return TRUE;
        wchar_t title[512]{};
        GetWindowTextW(window, title, static_cast<int>(std::size(title)));
        if (!title[0]) return TRUE;
        search->result = window;
        return FALSE;
    }

    void scan_processes()
    {
        std::vector<game_process> found;
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot != INVALID_HANDLE_VALUE)
        {
            PROCESSENTRY32W process{ sizeof(process) };
            if (Process32FirstW(snapshot, &process))
            {
                do
                {
                    if (_wcsicmp(process.szExeFile, L"javaw.exe") != 0 &&
                        _wcsicmp(process.szExeFile, L"java.exe") != 0) continue;
                    window_search search{ process.th32ProcessID, nullptr };
                    EnumWindows(find_window, reinterpret_cast<LPARAM>(&search));
                    if (!search.result) continue;
                    wchar_t title[512]{};
                    GetWindowTextW(search.result, title, static_cast<int>(std::size(title)));
                    std::wstring normalized = title;
                    std::wstring lower = normalized;
                    std::transform(lower.begin(), lower.end(), lower.begin(), towlower);
                    if (lower.find(L"minecraft") == std::wstring::npos) continue;
                    const bool version_1211 = lower.find(L"1.21.1") != std::wstring::npos;
                    found.push_back({ process.th32ProcessID, search.result, normalized,
                        version_1211 });
                } while (Process32NextW(snapshot, &process));
            }
            CloseHandle(snapshot);
        }
        processes = std::move(found);
        selected_process = std::clamp(selected_process, 0, std::max(0, static_cast<int>(processes.size()) - 1));
        if (current_stage == stage::scanning && !processes.empty())
        {
            current_stage = stage::ready;
            status_detail = "Minecraft runtime found. Ready to inject.";
        }
        else if (current_stage == stage::ready && processes.empty())
        {
            current_stage = stage::scanning;
            status_detail = "Searching for a Fabric 1.21.1 Minecraft session";
        }
    }

    std::filesystem::path extract_client()
    {
        const auto bytes = resource_bytes(IDR_EMBEDDED_CLIENT);
        if (bytes.empty()) return {};
        PWSTR local = nullptr;
        if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_CREATE, nullptr, &local)) || !local)
            return {};
        std::filesystem::path directory = std::filesystem::path(local) / L"Thedorius" / L"Runtime" / L"0.3";
        CoTaskMemFree(local);
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        if (error) return {};
        const std::filesystem::path path = directory / L"Onestep.dll";
        HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
            FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE) return {};
        DWORD written = 0;
        const BOOL okay = WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr);
        FlushFileBuffers(file);
        CloseHandle(file);
        return okay && written == bytes.size() ? path : std::filesystem::path{};
    }

    bool module_already_loaded(DWORD pid)
    {
        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
        if (snapshot == INVALID_HANDLE_VALUE) return false;
        MODULEENTRY32W entry{ sizeof(entry) };
        bool found = false;
        if (Module32FirstW(snapshot, &entry))
        {
            do
            {
                if (_wcsicmp(entry.szModule, L"Onestep.dll") == 0) { found = true; break; }
            } while (Module32NextW(snapshot, &entry));
        }
        CloseHandle(snapshot);
        return found;
    }

    bool inject_client(const game_process& target, const std::filesystem::path& dll, std::string& error)
    {
        if (module_already_loaded(target.pid)) { error = "Thedorius is already loaded in this session."; return true; }
        HANDLE process = OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
            PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ, FALSE, target.pid);
        if (!process) { error = "Minecraft process could not be opened."; return false; }
        const std::wstring path = dll.wstring();
        const SIZE_T bytes = (path.size() + 1) * sizeof(wchar_t);
        void* remote = VirtualAllocEx(process, nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!remote) { CloseHandle(process); error = "Remote memory allocation failed."; return false; }
        SIZE_T written = 0;
        const bool copied = WriteProcessMemory(process, remote, path.c_str(), bytes, &written) && written == bytes;
        auto load_library = reinterpret_cast<LPTHREAD_START_ROUTINE>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
        HANDLE thread = copied && load_library
            ? CreateRemoteThread(process, nullptr, 0, load_library, remote, 0, nullptr) : nullptr;
        bool success = false;
        if (thread)
        {
            if (WaitForSingleObject(thread, 15000) == WAIT_OBJECT_0)
            {
                DWORD result = 0;
                success = GetExitCodeThread(thread, &result) && result != 0;
            }
            CloseHandle(thread);
        }
        VirtualFreeEx(process, remote, 0, MEM_RELEASE);
        CloseHandle(process);
        if (!success) error = "Client injection failed. Check Windows security and process permissions.";
        return success;
    }

    float approach(float current, float target, float speed)
    {
        return current + (target - current) * (1.0f - std::exp(-speed * ImGui::GetIO().DeltaTime));
    }

    ImU32 alpha_color(ImU32 color, float alpha)
    {
        ImVec4 value = ImGui::ColorConvertU32ToFloat4(color);
        value.w *= alpha;
        return ImGui::ColorConvertFloat4ToU32(value);
    }

    void draw_background(ImDrawList* draw, const ImVec2& size)
    {
        if (background_texture.id)
        {
            const float image_aspect = static_cast<float>(background_texture.width) / background_texture.height;
            const float view_aspect = size.x / size.y;
            ImVec2 uv0(0, 0), uv1(1, 1);
            if (image_aspect > view_aspect)
            {
                const float visible = view_aspect / image_aspect;
                uv0.x = (1.0f - visible) * 0.5f;
                uv1.x = 1.0f - uv0.x;
            }
            else
            {
                const float visible = image_aspect / view_aspect;
                uv0.y = (1.0f - visible) * 0.5f;
                uv1.y = 1.0f - uv0.y;
            }
            draw->AddImage(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(background_texture.id)),
                ImVec2(0, 0), size, uv0, uv1);
        }
        draw->AddRectFilled(ImVec2(0, 0), size, IM_COL32(5, 4, 8, 184));
    }

    void render_ui()
    {
        const ImVec2 size = ImGui::GetIO().DisplaySize;
        draw_background(ImGui::GetBackgroundDrawList(), size);

        ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
        ImGui::SetNextWindowSize(size);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0));
        ImGui::Begin("##ThedoriusLauncherCanvas", nullptr,
            ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoBringToFrontOnFocus);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar(2);
        ImDrawList* draw = ImGui::GetWindowDrawList();

        const double now = ImGui::GetTime();
        const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(now) * 2.1f);
        const ImU32 accent = IM_COL32(155, 69, 255, 255);
        const ImVec2 panel_min(66.0f, 58.0f);
        const ImVec2 panel_max(size.x - 66.0f, size.y - 58.0f);
        draw->AddRectFilled(panel_min, panel_max, IM_COL32(17, 15, 22, 245), 20.0f);
        draw->AddRect(panel_min, panel_max, IM_COL32(255, 255, 255, 16), 20.0f);

        const ImVec2 sidebar_max(panel_min.x + 214.0f, panel_max.y);
        draw->AddRectFilled(panel_min, sidebar_max, IM_COL32(24, 21, 29, 250), 20.0f,
            ImDrawFlags_RoundCornersLeft);
        if (logo_texture.id)
            draw->AddImage(reinterpret_cast<ImTextureID>(static_cast<intptr_t>(logo_texture.id)),
                ImVec2(panel_min.x + 20.0f, panel_min.y + 20.0f), ImVec2(panel_min.x + 70.0f, panel_min.y + 70.0f));
        draw->AddText(bold_font, 19.0f, ImVec2(panel_min.x + 81.0f, panel_min.y + 23.0f),
            IM_COL32(244, 242, 247, 255), "Thedorius");
        draw->AddText(ui_font, 13.0f, ImVec2(panel_min.x + 82.0f, panel_min.y + 47.0f),
            IM_COL32(132, 126, 140, 255), "Native Loader 0.3");

        const char* labels[] = { "Runtime", "Client", "Security" };
        const char* values[] = { "Fabric 1.21.1", "Embedded DLL", "Test signed" };
        for (int index = 0; index < 3; ++index)
        {
            const float y = panel_min.y + 118.0f + index * 62.0f;
            draw->AddText(ui_font, 12.0f, ImVec2(panel_min.x + 22.0f, y), IM_COL32(126, 120, 133, 255), labels[index]);
            draw->AddText(bold_font, 14.0f, ImVec2(panel_min.x + 22.0f, y + 20.0f), IM_COL32(225, 221, 230, 255), values[index]);
        }

        const ImVec2 content_min(sidebar_max.x + 30.0f, panel_min.y + 28.0f);
        draw->AddText(bold_font, 23.0f, content_min, IM_COL32(245, 243, 248, 255), "Attach Thedorius");
        draw->AddText(ui_font, 14.0f, ImVec2(content_min.x, content_min.y + 34.0f),
            IM_COL32(145, 139, 151, 255), "Select a Minecraft session and load the embedded client.");

        const ImVec2 status_min(content_min.x, content_min.y + 82.0f);
        const ImVec2 status_max(panel_max.x - 30.0f, status_min.y + 116.0f);
        draw->AddRectFilled(status_min, status_max, IM_COL32(27, 24, 32, 244), 12.0f);
        draw->AddRect(status_min, status_max, IM_COL32(255, 255, 255, 12), 12.0f);
        draw->AddCircleFilled(ImVec2(status_min.x + 32.0f, status_min.y + 34.0f), 15.0f,
            alpha_color(accent, 0.13f + pulse * 0.05f), 0);
        draw->AddCircle(ImVec2(status_min.x + 32.0f, status_min.y + 34.0f), 9.0f,
            current_stage == stage::failed ? IM_COL32(224, 76, 99, 255) : accent, 0, 2.0f);

        const char* state_title = "Scanning for Minecraft";
        float target_progress = 0.20f;
        if (current_stage == stage::ready) { state_title = "Minecraft session ready"; target_progress = 0.42f; }
        if (current_stage == stage::extracting) { state_title = "Preparing embedded client"; target_progress = 0.61f; }
        if (current_stage == stage::injecting) { state_title = "Injecting Thedorius"; target_progress = 0.82f; }
        if (current_stage == stage::success) { state_title = "Client loaded successfully"; target_progress = 1.0f; }
        if (current_stage == stage::failed) { state_title = "Injection failed"; target_progress = 1.0f; }
        visual_progress = approach(visual_progress, target_progress, 5.5f);
        draw->AddText(bold_font, 16.0f, ImVec2(status_min.x + 58.0f, status_min.y + 18.0f),
            IM_COL32(238, 235, 242, 255), state_title);
        draw->AddText(ui_font, 13.0f, ImVec2(status_min.x + 58.0f, status_min.y + 43.0f),
            IM_COL32(139, 133, 146, 255), status_detail.c_str());
        const float track_left = status_min.x + 20.0f;
        const float track_right = status_max.x - 20.0f;
        const float track_y = status_min.y + 84.0f;
        draw->AddRectFilled(ImVec2(track_left, track_y), ImVec2(track_right, track_y + 6.0f), IM_COL32(50, 46, 56, 255), 3.0f);
        draw->AddRectFilled(ImVec2(track_left, track_y),
            ImVec2(track_left + (track_right - track_left) * visual_progress, track_y + 6.0f), accent, 3.0f);

        const float list_y = status_max.y + 20.0f;
        draw->AddText(bold_font, 14.0f, ImVec2(content_min.x, list_y), IM_COL32(226, 223, 231, 255), "Minecraft sessions");
        ImGui::SetCursorScreenPos(ImVec2(content_min.x, list_y + 27.0f));
        ImGui::BeginGroup();
        if (processes.empty())
        {
            const ImVec2 empty_min = ImGui::GetCursorScreenPos();
            draw->AddRectFilled(empty_min, ImVec2(status_max.x, empty_min.y + 70.0f), IM_COL32(255, 255, 255, 5), 10.0f);
            draw->AddText(ui_font, 14.0f, ImVec2(empty_min.x + 16.0f, empty_min.y + 24.0f),
                IM_COL32(128, 122, 135, 255), "No Minecraft window detected yet.");
            ImGui::Dummy(ImVec2(status_max.x - empty_min.x, 70.0f));
        }
        else
        {
            for (int index = 0; index < static_cast<int>(processes.size()); ++index)
            {
                ImGui::PushID(index);
                const ImVec2 row_min = ImGui::GetCursorScreenPos();
                ImGui::InvisibleButton("##process", ImVec2(status_max.x - content_min.x, 52.0f));
                const bool selected = index == selected_process;
                const bool hovered = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked()) selected_process = index;
                draw->AddRectFilled(row_min, ImVec2(status_max.x, row_min.y + 52.0f),
                    selected ? IM_COL32(69, 43, 100, 210) : (hovered ? IM_COL32(255, 255, 255, 8) : IM_COL32(255, 255, 255, 5)), 9.0f);
                const std::string title(processes[index].title.begin(), processes[index].title.end());
                draw->AddText(bold_font, 14.0f, ImVec2(row_min.x + 14.0f, row_min.y + 9.0f),
                    IM_COL32(235, 232, 240, 255), title.c_str());
                const std::string meta = "PID " + std::to_string(processes[index].pid) +
                    (processes[index].confirmed_1211 ? "  |  1.21.1 confirmed" : "  |  version from runtime");
                draw->AddText(ui_font, 12.0f, ImVec2(row_min.x + 14.0f, row_min.y + 29.0f),
                    IM_COL32(130, 124, 138, 255), meta.c_str());
                ImGui::PopID();
            }
        }
        ImGui::EndGroup();

        const ImVec2 button_size(154.0f, 38.0f);
        const ImVec2 button_min(panel_max.x - 30.0f - button_size.x, panel_max.y - 58.0f);
        ImGui::SetCursorScreenPos(button_min);
        ImGui::BeginDisabled(processes.empty() || current_stage == stage::extracting || current_stage == stage::injecting || current_stage == stage::success);
        ImGui::InvisibleButton("##inject", button_size);
        const bool hovered = ImGui::IsItemHovered();
        const bool clicked = ImGui::IsItemClicked();
        draw->AddRectFilled(button_min, ImVec2(button_min.x + button_size.x, button_min.y + button_size.y),
            hovered ? IM_COL32(170, 92, 255, 255) : accent, 9.0f);
        const char* inject_label = current_stage == stage::failed ? "Try again" : "Inject client";
        const ImVec2 inject_extent = bold_font->CalcTextSizeA(14.0f, FLT_MAX, 0.0f, inject_label);
        draw->AddText(bold_font, 14.0f,
            ImVec2(button_min.x + (button_size.x - inject_extent.x) * 0.5f,
                button_min.y + (button_size.y - inject_extent.y) * 0.5f), IM_COL32(255, 255, 255, 255), inject_label);
        ImGui::EndDisabled();

        if (clicked && !processes.empty())
        {
            const game_process target = processes[selected_process];
            current_stage = stage::extracting;
            status_detail = "Extracting the signed client from launcher resources";
            const std::filesystem::path client = extract_client();
            if (client.empty())
            {
                current_stage = stage::failed;
                status_detail = "The embedded client could not be written to LocalAppData.";
            }
            else
            {
                current_stage = stage::injecting;
                status_detail = "Loading Onestep.dll into the selected game";
                std::string error;
                if (inject_client(target, client, error))
                {
                    current_stage = stage::success;
                    status_detail = "Injection complete. Returning focus to Minecraft.";
                    success_time = std::chrono::steady_clock::now();
                    ShowWindow(target.window, SW_RESTORE);
                    SetForegroundWindow(target.window);
                }
                else
                {
                    current_stage = stage::failed;
                    status_detail = error;
                }
            }
        }

        if (current_stage == stage::success &&
            std::chrono::steady_clock::now() - success_time > std::chrono::milliseconds(1800))
            running = false;

        ImGui::End();
    }

    LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
    {
        if (ImGui_ImplWin32_WndProcHandler(window, message, wparam, lparam)) return true;
        switch (message)
        {
        case WM_CLOSE: running = false; return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        case WM_NCHITTEST:
        {
            const LRESULT result = DefWindowProcW(window, message, wparam, lparam);
            if (result == HTCLIENT)
            {
                POINT point{ GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam) };
                ScreenToClient(window, &point);
                if (point.y < 50) return HTCAPTION;
            }
            return result;
        }
        default: return DefWindowProcW(window, message, wparam, lparam);
        }
    }

    bool create_window(HINSTANCE instance)
    {
        WNDCLASSEXW klass{ sizeof(klass), CS_OWNDC, window_proc, 0, 0, instance, nullptr,
            LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr, L"ThedoriusLoaderWindow", nullptr };
        if (!RegisterClassExW(&klass)) return false;
        const int width = 940, height = 590;
        const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
        const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;
        app_window = CreateWindowExW(WS_EX_APPWINDOW, klass.lpszClassName, L"Thedorius",
            WS_POPUP, x, y, width, height, nullptr, nullptr, instance, nullptr);
        if (!app_window) return false;
        const DWORD corner = 2;
        DwmSetWindowAttribute(app_window, 33, &corner, sizeof(corner));
        device_context = GetDC(app_window);
        PIXELFORMATDESCRIPTOR descriptor{ sizeof(descriptor), 1, PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
            PFD_TYPE_RGBA, 32, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 8, 0, PFD_MAIN_PLANE };
        const int format = ChoosePixelFormat(device_context, &descriptor);
        if (!format || !SetPixelFormat(device_context, format, &descriptor)) return false;
        render_context = wglCreateContext(device_context);
        if (!render_context || !wglMakeCurrent(device_context, render_context)) return false;
        ShowWindow(app_window, SW_SHOW);
        UpdateWindow(app_window);
        return true;
    }
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int)
{
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (!create_window(instance)) return 1;
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    gui::surface_style::configure(ImGui::GetStyle());
    font_memory = resource_bytes(IDR_LAUNCHER_FONT);
    bold_font_memory = resource_bytes(IDR_LAUNCHER_FONT_BOLD);
    ImFontConfig font_config{};
    font_config.FontDataOwnedByAtlas = false;
    ui_font = font_memory.empty() ? io.Fonts->AddFontDefault()
        : io.Fonts->AddFontFromMemoryTTF(font_memory.data(), static_cast<int>(font_memory.size()), 17.0f, &font_config);
    bold_font = bold_font_memory.empty() ? ui_font
        : io.Fonts->AddFontFromMemoryTTF(bold_font_memory.data(), static_cast<int>(bold_font_memory.size()), 20.0f, &font_config);
    io.FontDefault = ui_font;
    ImGui_ImplWin32_Init(app_window);
    ImGui_ImplOpenGL3_Init("#version 130");
    background_texture = create_texture(resource_bytes(IDR_LAUNCHER_BACKGROUND));
    logo_texture = create_texture(resource_bytes(IDR_LAUNCHER_LOGO));

    while (running)
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
            if (message.message == WM_QUIT) running = false;
        }
        if (!running) break;
        if (std::chrono::steady_clock::now() - last_scan > std::chrono::milliseconds(1200) &&
            current_stage != stage::extracting && current_stage != stage::injecting && current_stage != stage::success)
        {
            scan_processes();
            last_scan = std::chrono::steady_clock::now();
        }
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        render_ui();
        ImGui::Render();
        RECT client{};
        GetClientRect(app_window, &client);
        glViewport(0, 0, client.right, client.bottom);
        glClearColor(0.02f, 0.02f, 0.03f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SwapBuffers(device_context);
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    if (background_texture.id) glDeleteTextures(1, &background_texture.id);
    if (logo_texture.id) glDeleteTextures(1, &logo_texture.id);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    wglMakeCurrent(nullptr, nullptr);
    if (render_context) wglDeleteContext(render_context);
    if (device_context && app_window) ReleaseDC(app_window, device_context);
    if (app_window) DestroyWindow(app_window);
    CoUninitialize();
    return 0;
}
