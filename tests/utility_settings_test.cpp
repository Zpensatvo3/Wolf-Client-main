#define main existing_config_fields_round_trip_test
#include "config_fields_test.cpp"
#undef main
#include "../src/modules/misc/sprint.hpp"
#include "../src/modules/misc/settings/inv_walk_settings.hpp"
#include "../src/modules/misc/fast_place.hpp"
#include "../src/modules/misc/auto_armor.hpp"
#include "../src/modules/misc/auto_tool.hpp"
#include "../src/modules/misc/pearl_macro.hpp"
#include "../src/modules/misc/mouse_tweaks.hpp"
#include "../src/modules/misc/bridge_assist.hpp"
#include "../src/modules/misc/chest_stealer.hpp"
#include "../src/modules/misc/elytra_swapper.hpp"
#include "../src/modules/misc/pot_refill.hpp"
#include <cassert>
#include <set>

template<class Model>
class settings_fixture final : public modules::module
{
public:
    settings_fixture() : module("utility_fixture") {}
    modules::settings::store<Model> settings;
    void visit_config(modules::config_visitor& visitor) override
    {
        settings.visit(visitor, Model::schema());
    }
};

template<class Model>
void check_contract(std::initializer_list<const char*> expected)
{
    settings_fixture<Model> owner;
    const Model initial;
    const auto encoded = owner.export_config_settings();
    assert(owner.validate_config_settings(encoded) && owner.settings.read() == initial);
    assert(owner.apply_config_settings(encoded) && owner.settings.read() == initial);
    assert(owner.apply_config_settings("{}") && owner.settings.read() == initial);
    std::set<std::string> keys;
    for (const auto& d : Model::schema())
    {
        assert(keys.insert(d.key).second);
        assert(encoded.find(std::string("\"") + d.key + "\":") != std::string::npos);
        const auto saved = owner.export_config_settings();
        const std::string invalid = std::string("{\"") + d.key + "\":\"bad\"}";
        assert(!owner.validate_config_settings(invalid));
        assert(!owner.apply_config_settings(invalid));
        assert(owner.export_config_settings() == saved);
        if (d.type == modules::settings::control::integer)
        {
            for (const char* value : {"0.5", "2147483648", "-2147483649", "1e999"})
            {
                assert(!owner.apply_config_settings(std::string("{\"") + d.key + "\":" + value + "}"));
                assert(owner.export_config_settings() == saved);
            }
        }
    }
    assert(keys == std::set<std::string>(expected.begin(), expected.end()));
}

