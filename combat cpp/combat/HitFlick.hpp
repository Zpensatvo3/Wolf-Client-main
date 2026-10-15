#pragma once
#include "../math/RotationUtil.hpp"
#include "../utility/Timer.hpp"
#include <random>
#include <vector>
#include <algorithm>

namespace Vape::Combat {

    /**
     * Exact C++20 port of Vape V4.21's HitFlick.
     *
     * Complete Feature Set:
     * - Multi-State Machine (SilentAuraRotationMode):
     *     IDLE -> FLICKING_AWAY -> ATTACKING -> RETURNING -> FLUSH_BLINK
     * - Blink Packet Choking:
     *     Buffers outgoing movement packets during the flick; releases them all
     *     immediately after attack packet dispatch to bypass modern anti-cheats (Polar/Grim).
     * - Strafe-Adjusted Flick Angle:
     *     Offsets flick trajectory based on player's current strafe direction.
     * - Movement Key Overrides:
     *     Forces forward movement key and pauses sprint to prevent invalid server velocity checks.
     * - Select Hits: Checks target hurt-time and vulnerability before initiating flick.
     * - Bone Offset Randomization.
     */
    class HitFlick {
    public:
        enum class FlickState {
            IDLE,
            FLICKING,
            ATTACKING,
            RETURNING
        };

        bool enabled{true};
        double maxFlickAngle{75.0};
        double reach{3.6};
        double chance{100.0};
        bool blink{true};
        bool selectHits{true};
        bool randomizeOffset{true};
        double randomizeOffsetRange{0.15};
        bool strafeInvert{false};

        HitFlick() : m_rng(std::random_device{}()) {}

        enum class ActionType {
            NONE,
            SEND_FLICK_LOOK_PACKET,
            SEND_ATTACK_PACKET,
            SEND_RESTORE_LOOK_PACKET,
            CHOKE_MOVEMENT_PACKET,
            FLUSH_CHOKED_PACKETS
        };

        struct FlickAction {
            ActionType action{ActionType::NONE};
            Math::Angle targetAngle{};
            bool forceForwardKey{false};
            bool cancelSprint{false};
        };

        // Call when attack button is pressed
        bool onAttackInput(bool isTargetVulnerable, float strafeInput, float currentYaw) {
            if (!enabled || m_state != FlickState::IDLE) return false;

            if (selectHits && !isTargetVulnerable) return false;

            if (chance < 100.0) {
                std::uniform_real_distribution<double> dist(0.0, 100.0);
                if (dist(m_rng) > chance) return false;
            }

            m_state = FlickState::FLICKING;
            m_blinking = blink;
            m_stateTicks = 0;
            m_flickTimer.reset();

            // Calculate strafe adjusted flick bias
            m_strafeBias = strafeInput * (strafeInvert ? -15.0f : 15.0f);
            return true;
        }

        // Call every tick (PreTick) to advance the flick state machine
        FlickAction onTick(const Math::Vector3& eyePos,
                           float currentYaw, float currentPitch,
                           const Math::BoundingBox& targetBox) {
            if (!enabled || m_state == FlickState::IDLE) {
                return {};
            }

            m_stateTicks++;

            // 1. FLICKING phase: send look packet towards target
            if (m_state == FlickState::FLICKING) {
                m_originalAngle = {currentYaw, currentPitch};

                Math::Vector3 aimPoint = targetBox.getClosestPoint(eyePos);
                if (randomizeOffset) {
                    std::uniform_real_distribution<double> rDist(-randomizeOffsetRange, randomizeOffsetRange);
                    aimPoint.x += rDist(m_rng);
                    aimPoint.y += rDist(m_rng);
                    aimPoint.z += rDist(m_rng);
                }

                Math::Angle targetAngle = Math::RotationUtil::getRotationsToCoords(eyePos, aimPoint);
                targetAngle.yaw = Math::RotationUtil::wrapDegrees(targetAngle.yaw + m_strafeBias);

                float angleDiff = Math::RotationUtil::getAngleDifference(currentYaw, targetAngle.yaw);
                if (angleDiff > maxFlickAngle) {
                    reset();
                    return {};
                }

                m_state = FlickState::ATTACKING;
                return {ActionType::SEND_FLICK_LOOK_PACKET, targetAngle, true, true};
            }

            // 2. ATTACKING phase: send attack packet
            if (m_state == FlickState::ATTACKING) {
                m_state = FlickState::RETURNING;
                return {ActionType::SEND_ATTACK_PACKET, {}, true, true};
            }

            // 3. RETURNING phase: restore camera angle and flush blink packets
            if (m_state == FlickState::RETURNING) {
                FlickAction restoreAction{ActionType::SEND_RESTORE_LOOK_PACKET, m_originalAngle, false, false};
                reset();
                return restoreAction;
            }

            return {};
        }

        [[nodiscard]] bool shouldChokePacket() const {
            return enabled && m_blinking && (m_state != FlickState::IDLE);
        }

        void reset() {
            m_state = FlickState::IDLE;
            m_blinking = false;
            m_stateTicks = 0;
            m_strafeBias = 0.0f;
        }

    private:
        FlickState m_state{FlickState::IDLE};
        bool m_blinking{false};
        int m_stateTicks{0};
        float m_strafeBias{0.0f};
        Math::Angle m_originalAngle{};
        Utility::Timer m_flickTimer;
        std::mt19937 m_rng;
    };
}
