#pragma once
#include "../math/RotationUtil.hpp"
#include "../math/Vector3.hpp"
#include <vector>
#include <algorithm>
#include <random>

namespace Vape::Combat {

    struct CombatTarget {
        int entityId{0};
        Math::BoundingBox boundingBox{};
        Math::Vector3 velocity{};
        float health{20.0f};
        int armor{0};
        bool isBlocking{false};
        bool isDead{false};
    };

    /**
     * Independent C++ SilentAura decision model; not a byte-for-byte Java port.
     *
     * Complete Feature Set:
     * - Multi-Criteria Target Selection: Distance, Health, Armor, Threat
     * - Multi-Area Targeting: Closest box point, Center mass, Box, Ring
     * - PID Silent Rotation Smoothing (Proportional-Integral) with configurable Aim Speed (1-10)
     * - 3D Organic Micro-Jitter (X, Y, Z axes)
     * - Extra Swing Distance: Pre-aims and locks onto targets before they enter attack reach
     * - 1.21 Attack Cooldown Synchronization (Perfect Swing)
     * - Shield Check with Axe bypass
     * - Require Mouse Down option
     */
    class SilentAura {
    public:
        enum class Priority {
            DISTANCE,
            HEALTH,
            ARMOR,
            THREAT
        };

        enum class TargetArea {
            CLOSEST,
            CENTER,
            BOX,
            RING
        };

        bool enabled{true};
        double reach{3.8};
        double extraSwingDistance{1.0}; // Begins aiming before target enters reach
        double maxFov{360.0};
        Priority priority{Priority::DISTANCE};
        TargetArea targetArea{TargetArea::CLOSEST};
        double aimSpeed{7.0};           // 1.0 - 10.0
        float minCooldownProgress{0.95f};
        bool perfectSwing{true};
        bool shieldCheck{true};
        bool requireMouseDown{false};
        bool humanJitter{true};

        SilentAura() : m_rng(std::random_device{}()) {}

        struct AuraAction {
            bool shouldAttack{false};
            bool isPreAiming{false};
            int targetEntityId{-1};
            Math::Angle targetAngle{};
        };

        // Call every frame/tick to compute smooth PID silent rotations and attack timing
        AuraAction update(const Math::Vector3& playerEyePos,
                          float currentYaw, float currentPitch,
                          float mouseSensitivity,
                          float playerAttackCooldown,
                          bool isAttackKeyPressed,
                          bool hasAxeInHotbar,
                          const std::vector<CombatTarget>& candidates,
                          double deltaTime) {
            if (!enabled) {
                reset();
                return {};
            }

            if (requireMouseDown && !isAttackKeyPressed) {
                reset();
                return {};
            }

            const CombatTarget* bestTarget = selectBestTarget(playerEyePos, currentYaw, hasAxeInHotbar, candidates);
            if (!bestTarget) {
                reset();
                return {};
            }
            if(m_targetId!=bestTarget->entityId) {reset();m_targetId=bestTarget->entityId;}

            double dist = playerEyePos.distanceTo(bestTarget->boundingBox.getClosestPoint(playerEyePos));
            bool inAttackReach = (dist <= reach);
            bool inPreAimReach = (dist <= reach + extraSwingDistance);

            if (!inPreAimReach) {
                reset();
                return {};
            }

            // 1. Resolve aim coordinate on target
            Math::Vector3 aimCoord = resolveAimCoordinate(playerEyePos, bestTarget->boundingBox);

            // Apply 3D micro-jitter if enabled
            if (humanJitter) {
                std::uniform_real_distribution<double> jDist(-0.04, 0.04);
                aimCoord.x += jDist(m_rng);
                aimCoord.y += jDist(m_rng);
                aimCoord.z += jDist(m_rng);
            }
            // A jittered closest-face point must stay inside the actual box;
            // otherwise the subsequent ray validation rejects most edge hits.
            const auto& b=bestTarget->boundingBox;
            aimCoord.x=std::clamp(aimCoord.x,b.min.x+.001,b.max.x-.001);
            aimCoord.y=std::clamp(aimCoord.y,b.min.y+.001,b.max.y-.001);
            aimCoord.z=std::clamp(aimCoord.z,b.min.z+.001,b.max.z-.001);

            // 2. Calculate ideal rotations to target
            Math::Angle idealAngle = Math::RotationUtil::getRotationsToCoords(playerEyePos, aimCoord);

            // 3. PID Smoothing
            Math::Angle smoothedAngle = calculatePIDSmoothing(currentYaw, currentPitch, idealAngle.yaw, idealAngle.pitch, mouseSensitivity, deltaTime);

            // 4. Attack Cooldown check
            bool readyToAttack = inAttackReach;
            if (perfectSwing && playerAttackCooldown < minCooldownProgress) {
                readyToAttack = false;
            }

            return {
                readyToAttack,
                !readyToAttack, // Pre-aiming active
                bestTarget->entityId,
                smoothedAngle
            };
        }

