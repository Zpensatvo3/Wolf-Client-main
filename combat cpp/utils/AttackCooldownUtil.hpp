#pragma once
#include <algorithm>

namespace Vape::Combat {

    /**
     * Exact C++20 port of Vape V4.21's AttackCooldownUtil.
     *
     * Calculation Mechanics:
     * - Minecraft 1.21 Attack Cooldown scaling curve:
     *     attackStrengthMultiplier = 0.2 + cooldownProgress^2 * 0.8
     * - Cooldown readiness with offset (in ticks / partial ticks):
     *     Allows early attacks (negative offset) or post-cooldown extra delays
     * - Sweep attack eligibility (cooldown >= 0.848f and player on ground)
     * - Critical hit eligibility:
     *     Player must be falling (fallDistance > 0), not on ground, not in water/lava,
     *     not climbing ladders/vines, and not affected by blindness.
     */
    class AttackCooldownUtil {
    public:
        // Read vanilla getAttackCooldownProgress(offsetTicks) first. One tick is
        // not a fixed percentage of every weapon's recharge duration.
        [[nodiscard]] static bool isAttackReady(float cooldownAtRequestedOffset) {
            return cooldownAtRequestedOffset >= 1.0f;
        }

        // Returns damage multiplier based on 1.21 cooldown curve
        [[nodiscard]] static float getDamageMultiplier(float cooldownProgress) {
            float cd = std::clamp(cooldownProgress, 0.0f, 1.0f);
            return 0.2f + cd * cd * 0.8f;
        }

        // Checks if sweep attack will trigger (deals AOE damage to surrounding mobs)
        [[nodiscard]] static bool canSweepAttack(float cooldownProgress, bool isPlayerOnGround, bool isSprinting) {
            return cooldownProgress >= 0.848f && isPlayerOnGround && !isSprinting;
        }

        // Checks if an attack qualifies as a critical hit (deals 1.5x damage + extra knockback)
        [[nodiscard]] static bool isCriticalHit(float playerFallDistance,
                                               double playerMotionY,
                                               bool isPlayerOnGround,
                                               bool isInLiquid,
                                               bool isClimbing,
                                               bool hasBlindness) {
            return (playerFallDistance > 0.0f || playerMotionY < 0.0)
                   && !isPlayerOnGround
                   && !isInLiquid
                   && !isClimbing
                   && !hasBlindness;
        }
    };
}
