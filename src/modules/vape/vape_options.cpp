#include "vape_module.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include <algorithm>
#include <cmath>

float modules::vape_module::get_options_height()
{
    switch (kind) {
    case vape_kind::aim: return 470.f;
    case vape_kind::bow: return 220.f;
    case vape_kind::left: return 220.f;
    case vape_kind::right: return 250.f;
    case vape_kind::sprint: return 70.f;
    case vape_kind::trigger: return 430.f;
    }
    return 45.f;
}
void modules::vape_module::render_options()
{
    std::lock_guard lock(settings_mutex);
    const auto before = settings;
    auto& c = settings;
    if (kind == vape_kind::left || kind == vape_kind::right) {
        float cps[2]{c.min_cps,c.max_cps};
        gui::option_range("CPS",cps,1.f,30.f,"%.1f");
        c.min_cps = cps[0]; c.max_cps = cps[1];
        const char* modes[]{"Normal","Extra","Extra+"};
        gui::option_combo(gui::text("Randomization","Rastgelelik"),&c.randomization,modes,3);
        gui::option_toggle("Jitter",&c.jitter);
        if (kind == vape_kind::left) {
            gui::option_toggle(gui::text("Weapons Only","Sadece silah"),&c.weapons_only);
            gui::option_toggle(gui::text("Mining Protection","Kazma koruması"),&c.protect_blocks);
        } else {
            gui::option_toggle(gui::text("Hold to Click","Basılı tutarken"),&c.hold_to_click);
            gui::option_toggle(gui::text("Blocks Only","Sadece blok"),&c.blocks_only);
            gui::option_toggle(gui::text("Allow Offhand","Sol ele izin ver"),&c.offhand);
        }
    } else if (kind == vape_kind::aim) {
        const char* engines[]{"Simple","Adaptive"};
        gui::option_combo(gui::text("Engine","Motor"),&c.aim_mode,engines,2);
        {
            const char* areas[]{gui::text("Center","Merkez"),gui::text("Closest","En yakın")};
            gui::option_combo(gui::text("Target Area","Hedef alanı"),&c.target_area,areas,2);
        }
        gui::option_slider(gui::text("Horizontal Speed","Yatay hız"),&c.horizontal,.1f,20.f,"%.1f");
        gui::option_toggle(gui::text("Vertical Aim","Dikey nişan"),&c.aim_vertical);
        if (c.aim_vertical) gui::option_slider(gui::text("Vertical Speed","Dikey hız"),&c.vertical,.1f,20.f,"%.1f");
        gui::option_slider(gui::text("Range","Menzil"),&c.range,1.f,8.f,"%.1f m");
        gui::option_slider("FOV",&c.fov,10.f,360.f,"%.0f");
        const char* sorting[]{gui::text("Angle","Açı"),gui::text("Distance","Mesafe"),
            gui::text("Health","Can"),gui::text("Armor","Zırh"),gui::text("Threat","Tehdit")};
        gui::option_combo(gui::text("Priority","Öncelik"),&c.sort,sorting,5);
        gui::option_toggle(gui::text("Strafe Increase","Strafe takviyesi"),&c.strafe_increase);
        gui::option_toggle(gui::text("On Click Only","Sadece tıklarken"),&c.require_mouse);
        gui::option_toggle(gui::text("Weapons Only","Sadece silah"),&c.weapons_only);
        gui::option_toggle(gui::text("Mining Protection","Kazma koruması"),&c.protect_blocks);
    } else if (kind == vape_kind::bow) {
        gui::option_slider(gui::text("Aim Speed","Nişan hızı"),&c.bow_speed,.1f,20.f,"%.1f");
        gui::option_slider(gui::text("Range","Menzil"),&c.range,5.f,100.f,"%.0f m");
        gui::option_slider("FOV",&c.fov,10.f,360.f,"%.0f");
        gui::option_toggle(gui::text("Movement Prediction","Hareket tahmini"),&c.prediction);
        gui::option_toggle(gui::text("High Arc","Yüksek kavis"),&c.high_arc);
    } else if (kind == vape_kind::sprint) {
        gui::option_toggle(gui::text("Stop When Invisible","Görünmezken dur"),&c.cancel_invis);
    } else {
        int delay[2]{c.mouse_delay_min,c.mouse_delay_max};
        gui::option_range(gui::text("Mouse Over Delay","Hedef bekleme"),delay,0,200,"%d ms");
        c.mouse_delay_min = delay[0]; c.mouse_delay_max = delay[1];
        float extra[2]{c.delay_min,c.delay_max};
        gui::option_range(gui::text("Extra Delay","Ek gecikme"),extra,-3.f,5.f,"%.1f ticks");
        c.delay_min = extra[0]; c.delay_max = extra[1];
        gui::option_slider(gui::text("Early Hit Chance","Erken vuruş şansı"),&c.early_chance,0.f,100.f,"%.0f%%");
        gui::option_slider(gui::text("Miss Chance","Iskalama şansı"),&c.miss_chance,0.f,100.f,"%.0f%%");
        gui::option_toggle(gui::text("Air Crits","Havada kritik"),&c.air_crits);
        gui::option_toggle(gui::text("Shield Check","Kalkan kontrolü"),&c.shield_check);
        gui::option_toggle(gui::text("Select First Hit","İlk vuruşu bekle"),&c.first_hit);
        gui::option_toggle(gui::text("Weapons Only","Sadece silah"),&c.weapons_only);
        gui::option_toggle(gui::text("On Click Only","Sadece tıklarken"),&c.require_mouse);
    }
    if (enabled) if (const auto* reason = conflict()) ImGui::TextWrapped("%s",reason);
    if (enabled && !runtime_issue.empty()) ImGui::TextWrapped("%s",runtime_issue.c_str());
    if (!(before == settings)) reset_requested.store(true);
}

