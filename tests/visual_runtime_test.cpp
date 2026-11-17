#include "../src/modules/visual/runtime_budget.hpp"
#include <cassert>
#include <future>
#include <iostream>
#include <random>
#include <set>

int main()
{
    using namespace std::chrono_literals;
    // Differential spatial test, including negative cell boundaries: every
    // pair within the shipping radius must remain in the candidate set.
    std::mt19937 random(42);
    std::uniform_real_distribution<double> position(-30.,30.);
    std::vector<std::pair<double,double>> players;
    visual_runtime::nearby_grid grid;
    for (int i=0;i<300;++i) {
        players.emplace_back(position(random),position(random));
        grid.add(players.back().first,players.back().second,i);
    }
    for (int query=0;query<2000;++query) {
        const double x=position(random),z=position(random);
        std::set<std::size_t> candidates;
        grid.visit(x,z,[&](auto i) { assert(candidates.insert(i).second); });
        for (std::size_t i=0;i<players.size();++i) {
            const auto dx=x-players[i].first,dz=z-players[i].second;
            if (dx*dx+dz*dz<=1.65*1.65) assert(candidates.contains(i));
        }
    }
    grid.visit(std::nan(""),0,[](auto) { assert(false); });
    std::map<std::string,std::string> records;
    for (int i=0;i<10000;++i) records.emplace(std::to_string(i),"original-team");
    const std::set<std::string> present{"1","2"};
    std::string cursor;
    int calls=0;
    const auto restore=[&](const auto& name,const auto& original) {
        ++calls; assert(!present.contains(name)); assert(original=="original-team");
        return name!="0"; // Retryable failure must not starve the other records.
    };
    for (int pass=0;pass<200;++pass) {
        const int before=calls;
        visual_runtime::retire_records(records,present,cursor,restore);
        assert(calls-before<=64);
    }
    assert(records.size()==3 && records.contains("0"));
    visual_runtime::retire_records(records,present,cursor,[](auto&,auto&) { return true; });
    assert(records.size()==2);

    // Gate the first decode, then issue a large burst. Only the first and
    // latest jobs may run, and the first result must never be published.
    std::promise<void> entered,release;
    auto gate=release.get_future().share();
    std::atomic<int> loads=0;
    visual_runtime::latest_job<int> jobs([&](int key) {
        ++loads;
        if (key==0) { entered.set_value(); gate.wait(); }
        return key*2;
    });
    jobs.request(0);
    assert(entered.get_future().wait_for(2s)==std::future_status::ready);
    for (int key=1;key<=10000;++key) jobs.request(key);
    release.set_value();
    std::optional<int> result;
    const auto deadline=std::chrono::steady_clock::now()+2s;
    while (!(result=jobs.take(10000)) && std::chrono::steady_clock::now()<deadline)
        std::this_thread::yield();
    assert(result && *result==20000 && loads==2 && !jobs.take(0));
    jobs.request(10000); assert(!jobs.take(10000)); // No repeat decode each frame.
    jobs.cancel(); jobs.request(3);
    while (!(result=jobs.take(3)) && std::chrono::steady_clock::now()<deadline)
        std::this_thread::yield();
    assert(result && *result==6);
    jobs.shutdown(); jobs.shutdown(); jobs.request(4);
    assert(!jobs.take(4));
    std::cout << "PASS visual runtime: spatial equivalence, 10000 undo records, fair retry, coalesced decode and shutdown\n";
}
