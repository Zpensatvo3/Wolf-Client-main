#pragma once
#include <random>
#include <chrono>
#include <cmath>
#include <algorithm>
#include <cstdint>

namespace Vape::Combat {

    /**
     * Exact C++20 port of Vape V4.21's AutoClickerTimingState.
     *
     * Mathematically models human clicking biology:
     * - Irwin-Hall 6-sample Gaussian approximation for click intervals
     * - Fatigue build-up (+0.015 per click up to 40% slowdown) & idle recovery
     * - Neuromuscular muscle-memory delay repetition (~6% chance)
     * - Periodic target CPS drift (every 1.2s - 3.2s) & long-term rhythm shifts (30s - 90s)
     * - Initial engagement burst clicks (2-5 rapid clicks after idle)
     */
    class AutoClickerTimingState {
    public:
        static constexpr long long MIN_DRIFT_INTERVAL_MS = 1200LL;
        static constexpr long long MAX_DRIFT_INTERVAL_MS = 3200LL;
        static constexpr long long MIN_TARGET_INTERVAL_MS = 30000LL;
        static constexpr long long MAX_TARGET_INTERVAL_MS = 90000LL;
        static constexpr long long IDLE_RESET_THRESHOLD_MS = 1200LL;
        static constexpr double MAX_FATIGUE = 0.4;

        AutoClickerTimingState(uint64_t seed = 0x9E3779B97F4A7C15ULL)
            : m_rng(seed == 0 ? std::random_device{}() : seed) {
            uint64_t mixed = mixSeed(seed ^ 0x9E3779B97F4A7C15ULL);
            m_timingSalt = mixSeed(mixed + 0xD1B54A32D192ED83ULL);
        }

        void configureCpsRange(int minCps, int maxCps) {
            double rawMin = std::max(0, minCps);
            double rawMax = std::max(rawMin, static_cast<double>(maxCps));
            double scaledMin = std::max(0.5, rawMin * 1.5);
            double scaledMax = std::max(0.5, rawMax * 1.5);

            m_minimumCps = scaledMin;
            m_maximumCps = std::max(scaledMin + 0.1, scaledMax);

            if (!m_initialized) {
                m_targetCps = (m_minimumCps + m_maximumCps) * 0.5;
                m_currentCps = std::clamp(
                    m_targetCps + sampleNormalDistribution() * 0.25,
                    m_minimumCps, m_maximumCps
                );
            }
        }

        long long getNextDelayMillis() {
            auto nowPoint = std::chrono::steady_clock::now();
            long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                nowPoint.time_since_epoch()
            ).count();

            if (!m_initialized) {
                m_initialized = true;
                initializeTiming(now);
                m_lastClickTimestamp = now;
            }

            long long idleDuration = m_lastClickTimestamp == 0 ? 0 : now - m_lastClickTimestamp;
            if (idleDuration >= IDLE_RESET_THRESHOLD_MS) {
                double recovery = static_cast<double>(std::min(5000LL, idleDuration)) / 10.0;
                m_fatigue = std::max(0.0, m_fatigue - 0.004 * recovery);
                std::uniform_real_distribution<double> uDist(0.0, 1.0);
                std::uniform_int_distribution<int> bDist(2, 5);
                m_burstClicksRemaining = uDist(m_rng) < 0.7 ? bDist(m_rng) : 0;
                m_repeatDelayChance = 0.03 + uDist(m_rng) * 0.06;
                m_pauseChanceOffset = uDist(m_rng) * 0.04;
            }

            std::uniform_real_distribution<double> uDist(0.0, 1.0);
            std::uniform_int_distribution<int> vDist(80, 199);
            if (++m_clicksSinceVariation > vDist(m_rng)) {
                m_repeatDelayChance = 0.03 + uDist(m_rng) * 0.06;
                m_pauseChanceOffset = uDist(m_rng) * 0.04;
                m_clicksSinceVariation = 0;
            }

            // Periodic drift check
            if (now >= m_nextDriftTimestamp) {
                double drift = sampleNormalDistribution();
                m_currentCps += 0.25 * (m_targetCps - m_currentCps) + 0.45 * drift;
                if (uDist(m_rng) < 0.03) {
                    m_currentCps += (uDist(m_rng) - 0.5) * 1.2;
                }
                m_currentCps = std::clamp(m_currentCps, m_minimumCps, m_maximumCps);
                scheduleDrift(now);
            }

            // Long-term target rhythm shift
            if (now >= m_nextTargetTimestamp) {
                m_targetOffset = (uDist(m_rng) * 2.0 - 1.0) * 0.75;
                m_targetCps = std::clamp(
                    (m_minimumCps + m_maximumCps) * 0.5 + m_targetOffset,
                    m_minimumCps, m_maximumCps
                );
                scheduleTargetChange(now);
            }

