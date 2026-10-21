#pragma once
#include "../anchor_config.hpp"

namespace module_settings
{
    struct anchor_macro
    {
        int execution_mode = 0; // Normal / Legit; legacy mode below remains a migration field.
        // Argon-style controls; delays are game ticks (0..20), not milliseconds.
        bool while_use = true, stop_on_kill = false, click_simulation = false;
        bool only_own = false, only_charge = false;
        int place_chance = 100, switch_delay = 0, switch_chance = 100;
        int glowstone_delay = 0, glowstone_chance = 100;
        int explode_delay = 0, explode_chance = 100, explode_slot = 1;
        // Hidden legacy profiles remain serializable for old public configs.
        int mode = 0;
        bool legit_hybrid_slot_input = false, legit_camera_movement = true, legit_randomize = true;
        float legit_camera_strength = .28f;
        int legit_variance = 14, legit_switch_delay = 28, legit_place_delay = 38;
        int legit_charge_delay = 38, legit_detonate_delay = 38, legit_return_delay = 24;
        bool blatant_hybrid_slot_input = false, blatant_camera_movement = false, blatant_randomize = false;
        float blatant_camera_strength = .28f;
        int blatant_variance = 14, blatant_switch_delay = 0, blatant_place_delay = 0;
        int blatant_charge_delay = 0, blatant_detonate_delay = 0, blatant_return_delay = 0;
        bool operator==(const anchor_macro&) const = default;
        anchor_engine::profile selected_profile() const
        { return mode == 1 ? anchor_engine::profile::blatant : anchor_engine::profile::legit; }
        anchor_engine::config selected_config() const
        {
            const bool fast = mode == 1;
            anchor_engine::config c;
            c.hybrid_slot_input = fast ? blatant_hybrid_slot_input : legit_hybrid_slot_input;
            c.camera_movement = fast ? blatant_camera_movement : legit_camera_movement;
            c.camera_strength = fast ? blatant_camera_strength : legit_camera_strength;
            c.switch_delay = fast ? blatant_switch_delay : legit_switch_delay;
            c.place_delay = fast ? blatant_place_delay : legit_place_delay;
            c.charge_delay = fast ? blatant_charge_delay : legit_charge_delay;
            c.detonate_delay = fast ? blatant_detonate_delay : legit_detonate_delay;
            c.return_delay = fast ? blatant_return_delay : legit_return_delay;
            c.timing_variance = (fast ? blatant_randomize : legit_randomize)
                ? (fast ? blatant_variance : legit_variance) : 0;
            c.separate_delays = true;
            return c;
        }
        static std::span<const modules::settings::descriptor<anchor_macro>> schema()
        {
            using d = modules::settings::descriptor<anchor_macro>;
            static const modules::settings::label execution_modes[]{{"Normal"},{"Legit"}};
            static const modules::settings::label modes[]{{"Legit"},{"Blatant"}};
            static const auto legit = +[](const anchor_macro& c) { return c.mode == 0; };
            static const auto blatant = +[](const anchor_macro& c) { return c.mode == 1; };
            static const auto fields = [] {
                std::array values{
                d::choice("execution_mode", {"Mode"}, &anchor_macro::execution_mode, execution_modes),
                d::toggle("while_use", {"While Use"}, &anchor_macro::while_use),
                d::toggle("stop_on_kill", {"Stop on Kill"}, &anchor_macro::stop_on_kill),
                d::toggle("click_simulation", {"Click Simulation"}, &anchor_macro::click_simulation),
                d::integer("place_chance", {"Place Chance"}, &anchor_macro::place_chance,0,100,"%d%%"),
                d::integer("switch_delay", {"Switch Delay"}, &anchor_macro::switch_delay,0,20,"%d ticks"),
                d::integer("switch_chance", {"Switch Chance"}, &anchor_macro::switch_chance,0,100,"%d%%"),
                d::integer("glowstone_delay", {"Glowstone Delay"}, &anchor_macro::glowstone_delay,0,20,"%d ticks"),
                d::integer("glowstone_chance", {"Glowstone Chance"}, &anchor_macro::glowstone_chance,0,100,"%d%%"),
                d::integer("explode_delay", {"Explode Delay"}, &anchor_macro::explode_delay,0,20,"%d ticks"),
                d::integer("explode_chance", {"Explode Chance"}, &anchor_macro::explode_chance,0,100,"%d%%"),
                d::integer("explode_slot", {"Explode Slot"}, &anchor_macro::explode_slot,1,9,"Slot %d"),
                d::toggle("only_own", {"Only Own"}, &anchor_macro::only_own),
                d::toggle("only_charge", {"Only Charge"}, &anchor_macro::only_charge),
                d::choice("mode", {"Mode"}, &anchor_macro::mode, modes),
                d::integer("legit_switch_delay", {"Slot Switch Delay"}, &anchor_macro::legit_switch_delay,0,500,"%d ms").when(legit),
                d::integer("legit_place_delay", {"Place Delay"}, &anchor_macro::legit_place_delay,0,500,"%d ms").when(legit),
                d::integer("legit_charge_delay", {"Charge Delay"}, &anchor_macro::legit_charge_delay,0,500,"%d ms").when(legit),
                d::integer("legit_detonate_delay", {"Detonation Delay"}, &anchor_macro::legit_detonate_delay,0,500,"%d ms").when(legit),
                d::integer("legit_return_delay", {"Return Delay"}, &anchor_macro::legit_return_delay,0,500,"%d ms").when(legit),
                d::toggle("legit_randomize", {"Randomize Delays"}, &anchor_macro::legit_randomize).when(legit),
                d::integer("legit_variance", {"Random Variation"}, &anchor_macro::legit_variance,0,150,"+/- %d ms")
                    .when([](const anchor_macro& c) { return c.mode == 0 && c.legit_randomize; }),
                d::toggle("legit_hybrid_slot_input", {"Hybrid Slot Input"}, &anchor_macro::legit_hybrid_slot_input).when(legit),
                d::toggle("legit_camera_movement", {"Camera Movement"}, &anchor_macro::legit_camera_movement).when(legit),
                d::number("legit_camera_strength", {"Camera Strength"}, &anchor_macro::legit_camera_strength,0,1.2f)
                    .when([](const anchor_macro& c) { return c.mode == 0 && c.legit_camera_movement; }),
                d::integer("blatant_switch_delay", {"Slot Switch Delay"}, &anchor_macro::blatant_switch_delay,0,500,"%d ms").when(blatant),
                d::integer("blatant_place_delay", {"Place Delay"}, &anchor_macro::blatant_place_delay,0,500,"%d ms").when(blatant),
                d::integer("blatant_charge_delay", {"Charge Delay"}, &anchor_macro::blatant_charge_delay,0,500,"%d ms").when(blatant),
                d::integer("blatant_detonate_delay", {"Detonation Delay"}, &anchor_macro::blatant_detonate_delay,0,500,"%d ms").when(blatant),
                d::integer("blatant_return_delay", {"Return Delay"}, &anchor_macro::blatant_return_delay,0,500,"%d ms").when(blatant),
                d::toggle("blatant_randomize", {"Randomize Delays"}, &anchor_macro::blatant_randomize).when(blatant),
                d::integer("blatant_variance", {"Random Variation"}, &anchor_macro::blatant_variance,0,150,"+/- %d ms")
                    .when([](const anchor_macro& c) { return c.mode == 1 && c.blatant_randomize; }),
                d::toggle("blatant_hybrid_slot_input", {"Hybrid Slot Input"}, &anchor_macro::blatant_hybrid_slot_input).when(blatant),
                d::toggle("blatant_camera_movement", {"Camera Movement"}, &anchor_macro::blatant_camera_movement).when(blatant),
                d::number("blatant_camera_strength", {"Camera Strength"}, &anchor_macro::blatant_camera_strength,0,1.2f)
                    .when([](const anchor_macro& c) { return c.mode == 1 && c.blatant_camera_movement; })
                };
                for (std::size_t i = 14; i < values.size(); ++i) values[i].visible = false;
                return values;
            }();
            return fields;
        }
        static bool visit(modules::config_visitor& v, anchor_macro& live)
        {
            auto c = live;
            const bool importing = v.importing();
            if (importing) v.stage_values(true);
            modules::settings::visit_fields(v,c,schema());
            if (!importing) return false;
            if (!modules::settings::normalize(c,schema())) v.reject("Invalid Anchor settings.");
            // Public-config merging still supplies the selected legacy profile.
            // Convert its explicit millisecond fields once, preserving new keys.
            const bool fast = c.mode == 1;
            const char* switch_key = fast ? "blatant_switch_delay" : "legit_switch_delay";
            const char* charge_key = fast ? "blatant_charge_delay" : "legit_charge_delay";
            const char* explode_key = fast ? "blatant_detonate_delay" : "legit_detonate_delay";
            const auto ticks = [](int ms) { return std::clamp((ms+49)/50,0,20); };
            if (!v.has_field("switch_delay") && v.has_field(switch_key))
                c.switch_delay = ticks(fast ? c.blatant_switch_delay : c.legit_switch_delay);
            if (!v.has_field("glowstone_delay") && v.has_field(charge_key))
                c.glowstone_delay = ticks(fast ? c.blatant_charge_delay : c.legit_charge_delay);
            if (!v.has_field("explode_delay") && v.has_field(explode_key))
                c.explode_delay = ticks(fast ? c.blatant_detonate_delay : c.legit_detonate_delay);
            v.stage_values(false);
            if (!v.reading() || !v.valid()) return false;
            live = c; return true;
        }

    };
}
