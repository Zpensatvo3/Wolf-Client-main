#include "fast_stop.hpp"
#include "fast_stop_engine.hpp"
#include "../../gui/gui.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/overlay_context.hpp"
#include "../ui_redirect.hpp"

struct modules::fast_stop::runtime
{
    fast_stop_logic::engine engine;
    services::game::overlay_session session;
    void reset() { engine.reset(); session.reset(); }
};

modules::fast_stop::fast_stop() : module("fast stop", "Movement", "fast_stop"), runtime_(std::make_unique<runtime>()) {}
modules::fast_stop::~fast_stop() = default;

void modules::fast_stop::on_disable(::cache&)
{
    reset_requested_.store(true, std::memory_order_release);
}

void modules::fast_stop::render_overlay()
{
    if (!platform::jni_context::bind_current_thread())
    {
        reset_requested_.store(true, std::memory_order_release);
        return;
    }
    struct clear_on_exit
    {
        ~clear_on_exit() { platform::jni_context::clear_exception(); }
    } clear_exception;
    jni::frame frame{64};
    auto& state = *runtime_;
    if (reset_requested_.exchange(false, std::memory_order_acq_rel)) state.reset();
    const HWND window = gui::get_window();
    const bool focused = window && GetForegroundWindow() == window;
    if (!enabled || !focused || gui::draw || gui::loader_visible || gui::should_unload)
    {
        state.reset();
        return;
    }
    try
    {
        services::game::client_context context;
        if (!services::game::capture_client_context(context, true, false) || context.screen)
        { state.reset(); return; }
        if (state.session.observe({context.world, context.player})) state.engine.reset();
        auto& player = context.player;
        if (!player.input.is_valid() || !player.getAbilities.is_valid() || !player.isAlive.is_valid() ||
            !player.isOnGround.is_valid() || !player.isTouchingWater.is_valid() || !player.isInLava.is_valid() ||
            !player.isClimbing.is_valid() || !player.isFallFlying.is_valid() || !player.hasVehicle.is_valid() ||
            !player.hurtTime.is_valid() || !player.getVelocity.is_valid() || !player.setVelocity.is_valid())
        { state.reset(); return; }

        auto input = player.input.get();
        auto abilities = player.getAbilities.call();
        auto options = context.instance.options.get();
        auto motion = player.getVelocity.call();
        if (!input || !abilities || !options || !motion || platform::jni_context::clear_exception() ||
            !input.movementForward.is_valid() || !input.movementSideways.is_valid() || !input.jumping.is_valid() ||
            !abilities.flying.is_valid() || !motion.x.is_valid() || !motion.y.is_valid() || !motion.z.is_valid())
        { state.reset(); return; }

        // Read the game's current bindings, so remapped keys and opposite held
        // directions are respected without hard-coded WASD or synthetic input.
        const maps::KeyBinding directions[]{options.forwardKey.get(), options.backKey.get(),
            options.leftKey.get(), options.rightKey.get()};
        auto jump = options.jumpKey.get();
        bool direction_pressed = false;
        for (const auto& binding : directions)
        {
            if (!binding || !binding.isPressed.is_valid()) { state.reset(); return; }
            const bool pressed = binding.isPressed.call() == JNI_TRUE;
            direction_pressed = direction_pressed || pressed;
        }
        if (!jump || !jump.isPressed.is_valid() || platform::jni_context::clear_exception())
        { state.reset(); return; }

        fast_stop_logic::observation observed;
        observed.active = enabled;
        observed.focused = focused;
        observed.alive = player.isAlive.call() == JNI_TRUE;
        observed.grounded = player.isOnGround.call() == JNI_TRUE;
        observed.in_water = player.isTouchingWater.call() == JNI_TRUE;
        observed.in_lava = player.isInLava.call() == JNI_TRUE;
        observed.climbing = player.isClimbing.call() == JNI_TRUE;
        observed.gliding = player.isFallFlying.call() == JNI_TRUE;
        observed.riding = player.hasVehicle.call() == JNI_TRUE;
        observed.flying = abilities.flying.get() == JNI_TRUE;
        const bool jump_pressed = jump.isPressed.call() == JNI_TRUE;
        observed.jumping = input.jumping.get() == JNI_TRUE || jump_pressed;
        observed.hurt_time = player.hurtTime.get();
        observed.forward = input.movementForward.get();
        observed.sideways = input.movementSideways.get();
        observed.directional_pressed = direction_pressed;
        observed.motion = {motion.x.get(), motion.y.get(), motion.z.get()};
        if (platform::jni_context::clear_exception()) { state.reset(); return; }
        if (const auto stop = state.engine.update(observed))
        {
            player.setVelocity.call(stop->x, stop->y, stop->z);
            if (platform::jni_context::clear_exception()) state.reset();
        }
    }
    catch (...)
    {
        platform::jni_context::clear_exception();
        state.reset();
    }
}

void modules::fast_stop::render_options()
{
    ImGui::TextWrapped("%s", "Stops horizontal drift when you release movement on the ground. Preserves vertical velocity; pauses in air, fluids, vehicles, flight and while hurt. Does not press keys.");
}
