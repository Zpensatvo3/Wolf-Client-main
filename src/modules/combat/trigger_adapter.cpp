#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "trigger_adapter.hpp"
#include "mace_swap.hpp"
#include "shield_breaker.hpp"
#include "../../gui/gui.hpp"
#include "../../friends/friends.hpp"
#include "../../services/game/attack_service.hpp"
#include "../../services/game/attack_coordinator.hpp"
#include "../../services/game/client_context.hpp"
#include "../../services/game/target_service.hpp"
#include "../../services/game/hotbar_service.hpp"
#include "../../services/game/inventory_service.hpp"
#include "../../services/input/input_service.hpp"
#include "../../platform/jni/jni_context.hpp"
#include <chrono>
#include <cmath>
#include <string>
#include <optional>

namespace
{
    using namespace trigger_engine;
    std::int64_t now_ms()
    {
        return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    }
    std::optional<weapon_type> detect_weapon(const maps::ClientPlayerEntity& player)
    {
        auto stack = player.getMainHandStack.call();
        const bool error = platform::jni_context::clear_exception();
        if (error || !stack) return std::nullopt;
        const bool empty = stack.isEmpty.call() == JNI_TRUE;
        if (platform::jni_context::clear_exception()) return std::nullopt;
        if (empty) return weapon_type::empty;
        auto item = stack.item.get();
        if (platform::jni_context::clear_exception() || !item) return std::nullopt;
        const bool sword = item.is_instance_of<maps::SwordItem>();
        if (platform::jni_context::clear_exception()) return std::nullopt;
        if (sword) return weapon_type::sword;
        const bool axe = item.is_instance_of<maps::AxeItem>();
        if (platform::jni_context::clear_exception()) return std::nullopt;
        if (axe) return weapon_type::axe;
        const bool mace = item.is_instance_of<maps::MaceItem>();
        if (platform::jni_context::clear_exception()) return std::nullopt;
        if (mace) return weapon_type::mace;
        auto translation = item.getTranslationKey.call();
        if (platform::jni_context::clear_exception() || !translation) return std::nullopt;
        auto key = translation.to_string();
        if (platform::jni_context::clear_exception()) return std::nullopt;
        if (key.ends_with("_sword") || key.ends_with(".sword")) return weapon_type::sword;
        if (key.ends_with("_axe") || key.ends_with(".axe")) return weapon_type::axe;
        if (key.ends_with("_mace") || key.ends_with(".mace")) return weapon_type::mace;
        return weapon_type::other;
    }
    bool target_allowed(const maps::Entity& target, target_mode mode)
    {
        if (!target) return false;
        const bool living = target.is_instance_of<maps::LivingEntity>();
        if (platform::jni_context::clear_exception() || !living) return false;
        const bool alive = target.isAlive.call() == JNI_TRUE;
        if (platform::jni_context::clear_exception() || !alive) return false;
        const bool invisible = target.isInvisible.call() == JNI_TRUE;
        if (platform::jni_context::clear_exception() || invisible) return false;
        const bool player = target.is_instance_of<maps::PlayerEntity>();
        const bool animal = target.is_instance_of<maps::AnimalEntity>();
        const bool mob = target.is_instance_of<maps::MobEntity>();
        if (platform::jni_context::clear_exception()) return false;
        if (player && friends::is_friend(target)) return false;
        return mode == target_mode::players ? player : mode == target_mode::players_and_mobs
            ? player || (mob && !animal) : player || mob || animal;
    }
    std::optional<bool> shield_faces_player(const maps::ClientPlayerEntity& player, const maps::Entity& target)
    {
        const bool is_player = target.is_instance_of<maps::PlayerEntity>();
        if (platform::jni_context::clear_exception()) return std::nullopt;
        if (!is_player) return false;
        maps::PlayerEntity other(target.object_instance, false);
        const bool blocking = other.isBlocking.call() == JNI_TRUE;
        if (platform::jni_context::clear_exception()) return std::nullopt;
        if (!blocking) return false;
        const auto a = player.get_position(), b = target.get_position();
        const double yaw = target.yaw.get();
        if (platform::jni_context::clear_exception() || !std::isfinite(yaw) || !std::isfinite(a.x) || !std::isfinite(a.z) || !std::isfinite(b.x) || !std::isfinite(b.z)) return std::nullopt;
        const double facing = std::atan2(-(a.x - b.x), a.z - b.z) * (180. / 3.14159265358979323846);
        return std::abs(std::remainder(yaw - facing, 360.)) < 90.;
    }
    bool read_critical_conditions(const maps::ClientPlayerEntity& player, observation& frame)
    {
        frame.on_ground = player.isOnGround.call() == JNI_TRUE;
        const float falling = player.fallDistance.get();
        const bool water = player.isTouchingWater.call() == JNI_TRUE;
        const bool swimming = player.isSwimming.call() == JNI_TRUE;
        const bool flying = player.isFallFlying.call() == JNI_TRUE;
        const bool climbing = player.isClimbing.call() == JNI_TRUE;
        const bool riding = player.hasVehicle.call() == JNI_TRUE;
        const bool sprinting = player.isSprinting.call() == JNI_TRUE;
        auto blindness = maps::StatusEffects{}.BLINDNESS.get();
        const bool blind = blindness && player.hasStatusEffectEntry.call(blindness) == JNI_TRUE;
        if (platform::jni_context::clear_exception() || !blindness || !std::isfinite(falling)) return false;
        frame.critical_possible = !water && !swimming && !flying && !climbing && !riding && !sprinting && !blind;
        frame.critical_valid = frame.critical_possible && !frame.on_ground && falling > 0;
        return true;
    }
    bool validate_delivery(const config& settings, const adapter_runtime& state, const observation& sent,
        int expected_slot, weapon_type expected_weapon, std::uint64_t generation, float threshold, std::int64_t deadline)
    {
        // Read-only client-thread validation. The ticket stores values and this
        // callback, never a local JNI target reference. Runtime/backoff is still
        // changed exclusively by the overlay's normal cancellation path.
        if (now_ms() >= deadline || state.reset_generation.load(std::memory_order_acquire) != generation ||
            state.session != sent.session || state.settings_revision != sent.settings_revision ||
            state.weapon_revision != sent.weapon_revision || state.engine.target_id != sent.target_id ||
            services::input::manual_attack_generation() != sent.manual_generation) return false;
        const HWND window = gui::get_window();
        if (!window || GetForegroundWindow() != window || gui::draw || gui::loader_visible ||
            (settings.on_click && !services::input::physical_button_down(services::input::mouse_button::left))) return false;
        if (!platform::jni_context::bind_current_thread()) return false;
        struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } clear_exception;
        jni::frame local_frame{192};
        try {
            services::game::client_context context;
            if (!services::game::capture_client_context(context, true, true) || context.screen ||
                !state.world || !state.player || !state.world.is_same_object(context.world) ||
                !state.player.is_same_object(context.player) || platform::jni_context::clear_exception()) return false;
            const bool alive = context.player.isAlive.call() == JNI_TRUE;
            const bool using_item = context.player.isUsingItem.call() == JNI_TRUE;
            if (platform::jni_context::clear_exception() || !alive || using_item) return false;
            const int slot = services::hotbar::selected_slot(context.player.getInventory.call());
            if (platform::jni_context::clear_exception() || slot != expected_slot) return false;
            const auto weapon = detect_weapon(context.player);
            if (!weapon || *weapon != expected_weapon) return false;
            auto cached = services::target::crosshair_entity(context.instance);
            if (!target_allowed(cached, settings.targets) || platform::jni_context::clear_exception()) return false;
            const int id = cached.getId.call();
            if (platform::jni_context::clear_exception() || id != sent.target_id) return false;
            auto fresh = services::target::fresh_crosshair_entity(context.instance, context.player);
            if (!fresh || !fresh.is_same_object(cached) || platform::jni_context::clear_exception()) return false;
            float cooldown = 0;
            if (!services::attack::read_cooldown(context.player, cooldown) || cooldown < threshold) return false;
            const int ticks = context.player.lastAttackedTicks.get();
            if (platform::jni_context::clear_exception() || ticks < sent.attack_ticks) return false;
            if (settings.criticals == critical_mode::only) {
                observation current;
                if (!read_critical_conditions(context.player, current) || !current.critical_valid) return false;
            }
            const auto shield = shield_faces_player(context.player, fresh);
            if (!shield || (*shield && *weapon != weapon_type::axe)) return false;
            return state.reset_generation.load(std::memory_order_acquire) == generation;
        } catch (...) {
            platform::jni_context::clear_exception(); return false;
        }
    }
    void finish_preparation(adapter_runtime& state, const services::game::client_context& context, bool consumed)
    {
        const bool had_preparation = state.prepared != preparation::none;
        const bool same_session = context.player && context.world && state.world && state.player &&
            state.world.is_same_object(context.world) && state.player.is_same_object(context.player);
        const maps::ClientPlayerEntity player = same_session ? context.player : maps::ClientPlayerEntity{};
        const maps::ClientPlayerInteractionManager manager = same_session ? context.interaction : maps::ClientPlayerInteractionManager{};
        if (state.prepared == preparation::shield)
            modules::shield_breaker::finish_trigger_preparation(player, manager, consumed);
        if (state.prepared == preparation::mace)
            modules::mace_swap::finish_trigger_preparation(player, manager, consumed);
        if (had_preparation && consumed && same_session) {
            state.return_slot = state.original_slot; state.return_from_slot = state.prepared_slot;
        }
        if (had_preparation && !consumed) state.previous_slot = -1;
        state.prepared = preparation::none; state.prepared_slot = -1;
        state.original_slot = -1;
    }
    void stop(adapter_runtime& state, const services::game::client_context& context)
    {
        platform::jni_context::clear_exception();
        services::input::cancel_trigger_click();
        finish_preparation(state, context, false);
        services::combat::release_trigger();
        invalidate(state.engine, now_ms());
    }
    bool capture(const config& settings, adapter_runtime& state, services::game::client_context& context,
        observation& frame, maps::Entity& target, weapon_type& weapon, int& slot)
    {
        const HWND window = gui::get_window();
        if (!window || GetForegroundWindow() != window || gui::draw || gui::loader_visible) return false;
        if (!services::game::capture_client_context(context, true, true)) return false;
        if (context.screen) return false;
        const bool alive = context.player.isAlive.call() == JNI_TRUE;
        const bool using_item = context.player.isUsingItem.call() == JNI_TRUE;
        if (platform::jni_context::clear_exception() || !alive || using_item) return false;
        if (!state.world || !state.player || !state.world.is_same_object(context.world) || !state.player.is_same_object(context.player)) {
            finish_preparation(state, context, false);
            state.world = context.world; state.player = context.player;
            ++state.session; state.previous_slot = -1; state.return_slot = state.return_from_slot = -1;
        }
        if (platform::jni_context::clear_exception()) return false;
        frame.physical_left = services::input::physical_button_down(services::input::mouse_button::left);
        if (settings.on_click && !frame.physical_left) return false;
        target = services::target::crosshair_entity(context.instance);
        if (!target_allowed(target, settings.targets) || platform::jni_context::clear_exception()) return false;
        if (target.is_same_object(context.player) || platform::jni_context::clear_exception()) return false;
        frame.target_id = target.getId.call();
        if (platform::jni_context::clear_exception()) return false;
        const auto detected = detect_weapon(context.player);
        if (!detected) return false;
        weapon = *detected;
        slot = services::hotbar::selected_slot(context.player.getInventory.call());
        if (platform::jni_context::clear_exception() || !services::hotbar::valid_slot(slot)) return false;
        if (state.prepared != preparation::none) {
            const int owned = state.prepared == preparation::shield ? modules::shield_breaker::trigger_prepared_slot()
                : modules::mace_swap::trigger_prepared_slot();
            if (slot != state.prepared_slot || owned != slot) { stop(state, context); ++state.weapon_revision; }
        }
        const bool authorized_swap = state.prepared != preparation::none && slot == state.prepared_slot;
        const bool authorized_return = slot == state.return_slot && state.previous_slot == state.return_from_slot;
        if (!authorized_swap && !authorized_return && (slot != state.previous_slot || weapon != state.previous_weapon)) ++state.weapon_revision;
        if (authorized_return || (state.return_slot >= 0 && slot != state.return_from_slot)) state.return_slot = state.return_from_slot = -1;
        state.previous_slot = slot; state.previous_weapon = weapon;
        frame.weapon_permitted = authorized_swap || weapon_allowed(weapon, settings.weapon);
        // A physical click may already have caused a helper's slot switch before
        // this frame. Apply the activation filter to that helper's original slot,
        // not to its automatically selected mace/axe.
        if (!frame.weapon_permitted) {
            int starting_slot = modules::shield_breaker::starting_slot_if_owned(context.player, context.world, slot);
            if (starting_slot < 0) starting_slot = modules::mace_swap::starting_slot_if_owned(context.player, context.world, slot);
            if (starting_slot >= 0) {
                auto inventory = context.player.getInventory.call();
                if (platform::jni_context::clear_exception() || !inventory) return false;
                auto items = inventory.main.get();
                if (platform::jni_context::clear_exception() || !items) return false;
                const auto kind = settings.weapon == weapon_mode::sword ? services::inventory::hotbar_item_kind::sword : services::inventory::hotbar_item_kind::axe;
                frame.weapon_permitted = services::inventory::slot_matches(items, starting_slot, kind);
                if (platform::jni_context::clear_exception()) return false;
            }
        }
        if (!services::attack::read_cooldown(context.player, frame.cooldown)) return false;
        frame.attack_ticks = context.player.lastAttackedTicks.get();
        if (platform::jni_context::clear_exception() || !read_critical_conditions(context.player, frame)) return false;
        frame.session = state.session; frame.settings_revision = state.settings_revision;
        frame.weapon_revision = state.weapon_revision;
        frame.manual_generation = services::input::manual_attack_generation();
        if (state.engine.ticket) {
            const auto receipt = services::input::trigger_click_receipt(static_cast<std::uint32_t>(state.engine.ticket));
            frame.input_delivered = receipt.delivered; frame.input_cancelled = receipt.cancelled;
        }
        frame.eligible = true;
        return true;
    }
}

