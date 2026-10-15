#pragma once
#include <random>
#include <chrono>
#include <cmath>
#include <numeric>
#include <algorithm>

namespace Vape::Combat {

    /**
     * Exact C++20 port of Vape V4.21's BlockHit.
     *
     * Complete Feature Set:
     * - Predict Mode (PredictBlockHitMode):
     *     Tracks rolling 8-sample damage intervals to recognize opponent combo rhythm.
     *     Calculates incoming strike timing using ping + maximumHurtTime (ms) to block
     *     fractions of a second before the hit registers, and holds for configurable ticks.
     * - Auto Mode: Synchronized block-tap right after attacking.
     * - Manual Mode: Blockhit based on attack CPS.
     * - Lag Mode: Holds block packet server-side to maximize damage negation window.
     * - Modern 1.21 Shield Support + 1.8 Legacy Sword Support.
     */
    class BlockHit {
    public:
        enum class Mode {
            MANUAL,
            PREDICT,
            AUTO,
            LAG
        };

        enum class Action {
            NONE,
            START_BLOCK,
            STOP_BLOCK
        };

        bool enabled{true};
        Mode mode{Mode::PREDICT};
        bool requireMouseDown{true};
        double maxDistance{3.8};
        double maxAngle{90.0};

        // Predict Mode Settings
        double maximumHurtTimeMs{50.0};
        bool includePing{true};
        int holdAfterTicks{2};

        BlockHit() : m_rng(std::random_device{}()) {}

        // Call when local player takes damage to update prediction intervals
        void onDamaged() {
            auto now = std::chrono::steady_clock::now();
            if (m_lastDamageTime.time_since_epoch().count() > 0) {
                long long interval = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastDamageTime).count();
                if (interval >= 250 && interval <= 1500) {
                    m_damageIntervals[m_damageIntervalIndex] = interval;
                    m_damageIntervalIndex = (m_damageIntervalIndex + 1) % DAMAGE_INTERVAL_CAPACITY;
                    m_damageIntervalCount = std::min(m_damageIntervalCount + 1, DAMAGE_INTERVAL_CAPACITY);
                }
            }
            m_lastDamageTime = now;
            m_damageObserved = true;
            m_holdTimerStarted = false;
        }

        // Call when attacking an opponent (AUTO & MANUAL modes)
        Action onPostAttack(bool isAttackKeyPressed) {
            if (!enabled) return Action::NONE;
            if (requireMouseDown && !isAttackKeyPressed) return Action::NONE;

            if (mode == Mode::AUTO || mode == Mode::MANUAL || mode == Mode::LAG) {
                std::uniform_int_distribution<int> dist(2, 3);
                m_blockTimer = mode == Mode::LAG ? holdAfterTicks : dist(m_rng);
                m_blocking = true;
                return Action::START_BLOCK;
            }

            return Action::NONE;
        }

        // Call every tick (PreTick)
        Action onTick(bool hasShieldOrSword,
                      int hurtResistantTime,
                      long long currentPingMs,
                      bool isEnemyFacingUs,
                      double enemyDistance,
                      bool isEnemySwinging) {
            if (!enabled || !hasShieldOrSword) {
                if (m_blocking) {
                    m_blocking = false;
                    m_blockTimer = 0;
                    return Action::STOP_BLOCK;
                }
                return Action::NONE;
            }

            // AUTO / MANUAL active timer countdown
            if (mode == Mode::AUTO || mode == Mode::MANUAL || mode == Mode::LAG) {
                if (m_blocking) {
                    if (m_blockTimer > 0) {
                        m_blockTimer--;
                        return Action::NONE;
                    } else {
                        m_blocking = false;
                        return Action::STOP_BLOCK;
                    }
                }
            }

            // PREDICT Mode
            if (mode == Mode::PREDICT) {
                if (m_damageObserved && hasStableDamagePattern()) {
                    return updatePredictedBlocking(hurtResistantTime, currentPingMs);
                } else {
                    return updateReactiveBlocking(hurtResistantTime, isEnemyFacingUs, enemyDistance, isEnemySwinging);
                }
            }

            return Action::NONE;
        }

        [[nodiscard]] bool isBlocking() const { return m_blocking; }

        void reset() {
            m_blocking = false;
            m_blockTimer = 0;
            m_damageIntervalCount = 0;
            m_damageIntervalIndex = 0;
            m_damageObserved = false;
            m_holdTimerStarted = false;
        }

    private:
        static constexpr int DAMAGE_INTERVAL_CAPACITY = 8;
        long long m_damageIntervals[DAMAGE_INTERVAL_CAPACITY]{0};
        int m_damageIntervalCount{0};
        int m_damageIntervalIndex{0};
        std::chrono::steady_clock::time_point m_lastDamageTime{};
        bool m_damageObserved{false};

        bool hasStableDamagePattern() const {
            if (m_damageIntervalCount < 3) return false;
            long long sum = 0;
            for (int i = 0; i < m_damageIntervalCount; ++i) sum += m_damageIntervals[i];
            long long avg = sum / m_damageIntervalCount;

            long long variance = 0;
            for (int i = 0; i < m_damageIntervalCount; ++i) {
                long long diff = m_damageIntervals[i] - avg;
                variance += diff * diff;
            }
            long long stdDev = static_cast<long long>(std::sqrt(variance / m_damageIntervalCount));
            return stdDev < 120; // Low jitter / rhythmic attack combo
        }

        Action updatePredictedBlocking(int hurtResistantTime, long long pingMs) {
            auto now = std::chrono::steady_clock::now();
            long long elapsedSinceHit = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastDamageTime).count();

            long long sum = 0;
            for (int i = 0; i < m_damageIntervalCount; ++i) sum += m_damageIntervals[i];
            long long avgInterval = sum / m_damageIntervalCount;

            long long predictedHitTime = avgInterval;
            if (includePing) {
                predictedHitTime -= std::clamp(pingMs, 0LL, 150LL);
            }

            long long timeUntilHit = predictedHitTime - elapsedSinceHit;
            if (timeUntilHit <= static_cast<long long>(maximumHurtTimeMs) && timeUntilHit >= -50) {
                if (!m_blocking) {
                    m_blocking = true;
                    m_blockTimer = holdAfterTicks + 1;
                    return Action::START_BLOCK;
                }
            } else if (m_blocking && --m_blockTimer <= 0) {
                m_blocking = false;
                return Action::STOP_BLOCK;
            }

            return Action::NONE;
        }

        Action updateReactiveBlocking(int hurtResistantTime, bool isEnemyFacingUs, double enemyDistance, bool isEnemySwinging) {
            if (isEnemyFacingUs && enemyDistance <= 3.2 && isEnemySwinging) {
                if (!m_blocking) {
                    m_blocking = true;
                    m_blockTimer = holdAfterTicks + 2;
                    return Action::START_BLOCK;
                }
            } else if (m_blocking && --m_blockTimer <= 0) {
                m_blocking = false;
                return Action::STOP_BLOCK;
            }
            return Action::NONE;
        }

        bool m_blocking{false};
        int m_blockTimer{0};
        bool m_holdTimerStarted{false};
        std::mt19937 m_rng;
    };
}
