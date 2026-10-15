#include "trigger_adapter.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include <imgui/imgui.h>

void trigger_engine::render_options(config& settings, adapter_runtime& state, const char* id_suffix)
{
    ImGui::PushID(id_suffix);
    config candidate = settings;
    float cooldown[2]{candidate.cooldown[0] * 100, candidate.cooldown[1] * 100};
    if (gui::option_range("Cooldown", cooldown, 50.f, 100.f, "%.0f%%")) {
        candidate.cooldown[0] = cooldown[0] / 100; candidate.cooldown[1] = cooldown[1] / 100;
    }
    gui::option_range(gui::text("Reaction", "Tepki"), candidate.reaction, 0, 150, "%d ms");
    const char* targets[] = {gui::text("Players", "Oyuncular"), gui::text("Players + Mobs", "Oyuncular + Yaratiklar"), gui::text("All Living", "Tum Canlilar")};
    const char* weapons[] = {gui::text("Any Weapon", "Herhangi Bir Silah"), gui::text("Sword", "Kilic"), gui::text("Axe", "Balta")};
    const char* criticals[] = {gui::text("Off", "Kapali"), gui::text("Smart", "Akilli"), gui::text("Criticals Only", "Sadece Kritik")};
    int target = static_cast<int>(candidate.targets), weapon = static_cast<int>(candidate.weapon), critical = static_cast<int>(candidate.criticals);
    gui::option_combo(gui::text("Targets", "Hedefler"), &target, targets, 3);
    gui::option_combo(gui::text("Starting Weapon", "Baslangic Silahi"), &weapon, weapons, 3);
    gui::option_combo(gui::text("Criticals", "Kritikler"), &critical, criticals, 3);
    candidate.targets = static_cast<target_mode>(target); candidate.weapon = static_cast<weapon_mode>(weapon); candidate.criticals = static_cast<critical_mode>(critical);
    gui::option_toggle(gui::text("On Click Only", "Sadece Tiklarken"), &candidate.on_click);
    if (normalize(candidate) && !same_config(candidate, settings)) {
        settings = candidate; ++state.settings_revision; request_reset(state);
    }
    ImGui::PopID();
}
