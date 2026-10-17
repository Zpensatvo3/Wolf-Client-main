#include "../src/ui/profile_model.hpp"
#include <cassert>

int main()
{
    using namespace std::chrono;
    using namespace gui::profile;
    auth::client::view_state license;
    const auto now = *expiry_time("2026-10-17T12:00:00.000Z");
    assert(license_label(license, now) == "Development");
    license.status = auth::client::phase::authorized;
    assert(license_label(license, now) == "Unavailable");
    license.expires_at = "lifetime";
    assert(license_label(license, now) == "Lifetime");
    license.expires_at = "2026-11-16T12:00:00.000Z";
    assert(license_label(license, now) == "30 days");
    assert(license_label(license, now + hours(1)) == "30 days");
    assert(license_label(license, now + days(1)) == "29 days");
    assert(license_label(license, now + days(29)) == "1 day");
    assert(license_label(license, now + days(29) + hours(22)) == "2 hours");
    assert(license_label(license, now + days(29) + hours(23)) == "1 hour");
    assert(license_label(license, now + days(30) - seconds(50)) == "1 minute");
    assert(license_label(license, now + days(30)) == "Expired");
    for (const auto invalid : {"", "null", "30 days", "2026-02-29T12:00:00Z", "2024-13-01T00:00:00Z",
        "2024-01-32T00:00:00Z", "2026-10-17T24:00:00Z", "2026-10-17T12:60:00Z",
        "2026-10-17T12:00:60Z", "2026-10-17T12:00:00.Z", "2026-10-17T12:00:00.badZ",
        "2026-10-17T12:00:00.1234Z", "2026-10-17T12:00:00+03:00", "2026-10-17T12:00:00Zjunk"})
    {
        assert(!expiry_time(invalid));
        license.expires_at = invalid;
        assert(license_label(license, now) == "Unavailable");
    }
    assert(expiry_time("2024-02-29T12:00:00Z"));
    assert(expiry_time("2026-10-17T12:00:00Z") == now);
    assert(expiry_time("2026-10-17T12:00:00.1Z") == now + milliseconds(100));
    assert(expiry_time("2026-10-17T12:00:00.12Z") == now + milliseconds(120));
    license.expires_at = "lifetime";
    for (const auto phase : {auth::client::phase::awaiting_key, auth::client::phase::error,
        auth::client::phase::verifying, auth::client::phase::stopping})
    {
        license.status = phase;
        assert(license_label(license, now) == "Not signed in");
    }
}
