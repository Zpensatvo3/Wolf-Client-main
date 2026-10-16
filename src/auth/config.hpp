#pragma once
#include "session_policy.hpp"
#include <string_view>

#ifndef ONESTEP_AUTH_REQUIRED
#define ONESTEP_AUTH_REQUIRED 0
#endif
#ifndef ONESTEP_AUTH_SERVER_DOMAIN
#define ONESTEP_AUTH_SERVER_DOMAIN ""
#endif
#ifndef ONESTEP_AUTH_PUBLIC_KEY_SPKI_BASE64
#define ONESTEP_AUTH_PUBLIC_KEY_SPKI_BASE64 ""
#endif

namespace auth::config
{
    inline constexpr bool required = ONESTEP_AUTH_REQUIRED != 0;
    // Host name only. WinHTTP always uses HTTPS/443.
    inline constexpr char server_domain[] = ONESTEP_AUTH_SERVER_DOMAIN;
    inline constexpr char public_key[] = ONESTEP_AUTH_PUBLIC_KEY_SPKI_BASE64;
    constexpr bool valid_server_domain(std::string_view host)
    {
        if (!policy::valid_domain(host)) return false;
        // DNS is case-insensitive; an explicit old host must not bypass this guard.
        constexpr std::string_view legacy = "wolf-auth-api.gogogamesas.workers.dev";
        if (host.size() != legacy.size()) return true;
        for (std::size_t i = 0; i < host.size(); ++i)
        {
            const char c = host[i] >= 'A' && host[i] <= 'Z' ? host[i] + ('a' - 'A') : host[i];
            if (c != legacy[i]) return true;
        }
        return false;
    }
    static_assert(!required || valid_server_domain(server_domain),
        "OneStep auth requires an explicit HTTPS hostname; legacy Wolf fallback is unsupported.");
    static_assert(!required || sizeof(public_key) > 1,
        "OneStep auth requires an explicit public session verification key.");
}
