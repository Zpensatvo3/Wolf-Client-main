#pragma once
#include "AutoClickerTimingState.hpp"
#include <random>
#include <chrono>
#include <cmath>
#include <algorithm>

namespace Vape::Combat {

    /**
     * Exact C++20 port of Vape V4.21's ClickEngine.
     *
     * Features:
     * - 3 Randomization Modes:
     *     0: Normal (Fixed 1000/CPS)
     *     1: Extra  (Legacy burst clicking & fast/slow phase cycling)
     *     2: Extra+ (Bio-mimetic AutoClickerTimingState with fatigue and drift)
     * - Step-Interpolated Jitter:
     *     Generates organic offset [-7.0, +7.0], distributes it across (abs(H) + abs(V)) * 0.45 steps,
     *     and applies mouse sensitivity scaling (sens * 0.6 + 0.2)^3 * 8.0.
     */
    class ClickEngine {
    public:
        enum class Button { LEFT, RIGHT };
        enum class RandomizationMode { NORMAL, EXTRA, EXTRA_PLUS };

        ClickEngine(Button button, double minCps = 9.0, double maxCps = 13.5,
                    RandomizationMode mode = RandomizationMode::EXTRA_PLUS)
            : m_button(button), m_minCps(minCps), m_maxCps(maxCps),
              m_randomizationMode(mode), m_rng(std::random_device{}()) {
            m_timingState.configureCpsRange(static_cast<int>(minCps), static_cast<int>(maxCps));
            calculateNextDelay();
        }

        [[nodiscard]] bool canClick() const {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastClick).count();
            return elapsed >= m_currentDelayMs;
        }

        void onClickTriggered() {
            m_lastClick = std::chrono::steady_clock::now();
            if (m_jitterEnabled) {
                generateJitter();
            }
            calculateNextDelay();
        }

        // Call on PreRenderTick / Frame to smoothly apply step-interpolated jitter to player camera
        void applyJitterRotation(float& playerYaw, float& playerPitch, float mouseSensitivity) {
            if (!m_jitterEnabled || (m_pendingYawJitter == 0.0f && m_pendingPitchJitter == 0.0f)) {
                return;
            }

            float sensitivityFactor = mouseSensitivity * 0.6f + 0.2f;
            float mouseScale = sensitivityFactor * sensitivityFactor * sensitivityFactor * 8.0f;

            float yawDelta = m_pendingYawJitter * mouseScale;
            float pitchDelta = -m_pendingPitchJitter * mouseScale;

            playerYaw += yawDelta;
            playerPitch = std::clamp(playerPitch + pitchDelta, -90.0f, 90.0f);

            m_pendingYawJitter = std::floor(m_pendingYawJitter);
            m_pendingPitchJitter = std::floor(m_pendingPitchJitter);
        }

        // Call on PreTick to advance remaining jitter interpolation steps
        void updateJitter() {
            if (!m_jitterEnabled) return;

            if (m_remainingJitterSteps > 0.0) {
                m_pendingYawJitter += static_cast<float>(m_horizontalJitterOffset / m_jitterStepCount);
                m_pendingPitchJitter += static_cast<float>(m_verticalJitterOffset / m_jitterStepCount);
                m_remainingJitterSteps -= 1.0;
            } else {
                m_pendingYawJitter = std::floor(m_pendingYawJitter);
                m_pendingPitchJitter = std::floor(m_pendingPitchJitter);
            }
        }

        void setCps(double minCps, double maxCps) {
            m_minCps = minCps;
            m_maxCps = maxCps;
            m_timingState.configureCpsRange(static_cast<int>(minCps), static_cast<int>(maxCps));
            calculateNextDelay();
        }

        void setJitter(bool enabled) {
            m_jitterEnabled = enabled;
        }

        void setRandomizationMode(RandomizationMode mode) {
            m_randomizationMode = mode;
        }

    private:
        void generateJitter() {
            constexpr float maxOffset = 7.0f;
            std::uniform_real_distribution<double> dist(-maxOffset, maxOffset);
            m_horizontalJitterOffset = dist(m_rng);
            m_verticalJitterOffset = dist(m_rng);

            m_remainingJitterSteps = m_jitterStepCount =
                (std::abs(m_horizontalJitterOffset) + std::abs(m_verticalJitterOffset)) * 0.45;
        }

