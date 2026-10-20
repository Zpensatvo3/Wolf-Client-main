// Session-setter tests were replaced by signed-lease tests in auth_v2_native_test.cpp.
#include "../src/auth/auth.hpp"
#include <cassert>
int main(){
    auth::controlled_shutdown();
    for(auto scope:{auth::scope::modules,auth::scope::input,auth::scope::bridge,auth::scope::configs,auth::scope::render})
        assert(!auth::permitted(scope));
    auth::start_heartbeat_monitor("unused.invalid");
    assert(!auth::is_authenticated());
    auth::controlled_shutdown();
    auth::stop_heartbeat_monitor();
}
