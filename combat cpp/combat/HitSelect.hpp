#pragma once
#include <random>

namespace Vape::Combat {

    class HitSelect {
    public:
        enum class Mode {
            PASSIVE, // Waits for incoming hit before allowing attack
            ACTIVE   // Immediately triggers counter-attack upon taking damage
        };

        bool enabled{true};
        Mode mode{Mode::ACTIVE};
        double chance{90.0};

        HitSelect() : m_rng(std::random_device{}()) {}

        // Call when local player takes damage (hurt-time reset)
        [[nodiscard]] bool onDamageTaken() {
            if (!enabled) return false;

            if (chance < 100.0) {
                std::uniform_real_distribution<double> dist(0.0, 100.0);
                if (dist(m_rng) > chance) return false;
            }

            m_waitingForHit = false;
            return mode == Mode::ACTIVE; // True triggers instant counter-attack
        }

        // Returns true if player should hold off on attacking while waiting for hit trade
        [[nodiscard]] bool shouldCancelPrematureAttack() const {
            if (!enabled) return false;
            return m_waitingForHit;
        }

        void onTick(int hurtTime) {
            if (!enabled) return;
            if (hurtTime == 0 && !m_waitingForHit) {
                m_waitingForHit = true;
            }
        }

    private:
        bool m_waitingForHit{true};
        std::mt19937 m_rng;
    };
}
