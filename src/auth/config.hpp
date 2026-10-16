#pragma once

#ifndef WOLF_AUTH_REQUIRED
#define WOLF_AUTH_REQUIRED 0
#endif
#ifndef WOLF_AUTH_SERVER_DOMAIN
#define WOLF_AUTH_SERVER_DOMAIN "wolf-auth-api.gogogamesas.workers.dev"
#endif
#ifndef WOLF_AUTH_PUBLIC_KEY_SPKI_BASE64
#define WOLF_AUTH_PUBLIC_KEY_SPKI_BASE64 ""
#endif

namespace auth::config
{
    inline constexpr bool required = WOLF_AUTH_REQUIRED != 0;
    // Host name only. WinHTTP always uses HTTPS/443.
    inline constexpr char server_domain[] = WOLF_AUTH_SERVER_DOMAIN;
    inline constexpr char public_key[] = WOLF_AUTH_PUBLIC_KEY_SPKI_BASE64;
}
