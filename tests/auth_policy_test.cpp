#include "../src/auth/session_policy.hpp"
#include <cassert>
#include <limits>
#include <string>

int main()
{
    using namespace auth::policy;
    constexpr std::int64_t now = 1'800'000'000'000;
    assert(valid_times(now, now + maximum_session_ms, now));
    assert(valid_times(now, now + 1, now)); // License expires before normal session TTL.
    assert(!valid_times(now, now, now));
    assert(!valid_times(now, now + maximum_session_ms + 1, now));
    assert(!valid_times(now + clock_skew_ms + 1, now + maximum_session_ms, now));
    assert(!valid_times(0, now + 1, now));
    assert(!valid_times(now, std::numeric_limits<std::int64_t>::max(), now));
    assert(before_deadline(999, 1000));
    assert(!before_deadline(1000, 1000));
    assert(!before_deadline(1, 0));
    assert(valid_license("ONESTEP-12345678"));
    assert(!valid_license("short"));
    assert(!valid_license("KEY\"WITHQUOTE"));
    assert(!valid_license(std::string(129, 'A')));
    assert(valid_domain("auth.example.com"));
    assert(!valid_domain("https://auth.example.com"));
    assert(!valid_domain("auth.example.com/path"));
    assert(!valid_domain("user@auth.example.com"));
    assert(!valid_domain("auth..example.com"));
    assert(!valid_domain("-auth.example.com"));
    assert(!valid_domain("auth-.example.com"));
    assert(!valid_domain("auth.example.com."));
}
