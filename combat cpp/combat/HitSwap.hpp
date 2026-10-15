#pragma once

namespace Vape::Combat {

    class HitSwap {
    public:
        bool enabled{true};
        bool swapAxes{true};
        bool swapMaces{true};
        bool stunSlam{true};
        bool autoRestore{true};

        enum class Action {
            NONE,
            SWAP_SLOT,
            TRIGGER_ATTACK,
            RESTORE_SLOT
        };

        struct SwapResult {
            Action action{Action::NONE};
            int targetSlot{-1};
        };

        // Call before attacking an opponent to dynamically choose best weapon
        SwapResult onPreAttack(int currentSlot, bool isTargetBlocking, double playerFallDistance,
                               int targetArmor, int swordSlot, int axeSlot, int maceSlot) {
            if (!enabled || m_swapped) return {};

            // 1. Stun-Slam Multi-Phase: Target is blocking with shield
            if (stunSlam && isTargetBlocking && axeSlot != -1 && maceSlot != -1) {
                m_originalSlot = currentSlot;
                m_swapped = true;
                m_stunSlamActive = true;
                m_stunSlamSecondPhase = false;
                m_restoreTicks = 0;
                return {Action::SWAP_SLOT, axeSlot}; // Phase 1: Axe to disable shield
            }

            // 2. Shield break priority
            if (swapAxes && isTargetBlocking && axeSlot != -1) {
                m_originalSlot = currentSlot;
                m_swapped = true;
                m_restoreTicks = 0;
                return {Action::SWAP_SLOT, axeSlot};
            }

            // 3. Mace Smash priority (falling >= 1.5 blocks)
            if (swapMaces && playerFallDistance >= 1.5 && maceSlot != -1) {
                m_originalSlot = currentSlot;
                m_swapped = true;
                m_restoreTicks = 0;
                return {Action::SWAP_SLOT, maceSlot};
            }

            // 4. Heavy armor priority (armor >= 15): Mace with Breach
            if (swapMaces && targetArmor >= 15 && maceSlot != -1) {
                m_originalSlot = currentSlot;
                m_swapped = true;
                m_restoreTicks = 0;
                return {Action::SWAP_SLOT, maceSlot};
            }

            // 5. Default ground sword
            if (swordSlot != -1 && currentSlot != swordSlot) {
                m_originalSlot = currentSlot;
                m_swapped = true;
                m_restoreTicks = 0;
                return {Action::SWAP_SLOT, swordSlot};
            }

            return {};
        }

        // Call every tick to handle Stun-Slam phase 2 and slot restoration
        SwapResult onTick(int maceSlot) {
            if (!m_swapped) return {};

            // Stun-Slam Phase 2: swap to Mace right after Axe disabled the shield
            if (m_stunSlamActive && !m_stunSlamSecondPhase) {
                if (++m_restoreTicks >= 1) {
                    m_stunSlamSecondPhase = true;
                    m_restoreTicks = 0;
                    if (maceSlot != -1) {
                        return {Action::TRIGGER_ATTACK, maceSlot};
                    }
                }
                return {};
            }

            // Restore original slot
            if (autoRestore) {
                int maxDelay = m_stunSlamActive ? 2 : 1;
                if (++m_restoreTicks >= maxDelay) {
                    int slot = m_originalSlot;
                    reset();
                    if (slot != -1) {
                        return {Action::RESTORE_SLOT, slot};
                    }
                }
            }

            return {};
        }

        void reset() {
            m_originalSlot = -1;
            m_swapped = false;
            m_restoreTicks = 0;
            m_stunSlamActive = false;
            m_stunSlamSecondPhase = false;
        }

    private:
        int m_originalSlot{-1};
        bool m_swapped{false};
        int m_restoreTicks{0};
        bool m_stunSlamActive{false};
        bool m_stunSlamSecondPhase{false};
    };
}
