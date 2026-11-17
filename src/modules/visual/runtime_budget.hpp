#pragma once
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace visual_runtime
{
    // Ordered undo records permit a stable, fair cursor even when restores
    // fail or players arrive. Failed records remain available for retry.
    template<class Map, class Seen, class Restore>
    void retire_records(Map& records,const Seen& seen,std::string& after,Restore&& restore,
        std::size_t budget=64)
    {
        const auto count=(std::min)(budget,records.size());
        for (std::size_t i=0;i<count && !records.empty();++i) {
            auto it=records.upper_bound(after);
            if (it==records.end()) it=records.begin();
            after=it->first;
            if (!seen.contains(it->first) && restore(it->first,it->second)) records.erase(it);
        }
        if (records.empty()) after.clear();
    }

    // A small spatial index for name-tag carriers. Exact distance checks stay
    // with the caller; floor is important on the negative side of the world.
    class nearby_grid
    {
        std::map<std::pair<int,int>, std::vector<std::size_t>> cells_;
        static int cell(double value) { return static_cast<int>(std::floor(value / 2.)); }
    public:
        void add(double x, double z, std::size_t index)
        {
            if (std::isfinite(x) && std::isfinite(z) && std::abs(x)<3.1e7 && std::abs(z)<3.1e7)
                cells_[{cell(x),cell(z)}].push_back(index);
        }
        template<class F> void visit(double x, double z, F&& callback) const
        {
            if (!std::isfinite(x) || !std::isfinite(z) || std::abs(x)>=3.1e7 || std::abs(z)>=3.1e7) return;
            const int cx=cell(x), cz=cell(z);
            for (int dx=-1;dx<=1;++dx) for (int dz=-1;dz<=1;++dz)
                if (const auto it=cells_.find({cx+dx,cz+dz}); it!=cells_.end())
                    for (auto index:it->second) callback(index);
        }
    };

    // One running job, one latest request and one result. No detached threads
    // or growing queue; stale decodes are discarded before publication.
    template<class Result> class latest_job
    {
        std::function<Result(int)> load_;
        std::mutex mutex_;
        std::condition_variable wake_;
        std::thread thread_;
        bool closing_=false;
        int wanted_=-1;
        std::uint64_t generation_=0, completed_=0;
        std::optional<Result> result_;
        void run()
        {
            std::unique_lock lock(mutex_);
            for (;;) {
                wake_.wait(lock,[&] { return closing_ || (wanted_>=0 && generation_!=completed_); });
                if (closing_) return;
                const auto generation=generation_;
                const int key=wanted_;
                lock.unlock();
                Result result{};
                try { result=load_(key); } catch (...) { }
                lock.lock();
                if (!closing_ && generation==generation_) {
                    result_=std::move(result);
                    completed_=generation;
                }
            }
        }
    public:
        explicit latest_job(std::function<Result(int)> load):load_(std::move(load)) {}
        ~latest_job() { shutdown(); }
        void request(int key)
        {
            std::lock_guard lock(mutex_);
            if (closing_ || key<0 || key==wanted_) return;
            if (!thread_.joinable()) thread_=std::thread([this] { run(); });
            wanted_=key; ++generation_; result_.reset(); wake_.notify_all();
        }
        std::optional<Result> take(int key)
        {
            std::lock_guard lock(mutex_);
            if (key!=wanted_ || !result_) return {};
            auto result=std::move(result_); result_.reset(); return result;
        }
        void cancel()
        {
            std::lock_guard lock(mutex_);
            wanted_=-1; ++generation_; result_.reset();
        }
        void shutdown()
        {
            { std::lock_guard lock(mutex_); closing_=true; result_.reset(); wake_.notify_all(); }
            if (thread_.joinable()) thread_.join();
        }
    };
}
