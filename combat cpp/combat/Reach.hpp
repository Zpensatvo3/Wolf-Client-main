#pragma once
#include "../math/Vector3.hpp"
#include <optional>
#include <algorithm>
#include <random>

namespace Vape::Combat {

    /**
     * Exact C++20 port of Vape V4.21's Reach.
     *
     * Features:
     * - Randomized Range Range [min, max] (e.g. 3.05 - 3.25)
     * - Activation Chance (0 - 100%)
     * - Only While Sprinting filter
     * - Disable In Water check
     * - Vertical Height Check (avoids blatant hits when target is far above/below)
     * - Client-side Misplace Calculation (shifts bounding box & particles for natural appearance)
     * - Ray-AABB intersection solver
     */
    class Reach {
    public:
        bool enabled{true};
        double minRange{3.0};
        double maxRange{3.25};
        double chance{100.0};
        bool onlyWhileSprinting{false};
        bool disableInWater{true};
        bool verticalCheck{true};
        double maxVerticalDiff{2.2};
        bool misplace{false};

        Reach() : m_rng(std::random_device{}()) {}

        // Ray-AABB intersection test
        static std::optional<double> rayIntersectsBox(const Math::Vector3& rayOrigin, const Math::Vector3& rayDir,
                                                      const Math::BoundingBox& box, double maxDist) {
            double tmin = 0.0;
            double tmax = maxDist;

            // X slab
            if (std::abs(rayDir.x) > 1e-9) {
                double invD = 1.0 / rayDir.x;
                double t1 = (box.min.x - rayOrigin.x) * invD;
                double t2 = (box.max.x - rayOrigin.x) * invD;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return std::nullopt;
            } else if (rayOrigin.x < box.min.x || rayOrigin.x > box.max.x) {
                return std::nullopt;
            }

            // Y slab
            if (std::abs(rayDir.y) > 1e-9) {
                double invD = 1.0 / rayDir.y;
                double t1 = (box.min.y - rayOrigin.y) * invD;
                double t2 = (box.max.y - rayOrigin.y) * invD;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return std::nullopt;
            } else if (rayOrigin.y < box.min.y || rayOrigin.y > box.max.y) {
                return std::nullopt;
            }

            // Z slab
            if (std::abs(rayDir.z) > 1e-9) {
                double invD = 1.0 / rayDir.z;
                double t1 = (box.min.z - rayOrigin.z) * invD;
                double t2 = (box.max.z - rayOrigin.z) * invD;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return std::nullopt;
            } else if (rayOrigin.z < box.min.z || rayOrigin.z > box.max.z) {
                return std::nullopt;
            }

            return tmin;
        }

        // Returns effective reach distance for the current tick
        [[nodiscard]] double getEffectiveReach(bool isSprinting, bool isInWater) {
            if (!enabled) return 3.0; // Vanilla reach

            if (onlyWhileSprinting && !isSprinting) return 3.0;
            if (disableInWater && isInWater) return 3.0;

            if (chance < 100.0) {
                std::uniform_real_distribution<double> dist(0.0, 100.0);
                if (dist(m_rng) > chance) return 3.0;
            }

            if (maxRange > minRange) {
                std::uniform_real_distribution<double> dist(minRange, maxRange);
                return dist(m_rng);
            }
            return minRange;
        }

        // Raycasts target bounding box using effective reach and vertical constraints
        [[nodiscard]] std::optional<double> checkTargetIntersection(const Math::Vector3& eyePos,
                                                                    const Math::Vector3& lookDir,
                                                                    const Math::BoundingBox& targetBox,
                                                                    bool isSprinting,
                                                                    bool isInWater,
                                                                    std::optional<double> sampledReach = std::nullopt) {
            if (!enabled) return std::nullopt;

            if (verticalCheck) {
                double dy = std::abs(eyePos.y - targetBox.getCenter().y);
                if (dy > maxVerticalDiff) return std::nullopt;
            }

            double effectiveReach = sampledReach ? *sampledReach : getEffectiveReach(isSprinting, isInWater);

            Math::BoundingBox effectiveBox = targetBox;
            if (misplace) {
                // Client-side misplace expands AABB by difference over vanilla 3.0
                double misplaceDist = std::max(0.0, minRange - 3.0);
                effectiveBox = effectiveBox.expand(misplaceDist);
            }

            return rayIntersectsBox(eyePos, lookDir, effectiveBox, effectiveReach);
        }

    private:
        std::mt19937 m_rng;
    };
}
