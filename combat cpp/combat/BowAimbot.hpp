#pragma once
#include "../math/Vector3.hpp"
#include "../math/RotationUtil.hpp"
#include <optional>
#include <cmath>
#include <vector>
#include <algorithm>
#include <numbers>

namespace Vape::Combat {

    /**
     * Exact C++20 port of Vape V4.21's BowAimbot Module for Minecraft 1.21+ (Fabric/Yarn).
     *
     * Features:
     * - Ballistic Trajectory Equation Solver:
     *     Solves projectile motion under Minecraft arrow gravity (g = 0.05 blocks/tick^2).
     *     Supports both standard flat trajectory (Low Arc) and long-range mortar (High Arc).
     *     Gracefully handles out-of-range targets by locking pitch to -45.0° (maximum ballistic reach).
     * - Kinematic Velocity & Movement Lead Prediction:
     *     Calculates arrow time-of-flight based on projectile speed:
     *     flightTicks = distance / (v * 0.95)
     *     predictedPos = eyePos + targetVelocity * flightTicks
     * - Dynamic Bow / Crossbow Charge Scaling:
     *     Automatically adapts to Bow pull progress (0.1 to 1.0 -> 0.3 to 3.0 blocks/tick)
     *     and Crossbow charged state (constant 3.0 blocks/tick).
     * - Bio-mimetic Rotation Smoothing:
     *     Applies frame-rate independent interpolation using configurable aimSpeed.
     */
    class BowAimbot {
    public:
        // Configurable Parameters
        bool enabled{true};
        float aimSpeed{8.0f};           // Aim speed factor (0.1 to 20.0)
        double maxRange{50.0};          // Maximum acquisition distance in blocks
        float maxAngle{90.0f};          // FOV cone limit in degrees
        bool predictMovement{true};     // Lead targets using their 3D velocity
        double gravity{0.05};           // Minecraft arrow gravity in blocks/tick^2
        bool highArc{false};            // Fire at high ballistic arc (mortar mode)

        struct TargetCandidate {
            int entityId{0};
            Math::Vector3 eyePos{};
            Math::Vector3 velocity{};
            bool isAlive{true};
        };

        struct AimResult {
            Math::Angle targetAngle{};
            Math::Angle smoothedAngle{};
            Math::Vector3 predictedTargetPos{};
            float arrowVelocity{3.0f};
            bool isOutOfRange{false};
        };

        /**
         * Calculates initial arrow speed from bow pull progress or crossbow state.
         * @param isCrossbow true if wielding a charged crossbow
         * @param bowPullProgress pull progress from 0.0f to 1.0f (BowItem.getPullProgress)
         * @return arrow speed in blocks/tick, or 0.0f if not ready to fire
         */
        [[nodiscard]] static float calculateArrowVelocity(bool isCrossbow, float bowPullProgress) {
            if (isCrossbow) {
                return 3.0f; // Crossbow fired projectiles travel at fixed 3.0 blocks/tick
            }
            if (bowPullProgress < 0.1f) {
                return 0.0f; // Not charged enough to release
            }
            return bowPullProgress * 3.0f;
        }

