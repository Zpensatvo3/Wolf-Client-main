#define main existing_config_fields_round_trip_test
#include "config_fields_test.cpp"
#undef main
#include "../src/modules/combat/settings/shield_breaker_settings.hpp"
#include "../src/modules/combat/settings/anti_bot_settings.hpp"
#include "../src/modules/client/settings/notifications_settings.hpp"
#include "../src/modules/combat/settings/mace_swap_settings.hpp"
#include "../src/modules/combat/settings/hit_crystal_settings.hpp"
#include "../src/modules/combat/settings/crystal_aura_settings.hpp"
#include "../src/modules/combat/settings/auto_totem_settings.hpp"
#include "../src/modules/misc/settings/left_clicker_settings.hpp"
#include "../src/modules/misc/settings/right_clicker_settings.hpp"
#include "../src/modules/misc/settings/inventory_clicker_settings.hpp"
#include "../src/modules/misc/settings/fast_exp_settings.hpp"
#include "../src/modules/misc/settings/trajectories_settings.hpp"
#include "../src/modules/visual/settings/glow_esp_settings.hpp"
#include "../src/modules/visual/settings/player_esp_settings.hpp"
#include "../src/modules/visual/settings/storage_esp_settings.hpp"
#include "../src/modules/visual/settings/item_esp_settings.hpp"
#include "../src/modules/visual/settings/block_esp_settings.hpp"
#include "../src/modules/visual/settings/radar_settings.hpp"
#include "../src/modules/visual/settings/spotify_hud_settings.hpp"
#include "../src/modules/visual/settings/nametag_settings.hpp"
#include "../src/modules/visual/settings/chams_settings.hpp"
#include "../src/modules/render/settings/hud_settings.hpp"
#include "../src/modules/render/settings/arraylist_settings.hpp"
#include "../src/modules/combat/anchor_config.hpp"
#include "../src/modules/combat/settings/anchor_macro_settings.hpp"
#include "../src/modules/combat/blatant_aim_config.hpp"
#include "../src/modules/combat/legit_aim_config.hpp"
#include <limits>
#include <cassert>
#include <set>
#include "../src/modules/core/settings_store.hpp"

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

class anchor_fixture final : public modules::module
{
public:
    anchor_fixture(anchor_engine::profile p) : module("anchor_fixture"), mode(p) {}
    anchor_engine::profile mode;
    anchor_engine::config settings;
    void visit_config(modules::config_visitor& v) override { modules::settings::visit(v, settings, anchor_engine::schema(mode)); }
};
class blatant_fixture final : public modules::module
{
public:
    blatant_fixture() : module("blatant_fixture") {}
    blatant_aim_engine::config settings;
    void visit_config(modules::config_visitor& v) override { blatant_aim_engine::visit_config(v, settings); }
};

