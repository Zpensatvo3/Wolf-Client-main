#include "../src/modules/misc/gear_policy.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using namespace modules::gear_logic;
    assert(armor_type("item.minecraft.diamond_helmet") == 3);
    assert(armor_type("item.minecraft.elytra") == -1);
    assert(armor_score("item.minecraft.netherite_boots") > armor_score("item.minecraft.diamond_boots"));
    assert(screen_slot(0) == 36 && screen_slot(8) == 44 && screen_slot(9) == 9 && screen_slot(-1) == -1);
    std::array<std::string, 40> keys{};
    keys[9] = "item.minecraft.diamond_chestplate";
    keys[38] = "item.minecraft.elytra";
    assert(choose_armor(keys, true).first == -1);
    assert((choose_armor(keys, false) == std::pair{9, 2}));
    keys[0] = "item.minecraft.diamond_helmet";
    assert((choose_armor(keys, true) == std::pair{0, 3}));
    keys[39] = "item.minecraft.diamond_helmet";
    assert(choose_armor(keys, true).first == -1);
    keys[1] = "item.minecraft.elytra";
    assert(choose_swap(keys, 0, false) == -1 && choose_swap(keys, 0, true) == 1);
    keys[10] = "item.minecraft.elytra";
    assert(choose_swap(keys, 0, true) == 10 && choose_swap(keys, 1, false) == 9);
    std::cout << "PASS gear policy\n";
}
