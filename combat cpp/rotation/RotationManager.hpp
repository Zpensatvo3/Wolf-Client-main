#pragma once
#include "../math/RotationUtil.hpp"
#include <cmath>
#include <numbers>

namespace Vape::Combat {

    class RotationManager {
    public:
        bool movementCorrection{true}; // Silent Strafe Fix

        void setSilentRotations(float yaw, float pitch, int durationTicks = 1) {
            m_targetYaw = yaw;
            m_targetPitch = pitch;
            m_activeTicks = durationTicks;
            m_silentActive = true;
        }

        void clearSilentRotations() {
            m_silentActive = false;
            m_activeTicks = 0;
        }

        void onTick() {
            if (m_silentActive) {
                if (--m_activeTicks <= 0) {
                    clearSilentRotations();
                }
            }
        }

        // Silent Strafe Fix: Rewrites player WASD movement vectors so player moves in the direction
        // of their camera view rather than where the silent combat rotation is pointing.
        void applyMovementCorrection(float& forward, float& sideways, float cameraYaw) const {
            if (!m_silentActive || !movementCorrection) return;
            if (forward == 0.0f && sideways == 0.0f) return;

            float angleDiff = cameraYaw - m_targetYaw;
            float angleRad = static_cast<float>(angleDiff * (std::numbers::pi / 180.0));

            float cosA = std::cos(angleRad);
            float sinA = std::sin(angleRad);

            float correctedForward = forward * cosA - sideways * sinA;
            float correctedSideways = forward * sinA + sideways * cosA;

            forward = std::round(correctedForward);
            sideways = std::round(correctedSideways);
        }

        [[nodiscard]] bool isSilentActive() const { return m_silentActive; }
        [[nodiscard]] float getTargetYaw() const { return m_targetYaw; }
        [[nodiscard]] float getTargetPitch() const { return m_targetPitch; }

    private:
        bool m_silentActive{false};
        float m_targetYaw{0.0f};
        float m_targetPitch{0.0f};
        int m_activeTicks{0};
    };
}
