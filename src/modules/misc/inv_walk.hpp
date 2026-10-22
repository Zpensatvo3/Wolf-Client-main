#pragma once
#include "../module_support.hpp"
#include "../core/settings_store.hpp"
#include "settings/inv_walk_settings.hpp"
#include "inv_walk_engine.hpp"
#include "../../mappings.hpp"
#include <array>

namespace modules
{
    class inv_walk final : public module
    {
    public:
        inv_walk() : module("inv walk", "Movement", "inv_walk") {}
        void run(::cache&) override {} // No game-state writes on the worker.
        void render_overlay() override;
        bool render_when_disabled() const override { return true; }
        void on_enable(::cache&) override { reset_requested.store(true); }
        void on_disable(::cache&) override { reset_requested.store(true); }
        void cancel_on_client_thread();
        bool gate_inventory_input(UINT message, WPARAM key, LPARAM data);
        void on_render_shutdown() override { cancel_on_client_thread(); }
        void render_options() override;
        float get_options_height() override { return 200.f; }
        void visit_config(config_visitor& v) override { settings_.visit(v, module_settings::inv_walk::schema()); }
    private:
        struct owned_key
        {
            maps::KeyBinding key{nullptr, true};
            maps::InputKey binding{nullptr, true};
        };
        modules::settings::store<module_settings::inv_walk> settings_;
        std::array<owned_key, inv_walk_engine::key_count> owned_;
        maps::ClientWorld world_{nullptr, true};
        maps::ClientPlayerEntity player_{nullptr, true};
        std::atomic_bool reset_requested{false};
        std::mutex runtime_mutex;
        bool release_key(owned_key& key, bool vanilla);
        bool release_owned(bool vanilla);
        void update_key(unsigned index, const maps::KeyBinding& key, bool desired);
    };
}