            bool isBurst = m_burstClicksRemaining > 0;
            double effectiveCps = m_currentCps;
            if (isBurst) {
                effectiveCps *= 1.0 + 0.05 * (0.4 + 0.8 * uDist(m_rng));
                --m_burstClicksRemaining;
            }

            effectiveCps *= (1.0 - std::min(MAX_FATIGUE, m_fatigue));
            effectiveCps = std::clamp(effectiveCps, m_minimumCps, m_maximumCps);

            double baseDelay = 1000.0 / effectiveCps;
            double timingMultiplier = std::exp(0.24 * sampleClickNoise());

            std::uniform_int_distribution<int> jDist(-5, 5);
            int maxMicroJitter = 35 + jDist(m_rng);
            std::uniform_int_distribution<int> rJitter(0, std::max(1, maxMicroJitter));
            double delayMs = baseDelay * timingMultiplier + rJitter(m_rng);

            if (isBurst) {
                std::uniform_int_distribution<int> bAdd(0, 14);
                delayMs += bAdd(m_rng);
            }

            // Neuromuscular muscle-memory delay repetition
            if (m_previousDelayMs > 0 && uDist(m_rng) < m_repeatDelayChance) {
                std::uniform_int_distribution<long long> repAdd(-3, 3);
                delayMs = static_cast<double>(m_previousDelayMs + repAdd(m_rng));
            }

            if (uDist(m_rng) < 0.07) {
                delayMs *= (0.7 + uDist(m_rng) * 0.2);
            }

            // Human micro-pause chance (fatigue induced)
            double pauseChance = std::min(0.04 + 0.12 * m_fatigue + m_pauseChanceOffset, 0.18);
            if (uDist(m_rng) < pauseChance) {
                std::uniform_int_distribution<int> pAdd(50, 150);
                delayMs += pAdd(m_rng);
            }

            m_fatigue = std::min(MAX_FATIGUE, m_fatigue + 0.015);
            m_lastClickTimestamp = now;
            m_previousDelayMs = static_cast<long long>(std::max(1.0, std::min(delayMs, 2147483647.0)));
            return m_previousDelayMs;
        }

    private:
        static uint64_t mixSeed(uint64_t seed) {
            seed = (seed ^ (seed >> 30)) * 0xBF58476D1CE4E5B9ULL;
            seed = (seed ^ (seed >> 27)) * 0x94D049BB133111EBULL;
            return seed ^ (seed >> 31);
        }

        // Irwin-Hall 6-sample distribution (Vape's exact algorithm)
        double sampleNormalDistribution() {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            double sum = 0.0;
            for (int i = 0; i < 6; ++i) {
                sum += dist(m_rng);
            }
            return (sum - 3.0) * 1.22474487139;
        }

        double sampleClickNoise() {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            double sample = sampleNormalDistribution();
            double roll = dist(m_rng);
            if (roll < 0.12) {
                return (dist(m_rng) - 0.5) * 3.0;
            }
            if (roll < 0.17) {
                return sample * (1.4 + dist(m_rng) * 0.6);
            }
            return sample;
        }

        void scheduleDrift(long long now) {
            m_nextDriftTimestamp = now + randomDuration(now, MIN_DRIFT_INTERVAL_MS, MAX_DRIFT_INTERVAL_MS);
        }

        void scheduleTargetChange(long long now) {
            m_nextTargetTimestamp = now + randomDuration(now, MIN_TARGET_INTERVAL_MS, MAX_TARGET_INTERVAL_MS);
        }

        long long randomDuration(long long now, long long minDur, long long maxDur) {
            long long timeComponent = (now ^ m_timingSalt) & 0xFFFFLL;
            long long range = maxDur - minDur + 1LL;
            std::uniform_int_distribution<long long> dist(0, range - 1);
            return minDur + (std::abs(timeComponent) + dist(m_rng)) % range;
        }

        void initializeTiming(long long now) {
            scheduleDrift(now);
            scheduleTargetChange(now);
            std::uniform_real_distribution<double> dist(-1.0, 1.0);
            m_targetOffset = dist(m_rng) * 0.75;
            m_targetCps = std::clamp(
                (m_minimumCps + m_maximumCps) * 0.5 + m_targetOffset,
                m_minimumCps, m_maximumCps
            );
        }

        std::mt19937_64 m_rng;
        uint64_t m_timingSalt{0};
        double m_minimumCps{10.0};
        double m_maximumCps{14.0};
        double m_currentCps{12.0};
        double m_targetCps{12.0};
        double m_targetOffset{0.0};
        double m_fatigue{0.0};
        double m_repeatDelayChance{0.06};
        double m_pauseChanceOffset{0.0};

        bool m_initialized{false};
        int m_clicksSinceVariation{0};
        int m_burstClicksRemaining{0};
        long long m_lastClickTimestamp{0};
        long long m_nextDriftTimestamp{0};
        long long m_nextTargetTimestamp{0};
        long long m_previousDelayMs{0};
    };
}
