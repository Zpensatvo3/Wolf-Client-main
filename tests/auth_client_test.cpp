// Launcher-only lifecycle with an inert backend; no real license or server.
#include "../src/auth/auth.hpp"
#include "../src/auth/client.hpp"
#include <cassert>
#include <chrono>
#include <thread>
bool auth::permitted(scope){return false;}
auth::auth_result auth::redeem_launcher_ticket(const std::string&,const std::string&,const std::string&,const std::string&){
    return {false,"No launcher grant"};
}
void auth::start_heartbeat_monitor(const std::string&){}
void auth::controlled_shutdown(){}
int main(){
    auth::client::initialize();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(12);
    while(auth::client::snapshot().status==auth::client::phase::verifying){
        assert(std::chrono::steady_clock::now()<deadline);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto ui=auth::client::snapshot();ui.status=auth::client::phase::authorized;
    assert(!auth::client::access_allowed()&&auth::client::login_required());
    auth::client::shutdown();
    assert(!auth::client::access_allowed());
}
