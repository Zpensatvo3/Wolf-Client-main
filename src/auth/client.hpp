#pragma once

#include <string>
#include <string_view>

namespace auth::client
{
    enum class phase { disabled, awaiting_key, verifying, authorized, error, stopping };

    struct view_state
    {
        phase status = phase::disabled;
        std::string message;
        std::string expires_at;
        bool configured = false;
    };

    // initialize/shutdown belong to the runtime worker. The UI may submit keys
    // and read snapshots. One owned worker serializes all login attempts.
    void initialize();
    void shutdown();
    view_state snapshot();
    bool access_allowed();
    bool login_required();
}
