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
    RectF keyRect(456, 240, 300, 49), actionRect(456, 305, 300, 47);
    Color bg(255, 12, 14, 20), fg(255, 241, 242, 247), muted(255, 116, 122, 140);
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
    void draw(Graphics& g, const view& v, float animation) {
        g.SetSmoothingMode(SmoothingModeAntiAlias);
        g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
        g.Clear(bg);

        const Color surface(255, 20, 22, 30);
        const Color inputFill(255, 24, 27, 36);
        const Color border(255, 43, 47, 60);
        const Color tertiary(255, 147, 151, 166);
        const Color accent(255, 215, 105, 133);
        const Color success(255, 113, 206, 166);
        const bool isLogin = v.screen == phase::login || v.screen == phase::working;
        const bool isBusy = busy.load();
        const bool isDone = v.screen == phase::done;
        const bool canSubmit = !isBusy && !isDone;
        const auto chosen = std::find_if(v.games.begin(), v.games.end(),
            [&](const auto& item) { return item.pid == v.selected; });
        const bool hasSelection = chosen != v.games.end();

        // Quiet architectural detail: a restrained halo and vertical guides.
        for (int i = 8; i >= 1; --i) {
            SolidBrush halo(Color(BYTE(2 + (8 - i)), 191, 69, 108));
            g.FillEllipse(&halo, 10.0f - i * 20.0f, 92.0f - i * 16.0f,
                332.0f + i * 39.0f, 246.0f + i * 29.0f);
        }
        Pen guide(Color(255, 28, 30, 40));
        g.DrawLine(&guide, 404.0f, 84.0f, 404.0f, 456.0f);
        g.DrawLine(&guide, 26.0f, 64.0f, 794.0f, 64.0f);
        g.DrawLine(&guide, 26.0f, 469.0f, 794.0f, 469.0f);

        // Native titlebar remains draggable and the existing window actions remain intact.
        SolidBrush brand(accent);
        g.FillEllipse(&brand, 28.0f, 26.0f, 8.0f, 8.0f);
        text(g, L"ONESTEP", RectF(45, 16, 115, 30), 14, fg, false, false, true);
        text(g, L" /  LAUNCHER", RectF(148, 18, 132, 28), 10.5f, muted, false, true);
        if (v.screen == phase::ready || v.screen == phase::done) {
            rounded_box(g, RectF(636, 17, 68, 28), 7,
                hover == 5 ? Color(255, 36, 38, 49) : Color(255, 19, 21, 29),
                hover == 5 ? border : Color(255, 29, 32, 42));
            text(g, L"Çıkış", RectF(636, 17, 68, 27), 11,
                hover == 5 ? fg : tertiary, true);
        }
        if (hover == 3) rounded_box(g, RectF(728, 12, 30, 30), 7, Color(255, 34, 37, 48), border);
        if (hover == 4) rounded_box(g, RectF(770, 12, 30, 30), 7, Color(255, 68, 30, 43), border);
        Pen chrome(Color(255, 157, 161, 176), 1.4f);
        g.DrawLine(&chrome, 739, 28, 748, 28);
        g.DrawLine(&chrome, 780, 22, 788, 30);
        g.DrawLine(&chrome, 788, 22, 780, 30);

        // Editorial left panel: clear hierarchy, without repeating a giant logo.
        text(g, L"YOUR NEXT STEP", RectF(40, 107, 320, 25), 10, accent, false, true, true);
        text(g, L"ONE STEP", RectF(37, 155, 360, 65), 45, fg, false, false, true);
        text(g, L"AHEAD.", RectF(37, 213, 360, 65), 45, fg, false, false, true);
        SolidBrush stripe(accent);
        g.FillRectangle(&stripe, 40.0f, 298.0f, 32.0f, 3.0f);
        const wchar_t* helper = isLogin
            ? L"Lisansını doğrula.\nGerisini OneStep halleder."
            : isDone ? L"Oturum başlatıldı.\nArtık Minecraft'a dönebilirsin."
            : L"Client hazır olduğunda\nMinecraft oturumunu seç.";
        text(g, helper, RectF(40, 312, 330, 53), 12.2f, tertiary);

        const int stage = v.screen == phase::login ? 0 :
            v.screen == phase::working ? 1 :
            v.screen == phase::done ? 3 : 2;
        const wchar_t* stages[] = {L"ACCESS", L"VERIFY", L"LAUNCH"};
        for (int i = 0; i < 3; ++i) {
            const float x = 40.0f + float(i) * 119.0f;
            const bool reached = stage >= i + 1;
            SolidBrush track(reached ? accent : Color(255, 54, 58, 69));
            g.FillRectangle(&track, x, 405.0f, 103.0f, 3.0f);
            text(g, std::wstring(L"0") + std::to_wstring(i + 1),
                RectF(x, 417.0f, 25.0f, 27.0f), 11.0f,
                reached ? accent : muted, false, true, true);
            text(g, stages[i], RectF(x + 29.0f, 418.0f, 77.0f, 26.0f),
                9.0f, reached ? fg : muted, false, true);
        }

        // Action surface: all real controls live in one compact, readable panel.
        rounded_box(g, RectF(423, 86, 368, 369), 16, surface, border);
        text(g, isLogin ? L"SECURE ACCESS" : isDone ? L"SESSION ACTIVE" : L"GAME SESSION",
            RectF(456, 113, 222, 23), 10.5f, accent, false, true, true);
        rounded_box(g, RectF(691, 110, 68, 24), 11,
            Color(255, 34, 40, 46), Color(255, 48, 63, 65));
        text(g, L"WIN x64", RectF(691, 109, 68, 25), 9.5f, success, true, true, true);

        const wchar_t* headline =
            v.screen == phase::login ? L"Tekrar hoş geldin." :
            v.screen == phase::working ? L"Kontrol ediliyor." :
            v.screen == phase::ready ? L"Oturumunu seç." :
            v.screen == phase::launching ? L"Başlatılıyor." : L"Her şey hazır.";
        text(g, headline, RectF(456, 151, 302, 45), 24, fg, false, false, true);
        text(g, isLogin ? L"Devam etmek için lisans anahtarını gir."
                : isDone ? L"Client oturumu başarıyla hazırlandı."
                : L"Açık Minecraft penceresini seç.",
            RectF(456, 190, 302, 26), 10.5f, tertiary);

        text(g, isLogin ? L"LİSANS ANAHTARI" : L"MINECRAFT OTURUMU",
            RectF(456, 219, 300, 19), 9.5f, tertiary, false, true, true);
        rounded_box(g, keyRect, 9, inputFill,
            hover == 1 && !isBusy ? Color(255, 118, 75, 96) : border);
        if (isLogin) {
            text(g, L">", RectF(468, 247, 18, 30), 17, accent, true, true);
            if (preview) text(g, L"••••  ••••  ••••  ••••",
                RectF(488, 249, 196, 28), 12, tertiary, false, true);
            text(g, reveal ? L"GİZLE" : L"GÖSTER",
                RectF(692, 248, 60, 28), 9.5f,
                hover == 1 && !isBusy ? fg : tertiary, true, true, true);
        } else {
            const auto caption = hasSelection
                ? chosen->title + L"  /  PID " + std::to_wstring(chosen->pid)
                : L"Minecraft seç veya listeyi yenile";
            text(g, caption, RectF(471, 246, 253, 34), 11.5f,
                hasSelection ? fg : tertiary);
            text(g, L"v", RectF(727, 247, 24, 32), 12, accent, true, true, true);
        }

        const Color button = !canSubmit ? Color(255, 58, 43, 54) :
            hover == 2 ? Color(255, 232, 118, 149) : Color(255, 200, 85, 119);
        rounded_box(g, actionRect, 10, button,
            canSubmit ? Color(255, 234, 129, 157) : Color(255, 72, 57, 67));
        std::wstring label =
            v.screen == phase::login ? L"Giriş yap   →" :
            v.screen == phase::done ? L"Client çalışıyor" :
            v.screen == phase::ready && !hasSelection ? L"Minecraft seç   →" :
            L"Client'i başlat   →";
        if (isBusy) {
            label = v.screen == phase::launching ? L"Başlatılıyor" : L"Doğrulanıyor";
            label += std::wstring(1 + int(animation * 2.0f) % 3, L'.');
        }
        text(g, label, RectF(456, 307, 300, 45), 13,
            canSubmit ? Color(255, 255, 248, 251) : Color(255, 190, 164, 175),
            true, false, true);

        const Color statusColor = v.error ? Color(255, 248, 133, 149)
            : isBusy ? Color(255, 199, 203, 214) : success;
        rounded_box(g, RectF(456, 367, 300, 59), 8,
            Color(255, 25, 29, 37), Color(255, 40, 45, 55));
        SolidBrush statusDot(statusColor);
        g.FillEllipse(&statusDot, 469.0f, 382.0f, 6.0f, 6.0f);
        text(g, v.error ? L"İŞLEM BAŞARISIZ" : isBusy ? L"İŞLEM DEVAM EDİYOR" :
            isDone ? L"OTURUM AKTİF" : L"DURUM",
            RectF(482, 371, 258, 23), 9, statusColor, false, true, true);
        text(g, v.message, RectF(469, 389, 274, 31), 10.1f, tertiary);
        if (v.total != 0 && isBusy) {
            const float ratio = float(std::min(v.received, v.total)) / float(v.total);
            SolidBrush track(Color(255, 47, 48, 60)), progress(accent);
            g.FillRectangle(&track, 456.0f, 439.0f, 300.0f, 3.0f);
            g.FillRectangle(&progress, 456.0f, 439.0f, 300.0f * ratio, 3.0f);
        }

        text(g, L"FABRIC 1.21.1  /  WINDOWS x64", RectF(28, 476, 365, 26),
            10, muted, false, true);
        if (!v.release.version.empty() && !isLogin) {
            text(g, L"v" + launcher::wide(v.release.version), RectF(410, 476, 190, 26),
                9.5f, muted, true, true);
        }
        text(g, L"TLS  /  SIGNED RELEASE", RectF(598, 476, 194, 26),
            9.5f, success, true, true);
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
        MoveWindow(edit, int(488 * dpi), int(252 * dpi), int(196 * dpi), int(25 * dpi), TRUE);
        if (inputFont) DeleteObject(inputFont);
        inputFont = CreateFontW(-int(14 * dpi), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH, L"Consolas");
        SendMessageW(edit, WM_SETFONT, WPARAM(inputFont), TRUE);
    }
    LRESULT CALLBACK procedure(HWND handle, UINT message, WPARAM w, LPARAM l) {
        switch (message) {
        case WM_CREATE:
            window = handle; dpi = GetDpiForWindow(handle) / 96.0f;
            editBrush = CreateSolidBrush(RGB(24, 27, 36));
            edit = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD,
                0, 0, 0, 0, handle, reinterpret_cast<HMENU>(100), GetModuleHandleW(nullptr), nullptr);
            SendMessageW(edit, EM_SETLIMITTEXT, 128, 0); SendMessageW(edit, EM_SETPASSWORDCHAR, L'•', 0);
            SetWindowSubclass(edit, input_proc, 1, 0); resize_controls();
            SetTimer(handle, 1, 100, nullptr); SetFocus(edit); return 0;
        case WM_CTLCOLOREDIT:
            SetTextColor(reinterpret_cast<HDC>(w), RGB(241, 242, 247)); SetBkColor(reinterpret_cast<HDC>(w), RGB(24, 27, 36));
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
                    if (x >= 688 && !busy.load()) { reveal = !reveal; SendMessageW(edit, EM_SETPASSWORDCHAR, reveal ? 0 : L'•', 0); InvalidateRect(edit, nullptr, TRUE); }
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
