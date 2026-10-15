#pragma once

namespace Vape::Combat {

    class Sprint {
    public:
        bool enabled{true};
        bool cancelInvis{true};

        // Called on local player pre-tick
        [[nodiscard]] bool shouldSprint(bool isMovingForward, bool isSneaking, bool isCollidingHorizontally,
                                        bool isInvisible, bool hasBlindness, int foodLevel) const {
            if (!enabled) return false;

            // Invisibility particle avoidance
            if (cancelInvis && isInvisible) return false;

            // Blindness or low hunger
            if (hasBlindness || foodLevel <= 6) return false;

            // Player must be pressing forward and not sneaking or colliding
            return isMovingForward && !isSneaking && !isCollidingHorizontally;
        }
    };
}
