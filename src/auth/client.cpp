#include "client.hpp"
#include "auth.hpp"
#include "config.hpp"
#include "session_policy.hpp"
#include "launcher_protocol.hpp"

#include <Windows.h>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace
{
    std::mutex state_mutex;
    std::condition_variable work_ready;
    auth::client::view_state state;
    std::string pending_key;
    std::thread worker;
    bool stopping = false;
    std::atomic_bool granted{false};

    void wipe(std::string& value)
    {
        if (!value.empty()) SecureZeroMemory(value.data(), value.size());
        value.clear();
    }

    void run_worker()
    {
        auto launch_ticket = auth::launch::receive_ticket();
        if (!launch_ticket.empty()) {
            {
                std::lock_guard lock(state_mutex);
                if (stopping) { auth::launch::wipe(launch_ticket); return; }
                state.status = auth::client::phase::verifying;
                state.message = "Verifying launcher session...";
            }
            auth::auth_result result;
            try {
                result = auth::redeem_launcher_ticket(launch_ticket, auth::config::server_domain,
                    auth::launch::file_hash(auth::launch::own_module_path()));
                if (result.success) auth::start_heartbeat_monitor(auth::config::server_domain);
            } catch (...) {
                auth::controlled_shutdown();
                result.success = false;
                result.message = "Could not start the launcher session. Please retry.";
            }
            auth::launch::wipe(launch_ticket);
            auth::launch::wipe(result.session_token);
            std::lock_guard lock(state_mutex);
            if (stopping) return;
            const bool accepted = result.success && auth::is_authenticated();
            granted.store(accepted, std::memory_order_release);
            state.status = accepted ? auth::client::phase::authorized : auth::client::phase::error;
            state.message = accepted ? "" : result.message;
            state.expires_at = accepted ? result.expires_at : "";
            if (accepted) {
                const auto name = L"Local\\Onestep.Launch.Ready." + std::to_wstring(GetCurrentProcessId());
                HANDLE ready = OpenEventW(EVENT_MODIFY_STATE, FALSE, name.c_str());
                if (ready) { SetEvent(ready); CloseHandle(ready); }
            }
        }
        for (;;)
        {
            std::string key;
            {
                std::unique_lock lock(state_mutex);
                work_ready.wait(lock, [] { return stopping || !pending_key.empty(); });
                if (stopping) return;
                // Copy then wipe: moving short strings leaves an SSO copy behind.
                key = pending_key;
                wipe(pending_key);
            }

            auth::auth_result result;
            try
            {
                result = auth::authenticate(key, auth::config::server_domain);
                wipe(key);
                if (result.success)
                    auth::start_heartbeat_monitor(auth::config::server_domain);
            }
            catch (...)
            {
                wipe(key);
                auth::controlled_shutdown();
                result.success = false;
                result.message = "Authentication could not be completed. Please retry.";
            }
            wipe(result.session_token);
            wipe(result.hwid);

            std::lock_guard lock(state_mutex);
            if (stopping) return; // A late response must never reopen a closing client.
            const bool accepted = result.success && auth::is_authenticated();
            state.status = accepted ? auth::client::phase::authorized : auth::client::phase::error;
            state.message = accepted ? "" : (result.success ? "The session could not start. Please retry." : result.message);
            state.expires_at = accepted ? result.expires_at : "";
            granted.store(accepted, std::memory_order_release);
        }
    }
}

void auth::client::initialize()
{
    std::lock_guard lock(state_mutex);
    state = {};
    stopping = false;
    granted.store(false, std::memory_order_release);
    if (!config::required) return;
    state.configured = policy::valid_domain(config::server_domain) && config::public_key[0];
    state.status = state.configured ? phase::awaiting_key : phase::error;
    if (!state.configured)
    {
        state.message = "Authentication is not configured for this build.";
        return;
    }
    try { worker = std::thread(run_worker); }
    catch (...)
    {
        state.configured = false;
        state.status = phase::error;
        state.message = "Authentication service could not start.";
    }
}

void auth::client::shutdown()
{
    {
        std::lock_guard lock(state_mutex);
        stopping = true;
        state.status = phase::stopping;
        granted.store(false, std::memory_order_release);
        wipe(pending_key);
    }
    work_ready.notify_all();
    if (worker.joinable()) worker.join();
    auth::controlled_shutdown();
}

bool auth::client::submit_license(std::string_view key)
{
    while (!key.empty() && (key.front() == ' ' || key.front() == '\r' || key.front() == '\n' || key.front() == '\t')) key.remove_prefix(1);
    while (!key.empty() && (key.back() == ' ' || key.back() == '\r' || key.back() == '\n' || key.back() == '\t')) key.remove_suffix(1);
    {
        std::lock_guard lock(state_mutex);
        if (!config::required || !state.configured || stopping ||
            state.status == phase::verifying || state.status == phase::authorized) return false;
        if (!policy::valid_license(key))
        {
            state.status = phase::error;
            state.message = "Enter a valid license key (8-128 letters, digits or hyphens).";
            return false;
        }
        pending_key.assign(key);
        state.status = phase::verifying;
        state.message.clear();
    }
    work_ready.notify_one();
    return true;
}

auth::client::view_state auth::client::snapshot()
{
    std::lock_guard lock(state_mutex);
    return state;
}

bool auth::client::access_allowed()
{
    return !config::required || (granted.load(std::memory_order_acquire) && auth::is_authenticated());
}

bool auth::client::login_required()
{
    return config::required && !access_allowed();
}
