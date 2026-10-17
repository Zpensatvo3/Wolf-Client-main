#pragma once

#include "../auth/client.hpp"
#include <charconv>
#include <chrono>
#include <optional>
#include <string>
#include <string_view>

namespace gui::profile
{
    // Presentation only: this must never grant access or alter session policy.
    inline std::optional<std::chrono::sys_time<std::chrono::milliseconds>> expiry_time(std::string_view text)
    {
        using namespace std::chrono;
        if (text.size() < 20 || text.size() > 24 || text[4] != '-' || text[7] != '-' ||
            text[10] != 'T' || text[13] != ':' || text[16] != ':' || text.back() != 'Z') return {};
        const auto number = [&](std::size_t offset, std::size_t count) -> int
        {
            int value = 0;
            const char* first = text.data() + offset;
            for (std::size_t i = 0; i < count; ++i) if (first[i] < '0' || first[i] > '9') return -1;
            const auto result = std::from_chars(first, first + count, value);
            return result.ec == std::errc{} && result.ptr == first + count ? value : -1;
        };
        const int y = number(0, 4), m = number(5, 2), d = number(8, 2);
        const int h = number(11, 2), minute = number(14, 2), second = number(17, 2);
        const year_month_day date{year(y), month(static_cast<unsigned>(m)), day(static_cast<unsigned>(d))};
        if (y < 1970 || !date.ok() || h < 0 || h > 23 || minute < 0 || minute > 59 || second < 0 || second > 59) return {};
        int fraction = 0;
        if (text.size() != 20)
        {
            if (text[19] != '.' || text.size() < 22) return {};
            const auto digits = text.size() - 21;
            fraction = number(20, digits);
            if (fraction < 0) return {};
            for (std::size_t i = digits; i < 3; ++i) fraction *= 10;
        }
        return sys_days(date) + hours(h) + minutes(minute) + seconds(second) + milliseconds(fraction);
    }

    inline std::string license_label(const auth::client::view_state& license,
        std::chrono::sys_time<std::chrono::milliseconds> now)
    {
        using namespace std::chrono;
        if (license.status == auth::client::phase::disabled) return "Development";
        if (license.status != auth::client::phase::authorized) return "Not signed in";
        if (license.expires_at == "lifetime") return "Lifetime";
        const auto expiry = expiry_time(license.expires_at);
        if (!expiry) return "Unavailable"; // Missing data is NOT a lifetime license.
        if (*expiry <= now) return "Expired";
        const auto remaining = *expiry - now;
        const auto days_left = ceil<days>(remaining).count();
        if (remaining >= days(1)) return std::to_string(days_left) + (days_left == 1 ? " day" : " days");
        if (remaining >= hours(1))
        {
            const auto count = ceil<hours>(remaining).count();
            return std::to_string(count) + (count == 1 ? " hour" : " hours");
        }
        const auto count = ceil<minutes>(remaining).count();
        return std::to_string(count) + (count == 1 ? " minute" : " minutes");
    }
}
