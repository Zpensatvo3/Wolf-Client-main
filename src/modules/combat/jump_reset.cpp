#include "../../ui/settings_renderer.hpp"
#include <Windows.h>
#include "jump_reset.hpp"
#include "../../services/game/client_context.hpp"
#include "../../services/input/input_service.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include <algorithm>
#include <chrono>
#include <optional>
#include <random>
#include <imgui/imgui.h>

namespace
{
    std::int64_t now_ms()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    struct percent_sampler final : jump_reset_engine::sampler
    {
        std::mt19937_64& generator;
        explicit percent_sampler(std::mt19937_64& source) : generator(source) {}
        int percent() override { return std::uniform_int_distribution<int>(1, 100)(generator); }
    };
    enum issue { no_issue = 0, read_error = 1, wrong_space_binding = 2, input_rejected = 3, consume_timeout = 4, release_failed = 5, source_unconfirmed = 6 };
    struct binding_state { bool physical = false, space_binding = false; unsigned virtual_key = 0, scan_code = 0; };

    std::optional<binding_state> read_binding(const maps::KeyBinding& binding)
    {
        if (!binding || !binding.boundKey.is_valid()) return std::nullopt;
        auto key = binding.boundKey.get();
        if (platform::jni_context::clear_exception() || !key || !key.getCode.is_valid() || !key.getCategory.is_valid()) return std::nullopt;
        const int code = key.getCode.call();
        auto category = key.getCategory.call();
        auto symbols = maps::InputKeyType{}.KEYSYM.get();
        auto scans = maps::InputKeyType{}.SCANCODE.get();
        auto mouse = maps::InputKeyType{}.MOUSE.get();
        if (platform::jni_context::clear_exception() || !category || !symbols || !scans || !mouse) return std::nullopt;
        binding_state result;
        if (category.is_same_object(symbols)) {
            const unsigned vk = services::input::windows_key_for_glfw(code);
            if (!vk) return std::nullopt;
            result.virtual_key = vk;
            result.scan_code = code == 257 ? 0x1c : code == 335 ? 0x11c : 0;
            // GLFW distinguishes main Enter and keypad Enter by scancode.
            result.physical = code == 257 ? services::input::physical_scan_down(0x1c) :
                code == 335 ? services::input::physical_scan_down(0x11c) : services::input::physical_key_down(vk);
            result.space_binding = code == 32;
        } else if (category.is_same_object(scans)) {
            if (code <= 0 || code >= 512) return std::nullopt;
            result.physical = services::input::physical_scan_down(static_cast<unsigned>(code));
            result.scan_code = static_cast<unsigned>(code);
            result.space_binding = code == 0x39;
        } else if (category.is_same_object(mouse)) {
            const unsigned buttons[]{VK_LBUTTON, VK_RBUTTON, VK_MBUTTON, VK_XBUTTON1, VK_XBUTTON2};
            if (code < 0 || code >= 5) return std::nullopt;
            result.physical = services::input::physical_key_down(buttons[code]);
            result.virtual_key = buttons[code];
        } else return std::nullopt;
        return platform::jni_context::clear_exception() ? std::nullopt : std::optional<binding_state>{result};
    }

    bool playable(const services::game::client_context& context)
    {
        if (context.screen) return false;
        const bool alive = context.player.isAlive.call() == JNI_TRUE;
        const bool using_item = context.player.isUsingItem.call() == JNI_TRUE;
        const bool sleeping = context.player.isSleeping.call() == JNI_TRUE;
        const bool water = context.player.isTouchingWater.call() == JNI_TRUE;
        const bool swimming = context.player.isSwimming.call() == JNI_TRUE;
        const bool flying = context.player.isFallFlying.call() == JNI_TRUE;
        const bool climbing = context.player.isClimbing.call() == JNI_TRUE;
        const bool riding = context.player.hasVehicle.call() == JNI_TRUE;
        auto abilities = context.player.getAbilities.call();
        if (platform::jni_context::clear_exception() || !abilities || !abilities.allowFlying.is_valid() || !abilities.flying.is_valid()) return false;
        const bool flight_enabled = abilities.allowFlying.get() == JNI_TRUE || abilities.flying.get() == JNI_TRUE;
        return !platform::jni_context::clear_exception() && alive && !using_item && !sleeping &&
            !water && !swimming && !flying && !climbing && !riding && !flight_enabled;
    }
}

