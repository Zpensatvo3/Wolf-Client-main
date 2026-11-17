#include "../src/services/game/attack_coordinator.hpp"
#include <cassert>
#include <iostream>

int main()
{
    using namespace services::combat;
    int attacks = 0;
    auto attack = [&] { ++attacks; return true; };
    release_trigger(); release_crystal_aura();
    claim_trigger(-1,1); claim_trigger(5,0);
    assert(try_unowned_attack(attack));
    attacks = 0;
    assert(!claim_crystal_aura(-1,1) && !claim_crystal_aura(5,0));
    assert(claim_crystal_aura(5,1));
    claim_trigger(-1,0);
    assert(!trigger_owns_attack());
    assert(!try_unowned_attack(attack) && attacks == 0); // Other attack owners cannot cut in.
    assert(!try_crystal_aura_action(6,1,attack));
    assert(!try_crystal_aura_action(5,2,attack));
    assert(try_crystal_aura_action(5,1,attack) && attacks == 1);
    release_trigger(); // An inactive Triggerbot cannot clear another module's claim.
    assert(try_crystal_aura_action(5,1,attack));
    assert(claim_crystal_aura(6,1)); // Handoff after target dies.
    assert(!try_crystal_aura_action(5,1,attack));
    assert(try_crystal_aura_action(6,1,attack));
    claim_trigger(8,2); // Triggerbot retains priority.
    assert(trigger_owns_attack() && !claim_crystal_aura(6,1));
    const int before=attacks;
    assert(!try_unowned_attack(attack) && attacks==before);
    assert(!try_crystal_aura_action(6,1,attack));
    release_crystal_aura();
    assert(trigger_owns_attack());
    release_trigger();
    assert(claim_crystal_aura(6,3));
    assert(!try_crystal_aura_action(6,1,attack)); // Old-world tickets rejected.
    release_crystal_aura();
    assert(try_unowned_attack(attack));
    assert(!try_crystal_aura_action(6,3,attack));
    std::cout << "Crystal Aura ownership: attack exclusion, Triggerbot priority and stale-action rejection passed\n";
}
