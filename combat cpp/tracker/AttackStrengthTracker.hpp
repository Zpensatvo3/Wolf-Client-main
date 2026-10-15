#pragma once
#include <unordered_map>
#include <chrono>
#include <algorithm>

namespace Vape::Combat {

    enum class WeaponType {
        SWORD,
        AXE,
        MACE,
        PICKAXE,
        SHOVEL,
        HOE,
        FIST
    };

    struct OpponentAttackState {
        std::chrono::steady_clock::time_point lastSwingTime{};
        float attackSpeed{4.0f}; // Default fist attack speed

        void onSwing(WeaponType weapon) {
            lastSwingTime = std::chrono::steady_clock::now();
            switch (weapon) {
                case WeaponType::SWORD:   attackSpeed = 1.6f; break;
                case WeaponType::AXE:     attackSpeed = 0.8f; break;
                case WeaponType::MACE:    attackSpeed = 0.6f; break; // 1.21 Mace is slow (0.6)
                case WeaponType::PICKAXE: attackSpeed = 1.2f; break;
                case WeaponType::SHOVEL:  attackSpeed = 1.0f; break;
                case WeaponType::HOE:     attackSpeed = 2.0f; break;
                case WeaponType::FIST:    attackSpeed = 4.0f; break;
            }
        }

        [[nodiscard]] float getCooldownProgress() const {
            if (lastSwingTime.time_since_epoch().count() == 0) return 1.0f;
            float cooldownMs = (1.0f / std::max(0.1f, attackSpeed)) * 1000.0f;
            auto now = std::chrono::steady_clock::now();
            float elapsedMs = static_cast<float>(
                std::chrono::duration_cast<std::chrono::milliseconds>(now - lastSwingTime).count()
            );
            return std::clamp(elapsedMs / cooldownMs, 0.0f, 1.0f);
        }

        [[nodiscard]] bool isFullCharge() const {
            return getCooldownProgress() >= 0.95f;
        }

        [[nodiscard]] float calculatePredictedDamage(WeaponType weapon, float attackerFallDistance,
                                                    bool isAttackerOnGround, bool isAttackerInLiquid) const {
            float baseDamage = 1.0f;
            switch (weapon) {
                case WeaponType::SWORD:   baseDamage = 7.0f; break; // Diamond/Netherite sword approx
                case WeaponType::AXE:     baseDamage = 9.0f; break;
                case WeaponType::MACE:    baseDamage = 6.0f + (attackerFallDistance > 1.5f ? attackerFallDistance * 3.0f : 0.0f); break;
                default:                  baseDamage = 1.0f; break;
            }

            // Minecraft 1.21 cooldown scaling: baseDamage * (0.2 + cooldown^2 * 0.8)
            float cd = getCooldownProgress();
            baseDamage *= (0.2f + cd * cd * 0.8f);

            // Critical hit calculation (falling, not on ground, not in water)
            if (attackerFallDistance > 0.0f && !isAttackerOnGround && !isAttackerInLiquid) {
                baseDamage *= 1.5f;
            }

            return baseDamage;
        }
    };

    class AttackStrengthTracker {
    public:
        void onOpponentSwing(int entityId, WeaponType weapon) {
            m_states[entityId].onSwing(weapon);
        }

        [[nodiscard]] const OpponentAttackState* getState(int entityId) const {
            auto it = m_states.find(entityId);
            return it != m_states.end() ? &it->second : nullptr;
        }

        void clear() {
            m_states.clear();
        }

    private:
        std::unordered_map<int, OpponentAttackState> m_states;
    };
}
