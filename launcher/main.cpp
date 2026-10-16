#include "launcher.hpp"
#include "../src/auth/auth.hpp"
#include "../src/auth/config.hpp"
#include "../src/auth/launcher_protocol.hpp"
#include <objidl.h>
#include <gdiplus.h>
#include <dwmapi.h>
#include <commctrl.h>
#include <shellapi.h>
#include <thread>
#include <mutex>
#include <algorithm>
#include <cmath>
#include <memory>

using namespace Gdiplus;
namespace {
    constexpr int width = 820, height = 520;
    constexpr UINT changed = WM_APP + 1;
    HWND window{}, edit{}; HFONT inputFont{}; HBRUSH editBrush{};
    float dpi = 1.0f;
    std::atomic_bool cancel{false}, busy{false};
    std::thread worker;
    enum class phase { login, working, ready, launching, done };
    struct view { phase screen = phase::login; std::wstring message = L"Keyini gir, kaldığın yerden devam et.";
        bool error = false; uint64_t received = 0, total = 0; launcher::release release;
        std::vector<launcher::game> games; DWORD selected = 0; };
    view state; std::mutex stateMutex;
    int hover = 0; bool reveal = false; bool preview = false;
    RectF keyRect(226, 262, 368, 44), actionRect(226, 324, 368, 46);
    Color bg(255, 10, 10, 13), fg(255, 231, 230, 235), muted(255, 112, 111, 125);
    Color red(255, 186, 69, 91), green(255, 116, 197, 152);
    void update(const std::function<void(view&)>& fn) {
        { std::lock_guard lock(stateMutex); fn(state); }
        if (window) PostMessageW(window, changed, 0, 0);
    }
    view snapshot() { std::lock_guard lock(stateMutex); return state; }
    bool inside(RectF rect, float x, float y) { return rect.Contains(x, y); }
    void text(Graphics& g, const std::wstring& value, RectF rect, float size, Color color,
        bool center = false, bool mono = false, bool bold = false) {
        Font font(mono ? L"Consolas" : L"Segoe UI", size, bold ? FontStyleBold : FontStyleRegular, UnitPixel);
        SolidBrush brush(color); StringFormat format;
        format.SetAlignment(center ? StringAlignmentCenter : StringAlignmentNear);
        format.SetLineAlignment(StringAlignmentCenter); format.SetTrimming(StringTrimmingEllipsisCharacter);
        g.DrawString(value.c_str(), -1, &font, rect, &format, &brush);
    }
    void box(Graphics& g, RectF rect, Color fill, Color border) {
        SolidBrush brush(fill); Pen pen(border, 1);
        g.FillRectangle(&brush, rect); g.DrawRectangle(&pen, rect);
    }
    void draw(Graphics& g, const view& v, float animation) {
        g.SetSmoothingMode(SmoothingModeAntiAlias); g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.Clear(bg);
        // Sparse terminal grid and a quiet, warm glow instead of noisy decoration.
        Pen grid(Color(10, 140, 106, 125));
        for (int x = 24; x < width; x += 32) g.DrawLine(&grid, float(x), 58.0f, float(x), 465.0f);
        for (int y = 74; y < 464; y += 32) g.DrawLine(&grid, 24.0f, float(y), 796.0f, float(y));
        for (int i = 12; i > 0; --i) {
            SolidBrush glow(Color(2, 167, 51, 77));
            g.FillEllipse(&glow, 292.0f - i * 10, 90.0f - i * 5, 236.0f + i * 20, 112.0f + i * 10);
        }
        SolidBrush dot(red); g.FillRectangle(&dot, 27, 25, 6, 6);
        text(g, L"ONESTEP", RectF(43, 15, 94, 26), 12, fg, false, true, true);
        text(g, L"/ launcher", RectF(123, 15, 100, 26), 11, muted, false, true);
        if (v.screen == phase::ready || v.screen == phase::done)
            text(g, L"Çıkış", RectF(628, 14, 70, 30), 10.5f, hover == 5 ? fg : muted, true, true);
        Pen line(Color(255, 31, 29, 36)); g.DrawLine(&line, 24, 56, 796, 56);
        if (hover == 3) { SolidBrush fill(Color(255, 30, 28, 35)); g.FillRectangle(&fill, 728, 12, 30, 30); }
        if (hover == 4) { SolidBrush fill(Color(255, 65, 28, 37)); g.FillRectangle(&fill, 770, 12, 30, 30); }
        Pen chrome(Color(255, 153, 147, 162), 1.3f);
        g.DrawLine(&chrome, 739, 28, 748, 28);
        g.DrawLine(&chrome, 780, 22, 788, 30); g.DrawLine(&chrome, 788, 22, 780, 30);
        // Deliberately code-native terminal lettering, matching the supplied reference.
        static const char* glyphs[] = {
            "01110100011000110001100011000101110", // O
            "10001110011010110011100011000110001", // N
            "11111100001000011110100001000011111", // E
            "01111100001000001110000010000111110", // S
            "11111001000010000100001000010000100", // T
            "11111100001000011110100001000011111", // E
            "11110100011000111110100001000010000"  // P
        };
        for (int letter = 0; letter < 7; ++letter) for (int row = 0; row < 7; ++row) {
            SolidBrush stroke(Color(255, BYTE(143 + row * 5), BYTE(49 + row * 2), BYTE(69 + row * 3)));
            for (int col = 0; col < 5; ++col) if (glyphs[letter][row * 5 + col] == '1') {
                float x = 268.5f + letter * 42.0f + col * 7.0f, y = 115.0f + row * 8.0f;
                g.FillRectangle(&stroke, x, y, 5.5f, 2.2f);
                g.FillRectangle(&stroke, x, y + 3.3f, 5.5f, 1.1f);
            }
        }
        text(g, L"Y O U R   N E X T   S T E P", RectF(160, 195, 500, 22), 9.5f, muted, true, true);
        const bool login = v.screen == phase::login || v.screen == phase::working;
        text(g, login ? L"LİSANS ANAHTARI" : L"MINECRAFT OTURUMU", RectF(226, 235, 368, 20), 10,
            Color(255, 145, 137, 153), false, true);
        box(g, keyRect, Color(255, 17, 16, 21), hover == 1 ? Color(255, 101, 51, 66) : Color(255, 48, 39, 49));
        if (login) {
            text(g, L"›", RectF(239, 266, 18, 34), 19, red, true, true);
            if (preview) text(g, L"••••  ••••  ••••  ••••", RectF(263, 266, 277, 34), 14, Color(255, 138, 126, 145), false, true);
            text(g, reveal ? L"GİZLE" : L"GÖSTER", RectF(532, 270, 52, 28), 9, muted, true, true);
        } else {
            auto selected = std::find_if(v.games.begin(), v.games.end(), [&](auto& item) { return item.pid == v.selected; });
            const auto caption = selected == v.games.end() ? L"Minecraft bulunamadı · yenile" :
                selected->title + L"  ·  " + std::to_wstring(selected->pid);
            text(g, caption, RectF(239, 266, 312, 34), 12, fg);
            text(g, L"⌄", RectF(558, 268, 24, 30), 16, muted, true);
        }
        const bool active = !busy.load();
        Color button = hover == 2 && active ? Color(255, 151, 49, 72) : Color(255, 114, 37, 55);
        if (!active) button = Color(255, 41, 27, 35);
        box(g, actionRect, button, active ? Color(255, 170, 62, 85) : Color(255, 72, 40, 52));
        std::wstring label = login ? L"Giriş yap   →" : v.screen == phase::done ? L"Client çalışıyor   ✓" : L"Client’i başlat   →";
        if (busy.load()) label = v.screen == phase::launching ? L"Başlatılıyor" : L"Bağlanılıyor";
        if (busy.load()) label += std::wstring(1 + int(animation * 2) % 3, L'.');
        text(g, label, RectF(226, 325, 368, 44), 13, active ? fg : Color(255, 161, 143, 154), true, false, true);
        const auto stateColor = v.error ? Color(255, 235, 119, 128) : green;
        text(g, L">", RectF(226, 389, 18, 22), 12, stateColor, false, true);
        text(g, v.message, RectF(246, 386, 348, 40), 11.5f, stateColor, false, true);
        if (v.total && busy.load()) {
            SolidBrush track(Color(255, 33, 31, 39)), progress(Color(255, 171, 64, 88));
            g.FillRectangle(&track, 226, 438, 368, 2);
            g.FillRectangle(&progress, 226.0f, 438.0f, 368.0f * float(v.received) / float(v.total), 2.0f);
        }
        if (!login && !v.release.version.empty()) text(g, L"v" + launcher::wide(v.release.version), RectF(226, 432, 368, 22), 10, muted, true, true);
        g.DrawLine(&line, 24, 471, 796, 471);
        text(g, L"FABRIC 1.21.1  /  WINDOWS x64", RectF(27, 478, 400, 26), 9.5f, muted, false, true);
        text(g, L"TLS / İMZA KONTROLÜ", RectF(580, 478, 213, 26), 9.5f, Color(255, 126, 156, 142), true, true);
    }
    void controls() {
        const auto v = snapshot();
        const bool show = v.screen == phase::login || v.screen == phase::working;
        ShowWindow(edit, show ? SW_SHOW : SW_HIDE);
        EnableWindow(edit, !busy.load());
        InvalidateRect(window, nullptr, FALSE);
    }
    void begin(const std::function<void()>& action) {
        if (busy.exchange(true)) return;
        if (worker.joinable()) worker.join();
        cancel.store(false);
        try {
            worker = std::thread([action] {
                try { action(); }
                catch (const std::exception& error) {
                    update([&](auto& v) { v.error = true; v.message = launcher::wide(error.what());
                        v.screen = auth::is_authenticated() && !v.release.path.empty() ? phase::ready : phase::login; });
                } catch (...) { update([](auto& v) { v.error = true; v.message = L"İşlem tamamlanamadı. Tekrar dene."; v.screen = phase::login; }); }
                busy.store(false); if (window) PostMessageW(window, changed, 0, 0);
            });
        } catch (...) { busy.store(false); }
        controls();
    }
    void login() {
        if (busy.load()) return;
        wchar_t input[130]{}; GetWindowTextW(edit, input, 130);
        auto key = std::make_shared<std::string>(launcher::narrow(input)); SecureZeroMemory(input, sizeof(input));
        SetWindowTextW(edit, L"");
        update([](auto& v) { v.screen = phase::working; v.error = false; v.message = L"Lisans doğrulanıyor..."; v.received = v.total = 0; });
        begin([key] {
            struct key_guard { std::string& value; ~key_guard() { auth::launch::wipe(value); } } secret{*key};
            auth::controlled_shutdown();
            auto result = auth::authenticate(*key, auth::config::server_domain);
            auth::launch::wipe(*key); auth::launch::wipe(result.session_token);
            if (!result.success) throw std::runtime_error(result.message);
            auth::start_heartbeat_monitor(auth::config::server_domain);
            update([](auto& v) { v.message = L"Güncelleme kontrol ediliyor..."; });
            auto release = launcher::manifest();
            update([](auto& v) { v.message = L"Client indiriliyor..."; });
            launcher::download(release, cancel, [](auto read, auto total) {
                update([&](auto& v) { v.received = read; v.total = total;
                    v.message = L"İndiriliyor · %" + std::to_wstring(read * 100 / total); });
            });
            auto list = launcher::games();
            update([&](auto& v) { v.screen = phase::ready; v.release = release; v.games = list;
                v.selected = list.size() == 1 ? list.front().pid : 0;
                v.message = list.empty() ? L"Minecraft’ı aç, ardından oturum listesini yenile." : L"Dosya doğrulandı. Başlatmaya hazırsın."; });
        });
    }
    void select_game() {
        if (busy.load()) return;
        auto list = launcher::games();
        HMENU menu = CreatePopupMenu();
        for (size_t i = 0; i < list.size(); ++i) {
            const auto label = list[i].title + L"  [" + std::to_wstring(list[i].pid) + L"]";
            AppendMenuW(menu, MF_STRING, UINT_PTR(i + 1), label.c_str());
        }
        if (list.empty()) AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"Açık Minecraft bulunamadı");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr); AppendMenuW(menu, MF_STRING, 10000, L"Listeyi yenile");
        POINT point{int(keyRect.X * dpi), int((keyRect.Y + keyRect.Height) * dpi)}; ClientToScreen(window, &point);
        int chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, point.x, point.y, 0, window, nullptr);
        DestroyMenu(menu);
        update([&](auto& v) { v.games = list;
            if (chosen > 0 && chosen < 10000) v.selected = list[size_t(chosen - 1)].pid;
            else if (list.size() == 1) v.selected = list.front().pid;
            else if (std::none_of(list.begin(), list.end(), [&](auto& g) { return g.pid == v.selected; })) v.selected = 0;
            if (v.screen == phase::done) v.screen = phase::ready;
        });
    }
    void action() {
        const auto v = snapshot(); if (busy.load()) return;
        if (v.screen == phase::login) { login(); return; }
        if (v.screen == phase::done) return;
        const auto selected = std::find_if(v.games.begin(), v.games.end(), [&](auto& item) { return item.pid == v.selected; });
        if (selected == v.games.end()) { select_game(); return; }
        const auto game = *selected;
        update([](auto& value) { value.screen = phase::launching; value.error = false; value.total = 0; });
        begin([game, release = v.release] {
            launcher::launch(game, release, cancel, [](auto& message) { update([&](auto& value) { value.message = message; }); });
            update([](auto& value) { value.screen = phase::done; });
        });
    }
    LRESULT CALLBACK input_proc(HWND control, UINT message, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
        if (message == WM_KEYDOWN && w == VK_RETURN) { action(); return 0; }
        return DefSubclassProc(control, message, w, l);
    }
    void resize_controls() {
        MoveWindow(edit, int(263 * dpi), int(273 * dpi), int(263 * dpi), int(25 * dpi), TRUE);
        if (inputFont) DeleteObject(inputFont);
        inputFont = CreateFontW(-int(14 * dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
        SendMessageW(edit, WM_SETFONT, WPARAM(inputFont), TRUE);
    }
    LRESULT CALLBACK procedure(HWND handle, UINT message, WPARAM w, LPARAM l) {
        switch (message) {
        case WM_CREATE:
            window = handle; dpi = GetDpiForWindow(handle) / 96.0f;
            editBrush = CreateSolidBrush(RGB(17, 16, 21));
            edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD,
                0, 0, 0, 0, handle, reinterpret_cast<HMENU>(100), GetModuleHandleW(nullptr), nullptr);
            SendMessageW(edit, EM_SETLIMITTEXT, 128, 0); SendMessageW(edit, EM_SETPASSWORDCHAR, L'•', 0);
            SetWindowSubclass(edit, input_proc, 1, 0); resize_controls();
            SetTimer(handle, 1, 100, nullptr); SetFocus(edit); return 0;
        case WM_CTLCOLOREDIT:
            SetTextColor(reinterpret_cast<HDC>(w), RGB(224, 216, 229)); SetBkColor(reinterpret_cast<HDC>(w), RGB(17, 16, 21));
            return reinterpret_cast<LRESULT>(editBrush);
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{}; HDC dc = BeginPaint(handle, &paint);
            RECT rect{}; GetClientRect(handle, &rect); Bitmap buffer(rect.right, rect.bottom, PixelFormat32bppPARGB);
            Graphics graphics(&buffer); graphics.ScaleTransform(dpi, dpi);
            draw(graphics, snapshot(), float(GetTickCount64() % 100000) / 1000.0f);
            Graphics target(dc); target.DrawImage(&buffer, 0, 0); EndPaint(handle, &paint); return 0;
        }
        case WM_TIMER: if (busy.load()) InvalidateRect(handle, nullptr, FALSE); return 0;
        case changed: controls(); return 0;
        case WM_MOUSEMOVE: {
            float x = float(short(LOWORD(l))) / dpi, y = float(short(HIWORD(l))) / dpi;
            int next = inside(keyRect, x, y) ? 1 : inside(actionRect, x, y) ? 2 :
                inside(RectF(728, 12, 30, 30), x, y) ? 3 : inside(RectF(770, 12, 30, 30), x, y) ? 4 :
                inside(RectF(628, 14, 70, 30), x, y) ? 5 : 0;
            if (hover != next) { hover = next; InvalidateRect(handle, nullptr, FALSE); }
            SetCursor(LoadCursorW(nullptr, hover ? IDC_HAND : IDC_ARROW));
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, handle, 0}; TrackMouseEvent(&tracking); return 0;
        }
        case WM_MOUSELEAVE: hover = 0; InvalidateRect(handle, nullptr, FALSE); return 0;
        case WM_LBUTTONUP: {
            float x = float(short(LOWORD(l))) / dpi, y = float(short(HIWORD(l))) / dpi;
            if (inside(RectF(770, 12, 30, 30), x, y)) SendMessageW(handle, WM_CLOSE, 0, 0);
            else if (inside(RectF(728, 12, 30, 30), x, y)) ShowWindow(handle, SW_MINIMIZE);
            else if (inside(RectF(628, 14, 70, 30), x, y) &&
                (snapshot().screen == phase::ready || snapshot().screen == phase::done) && !busy.load()) {
                begin([] { auth::controlled_shutdown(); update([](auto& value) { value = {}; }); });
            }
            else if (inside(actionRect, x, y)) action();
            else if (inside(keyRect, x, y)) {
                if (snapshot().screen == phase::login) {
                    if (x >= 532) { reveal = !reveal; SendMessageW(edit, EM_SETPASSWORDCHAR, reveal ? 0 : L'•', 0); InvalidateRect(edit, nullptr, TRUE); }
                    else SetFocus(edit);
                } else if (!busy.load()) select_game();
            }
            return 0;
        }
        case WM_NCHITTEST: {
            POINT point{short(LOWORD(l)), short(HIWORD(l))}; ScreenToClient(handle, &point);
            if (point.y < int(56 * dpi) && point.x < int(620 * dpi)) return HTCAPTION;
            return HTCLIENT;
        }
        case WM_DPICHANGED: {
            dpi = HIWORD(w) / 96.0f; auto bounds = reinterpret_cast<RECT*>(l);
            SetWindowPos(handle, nullptr, bounds->left, bounds->top, bounds->right - bounds->left,
                bounds->bottom - bounds->top, SWP_NOZORDER | SWP_NOACTIVATE); resize_controls(); return 0;
        }
        case WM_CLOSE:
            cancel.store(true); ShowWindow(handle, SW_HIDE); PostQuitMessage(0); return 0;
        case WM_DESTROY: PostQuitMessage(0); return 0;
        }
        return DefWindowProcW(handle, message, w, l);
    }
    int save_preview(const std::wstring& path) {
        preview = true; Bitmap image(width, height, PixelFormat32bppARGB); Graphics graphics(&image);
        draw(graphics, snapshot(), 0);
        UINT count{}, bytes{}; GetImageEncodersSize(&count, &bytes); std::vector<unsigned char> data(bytes);
        auto encoders = reinterpret_cast<ImageCodecInfo*>(data.data()); GetImageEncoders(count, bytes, encoders);
        for (UINT i = 0; i < count; ++i) if (!wcscmp(encoders[i].MimeType, L"image/png"))
            return image.Save(path.c_str(), &encoders[i].Clsid) == Ok ? 0 : 1;
        return 1;
    }
}
int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int show) {
    ULONG_PTR token{}; GdiplusStartupInput input; if (GdiplusStartup(&token, &input, nullptr) != Ok) return 1;
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    int argc{}; auto argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv && argc == 3 && !wcscmp(argv[1], L"--preview")) {
        const int result = save_preview(argv[2]); LocalFree(argv); CoUninitialize(); GdiplusShutdown(token); return result;
    }
    if (argv) LocalFree(argv);
    // Prevent two launchers from racing over one login/download pipeline.
    HANDLE singleton = CreateMutexW(nullptr, FALSE, L"Local\\Onestep.Launcher.UI");
    if (!singleton || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (singleton) CloseHandle(singleton); CoUninitialize(); GdiplusShutdown(token); return 0;
    }
    WNDCLASSW cls{}; cls.hInstance = instance; cls.lpfnWndProc = procedure; cls.lpszClassName = L"Onestep.Launcher.Window";
    cls.hCursor = LoadCursorW(nullptr, IDC_ARROW); RegisterClassW(&cls);
    const auto scale = GetDpiForSystem() / 96.0f;
    const int w = int(width * scale), h = int(height * scale);
    HWND handle = CreateWindowExW(WS_EX_APPWINDOW, cls.lpszClassName, L"Onestep Launcher", WS_POPUP | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        (GetSystemMetrics(SM_CXSCREEN) - w) / 2, (GetSystemMetrics(SM_CYSCREEN) - h) / 2,
        w, h, nullptr, nullptr, instance, nullptr);
    if (handle) {
        BOOL dark = TRUE; DwmSetWindowAttribute(handle, 20, &dark, sizeof(dark));
        int corners = 2; DwmSetWindowAttribute(handle, 33, &corners, sizeof(corners));
        ShowWindow(handle, show); UpdateWindow(handle);
        MSG message{}; while (GetMessageW(&message, nullptr, 0, 0) > 0) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    cancel.store(true); if (worker.joinable()) worker.join();
    auth::controlled_shutdown(); if (handle) DestroyWindow(handle); window = nullptr;
    if (inputFont) DeleteObject(inputFont); if (editBrush) DeleteObject(editBrush);
    CloseHandle(singleton); CoUninitialize(); GdiplusShutdown(token); return handle ? 0 : 1;
}