void trigger_engine::request_reset(adapter_runtime& state)
{
    std::lock_guard lock(state.submission_mutex);
    state.reset_generation.fetch_add(1, std::memory_order_release);
    services::input::cancel_trigger_click();
    services::combat::release_trigger();
}
void trigger_engine::run_overlay(const config& settings, adapter_runtime& state, bool enabled)
{
    if (!platform::jni_context::bind_current_thread()) {
        services::input::cancel_trigger_click(); services::combat::release_trigger(); return;
    }
    jni::frame local_frame{192};
    services::game::client_context context{};
    try {
        const auto generation = state.reset_generation.load(std::memory_order_acquire);
        if (state.seen_reset != generation || !enabled) {
            services::game::capture_client_context(context, false, false);
            stop(state, context); state.seen_reset = generation;
            if (!enabled) { state.world.clear_ref(); state.player.clear_ref(); return; }
        }
        observation frame; maps::Entity target; weapon_type weapon; int slot = -1;
        if (!capture(settings, state, context, frame, target, weapon, slot)) { stop(state, context); return; }
        const auto previous_ticket = state.engine.ticket;
        const auto decision = step(settings, state.engine, frame, now_ms(), state.random);
        if (decision == action::cancel || decision == action::retire) {
            const bool consumed = decision == action::retire;
            services::input::cancel_trigger_click(static_cast<std::uint32_t>(previous_ticket));
            finish_preparation(state, context, consumed);
        }
        if (!frame.weapon_permitted) { stop(state, context); return; }
        services::combat::claim_trigger(frame.target_id, frame.session);
        if (decision != action::prepare) return;
        if (state.prepared == preparation::none) {
            state.original_slot = slot;
            const auto shield = shield_faces_player(context.player, target);
            if (!shield || platform::jni_context::clear_exception()) { stop(state, context); return; }
            if (*shield && weapon != weapon_type::axe) {
                if (!modules::shield_breaker::prepare_for_trigger_attack()) return;
                state.prepared = preparation::shield;
                state.prepared_slot = modules::shield_breaker::trigger_prepared_slot();
            } else if (!*shield && modules::mace_swap::prepare_for_trigger_attack()) {
                state.prepared = preparation::mace;
                state.prepared_slot = modules::mace_swap::trigger_prepared_slot();
            }
            // A helper can accept an already-selected weapon without owning a swap.
            if (state.prepared_slot < 0) state.prepared = preparation::none;
        }
        observation confirmed; maps::Entity confirmed_target; weapon_type confirmed_weapon; int confirmed_slot = -1;
        if (!capture(settings, state, context, confirmed, confirmed_target, confirmed_weapon, confirmed_slot) ||
            confirmed.target_id != frame.target_id || confirmed.session != frame.session ||
            confirmed.weapon_revision != frame.weapon_revision || confirmed.settings_revision != frame.settings_revision ||
            confirmed.manual_generation != frame.manual_generation || state.reset_generation.load(std::memory_order_acquire) != generation) {
            stop(state, context); return;
        }
        const auto shield_now = shield_faces_player(context.player, confirmed_target);
        if (!shield_now) { stop(state, context); return; }
        if (*shield_now && confirmed_weapon != weapon_type::axe) {
            finish_preparation(state, context, false); state.engine.status = phase::preparing; return;
        }
        auto fresh_target = services::target::fresh_crosshair_entity(context.instance, context.player);
        if (!fresh_target || !fresh_target.is_same_object(confirmed_target) || platform::jni_context::clear_exception()) {
            stop(state, context); return;
        }
        const auto confirmed_decision = step(settings, state.engine, confirmed, now_ms(), state.random);
        if (confirmed_decision != action::prepare) {
            if (confirmed_decision == action::cancel || confirmed_decision == action::retire)
                finish_preparation(state, context, confirmed_decision == action::retire);
            return;
        }
        services::input::click_ticket ticket;
        bool reset_before_send = false;
        {
            std::lock_guard lock(state.submission_mutex);
            reset_before_send = state.reset_generation.load(std::memory_order_acquire) != generation;
            if (!reset_before_send) {
                ticket = services::input::submit_trigger_click(gui::get_window(), confirmed.physical_left,
                    [settings, &state, confirmed, confirmed_slot, confirmed_weapon, generation,
                        threshold = state.engine.threshold, deadline = now_ms() + 180] {
                        return validate_delivery(settings, state, confirmed, confirmed_slot, confirmed_weapon,
                            generation, threshold, deadline);
                    });
            }
        }
        if (reset_before_send) { stop(state, context); return; }
        if (ticket.submission != services::input::submission_status::complete) {
            finish_preparation(state, context, false); submission_failed(state.engine, now_ms()); return;
        }
        submitted(state.engine, ticket.id, confirmed, now_ms());
    } catch (...) {
        platform::jni_context::clear_exception(); stop(state, context);
    }
    platform::jni_context::clear_exception();
}
