#pragma once
#include "../math/Vector3.hpp"
#include "../math/RotationUtil.hpp"
#include <cmath>

namespace Vape::Combat {

    struct TargetEntityInfo {
        int id{0};
        Math::Vector3 position{};
        float health{20.0f};
        int armorValue{0};
        int equipmentTier{0};
        bool isBlocking{false};
        bool isDead{false};
        bool isFriend{false};
    };

    /**
     * Exact C++20 port of Vape V4.21's Entity Comparators:
     * - EntityAngleComparator
     * - EntityDistanceComparator
     * - EntityHealthComparator
     * - EntityArmorValueComparator
     * - EntityEquipmentValueComparator
     */
    struct EntityAngleComparator {
        Math::Vector3 eyePos;
        float playerYaw;

        bool operator()(const TargetEntityInfo& a, const TargetEntityInfo& b) const {
            float fovA = Math::RotationUtil::getAngleDifference(
                playerYaw,
                Math::RotationUtil::getRotationsToCoords(eyePos, a.position).yaw
            );
            float fovB = Math::RotationUtil::getAngleDifference(
                playerYaw,
                Math::RotationUtil::getRotationsToCoords(eyePos, b.position).yaw
            );
            return fovA < fovB;
        }
    };

    struct EntityDistanceComparator {
        Math::Vector3 eyePos;

        bool operator()(const TargetEntityInfo& a, const TargetEntityInfo& b) const {
            return eyePos.distanceTo(a.position) < eyePos.distanceTo(b.position);
        }
    };

    struct EntityHealthComparator {
        bool operator()(const TargetEntityInfo& a, const TargetEntityInfo& b) const {
            return a.health < b.health;
        }
    };

    struct EntityArmorValueComparator {
        bool operator()(const TargetEntityInfo& a, const TargetEntityInfo& b) const {
            return a.armorValue < b.armorValue;
        }
    };

    struct EntityEquipmentValueComparator {
        bool operator()(const TargetEntityInfo& a, const TargetEntityInfo& b) const {
            return a.equipmentTier > b.equipmentTier; // Highest threat first
        }
    };
}