void modules::vape_module::visit_config(config_visitor& v)
{
    std::lock_guard lock(settings_mutex);
    auto c = settings;
    if (v.importing()) v.stage_values(true);
    if (kind == vape_kind::left || kind == vape_kind::right) {
        v.field("min_cps",c.min_cps); v.field("max_cps",c.max_cps);
        v.bounded_enum("randomization",c.randomization,0,2); v.field("jitter",c.jitter);
        if (kind == vape_kind::left) {
            v.field("weapons_only",c.weapons_only); v.field("protect_blocks",c.protect_blocks);
        } else {
            v.field("hold_to_click",c.hold_to_click); v.field("blocks_only",c.blocks_only); v.field("offhand",c.offhand);
        }
    } else if (kind == vape_kind::aim) {
        v.field("horizontal_speed",c.horizontal); v.field("vertical_speed",c.vertical);
        v.field("aim_vertical",c.aim_vertical); v.field("range",c.range); v.field("fov",c.fov);
        v.bounded_enum("sort",c.sort,0,4); v.field("strafe_increase",c.strafe_increase);
        v.bounded_enum("engine",c.aim_mode,0,1); v.bounded_enum("target_area",c.target_area,0,1);
        v.field("require_mouse",c.require_mouse); v.field("weapons_only",c.weapons_only); v.field("protect_blocks",c.protect_blocks);
    } else if (kind == vape_kind::bow) {
        v.field("aim_speed",c.bow_speed); v.field("range",c.range); v.field("fov",c.fov);
        v.field("prediction",c.prediction); v.field("high_arc",c.high_arc);
    } else if (kind == vape_kind::sprint) {
        v.field("cancel_invis",c.cancel_invis);
    } else {
        v.field("mouse_delay_min",c.mouse_delay_min); v.field("mouse_delay_max",c.mouse_delay_max);
        v.field("extra_delay_min",c.delay_min); v.field("extra_delay_max",c.delay_max);
        v.field("early_chance",c.early_chance); v.field("miss_chance",c.miss_chance);
        v.field("air_crits",c.air_crits); v.field("shield_check",c.shield_check); v.field("first_hit",c.first_hit);
        v.field("weapons_only",c.weapons_only); v.field("require_mouse",c.require_mouse);
    }
    if (!v.importing()) return;
    const bool valid = std::isfinite(c.min_cps) && std::isfinite(c.max_cps) && std::isfinite(c.horizontal) &&
        std::isfinite(c.vertical) && std::isfinite(c.range) && std::isfinite(c.fov) && std::isfinite(c.bow_speed) &&
        std::isfinite(c.miss_chance) && std::isfinite(c.early_chance) && std::isfinite(c.delay_min) && std::isfinite(c.delay_max);
    if (!valid) v.reject("Vape: non-finite settings.");
    v.stage_values(false);
    if (!v.reading() || !v.valid()) return;
    c.min_cps = std::clamp(c.min_cps,1.f,30.f); c.max_cps = std::clamp(c.max_cps,1.f,30.f);
    if (c.min_cps > c.max_cps) std::swap(c.min_cps,c.max_cps);
    c.horizontal = std::clamp(c.horizontal,.1f,20.f); c.vertical = std::clamp(c.vertical,.1f,20.f);
    c.range = std::clamp(c.range,kind == vape_kind::bow ? 5.f : 1.f,kind == vape_kind::bow ? 100.f : 8.f);
    c.fov = std::clamp(c.fov,10.f,360.f); c.bow_speed = std::clamp(c.bow_speed,.1f,20.f);
    c.miss_chance = std::clamp(c.miss_chance,0.f,100.f); c.early_chance = std::clamp(c.early_chance,0.f,100.f);
    c.delay_min = std::clamp(c.delay_min,-3.f,5.f); c.delay_max = std::clamp(c.delay_max,-3.f,5.f);
    if (c.delay_min > c.delay_max) std::swap(c.delay_min,c.delay_max);
    c.mouse_delay_min = std::clamp(c.mouse_delay_min,0,200); c.mouse_delay_max = std::clamp(c.mouse_delay_max,0,200);
    if (c.mouse_delay_min > c.mouse_delay_max) std::swap(c.mouse_delay_min,c.mouse_delay_max);
    settings = c; reset_requested.store(true);
}
