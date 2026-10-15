#pragma once
#include "../utility/Timer.hpp"
#include <random>
#include <optional>
#include <algorithm>

namespace Vape::Combat {

    /**
     * Independent C++ decision model, checked against the recovered Java Triggerbot.
     *
     * Complete Feature Set:
     * - Mouse Over Delay (0-200ms): Simulates human reaction time after crosshair reaches target
     * - Select First Hit: Intentionally waits for opponent to hit first (combo lock) with 1000ms grace period
     * - Air Crits: Prevents attacking in air unless falling downwards (guaranteed 1.5x critical hit)
     * - Shield Check: Detects blocking opponents; automatically bypassed if wielding/swapping Axe or Mace
     * - Extra Delay & Early Hit Chance: Configurable tick delay after cooldown (negative values hit early)
     * - Target Miss Chance: Randomly swings and misses when hovering valid targets to mimic human error
     * - Activation Click Control: Ignores premature manual clicks when attack is not ready
     * - 1.21 Cooldown Bypass: Auto-Mace falling or HitSwap weapon swaps bypass cooldown restrictions
     */
    class Triggerbot {
    public:
        // Configurable Values
        bool enabled{true};
        bool requireMouseDown{true};
        bool ignoreActivationClick{false}; // Not advertised until input cancellation is supported.
        bool airCrits{true};
        bool shieldCheck{true};
        bool selectFirstHit{false};
        double targetMissChance{0.0};  // % chance to miss
        double earlyHitChance{0.0};    // % chance to hit 2-3 ticks early
        double extraDelayMinTicks{0.0};
        double extraDelayMaxTicks{0.0};
        long long mouseOverDelayMinMs{0};
        long long mouseOverDelayMaxMs{80};
        bool limitToWeapons{true};

        Triggerbot() : m_rng(std::random_device{}()) {}

        enum class TriggerAction {
            NONE,
            ATTACK,
            SWING_MISS
        };

        // Java asks getCooledAttackStrength(-extraDelay + earlyHitTicks).
        // An offset is a number of PLAYER ticks, not 5% of a weapon's cooldown.
        // The adapter must read vanilla cooldown at this offset before onTick.
        float cooldownOffsetTicks(int targetId) {
            if (targetId != m_cycleTargetId) {
                m_cycleTargetId = targetId;
                m_offsetSampled = false;
            }
            if (!m_offsetSampled) {
                const auto low = std::min(extraDelayMinTicks, extraDelayMaxTicks);
                const auto high = std::max(extraDelayMinTicks, extraDelayMaxTicks);
                m_extraTicks = low == high ? low : std::uniform_real_distribution<double>(low, high)(m_rng);
                m_earlyHitTicks = 0;
                if (earlyHitChance > 0.0 && std::uniform_real_distribution<double>(0.0,100.0)(m_rng) < earlyHitChance)
                    m_earlyHitTicks = std::uniform_int_distribution<int>(2,3)(m_rng);
                m_offsetSampled = true;
            }
            return static_cast<float>(-m_extraTicks + m_earlyHitTicks);
        }

        // A decision is not an attack receipt. Commit only after vanilla really
        // consumed the local attack; a rejected/preparation-only attempt retries
        // with the same timing sample, never a freshly rolled threshold.
        void onAttackProcessed() {
            m_attackTimer.reset();
            m_offsetSampled = false;
        }

        // Call on mouse / key input event
        void onInput(bool isMouseDown, bool isAttackKey) {
            if (!isAttackKey) return;

            if (!isMouseDown) {
                m_mouseDown = false;
                resetSelectState();
                resetMouseOverState();
                m_offsetSampled = false;
                return;
            }
            m_mouseDown = true;
        }

