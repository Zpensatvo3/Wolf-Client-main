// Direct DLL activation is intentionally gone. Full operator-run v2 service
// verification lives in launcher_live_probe.cpp; it never touches Minecraft.
#include "../src/auth/auth.hpp"
#include <iostream>
int main(){
    auth::controlled_shutdown();
    const auto result=auth::authenticate("TEST-KEY-NOT-A-LICENSE","unused.invalid");
    if(result.success||auth::permitted(auth::scope::modules))return 1;
    std::cout<<"PASS: direct DLL license login is unavailable; use the launcher v2 probe.\\n";
}
