#pragma once

#include "../../utils/utils.hpp"
#include <cstdint>
#include <random>
#include <string>
#include "config_fields.hpp"

class cache;

namespace modules
{
    inline constexpr std::uintptr_t synthetic_mouse_tag = 0x574F4C46434C4943ULL;

    // Shared module contract. Gameplay modules only implement behaviour; the
    // registry, UI and config layers use this stable surface.
    class module
    {
    public:
        module(const char* name = "no name", const char* category = "Combat", const char* id = nullptr);
        virtual ~module() = default;

        virtual void run(::cache& cache);
        virtual void render_options();
        virtual void render_overlay() {};
        virtual bool render_when_disabled() const { return false; }
        virtual void on_enable(::cache& cache);
        virtual void on_disable(::cache& cache);

        std::string export_config_settings() const;
        bool validate_config_settings(const std::string& object) const;
        bool apply_config_settings(const std::string& object);
        virtual void visit_config(config_visitor&) {}

        int get_keybind() const;
        void set_keybind(int value);
        const char* get_id() const;
        const char* get_name() const;
        const char* get_category() const;
        void key_bind_selector();
        void begin_keybind_capture();

        bool display_options = false;
        float display_options_progress = 0.0f;
        bool enabled = false;
        bool prev_enabled = false;
        virtual float get_options_height() { return 45.0f; }

    protected:
        int keybind = 0;
        std::random_device rd{};
        std::mt19937_64 gen{ rd() };

    private:
        utils::null_string keycode_to_string(int keycode) const;
        static std::string make_stable_id(const char* name);

        bool scanning = false;
        std::string id_storage;
        const char* name = "no name";
        const char* category = "Combat";
    };
}