void modules::jump_reset::run(::cache&)
{
    // No JNI/game-state/input mutation from the module worker.
}

void modules::jump_reset::request_reset()
{
    {
        std::lock_guard lock(settings_mutex);
        reset_generation.fetch_add(1, std::memory_order_release);
    }
    // Tombstone queued Space before the next client frame. This is native input
    // cleanup only; game binding/runtime cleanup remains on the client thread.
    const auto id = space_ticket.load(std::memory_order_acquire);
    if (id) services::input::release_jump_space(id);
}
void modules::jump_reset::on_enable(::cache&) { request_reset(); }
void modules::jump_reset::on_disable(::cache&) { request_reset(); }

bool modules::jump_reset::release_input()
{
    bool success = true;
    const auto id = space_ticket.load(std::memory_order_acquire);
    if (id) {
        if (services::input::release_jump_space(id)) {
            space_ticket.store(0, std::memory_order_release);
            synthetic_space_down.store(false, std::memory_order_release);
        } else { success = false; last_issue.store(release_failed); }
    }
    if (owned_jump_key) {
        // Restore only the binding we own, and only while its assignment is
        // unchanged. A physical press during our hold must remain pressed.
        auto current = owned_jump_key.boundKey.get();
        const bool error = platform::jni_context::clear_exception();
        if (!error && current && current.is_same_object(owned_bound_key)) {
            const bool held = owned_scan_code ? services::input::physical_scan_down(owned_scan_code) :
                services::input::physical_key_down(owned_virtual_key);
            if (owned_jump_key.setPressed.is_valid()) owned_jump_key.setPressed.call(held ? JNI_TRUE : JNI_FALSE);
        }
        if (platform::jni_context::clear_exception() || error) { last_issue.store(read_error); success = false; }
        else {
            owned_jump_key.clear_ref(); owned_bound_key.clear_ref();
            owned_virtual_key = owned_scan_code = 0;
        }
    }
    return success;
}

void modules::jump_reset::cancel_on_client_thread()
{
    if (!platform::jni_context::bind_current_thread()) { request_reset(); return; }
    struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } clear_exception;
    jni::frame frame{48};
    release_input(); jump_reset_engine::invalidate(engine);
    world.clear_ref(); player.clear_ref(); previous_damage_source.clear_ref();
    source_revision = consumed_source_revision = 0; source_observed_at = 0; source_is_living = false;
    last_damage_at = -1; awaiting_damage_source = false;
}

bool modules::jump_reset::validate_space_delivery(std::uint64_t generation, std::uint64_t revision,
    std::uint64_t expected_session, std::uint64_t damage_id, std::int64_t expires_at)
{
    if (!enabled || gui::draw || gui::loader_visible || gui::should_unload ||
        reset_generation.load(std::memory_order_acquire) != generation ||
        settings_revision.load(std::memory_order_acquire) != revision || session != expected_session ||
        engine.damage_id != damage_id || now_ms() >= expires_at) return false;
    if (!platform::jni_context::bind_current_thread()) return false;
    struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } clear_exception;
    jni::frame frame{96};
    services::game::client_context context;
    if (!services::game::capture_client_context(context, true, false) || !playable(context) ||
        !world || !player || !world.is_same_object(context.world) || !player.is_same_object(context.player) ||
        platform::jni_context::clear_exception()) return false;
    const bool grounded = context.player.isOnGround.call() == JNI_TRUE;
    if (!context.player.jumpingCooldown.is_valid()) return false;
    const bool jump_ready = context.player.jumpingCooldown.get() == 0;
    auto options = context.instance.options.get();
    if (platform::jni_context::clear_exception() || !options) return false;
    auto key = options.jumpKey.get();
    const auto binding = read_binding(key);
    return !platform::jni_context::clear_exception() && grounded && jump_ready && binding &&
        binding->space_binding && !binding->physical && !services::input::physical_key_down(VK_SPACE);
}

