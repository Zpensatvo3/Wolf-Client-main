#pragma once
#include <chrono>

namespace Vape::Utility {

    class Timer {
    public:
        Timer() : m_start(std::chrono::steady_clock::now()) {}

        void reset() {
            m_start = std::chrono::steady_clock::now();
        }

        [[nodiscard]] long long getElapsedMs() const {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::milliseconds>(now - m_start).count();
        }

        [[nodiscard]] bool hasElapsed(long long milliseconds) const {
            return getElapsedMs() >= milliseconds;
        }

    private:
        std::chrono::steady_clock::time_point m_start;
    };
}
