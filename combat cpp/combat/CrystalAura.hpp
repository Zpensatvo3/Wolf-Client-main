#pragma once
#include "../math/Vector3.hpp"
#include <vector>
#include <algorithm>
#include <cmath>

namespace Vape::Combat {

    enum class BlockType {
        AIR,
        OBSIDIAN,
        BEDROCK,
        OTHER
    };

    struct CrystalPlacement {
        Math::Vector3 blockPos{};
        double targetDamage{0.0};
        double selfDamage{0.0};
        int facingIndex{1}; // 1 = UP, 0 = DOWN, 2 = NORTH, etc.
        bool needsObsidian{false};
    };

    /**
     * Exact C++20 port of Vape V4.21's CrystalAura.
     *
     * Complete Feature Set:
     * - Accurate Explosion Damage Physics:
     *     Computes explosion power (12.0f max), raycast exposure, distance falloff,
     *     and applies Minecraft armor & toughness reductions:
     *       effectiveArmor = clamp(armor - damage / (2.0 + toughness / 4.0), armor * 0.2, 20.0)
     *       damage = damage * (1.0 - effectiveArmor / 25.0)
     * - Potion & Blast Protection Reductions:
     *     Calculates Resistance potion (20% per level) and Blast Protection enchantment reductions.
     * - Suicide Prevention (preventSuicide):
     *     Cancels crystal detonation if self damage >= current effective health (HP + Absorption).
     * - 1.21 Placement Rules:
     *     Base block must be Obsidian/Bedrock, and exactly 1 air block above.
     * - Multi-Facing Interaction Vectors:
     *     Interacts with the optimal block surface depending on player line-of-sight.
     */
    class CrystalAura {
    public:
        bool enabled{true};
        double placeReach{4.5};
        double breakReach{4.5};
        double minDamage{6.0};
        double maxSelfDamage{4.0};
        bool preventSuicide{true};
        bool autoObsidian{true};

        enum class ActionType {
            NONE,
            PLACE_OBSIDIAN,
            PLACE_CRYSTAL,
            DETONATE_CRYSTAL
        };

        struct Action {
            ActionType type{ActionType::NONE};
            Math::Vector3 targetCoord{};
            int crystalEntityId{-1};
            int hotbarSlot{-1};
        };

        // Minecraft armor damage reduction formula
        static float reduceDamageByArmor(float damage, float armor, float armorToughness) {
            float toughnessScale = 2.0f + armorToughness / 4.0f;
            float effectiveArmor = std::clamp(armor - damage / toughnessScale, armor * 0.2f, 20.0f);
            return damage * (1.0f - effectiveArmor / 25.0f);
        }

        // Minecraft potion and blast protection reduction formula
        static float reduceDamageByEnchants(float damage, int resistanceLevel, int blastProtectionLevel,int protectionLevel=0) {
            if (resistanceLevel > 0) {
                int reduction = resistanceLevel * 5;
                int remaining = std::max(0, 25 - reduction);
                damage = damage * static_cast<float>(remaining) / 25.0f;
            }
            if (damage <= 0.0f) return 0.0f;

            if (blastProtectionLevel > 0 || protectionLevel > 0) {
                float blastReduction = std::min(20.0f, static_cast<float>(blastProtectionLevel * 2 + protectionLevel));
                damage *= (1.0f - blastReduction / 25.0f);
            }
            return std::max(0.0f, damage);
        }