        // Call every tick (PreTick)
        TriggerAction onTick(bool isPlayerAlive,
                             int playerHurtTime,
                             double playerFallDistance,
                             double playerMotionY,
                             bool isPlayerOnGround,
                             float playerAttackCooldown,
                             bool isHoldingWeapon,
                             bool hasCrosshairEntity,
                             int crosshairEntityId,
                             bool isCrosshairEntityAlive,
                             bool isCrosshairEntityBlocking,
                             bool canBypassShield,
                             bool canBypassCooldown,
                             bool isGuiOpen) {
            if (!enabled || !isPlayerAlive || isGuiOpen) {
                return TriggerAction::NONE;
            }

            // Require mouse down check
            if (requireMouseDown && !m_mouseDown) {
                resetSelectState();
                resetMouseOverState();
                return TriggerAction::NONE;
            }

            // Hurt time tracking for Select First Hit
            bool wasJustHurt = playerHurtTime > m_lastHurtTime;
            m_lastHurtTime = playerHurtTime;

            if (selectFirstHit && m_waitingForFirstHit && m_selectTargetId != -1 && wasJustHurt) {
                m_waitingForFirstHit = false;
            }

            if (!m_attackTimer.hasElapsed(50)) {
                return TriggerAction::NONE;
            }

            if (limitToWeapons && !isHoldingWeapon) {
                return TriggerAction::NONE;
            }

            // Air Crits: only attack in air if falling downward
            if (airCrits && !isPlayerOnGround) {
                if (playerMotionY >= 0.0 || playerFallDistance <= 0.0) {
                    return TriggerAction::NONE;
                }
            }

            bool crosshairValid = hasCrosshairEntity && isCrosshairEntityAlive;

            // Shield check (unless bypassed by Axe / Mace)
            if (crosshairValid && shieldCheck && isCrosshairEntityBlocking && !canBypassShield) {
                crosshairValid = false;
            }

            if (crosshairValid) {
                // Update Select First Hit
                if (!updateSelectFirstHit(crosshairEntityId, wasJustHurt)) {
                    return TriggerAction::NONE;
                }

                // Mouse Over Delay check
                if (!hasMouseOverDelayElapsed(crosshairEntityId)) {
                    return TriggerAction::NONE;
                }

                // Attack cooldown check
                if (canBypassCooldown || playerAttackCooldown >= 1.0f) {
                    // Check miss chance
                    if (targetMissChance > 0.0) {
                        std::uniform_real_distribution<double> dist(0.0, 100.0);
                        if (dist(m_rng) < targetMissChance) {
                            return TriggerAction::SWING_MISS;
                        }
                    }

                    return TriggerAction::ATTACK;
                }
            } else {
                resetMouseOverState();
                updateSelectGrace();
            }

            return TriggerAction::NONE;
        }

        void onDisable() {
            m_mouseDown = false;
            m_earlyHitTicks = 0;
            m_offsetSampled = false;
            m_cycleTargetId = -1;
            resetMouseOverState();
            resetSelectState();
        }

    private:
        bool hasMouseOverDelayElapsed(int entityId) {
            if (m_mouseOverTargetId != entityId) {
                m_mouseOverTargetId = entityId;
                if (mouseOverDelayMaxMs > mouseOverDelayMinMs) {
                    std::uniform_int_distribution<long long> dist(mouseOverDelayMinMs, mouseOverDelayMaxMs);
                    m_mouseOverDelayMs = dist(m_rng);
                } else {
                    m_mouseOverDelayMs = mouseOverDelayMinMs;
                }
                m_mouseOverTimer.reset();
            }
            return m_mouseOverTimer.hasElapsed(m_mouseOverDelayMs);
        }

        void resetMouseOverState() {
            m_mouseOverTargetId = -1;
            m_mouseOverDelayMs = 0;
            m_mouseOverTimer.reset();
        }

        bool updateSelectFirstHit(int entityId, bool wasJustHurt) {
            if (!selectFirstHit) {
                resetSelectState();
                return true;
            }
            if (m_selectTargetId != entityId) {
                m_selectTargetId = entityId;
                m_waitingForFirstHit = true;
            }
            m_graceActive = false;
            m_selectTimer.reset();

            if (m_waitingForFirstHit && wasJustHurt) {
                m_waitingForFirstHit = false;
            }
            return !m_waitingForFirstHit;
        }

        void updateSelectGrace() {
            if (!selectFirstHit || m_selectTargetId == -1) return;

            if (!m_graceActive) {
                m_graceActive = true;
                m_selectTimer.reset();
                return;
            }
            if (m_selectTimer.hasElapsed(1000)) {
                resetSelectState();
            }
        }

        void resetSelectState() {
            m_selectTargetId = -1;
            m_waitingForFirstHit = false;
            m_graceActive = false;
            m_selectTimer.reset();
        }

        Utility::Timer m_attackTimer;
        Utility::Timer m_mouseOverTimer;
        Utility::Timer m_selectTimer;

        bool m_mouseDown{false};
        int m_lastHurtTime{0};
        int m_earlyHitTicks{0};
        int m_cycleTargetId{-1};
        bool m_offsetSampled{false};
        double m_extraTicks{0};

        int m_mouseOverTargetId{-1};
        long long m_mouseOverDelayMs{0};

        int m_selectTargetId{-1};
        bool m_waitingForFirstHit{false};
        bool m_graceActive{false};

        std::mt19937 m_rng;
    };
}