int main()
{
    check_contract<module_settings::anchor_macro>({"mode",
        "legit_switch_delay","legit_place_delay","legit_charge_delay","legit_detonate_delay","legit_return_delay",
        "legit_randomize","legit_variance","legit_hybrid_slot_input","legit_camera_movement","legit_camera_strength",
        "blatant_switch_delay","blatant_place_delay","blatant_charge_delay","blatant_detonate_delay","blatant_return_delay",
        "blatant_randomize","blatant_variance","blatant_hybrid_slot_input","blatant_camera_movement","blatant_camera_strength"});
    settings_fixture<module_settings::anchor_macro> macro;
    assert(macro.apply_config_settings(R"({"mode":1,"blatant_place_delay":9999,"blatant_variance":-2})"));
    assert(macro.settings.read().blatant_place_delay == 500 && macro.settings.read().blatant_variance == 0);
    const auto macro_saved = macro.export_config_settings();
    assert(!macro.apply_config_settings(R"({"mode":2,"legit_switch_delay":5})"));
    assert(macro.export_config_settings() == macro_saved);
    check_contract<module_settings::crystal_aura>({"target_mode","mob_ground_test","action_delay_ms","switch_delay_ms","launch_timeout_ms","confirm_timeout_ms"});
    settings_fixture<module_settings::crystal_aura> aura;
    assert(aura.apply_config_settings(R"({"action_delay_ms":0,"switch_delay_ms":9999,"launch_timeout_ms":1,"confirm_timeout_ms":9999})"));
    const auto aura_settings = aura.settings.read();
    assert(aura_settings.action_delay_ms == 50 && aura_settings.switch_delay_ms == 300);
    assert(aura_settings.launch_timeout_ms == 300 && aura_settings.confirm_timeout_ms == 3000);
    assert(aura_settings.target_mode == 0 && aura_settings.mob_ground_test); // Old configs still target players only.
    assert(aura.apply_config_settings(R"({"target_mode":2,"mob_ground_test":false})"));
    assert(aura.settings.read().target_mode == 2 && !aura.settings.read().mob_ground_test);
    const auto aura_saved = aura.export_config_settings();
    assert(!aura.apply_config_settings(R"({"target_mode":3,"mob_ground_test":true})"));
    assert(aura.export_config_settings() == aura_saved);
    using namespace module_settings;
    check_contract<notifications>({"position","duration","max_visible","module_changes","operation_results"});
    assert(notifications{}.module_changes && notifications{}.operation_results && notifications{}.max_visible == 3);
    check_contract<anti_bot>({"check_player_list","spawn_grace_ticks","missing_list_ticks"});
    assert(anti_bot_default_enabled && anti_bot{}.check_player_list);
    assert(anti_bot{}.spawn_grace_ticks == 40 && anti_bot{}.missing_list_ticks == 40);
    check_contract<shield_breaker>({"switch_delay","return_delay","axe_slot"});
    check_contract<mace_swap>({"preferred_mace_slot","return_delay"});
    check_contract<hit_crystal>({"auto_switch","restore_slot","place_delay","break_delay"});
    check_contract<auto_totem>({"mode","fill_offhand","fill_hotbar","auto_open_inventory","offhand_first","pause_with_cursor_item","hotbar_slot","delay_ms","timing_randomness_ms","initial_reaction_ms","confirmation_timeout_ms",
        "legit_reaction_delay_ms","legit_action_delay_ms","legit_timing_variation_ms","legit_confirmation_timeout_ms","legit_hotbar_slot","legit_health_threshold","legit_fill_offhand","legit_fill_hotbar","legit_offhand_first","legit_low_health_only","legit_cursor_guided_swap","legit_cursor_travel_ms","legit_pause_with_cursor_item","legit_preserve_hotbar"});
    check_contract<left_clicker>({"min_cps","max_cps","jitter_percent","jitter_camera","jitter_amount","fatigue_enabled","fatigue_factor","glfw_input"});
    check_contract<right_clicker>({"min_cps","max_cps","jitter_percent","jitter_camera","jitter_amount","fatigue_enabled","fatigue_factor","glfw_input","only_blocks"});
    check_contract<inventory_clicker>({"min_cps","max_cps","click_delay"});
    check_contract<fast_exp>({"min_cps","max_cps","cps"});
    check_contract<trajectories>({"show_bows","show_throwables","show_potions","show_landing_marker","show_flight_time","line_thickness","max_ticks"});
    check_contract<glow_esp>({"box_thickness","show_names","show_health","show_distance","fov","use_game_fov"});
    check_contract<player_esp>({"esp_mode","box_thickness","fill_opacity","show_names","show_health","show_distance","fov","use_game_fov","use_theme_color","esp_color"});
    check_contract<storage_esp>({"show_labels","show_distance","box_style"});
    check_contract<item_esp>({"show_boxes","show_names","show_count","show_distance","use_theme_color","color","line_thickness","fill_opacity"});
    check_contract<block_esp>({"line_thickness","fill_opacity","selected_blocks"});
    check_contract<radar>({"size","range","position_x","position_y","background_opacity","rotate_with_player","show_names","show_distance_rings","show_cardinals","show_player_count"});
    check_contract<spotify_hud>({"position_x","position_y","scale","background_opacity","show_artwork","show_progress","show_visualizer","show_brand","show_lyrics","lyric_lines","enable_lyrics","show_when_idle"});
    check_contract<nametag>({"scale","show_health","show_armor"});
    check_contract<chams>({"opacity","glow","glow_strength","glow_radius","through_walls","theme_color","ignore_friends","visible_color","hidden_color"});
    check_contract<hud>({"scale","position_x","position_y"});
    check_contract<arraylist>({"scale","position_x","position_y","bar_glow","text_shadow","bg_opacity","bar_width","show_keybinds"});

    settings_fixture<block_esp> blocks;
    assert(blocks.apply_config_settings(R"({"selected_blocks":["minecraft:stone","minecraft:diamond_ore"]})"));
    assert(blocks.settings.read().selected_blocks == (std::vector<std::string>{"minecraft:stone","minecraft:diamond_ore"}));
    const auto block_saved = blocks.export_config_settings();
    assert(!blocks.apply_config_settings(R"({"selected_blocks":["minecraft:stone",12],"range":4})"));
    assert(blocks.export_config_settings() == block_saved);

    settings_fixture<chams> colors;
    assert(colors.apply_config_settings(R"({"visible_color":[-1,2,0.5,0.8],"range":200})"));
    const auto c = colors.settings.read();
    assert(c.visible_color[0] == 0 && c.visible_color[1] == 1 && c.visible_color[2] == .5f);
    assert(colors.export_config_settings().find("\"range\"") == std::string::npos);
    // Old range values import harmlessly but are never re-exported or applied.
    settings_fixture<storage_esp> storage;
    assert(storage.apply_config_settings(R"({"range":4,"show_labels":false})"));
    assert(!storage.settings.read().show_labels);
    assert(storage.export_config_settings().find("\"range\"") == std::string::npos);
    assert(blocks.apply_config_settings(R"({"range":3})"));
    assert(blocks.export_config_settings().find("\"range\"") == std::string::npos);
    settings_fixture<nametag> tags;
    assert(tags.apply_config_settings(R"({"max_distance":16,"show_armor":false})"));
    assert(!tags.settings.read().show_armor);
    assert(tags.export_config_settings().find("max_distance") == std::string::npos);
    const auto color_saved = colors.export_config_settings();
    assert(!colors.apply_config_settings(R"({"visible_color":[0,0,0]})"));
    assert(!colors.apply_config_settings(R"({"hidden_color":[0,0,1e999,1]})"));
    assert(colors.export_config_settings() == color_saved);

    settings_fixture<fast_exp> exp;
    assert(exp.apply_config_settings(R"({"cps":24})"));
    assert(exp.settings.read().min_cps == 22 && exp.settings.read().max_cps == 26 && exp.settings.read().legacy_cps == 0);
    assert(exp.apply_config_settings(R"({"min_cps":10,"max_cps":12,"cps":30})"));
    assert(exp.settings.read().min_cps == 10 && exp.settings.read().max_cps == 12);
    settings_fixture<inventory_clicker> inventory;
    assert(inventory.apply_config_settings(R"({"click_delay":135})") && inventory.settings.read().click_delay == 135);
    assert(exp.apply_config_settings(R"({"min_cps":1,"max_cps":50})"));
    assert(exp.settings.read().min_cps == 1 && exp.settings.read().max_cps == 50);
    settings_fixture<auto_totem> totem;
    assert(totem.apply_config_settings(R"({"delay_ms":900,"initial_reaction_ms":800,"timing_randomness_ms":220})"));
    assert(totem.settings.read().delay_ms == 900 && totem.settings.read().initial_reaction_ms == 800 &&
        totem.settings.read().timing_randomness_ms == 220);
    settings_fixture<auto_totem> legit_totem;
    assert(legit_totem.apply_config_settings(R"({"mode":1,"legit_reaction_delay_ms":1100,"legit_action_delay_ms":45,"legit_timing_variation_ms":240})"));
    assert(legit_totem.settings.read().legit_reaction_delay_ms == 1100 && legit_totem.settings.read().legit_action_delay_ms == 45 &&
        legit_totem.settings.read().legit_timing_variation_ms == 240);
    settings_fixture<radar> radar_overlay;
    assert(radar_overlay.apply_config_settings(R"({"position_x":2100,"position_y":1300})"));
    assert(radar_overlay.settings.read().position_x == 2100 && radar_overlay.settings.read().position_y == 1300);

    for (auto mode : {anchor_engine::profile::legit, anchor_engine::profile::blatant, anchor_engine::profile::safe}) {
        anchor_fixture anchor(mode);
        const auto saved = anchor.export_config_settings();
        assert(anchor.validate_config_settings(saved));
        assert(anchor.apply_config_settings(saved));
        assert((saved.find("\"aim_delay\"") != std::string::npos) == (mode == anchor_engine::profile::safe));
        assert(anchor.apply_config_settings(R"({"camera_jitter":false,"jitter_ms":55,"camera_jitter_strength":0.7,"switch_delay":12})"));
        assert(!anchor.settings.camera_movement && anchor.settings.timing_variance == 55 &&
            anchor.settings.camera_strength == .7f && anchor.settings.switch_delay == 12);
        const auto before = anchor.export_config_settings();
        assert(!anchor.apply_config_settings(R"({"action_delay":45,"camera_jitter":"bad"})"));
        assert(anchor.export_config_settings() == before);
        for (const auto& d : anchor_engine::schema(mode))
            if (mode == anchor_engine::profile::blatant && std::string(d.key) != "hybrid_slot_input") assert(!d.visible);
    }
    blatant_fixture aim;
    for (int value : {1,3,4}) {
        assert(aim.apply_config_settings("{\"aim_point\":" + std::to_string(value) + "}"));
        assert(static_cast<int>(aim.settings.point) == value);
    }
    assert(aim.apply_config_settings(R"({"aim_point":0})") && aim.settings.point == blatant_aim_engine::aim_point::body);
    assert(aim.apply_config_settings(R"({"aim_point":2})") && aim.settings.point == blatant_aim_engine::aim_point::adaptive);
    const auto aim_saved = aim.export_config_settings();
    assert(!aim.apply_config_settings(R"({"aim_point":9,"fov":15})"));
    assert(aim.export_config_settings() == aim_saved);
    auto color = chams{};
    color.visible_color[2] = std::numeric_limits<float>::infinity();
    assert(!modules::settings::normalize(color, chams::schema()));
    auto legit = legit_aim_engine::config{};
    legit.point = static_cast<legit_aim_engine::aim_point>(3);
    assert(!modules::settings::normalize(legit, legit_aim_engine::schema()));
    std::cout << "Remaining module contracts: stable keys, defaults, colors, lists, sparse enums, legacy EXP/anchor keys and atomic imports passed\n";
}