        void reset() {
            m_yawIntegral = 0.0f;
            m_pitchIntegral = 0.0f;
            m_targetId = -1;
        }

    private:
        const CombatTarget* selectBestTarget(const Math::Vector3& playerEyePos, float currentYaw,
                                            bool hasAxeInHotbar, const std::vector<CombatTarget>& candidates) const {
            const CombatTarget* best = nullptr;
            double bestScore = 999999.0;
            double maxSearchDist = reach + extraSwingDistance;

            for (const auto& target : candidates) {
                if (target.isDead) continue;
                if (shieldCheck && target.isBlocking && !hasAxeInHotbar) continue;

                double dist = playerEyePos.distanceTo(target.boundingBox.getClosestPoint(playerEyePos));
                if (dist > maxSearchDist) continue;

                Math::Angle rots = Math::RotationUtil::getRotationsToCoords(
                    playerEyePos, target.boundingBox.getClosestPoint(playerEyePos)
                );
                float fov = Math::RotationUtil::getAngleDifference(currentYaw, rots.yaw);
                if (fov > maxFov / 2.0) continue;

                double score = 0.0;
                switch (priority) {
                    case Priority::DISTANCE: score = dist; break;
                    case Priority::HEALTH:   score = target.health; break;
                    case Priority::ARMOR:    score = target.armor; break;
                    case Priority::THREAT:   score = target.health * 0.5 + dist; break;
                }

                if (score < bestScore) {
                    bestScore = score;
                    best = &target;
                }
            }

            return best;
        }

        Math::Vector3 resolveAimCoordinate(const Math::Vector3& eyePos, const Math::BoundingBox& box) const {
            switch (targetArea) {
                case TargetArea::CLOSEST:
                    return box.getClosestPoint(eyePos);
                case TargetArea::CENTER:
                    return box.getCenter();
                case TargetArea::BOX: {
                    Math::Vector3 c = box.getCenter();
                    c.y = box.min.y + (box.max.y - box.min.y) * 0.75;
                    return c;
                }
                case TargetArea::RING: {
                    Math::Vector3 c = box.getCenter();
                    c.y = box.min.y + (box.max.y - box.min.y) * 0.5;
                    return c;
                }
            }
            return box.getClosestPoint(eyePos);
        }

        Math::Angle calculatePIDSmoothing(float currentYaw, float currentPitch,
                                         float targetYaw, float targetPitch,
                                         float mouseSensitivity, double deltaTime) {
            float yawDiff = Math::RotationUtil::wrapDegrees(targetYaw - currentYaw);
            float pitchDiff = targetPitch - currentPitch;
            deltaTime=std::clamp(deltaTime,.001,.05);

            // Proportional and Integral terms
            float pFactor = static_cast<float>(aimSpeed * 0.45);
            m_yawIntegral = std::clamp(m_yawIntegral + yawDiff * static_cast<float>(deltaTime) * 0.1f, -5.0f, 5.0f);
            m_pitchIntegral = std::clamp(m_pitchIntegral + pitchDiff * static_cast<float>(deltaTime) * 0.1f, -5.0f, 5.0f);

            float yawStep = yawDiff * pFactor * static_cast<float>(deltaTime) * 8.0f + m_yawIntegral * 0.2f;
            float pitchStep = pitchDiff * pFactor * static_cast<float>(deltaTime) * 6.0f + m_pitchIntegral * 0.2f;
            // The recovered rotation layer clamps a step to its remaining
            // error. The old port omitted that layer and overshot at speed 7+.
            yawStep=std::clamp(yawStep,std::min(0.f,yawDiff),std::max(0.f,yawDiff));
            pitchStep=std::clamp(pitchStep,std::min(0.f,pitchDiff),std::max(0.f,pitchDiff));

            float newYaw = currentYaw + yawStep;
            float newPitch = std::clamp(currentPitch + pitchStep, -90.0f, 90.0f);

            // Apply GCD quantization
            newYaw = Math::RotationUtil::applySensitivityGCD(currentYaw, newYaw, mouseSensitivity);
            newPitch = Math::RotationUtil::applySensitivityGCD(currentPitch, newPitch, mouseSensitivity);

            return {Math::RotationUtil::wrapDegrees(newYaw), std::clamp(newPitch, -90.0f, 90.0f)};
        }

        float m_yawIntegral{0.0f};
        float m_pitchIntegral{0.0f};
        int m_targetId{-1};
        std::mt19937 m_rng;
    };
}
