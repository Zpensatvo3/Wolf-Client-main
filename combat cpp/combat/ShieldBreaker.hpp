#pragma once

namespace Vape::Combat {

    /**
     * Exact C++20 port of Vape V4.21's ShieldBreaker Module for Minecraft 1.21+ (Fabric/Yarn).
     *
     * Features:
     * - Auto-Detection of Raised Shields:
     *     Monitors target player blocking state within attack range (default 4.0 blocks).
     * - Intelligent Hotbar Axe Swapping:
     *     Automatically finds an Axe in hotbar (slots 0-8), swaps before impact to disable shield (100 ticks / 5s),
     *     and safely restores original weapon slot.
     * - Configurable Timing Delays:
     *     swapDelayTicks (0-5 ticks): Pre-attack delay after selecting axe to bypass packet flag checks.
     *     swapBackDelayTicks (0-5 ticks): Delay before switching back to original weapon.
     * - Follow-Up Double-Click:
     *     Sends an immediate follow-up attack packet (or swing) right after breaking the shield
     *     to guarantee damage on the freshly vulnerable target.
     */
    class ShieldBreaker {
    public:
        // Configurable Parameters
        bool enabled{true};
        int swapDelayTicks{0};         // Ticks to wait after equipping axe before striking
        int swapBackDelayTicks{1};     // Ticks to wait after strike before restoring weapon
        bool doubleClick{true};        // Execute immediate follow-up attack after shield disable
        double maxBreakReach{4.0};     // Maximum distance to attempt shield break

        enum class Action {
            NONE,
            SWAP_TO_AXE_AND_ATTACK,     // 0 tick delay: immediate swap & strike
            SWAP_TO_AXE_DELAYED,        // Delay before attack: switch slot and wait
            EXECUTE_AXE_ATTACK,         // Delayed strike triggered
            EXECUTE_DOUBLE_ATTACK,      // Player already had axe: trigger double strike
            RESTORE_ORIGINAL_SLOT       // Reset back to original weapon slot
        };

        struct AttackAttemptResult {
            Action action{Action::NONE};
            int slotToSelect{-1};
            bool attackRequired{false};
            bool doubleClickRequired{false};
        };

        struct TickResult {
            Action action{Action::NONE};
            int slotToSelect{-1};
            bool attackRequired{false};
            bool doubleClickRequired{false};
        };

        /**
         * Intercepts attack attempt before standard damage packet dispatch.
         * Call when left-click / attack packet is initiated.
         *
         * @param isTargetBlocking true if opponent is actively raising a shield
         * @param targetDistance distance to opponent in blocks
         * @param currentSlot player's currently selected hotbar slot (0-8)
         * @param axeSlot hotbar slot index containing an Axe (-1 if none)
         * @return AttackAttemptResult containing required actions
         */
        AttackAttemptResult onAttackAttempt(bool isTargetBlocking,
                                            double targetDistance,
                                            int currentSlot,
                                            int axeSlot) {
            if (!enabled || m_active) {
                return {};
            }

            if (!isTargetBlocking || targetDistance > maxBreakReach) {
                return {};
            }

            // If player is already holding the axe in main hand
            if (currentSlot == axeSlot) {
                if (doubleClick) {
                    return AttackAttemptResult{
                        .action = Action::EXECUTE_DOUBLE_ATTACK,
                        .slotToSelect = currentSlot,
                        .attackRequired = true,
                        .doubleClickRequired = true
                    };
                }
                return {};
            }

            if (axeSlot == -1) {
                return {}; // No axe available in hotbar
            }

            // Start shield breaker sequence
            m_originalSlot = currentSlot;
            m_axeSlot = axeSlot;
            m_active = true;
            m_swapTicks = 0;
            m_restoreTicks = 0;

            if (swapDelayTicks == 0) {
                // Immediate attack
                m_waitingToAttack = false;
                return AttackAttemptResult{
                    .action = Action::SWAP_TO_AXE_AND_ATTACK,
                    .slotToSelect = axeSlot,
                    .attackRequired = true,
                    .doubleClickRequired = doubleClick
                };
            }

            // Delayed attack
            m_waitingToAttack = true;
            return AttackAttemptResult{
                .action = Action::SWAP_TO_AXE_DELAYED,
                .slotToSelect = axeSlot,
                .attackRequired = false,
                .doubleClickRequired = false
            };
        }

        /**
         * Call every tick (PreTick) to manage delays and weapon restore.
         */
        TickResult onTick(bool isTargetStillBlocking) {
            if (!enabled || !m_active) {
                return {};
            }

            // Handle delayed axe attack
            if (m_waitingToAttack) {
                if (++m_swapTicks >= swapDelayTicks) {
                    m_waitingToAttack = false;
                    return TickResult{
                        .action = Action::EXECUTE_AXE_ATTACK,
                        .slotToSelect = m_axeSlot,
                        .attackRequired = isTargetStillBlocking,
                        .doubleClickRequired = doubleClick
                    };
                }
                return {};
            }

            // Handle weapon restoration delay
            if (++m_restoreTicks >= swapBackDelayTicks) {
                int restoreSlot = m_originalSlot;
                reset();
                if (restoreSlot != -1) {
                    return TickResult{
                        .action = Action::RESTORE_ORIGINAL_SLOT,
                        .slotToSelect = restoreSlot,
                        .attackRequired = false,
                        .doubleClickRequired = false
                    };
                }
            }

            return {};
        }

        void reset() {
            m_active = false;
            m_waitingToAttack = false;
            m_originalSlot = -1;
            m_axeSlot = -1;
            m_swapTicks = 0;
            m_restoreTicks = 0;
        }

        [[nodiscard]] bool isActive() const { return m_active; }
        [[nodiscard]] bool isWaitingToAttack() const { return m_waitingToAttack; }
        [[nodiscard]] int getOriginalSlot() const { return m_originalSlot; }

    private:
        bool m_active{false};
        bool m_waitingToAttack{false};
        int m_originalSlot{-1};
        int m_axeSlot{-1};
        int m_swapTicks{0};
        int m_restoreTicks{0};
    };
}
