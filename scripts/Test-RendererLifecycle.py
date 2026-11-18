"""Run shipping renderer teardown/early shutdown and transactional hook setup offline."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parent.parent

def extract(s,signature):
    a=s.index(signature);b=s.index('{',a)+1;level=1
    while level:
        level+=(s[b]=='{')-(s[b]=='}');b+=1
    return s[a:b]
gui=(ROOT/'src/gui/gui.cpp').read_text()
prefix=gui[gui.index('static BOOL WINAPI detour_wglSwapBuffers'):gui.index('\n\tHGLRC last_context',gui.index('static BOOL WINAPI detour_wglSwapBuffers'))]
window_gate=gui[gui.index('\t// if already init, and window changed'):gui.index('\n\tif (run_once)',gui.index('\t// if already init, and window changed'))]
source=r'''
#include <atomic>
#include <cassert>
#include <cstdint>
#include <thread>
#include <type_traits>
#include <vector>
#include <iostream>
#include "src/gui/hook_transaction.hpp"
#include "src/modules/core/module.hpp"
static_assert(std::is_same_v<decltype(modules::module::enabled),std::atomic_bool>);
using HWND=void*;using HDC=void*;using HGLRC=void*;using LONG_PTR=std::intptr_t;using GLuint=unsigned;
using BOOL=int;using WNDPROC=void(*)();constexpr BOOL TRUE=1,FALSE=0;constexpr int GWLP_WNDPROC=1;
#define WINAPI
void* handle(int n){return reinterpret_cast<void*>(std::intptr_t(n));}
void proc_a(){};
std::atomic<HWND> window{nullptr};WNDPROC original_WndProc=nullptr;
struct ImGuiContext{};ImGuiContext context;
ImGuiContext* imGuiContext=nullptr;
HGLRC new_context=nullptr,original_context=nullptr,current_context=handle(20);HDC current_device=handle(2);
bool run_once=true,opengl_backend_ready=false,win32_backend_ready=false,panel_was_open=false,compatible=true;
std::atomic_bool request_shutdown{false},shut_down{false};
int gl_stops=0,win_stops=0,contexts_deleted=0,procs_restored=0,swaps=0,modules_stopped=0;
HWND restored_window=nullptr; LONG_PTR restored_proc=0;
int wglSwapBuffers=0;
BOOL swap(HDC){++swaps;return TRUE;} BOOL(*original_wglSwapBuffers)(HDC)=swap;
int MH_DisableHook(int){return 0;}
HWND WindowFromDC(HDC p){return p;}
BOOL IsWindow(HWND p){return p!=nullptr;}
LONG_PTR SetWindowLongPtrA(HWND h,int,LONG_PTR p){++procs_restored;restored_window=h;restored_proc=p;return p;}
HGLRC wglGetCurrentContext(){return current_context;}HDC wglGetCurrentDC(){return current_device;}
BOOL wglMakeCurrent(HDC d,HGLRC c){if(!compatible)return FALSE;current_device=d;current_context=c;return TRUE;}
BOOL wglDeleteContext(HGLRC){++contexts_deleted;return TRUE;}
void OutputDebugStringA(const char*){}
void glDeleteTextures(int,GLuint*){}
GLuint icon_textures[2]{};struct attempts {void fill(bool){}} icon_texture_attempted;
namespace services::game::menu_input {void shutdown(){}}
namespace modules {
 module::module(const char* n,const char* c,const char* id):id_storage(id?id:n),name(n),category(c){}
 void module::run(::cache&){}
 void module::render_options(){}
 void module::on_enable(::cache&){}
 void module::on_disable(::cache&){}
 struct test_module:module {void on_render_shutdown()override{assert(current_context==handle(20));++modules_stopped;}};
 // The shipping cleanup only casts these pointers; no test modules are registered.
 class jump_reset:public module {public:void cancel_on_client_thread(){}};
 class inv_walk:public module {public:void cancel_on_client_thread(){}};
 std::vector<module*> entries;
 const std::vector<module*>& get_modules(){return entries;}
 module* find_module_by_id(const char*){return nullptr;}
}
namespace gui {
 std::atomic_bool should_unload{false};
 namespace background {void shutdown(){}}
 namespace grim {void reset_runtime_state(){}}
 void reset_widget_runtime_state(){}
}
namespace configs::keybind_profile {bool flush(bool){return true;}}
namespace ImGui {void SetCurrentContext(ImGuiContext*){} void DestroyContext(ImGuiContext*){}}
void ImGui_ImplOpenGL3_Shutdown(){++gl_stops;assert(imGuiContext && opengl_backend_ready);}
void ImGui_ImplWin32_Shutdown(){++win_stops;assert(imGuiContext && win32_backend_ready);}
UNINIT
PREFIX
 return FALSE; // Normal initialization would start here.
}
BOOL other_window(HWND current_window,HDC device){GATE return FALSE;}
struct operations {
 int fail=0,removed=0,uninitialized=0;
 bool initialize(){return fail!=0;}bool create(){return fail!=1;}bool enable(){return fail!=2;}
 void remove(){++removed;}void uninitialize(){++uninitialized;}
};
int main(){
 // First callback can be shutdown, before any ImGui/backend initialization.
 request_shutdown=true;assert(detour_wglSwapBuffers(handle(2))==TRUE);
 assert(shut_down && !request_shutdown && gl_stops==0 && win_stops==0 && procs_restored==0);
 assert(uninit(handle(2)));assert(gl_stops==0 && win_stops==0 && contexts_deleted==0);
 gui::should_unload=true;assert(detour_wglSwapBuffers(handle(2))==TRUE);gui::should_unload=false;
 modules::test_module entry;modules::entries={&entry};
 // A different live window is a passthrough, not a teardown/rebind.
 window=handle(1);original_WndProc=proc_a;run_once=false;
 assert(other_window(handle(2),handle(2))==TRUE);assert(window==handle(1) && procs_restored==0);
 // Real cleanup restores the stored owner's HWND, not the incoming HDC/window.
 new_context=handle(30);imGuiContext=&context;opengl_backend_ready=win32_backend_ready=true;
 compatible=false;assert(!uninit(handle(2)));assert(imGuiContext && new_context && procs_restored==0);
 compatible=true;assert(uninit(handle(2)));assert(restored_window==handle(1));
 assert(restored_proc==reinterpret_cast<LONG_PTR>(proc_a));
 assert(gl_stops==1 && win_stops==1 && contexts_deleted==1 && run_once && !window && modules_stopped==1);
 assert(current_context==handle(20));
 assert(uninit(handle(2)));assert(gl_stops==1 && contexts_deleted==1);
 // Partial setup owns only the GL backend, not Win32.
 imGuiContext=&context;new_context=handle(40);opengl_backend_ready=true;
 assert(uninit(handle(2)));assert(gl_stops==2 && win_stops==1);
 for(int fail=0;fail<4;++fail){operations ops;ops.fail=fail;
  {gui::hook_transaction transaction(ops);
   if(transaction.initialize() && transaction.create() && transaction.enable())transaction.commit();}
  assert(ops.removed==(fail==2));assert(ops.uninitialized==(fail==1 || fail==2));
 }
 std::atomic_bool requested=false;
 std::thread first([&]{for(int n=0;n<100001;++n)modules::toggle_requested_state(requested);});
 std::thread second([&]{for(int n=0;n<100000;++n)modules::toggle_requested_state(requested);});
 first.join();second.join();assert(requested);
 std::cout<<"PASS shipping renderer: early unload, partial backend, repeated cleanup, owner HWND, auxiliary window, context retry; hook rollback; concurrent toggle parity\n";
}
'''.replace('UNINIT',extract(gui,'static bool uninit(')).replace('PREFIX',prefix).replace('GATE',window_gate)
with tempfile.TemporaryDirectory(prefix='onestep-render-lifecycle-') as tmp:
    p=Path(tmp);(p/'test.cpp').write_text(source)
    subprocess.run(['g++','-std=c++20','-pthread','-Wall','-Wextra','-I',str(ROOT),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
