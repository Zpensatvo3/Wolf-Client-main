#include "no_jump_delay.hpp"
#include "no_jump_delay_policy.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/client_context.hpp"
#include "../ui_redirect.hpp"

void modules::no_jump_delay::render_overlay()
{
    const HWND window = gui::get_window();
    const bool focused = window && GetForegroundWindow() == window;
    if (!enabled || !focused || gui::draw || gui::loader_visible || gui::should_unload)
        return;
    if (!platform::jni_context::bind_current_thread())
        return;

    struct clear_on_exit
    {
        ~clear_on_exit() { platform::jni_context::clear_exception(); }
    } clear_exception;
    jni::frame frame{32};
    try
    {
        // Resolve the current session every frame; never write to a player
        // retained from a disconnected world or from the worker's cache.
        services::game::client_context context;
        if (!services::game::capture_client_context(context, true, false) || context.screen)
            return;
        if (!context.player.jumpingCooldown.is_valid() || !context.player.isAlive.is_valid())
            return;

        const bool alive = context.player.isAlive.call() == JNI_TRUE;
        const int cooldown = context.player.jumpingCooldown.get();
        if (platform::jni_context::clear_exception())
            return;
        if (no_jump_delay_policy::should_clear({enabled, focused, false, alive, cooldown}))
            context.player.jumpingCooldown.set(0);
    }
    catch (...)
    {
        platform::jni_context::clear_exception();
    }
}

void modules::no_jump_delay::render_options()
{
    ImGui::TextWrapped("%s", gui::text(
        "Removes the repeat-jump cooldown. Does not press jump for you.",
        "Tekrarlanan ziplama beklemesini kaldirir. Senin yerine ziplama tusuna basmaz."));
}