int main()
{
    using namespace module_settings;
    check_contract<sprint>({"stop_while_using_item"});
    check_contract<inv_walk>({"allow_containers", "allow_jump", "allow_sneak"});
    assert(inv_walk{}.allow_containers && inv_walk{}.allow_jump && !inv_walk{}.allow_sneak);
    check_contract<fast_place>({"delay"});
    check_contract<auto_armor>({"action_delay_ms", "protect_elytra", "work_without_inventory"});
    check_contract<auto_tool>({"require_left_click", "prefer_hotbar_only"});
    check_contract<pearl_macro>({"switch_delay", "return_delay"});
    check_contract<mouse_tweaks>({"right_click_tweak", "left_click_with_item", "left_click_without_item"});
    check_contract<bridge_assist>({"edge_offset", "unsneak_delay", "require_block_item", "only_while_moving"});
    check_contract<chest_stealer>({"min_delay_ms", "max_delay_ms", "open_reaction_ms", "random_slot_order"});
    check_contract<elytra_swapper>({"target_mode", "swap_delay_ms", "use_hotbar", "work_without_inventory"});
    check_contract<pot_refill>({"delay_ms", "random_delay", "max_delay_ms", "slot_order"});

    assert(sprint{}.stop_while_using_item && fast_place{}.delay == 0);
    assert(auto_armor{}.action_delay_ms == 120 && auto_armor{}.protect_elytra);
    assert(auto_tool{}.require_left_click && auto_tool{}.prefer_hotbar_only);
    assert(pearl_macro{}.switch_delay == 25 && pearl_macro{}.return_delay == 75);
    assert(mouse_tweaks{}.right_click_tweak && mouse_tweaks{}.left_click_with_item && mouse_tweaks{}.left_click_without_item);
    assert(bridge_assist{}.edge_offset == .08f && bridge_assist{}.unsneak_delay == 55);
    assert(bridge_assist{}.require_block_item && bridge_assist{}.only_while_moving);
    assert(chest_stealer{}.min_delay_ms == 85 && chest_stealer{}.max_delay_ms == 145);
    assert(chest_stealer{}.open_reaction_ms == 95 && chest_stealer{}.random_slot_order);
    assert(elytra_swapper{}.target_mode == 0 && elytra_swapper{}.swap_delay_ms == 70 && elytra_swapper{}.use_hotbar);
    assert(pot_refill{}.delay_ms == 100);
    assert(!pot_refill{}.random_delay && pot_refill{}.max_delay_ms == 160 && pot_refill{}.slot_order == 0);
    settings_fixture<pot_refill> refill;
    // Old configs retain their fixed speed, including delays above the new default maximum.
    assert(refill.apply_config_settings(R"({"delay_ms":700})"));
    assert(refill.settings.read().delay_ms == 700 && refill.settings.read().max_delay_ms == 700);
    assert(!refill.settings.read().random_delay && refill.settings.read().slot_order == 0);
    assert(refill.apply_config_settings(R"({"random_delay":true,"max_delay_ms":50,"slot_order":2})"));
    assert(refill.settings.read().max_delay_ms == 700 && refill.settings.read().slot_order == 2);
    const auto refill_before = refill.export_config_settings();
    assert(!refill.apply_config_settings(R"({"delay_ms":100,"slot_order":3})"));
    assert(refill.export_config_settings() == refill_before);

    settings_fixture<chest_stealer> chest;
    // Imported limits retain the old runtime allowance beyond UI slider limits.
    assert(chest.apply_config_settings(R"({"min_delay_ms":700,"max_delay_ms":500,"open_reaction_ms":700})"));
    auto c = chest.settings.read();
    assert(c.min_delay_ms == 700 && c.max_delay_ms == 700 && c.open_reaction_ms == 700);
    assert(chest.apply_config_settings(R"({"min_delay_ms":2000,"max_delay_ms":1})"));
    c = chest.settings.read();
    assert(c.min_delay_ms == 800 && c.max_delay_ms == 800);
    assert(chest.apply_config_settings(R"({"max_delay_ms":2000})"));
    assert(chest.settings.read().max_delay_ms == 1000);
    assert(chest_stealer::schema()[0].integer_display_maximum == 500);
    assert(chest_stealer::schema()[2].integer_display_maximum == 800);

    settings_fixture<fast_place> place;
    assert(place.apply_config_settings(R"({"delay":1000000})") && place.settings.read().delay == 4);
    assert(place.apply_config_settings(R"({"delay":-1000000})") && place.settings.read().delay == 0);
    // Preserve the serializer's existing global numeric limit as well.
    assert(!place.apply_config_settings(R"({"delay":2147483647})") && place.settings.read().delay == 0);
    settings_fixture<bridge_assist> bridge;
    const auto before = bridge.export_config_settings();
    assert(!bridge.apply_config_settings(R"({"edge_offset":0.2,"only_while_moving":"bad"})"));
    assert(bridge.export_config_settings() == before);
    assert(bridge.apply_config_settings(R"({"edge_offset":2,"unsneak_delay":999})"));
    assert(bridge.settings.read().edge_offset == .27f && bridge.settings.read().unsneak_delay == 300);

    settings_fixture<elytra_swapper> swap;
    assert(!swap.apply_config_settings(R"({"swap_delay_ms":100,"target_mode":2})"));
    assert(swap.settings.read() == elytra_swapper{});
    assert(swap.apply_config_settings(R"({"target_mode":1,"swap_delay_ms":0})"));
    assert(swap.settings.read().target_mode == 1 && swap.settings.read().swap_delay_ms == 20);
    settings_fixture<auto_tool> tool;
    assert(!auto_tool::schema()[1].visible);
    assert(tool.apply_config_settings(R"({"prefer_hotbar_only":false})"));
    assert(!tool.settings.read().prefer_hotbar_only);
    std::cout << "Eleven utility schemas: defaults, stable keys, hidden fields, integer limits and atomic imports passed\n";
}
