#pragma once
#include "ClickEngine.hpp"

namespace Vape::Combat {

    class LeftClicker {
    public:
        bool enabled{true};
        bool breakBlocksProtection{true};
        bool weaponsOnly{true};

        LeftClicker(double minCps = 9.0, double maxCps = 13.5)
            : m_engine(ClickEngine::Button::LEFT, minCps, maxCps) {}

        [[nodiscard]] bool shouldClick(bool isAttackKeyPressed, bool isLookingAtBlockToBreak,
                                       bool isHoldingWeapon, bool isGuiOpen) const {
            if (!enabled || isGuiOpen || !isAttackKeyPressed) {
                return false;
            }

            if (breakBlocksProtection && isLookingAtBlockToBreak) {
                return false;
            }

            if (weaponsOnly && !isHoldingWeapon) {
                return false;
            }

            return m_engine.canClick();
        }

        void onClickProcessed() {
            m_engine.onClickTriggered();
        }

        void updateJitter(float& playerYaw, float& playerPitch, float mouseSensitivity) {
            m_engine.updateJitter();
            m_engine.applyJitterRotation(playerYaw, playerPitch, mouseSensitivity);
        }

        ClickEngine& getEngine() { return m_engine; }

    private:
        ClickEngine m_engine;
    };
}
