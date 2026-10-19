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
    constexpr int width = 760, height = 430;
    constexpr UINT changed = WM_APP + 1;
    HWND window{}, edit{}; HFONT inputFont{}; HBRUSH editBrush{};
    float dpi = 1.0f;
    std::atomic_bool cancel{false}, busy{false};
    std::thread worker;
    enum class phase { login, working, ready, launching, done };
    struct view { phase screen = phase::login; std::wstring message = L"Ready to sign in.";
        bool error = false; uint64_t received = 0, total = 0; launcher::release release;
        std::vector<launcher::game> games; DWORD selected = 0; };
    view state; std::mutex stateMutex;
    int hover = 0; bool reveal = false; bool preview = false;
    RectF keyRect(431, 155, 273, 46), actionRect(431, 235, 273, 49);
    Color bg(255, 10, 11, 18), fg(255, 247, 247, 250), muted(255, 133, 137, 154);
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
    // Rounded surfaces keep the native GDI+ UI cohesive without a browser dependency.
    void rounded_box(Graphics& g, RectF rect, float radius, Color fill, Color border) {
        const float diameter = radius * 2.0f;
        GraphicsPath path;
        path.AddArc(rect.X, rect.Y, diameter, diameter, 180, 90);
        path.AddArc(rect.GetRight() - diameter, rect.Y, diameter, diameter, 270, 90);
        path.AddArc(rect.GetRight() - diameter, rect.GetBottom() - diameter, diameter, diameter, 0, 90);
        path.AddArc(rect.X, rect.GetBottom() - diameter, diameter, diameter, 90, 90);
        path.CloseFigure();
        SolidBrush background(fill);
        Pen outline(border, 1.0f);
        g.FillPath(&background, &path);
        g.DrawPath(&outline, &path);
    }

    void gradient_button(Graphics& g, RectF r, float radius, Color left, Color right, Color line) {
        GraphicsPath path; const float d = radius * 2.0f;
        path.AddArc(r.X, r.Y, d, d, 180, 90);
        path.AddArc(r.GetRight() - d, r.Y, d, d, 270, 90);
        path.AddArc(r.GetRight() - d, r.GetBottom() - d, d, d, 0, 90);
        path.AddArc(r.X, r.GetBottom() - d, d, d, 90, 90);
        path.CloseFigure();
        LinearGradientBrush gradient(PointF(r.X, r.Y), PointF(r.GetRight(), r.Y), left, right);
        Pen outline(line, 1.0f);
        g.FillPath(&gradient, &path);
        g.DrawPath(&outline, &path);
    }
    std::wstring status_text(const view& v) {
        if (v.screen == phase::login && !v.error) return L"Ready to sign in.";
        if (v.screen == phase::done && !v.error) return L"Client is running.";
        if (v.screen == phase::ready && !v.error && v.games.empty()) return L"Open Minecraft, then refresh.";
        if (v.screen == phase::ready && !v.error && v.selected == 0) return L"Select a Minecraft window.";
        if (v.screen == phase::ready && !v.error) return L"Ready to launch.";
        if (v.message.empty()) return v.error ? L"Please try again." : L"Preparing...";
        // Backend / game loader can return old localized messages.
        // Use English UI fallback rather than presenting mixed languages.
        if (std::any_of(v.message.begin(), v.message.end(),
            [](wchar_t c) { return static_cast<unsigned int>(c) > 127U; }))
            return v.error ? L"Unable to continue. Please retry." : L"Preparing your session...";
        return v.message;
    }
    void draw(Graphics& g, const view& v, float animation) {
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.Clear(bg);
        const Color pink(255, 236, 79, 132), palePink(255, 252, 119, 163);
        const Color secondary(255, 165, 170, 186), border(255, 46, 49, 65);
        const Color panel(255, 18, 20, 30), field(255, 23, 26, 38);
        const Color success(255, 58, 233, 152);
        const bool loginScreen = v.screen == phase::login || v.screen == phase::working;
        const bool working = busy.load(), finished = v.screen == phase::done;
        const auto selected = std::find_if(v.games.begin(), v.games.end(),
            [&](const auto& item) { return item.pid == v.selected; });

        // Muted pink halo limited to the hero area; the rest of the window remains quiet.
        GraphicsState saved = g.Save();
        g.SetClip(RectF(0, 55, 396, 350));
        for (int i = 15; i > 0; --i) {
            const float spread = i * 12.0f;
            SolidBrush glow(Color(BYTE(2 + (15 - i) / 2), 231, 40, 104));
            g.FillEllipse(&glow, -75.0f - spread, 136.0f - spread,
                330.0f + 2 * spread, 330.0f + 2 * spread);
        }
        Pen rim(Color(67, 230, 47, 115), 1.3f);
        g.DrawArc(&rim, -110, 112, 486, 464, 220, 108);
        g.Restore(saved);

        SolidBrush brand(pink);
        g.FillEllipse(&brand, 27, 24, 10, 10);
        text(g, L"OneStep", RectF(48, 14, 160, 32), 14.2f, fg, false, false, true);

        if (v.screen == phase::ready || finished)
            text(g, L"Sign out", RectF(592, 15, 71, 29), 10.2f,
                hover == 5 ? fg : muted, true);
        if (hover == 3) rounded_box(g, RectF(673, 13, 30, 30), 7, Color(255, 33, 36, 47), border);
        if (hover == 4) rounded_box(g, RectF(716, 13, 30, 30), 7, Color(255, 66, 27, 45), border);
        Pen chrome(Color(255, 173, 177, 189), 1.4f);
        g.DrawLine(&chrome, 684, 28, 694, 28);
        g.DrawLine(&chrome, 726, 23, 735, 32);
        g.DrawLine(&chrome, 735, 23, 726, 32);

        if (loginScreen) {
            text(g, L"Welcome", RectF(50, 147, 193, 52), 32, fg, false, false, true);
            text(g, L"back.", RectF(224, 147, 147, 52), 32, pink, false, false, true);
            text(g, L"Enter your license key to continue.",
                RectF(52, 199, 340, 37), 12.3f, secondary);
        } else {
            text(g, L"Choose", RectF(49, 129, 320, 53), 34, fg, false, false, true);
            text(g, L"your", RectF(49, 177, 100, 52), 34, fg, false, false, true);
            text(g, L"session.", RectF(141, 177, 235, 52), 34, pink, false, false, true);
            text(g, L"Select a window and launch.",
                RectF(52, 240, 333, 31), 12.3f, secondary);
        }

        rounded_box(g, RectF(405, 93, 325, 292), 15, panel, border);
        text(g, loginScreen ? L"License Key" : L"Session",
            RectF(431, 117, 273, 29), 13.4f, fg, false, false, true);
        rounded_box(g, keyRect, 10, field,
            hover == 1 && !working ? Color(255, 124, 62, 92) : border);
        if (loginScreen) {
            Pen key(pink, 1.7f);
            g.DrawEllipse(&key, 448, 164, 10, 10);
            g.DrawLine(&key, 453, 174, 444, 183);
            g.DrawLine(&key, 445, 179, 448, 182);
            g.DrawLine(&key, 448, 177, 451, 180);
            if (preview) text(g, L"••••  ••••  ••••  ••••",
                RectF(471, 164, 177, 27), 11.8f, secondary, false, true);
            // Compact eye affordance that toggles the underlying Win32 password control.
            g.DrawArc(&chrome, 668, 162, 23, 16, 200, 140);
            g.DrawArc(&chrome, 668, 162, 23, 16, 20, 140);
            SolidBrush iris(pink);
            g.FillEllipse(&iris, 677, 167, 5, 5);
            if (reveal) g.DrawLine(&chrome, 667, 181, 692, 159);
        } else {
            text(g, selected == v.games.end() ? L"Minecraft Window" : selected->title,
                RectF(447, 159, 208, 31), 12.4f,
                selected == v.games.end() ? secondary : fg, false, false, true);
            Pen arrow(pink, 1.7f);
            g.DrawLine(&arrow, 673, 169, 680, 176);
            g.DrawLine(&arrow, 680, 176, 687, 169);
        }

        const bool canClick = !working && !finished;
        const Color b1 = !canClick ? Color(255, 73, 49, 62)
            : hover == 2 ? palePink : Color(255, 245, 92, 137);
        const Color b2 = !canClick ? Color(255, 60, 41, 55)
            : hover == 2 ? pink : Color(255, 216, 44, 105);
        gradient_button(g, actionRect, 10, b1, b2,
            canClick ? Color(255, 255, 125, 163) : Color(255, 89, 62, 74));
        std::wstring label = loginScreen ? L"Sign In  →" :
            finished ? L"Client Running" : L"Launch Client  →";
        if (working) {
            label = v.screen == phase::launching ? L"Launching" : L"Please wait";
            label += std::wstring(1 + int(animation * 2.0f) % 3, L'.');
        }
        text(g, label, actionRect, 13.4f,
            canClick ? Color(255, 255, 249, 253) : secondary, true, false, true);

        rounded_box(g, RectF(431, 307, 273, 55), 9, field, border);
        const Color statusColor = v.error ? Color(255, 255, 121, 143)
            : working ? palePink : success;
        SolidBrush statusDot(statusColor);
        g.FillEllipse(&statusDot, 445, 319, 8, 8);
        text(g, v.error ? L"Error" : L"Status", RectF(465, 309, 220, 26),
            11.2f, fg, false, false, true);
        text(g, status_text(v), RectF(465, 331, 223, 23), 10.3f, secondary);
        if (working && v.total > 0) {
            const float part = float(std::min(v.received, v.total)) / float(v.total);
            SolidBrush track(Color(255, 49, 45, 58)), fill(pink);
            g.FillRectangle(&track, 433.0f, 370.0f, 269.0f, 3.0f);
            g.FillRectangle(&fill, 433.0f, 370.0f, 269.0f * part, 3.0f);
        }
    }
    void controls() {
        const auto v = snapshot();
        const bool show = v.screen == phase::login || v.screen == phase::working;
        // Never keep a revealed password after a screen transition or logout.
        if (show && reveal && !busy.load()) {
            reveal = false;
            SendMessageW(edit, EM_SETPASSWORDCHAR, L'•', 0);
            InvalidateRect(edit, nullptr, TRUE);
        }
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
                } catch (...) { update([](auto& v) { v.error = true; v.message = L"Something went wrong. Please retry."; v.screen = phase::login; }); }
                busy.store(false); if (window) PostMessageW(window, changed, 0, 0);
            });
        } catch (...) { busy.store(false); }
        controls();
    }
    void login() {
        if (busy.load()) return;
        wchar_t input[130]{}; GetWindowTextW(edit, input, 130);
        auto key = std::make_shared<std::string>(launcher::narrow(input)); SecureZeroMemory(input, sizeof(input));
        if (key->empty()) {
            update([](auto& v) { v.error = true; v.message = L"Enter a license key."; });
            return;
        }
        SetWindowTextW(edit, L"");
        update([](auto& v) { v.screen = phase::working; v.error = false; v.message = L"Checking license..."; v.received = v.total = 0; });
        begin([key] {
            struct key_guard { std::string& value; ~key_guard() { auth::launch::wipe(value); } } secret{*key};
            auth::controlled_shutdown();
            auto result = auth::authenticate(*key, auth::config::server_domain);
            auth::launch::wipe(*key); auth::launch::wipe(result.session_token);
            if (!result.success) throw std::runtime_error(result.message);
            auth::start_heartbeat_monitor(auth::config::server_domain);
            update([](auto& v) { v.message = L"Checking for updates..."; });
            auto release = launcher::manifest();
            update([](auto& v) { v.message = L"Downloading client..."; });
            launcher::download(release, cancel, [](auto read, auto total) {
                update([&](auto& v) { v.received = read; v.total = total;
                    v.message = L"Downloading... " + std::to_wstring(total ? read * 100 / total : 0) + L"%"; });
            });
            auto list = launcher::games();
            update([&](auto& v) { v.screen = phase::ready; v.release = release; v.games = list;
                v.selected = list.size() == 1 ? list.front().pid : 0;
                v.message = list.empty() ? L"Open Minecraft, then refresh." : L"Ready to launch."; });
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
        if (list.empty()) AppendMenuW(menu, MF_STRING | MF_GRAYED, 0, L"No Minecraft window found");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr); AppendMenuW(menu, MF_STRING, 10000, L"Refresh windows");
        POINT point{int(keyRect.X * dpi), int((keyRect.Y + keyRect.Height) * dpi)}; ClientToScreen(window, &point);
        int chosen = TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY, point.x, point.y, 0, window, nullptr);
        DestroyMenu(menu);
        update([&](auto& v) { v.games = list;
            if (chosen > 0 && chosen < 10000) v.selected = list[size_t(chosen - 1)].pid;
            else if (list.size() == 1) v.selected = list.front().pid;
            else if (std::none_of(list.begin(), list.end(), [&](auto& g) { return g.pid == v.selected; })) v.selected = 0;
            if (v.screen == phase::done) v.screen = phase::ready;
            v.error = false;
            v.message = v.selected ? L"Ready to launch." : L"Select a Minecraft window.";
        });
    }
    void action() {
        const auto v = snapshot(); if (busy.load()) return;
        if (v.screen == phase::login) { login(); return; }
        if (v.screen == phase::done) return;
        const auto selected = std::find_if(v.games.begin(), v.games.end(), [&](auto& item) { return item.pid == v.selected; });
        if (selected == v.games.end()) { select_game(); return; }
        const auto game = *selected;
        update([](auto& value) { value.screen = phase::launching; value.error = false; value.total = 0;
            value.message = L"Preparing launch..."; });
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
        MoveWindow(edit, int(471 * dpi), int(164 * dpi), int(184 * dpi), int(27 * dpi), TRUE);
        if (inputFont) DeleteObject(inputFont);
        inputFont = CreateFontW(-int(13 * dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
        SendMessageW(edit, WM_SETFONT, WPARAM(inputFont), TRUE);
    }
    LRESULT CALLBACK procedure(HWND handle, UINT message, WPARAM w, LPARAM l) {
        switch (message) {
        case WM_CREATE:
            window = handle; dpi = GetDpiForWindow(handle) / 96.0f;
            editBrush = CreateSolidBrush(RGB(23, 26, 38));
            edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD,
                0, 0, 0, 0, handle, reinterpret_cast<HMENU>(100), GetModuleHandleW(nullptr), nullptr);
            SendMessageW(edit, EM_SETLIMITTEXT, 128, 0); SendMessageW(edit, EM_SETPASSWORDCHAR, L'•', 0);
            SendMessageW(edit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"Enter license key"));
            SetWindowSubclass(edit, input_proc, 1, 0); resize_controls();
            SetTimer(handle, 1, 100, nullptr); SetFocus(edit); return 0;
        case WM_CTLCOLOREDIT:
            SetTextColor(reinterpret_cast<HDC>(w), RGB(247, 247, 250)); SetBkColor(reinterpret_cast<HDC>(w), RGB(23, 26, 38));
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
            const auto current = snapshot();
            const bool signedIn = current.screen == phase::ready || current.screen == phase::done;
            int next = inside(RectF(673, 13, 30, 30), x, y) ? 3 :
                inside(RectF(716, 13, 30, 30), x, y) ? 4 :
                signedIn && !busy.load() && inside(RectF(592, 15, 71, 29), x, y) ? 5 :
                !busy.load() && inside(keyRect, x, y) ? 1 :
                !busy.load() && current.screen != phase::done && inside(actionRect, x, y) ? 2 : 0;
            if (hover != next) { hover = next; InvalidateRect(handle, nullptr, FALSE); }
            SetCursor(LoadCursorW(nullptr, hover ? IDC_HAND : IDC_ARROW));
            TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, handle, 0}; TrackMouseEvent(&tracking); return 0;
        }
        case WM_MOUSELEAVE: hover = 0; InvalidateRect(handle, nullptr, FALSE); return 0;
        case WM_LBUTTONUP: {
            float x = float(short(LOWORD(l))) / dpi, y = float(short(HIWORD(l))) / dpi;
            if (inside(RectF(716, 13, 30, 30), x, y)) SendMessageW(handle, WM_CLOSE, 0, 0);
            else if (inside(RectF(673, 13, 30, 30), x, y)) ShowWindow(handle, SW_MINIMIZE);
            else if (inside(RectF(592, 15, 71, 29), x, y) &&
                (snapshot().screen == phase::ready || snapshot().screen == phase::done) && !busy.load()) {
                begin([] { auth::controlled_shutdown(); update([](auto& value) { value = {}; }); });
            }
            else if (inside(actionRect, x, y)) action();
            else if (inside(keyRect, x, y)) {
                if (snapshot().screen == phase::login) {
                    if (x >= 661 && !busy.load()) { reveal = !reveal; SendMessageW(edit, EM_SETPASSWORDCHAR, reveal ? 0 : L'•', 0); InvalidateRect(edit, nullptr, TRUE); }
                    else SetFocus(edit);
                } else if (!busy.load()) select_game();
            }
            return 0;
        }
        case WM_NCHITTEST: {
            POINT point{short(LOWORD(l)), short(HIWORD(l))}; ScreenToClient(handle, &point);
            if (point.y < int(56 * dpi) && point.x < int(580 * dpi)) return HTCAPTION;
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
    if (argv && argc == 3 && (!wcscmp(argv[1], L"--preview") ||
        !wcscmp(argv[1], L"--preview-session"))) {
        if (!wcscmp(argv[1], L"--preview-session")) {
            state.screen = phase::ready;
            state.games.push_back({12345, L"Minecraft Window"});
            state.selected = 12345;
            state.message = L"Ready to launch.";
        }
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
    HWND handle = CreateWindowExW(WS_EX_APPWINDOW, cls.lpszClassName, L"OneStep", WS_POPUP | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
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
