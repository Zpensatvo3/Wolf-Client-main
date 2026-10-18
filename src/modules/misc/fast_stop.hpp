#pragma once
#include "../module_support.hpp"
#include <memory>

namespace modules
{
    class fast_stop final : public module
    {
    public:
        fast_stop();
        ~fast_stop() override;
        void run(::cache&) override {} // No movement writes on the worker.
        void render_overlay() override;
        void render_options() override;
        void on_disable(::cache&) override;
        bool render_when_disabled() const override { return true; }
        float get_options_height() override { return 120.0f; }
        bool has_custom_options() override { return false; }
    private:
        struct runtime;
        std::unique_ptr<runtime> runtime_;
        std::atomic_bool reset_requested_{true};
    };
}
