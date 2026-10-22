#include "client.hpp"
#include "auth.hpp"
#include "config.hpp"
#include "launcher_protocol.hpp"
#include "../utils/version_manager.hpp"
#include <Windows.h>
#include <atomic>
#include <mutex>
#include <thread>
namespace {
    std::mutex state_mutex;
    auth::client::view_state state;
    std::thread worker;
    std::atomic_bool stopping{false};
}
void auth::client::initialize() {
    std::lock_guard lock(state_mutex);
    state={};stopping=false;
    if constexpr (!config::required) {
        state.configured=true;state.status=phase::authorized;
        state.message="";
        return;
    }
    state.configured=policy::valid_domain(config::server_domain)&&config::public_key[0];
    state.status=phase::verifying;state.message="Waiting for the OneStep launcher...";
    if(!state.configured){state.status=phase::error;state.message="Client authentication is not configured.";return;}
    try {worker=std::thread([] {
        auto handoff=auth::launch::receive_ticket();
        auth::auth_result result;
        if(!stopping&&!handoff.empty()) {
            try {
                result=auth::redeem_launcher_ticket(handoff,config::server_domain,
                    auth::launch::file_hash(auth::launch::own_module_path()),std::string(mc::target.name));
                if(result.success&&!stopping)auth::start_heartbeat_monitor(config::server_domain);
            } catch(...){auth::controlled_shutdown();result.message="Launcher session verification failed.";}
        }
        auth::launch::wipe(handoff);
        if(stopping){auth::controlled_shutdown();return;}
        const bool accepted=result.success&&auth::permitted(auth::scope::modules);
        {
            std::lock_guard guard(state_mutex);
            if(stopping)return;
            state.status=accepted?phase::authorized:phase::error;
            state.message=accepted?"":result.message.empty()?"Start OneStep from the current launcher.":result.message;
            state.expires_at=accepted?result.expires_at:"";
        }
        if(accepted) {
            const auto name=L"Local\\Onestep.Launch.Ready."+std::to_wstring(GetCurrentProcessId());
            HANDLE ready=OpenEventW(EVENT_MODIFY_STATE,FALSE,name.c_str());
            if(ready){SetEvent(ready);CloseHandle(ready);}
        }
    });}catch(...){state.status=phase::error;state.message="Launcher verification could not start.";}
}
void auth::client::shutdown() {
    stopping=true;
    auth::controlled_shutdown();
    if(worker.joinable())worker.join();
    auth::controlled_shutdown();
    std::lock_guard lock(state_mutex);state.status=phase::stopping;
}
auth::client::view_state auth::client::snapshot(){std::lock_guard lock(state_mutex);return state;}
bool auth::client::access_allowed(){return auth::permitted(auth::scope::modules);}
bool auth::client::login_required(){return !auth::permitted(auth::scope::render);}