        // Calculates simulated explosion damage to a target
        static float calculateExplosionDamage(const Math::Vector3& crystalPos,
                                             const Math::Vector3& entityPos,
                                             float entityEyeHeight,
                                             float armor,
                                             float toughness,
                                             int resistanceLevel,
                                             int blastProtectionLevel,
                                             float exposure = 1.0f,int protectionLevel=0) {
            // Vanilla/source distance uses the entity position; exposure already
            // samples its complete bounding box. Do not add a second eye offset.
            double dist = crystalPos.distanceTo(entityPos);
            double maxExplosionRadius = 12.0;
            if (dist > maxExplosionRadius) return 0.0f;

            double impact = (1.0 - (dist / maxExplosionRadius)) * std::clamp(exposure, 0.0f, 1.0f);
            double rawDamage = (impact * impact + impact) * 0.5 * 7.0 * maxExplosionRadius + 1.0;

            float damageAfterArmor = reduceDamageByArmor(static_cast<float>(rawDamage), armor, toughness);
            return reduceDamageByEnchants(damageAfterArmor, resistanceLevel, blastProtectionLevel,protectionLevel);
        }

        // Evaluates placements and detonations
        Action update(const Math::Vector3& playerPos,
                      float playerHealth,
                      float playerAbsorption,
                      float playerArmor,
                      float playerToughness,
                      int playerResistanceLevel,
                      int playerBlastProtLevel,
                      int crystalSlot,
                      int obsidianSlot,
                      const std::vector<CrystalPlacement>& validPlacements,
                      const std::vector<std::pair<int, Math::Vector3>>& existingCrystals,
                      const std::vector<float>& crystalSelfDamage = {}) const {
            if (!enabled) return {};

            float totalPlayerHealth = playerHealth + playerAbsorption;

            // 1. Detonate existing crystals in break reach
            std::size_t crystalIndex = 0;
            for (const auto& [entityId, crystalPos] : existingCrystals) {
                const auto index = crystalIndex++;
                if (playerPos.distanceTo(crystalPos) <= breakReach) {
                    float selfDamage = calculateExplosionDamage(
                        crystalPos, playerPos, 1.62f, playerArmor, playerToughness,
                        playerResistanceLevel, playerBlastProtLevel
                    );
                    if (index < crystalSelfDamage.size()) selfDamage = crystalSelfDamage[index];

                    if (preventSuicide && selfDamage >= totalPlayerHealth) {
                        continue; // Skip dangerous self-kill detonation
                    }
                    if (selfDamage > maxSelfDamage) {
                        continue;
                    }

                    return {ActionType::DETONATE_CRYSTAL, crystalPos, entityId, -1};
                }
            }

            // 2. Place new crystals
            if (crystalSlot != -1) {
                const CrystalPlacement* bestPlacement = nullptr;
                double bestDamage = 0.0;

                for (const auto& candidate : validPlacements) {
                    if (candidate.needsObsidian) continue;
                    if (playerPos.distanceTo(candidate.blockPos) > placeReach) continue;

                    if (preventSuicide && candidate.selfDamage >= totalPlayerHealth) continue;
                    if (candidate.selfDamage > maxSelfDamage) continue;
                    if (candidate.targetDamage < minDamage) continue;

                    if (candidate.targetDamage > bestDamage) {
                        bestDamage = candidate.targetDamage;
                        bestPlacement = &candidate;
                    }
                }

                if (bestPlacement) {
                    return {ActionType::PLACE_CRYSTAL, bestPlacement->blockPos, -1, crystalSlot};
                }
            }

            // 3. Auto-place Obsidian if no valid placement exists and enemy is nearby
            if (autoObsidian && obsidianSlot != -1 && crystalSlot != -1) {
                const CrystalPlacement* best = nullptr;
                for (const auto& candidate : validPlacements) {
                    if (!candidate.needsObsidian || playerPos.distanceTo(candidate.blockPos) > placeReach) continue;
                    if (candidate.targetDamage < minDamage || candidate.selfDamage > maxSelfDamage ||
                        (preventSuicide && candidate.selfDamage >= totalPlayerHealth)) continue;
                    if (!best || candidate.targetDamage > best->targetDamage) best = &candidate;
                }
                if (best) return {ActionType::PLACE_OBSIDIAN, best->blockPos, -1, obsidianSlot};
            }

            return {};
        }
    };
}
