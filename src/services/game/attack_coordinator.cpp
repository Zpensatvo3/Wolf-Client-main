#include "attack_coordinator.hpp"
#include <mutex>

namespace
{
    std::mutex ownership_mutex;
    int target = -1;
    std::uint64_t session = 0;
}
void services::combat::claim_trigger(int target_id, std::uint64_t epoch)
{
    std::lock_guard lock(ownership_mutex); target = target_id; session = epoch;
}
void services::combat::release_trigger()
{
    std::lock_guard lock(ownership_mutex); target = -1; session = 0;
}
bool services::combat::trigger_owns_attack()
{
    std::lock_guard lock(ownership_mutex); return target >= 0 && session != 0;
}
bool services::combat::try_unowned_attack(const std::function<bool()>& attempt)
{
    // Serialize the check and submission against ownership acquisition, including
    // Left Clicker's direct doAttack path. Never acquire this lock from a helper
    // while holding it; Triggerbot releases it before weapon preparation.
    std::lock_guard lock(ownership_mutex);
    return target < 0 && session == 0 && attempt && attempt();
}
