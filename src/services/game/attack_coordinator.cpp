#include "attack_coordinator.hpp"
#include <mutex>

namespace
{
    std::mutex ownership_mutex;
    int target = -1;
    std::uint64_t session = 0;
    enum class owner { none, trigger, crystal_aura };
    owner current_owner = owner::none;
}
void services::combat::claim_trigger(int target_id, std::uint64_t epoch)
{
    std::lock_guard lock(ownership_mutex);
    if (target_id < 0 || epoch == 0)
    {
        if (current_owner == owner::trigger)
        { target = -1; session = 0; current_owner = owner::none; }
        return;
    }
    target = target_id; session = epoch;
    current_owner = owner::trigger;
}
void services::combat::release_trigger()
{
    std::lock_guard lock(ownership_mutex);
    if (current_owner != owner::trigger) return;
    target = -1; session = 0; current_owner = owner::none;
}
bool services::combat::trigger_owns_attack()
{
    std::lock_guard lock(ownership_mutex); return current_owner == owner::trigger && target >= 0 && session != 0;
}
bool services::combat::claim_crystal_aura(int target_id, std::uint64_t epoch)
{
    std::lock_guard lock(ownership_mutex);
    if (current_owner == owner::trigger || target_id < 0 || epoch == 0) return false;
    current_owner = owner::crystal_aura; target = target_id; session = epoch; return true;
}
void services::combat::release_crystal_aura()
{
    std::lock_guard lock(ownership_mutex);
    if (current_owner != owner::crystal_aura) return;
    target = -1; session = 0; current_owner = owner::none;
}
bool services::combat::try_crystal_aura_action(int target_id, std::uint64_t epoch, const std::function<bool()>& attempt)
{
    std::lock_guard lock(ownership_mutex);
    return current_owner == owner::crystal_aura && target == target_id && session == epoch && attempt && attempt();
}
bool services::combat::try_unowned_attack(const std::function<bool()>& attempt)
{
    // Serialize the check and submission against ownership acquisition, including
    // direct attack submissions. Never acquire this lock from a helper
    // while holding it; Triggerbot releases it before weapon preparation.
    std::lock_guard lock(ownership_mutex);
    return current_owner == owner::none && target < 0 && session == 0 && attempt && attempt();
}
