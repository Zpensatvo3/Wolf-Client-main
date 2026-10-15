#pragma once
#include <vector>
#include <chrono>

namespace Vape::Combat {

    class AttackPacketTimingTracker {
    public:
        void onAttackSent(int targetEntityId) {
            m_lastTargetId = targetEntityId;
            m_lastAttackTime = std::chrono::steady_clock::now();
        }

        void onDamageConfirmed(int targetEntityId) {
            if (m_lastTargetId == -1 || targetEntityId != m_lastTargetId) return;

            auto now = std::chrono::steady_clock::now();
            auto delayMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastAttackTime).count();

            if (delayMs > 0 && delayMs < 600) {
                m_rollingDelays.push_back(delayMs);
                if (m_rollingDelays.size() > 20) {
                    m_rollingDelays.erase(m_rollingDelays.begin());
                }
            }
            m_lastConfirmedTime = now;
        }

        [[nodiscard]] long long getAverageHitDelayMs() const {
            if (m_rollingDelays.empty()) return 50; // Default 1 tick (50ms)
            long long sum = 0;
            for (auto d : m_rollingDelays) sum += d;
            return sum / static_cast<long long>(m_rollingDelays.size());
        }

    private:
        int m_lastTargetId{-1};
        std::chrono::steady_clock::time_point m_lastAttackTime{};
        std::chrono::steady_clock::time_point m_lastConfirmedTime{};
        std::vector<long long> m_rollingDelays;
    };
}
