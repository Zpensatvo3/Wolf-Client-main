#pragma once
#include <random>
#include <algorithm>

namespace Vape::Combat {

    /**
     * Exact C++20 port of Vape V4.21's Velocity Module for Minecraft 1.21+ (Fabric/Yarn).
     *
     * Features:
     * - Independent Horizontal & Vertical Reduction:
     *     Scales incoming knockback vectors (0% = completely cancel knockback, 100% = vanilla knockback).
     * - Complete Knockback Cancellation:
     *     Cancels the packet if both horizontal and vertical scalers are 0.
     * - Kite Mode Disengage Acceleration:
     *     Amplifies horizontal knockback (e.g. 120%) when running away/facing away to break opponent combos.
     * - Liquid & Chance Checks.
     */
    class Velocity {
    public:
        bool enabled{true};
        double horizontalPercent{90.0};  // Horizontal knockback % (0 to 100)
        double verticalPercent{100.0};   // Vertical knockback % (0 to 100)
        double chance{100.0};            // Activation probability %
        bool waterCheck{true};           // Disable reduction in water/lava
        bool kiteMode{false};            // Amplify knockback to escape when disengaging
        double kiteHorizontal{120.0};    // Horizontal knockback % while kiting
        double kiteVertical{100.0};      // Vertical knockback % while kiting

        Velocity() : m_rng(std::random_device{}()) {}

        struct VelocityResult {
            double x{0.0};
            double y{0.0};
            double z{0.0};
            bool cancelPacket{false};
        };

        /**
         * Intercepts and scales incoming velocity from EntityVelocityUpdateS2CPacket.
         * @param velX X velocity in blocks/tick (packetX / 8000.0)
         * @param velY Y velocity in blocks/tick (packetY / 8000.0)
         * @param velZ Z velocity in blocks/tick (packetZ / 8000.0)
         * @param isInLiquid whether player is in water or lava
         * @param isFacingAway whether player is facing away from damage source (for kite mode)
         * @return VelocityResult with modified motion vectors and cancellation flag
         */
        VelocityResult processVelocity(double velX, double velY, double velZ,
                                       bool isInLiquid, bool isFacingAway = false) {
            if (!enabled) {
                return {velX, velY, velZ, false};
            }

            if (waterCheck && isInLiquid) {
                return {velX, velY, velZ, false};
            }

            if (chance < 100.0) {
                std::uniform_real_distribution<double> dist(0.0, 100.0);
                if (dist(m_rng) >= chance) {
                    return {velX, velY, velZ, false};
                }
            }

            // Kite mode: amplify velocity to help disengage
            if (kiteMode && isFacingAway) {
                double hScale = kiteHorizontal / 100.0;
                double vScale = kiteVertical / 100.0;
                return {velX * hScale, velY * vScale, velZ * hScale, false};
            }

            // Standard reduction
            double hScale = horizontalPercent / 100.0;
            double vScale = verticalPercent / 100.0;

            if (hScale == 0.0 && vScale == 0.0) {
                return {0.0, 0.0, 0.0, true}; // Cancel packet completely
            }

            return {velX * hScale, velY * vScale, velZ * hScale, false};
        }

    private:
        std::mt19937 m_rng;
    };
}
