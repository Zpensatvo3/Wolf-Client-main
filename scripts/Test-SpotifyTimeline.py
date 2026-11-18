"""Exercise the shipping poll sample path and snapshot with deterministic clock/SMTC doubles."""
from pathlib import Path
import subprocess,tempfile
ROOT=Path(__file__).resolve().parent.parent
source=(ROOT/'src/modules/visual/spotify_source.cpp').read_text()
start=source.index('    const bool session_changed=')
end=source.index('    const auto update_lyrics=',start)
# Replace only the clock dependency; retain shipping sample/control-flow code.
body=source[start:end].replace('std::chrono::steady_clock::now()','clock_fixture::now()')
fixture=r'''
#include <cassert>
#include <iostream>
#include <string>
#include "src/modules/visual/spotify_source.hpp"
namespace clock_fixture {
 int age_ms=0,delay_ms=0,requests=0;bool playing=true;double position=100;
 auto now(){return std::chrono::steady_clock::time_point{}+std::chrono::milliseconds(age_ms);}
}
namespace winrt {
 struct clock {
  using duration=std::chrono::duration<long long,std::ratio<1,10000000>>;
  using point=std::chrono::time_point<clock,duration>;
  static point now(){return point{duration{10000000LL+clock_fixture::age_ms*10000LL}};}
 };
 std::string to_string(const std::string& s){return s;}
}
struct Properties {
 bool present=false;
 explicit operator bool()const{return present;}
 std::string Title()const{return "song";}std::string Artist()const{return "artist";}std::string AlbumTitle()const{return "album";}
};
enum class GlobalSystemMediaTransportControlsSessionPlaybackStatus {Playing,Paused};
struct Timeline {
 winrt::clock::duration Position()const{return winrt::clock::duration{static_cast<long long>(clock_fixture::position*1e7)};}
 winrt::clock::duration EndTime()const{return winrt::clock::duration{3000000000LL};}
 winrt::clock::point LastUpdatedTime()const{return winrt::clock::point{winrt::clock::duration{10000000LL}};}
};
struct Playback {
 auto PlaybackStatus()const {return clock_fixture::playing ? GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing : GlobalSystemMediaTransportControlsSessionPlaybackStatus::Paused;}
};
struct Session {
 int id=1;bool operator==(const Session&)const=default;
 Timeline GetTimelineProperties()const{return {};}
 Properties TryGetMediaPropertiesAsync()const{return {true};}
 Playback GetPlaybackInfo()const{return {};}
};
struct Cache { Session selected{0};Properties properties;std::chrono::steady_clock::time_point metadata_due{};double previous_duration=0,previous_position=0;};
void trim(std::string&){}
template<class Abort,class Deadline> Properties await_result(Properties p,Abort abort,Deadline){
 assert(!abort());++clock_fixture::requests;clock_fixture::age_ms+=clock_fixture::delay_ms;return p;
}
bool poll(modules::spotify::now_playing& local,Cache& cache,Session session,std::string active_key){
 auto aborted=[] {return false;};auto deadline=clock_fixture::now()+std::chrono::seconds(3);
BODY
 return track_changed;
}
int main(){
 for(int delay:{0,100,2000}){
  clock_fixture::age_ms=0;clock_fixture::delay_ms=delay;clock_fixture::playing=true;clock_fixture::position=100;
  Cache cache;modules::spotify::now_playing local;assert(poll(local,cache,{},""));
  double expected=100+delay/1000.;assert(std::abs(local.position-expected)<1e-8);
  assert(local.sampled_at==clock_fixture::now());
  auto snapshot=local;snapshot.extrapolate(clock_fixture::now());assert(std::abs(snapshot.position-expected)<1e-8);
  snapshot=local;snapshot.extrapolate(clock_fixture::now()+std::chrono::milliseconds(500));assert(std::abs(snapshot.position-expected-.5)<1e-8);
 }
 clock_fixture::age_ms=0;clock_fixture::delay_ms=2000;clock_fixture::playing=false;clock_fixture::position=40;
 Cache cache;modules::spotify::now_playing local;assert(poll(local,cache,{},""));
 local.extrapolate(clock_fixture::now()+std::chrono::seconds(3));assert(local.position==40);
 // Same metadata but a new session still invalidates per-track state.
 clock_fixture::delay_ms=0;clock_fixture::playing=true;
 assert(poll(local,cache,Session{2},"song\x1f" "artist\x1f" "album"));
 std::cout<<"PASS shipping Spotify poll+snapshot: 0/100/2000ms metadata wait, no double count, playback pause and session identity\n";
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='onestep-spotify-timeline-') as tmp:
    p=Path(tmp);(p/'test.cpp').write_text(fixture)
    subprocess.run(['g++','-std=c++20','-pthread','-Wall','-Wextra','-I',str(ROOT),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