        void calculateNextDelay() {
            int minCpsInt = static_cast<int>(m_minCps);
            int maxCpsInt = static_cast<int>(m_maxCps);
            int rangeSize = maxCpsInt - minCpsInt;

            std::uniform_int_distribution<int> cDist(0, std::max(0, rangeSize - 1));
            int selectedCps = rangeSize <= 0 ? minCpsInt : (minCpsInt + cDist(m_rng) + 1);
            if (selectedCps <= 0) selectedCps = 1;

            if (m_randomizationMode == RandomizationMode::NORMAL) {
                m_currentDelayMs = 1000 / selectedCps;
                return;
            }

            if (m_randomizationMode == RandomizationMode::EXTRA) {
                m_currentDelayMs = calculateLegacyExtraDelay(selectedCps);
                return;
            }

            // RandomizationMode::EXTRA_PLUS (AutoClickerTimingState)
            m_timingState.configureCpsRange(minCpsInt, maxCpsInt);
            m_currentDelayMs = m_timingState.getNextDelayMillis();
        }

        // Mode 1: Vape's Legacy Extra mode with burst phases and micro-delays
        long long calculateLegacyExtraDelay(int selectedCps) {
            if (!m_burstActive) {
                m_currentDelayMs = 1000 / selectedCps;
                std::uniform_int_distribution<int> bRoll(0, 3);
                if (bRoll(m_rng) == 1) {
                    m_burstActive = true;
                    std::uniform_int_distribution<int> bLen(1, 5);
                    m_burstLength = bLen(m_rng);
                }
            }

            if (m_burstActive && ++m_burstClickCount >= m_burstLength) {
                m_burstClickCount = 0;
                m_burstActive = false;
            }

            std::uniform_int_distribution<int> modRoll(0, 47);
            int modVal = m_fastPhaseActive ? 6 : 10;
            if (modRoll(m_rng) % modVal == 0 && !m_burstActive) {
                std::uniform_int_distribution<long long> extraD(40, 84);
                m_currentDelayMs += extraD(m_rng);
            }

            if (m_fastPhaseActive) {
                if (++m_fastPhaseClickCount >= m_configuredFastPhaseLength) {
                    std::uniform_int_distribution<int> sLen(75, 199);
                    m_slowPhaseLength = sLen(m_rng);
                    m_fastPhaseActive = false;
                    m_fastPhaseClickCount = 0;
                }
                std::uniform_int_distribution<int> pRoll(0, 4);
                long long phaseDelay = (pRoll(m_rng) == 3) ? 50LL : 25LL;
                return m_currentDelayMs + phaseDelay;
            }

            if (++m_slowPhaseClickCount >= m_slowPhaseLength) {
                m_fastPhaseActive = true;
                std::uniform_int_distribution<int> fLen(7, 14);
                m_configuredFastPhaseLength = fLen(m_rng);
                m_slowPhaseClickCount = 0;
            }

            return m_currentDelayMs;
        }

        Button m_button;
        double m_minCps{9.0};
        double m_maxCps{13.5};
        RandomizationMode m_randomizationMode{RandomizationMode::EXTRA_PLUS};
        AutoClickerTimingState m_timingState;

        std::chrono::steady_clock::time_point m_lastClick{};
        long long m_currentDelayMs{100};

        // Legacy Extra state
        bool m_burstActive{false};
        int m_burstLength{0};
        int m_burstClickCount{0};
        bool m_fastPhaseActive{true};
        int m_fastPhaseClickCount{0};
        int m_configuredFastPhaseLength{10};
        int m_slowPhaseLength{100};
        int m_slowPhaseClickCount{0};

        // Jitter state
        bool m_jitterEnabled{false};
        double m_horizontalJitterOffset{0.0};
        double m_verticalJitterOffset{0.0};
        double m_jitterStepCount{0.0};
        double m_remainingJitterSteps{0.0};
        float m_pendingYawJitter{0.0f};
        float m_pendingPitchJitter{0.0f};

        std::mt19937 m_rng;
    };
}
