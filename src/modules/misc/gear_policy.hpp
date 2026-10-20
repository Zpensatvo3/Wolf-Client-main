#pragma once
#include <array>
#include <string>
#include <string_view>
#include <utility>

namespace modules::gear_logic
{
    inline int armor_type(std::string_view key)
    {
        if (key.ends_with("_boots") || key.ends_with(".boots")) return 0;
        if (key.ends_with("_leggings") || key.ends_with(".leggings")) return 1;
        if (key.ends_with("_chestplate") || key.ends_with(".chestplate")) return 2;
        if (key.ends_with("_helmet") || key.ends_with(".helmet")) return 3;
        return -1;
    }
    inline bool elytra(std::string_view key) { return key.ends_with("_elytra") || key.ends_with(".elytra"); }
    inline bool matches(std::string_view key, int mode) { return mode == 0 ? elytra(key) : armor_type(key) == 2; }
    inline int armor_score(std::string_view key)
    {
        if (armor_type(key) < 0) return -1;
        if (key.find("netherite") != std::string_view::npos) return 6;
        if (key.find("diamond") != std::string_view::npos) return 5;
        if (key.find("iron") != std::string_view::npos) return 4;
        if (key.find("chainmail") != std::string_view::npos) return 3;
        if (key.find("golden") != std::string_view::npos) return 2;
        if (key.find("leather") != std::string_view::npos) return 1;
        return 0;
    }
    inline int screen_slot(int index) { return index >= 9 && index < 36 ? index : index >= 0 && index < 9 ? 36 + index : -1; }
    inline std::pair<int, int> choose_armor(const std::array<std::string, 40>& keys, bool protect_elytra)
    {
        int source = -1, chosen_type = -1, highest_gain = 0;
        for (int i = 0; i < 36; ++i) {
            const int type = armor_type(keys[i]), score = armor_score(keys[i]);
            if (type < 0 || score < 0 || (protect_elytra && type == 2 && elytra(keys[38]))) continue;
            const int gain = score - armor_score(keys[36 + type]);
            if (gain > highest_gain) { highest_gain = gain; source = i; chosen_type = type; }
        }
        return {source, chosen_type};
    }
    inline int choose_swap(const std::array<std::string, 40>& keys, int mode, bool use_hotbar)
    {
        for (int i = 9; i < 36; ++i) if (matches(keys[i], mode)) return i;
        if (use_hotbar) for (int i = 0; i < 9; ++i) if (matches(keys[i], mode)) return i;
        return -1;
    }
}
