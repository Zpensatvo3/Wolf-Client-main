#include "../src/auth/auth.hpp"
#include "../src/auth/client.hpp"
#include "../src/auth/config.hpp"
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace
{
    std::atomic_bool authenticated{false};
    std::atomic_int requests{0};
    std::mutex mutex;
    std::condition_variable changed;
    bool reply = false;
    bool accept = false;

    template<class Predicate> void wait_for(Predicate predicate)
    {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (!predicate())
        {
            assert(std::chrono::steady_clock::now() < deadline);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    void respond(bool success)
    {
        std::lock_guard lock(mutex);
        accept = success;
        reply = true;
        changed.notify_all();
    }
}

// A controlled backend exercises cancellation and ownership without requests
// to a real server or embedding a signing secret in the tests.
auth::auth_result auth::authenticate(const std::string& key, const std::string&)
{
    assert(key == "TEST-KEY-1234");
    ++requests;
    std::unique_lock lock(mutex);
    changed.wait(lock, [] { return reply; });
    reply = false;
    auth_result result;
    result.success = accept;
    result.message = accept ? "" : "Invalid license";
    authenticated = accept;
    return result;
}
void auth::start_heartbeat_monitor(const std::string&) {}
void auth::controlled_shutdown() { authenticated = false; }
bool auth::is_authenticated() { return authenticated; }

int main()
{
    using namespace auth::client;
    initialize();
    if constexpr (!auth::config::required)
    {
        assert(access_allowed() && !login_required());
        assert(!submit_license("TEST-KEY-1234"));
        shutdown();
        return 0;
    }
    assert(login_required() && !access_allowed());
    assert(!submit_license("bad"));
    assert(snapshot().status == phase::error && requests == 0);
    assert(submit_license("  TEST-KEY-1234\r\n"));
    wait_for([] { return requests == 1; });
    assert(!submit_license("TEST-KEY-1234"));
    respond(false);
    wait_for([] { return snapshot().status == phase::error; });
    assert(!access_allowed());
    assert(submit_license("TEST-KEY-1234"));
    wait_for([] { return requests == 2; });
    respond(true);
    wait_for([] { return access_allowed(); });
    authenticated = false;
    assert(!access_allowed());
    shutdown();

    initialize();
    assert(submit_license("TEST-KEY-1234"));
    wait_for([] { return requests == 3; });
    std::thread closer([] { shutdown(); });
    wait_for([] { return snapshot().status == phase::stopping; });
    respond(true); // Late success must not grant access while unloading.
    closer.join();
    assert(!access_allowed() && !authenticated);
    assert(!submit_license("TEST-KEY-1234"));
}