void modules::jump_reset::render_overlay()
{
    if (!platform::jni_context::bind_current_thread()) { request_reset(); return; }
    struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } clear_exception;
    jni::frame frame{160};
    try {
        const auto generation = reset_generation.load(std::memory_order_acquire);
        if (seen_reset != generation || !enabled) {
            cancel_on_client_thread(); seen_reset = generation;
            if (!enabled) return;
        }
        // A rejected native key-up is retried as cleanup, never as another jump.
        if ((space_ticket.load() || owned_jump_key) && !engine.holding) {
            if (!release_input()) return;
        }
        const HWND window = gui::get_window();
        if (!window || GetForegroundWindow() != window || gui::draw || gui::loader_visible || gui::should_unload) {
            cancel_on_client_thread(); return;
        }
        jump_reset_config::config current_settings;
        std::uint64_t revision;
        {
            std::lock_guard lock(settings_mutex);
            current_settings = settings; revision = settings_revision.load(std::memory_order_acquire);
        }
        if (!jump_reset_config::normalize(current_settings)) { cancel_on_client_thread(); return; }
        services::game::client_context context;
        if (!services::game::capture_client_context(context, true, false) || !playable(context)) {
            cancel_on_client_thread(); return;
        }
        const bool same_session = world && player && world.is_same_object(context.world) && player.is_same_object(context.player);
        if (platform::jni_context::clear_exception()) { last_issue.store(read_error); cancel_on_client_thread(); return; }
        if (!same_session) {
            cancel_on_client_thread(); world = context.world; player = context.player; ++session;
        }
        auto options = context.instance.options.get();
        if (platform::jni_context::clear_exception() || !options) { last_issue.store(read_error); cancel_on_client_thread(); return; }
        auto key = options.jumpKey.get();
        const auto binding = read_binding(key);
        if (!binding || platform::jni_context::clear_exception()) { last_issue.store(read_error); cancel_on_client_thread(); return; }
        if (current_settings.mode == 1 && !binding->space_binding) {
            last_issue.store(wrong_space_binding); cancel_on_client_thread(); return;
        }
        jump_reset_engine::observation observed;
        if (!context.player.hurtTime.is_valid() || !context.player.age.is_valid() ||
            !context.player.getRecentDamageSource.is_valid() || !key.isPressed.is_valid() || !context.player.jumpingCooldown.is_valid()) {
            last_issue.store(read_error); cancel_on_client_thread(); return;
        }
        observed.eligible = true; observed.session = session; observed.revision = revision;
        observed.on_ground = context.player.isOnGround.call() == JNI_TRUE;
        observed.jump_ready = context.player.jumpingCooldown.get() == 0;
        observed.hurt = context.player.hurtTime.get(); observed.age = context.player.age.get();
        observed.physical_jump = binding->physical;
        auto source = context.player.getRecentDamageSource.call();
        if (platform::jni_context::clear_exception()) { last_issue.store(read_error); cancel_on_client_thread(); return; }
        // Pair one fresh source with one hurt edge. The source packet can arrive
        // shortly BEFORE or AFTER the animation; never reuse it for later hits.
        const auto now = now_ms();
        const bool changed_source = source && (!previous_damage_source || !source.is_same_object(previous_damage_source));
        if (changed_source) {
            auto attacker = source.getAttacker.call();
            source_is_living = attacker && !attacker.is_same_object(context.player) && attacker.is_instance_of<maps::LivingEntity>();
            previous_damage_source = source; ++source_revision; source_observed_at = now;
        }
        observed.living_damage = source && source_is_living && source_revision != consumed_source_revision &&
            now - source_observed_at <= 105;
        if (platform::jni_context::clear_exception()) { last_issue.store(read_error); cancel_on_client_thread(); return; }
        const auto id = space_ticket.load();
        if (engine.holding) {
            if (id) {
                const auto receipt = services::input::jump_space_receipt(id);
                observed.delivered = receipt.delivered; observed.input_cancelled = receipt.cancelled;
                if (receipt.cancelled) last_issue.store(input_rejected);
            } else observed.delivered = static_cast<bool>(owned_jump_key);
        }
        // Do not adopt another module's or the user's held jump binding.
        if (!engine.holding && !owned_jump_key) {
            const bool pressed = key.isPressed.call() == JNI_TRUE;
            if (platform::jni_context::clear_exception()) { last_issue.store(read_error); cancel_on_client_thread(); return; }
            observed.physical_jump = observed.physical_jump || pressed;
        }
        percent_sampler random{gen};
        const bool seeded = engine.seeded;
        const bool was_confirming = engine.pending && !engine.confirmed;
        const bool fresh_damage = seeded && observed.hurt > 0 && observed.hurt > engine.previous_hurt;
        if (fresh_damage) { last_damage_at = now; awaiting_damage_source = !observed.living_damage; }
        const auto decision = jump_reset_engine::step(current_settings, engine, observed, now, random);
        if (was_confirming && !engine.pending && !engine.confirmed) last_issue.store(source_unconfirmed);
        if (!seeded || (observed.living_damage && (fresh_damage || (engine.pending && engine.confirmed) ||
            (awaiting_damage_source && last_damage_at >= 0 && now - last_damage_at <= 105)))) {
            consumed_source_revision = source_revision;
            awaiting_damage_source = false;
        }
        if (decision == jump_reset_engine::action::release) {
            if (engine.timed_out) last_issue.store(consume_timeout);
            release_input(); return;
        }
        if (decision != jump_reset_engine::action::press) return;

        // Refresh physical and grounded conditions immediately before sending.
        const auto confirmed_binding = read_binding(key);
        const bool grounded = context.player.isOnGround.call() == JNI_TRUE;
        const bool jump_ready = context.player.jumpingCooldown.get() == 0;
        const int age = context.player.age.get();
        if (!confirmed_binding || platform::jni_context::clear_exception() || !grounded || !jump_ready ||
            confirmed_binding->physical || key.isPressed.call() == JNI_TRUE || platform::jni_context::clear_exception()) {
            engine.pending = false; return;
        }
        bool sent = false;
        {
            std::lock_guard dispatch_lock(settings_mutex);
            if (settings_revision.load(std::memory_order_acquire) != revision ||
                reset_generation.load(std::memory_order_acquire) != generation) {
                jump_reset_engine::invalidate(engine); return;
            }
            owned_jump_key = key; owned_bound_key = key.boundKey.get();
            owned_virtual_key = confirmed_binding->virtual_key; owned_scan_code = confirmed_binding->scan_code;
            if (platform::jni_context::clear_exception() || !owned_bound_key) {
                sent = false;
            } else if (current_settings.mode == 0) {
                if (key.setPressed.is_valid()) {
                    if (!platform::jni_context::clear_exception() && owned_bound_key) {
                        key.setPressed.call(JNI_TRUE);
                        sent = !platform::jni_context::clear_exception();
                    }
                }
            } else {
                synthetic_space_down.store(true, std::memory_order_release);
                const auto damage_id = engine.damage_id;
                const auto expiry = engine.expires_at;
                const auto ticket = services::input::press_jump_space(window,
                    [this, generation, revision, expected_session = session, damage_id, expiry] {
                        return validate_space_delivery(generation, revision, expected_session, damage_id, expiry);
                    });
                space_ticket.store(ticket, std::memory_order_release);
                sent = ticket != 0;
                if (!sent) synthetic_space_down.store(false, std::memory_order_release);
            }
            if (sent) jump_reset_engine::submitted(engine, age, now_ms(), current_settings.mode == 0);
            else jump_reset_engine::submission_failed(engine, now_ms());
        }
        if (!sent) { last_issue.store(input_rejected); release_input(); }
        else last_issue.store(no_issue);
    } catch (...) {
        platform::jni_context::clear_exception(); last_issue.store(read_error); cancel_on_client_thread();
    }
}

