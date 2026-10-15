#pragma once

namespace gui::sound
{
    enum class type
    {
        click,
        toggle_on,
        toggle_off,
        tab_switch,
        open_gui,
        close_gui,
        slider_tick
    };

    void init();
    void shutdown();
    void play(type sound);
    void play_slider_tick();
    void set_volume(float volume);
    void set_enabled(bool enabled);
    bool is_enabled();
    float get_volume();
}
