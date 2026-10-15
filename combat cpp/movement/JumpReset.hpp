#pragma once
#include <random>
#include <cmath>

namespace Vape::Combat {

    /**
     * Exact C++20 port of Vape V4.21's JumpReset Module for Minecraft 1.21+ (Fabric/Yarn).
     *
     * Features:
     * - Competitive Knockback Reduction via Kinetic Jump Reset:
     *     Jumps on the exact damage/hurt tick to convert incoming horizontal momentum into vertical lift.
     * - Human Timing Accuracy Simulation:
     *     Configurable accuracy roll (default 90%) to mimic natural competitive player jump timing.
     * - Threshold & Liquid Filters:
     *     Ignores micro-knockback (< 0.05 blocks/tick) and automatically disables in water/lava.
     */
    class JumpReset {
    public:
        bool enabled{true};
        double chance{100.0};
        double accuracy{90.0};            // Timing accuracy % (simulates human mechanical precision)
        int delayTicks{0};                 // 0 = jump immediately on hurt packet
        bool waterCheck{true};
        double minHorizontalVelocity{0.05};// Threshold below which velocity packet is ignored

        JumpReset() : m_rng(std::random_device{}()) {}

        /**
         * Call when an incoming damage packet or EntityVelocityUpdateS2CPacket is received.
         * @param velX horizontal X velocity component (in blocks/tick, e.g. packetVal / 8000.0)
         * @param velZ horizontal Z velocity component (in blocks/tick, e.g. packetVal / 8000.0)
         * @param isOnGround whether local player is on ground
         * @param isInLiquid whether local player is in water or lava
         * @return true if immediate jump should be triggered
         */
        [[nodiscard]] bool onVelocityPacket(double velX, double velZ, bool isOnGround, bool isInLiquid) {
            if (!enabled || !isOnGround) return false;
            if (waterCheck && isInLiquid) return false;

            // Check if packet has meaningful horizontal velocity
            if (std::abs(velX) < minHorizontalVelocity && std::abs(velZ) < minHorizontalVelocity) {
                return false;
            }

            // Chance roll
            if (chance < 100.0) {
                std::uniform_real_distribution<double> dist(0.0, 100.0);
                if (dist(m_rng) >= chance) return false;
            }

            m_pendingJump = true;
            m_currentDelay = delayTicks;

            if (m_currentDelay == 0) {
                return checkAccuracyAndJump();
            }

            return false;
        }

        // Call every client tick. Returns true if jump should be executed
        [[nodiscard]] bool onTick(bool isOnGround) {
            if (!enabled || !isOnGround) { reset(); return false; }
            if (!m_pendingJump) return false;

            if (m_currentDelay > 0) {
                m_currentDelay--;
                if (m_currentDelay > 0) return false;
            }

            return checkAccuracyAndJump();
        }

        void reset() {
            m_pendingJump = false;
            m_currentDelay = 0;
        }

    private:
        bool checkAccuracyAndJump() {
            m_pendingJump = false;

            // Accuracy roll
            if (accuracy < 100.0) {
                std::uniform_real_distribution<double> dist(0.0, 100.0);
                if (dist(m_rng) >= accuracy) return false;
            }

            return true;
        }

        bool m_pendingJump{false};
        int m_currentDelay{0};
        std::mt19937 m_rng;
    };
}
