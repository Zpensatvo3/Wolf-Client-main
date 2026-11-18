"""Run shipping native Glow control flow with identity and failure doubles."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parent.parent
def function(s,signature):
 a=s.index(signature);b=s.index('{',a)+1;d=1
 while d:
  d+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
glow=(r/'src/modules/visual/glow_esp.cpp').read_text()
source=r'''
#include <algorithm>
#include <atomic>
#include <cassert>
#include <iostream>
#include <vector>
#include <map>
#include <cstdint>
struct item {bool flag=false; int id=1;}; using jobject=item*; using jboolean=int;
constexpr int JNI_TRUE=1,JNI_FALSE=0;
bool error=false,fail_next=false,fail_all=false;int writes=0,comparisons=0,refs=0,fail_at=-1;
std::vector<item*> players;
struct JNIEnv {
 bool IsSameObject(item* a,item* b){++comparisons;return a==b;}
 item* NewGlobalRef(item* p){++refs;return p;} void DeleteGlobalRef(item*){assert(--refs>=0);}
} env;
namespace jni {struct frame{frame(){}~frame(){}};JNIEnv* get_env(){return &env;}}
namespace platform::jni_context {bool clear_exception(){bool old=error;error=false;return old;}}
namespace maps {
 struct PlayerEntity{};
 struct Entity {
  item* object_instance; Entity(item* p,bool):object_instance(p){}
  explicit operator bool()const{return object_instance;}
  bool is_same_object(const Entity& e)const{return object_instance==e.object_instance;}
  template<class T> bool is_instance_of(){return true;}
  struct alive {int call(){return JNI_TRUE;}} isAlive;
  struct id_method {item* p;int call(){return p->id;}} getId{object_instance};
  int getFlag(int){return object_instance->flag;}
  void setFlag(int,int value){++writes;if(fail_next || fail_all){fail_next=false;error=true;}else object_instance->flag=value;}
 };
 struct World {item* object_instance;explicit operator bool()const{return object_instance;}};
 struct List {item* object_instance;explicit operator bool()const{return true;}};
 struct Object {item* object_instance;};
 struct Iterator {
  int at=0;
  struct has {Iterator* self;int call(){return self->at<int(players.size());}} hasNext{this};
  struct next_type {Iterator* self;Object call(){if(self->at==fail_at)error=true;return {players[self->at++]};}} next{this};
  Iterator* operator->(){return this;}
  explicit operator bool()const{return true;}
 };
 struct Iterable {
  Iterable(item*,bool){}
  struct method {Iterator call(){return {};}} iterator;
 };
}
struct cache {maps::World world;maps::Entity player;bool is_valid(){return true;}};
namespace platform::adapter {
 struct api {maps::List get_players(maps::World w){return {w.object_instance};}} adapter;
 api& get(){return adapter;}
}
namespace services::anti_bot {bool is_bot(const maps::Entity&){return false;}}
namespace mc {struct target_type{int build_id=12111;};constexpr target_type target;}
namespace modules {struct glow_esp {std::atomic_bool native_fallback_{true};void run(cache&);};}
struct glow_record {jobject entity=nullptr;jboolean original=0,written=1;bool applied=false;std::uint64_t seen=0;};
std::map<int,glow_record> glow_records;jobject glow_world=nullptr;std::uint64_t glow_generation=0;
RESTORE
RELEASE
RUN
int main(){
 item world,local,other;cache game{{&world},{&local,false}};players={&other};modules::glow_esp module;
 fail_next=true;module.run(game);assert(!other.flag && glow_records.size()==1 && writes==1);
 module.run(game);assert(other.flag && writes==2 && glow_records.at(1).applied);
 release_native_glow(true,false);assert(!other.flag && glow_records.empty());
 // Failed first write followed by an external true is not owned by us.
 fail_next=true;module.run(game);other.flag=true;int previous=writes;
 module.run(game);assert(writes==previous && glow_records.empty() && other.flag);
 release_native_glow(false,true);other.flag=false;
 module.run(game);assert(other.flag);
 item replacement{false,1};players={&replacement};module.run(game);
 assert(!other.flag && replacement.flag && glow_records.at(1).entity==&replacement);
 release_native_glow(true,false);assert(!replacement.flag);
 fail_all=true;players={&replacement};for(int n=0;n<5;++n)module.run(game);
 assert(!replacement.flag && !glow_records.at(1).applied);
 release_native_glow(true,false);assert(glow_records.empty());fail_all=false;
 item second{false,2};players={&replacement,&second};module.run(game);
 assert(replacement.flag && second.flag && glow_records.size()==2);
 players={&replacement};fail_at=0;module.run(game);assert(glow_records.size()==2 && second.flag);
 fail_at=-1;module.run(game);assert(glow_records.size()==1 && !second.flag);
 item new_world,new_player{false,1};game.world.object_instance=&new_world;players={&new_player};module.run(game);
 assert(replacement.flag && new_player.flag && glow_records.at(1).entity==&new_player);
 release_native_glow(true,false);assert(!new_player.flag && refs==0);
 for(int n:{100,200,400}){
  std::vector<item> data(n);players.clear();for(int i=0;i<n;++i){data[i].id=i;players.push_back(&data[i]);}
  module.run(game);comparisons=0;module.run(game);assert(comparisons<=2*n+10);
  std::cout<<"PASS Glow indexed identity: "<<n<<" players, "<<comparisons<<" comparisons\n";
  players.resize(n/2);module.run(game);assert(glow_records.size()==std::size_t(n/2));
  for(int i=n/2;i<n;++i)assert(!data[i].flag);
  release_native_glow(true,false);assert(glow_records.empty());
 }
 assert(refs==0);
 std::cout<<"PASS shipping Glow run: failed application retry, external ownership, ID reuse, departures and cleanup\n";
}
'''.replace('RESTORE',function(glow,'    bool restore_native_glow(')).replace('RELEASE',function(glow,'    void release_native_glow(')).replace('RUN',function(glow,'void modules::glow_esp::run('))
with tempfile.TemporaryDirectory(prefix='wolf-audit-20261118-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(source)
 subprocess.run(['g++','-std=c++20','-Wall','-Wextra','-I',str(r),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
