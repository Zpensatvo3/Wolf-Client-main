#pragma once
#include "ClickEngine.hpp"

namespace Vape::Combat {

    class RightClicker {
    public:
        bool enabled{true};
        bool holdToClick{true};
        bool blocksOnly{true};
        bool allowOffhand{true};

        RightClicker(double minCps = 12.0, double maxCps = 18.0)
            : m_engine(ClickEngine::Button::RIGHT, minCps, maxCps) {}

        [[nodiscard]] bool shouldClick(bool isUseKeyPressed, bool hasUsableMainhand,
                                       bool hasUsableOffhand, bool isGuiOpen) const {
            if (!enabled || isGuiOpen) return false;

            if (holdToClick && !isUseKeyPressed) return false;

            bool canUse = hasUsableMainhand || (allowOffhand && hasUsableOffhand);
            if (!canUse) return false;

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