jump_reset_config::config modules::jump_reset::settings_snapshot()
{
    std::lock_guard lock(settings_mutex); return settings;
}
void modules::jump_reset::apply_settings(jump_reset_config::config candidate, bool force_reset)
{
    if (!jump_reset_config::normalize(candidate)) return;
    bool changed = false;
    {
        std::lock_guard lock(settings_mutex);
        if (force_reset || !jump_reset_config::same_config(candidate, settings)) {
            settings = candidate; settings_revision.fetch_add(1, std::memory_order_release);
            reset_generation.fetch_add(1, std::memory_order_release); changed = true;
        }
    }
    const auto id = space_ticket.load(std::memory_order_acquire);
    if (changed && id) services::input::release_jump_space(id);
}

void modules::jump_reset::render_options()
{
    ImGui::PushID("jump_reset_settings");
    auto candidate = settings_snapshot();
    gui::option_section(gui::text("Timing", "Zamanlama"));
    const char* profile_names[] = {
        gui::text("Custom", "Özel"),
        gui::text("Immediate", "Anlık"),
        gui::text("Balanced", "Dengeli"),
        gui::text("Controlled", "Kontrollü")
    };
    int selected = static_cast<int>(jump_reset_config::selected_profile(candidate));
    const float width = ImGui::GetContentRegionAvail().x;
    const float spacing = 6.f;
    const float button_width = (width - 2.f * spacing) / 3.f;
    bool presets_fit = width >= 280.f;
    for (int index = 1; index <= 3; ++index)
        presets_fit = presets_fit && ImGui::CalcTextSize(profile_names[index]).x + 16.f <= button_width;
    if (presets_fit)
    {
        for (int index = 1; index <= 3; ++index)
        {
            ImGui::PushID(index);
            if (gui::action_button(profile_names[index], ImVec2(button_width, 28.f),
                selected == index ? gui::action_tone::accent : gui::action_tone::neutral))
            {
                jump_reset_config::apply_profile(candidate, static_cast<jump_reset_config::profile>(index));
                selected = index;
            }
            if (ImGui::IsItemHovered())
            {
                ImGui::BeginTooltip();
                const int chances[] = {0, 100, 85, 65}, delays[] = {0, 0, 15, 35};
                ImGui::Text(gui::text("%d%% activation / %d ms delay", "%%%d uygulama / %d ms gecikme"),
                    chances[index], delays[index]);
                ImGui::EndTooltip();
            }
            ImGui::PopID();
            if (index < 3) ImGui::SameLine(0, spacing);
        }
    }
    else if (gui::option_combo(gui::text("Preset", "Hazır ayar"), &selected, profile_names, 4))
        jump_reset_config::apply_profile(candidate, static_cast<jump_reset_config::profile>(selected));

    gui::settings::render(candidate, jump_reset_config::schema().subspan(1, 2));
    gui::option_section(gui::text("Input / conditions", "Giriş / koşullar"));
    gui::settings::render(candidate, jump_reset_config::schema().first(1));
    gui::settings::render(candidate, jump_reset_config::schema().last(1));

    ImGui::Spacing();
    if (gui::action_button(gui::text("Restore Defaults", "Varsayılana dön"),
        ImVec2(ImGui::GetContentRegionAvail().x, 28.f)))
        candidate = jump_reset_config::config{};
    if (jump_reset_config::normalize(candidate)) apply_settings(candidate);
    ImGui::PopID();
}
