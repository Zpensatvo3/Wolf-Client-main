#pragma once
#include "../math/Vector3.hpp"
#include <queue>
#include <chrono>
#include <random>

namespace Vape::Combat {

    struct DelayedVelocity {
        Math::Vector3 velocity;
        std::chrono::steady_clock::time_point releaseTime;
    };

    class KnockbackDelay {
    public:
        bool enabled{true};
        double chance{80.0};
        long long groundDelayMinMs{180};
        long long groundDelayMaxMs{250};
        long long airDelayMinMs{40};
        long long airDelayMaxMs{80};
        bool waterCheck{true};

        KnockbackDelay() : m_rng(std::random_device{}()) {}

        // Returns true if velocity should be intercepted and delayed
        bool onReceiveVelocity(const Math::Vector3& vel, bool isOnGround, bool isInLiquid) {
            if (!enabled) return false;
            if (waterCheck && isInLiquid) return false;

            if (chance < 100.0) {
                std::uniform_real_distribution<double> dist(0.0, 100.0);
                if (dist(m_rng) > chance) return false;
            }

            long long delayMs;
            if (isOnGround) {
                std::uniform_int_distribution<long long> dist(groundDelayMinMs, groundDelayMaxMs);
                delayMs = dist(m_rng);
            } else {
                std::uniform_int_distribution<long long> dist(airDelayMinMs, airDelayMaxMs);
                delayMs = dist(m_rng);
            }

            auto release = std::chrono::steady_clock::now() + std::chrono::milliseconds(delayMs);
            m_queue.push({vel, release});
            return true;
        }

        // Returns true and sets 'outVel' if a delayed velocity is ready to be released
        bool pollReleasedVelocity(Math::Vector3& outVel) {
            if (!enabled || m_queue.empty()) return false;

            auto now = std::chrono::steady_clock::now();
            if (now >= m_queue.front().releaseTime) {
                outVel = m_queue.front().velocity;
                m_queue.pop();
                return true;
            }

            return false;
        }

        void clear() {
            while (!m_queue.empty()) m_queue.pop();
        }

    private:
        std::queue<DelayedVelocity> m_queue;
        std::mt19937 m_rng;
    };
}
