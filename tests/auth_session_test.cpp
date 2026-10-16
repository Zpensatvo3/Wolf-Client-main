#include "../src/auth/auth.hpp"
#include <cassert>
#include <chrono>

int main()
{
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    auth::set_authenticated(true, "test-session", 1, now - 1);
    assert(!auth::is_authenticated());
    auth::set_authenticated(true, "", 1, now + 60'000);
    assert(!auth::is_authenticated());
    auth::set_authenticated(true, "test-session", 1, now + 60'000);
    assert(auth::is_authenticated());
    // Immediate shutdown wakes and joins the heartbeat before its first
    // scheduled request. No network or real license is used by this test.
    auth::start_heartbeat_monitor("unused.invalid");
    auth::controlled_shutdown();
    assert(!auth::is_authenticated());
    auth::stop_heartbeat_monitor(); // Repeated cleanup remains safe.
}
