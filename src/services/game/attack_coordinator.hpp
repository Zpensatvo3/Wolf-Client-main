#pragma once
#include <cstdint>
#include <functional>

namespace services::combat
{
    void claim_trigger(int target_id, std::uint64_t session);
    void release_trigger();
    bool trigger_owns_attack();
    bool claim_crystal_aura(int target_id, std::uint64_t session);
    void release_crystal_aura();
    bool try_crystal_aura_action(int target_id, std::uint64_t session, const std::function<bool()>& attempt);
    bool try_unowned_attack(const std::function<bool()>& attempt);
}
