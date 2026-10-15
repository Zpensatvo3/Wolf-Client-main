#include "../src/services/notifications/notification_service.hpp"
#include <cassert>
#include <iostream>
#include <thread>

int main()
{
    using namespace services::notifications;
    module_settings::notifications config;
    queue messages;
    messages.push({0, "module:aim", "Aim Assist", {}, kind::module_on}, 0, config.duration);
    const auto initial = messages.collect(0, config).front();
    assert(animation(initial, 0, config.duration, false).alpha == 0);
    assert(animation(initial, .3, config.duration, false).alpha == 1);
    assert(animation(initial, 3.39, config.duration, false).alpha < .01f);
    assert(animation(initial, config.duration, config.duration, false).remaining == 0);
    assert(animation(initial, 0, config.duration, true).alpha == 1);
    messages.push({0, "module:aim", "Aim Assist", {}, kind::module_off}, 1, config.duration);
    auto current = messages.collect(1, config);
    assert(current.size() == 1 && current[0].id == initial.id && current[0].created == 0);
    assert(current[0].updated == 1 && current[0].type == kind::module_off);
    assert(messages.collect(3.5, config).size() == 1);
    assert(messages.collect(4.5, config).empty());
    for (int i = 0; i < 100; ++i) messages.push({0, std::to_string(i), "Test", {}, kind::info}, 5, config.duration);
    assert(messages.size() == queue::capacity && messages.collect(5, config).size() == 3);
    assert(messages.collect(5, config).front().key == "99");
    assert(messages.collect(4, config).empty()); // Clock rollback cannot revive old cards.
    assert(bounded_text("ab\xC4\xB1" "cd", 3) == "ab");
    assert(bounded_text("ab\xC4\xB1" "cd", 4) == "ab\xC4\xB1");
    float at60 = 0, at120 = 0;
    for (int i = 0; i < 60; ++i) at60 = approach(at60, 100, 1.f / 60, false);
    for (int i = 0; i < 120; ++i) at120 = approach(at120, 100, 1.f / 120, false);
    assert(std::abs(at60 - at120) < .001f && approach(0, 100, .016f, true) == 100);

    configure(true, config);
    module_changed("panic", "panic", true);
    module_changed("click_gui", "click gui", true);
    module_changed("notifications", "notifications", true);
    assert(collect().entries.empty()); // No panic/GUI/controller animation.
    module_changed("aim_assist", "aim assist", true);
    module_changed("aim_assist", "aim assist", false);
    assert(collect().entries.size() == 1 && collect().entries.front().type == kind::module_off);
    operation_result("Saved", true); operation_result("Saved", true);
    assert(collect().entries.size() == 2);
    configure(false, config);
    operation_result("Disabled must stay silent", false);
    assert(collect().entries.empty());
    config.module_changes = false; config.operation_results = false;
    configure(true, config);
    module_changed("aim", "aim", true); operation_result("Hidden");
    assert(collect().entries.empty());
    config.module_changes = config.operation_results = true;
    configure(true, config);
    std::vector<std::thread> producers;
    for (int thread = 0; thread < 8; ++thread)
        producers.emplace_back([thread] {
            for (int i = 0; i < 100; ++i) operation_result(std::to_string(thread) + ":" + std::to_string(i), i % 2 == 0);
        });
    for (auto& thread : producers) thread.join();
    assert(collect().entries.size() == 3);
    clear(); assert(collect().entries.empty());
    std::cout << "Notifications: bounded/coalesced queue, fade/slide timing, Reduce Motion, thread safety, preferences and silent panic passed\n";
}
