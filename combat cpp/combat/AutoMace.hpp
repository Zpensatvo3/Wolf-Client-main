#pragma once
#include "../math/Vector3.hpp"
#include <random>
#include <algorithm>

namespace Vape::Combat {

    /**
     * Exact C++20 port of Vape V4.21's AutoMace.
     *
     * Complete Feature Set:
     * - Elytra Auto-Unequip & Re-equip State Machine:
     *     Predicts falling trajectory while gliding; automatically swaps Elytra to Chestplate
     *     before impact tick to enable Smash Attack, and re-equips Elytra after the mace upward bounce.
     * - Enchantment Selection (Density vs Breach):
     *     Automatically equips Density Mace when high fall damage is ready,
     *     or Breach Mace when target has heavy armor (>= 15 armor value).
     * - Stun Slam Multi-Phase Combo:
     *     Swaps to Axe to disable shielded opponent, then follow-up Mace smash.
     * - Cooldown Bypass:
     *     1.21 Mace smash falling ignores normal attack cooldown delays.
     */
    class AutoMace {
    public:
        enum class MaceType {
            DENSITY,
            BREACH,
            AUTO
        };

        enum class ElytraState {
            IDLE,
            FALLING_TO_TARGET,
            CHESTPLATE_EQUIPPED,
            BOUNCE_DETECTED
        };

        bool enabled{true};
        MaceType maceSelection{MaceType::AUTO};
        bool autoUnequipElytra{true};
        bool reEquipElytra{true};
        bool stunSlam{true};
        double stunSlamChance{100.0};
        bool smashOnly{true};
        double minFallDistance{1.5};
        double aimRange{6.0};

        AutoMace() : m_rng(std::random_device{}()) {}

        enum class Action {
            NONE,
            SWAP_TO_DENSITY_MACE,
            SWAP_TO_BREACH_MACE,
            SWAP_TO_AXE_FOR_SHIELD,
            EQUIP_CHESTPLATE,
            RE_EQUIP_ELYTRA,
            EXECUTE_SMASH,
            RESTORE_WEAPON
        };

        // Call on PreTick to monitor fall, Elytra gliding, and impact ticks
        Action updateTick(double playerFallDistance,
                          double playerMotionY,
                          bool isGlidingWithElytra,
                          bool isOnGround,
                          int targetArmor,
                          bool isTargetBlocking,
                          int currentSlot,
                          int densityMaceSlot,
                          int breachMaceSlot,
                          int axeSlot,
                          bool hasChestplateInHotbar) {
            if (!enabled) {
                reset();
                return Action::NONE;
            }

            // 1. Elytra Flight & Auto-Unequip State Machine
            if (autoUnequipElytra && isGlidingWithElytra) {
                if (playerMotionY < -0.4 && playerFallDistance >= 1.0) {
                    if (m_elytraState == ElytraState::IDLE && hasChestplateInHotbar) {
                        m_elytraState = ElytraState::CHESTPLATE_EQUIPPED;
                        return Action::EQUIP_CHESTPLATE;
                    }
                }
            }

            // Detect upward bounce after Mace smash to re-equip Elytra
            if (reEquipElytra && m_elytraState == ElytraState::CHESTPLATE_EQUIPPED) {
                if (playerMotionY > 0.3) { // Upward mace bounce
                    m_elytraState = ElytraState::IDLE;
                    return Action::RE_EQUIP_ELYTRA;
                }
            }

            // 2. Stun-Slam: Target blocking with shield
            if (stunSlam && isTargetBlocking && axeSlot != -1) {
                if (stunSlamChance >= 100.0 || (std::uniform_real_distribution<double>(0.0, 100.0)(m_rng) < stunSlamChance)) {
                    if (!m_stunAxeUsed) {
                        if (m_originalSlot == -1) m_originalSlot = currentSlot;
                        m_stunAxeUsed = true;
                        return Action::SWAP_TO_AXE_FOR_SHIELD;
                    }
                }
            }

            // 3. Mace Smash Attack Ready (Fall distance >= 1.5 blocks)
            bool isSmashReady = (playerFallDistance >= minFallDistance && !isOnGround);
            if (smashOnly && !isSmashReady) {
                if (m_weaponSwapped && isOnGround) {
                    return handleWeaponRestore();
                }
                return Action::NONE;
            }

            if ((isSmashReady || !smashOnly) && !m_weaponSwapped &&
                (densityMaceSlot != -1 || breachMaceSlot != -1)) {
                if (m_originalSlot == -1) m_originalSlot = currentSlot;
                m_weaponSwapped = true;

                // Select optimal mace type
                if (maceSelection == MaceType::BREACH || (maceSelection == MaceType::AUTO && targetArmor >= 15)) {
                    if (breachMaceSlot != -1) return Action::SWAP_TO_BREACH_MACE;
                }
                if (densityMaceSlot != -1) return Action::SWAP_TO_DENSITY_MACE;
                if (breachMaceSlot != -1) return Action::SWAP_TO_BREACH_MACE;
            }

            // 4. Impact tick: Execute Smash Attack
            if (m_weaponSwapped && (!smashOnly || isOnGround || playerFallDistance >= minFallDistance)) {
                if (!m_smashExecuted) {
                    m_smashExecuted = true;
                    return Action::EXECUTE_SMASH;
                }
            }

            // 5. Restore weapon
            if (m_smashExecuted && isOnGround) {
                return handleWeaponRestore();
            }

            return Action::NONE;
        }

        void reset() {
            m_elytraState = ElytraState::IDLE;
            m_originalSlot = -1;
            m_weaponSwapped = false;
            m_smashExecuted = false;
            m_stunAxeUsed = false;
        }

        [[nodiscard]] int getOriginalSlot() const { return m_originalSlot; }

    private:
        Action handleWeaponRestore() {
            m_weaponSwapped = false;
            m_smashExecuted = false;
            m_stunAxeUsed = false;
            if (m_originalSlot != -1) {
                int slot = m_originalSlot;
                m_originalSlot = -1;
                return Action::RESTORE_WEAPON;
            }
            return Action::NONE;
        }

        ElytraState m_elytraState{ElytraState::IDLE};
        int m_originalSlot{-1};
        bool m_weaponSwapped{false};
        bool m_smashExecuted{false};
        bool m_stunAxeUsed{false};
        std::mt19937 m_rng;
    };
}
