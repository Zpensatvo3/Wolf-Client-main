#include "launcher.hpp"
#include "ui.hpp"
#include "ui_messages.hpp"
#include "../src/auth/auth.hpp"
#include "../src/auth/config.hpp"
#include "../src/auth/launcher_protocol.hpp"
#include <dwmapi.h>
#include <commctrl.h>
#include <shellapi.h>
#include <thread>
#include <mutex>
#include <memory>
#include <fstream>
#include <stdexcept>
#include <cwctype>

using namespace Gdiplus;
namespace {
    namespace ui=launcher::ui;
    constexpr UINT changed=WM_APP+1;
    HWND window{},edit{},tooltip{}; HFONT inputFont{}; HBRUSH editBrush{};
    float dpi=1;
    std::atomic_bool cancel{false},busy{false};
    std::thread worker;
    enum class phase { login,working,ready,launching,done };
    struct view {
        phase screen=phase::login; std::wstring message=L"Ready to sign in.";
        bool error=false; uint64_t received=0,total=0; launcher::release release;
        std::vector<launcher::game> games; DWORD selected=0;
    };
    view state; std::mutex stateMutex;
    ui::target hover=ui::target::none,pressed=ui::target::none,keyboard=ui::target::none;
    bool reveal=false,offline=false;
    std::wstring statusDetail;
    bool login_screen(const view& v) { return v.screen==phase::login || v.screen==phase::working; }
    bool signed_in(const view& v) { return v.screen==phase::ready || v.screen==phase::launching || v.screen==phase::done; }
    void update(const std::function<void(view&)>& fn) {
        { std::lock_guard lock(stateMutex); fn(state); }
        if (window) PostMessageW(window,changed,0,0);
    }
    view snapshot() { std::lock_guard lock(stateMutex); return state; }
    ui::target hit(float x,float y) {
        const auto v=snapshot(); return ui::hit(x,y,login_screen(v),signed_in(v),busy.load());
    }
    ui::presentation presentation(const view& v,float animation) {
        ui::presentation p;
        p.login=login_screen(v); p.signed_in=signed_in(v); p.busy=busy.load(); p.done=v.screen==phase::done;
        p.error=v.error; p.reveal=reveal; p.hover=hover; p.keyboard=keyboard;
        p.input_focused=edit && GetFocus()==edit; p.animation=animation;
        p.message=ui::english(v.message); p.received=v.received; p.total=v.total;
        const auto selected=std::find_if(v.games.begin(),v.games.end(),[&](const auto& game) { return game.pid==v.selected; });
        if (selected!=v.games.end()) p.selected=selected->title;
        return p;
    }
    void draw(Graphics& g,const view& v,float animation) { ui::draw(g,presentation(v,animation)); }
    int pixels(float logical) { return static_cast<int>(std::lround(logical*dpi)); }
    RECT physical(RectF r) { return {pixels(r.X),pixels(r.Y),pixels(r.GetRight()),pixels(r.GetBottom())}; }
    void resize_controls() {
        const auto r=physical(ui::bounds.input());
        MoveWindow(edit,r.left,r.top,r.right-r.left,r.bottom-r.top,TRUE);
        auto font=CreateFontW(-pixels(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
        if (font) {
            SendMessageW(edit,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
            if (inputFont) DeleteObject(inputFont);
            inputFont=font;
        }
        if (tooltip) {
            TOOLINFOW tool{sizeof(tool)}; tool.hwnd=window; tool.uId=1; tool.rect=physical(ui::bounds.status);
            SendMessageW(tooltip,TTM_NEWTOOLRECTW,0,reinterpret_cast<LPARAM>(&tool));
            SendMessageW(tooltip,TTM_SETMAXTIPWIDTH,0,pixels(340));
        }
        RECT client{}; GetClientRect(window,&client);
        const auto region=CreateRoundRectRgn(0,0,client.right+1,client.bottom+1,pixels(26),pixels(26));
        if (region && !SetWindowRgn(window,region,TRUE)) DeleteObject(region);
        InvalidateRect(window,nullptr,FALSE);
    }
    void controls() {
        const auto v=snapshot();
        const bool wasVisible=IsWindowVisible(edit)!=FALSE;
        ShowWindow(edit,login_screen(v) ? SW_SHOW : SW_HIDE);
        EnableWindow(edit,!busy.load());
        statusDetail=ui::english(v.message);
        if (tooltip) {
            TOOLINFOW tool{sizeof(tool)}; tool.hwnd=window; tool.uId=1; tool.lpszText=statusDetail.data();
            SendMessageW(tooltip,TTM_UPDATETIPTEXTW,0,reinterpret_cast<LPARAM>(&tool));
        }
        if (!wasVisible && login_screen(v) && !busy.load() && GetForegroundWindow()==window) SetFocus(edit);
        InvalidateRect(window,nullptr,FALSE);
    }
    void begin(const std::function<void()>& action) {
        if (busy.exchange(true)) return;
        if (worker.joinable()) worker.join();
        cancel.store(false);
        try {
            worker=std::thread([action] {
                try { action(); }
                catch (const std::exception& error) {
                    update([&](auto& v) { v.error=true; v.message=launcher::wide(error.what());
                        v.screen=auth::is_authenticated() && !v.release.path.empty() ? phase::ready : phase::login; });
                } catch (...) {
                    update([](auto& v) { v.error=true; v.message=L"The operation could not be completed. Please retry."; v.screen=phase::login; });
                }
                busy.store(false); if (window) PostMessageW(window,changed,0,0);
            });
        } catch (...) {
            busy.store(false);
            update([](auto& v) { v.error=true; v.screen=phase::login; v.message=L"The background task could not be started. Please retry."; });
        }
        controls();
    }
    void login() {
        if (busy.load()) return;
        wchar_t input[130]{}; GetWindowTextW(edit,input,130);
        const bool empty=std::all_of(input,input+wcslen(input),[](wchar_t c) { return std::iswspace(c)!=0; });
        if (empty) {
            SecureZeroMemory(input,sizeof(input));
            update([](auto& v) { v.error=true; v.message=L"Enter a license key."; }); SetFocus(edit); return;
        }
        if (offline) { SecureZeroMemory(input,sizeof(input)); return; }
        auto key=std::make_shared<std::string>(launcher::narrow(input)); SecureZeroMemory(input,sizeof(input));
        SetWindowTextW(edit,L"");
        update([](auto& v) { v.screen=phase::working; v.error=false; v.message=L"Checking license..."; v.received=v.total=0; });
        begin([key] {
            struct key_guard { std::string& value; ~key_guard() { auth::launch::wipe(value); } } secret{*key};
            auth::controlled_shutdown();
            auto result=auth::authenticate(*key,auth::config::server_domain);
            auth::launch::wipe(*key); auth::launch::wipe(result.session_token);
            if (!result.success) throw std::runtime_error(result.message);
            auth::start_heartbeat_monitor(auth::config::server_domain);
            update([](auto& v) { v.message=L"Checking for updates..."; });
            auto release=launcher::manifest();
            update([](auto& v) { v.message=L"Downloading client..."; });
            launcher::download(release,cancel,[](auto read,auto total) {
                update([&](auto& v) { v.received=read; v.total=total;
                    v.message=L"Downloading client... "+std::to_wstring(total ? read*100/total : 0)+L"%"; });
            });
            auto list=launcher::games();
            update([&](auto& v) { v.screen=phase::ready; v.release=release; v.games=list;
                v.selected=list.size()==1 ? list.front().pid : 0;
                v.message=list.empty() ? L"Open Minecraft, then refresh." : v.selected ? L"Ready to launch." : L"Select a Minecraft window."; });
        });
    }
    void apply_game_list(const std::vector<launcher::game>& list,int chosen) {
        update([&](auto& v) {
            v.games=list;
            if (chosen>0 && chosen<=static_cast<int>(list.size())) v.selected=list[size_t(chosen-1)].pid;
            else if (list.size()==1) v.selected=list.front().pid;
            else if (std::none_of(list.begin(),list.end(),[&](const auto& game) { return game.pid==v.selected; })) v.selected=0;
            if (v.screen==phase::done) v.screen=phase::ready;
            v.error=false; v.message=list.empty() ? L"Open Minecraft, then refresh." : v.selected ? L"Ready to launch." : L"Select a Minecraft window.";
        });
    }
    struct menu_entry { std::wstring label; UINT id; bool enabled=true,selected=false; };
    void select_game() {
        if (busy.load() || offline) return;
        auto list=launcher::games(); if (list.size()>9998) list.resize(9998);
        std::vector<menu_entry> entries; entries.reserve(list.size()+2);
        const auto current=snapshot().selected;
        for (size_t i=0;i<list.size();++i)
            entries.push_back({list[i].title+L"  ["+std::to_wstring(list[i].pid)+L"]",UINT(i+1),true,list[i].pid==current});
        if (list.empty()) entries.push_back({L"No Minecraft windows found",0,false,false});
        entries.push_back({L"Refresh windows",10000,true,false});
        HMENU menu=CreatePopupMenu(); if (!menu) return;
        const auto brush=CreateSolidBrush(RGB(20,22,32));
        MENUINFO info{sizeof(info)}; info.fMask=MIM_BACKGROUND|MIM_STYLE; info.hbrBack=brush; info.dwStyle=MNS_CHECKORBMP;
        SetMenuInfo(menu,&info);
        for (const auto& item:entries)
            AppendMenuW(menu,MF_OWNERDRAW|(item.enabled ? MF_ENABLED : MF_GRAYED),item.id,reinterpret_cast<LPCWSTR>(&item));
        POINT point{pixels(ui::bounds.key.X),pixels(ui::bounds.key.GetBottom()+5)}; ClientToScreen(window,&point);
        const int chosen=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,0,window,nullptr);
        DestroyMenu(menu); if (brush) DeleteObject(brush);
        apply_game_list(list,chosen>0 && chosen<10000 ? chosen : 0); hover=ui::target::none;
    }
    void sign_out() {
        if (busy.load()) return;
        reveal=false; keyboard=ui::target::none; SetWindowTextW(edit,L"");
        SendMessageW(edit,EM_SETPASSWORDCHAR,L'\x2022',0);
        if (offline) update([](auto& v) { v={}; });
        else begin([] { auth::controlled_shutdown(); update([](auto& v) { v={}; }); });
    }
    void action() {
        const auto v=snapshot(); if (busy.load()) return;
        if (v.screen==phase::login) { login(); return; }
        if (v.screen==phase::done || offline) return;
        const auto selected=std::find_if(v.games.begin(),v.games.end(),[&](const auto& item) { return item.pid==v.selected; });
        if (selected==v.games.end()) {
            update([](auto& value) { value.error=true; value.message=L"Select a Minecraft window."; }); select_game(); return;
        }
        const auto game=*selected;
        update([](auto& value) { value.screen=phase::launching; value.error=false; value.total=0; value.message=L"Preparing launch..."; });
        begin([game,release=v.release] {
            launcher::launch(game,release,cancel,[](const auto& message) { update([&](auto& value) { value.message=ui::english(message); }); });
            update([](auto& value) { value.screen=phase::done; value.message=L"Client is running."; });
        });
    }
    void activate(ui::target item) {
        if (item==ui::target::close) { SendMessageW(window,WM_CLOSE,0,0); return; }
        if (item==ui::target::minimize) { ShowWindow(window,SW_MINIMIZE); return; }
        if (busy.load()) return;
        const auto v=snapshot();
        switch (item) {
        case ui::target::action: action(); break;
        case ui::target::signout: if (signed_in(v)) sign_out(); break;
        case ui::target::eye:
            if (v.screen==phase::login) {
                reveal=!reveal; SendMessageW(edit,EM_SETPASSWORDCHAR,reveal ? 0 : L'\x2022',0);
                InvalidateRect(edit,nullptr,TRUE); InvalidateRect(window,nullptr,FALSE);
            } break;
        case ui::target::input:
            if (login_screen(v)) { keyboard=ui::target::none; SetFocus(edit); } else select_game();
            break;
        default: break;
        }
    }
    LRESULT CALLBACK input_proc(HWND control,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR) {
        if (message==WM_KEYDOWN && w==VK_RETURN) { action(); return 0; }
        if (message==WM_KEYDOWN && w=='A' && GetKeyState(VK_CONTROL)<0) { SendMessageW(control,EM_SETSEL,0,-1); return 0; }
        if (message==WM_KEYDOWN && w==VK_TAB) {
            keyboard=GetKeyState(VK_SHIFT)<0 ? ui::target::action : ui::target::eye;
            SetFocus(window); InvalidateRect(window,nullptr,FALSE); return 0;
        }
        if (message==WM_SETFOCUS || message==WM_KILLFOCUS) InvalidateRect(window,nullptr,FALSE);
        if (message==WM_PRINTCLIENT || message==WM_PRINT) {
            // Standard EDIT painting may skip off-desktop windows. Supply its
            // print contract using the real EDIT contents/font/password char;
            // this is not an independent UI value or a fixed bullet overlay.
            const auto dc=reinterpret_cast<HDC>(w); if (!dc) return 0;
            const int saved=SaveDC(dc); RECT rect{}; GetClientRect(control,&rect);
            FillRect(dc,&rect,editBrush); IntersectClipRect(dc,rect.left,rect.top,rect.right,rect.bottom);
            SelectObject(dc,inputFont); SetBkMode(dc,TRANSPARENT);
            wchar_t characters[130]{}; const int length=GetWindowTextW(control,characters,130);
            if (length) {
                const auto mask=static_cast<wchar_t>(SendMessageW(control,EM_GETPASSWORDCHAR,0,0));
                if (mask) std::fill_n(characters,length,mask);
                SetTextColor(dc,busy.load() ? RGB(140,137,153) : RGB(239,239,247));
                DrawTextW(dc,characters,length,&rect,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
            } else {
                SetTextColor(dc,RGB(138,141,160));
                DrawTextW(dc,L"Enter license key",-1,&rect,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
            }
            SecureZeroMemory(characters,sizeof(characters)); RestoreDC(dc,saved); return 0;
        }
        if (message==WM_PAINT) {
            const auto result=DefSubclassProc(control,message,w,l);
            if (GetWindowTextLengthW(control)==0) {
                const bool own_dc=message==WM_PAINT && !w;
                const auto dc=own_dc ? GetDC(control) : reinterpret_cast<HDC>(w);
                if (dc) {
                    const int saved=SaveDC(dc); SelectObject(dc,inputFont); SetBkMode(dc,TRANSPARENT);
                    SetTextColor(dc,RGB(138,141,160)); RECT rect{}; GetClientRect(control,&rect);
                    DrawTextW(dc,L"Enter license key",-1,&rect,DT_LEFT|DT_SINGLELINE|DT_VCENTER|DT_NOPREFIX);
                    RestoreDC(dc,saved); if (own_dc) ReleaseDC(control,dc);
                }
            }
            return result;
        }
        return DefSubclassProc(control,message,w,l);
    }
    LRESULT CALLBACK procedure(HWND handle,UINT message,WPARAM w,LPARAM l) {
        switch (message) {
        case WM_CREATE: {
            window=handle; dpi=GetDpiForWindow(handle)/96.f; editBrush=CreateSolidBrush(RGB(23,25,36));
            edit=CreateWindowExW(0,L"EDIT",L"",WS_CHILD|WS_VISIBLE|WS_TABSTOP|ES_AUTOHSCROLL|ES_PASSWORD,
                0,0,0,0,handle,reinterpret_cast<HMENU>(100),GetModuleHandleW(nullptr),nullptr);
            if (!edit) return -1;
            SendMessageW(edit,EM_SETLIMITTEXT,128,0); SendMessageW(edit,EM_SETPASSWORDCHAR,L'\x2022',0);
            SendMessageW(edit,EM_SETMARGINS,EC_LEFTMARGIN|EC_RIGHTMARGIN,0);
            SetWindowSubclass(edit,input_proc,1,0);
            tooltip=CreateWindowExW(WS_EX_TOPMOST,TOOLTIPS_CLASSW,nullptr,WS_POPUP|TTS_ALWAYSTIP|TTS_NOPREFIX,
                CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,handle,nullptr,GetModuleHandleW(nullptr),nullptr);
            if (tooltip) {
                TOOLINFOW tool{sizeof(tool)}; tool.uFlags=TTF_SUBCLASS; tool.hwnd=handle; tool.uId=1;
                tool.rect=physical(ui::bounds.status); tool.lpszText=const_cast<wchar_t*>(L"Ready to sign in.");
                SendMessageW(tooltip,TTM_ADDTOOLW,0,reinterpret_cast<LPARAM>(&tool));
                SendMessageW(tooltip,TTM_SETMAXTIPWIDTH,0,pixels(340));
            }
            resize_controls(); if (!offline) { SetTimer(handle,1,80,nullptr); SetFocus(edit); }
            return 0;
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC:
            if (reinterpret_cast<HWND>(l)==edit) {
                SetTextColor(reinterpret_cast<HDC>(w),busy.load() ? RGB(140,137,153) : RGB(239,239,247));
                SetBkColor(reinterpret_cast<HDC>(w),RGB(23,25,36)); return reinterpret_cast<LRESULT>(editBrush);
            } break;
        case WM_ERASEBKGND: return 1;
        case WM_PAINT: {
            PAINTSTRUCT paint{}; HDC dc=BeginPaint(handle,&paint); RECT r{}; GetClientRect(handle,&r);
            if (r.right>0 && r.bottom>0) {
                Bitmap buffer(r.right,r.bottom,PixelFormat32bppPARGB);
                { Graphics graphics(&buffer); graphics.ScaleTransform(dpi,dpi);
                    draw(graphics,snapshot(),float(GetTickCount64()%100000)/1000.f); }
                Graphics target(dc); target.DrawImage(&buffer,0,0);
            }
            EndPaint(handle,&paint); return 0;
        }
        case WM_TIMER: if (busy.load()) InvalidateRect(handle,nullptr,FALSE); return 0;
        case changed: controls(); return 0;
        case WM_MEASUREITEM: {
            auto item=reinterpret_cast<MEASUREITEMSTRUCT*>(l);
            if (item->CtlType==ODT_MENU) { item->itemWidth=UINT(pixels(304)); item->itemHeight=UINT(pixels(38)); return TRUE; }
            break;
        }
        case WM_DRAWITEM: {
            const auto item=reinterpret_cast<DRAWITEMSTRUCT*>(l);
            if (item->CtlType!=ODT_MENU || !item->itemData) break;
            const auto& entry=*reinterpret_cast<const menu_entry*>(item->itemData);
            Graphics g(item->hDC); g.ScaleTransform(dpi,dpi); g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
            const RectF r(float(item->rcItem.left)/dpi,float(item->rcItem.top)/dpi,
                float(item->rcItem.right-item->rcItem.left)/dpi,float(item->rcItem.bottom-item->rcItem.top)/dpi);
            SolidBrush base(Color(255,20,22,32)); g.FillRectangle(&base,r);
            if (entry.enabled && (item->itemState&ODS_SELECTED))
                ui::panel(g,RectF(r.X+3,r.Y+2,r.Width-6,r.Height-4),7,Color(255,55,28,47),Color(255,46,25,41),Color(0,0,0,0));
            if (entry.selected) { SolidBrush accent(ui::pink); g.FillEllipse(&accent,r.X+13,r.Y+16,6.f,6.f); }
            ui::text(g,entry.label,RectF(r.X+29,r.Y,r.Width-43,r.Height),13.5f,entry.enabled ? ui::foreground : ui::muted);
            return TRUE;
        }
        case WM_MOUSEMOVE: {
            const auto next=hit(float(short(LOWORD(l)))/dpi,float(short(HIWORD(l)))/dpi);
            if (hover!=next) { hover=next; InvalidateRect(handle,nullptr,FALSE); }
            SetCursor(LoadCursorW(nullptr,hover==ui::target::input && login_screen(snapshot()) ? IDC_IBEAM : hover!=ui::target::none ? IDC_HAND : IDC_ARROW));
            TRACKMOUSEEVENT tracking{sizeof(tracking),TME_LEAVE,handle,0}; TrackMouseEvent(&tracking); return 0;
        }
        case WM_MOUSELEAVE: hover=ui::target::none; InvalidateRect(handle,nullptr,FALSE); return 0;
        case WM_LBUTTONDOWN:
            pressed=hit(float(short(LOWORD(l)))/dpi,float(short(HIWORD(l)))/dpi);
            if (pressed!=ui::target::none) SetCapture(handle);
            return 0;
        case WM_LBUTTONUP: {
            const auto item=hit(float(short(LOWORD(l)))/dpi,float(short(HIWORD(l)))/dpi);
            const auto original=pressed; pressed=ui::target::none;
            if (GetCapture()==handle) ReleaseCapture();
            if (original==item) activate(item);
            return 0;
        }
        case WM_CAPTURECHANGED: pressed=ui::target::none; return 0;
        case WM_ACTIVATEAPP:
            if (!w) { pressed=ui::target::none; hover=ui::target::none; if (GetCapture()==handle) ReleaseCapture(); }
            InvalidateRect(handle,nullptr,FALSE); break;
        case WM_KEYDOWN:
            if (w==VK_F5 && signed_in(snapshot())) { select_game(); return 0; }
            if (w==VK_TAB && !busy.load()) {
                if (login_screen(snapshot())) {
                    if (keyboard==ui::target::eye) keyboard=ui::target::action;
                    else { keyboard=ui::target::none; SetFocus(edit); }
                } else keyboard=keyboard==ui::target::input ? ui::target::action : keyboard==ui::target::action ? ui::target::signout : ui::target::input;
                InvalidateRect(handle,nullptr,FALSE); return 0;
            }
            if (w==VK_RETURN || w==VK_SPACE) { activate(keyboard==ui::target::none ? ui::target::action : keyboard); return 0; }
            break;
        case WM_NCHITTEST: {
            POINT point{short(LOWORD(l)),short(HIWORD(l))}; ScreenToClient(handle,&point);
            const float x=point.x/dpi,y=point.y/dpi;
            return y>=0 && y<58 && hit(x,y)==ui::target::none ? HTCAPTION : HTCLIENT;
        }
        case WM_DPICHANGED: {
            dpi=HIWORD(w)/96.f; const auto r=reinterpret_cast<const RECT*>(l);
            SetWindowPos(handle,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
            resize_controls(); return 0;
        }
        case WM_CLOSE: cancel.store(true); ShowWindow(handle,SW_HIDE); PostQuitMessage(0); return 0;
        case WM_DESTROY: KillTimer(handle,1); PostQuitMessage(0); return 0;
        }
        return DefWindowProcW(handle,message,w,l);
    }
    HWND create_window(HINSTANCE instance,float scale) {
        WNDCLASSW cls{}; cls.hInstance=instance; cls.lpfnWndProc=procedure; cls.lpszClassName=L"Onestep.Launcher.Window";
        cls.hCursor=LoadCursorW(nullptr,IDC_ARROW); RegisterClassW(&cls);
        const int w=int(ui::width*scale),h=int(ui::height*scale);
        return CreateWindowExW(offline ? WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE : WS_EX_APPWINDOW,cls.lpszClassName,L"OneStep",WS_POPUP|WS_MINIMIZEBOX|WS_CLIPCHILDREN,
            offline ? -32000 : (GetSystemMetrics(SM_CXSCREEN)-w)/2,
            offline ? -32000 : (GetSystemMetrics(SM_CYSCREEN)-h)/2,w,h,nullptr,nullptr,instance,nullptr);
    }
    void destroy_controls(HWND handle) {
        if (handle) DestroyWindow(handle);
        window=nullptr; edit=nullptr; tooltip=nullptr;
        if (inputFont) DeleteObject(inputFont);
        inputFont=nullptr;
        if (editBrush) DeleteObject(editBrush);
        editBrush=nullptr;
    }
    void preview_state(const std::wstring& mode) {
        state={}; busy.store(false); reveal=false; SetWindowTextW(edit,L"");
        if (mode==L"session" || mode==L"session-long" || mode==L"session-empty" || mode==L"launching" || mode==L"done") {
            state.screen=mode==L"launching" ? phase::launching : mode==L"done" ? phase::done : phase::ready;
            state.message=mode==L"launching" ? L"Preparing launch..." : mode==L"done" ? L"Client is running." : L"Ready to launch.";
            state.games={{4242,mode==L"session-long" ? L"Minecraft 1.21.11 - Fabric - A very long window title that must never cover the selection arrow" : L"Minecraft Window"}};
            state.selected=4242;
            if (mode==L"session-empty") { state.games.clear(); state.selected=0; state.message=L"Open Minecraft, then refresh."; }
            busy.store(mode==L"launching");
        } else if (mode==L"working" || mode==L"download") {
            state.screen=phase::working; busy.store(true);
            state.message=mode==L"download" ? L"Downloading client... 64%" : L"Checking license...";
            if (mode==L"download") { state.received=64; state.total=100; }
        } else if (mode==L"error") {
            state.error=true; state.message=L"The client session was not confirmed. Check the sign-in status in Minecraft.";
        } else if (mode==L"login-filled" || mode==L"login-revealed") {
            SetWindowTextW(edit,L"OS-DEMO-NOT-A-REAL-LICENSE"); reveal=mode==L"login-revealed";
        }
        SendMessageW(edit,EM_SETPASSWORDCHAR,reveal ? 0 : L'\x2022',0); controls();
    }
    int save_preview(const std::wstring& path,const std::wstring& mode,float scale,HINSTANCE instance) {
        offline=true; const auto handle=create_window(instance,scale); if (!handle) return 1;
        dpi=scale; SetWindowPos(handle,nullptr,0,0,pixels(float(ui::width)),pixels(float(ui::height)),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
        resize_controls(); preview_state(mode);
        Bitmap image(pixels(float(ui::width)),pixels(float(ui::height)),PixelFormat32bppARGB);
        { Graphics graphics(&image); graphics.ScaleTransform(dpi,dpi); draw(graphics,snapshot(),.75f); }
        if (login_screen(snapshot())) {
            // Render the real Win32 EDIT, not decorative placeholder bullets.
            const auto r=physical(ui::bounds.input()); const int w=r.right-r.left,h=r.bottom-r.top;
            BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=w;
            info.bmiHeader.biHeight=-h; info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
            void* bits{}; const auto dc=CreateCompatibleDC(nullptr);
            const auto bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
            if (dc && bitmap) {
                const auto old=SelectObject(dc,bitmap); RECT area{0,0,w,h}; FillRect(dc,&area,editBrush);
                SendMessageW(edit,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT|PRF_ERASEBKGND);
                GdiFlush();
                // GDI/UXTheme text drawing does not maintain the alpha channel
                // of a 32-bit DIB. The native EDIT is an opaque control.
                for (int i=0;i<w*h;++i) static_cast<DWORD*>(bits)[i]|=0xff000000;
                SelectObject(dc,old);
                { Bitmap native(bitmap,nullptr); Graphics graphics(&image); graphics.DrawImage(&native,r.left,r.top,w,h); }
            }
            if (bitmap) DeleteObject(bitmap);
            if (dc) DeleteDC(dc);
        }
        UINT count{},bytes{}; GetImageEncodersSize(&count,&bytes); std::vector<unsigned char> data(bytes);
        auto encoders=reinterpret_cast<ImageCodecInfo*>(data.data()); GetImageEncoders(count,bytes,encoders);
        int result=1;
        for (UINT i=0;i<count;++i) if (!wcscmp(encoders[i].MimeType,L"image/png")) {
            result=image.Save(path.c_str(),&encoders[i].Clsid)==Ok ? 0 : 1; break;
        }
        destroy_controls(handle); return result;
    }
    int ui_test(const std::wstring& report,HINSTANCE instance) {
        offline=true; const auto handle=create_window(instance,1); std::ofstream log{std::filesystem::path(report)};
        int failures=0;
        const auto check=[&](bool valid,const char* name) { log<<(valid ? "PASS " : "FAIL ")<<name<<'\n'; if (!valid) ++failures; };
        if (!handle || !log) { destroy_controls(handle); return 1; }
        const auto click=[&](RectF r) {
            const auto position=MAKELPARAM(pixels(r.X+r.Width*.5f),pixels(r.Y+r.Height*.5f));
            SendMessageW(handle,WM_LBUTTONDOWN,MK_LBUTTON,position); SendMessageW(handle,WM_LBUTTONUP,0,position);
        };
        preview_state(L"login"); action(); check(snapshot().message==L"Enter a license key." && !busy.load(),"empty key stays offline");
        SetWindowTextW(edit,L"OS-DEMO-NOT-A-REAL-LICENSE");
        RECT format{},client{}; SendMessageW(edit,EM_GETRECT,0,reinterpret_cast<LPARAM>(&format)); GetClientRect(edit,&client);
        check(format.right>format.left && format.bottom>format.top && format.right<=client.right,"native input text rectangle is valid");
        click(ui::bounds.eye()); check(reveal && SendMessageW(edit,EM_GETPASSWORDCHAR,0,0)==0,"eye reveals real EDIT");
        check(GetWindowTextLengthW(edit)==26,"reveal keeps input text");
        click(ui::bounds.eye()); check(!reveal && SendMessageW(edit,EM_GETPASSWORDCHAR,0,0)==L'\x2022',"eye masks real EDIT");
        click(RectF(ui::bounds.key.X,ui::bounds.key.Y,35,40)); check(!reveal,"input padding does not toggle eye");
        busy.store(true); click(ui::bounds.eye()); check(!reveal,"busy blocks reveal"); busy.store(false);
        preview_state(L"session"); apply_game_list({{11,L"Minecraft A"},{22,L"Minecraft B"}},2);
        check(snapshot().selected==22,"multiple windows explicit selection");
        apply_game_list({{11,L"Minecraft A"},{22,L"Minecraft B"}},0); check(snapshot().selected==22,"refresh preserves selection");
        apply_game_list({},0); check(snapshot().selected==0 && snapshot().message==L"Open Minecraft, then refresh.","missing window clears stale selection");
        apply_game_list({{11,L"Minecraft A"}},0); check(snapshot().selected==11,"single window selection preserved");
        click(ui::bounds.signout); check(snapshot().screen==phase::login && !reveal && GetWindowTextLengthW(edit)==0,"sign out resets only preview state");
        Bitmap image(ui::width,ui::height); Graphics g(&image);
        check(ui::advance(g,L"Welcome back.",42)<=316,"hero fits measured GDI+ text width");
        for (const UINT resolution:{96u,120u,144u}) {
            RECT frame{0,0,MulDiv(ui::width,int(resolution),96),MulDiv(ui::height,int(resolution),96)};
            SendMessageW(handle,WM_DPICHANGED,MAKEWPARAM(resolution,resolution),reinterpret_cast<LPARAM>(&frame));
            RECT actual{}; GetWindowRect(edit,&actual); MapWindowPoints(nullptr,handle,reinterpret_cast<POINT*>(&actual),2);
            const auto expected=physical(ui::bounds.input());
            check(EqualRect(&actual,&expected)!=FALSE,"DPI message aligns real EDIT with drawing");
            check(hit(ui::bounds.eye().X+10,ui::bounds.eye().Y+10)==ui::target::eye,"DPI eye hitbox");
        }
        POINT drag{pixels(260),pixels(30)}; ClientToScreen(handle,&drag);
        check(SendMessageW(handle,WM_NCHITTEST,0,MAKELPARAM(drag.x,drag.y))==HTCAPTION,"blank header drags window");
        click(ui::bounds.minimize); check(IsIconic(handle)!=FALSE,"minimize hitbox"); ShowWindow(handle,SW_HIDE);
        click(ui::bounds.close); check(cancel.load(),"close cancels pending work");
        destroy_controls(handle); log<<"Offline only; no authentication, download, ticket or game load.\n"; return failures ? 1 : 0;
    }
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    ULONG_PTR token{}; GdiplusStartupInput input; if (GdiplusStartup(&token,&input,nullptr)!=Ok) return 1;
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_STANDARD_CLASSES}; InitCommonControlsEx(&common);
    int argc{}; auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);
    if (argv && argc>=3 && !wcscmp(argv[1],L"--preview")) {
        const float scale=argc>=5 ? std::clamp(float(_wtof(argv[4])),1.f,1.5f) : 1.f;
        const int result=save_preview(argv[2],argc>=4 ? argv[3] : L"login",scale,instance);
        LocalFree(argv); CoUninitialize(); GdiplusShutdown(token); return result;
    }
    if (argv && argc==3 && !wcscmp(argv[1],L"--ui-test")) {
        const int result=ui_test(argv[2],instance); LocalFree(argv); CoUninitialize(); GdiplusShutdown(token); return result;
    }
    if (argv) LocalFree(argv);
    HANDLE singleton=CreateMutexW(nullptr,FALSE,L"Local\\Onestep.Launcher.UI");
    if (!singleton || GetLastError()==ERROR_ALREADY_EXISTS) {
        if (singleton) CloseHandle(singleton); CoUninitialize(); GdiplusShutdown(token); return 0;
    }
    const auto handle=create_window(instance,GetDpiForSystem()/96.f);
    if (handle) {
        BOOL dark=TRUE; DwmSetWindowAttribute(handle,20,&dark,sizeof(dark));
        int corners=2; DwmSetWindowAttribute(handle,33,&corners,sizeof(corners));
        ShowWindow(handle,show); UpdateWindow(handle);
        MSG message{}; while (GetMessageW(&message,nullptr,0,0)>0) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    cancel.store(true); if (worker.joinable()) worker.join();
    auth::controlled_shutdown(); destroy_controls(handle);
    CloseHandle(singleton); CoUninitialize(); GdiplusShutdown(token); return handle ? 0 : 1;
}
