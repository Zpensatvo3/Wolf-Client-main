"""Exercise the shipping Nametag cleanup routine with real JVM global references.

Scoreboard operations are deterministic doubles; JNI identity and global/weak
reference lifetimes use an actual JVM. This does not start Minecraft.
"""
from pathlib import Path
import argparse
import os
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def extract(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    level = 1
    end = opening + 1
    while level:
        level += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--java-home', type=Path, default=Path(os.environ.get('JAVA_HOME', '')))
    parser.add_argument('--compiler', default=os.environ.get('CXX', 'g++'))
    args = parser.parse_args()
    jdk = args.java_home.resolve()
    if os.name == 'nt' or not (jdk / 'lib/server/libjvm.so').is_file():
        parser.error('Linux JDK with lib/server/libjvm.so required; pass --java-home')
    compiler = shutil.which(args.compiler)
    if not compiler:
        parser.error('C++20 compiler not found')
    source = (ROOT / 'src/modules/visual/nametag.cpp').read_text(encoding='utf-8')
    fixture = r'''
#include <jni.h>
#include "src/modules/visual/runtime_budget.hpp"
#include <cassert>
#include <iostream>
#include <map>
#include <string>
#include <unordered_set>
#include <utility>
JNIEnv* current_env=nullptr;
bool java_error=false, fail_restore=false;
namespace jni { struct frame {}; JNIEnv* get_env() { return current_env; } }
namespace platform::jni_context { bool clear_exception() { return std::exchange(java_error,false); } }
struct board_state { jobject identity=nullptr; std::map<std::string,std::string> members; bool hidden=true; int removed=0; };
board_state* selected=nullptr;
namespace maps {
    struct String { std::string value; static String create(const char* s) { return {s}; } };
    struct Team {
        std::string name;
        explicit operator bool() const { return !name.empty(); }
    };
    struct Scoreboard {
        jobject object_instance=nullptr;
        explicit operator bool() const { return object_instance!=nullptr; }
        Team getTeam(const String& s) const { return {selected->hidden ? s.value : ""}; }
        void removeTeam(const Team&) const { selected->hidden=false; ++selected->removed; }
    };
    struct World {
        bool available=false;
        explicit operator bool() const { return available; }
        Scoreboard getScoreboard() const { return {selected ? selected->identity : nullptr}; }
    };
}
struct cache { bool valid=false; maps::World world; bool is_valid() const { return valid; } };
std::map<std::string,std::string> original_team_by_player;
std::string retire_after,hidden_team_name;
jobject scoreboard_identity=nullptr,carrier_identity=nullptr;
bool cleanup_pending=false;
bool release_suppressed_carriers(bool) {
    if (carrier_identity) { current_env->DeleteGlobalRef(carrier_identity); carrier_identity=nullptr; }
    return true;
}
bool restore_team(const maps::Scoreboard&,const maps::Team&,const std::string& name,const std::string& original) {
    if (fail_restore && name=="a") return false;
    if (selected->members[name]=="hidden") selected->members[name]=original;
    return true;
}
'''
    fixture += extract(source, '    void process_tag_cleanup(')
    fixture += r'''
int main() {
    JavaVMOption options[]={{const_cast<char*>("-Xcheck:jni"),nullptr}};
    JavaVMInitArgs args{}; args.version=JNI_VERSION_1_8; args.nOptions=1; args.options=options;
    JavaVM* vm=nullptr;
    assert(JNI_CreateJavaVM(&vm,reinterpret_cast<void**>(&current_env),&args)==JNI_OK);
    JNIEnv* env=current_env;
    jclass type=env->FindClass("java/lang/Object"); assert(type);
    jmethodID ctor=env->GetMethodID(type,"<init>","()V"); assert(ctor);
    jobject object=env->NewObject(type,ctor);
    jweak weak=env->NewWeakGlobalRef(object);
    scoreboard_identity=env->NewGlobalRef(object);
    carrier_identity=env->NewGlobalRef(object);
    board_state board{object,{{"a","hidden"},{"b","hidden"},{"c","server"}}};
    selected=&board; hidden_team_name="hidden"; cleanup_pending=true;
    original_team_by_player={{"a","red"},{"b","blue"},{"c","old"}};
    cache game{true,{true}};
    fail_restore=true; process_tag_cleanup(game,false);
    assert(cleanup_pending && env->IsSameObject(scoreboard_identity,object)==JNI_TRUE);
    assert(original_team_by_player.size()==1 && board.members["b"]=="blue" && board.members["c"]=="server");
    assert(!carrier_identity && board.hidden);
    fail_restore=false; process_tag_cleanup(game,false);
    assert(!cleanup_pending && !scoreboard_identity && !board.hidden && board.members["a"]=="red");
    process_tag_cleanup(game,false); assert(board.removed==1);
    scoreboard_identity=env->NewGlobalRef(object); cleanup_pending=true; hidden_team_name="hidden";
    game.valid=false; process_tag_cleanup(game,false);
    assert(cleanup_pending && scoreboard_identity);
    current_env=nullptr; process_tag_cleanup(game,true);
    assert(cleanup_pending && scoreboard_identity);
    current_env=env; process_tag_cleanup(game,true);
    assert(!cleanup_pending && !scoreboard_identity);
    // A different actual Java object must not receive the prior world's records.
    jobject next=env->NewObject(type,ctor);
    scoreboard_identity=env->NewGlobalRef(object); cleanup_pending=true; hidden_team_name="hidden";
    original_team_by_player={{"a","red"}};
    board_state newer{next,{{"a","other"}}}; selected=&newer; game.valid=true;
    process_tag_cleanup(game,false);
    assert(!cleanup_pending && !scoreboard_identity && newer.members["a"]=="other" && newer.removed==0);
    env->DeleteLocalRef(next); env->DeleteLocalRef(object); env->DeleteWeakGlobalRef(weak); env->DeleteLocalRef(type);
    assert(!env->ExceptionCheck());
    assert(vm->DestroyJavaVM()==JNI_OK);
    std::cout << "PASS Nametag actual JVM: global refs, failed restore retry, invalid world and identity switch\n";
}
'''
    with tempfile.TemporaryDirectory(prefix='onestep-nametag-jvm-') as directory:
        cpp = Path(directory) / 'nametag.cpp'
        executable = Path(directory) / 'nametag'
        cpp.write_text(fixture, encoding='utf-8')
        subprocess.run([compiler, '-std=c++20', '-pthread', '-UNDEBUG', '-I', str(ROOT),
                        '-I', str(jdk / 'include'), '-I', str(jdk / 'include/linux'), str(cpp),
                        '-L' + str(jdk / 'lib/server'), '-Wl,-rpath,' + str(jdk / 'lib/server'),
                        '-ljvm', '-o', str(executable)], check=True)
        subprocess.run([str(executable)], check=True, timeout=30)


if __name__ == '__main__':
    main()
