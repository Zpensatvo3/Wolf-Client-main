#define THEDORIUS_UI_NO_REDIRECT
#include "modules.hpp"
#undef THEDORIUS_UI_NO_REDIRECT
#include "module_catalog.hpp"

namespace
{
    using module_factory = modules::module* (*)();

    template <typename Module>
    modules::module* create_module()
    {
        return new Module();
    }

    modules::module* create_anchor() { return new modules::anchor_macro(); }
    modules::module* create_blatant_anchor() { return new modules::anchor_macro(true); }

    struct descriptor
    {
        const char* stable_id;
        module_factory create;
    };

    const descriptor default_modules[] = {
        { "friends_manager", &create_module<modules::friend_manager> },
        { "click_gui", &create_module<modules::click_gui> },
        { "panic", &create_module<modules::panic> },
        { "legit_click_aim_assist", &create_module<modules::aimassist> },
        { "blatant_click_aim_assist", &create_module<modules::blatant_clickaimassist> },
        { "aim_assist", &create_module<modules::clickaimassist> },
        { "triggerbot", &create_module<modules::triggerbot> },
        { "mace_swap", &create_module<modules::mace_swap> },
        { "anchor_macro", &create_anchor },
        { "blatant_anchor_macro", &create_blatant_anchor },
        { "safe_anchor", &create_module<modules::safe_anchor> },
        { "hit_crystal", &create_module<modules::hit_crystal> },
        { "watermark", &create_module<modules::hud> },
        { "arraylist", &create_module<modules::arraylist> },
        { "glow_esp", &create_module<modules::glow_esp> },
        { "chams", &create_module<modules::chams> },
        { "player_esp", &create_module<modules::player_esp> },
        { "storage_esp", &create_module<modules::storage_esp> },
        { "block_esp", &create_module<modules::block_esp> },
        { "radar", &create_module<modules::radar> },
        { "spotify_hud", &create_module<modules::spotify_hud> },
        { "full_bright", &create_module<modules::full_bright> },
        { "nametag", &create_module<modules::nametag> },
        { "jump_reset", &create_module<modules::jump_reset> },
        { "shield_breaker", &create_module<modules::shield_breaker> },
        { "auto_totem", &create_module<modules::auto_totem> },
        { "legit_auto_totem", &create_module<modules::legit_auto_totem> },
        { "pearl_macro", &create_module<modules::pearl_macro> },
        { "chest_stealer", &create_module<modules::chest_stealer> },
        { "elytra_swapper", &create_module<modules::elytra_swapper> },
        { "auto_tool", &create_module<modules::auto_tool> },
        { "auto_armor", &create_module<modules::auto_armor> },
        { "sprint", &create_module<modules::sprint> },
        { "trajectories", &create_module<modules::trajectories> },
        { "bridge_assist", &create_module<modules::bridge_assist> },
        { "right_clicker", &create_module<modules::right_clicker> },
        { "left_clicker", &create_module<modules::left_clicker> },
        { "inventory_clicker", &create_module<modules::inventory_clicker> },
        { "fast_place", &create_module<modules::fast_place> },
        { "fast_exp", &create_module<modules::fast_exp> },
        { "pot_refill", &create_module<modules::pot_refill> },
        { "mouse_tweaks", &create_module<modules::mouse_tweaks> },
        { "vape_aim_assist", &modules::create_vape_aim },
        { "vape_left_clicker", &modules::create_vape_left },
        { "vape_right_clicker", &modules::create_vape_right },
    };
}

void modules::register_default_modules()
{
    for (const descriptor& entry : default_modules)
    {
        module* instance = entry.create();
        if (!instance || std::string_view(instance->get_id()) != entry.stable_id)
        {
            delete instance;
            continue;
        }
        register_module(instance);
    }
}
