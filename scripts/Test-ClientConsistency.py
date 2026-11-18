"""Compile shipping input and resource cleanup paths with failure-injection doubles."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parent.parent

def function(source,signature):
    start=source.index(signature); opening=source.index('{',start); end=opening+1; depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}'); end+=1
    return source[start:end]

loop=(ROOT/'src/app/module_loop.cpp').read_text()
block=loop[loop.index('        bool pressed ='):loop.index('        if (keys_[entry].sample')]
source=r'''
#include <cassert>
#include <algorithm>
#include <vector>
#include <map>
#include <cstdint>
#include <iostream>
#include "src/platform/input/key_codes.hpp"
constexpr int VK_SPACE=0x20;
namespace gui { int menu_key=0x2d; }
int GetAsyncKeyState(int) { return 0x8000; }
namespace services::input { bool physical_key_down(int){return true;} bool physical_scan_down(unsigned){return true;} }
bool input(int binding) { BLOCK return pressed; }
using jobject=void*; using jboolean=int;
constexpr int JNI_TRUE=1,JNI_FALSE=0;
bool failure=false;
struct item { int value=1; int text=8; };
int deleted=0;
struct env_type { void DeleteGlobalRef(void*) { ++deleted; } } environment;
using JNIEnv=env_type;
namespace jni { struct frame { frame(int=0){} }; env_type* get_env(){return &environment;} }
namespace platform::jni_context { bool clear_exception(){bool old=failure; failure=false; return old;} }
void OutputDebugStringA(const char*){}
namespace maps {
 struct Text { void* object; Text(void* p,bool):object(p){} };
 struct Entity {
    item* object;
    struct visible { item* p; void call(int v){if(!failure)p->value=v;} } setCustomNameVisible;
    Entity(void* p,bool):object(static_cast<item*>(p)),setCustomNameVisible{object}{}
    int getFlag(int){return object->value;}
    void setFlag(int,int v){if(!failure)object->value=v;}
 };
 struct TextDisplayEntity {
    struct setter { item* p; void call(Text v){if(!failure)p->text=*static_cast<int*>(v.object);} } setText;
    TextDisplayEntity(void* p,bool):setText{static_cast<item*>(p)}{}
 };
 struct SimpleOption { void* object; SimpleOption(void* p,bool):object(p){} };
}
enum class carrier_kind {armor_stand,text_display};
struct suppressed_carrier {void* entity;void* original_text;carrier_kind kind;int original_name_visible;};
CARRIER
std::vector<suppressed_carrier> suppressed_carriers;
RELEASE_LIST
struct glow_record { jobject entity=nullptr; jboolean original=JNI_FALSE,written=JNI_TRUE; bool applied=false; std::uint64_t seen=0; };
std::map<int,glow_record> glow_records;
jobject glow_world=nullptr;
RESTORE_NATIVE
GLOW
bool write_gamma(maps::SimpleOption& option,double value){if(failure){failure=false;return false;} *static_cast<double*>(option.object)=value;return true;}
namespace modules { struct full_bright {void* gamma_option_=nullptr;double original_gamma=.5;bool original_gamma_captured=true;void restore_gamma(bool);}; }
GAMMA
int main(){
 for(int reserved:{0x1b,0x2d,0x23,0x5b,0x5c}) assert(!input(reserved));
 for(int usable:{0,1,2,4,5,6,0xa1,0x60,0x61,0x10d,255}) {
    assert(platform::keys::binding_allowed(usable));
    assert(input(usable)==(usable!=0));
 }
 assert(!input(256));
 item entity; suppressed_carrier carrier{&entity,nullptr,carrier_kind::armor_stand,0};
 failure=true; assert(!restore_and_release_carrier(carrier,true)); assert(carrier.entity && deleted==0);
 assert(restore_and_release_carrier(carrier,true)); assert(!carrier.entity && entity.value==0 && deleted==1);
 assert(restore_and_release_carrier(carrier,true)); assert(deleted==1);
 int original=3; item display; suppressed_carrier text{&display,&original,carrier_kind::text_display,0};
 failure=true; assert(!restore_and_release_carrier(text,true)); assert(text.original_text && deleted==1);
 assert(restore_and_release_carrier(text,true)); assert(display.text==3 && deleted==3);
 item first{1},external{0}; glow_records={{1,{&first,0,1,true,0}},{2,{&external,1,1,true,0}}};
 failure=true; release_native_glow(true,false); assert(glow_records.size()==1 && first.value==1 && external.value==0);
 release_native_glow(true,false); assert(glow_records.empty() && first.value==0);
 double gamma=16; modules::full_bright bright; bright.gamma_option_=&gamma;
 failure=true; bright.restore_gamma(false); assert(bright.gamma_option_ && gamma==16);
 bright.restore_gamma(false); assert(!bright.gamma_option_ && gamma==.5 && !bright.original_gamma_captured);
 int count=deleted; bright.restore_gamma(true); assert(count==deleted);
 std::vector<item> many(200);
 for(auto& one:many) suppressed_carriers.push_back({&one,nullptr,carrier_kind::armor_stand,0});
 assert(!release_suppressed_carriers(true,64)); assert(suppressed_carriers.size()==136);
 assert(release_suppressed_carriers(true,suppressed_carriers.size())); assert(suppressed_carriers.empty());
 std::cout<<"PASS shipping input: reserved keys, mouse/legacy VK, numpad; carrier/glow/gamma: failure, retry, external state, idempotent release\n";
}
'''
for marker,value in {
 'BLOCK':block,
 'CARRIER':function((ROOT/'src/modules/visual/nametag.cpp').read_text(),'bool restore_and_release_carrier('),
 'RELEASE_LIST':function((ROOT/'src/modules/visual/nametag.cpp').read_text(),'    bool release_suppressed_carriers('),
 'RESTORE_NATIVE':function((ROOT/'src/modules/visual/glow_esp.cpp').read_text(),'    bool restore_native_glow('),
 'GLOW':function((ROOT/'src/modules/visual/glow_esp.cpp').read_text(),'    void release_native_glow('),
 'GAMMA':function((ROOT/'src/modules/visual/full_bright.cpp').read_text(),'void modules::full_bright::restore_gamma('),
}.items(): source=source.replace(marker,value)
with tempfile.TemporaryDirectory(prefix='onestep-consistency-') as tmp:
    path=Path(tmp); cpp=path/'test.cpp'; binary=path/'test';cpp.write_text(source)
    subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-I',str(ROOT),str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
