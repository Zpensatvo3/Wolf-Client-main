#pragma once
#include "../math/Vector3.hpp"
#include <optional>
#include <vector>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Vape::Combat {

    struct RayTraceHit {
        bool hit{false};
        bool isEntity{false};
        int entityId{-1};
        Math::Vector3 hitPoint{};
        double distance{0.0};
        int blockFace{1}; // 0 = down, 1 = up, 2 = north, 3 = south, 4 = west, 5 = east
    };

    /**
     * Exact C++20 port of Vape V4.21's RayTraceUtil.
     *
     * Calculation Mechanics:
     * - Ray-AABB intersection with custom hitbox expansion (HitBoxes / Reach).
     * - Eye-to-target Line of Sight (LOS) calculation.
     * - Direction vector generation from yaw & pitch angles.
     * - Extended reach raycasting along camera or silent rotation vectors.
     */
    class RayTraceUtil {
    public:
        // Converts yaw and pitch angles into a normalized 3D direction vector
        [[nodiscard]] static Math::Vector3 getVectorForRotation(float yaw, float pitch) {
            float yawRad = static_cast<float>(yaw * (std::numbers::pi / 180.0));
            float pitchRad = static_cast<float>(pitch * (std::numbers::pi / 180.0));

            float cosYaw = std::cos(-yawRad - static_cast<float>(std::numbers::pi));
            float sinYaw = std::sin(-yawRad - static_cast<float>(std::numbers::pi));
            float cosPitch = -std::cos(-pitchRad);
            float sinPitch = std::sin(-pitchRad);

            return {sinYaw * cosPitch, sinPitch, cosYaw * cosPitch};
        }

        // Ray-AABB intersection test returning the exact hit coordinate and distance
        [[nodiscard]] static std::optional<RayTraceHit> rayIntersectsBox(const Math::Vector3& origin,
                                                                        const Math::Vector3& dir,
                                                                        const Math::BoundingBox& box,
                                                                        double maxDistance) {
            double tmin = 0.0;
            double tmax = maxDistance;

            // X slab
            if (std::abs(dir.x) > 1e-9) {
                double invD = 1.0 / dir.x;
                double t1 = (box.min.x - origin.x) * invD;
                double t2 = (box.max.x - origin.x) * invD;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return std::nullopt;
            } else if (origin.x < box.min.x || origin.x > box.max.x) {
                return std::nullopt;
            }

            // Y slab
            if (std::abs(dir.y) > 1e-9) {
                double invD = 1.0 / dir.y;
                double t1 = (box.min.y - origin.y) * invD;
                double t2 = (box.max.y - origin.y) * invD;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return std::nullopt;
            } else if (origin.y < box.min.y || origin.y > box.max.y) {
                return std::nullopt;
            }

            // Z slab
            if (std::abs(dir.z) > 1e-9) {
                double invD = 1.0 / dir.z;
                double t1 = (box.min.z - origin.z) * invD;
                double t2 = (box.max.z - origin.z) * invD;
                if (t1 > t2) std::swap(t1, t2);
                tmin = std::max(tmin, t1);
                tmax = std::min(tmax, t2);
                if (tmin > tmax) return std::nullopt;
            } else if (origin.z < box.min.z || origin.z > box.max.z) {
                return std::nullopt;
            }

            if (tmin < 0.0 || tmin > maxDistance) return std::nullopt;

            Math::Vector3 hitPoint = origin + dir * tmin;
            return RayTraceHit{true, false, -1, hitPoint, tmin, 1};
        }

        // Extended reach entity raytrace along given look angles
        [[nodiscard]] static std::optional<RayTraceHit> rayTraceEntities(const Math::Vector3& eyePos,
                                                                         float yaw,
                                                                         float pitch,
                                                                         double reachDistance,
                                                                         float hitboxExpansion,
                                                                         const std::vector<std::pair<int, Math::BoundingBox>>& entities) {
            Math::Vector3 lookDir = getVectorForRotation(yaw, pitch);
            double closestDist = reachDistance;
            std::optional<RayTraceHit> bestHit = std::nullopt;

            for (const auto& [entityId, box] : entities) {
                Math::BoundingBox effectiveBox = hitboxExpansion > 0.0f ? box.expand(hitboxExpansion) : box;
                auto hit = rayIntersectsBox(eyePos, lookDir, effectiveBox, closestDist);
                if (hit && hit->distance < closestDist) {
                    closestDist = hit->distance;
                    bestHit = hit;
                    bestHit->isEntity = true;
                    bestHit->entityId = entityId;
                }
            }

            return bestHit;
        }

        // Line-of-sight test: checks if line between point A and point B is occluded by blocks
        [[nodiscard]] static bool isLineOccluded(const Math::Vector3& start,
                                                 const Math::Vector3& end,
                                                 const std::vector<Math::BoundingBox>& solidBlockBoxes) {
            Math::Vector3 diff = end - start;
            double dist = diff.length();
            if (dist < 1e-6) return false;
            Math::Vector3 dir = diff / dist;

            for (const auto& blockBox : solidBlockBoxes) {
                auto hit = rayIntersectsBox(start, dir, blockBox, dist);
                if (hit && hit->distance < dist - 0.05) {
                    return true; // Occluded
                }
            }
            return false;
        }
    };
}
