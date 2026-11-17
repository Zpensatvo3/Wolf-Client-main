#include "../src/services/input/input_service.hpp"
#include "../src/modules/core/module.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using namespace services::input;
    reset_physical_buttons(); reset_physical_keys();
    const auto start=manual_attack_generation();
    observe_physical(mouse_button::left,true);
    observe_physical(mouse_button::left,true); // Raw + window delivery is one press.
    assert(manual_attack_generation()==start+1 && physical_button_down(mouse_button::left));
    assert(physical_key_down(VK_LBUTTON));
    observe_physical(mouse_button::right,true);
    assert(manual_attack_generation()==start+1 && physical_key_down(VK_RBUTTON));
    observe_physical(mouse_button::left,false);
    observe_physical(mouse_button::left,true);
    assert(manual_attack_generation()==start+2);
    assert(is_synthetic_tag(modules::synthetic_mouse_tag));
    assert(is_synthetic_tag(static_cast<std::uint32_t>(modules::synthetic_mouse_tag)));
    assert(!is_synthetic_tag(0));
    observe_physical_key('W',0x11,true);
    assert(physical_key_down('W') && physical_scan_down(0x11));
    observe_physical_key(VK_CONTROL,0x11d,true);
    assert(physical_key_down(VK_RCONTROL) && !physical_key_down(VK_LCONTROL));
    observe_physical_key(VK_CONTROL,0x11d,false);
    assert(!physical_key_down(VK_RCONTROL));
    observe_physical_key(VK_SHIFT,0x36,true);
    assert(physical_key_down(0xa1));
    observe_physical_key(999,999,true);
    assert(!physical_key_down(999) && !physical_scan_down(999) && !physical_scan_down(0));
    reset_physical_buttons(); reset_physical_keys();
    assert(manual_attack_generation()==start+3);
    assert(!physical_button_down(mouse_button::left) && !physical_button_down(mouse_button::right));
    assert(!physical_key_down('W') && !physical_scan_down(0x11) && !physical_key_down(VK_LBUTTON));
    assert(input_test_send_calls==0);
    std::cout << "Physical input: deduplicated manual attacks, synthetic tags, key/scancode state and resets passed\n";
}
