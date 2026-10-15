#pragma once
#include <algorithm>
#include <cmath>
#include <string>

namespace Vape::Combat {

    enum class ItemMaterial {
        NETHERITE,
        DIAMOND,
        IRON,
        GOLDEN,
        STONE,
        WOODEN,
        MACE_SPECIAL,
        OTHER
    };

    struct WeaponScoreInfo {
        ItemMaterial material{ItemMaterial::OTHER};
        bool isSword{false};
        bool isAxe{false};
        bool isMace{false};
        int sharpnessLevel{0};
        int densityLevel{0}; // 1.21 Mace enchantment: increases damage per fallen block
        int breachLevel{0};  // 1.21 Mace enchantment: reduces target armor effectiveness by 15% per level
        int fireAspectLevel{0};
        int knockbackLevel{0};
    };

    /**
     * Exact C++20 port of Vape V4.21's ItemStackScoreUtil.
     *
     * Calculation Mechanics:
     * - Evaluates weapon attack damage, 1.21 Mace fall damage, Density & Breach multipliers.
     * - Evaluates armor damage reductions, toughness scaling, and protection enchantments.
     * - Provides weapon selection scoring for HitSwap, AutoMace, and Triggerbot.
     */
    class ItemStackScoreUtil {
    public:
        // Returns base melee damage for a given material and weapon type
        [[nodiscard]] static float getBaseDamage(ItemMaterial material, bool isSword, bool isAxe, bool isMace) {
            if (isMace) {
                return 6.0f; // 1.21 Mace base damage
            }
            if (isSword) {
                switch (material) {
                    case ItemMaterial::NETHERITE: return 8.0f;
                    case ItemMaterial::DIAMOND:   return 7.0f;
                    case ItemMaterial::IRON:      return 6.0f;
                    case ItemMaterial::STONE:     return 5.0f;
                    case ItemMaterial::WOODEN:
                    case ItemMaterial::GOLDEN:    return 4.0f;
                    default:                      return 1.0f;
                }
            }
            if (isAxe) {
                switch (material) {
                    case ItemMaterial::NETHERITE:
                    case ItemMaterial::DIAMOND:   return 9.0f;
                    case ItemMaterial::IRON:      return 9.0f;
                    case ItemMaterial::STONE:     return 9.0f;
                    case ItemMaterial::WOODEN:
                    case ItemMaterial::GOLDEN:    return 7.0f;
                    default:                      return 1.0f;
                }
            }
            return 1.0f;
        }

        // Calculates total weapon damage against a specific target armor and fall state
        [[nodiscard]] static float calculateTotalDamage(const WeaponScoreInfo& weapon,
                                                        double playerFallDistance,
                                                        float targetArmor,
                                                        float targetToughness) {
            float damage = getBaseDamage(weapon.material, weapon.isSword, weapon.isAxe, weapon.isMace);

            // 1.21 Mace Smash Attack Mechanics
            if (weapon.isMace && playerFallDistance > 1.5) {
                // Vanilla fall damage addition: 3.0 damage per fallen block
                float fallDamageBonus = static_cast<float>(playerFallDistance * 3.0);

                // Density enchantment: adds +0.5 damage per level per fallen block
                if (weapon.densityLevel > 0) {
                    fallDamageBonus += static_cast<float>(playerFallDistance * 0.5 * weapon.densityLevel);
                }
                damage += fallDamageBonus;
            }

            // Sharpness enchantment: adds 0.5 * level + 0.5 damage
            if (weapon.sharpnessLevel > 0) {
                damage += (0.5f * weapon.sharpnessLevel + 0.5f);
            }

            // Target Armor Reductions
            float effectiveArmor = targetArmor;

            // 1.21 Breach enchantment: reduces target armor effectiveness by 15% per level (up to 60%)
            if (weapon.breachLevel > 0) {
                float armorReductionPercent = std::min(0.60f, weapon.breachLevel * 0.15f);
                effectiveArmor *= (1.0f - armorReductionPercent);
            }

            // Apply Minecraft armor formula
            float toughnessScale = 2.0f + targetToughness / 4.0f;
            float clampedArmor = std::clamp(effectiveArmor - damage / toughnessScale, effectiveArmor * 0.2f, 20.0f);
            damage *= (1.0f - clampedArmor / 25.0f);

            return std::max(0.0f, damage);
        }

        // Weapon scoring algorithm to choose the highest damage output weapon
        [[nodiscard]] static float scoreWeapon(const WeaponScoreInfo& weapon,
                                               double fallDistance,
                                               float targetArmor,
                                               float targetToughness,
                                               bool isTargetBlockingWithShield) {
            // Against shield, Axe has maximum utility (disables shield)
            if (isTargetBlockingWithShield && weapon.isAxe) {
                return 1000.0f + getBaseDamage(weapon.material, false, true, false);
            }

            // During high fall (smash attack), Mace has maximum utility
            if (fallDistance >= 1.5 && weapon.isMace) {
                return 500.0f + calculateTotalDamage(weapon, fallDistance, targetArmor, targetToughness);
            }

            return calculateTotalDamage(weapon, fallDistance, targetArmor, targetToughness);
        }
    };
}
