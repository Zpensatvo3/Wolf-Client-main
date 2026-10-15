#pragma once
#include <cstdint>

namespace Vape::Combat {

    class WTap {
    public:
        enum class Mode {
            PACKET,   // Sends STOP_SPRINTING then START_SPRINTING packet
            KEYBOARD  // Simulates tapping the W key
        };

        enum class Action {
            NONE,
            SEND_SPRINT_RESET_PACKETS,
            RELEASE_FORWARD_KEY,
            PRESS_FORWARD_KEY
        };

        bool enabled{true};
        Mode mode{Mode::PACKET};

        // Call when an attack occurs
        Action onAttack(bool isSprinting) {
            if (!enabled || !isSprinting) return Action::NONE;

            if (mode == Mode::PACKET) {
                return Action::SEND_SPRINT_RESET_PACKETS;
            } else {
                m_state = 1; // Start keyboard cycle
                return Action::NONE;
            }
        }

        // Call every client tick for keyboard mode simulation
        Action onTick() {
            if (!enabled || mode != Mode::KEYBOARD || m_state == 0) {
                return Action::NONE;
            }

            if (m_state == 1) {
                m_state = 2;
                return Action::RELEASE_FORWARD_KEY;
            } else if (m_state == 2) {
                m_state = 0;
                return Action::PRESS_FORWARD_KEY;
            }

            return Action::NONE;
        }

    private:
        int m_state{0};
    };
}