        /**
         * Solves the exact ballistic trajectory angles for an arrow hitting targetPos.
         */
        [[nodiscard]] std::optional<AimResult> calculateBallisticSolution(
            const Math::Vector3& playerEyePos,
            float playerYaw,
            float playerPitch,
            const TargetCandidate& target,
            float arrowVelocity) const {

            if (!enabled || arrowVelocity < 0.3f) {
                return std::nullopt;
            }

            // Aim offset: 0.2 blocks below eye level (optimal chest/torso hit box)
            Math::Vector3 targetAimPos = target.eyePos;
            targetAimPos.y -= 0.2;

            // Target lead prediction using relative kinematics
            if (predictMovement) {
                double distance = (targetAimPos - playerEyePos).length();
                double flightTimeTicks = distance / (arrowVelocity * 0.95);
                targetAimPos.x += target.velocity.x * flightTimeTicks;
                targetAimPos.y += target.velocity.y * flightTimeTicks;
                targetAimPos.z += target.velocity.z * flightTimeTicks;
            }

            double deltaX = targetAimPos.x - playerEyePos.x;
            double deltaY = targetAimPos.y - playerEyePos.y;
            double deltaZ = targetAimPos.z - playerEyePos.z;

            double horizontalDistance = std::sqrt(deltaX * deltaX + deltaZ * deltaZ);
            float yaw = Math::RotationUtil::wrapDegrees(
                static_cast<float>(std::atan2(deltaZ, deltaX) * (180.0 / std::numbers::pi)) - 90.0f
            );

            // Ballistic equation: v^4 - g * (g * x^2 + 2 * y * v^2)
            double v = arrowVelocity;
            double v2 = v * v;
            double v4 = v2 * v2;
            double radical = v4 - gravity * (gravity * horizontalDistance * horizontalDistance + 2.0 * deltaY * v2);

            float pitch = 0.0f;
            bool outOfRange = false;

            if (radical < 0.0) {
                // Out of ballistic range for current velocity; elevate to -45.0 degrees for maximum reach
                pitch = -45.0f;
                outOfRange = true;
            } else {
                double sqrtRad = std::sqrt(radical);
                double pitchRad;
                if (highArc) {
                    // High mortar arc
                    pitchRad = std::atan((v2 + sqrtRad) / (gravity * horizontalDistance));
                } else {
                    // Standard direct low arc
                    pitchRad = std::atan((v2 - sqrtRad) / (gravity * horizontalDistance));
                }
                pitch = Math::RotationUtil::wrapDegrees(static_cast<float>(-(pitchRad * (180.0 / std::numbers::pi))));
                pitch = std::clamp(pitch, -90.0f, 90.0f);
            }

            // Smooth interpolation step
            float deltaYaw = Math::RotationUtil::wrapDegrees(yaw - playerYaw);
            float deltaPitch = Math::RotationUtil::wrapDegrees(pitch - playerPitch);

            float smoothedYaw = playerYaw + deltaYaw * (aimSpeed * 0.05f);
            float smoothedPitch = std::clamp(playerPitch + deltaPitch * (aimSpeed * 0.05f), -90.0f, 90.0f);

            return AimResult{
                .targetAngle = Math::Angle{yaw, pitch},
                .smoothedAngle = Math::Angle{Math::RotationUtil::wrapDegrees(smoothedYaw), smoothedPitch},
                .predictedTargetPos = targetAimPos,
                .arrowVelocity = arrowVelocity,
                .isOutOfRange = outOfRange
            };
        }

        /**
         * Finds the closest valid target candidate within range and FOV.
         */
        [[nodiscard]] const TargetCandidate* findBestTarget(
            const Math::Vector3& playerPos,
            float playerYaw,
            const std::vector<TargetCandidate>& candidates) const {

            const TargetCandidate* best = nullptr;
            double closestDistSq = maxRange * maxRange;

            for (const auto& candidate : candidates) {
                if (!candidate.isAlive) continue;

                double dx = candidate.eyePos.x - playerPos.x;
                double dy = candidate.eyePos.y - playerPos.y;
                double dz = candidate.eyePos.z - playerPos.z;
                double distSq = dx * dx + dy * dy + dz * dz;

                if (distSq > closestDistSq) continue;

                // Angle check
                float targetYaw = static_cast<float>(std::atan2(dz, dx) * (180.0 / std::numbers::pi)) - 90.0f;
                float angleDiff = Math::RotationUtil::getAngleDifference(playerYaw, targetYaw);

                if (angleDiff <= (maxAngle / 2.0f)) {
                    closestDistSq = distSq;
                    best = &candidate;
                }
            }

            return best;
        }
    };
}
