"""Run shipping clipping, scoreboard restore and media lifecycle code with inert platform fixtures."""
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parent.parent


def function(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 1
    end = opening + 1
    while depth:
        depth += (source[end] == "{") - (source[end] == "}")
        end += 1
    return source[start:end]


common = r'''
#include <algorithm>
#include <array>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <memory_resource>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>
using namespace std::chrono_literals;
'''
cosmetic = (ROOT / "src/modules/visual/cosmetic_renderer.cpp").read_text()
geometry = common + r'''
struct ImVec2 { float x=0,y=0; };
struct ImVec4 { float x=0,y=0,z=0,w=0; };
using ImTextureID=void*;
namespace maths { struct vector3d { double x,y,z; }; }
'''
geometry += cosmetic[cosmetic.index("    constexpr double pi"):cosmetic.index("    struct scene")]
geometry += "struct scene { std::vector<shadow> occluders; };\n"
geometry += function(cosmetic, "    polygon clip(")
geometry += "struct texture_vertex { point position; ImVec2 uv; }; using texture_polygon=std::pmr::vector<texture_vertex>;\n"
for signature in ("    texture_polygon clip_texture(", "    std::pmr::vector<texture_polygon> unoccluded_texture(",
                  "    std::pmr::vector<polygon> unoccluded(", "    std::pmr::vector<std::pair<point,point>> unoccluded_edge("):
    geometry += function(cosmetic, signature)
geometry += r'''
double area(const polygon& p) {
    double value=0;
    for (std::size_t i=0;i<p.size();++i) value+=p[i].x*p[(i+1)%p.size()].y-p[(i+1)%p.size()].x*p[i].y;
    return std::abs(value)*.5;
}
struct counting : std::pmr::memory_resource {
    int allocations=0, live=0;
    void* do_allocate(std::size_t bytes,std::size_t align) override {
        ++allocations; ++live; return std::pmr::new_delete_resource()->allocate(bytes,align);
    }
    void do_deallocate(void* p,std::size_t bytes,std::size_t align) override {
        --live; std::pmr::new_delete_resource()->deallocate(p,bytes,align);
    }
    bool do_is_equal(const std::pmr::memory_resource& other) const noexcept override { return this==&other; }
};
int main() {
    counting upstream;
    std::array<std::byte,256*1024> storage;
    for (int frame=0;frame<1000;++frame) {
        std::pmr::monotonic_buffer_resource backing(storage.data(),storage.size(),&upstream);
        std::pmr::unsynchronized_pool_resource arena(std::pmr::pool_options{32,4096},&backing);
        polygon square({{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}},&arena);
        auto half=clip(square,{{1,0,0},0});
        assert(std::abs(area(half)-2.)<1e-9 && half.get_allocator().resource()==&arena);
        texture_polygon textured({{{-1,0,0},{0,0}},{{1,0,0},{1,0}},{{1,1,0},{1,1}}},&arena);
        const auto clipped=clip_texture(textured,{{1,0,0},0});
        int intersections=0;
        for (const auto& p:clipped) if (p.position.x==0) { assert(std::abs(p.uv.x-.5f)<1e-6); ++intersections; }
        assert(intersections==2);
        scene blocked;
        blocked.occluders.push_back({{{1,0,0},2},{{-1,0,0},2},{{0,1,0},2},{{0,-1,0},2},{{0,0,1},2},{{0,0,-1},2}});
        assert(unoccluded(polygon(square,&arena),blocked).empty());
        assert(unoccluded_texture(texture_polygon(textured,&arena),blocked).empty());
        scene outside;
        outside.occluders.push_back({{{1,0,0},-3}});
        const auto visible=unoccluded(polygon(square,&arena),outside);
        assert(visible.size()==1 && std::abs(area(visible[0])-4.)<1e-9);
        scene strip;
        strip.occluders.push_back({{{1,0,0},1},{{-1,0,0},1}});
        auto edges=unoccluded_edge({-2,0,0},{2,0,0},strip,&arena);
        assert(edges.size()==2);
        double length=0; for (const auto& [a,b]:edges) length+=b.x-a.x;
        assert(std::abs(length-2.)<1e-9);
    }
    assert(upstream.live==0 && upstream.allocations==0);
    // Overflow must be released too, without imposing a visual geometry cap.
    {
        std::pmr::monotonic_buffer_resource backing(storage.data(),storage.size(),&upstream);
        std::pmr::vector<point> large(&backing); large.resize(100000);
        assert(upstream.live>0);
    }
    assert(upstream.live==0);
    std::cout << "PASS shipping clipping: geometry/UV/occlusion, 1000 scratch frames without heap fallback, overflow reclaimed\n";
}
'''

nametag = (ROOT / "src/modules/visual/nametag.cpp").read_text()
restore = common + r'''
bool exception=false, reject=false;
namespace platform::jni_context { bool clear_exception() { return std::exchange(exception,false); } }
namespace maps {
    struct String { std::string value; static String create(const char* s) { return {s}; } };
    struct Team {
        std::string name;
        explicit operator bool() const { return !name.empty(); }
        bool is_same_object(const Team& other) const { return name==other.name; }
    };
    struct Scoreboard {
        mutable std::map<std::string,std::string> players;
        explicit operator bool() const { return true; }
        Team getScoreHolderTeam(const String& name) const { return {players[name.value]}; }
        Team getTeam(const String& name) const { return {name.value=="deleted" ? "" : name.value}; }
        void addScoreHolderToTeam(const String& name,const Team& team) const {
            if (reject) exception=true; else players[name.value]=team.name;
        }
        void removeScoreHolderFromTeam(const String& name,const Team&) const { players[name.value]=""; }
    };
}
'''
restore += function(nametag, "    bool restore_team(")
restore += r'''
int main() {
    maps::Scoreboard board;
    maps::Team hidden{"hidden"};
    board.players["player"]="hidden";
    assert(restore_team(board,hidden,"player","red") && board.players["player"]=="red");
    board.players["player"]="new-server-team";
    assert(restore_team(board,hidden,"player","red") && board.players["player"]=="new-server-team");
    board.players["player"]="hidden"; reject=true;
    assert(!restore_team(board,hidden,"player","red") && board.players["player"]=="hidden");
    reject=false; assert(restore_team(board,hidden,"player","red"));
    board.players["player"]="hidden";
    assert(restore_team(board,hidden,"player","deleted") && board.players["player"].empty());
    std::cout << "PASS shipping scoreboard restore: original team, server override, retry, deleted team\n";
}
'''

spotify = (ROOT / "src/modules/visual/spotify_source.cpp").read_text()
media = common + r'''
#include "src/modules/visual/spotify_source.hpp"
std::atomic<bool> fail_init{true}, allow_finish{false};
std::atomic<int> calls{0}, cancellations{0}, live{0}, peak{0};
namespace winrt {
    enum class apartment_type { multi_threaded };
    void init_apartment(apartment_type) { if (fail_init.exchange(false)) throw std::runtime_error("COM unavailable"); }
    void uninit_apartment() {}
    struct hresult_canceled : std::runtime_error { hresult_canceled():runtime_error("canceled") {} };
    namespace Windows::Foundation { enum class AsyncStatus { Started,Completed,Canceled }; }
}
struct media_object { media_object(std::nullptr_t=nullptr) {} };
using GlobalSystemMediaTransportControlsSessionManager=media_object;
using GlobalSystemMediaTransportControlsSession=media_object;
using GlobalSystemMediaTransportControlsSessionMediaProperties=media_object;
constexpr auto poll_interval=std::chrono::milliseconds(10);
struct pending_operation {
    mutable bool canceled=false;
    auto Status() const {
        using winrt::Windows::Foundation::AsyncStatus;
        return canceled ? AsyncStatus::Canceled : allow_finish ? AsyncStatus::Completed : AsyncStatus::Started;
    }
    void Cancel() const { canceled=true; ++cancellations; }
    int GetResults() const { if (canceled) throw winrt::hresult_canceled(); return 7; }
};
'''
media += function(spotify, "    template<class Operation, class Cancelled>")
start = spotify.index("modules::spotify::source& modules::spotify::source::instance()")
end = spotify.index("void modules::spotify::source::poll(", start)
media += spotify[start:end]
media += r'''
void modules::spotify::source::poll(now_playing& local,std::string&,poll_state&,std::uint64_t epoch) {
    const int concurrent=++live;
    peak.store((std::max)(peak.load(),concurrent));
    struct leave { ~leave() { --live; } } guard;
    ++calls;
    await_result(pending_operation{},[&] { return cancelled(epoch); },std::chrono::steady_clock::now()+30s);
    local.active=true; local.title=std::to_string(epoch); local.sampled_at=std::chrono::steady_clock::now();
    ++local.artwork_revision;
}
template<class F> void eventually(F predicate) {
    const auto deadline=std::chrono::steady_clock::now()+3s;
    while (!predicate() && std::chrono::steady_clock::now()<deadline) std::this_thread::sleep_for(1ms);
    assert(predicate());
}
int main() {
    // Timeout and user cancellation exercise the actual await helper.
    try { await_result(pending_operation{},[]{return false;},std::chrono::steady_clock::now()); assert(false); }
    catch (const winrt::hresult_canceled&) {}
    auto& service=modules::spotify::source::instance();
    service.start(true);
    eventually([&] { return !service.is_running(); }); // Failed COM setup must be restartable.
    service.start(true);
    eventually([] { return calls.load()>0; });
    const auto begin=std::chrono::steady_clock::now();
    service.stop(); // Operation is deliberately pending for up to 30 seconds.
    assert(std::chrono::steady_clock::now()-begin<250ms);
    eventually([] { return live.load()==0; });
    assert(!service.snapshot().active && cancellations>=2);
    allow_finish=true; service.start(false);
    eventually([&] { return service.snapshot().active; });
    const auto revision=service.snapshot().artwork_revision;
    for (int i=0;i<100;++i) { service.stop(); service.start(i%2==0); }
    eventually([&] { return service.snapshot().active; });
    assert(service.snapshot().artwork_revision>=revision && peak==1);
    allow_finish=false;
    service.stop(); service.start(true);
    eventually([] { return live.load()==1; });
    service.shutdown();
    assert(live==0 && !service.snapshot().active && !service.is_running());
    service.start(true); assert(!service.is_running());
    std::cout << "PASS shipping media lifecycle: timeout/cancel, failed initialization retry, nonblocking stop, 100 restarts, joined shutdown\n";
}
'''

with tempfile.TemporaryDirectory(prefix="onestep-visual-runtime-") as directory:
    for name, fixture in (("clipping", geometry), ("restore", restore), ("media", media)):
        cpp = Path(directory) / (name + ".cpp")
        exe = Path(directory) / name
        cpp.write_text(fixture)
        subprocess.run([shutil.which("g++") or "g++", "-std=c++20", "-pthread", "-I", str(ROOT),
                        str(cpp), "-o", str(exe)], check=True)
        subprocess.run([str(exe)], check=True, timeout=30)
