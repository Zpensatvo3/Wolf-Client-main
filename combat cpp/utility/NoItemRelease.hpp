#pragma once

namespace Vape::Combat {

    enum class ItemCategory {
        BOW,
        CROSSBOW,
        SHIELD,
        FOOD,
        POTION,
        OTHER
    };

    class NoItemRelease {
    public:
        bool enabled{true};
        bool onlyInGui{true};
        bool bows{true};
        bool shields{true};
        bool foodAndPotions{true};

        // Returns true if RELEASE_USE_ITEM packet should be cancelled
        [[nodiscard]] bool shouldCancelRelease(bool inGui, ItemCategory activeItemCategory) const {
            if (!enabled) return false;
            if (onlyInGui && !inGui) return false;

            if (bows && (activeItemCategory == ItemCategory::BOW || activeItemCategory == ItemCategory::CROSSBOW)) {
                return true;
            }
            if (shields && activeItemCategory == ItemCategory::SHIELD) {
                return true;
            }
            if (foodAndPotions && (activeItemCategory == ItemCategory::FOOD || activeItemCategory == ItemCategory::POTION)) {
                return true;
            }

            return false;
        }
    };
}
