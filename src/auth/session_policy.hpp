#pragma once

#include <cstdint>
#include <string_view>

namespace auth::policy
{
    inline constexpr std::int64_t maximum_session_ms = 45 * 60 * 1000;
    inline constexpr std::int64_t clock_skew_ms = 30'000;

    inline bool valid_license(std::string_view key)
    {
        if (key.size() < 8 || key.size() > 128) return false;
        for (const char c : key)
            if (!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                (c >= '0' && c <= '9') || c == '-')) return false;
        return true;
    }

    inline constexpr bool valid_domain(std::string_view domain)
    {
        if (domain.empty() || domain.size() > 253) return false;
        std::size_t label = 0;
        char previous = 0;
        for (const char c : domain)
        {
            if (c == '.')
            {
                if (!label || previous == '-') return false;
                label = 0;
            }
            else
            {
                if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || (c == '-' && label))) return false;
                if (++label > 63) return false;
            }
            previous = c;
        }
        return label && previous != '-';
    }

    inline bool valid_times(std::int64_t issued, std::int64_t expires, std::int64_t now)
    {
        // Values near the integer limit are rejected before doing arithmetic.
        constexpr std::int64_t latest_supported_ms = 32'503'680'000'000; // year 3000
        if (now <= 0 || now > latest_supported_ms || issued <= 0 ||
            issued > latest_supported_ms || expires <= 0 || expires > latest_supported_ms)
            return false;
        return issued <= now + clock_skew_ms && expires > now && expires > issued &&
            expires - issued <= maximum_session_ms;
    }

    inline bool before_deadline(std::uint64_t now, std::uint64_t deadline)
    {
        return deadline != 0 && now < deadline;
    }
}
