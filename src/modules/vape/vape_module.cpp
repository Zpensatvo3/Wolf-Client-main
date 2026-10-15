#include "vape_module.hpp"
#include "game_bindings.hpp"
#include "advanced_runtime.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../../services/game/client_context.hpp"
#include "../../services/game/target_service.hpp"
#include "../../services/game/attack_coordinator.hpp"
#include "../../services/input/input_service.hpp"
#include "../core/module_registry.hpp"
#include "../../friends/friends.hpp"
#include "../../gui/gui.hpp"
#include "../../gui/widgets.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

// Independent user-supplied engines, corrected against the recovered Java source.
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "../../../combat cpp/aimassist/AimAssist.hpp"
#include "../../../combat cpp/combat/BowAimbot.hpp"
#include "../../../combat cpp/combat/Triggerbot.hpp"
#include "../../../combat cpp/clicker/LeftClicker.hpp"
#include "../../../combat cpp/clicker/RightClicker.hpp"
#include "../../../combat cpp/utility/Sprint.hpp"
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace vape_bridge
{
    using vector = Vape::Math::Vector3;
    using clock = std::chrono::steady_clock;
    struct runtime
    {
        maps::ClientWorld world{nullptr, true};
        maps::ClientPlayerEntity player{nullptr, true};
        Vape::Combat::AimAssist aim;
        Vape::Combat::BowAimbot bow;
        Vape::Combat::Triggerbot trigger;
        Vape::Combat::LeftClicker left;
        Vape::Combat::RightClicker right;
        Vape::Combat::Sprint sprint;
        int last_tick = -1, target_id = -1;
        bool mouse_down = false;
        bool clicking = false;
        clock::time_point aim_time{};
        clock::time_point target_time{};
    };

    bool finite(vector point)
    {
        return std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    }
    bool finite(const Vape::Math::BoundingBox& box)
    {
        return finite(box.min) && finite(box.max) &&
            box.min.x <= box.max.x && box.min.y <= box.max.y && box.min.z <= box.max.z;
    }
    template <typename Entity>
    vector position(const Entity& entity)
    {
        const auto p = entity.get_position(); return {p.x, p.y, p.z};
    }
    template <typename Entity>
    vector velocity(const Entity& entity)
    {
        auto value = entity.getVelocity.call();
        if (!value || platform::jni_context::clear_exception())
            return {(std::numeric_limits<double>::quiet_NaN)(),0,0};
        return {value.x.get(), value.y.get(), value.z.get()};
    }
    std::string item_key(const maps::ItemStack& stack)
    {
        if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception()) return {};
        auto item = stack.item.get();
        if (!item || platform::jni_context::clear_exception()) return {};
        auto text = item.getTranslationKey.call();
        if (!text || platform::jni_context::clear_exception()) return {};
        return text.to_string();
    }
    bool weapon(const std::string& key)
    {
        return key.ends_with("_sword") || key.ends_with("_axe") || key.ends_with(".mace");
    }
    bool usable(const maps::ItemStack& stack, bool blocks_only)
    {
        if (!stack || stack.isEmpty.call() == JNI_TRUE || platform::jni_context::clear_exception()) return false;
        auto item = stack.item.get();
        if (!item || platform::jni_context::clear_exception()) return false;
        return !blocks_only || item.is_instance_of<maps::BlockItem>();
    }
    bool pressed(const maps::KeyBinding& key)
    {
        return key && key.isPressed.is_valid() && key.isPressed.call() == JNI_TRUE;
    }
    bool block_target(const maps::MinecraftClient& instance)
    {
        auto hit = instance.crosshairTarget.get();
        if (!hit || platform::jni_context::clear_exception()) return false;
        auto type = hit.getType.call();
        auto block = maps::HitResult$Type{}.BLOCK.get();
        return type && block && type.is_same_object(block);
    }
    bool sensitivity(const maps::GameOptions& options, float& output)
    {
        auto setting = options.getMouseSensitivity.call();
        if (!setting || platform::jni_context::clear_exception()) return false;
        auto object = setting.getValue.call();
        if (!object || platform::jni_context::clear_exception()) return false;
        maps::Double value(object.object_instance, false);
        if (!value.doubleValue.is_valid()) return false;
        const auto raw = value.doubleValue.call();
        if (platform::jni_context::clear_exception() || !std::isfinite(raw) || raw < 0 || raw > 1) return false;
        output = static_cast<float>(raw); return true;
    }
    bool target_allowed(const maps::Entity& entity, const maps::ClientPlayerEntity& player)
    {
        return entity && entity.is_instance_of<maps::PlayerEntity>() && !entity.is_same_object(player) &&
            entity.isAlive.call() == JNI_TRUE && entity.isInvisible.call() != JNI_TRUE &&
            !friends::is_friend(entity);
    }
    struct target_snapshot
    {
        Vape::Combat::AimTargetCandidate aim;
        Vape::Combat::BowAimbot::TargetCandidate bow;
    };
    std::vector<target_snapshot> targets(const services::game::client_context& context)
    {
        std::vector<target_snapshot> result;
        auto players = platform::adapter::get().get_players(context.world);
        if (!players || platform::jni_context::clear_exception()) return result;
        auto array = players.toArray.call();
        if (!array || platform::jni_context::clear_exception()) return result;
        vape_bindings::Player local(context.player.object_instance, false);
        if (!local.canSee.is_valid()) return result;
        for (const auto& object : array.to_vector()) {
            jni::frame frame{48};
            maps::Entity entity(object.object_instance, false);
            if (!target_allowed(entity, context.player) || platform::jni_context::clear_exception()) continue;
            if (local.canSee.call(entity) != JNI_TRUE || platform::jni_context::clear_exception()) continue;
            auto bounds = entity.get_bounding_box();
            if (!bounds || !entity.getEyeY.is_valid() || platform::jni_context::clear_exception()) continue;
            target_snapshot sample;
            sample.aim.entityId = entity.getId.call();
            sample.aim.boundingBox = {{bounds.minX.get(),bounds.minY.get(),bounds.minZ.get()},
                {bounds.maxX.get(),bounds.maxY.get(),bounds.maxZ.get()}};
            sample.aim.velocity = velocity(entity);
            vape_bindings::Player target(entity.object_instance, false);
            if (!target.getArmor.is_valid() || !target.getHealth.is_valid()) continue;
            sample.aim.health = target.getHealth.call();
            sample.aim.armorValue = target.getArmor.call();
            const auto weapon = vape_advanced::weapon(target.getMainHandStack.call());
            sample.aim.equipmentTier = static_cast<int>(Vape::Combat::ItemStackScoreUtil::getBaseDamage(
                weapon.material,weapon.isSword,weapon.isAxe,weapon.isMace));
            sample.bow.entityId = sample.aim.entityId;
            sample.bow.eyePos = position(entity);
            sample.bow.eyePos.y = entity.getEyeY.call();
            sample.bow.velocity = sample.aim.velocity;
            if (platform::jni_context::clear_exception() || !finite(sample.aim.boundingBox) ||
                !finite(sample.bow.eyePos) || !finite(sample.aim.velocity) ||
                !std::isfinite(sample.aim.health)) continue;
            result.push_back(sample);
        }
        return result;
    }

    // Common IO arbitration only; no Wolf target selection, timing or aim math.
    bool active(const char* id)
    {
        const auto* module = modules::find_module_by_id(id);
        return module && module->enabled;
    }
    const char* name(modules::vape_kind kind)
    {
        switch (kind) {
        case modules::vape_kind::aim: return "vape aim assist";
        case modules::vape_kind::bow: return "vape bow aimbot";
        case modules::vape_kind::left: return "vape left clicker";
        case modules::vape_kind::right: return "vape right clicker";
        case modules::vape_kind::sprint: return "vape sprint";
        case modules::vape_kind::trigger: return "vape triggerbot";
        }
        return "vape";
    }
    const char* id(modules::vape_kind kind)
    {
        switch (kind) {
        case modules::vape_kind::aim: return "vape_aim_assist";
        case modules::vape_kind::bow: return "vape_bow_aimbot";
        case modules::vape_kind::left: return "vape_left_clicker";
        case modules::vape_kind::right: return "vape_right_clicker";
        case modules::vape_kind::sprint: return "vape_sprint";
        case modules::vape_kind::trigger: return "vape_triggerbot";
        }
        return "vape";
    }
}

using namespace vape_bridge;

modules::vape_module::vape_module(vape_kind selected)
    : module(vape_bridge::name(selected), "Vape", vape_bridge::id(selected)), kind(selected)
{
    if (kind == vape_kind::right) { settings.min_cps = 12; settings.max_cps = 18; }
    if (kind == vape_kind::bow) { settings.range = 50; settings.fov = 90; }
    if (kind == vape_kind::trigger) {
        // Recovered Java defaults: enabling the module is sufficient to run it.
        // Existing imported settings still override these defaults normally.
        settings.require_mouse = false; settings.air_crits = false;
        settings.shield_check = false; settings.weapons_only = false;
        settings.mouse_delay_max = 0;
    }
}
modules::vape_module::~vape_module() = default;
void modules::vape_module::on_enable(::cache&) { reset_requested.store(true); }
void modules::vape_module::on_disable(::cache&) { reset_requested.store(true); }

const char* modules::vape_module::conflict() const
{
    using vape_bridge::active;
    if (kind == vape_kind::aim || kind == vape_kind::bow) {
        if (active("aim_assist") || active("legit_click_aim_assist") || active("blatant_click_aim_assist"))
            return gui::text("Pause: Wolf Aim Assist is enabled", "Bekliyor: Wolf Aim Assist açık");
    }
    if (kind == vape_kind::left || kind == vape_kind::trigger) {
        if (vape_advanced::owns_attack())
            return gui::text("Pause: Vape attack owner is enabled", "Bekliyor: Vape saldırı sahibi açık");
        if (active("triggerbot") || active("left_clicker") || active("shield_breaker") ||
            active("mace_swap") || active("hit_crystal") || active("anchor_macro") || active("blatant_anchor_macro"))
            return gui::text("Pause: Wolf attack module is enabled", "Bekliyor: Wolf saldırı modülü açık");
        if (kind == vape_kind::left && active("vape_triggerbot"))
            return gui::text("Pause: Vape Triggerbot is enabled", "Bekliyor: Vape Triggerbot açık");
    }
    if (kind == vape_kind::right && (active("right_clicker") || active("fast_exp")))
        return gui::text("Pause: Wolf right click module is enabled", "Bekliyor: Wolf sağ tık modülü açık");
    if (kind == vape_kind::sprint && active("sprint"))
        return gui::text("Pause: Wolf Sprint is enabled", "Bekliyor: Wolf Sprint açık");
    return nullptr;
}

void modules::vape_module::render_overlay()
{
    if (!platform::jni_context::bind_current_thread()) { runtime_issue="Vape: render-thread JNI unavailable."; reset_requested.store(true); return; }
    if (reset_requested.exchange(false) || !enabled) state.reset();
    const auto window = gui::get_window();
    if (!enabled || gui::draw || gui::loader_visible || gui::should_unload || !window ||
        GetForegroundWindow() != window || conflict()) { state.reset(); if(!enabled)runtime_issue.clear(); return; }
    runtime_issue.clear();
    struct clear_on_exit { ~clear_on_exit() { platform::jni_context::clear_exception(); } } cleanup;
    try {
        jni::frame frame{256};
        services::game::client_context context;
        if (!services::game::capture_client_context(context)) { runtime_issue="Vape: client/player snapshot unavailable."; state.reset(); return; }
        if (context.screen ||
            context.player.isAlive.call() != JNI_TRUE || platform::jni_context::clear_exception()) { state.reset(); return; }
        auto camera = context.instance.getCameraEntity.call();
        if (!camera || !camera.is_same_object(context.player) || platform::jni_context::clear_exception()) { state.reset(); return; }
        if (state && (!state->world.is_same_object(context.world) || !state->player.is_same_object(context.player) ||
            platform::jni_context::clear_exception())) state.reset();
        options config;
        { std::lock_guard lock(settings_mutex); config = settings; }
        if (!state) {
            state = std::make_unique<runtime>(); state->world = context.world; state->player = context.player;
            auto& click = kind == vape_kind::right ? state->right.getEngine() : state->left.getEngine();
            click.setRandomizationMode(static_cast<Vape::Combat::ClickEngine::RandomizationMode>(config.randomization));
            click.setCps(config.min_cps, config.max_cps); click.setJitter(config.jitter);
        }
        auto& r = *state;
        const auto age = context.player.age.get();
        const bool tick = r.last_tick != age;
        r.last_tick = age;
        auto options = context.instance.options.get();
        auto main = context.player.getMainHandStack.call();
        const std::string key = item_key(main);
        const bool holding_weapon = weapon(key);
        const bool left = services::input::physical_button_down(services::input::mouse_button::left);
        const bool right = services::input::physical_button_down(services::input::mouse_button::right);
        const bool using_item = context.player.isUsingItem.call() == JNI_TRUE;
        const bool looking_at_block = block_target(context.instance);
        if (!options || platform::jni_context::clear_exception()) { state.reset(); return; }
        if (!context.player.yaw.is_valid() || !context.player.pitch.is_valid()) { state.reset(); return; }
        float yaw = context.player.yaw.get(), pitch = context.player.pitch.get();
        const float original_yaw = yaw, original_pitch = pitch;
        float mouse_sensitivity = 0;
        if (!std::isfinite(yaw) || !std::isfinite(pitch) || platform::jni_context::clear_exception()) { state.reset(); return; }
        if (kind != vape_kind::sprint && kind != vape_kind::trigger && !sensitivity(options, mouse_sensitivity)) {
            runtime_issue="Vape: mouse sensitivity binding unavailable."; state.reset(); return;
        }

        if (kind == vape_kind::left || kind == vape_kind::right) {
            if (kind == vape_kind::left) {
                r.left.breakBlocksProtection = config.protect_blocks; r.left.weaponsOnly = config.weapons_only;
                if (!using_item && r.left.shouldClick(left, looking_at_block, holding_weapon, false)) {
                    const bool sent = services::combat::try_unowned_attack([&] {
                        return enabled && !reset_requested.load() &&
                            services::input::game_mouse_click(services::input::mouse_button::left);
                    });
                    if (sent) { r.left.onClickProcessed(); r.clicking = true; }
                }
                if (!left || using_item || (config.protect_blocks && looking_at_block) ||
                    (config.weapons_only && !holding_weapon)) r.clicking = false;
                if (r.clicking) {
                    if (tick) r.left.getEngine().updateJitter();
                    r.left.getEngine().applyJitterRotation(yaw, pitch, mouse_sensitivity);
                }
            } else {
                r.right.holdToClick = config.hold_to_click; r.right.blocksOnly = config.blocks_only; r.right.allowOffhand = config.offhand;
                auto off = context.player.getOffHandStack.call();
                const bool main_usable = usable(main, config.blocks_only), off_usable = usable(off, config.blocks_only);
                if (platform::jni_context::clear_exception()) { state.reset(); return; }
                if (!using_item && r.right.shouldClick(right, main_usable, off_usable, false)) {
                    bool sent = false;
                    if (right && enabled && !reset_requested.load())
                        sent = services::input::game_mouse_click(services::input::mouse_button::right);
                    else if (!config.hold_to_click && enabled && !reset_requested.load() && context.instance.doItemUse.is_valid()) {
                        context.instance.doItemUse.call(); sent = !platform::jni_context::clear_exception();
                    }
                    if (sent) { r.right.onClickProcessed(); r.clicking = true; }
                }
                if ((config.hold_to_click && !right) || using_item ||
                    !(main_usable || (config.offhand && off_usable))) r.clicking = false;
                if (r.clicking) {
                    if (tick) r.right.getEngine().updateJitter();
                    r.right.getEngine().applyJitterRotation(yaw, pitch, mouse_sensitivity);
                }
            }
        } else if (kind == vape_kind::sprint && tick) {
            vape_bindings::Player local(context.player.object_instance, false);
            if (!local.getHungerManager.is_valid() || !local.horizontalCollision.is_valid() ||
                !local.hasStatusEffectEntry.is_valid()) { state.reset(); return; }
            auto hunger = local.getHungerManager.call();
            auto blindness = maps::StatusEffects{}.BLINDNESS.get();
            if (!hunger || !blindness || !hunger.getFoodLevel.is_valid() || platform::jni_context::clear_exception()) { state.reset(); return; }
            const bool forward = pressed(options.forwardKey.get()), sneak = pressed(options.sneakKey.get());
            const bool collision = local.horizontalCollision.get() == JNI_TRUE;
            const bool invisible = local.isInvisible.call() == JNI_TRUE;
            const bool blind = local.hasStatusEffectEntry.call(blindness) == JNI_TRUE;
            const int food = hunger.getFoodLevel.call();
            if (platform::jni_context::clear_exception()) { state.reset(); return; }
            r.sprint.cancelInvis = config.cancel_invis;
            // Vanilla remains responsible for stopping sprint; no unowned key/flag restoration.
            if (enabled && !reset_requested.load() && !using_item &&
                r.sprint.shouldSprint(forward, sneak, collision, invisible, blind, food))
                context.player.setSprinting.call(JNI_TRUE);
        } else if (kind == vape_kind::trigger) {
            r.trigger.requireMouseDown = config.require_mouse; r.trigger.airCrits = config.air_crits;
            r.trigger.shieldCheck = config.shield_check; r.trigger.selectFirstHit = config.first_hit;
            r.trigger.limitToWeapons = config.weapons_only; r.trigger.targetMissChance = config.miss_chance;
            r.trigger.earlyHitChance = config.early_chance; r.trigger.extraDelayMinTicks = config.delay_min;
            r.trigger.extraDelayMaxTicks = config.delay_max; r.trigger.mouseOverDelayMinMs = config.mouse_delay_min;
            r.trigger.mouseOverDelayMaxMs = config.mouse_delay_max;
            if (left != r.mouse_down) { r.trigger.onInput(left, true); r.mouse_down = left; }
            if (!tick || using_item) return;
            // Fresh vanilla ray is an IO validation, not Wolf target-selection math.
            const bool extended = vape_advanced::reach_active();
            auto target = extended ? services::target::crosshair_entity(context.instance) :
                services::target::fresh_crosshair_entity(context.instance, context.player);
            if (extended && !vape_advanced::confirm_reach_target(target)) target = maps::Entity{};
            bool valid = target_allowed(target, context.player);
            maps::LivingEntity living(valid ? target.object_instance : nullptr, false);
            const int target_id = valid ? target.getId.call() : -1;
            const bool blocking = valid && living.isBlocking.call() == JNI_TRUE;
            const auto motion = velocity(context.player);
            if (!context.player.getAttackCooldownProgress.is_valid() || !context.player.hurtTime.is_valid()) { state.reset(); return; }
            const float cooldown = context.player.getAttackCooldownProgress.call(r.trigger.cooldownOffsetTicks(target_id));
            const int hurt = context.player.hurtTime.get();
            const double fall = context.player.fallDistance.get();
            const bool grounded = context.player.isOnGround.call() == JNI_TRUE;
            if (platform::jni_context::clear_exception() || !finite(motion) || !std::isfinite(cooldown) ||
                !std::isfinite(fall)) { state.reset(); return; }
            auto action = r.trigger.onTick(true, hurt, fall, motion.y, grounded, cooldown, holding_weapon,
                valid, target_id, valid, blocking, key.ends_with("_axe") || key.ends_with(".mace"), false, false);
            if (action == Vape::Combat::Triggerbot::TriggerAction::ATTACK && valid) {
                const bool consumed = services::combat::try_unowned_attack([&] {
                    auto confirmed = extended && vape_advanced::confirm_reach_target(target) ? target :
                        services::target::fresh_crosshair_entity(context.instance, context.player);
                    auto vanilla_target = services::target::crosshair_entity(context.instance);
                    if (!confirmed || !confirmed.is_same_object(target) || platform::jni_context::clear_exception() ||
                        !vanilla_target || !vanilla_target.is_same_object(confirmed) || platform::jni_context::clear_exception() ||
                        (config.require_mouse && !services::input::physical_button_down(services::input::mouse_button::left)) ||
                        !enabled || reset_requested.load() || conflict()) return false;
                    if (!context.instance.doAttack.is_valid()) return false;
                    const float before = context.player.getAttackCooldownProgress.call(0.f);
                    if (platform::jni_context::clear_exception()) return false;
                    // Minecraft.doAttack's boolean describes block breaking,
                    // not whether an entity attack succeeded (normally false).
                    context.instance.doAttack.call();
                    if (platform::jni_context::clear_exception()) return false;
                    const float after = context.player.getAttackCooldownProgress.call(0.f);
                    return !platform::jni_context::clear_exception() && std::isfinite(after) && after < before;
                });
                if (consumed) r.trigger.onAttackProcessed();
            } else if (action == Vape::Combat::Triggerbot::TriggerAction::SWING_MISS) {
                if (!enabled || reset_requested.load() || conflict() ||
                    (config.require_mouse && !services::input::physical_button_down(services::input::mouse_button::left))) return;
                auto hand = maps::Hand{}.MAIN_HAND.get();
                if (hand && !platform::jni_context::clear_exception()) {
                    context.player.swingHand.call(hand);
                    if (!platform::jni_context::clear_exception()) r.trigger.onAttackProcessed();
                }
            }
        } else if (kind == vape_kind::aim || kind == vape_kind::bow) {
            if (!context.player.getEyeY.is_valid()) { state.reset(); return; }
            auto eye = position(context.player); eye.y = context.player.getEyeY.call();
            if (!finite(eye) || platform::jni_context::clear_exception()) { state.reset(); return; }
            if (kind == vape_kind::aim) {
                vape_bindings::Manager manager(context.interaction.object_instance, false);
                if (config.protect_blocks && !manager.breaking.is_valid()) { state.reset(); return; }
                const bool breaking_blocks = left && looking_at_block && manager.breaking.call() == JNI_TRUE;
                // Bow Aimbot has priority only while an actual bow/crossbow
                // input is active, not for every weapon while its toggle is on.
                bool bow_input = false;
                if (right && active("vape_bow_aimbot")) {
                    auto off = context.player.getOffHandStack.call();
                    auto crossbow = key.ends_with(".crossbow") ? main : off;
                    if (item_key(crossbow).ends_with(".crossbow")) {
                        vape_bindings::Crossbow binding;
                        bow_input = binding.isCharged.is_valid() && binding.isCharged.call(crossbow) == JNI_TRUE;
                    }
                    if (platform::jni_context::clear_exception()) { state.reset(); return; }
                }
                const bool activated = !using_item && (!config.require_mouse || left) &&
                    !bow_input && (!config.weapons_only || holding_weapon) && (!config.protect_blocks || !breaking_blocks);
                if (!activated) { r.aim.reset(); r.target_id = -1; r.aim_time={}; return; }
                r.aim.distance = config.range; r.aim.maxAngle = config.fov;
                r.aim.requireMouseDown = config.require_mouse; r.aim.weaponsOnly = config.weapons_only;
                r.aim.checkBlockBreak = config.protect_blocks;
                r.aim.targetSortMode = static_cast<Vape::Combat::AimAssist::TargetSortMode>(config.sort);
                r.aim.mode = static_cast<Vape::Combat::AimAssist::EngineMode>(config.aim_mode);
                r.aim.getAdaptiveTargeting().targetArea = static_cast<Vape::Combat::AimAssistTargeting::TargetArea>(config.target_area);
                auto& simple = r.aim.getSimpleRotation();
                simple.horizontalSpeed = config.horizontal; simple.verticalSpeed = config.vertical;
                simple.aimVertically = config.aim_vertical; simple.strafeIncrease = config.strafe_increase;
                {
                    std::vector<Vape::Combat::AimTargetCandidate> candidates;
                    for (const auto& sample : targets(context)) candidates.push_back(sample.aim);
                    const Vape::Combat::AimTargetCandidate* target = nullptr;
                    const auto now=clock::now();
                    // A held activation retains a valid target instead of
                    // resetting all velocity buffers whenever two players cross.
                    if (config.require_mouse || now-r.target_time<std::chrono::milliseconds(700)) for (const auto& candidate : candidates)
                        if (candidate.entityId==r.target_id && r.aim.isValidTarget(eye,yaw,candidate)) { target=&candidate; break; }
                    if (!target) {target=r.aim.findBestTarget(eye,yaw,candidates);r.target_time=now;}
                    const int next_id = target ? target->entityId : -1;
                    if (r.target_id != next_id) { r.aim.reset(); r.aim_time={}; }
                    r.target_id = next_id;
                    const float strafe = (pressed(options.rightKey.get()) ? 1.f : 0.f) -
                        (pressed(options.leftKey.get()) ? 1.f : 0.f);
                    if (platform::jni_context::clear_exception()) { state.reset(); return; }
                    const auto motion = velocity(context.player);
                    if (!finite(motion)) { state.reset(); return; }
                    const double elapsed=r.aim_time.time_since_epoch().count()?
                        std::chrono::duration<double>(now-r.aim_time).count():.001;
                    r.aim_time=now;
                    r.aim.updateFrame(eye, yaw, pitch, strafe, left, holding_weapon, breaking_blocks, target,
                        motion,context.player.isOnGround.call()==JNI_TRUE,elapsed);
                }
                r.aim.applyRenderTick(yaw, pitch, mouse_sensitivity);
            } else {
                // Bow smoothing is defined per tick (aimSpeed * .05) in the supplied engine.
                if (!tick) return;
                auto active_stack = context.player.getActiveItem.call();
                float speed = 0.f;
                const auto active_key = item_key(active_stack);
                if (using_item && active_key.ends_with(".bow")) {
                    vape_bindings::Bow bow;
                    if (!bow.getPullProgress.is_valid() || !context.player.getItemUseTime.is_valid()) return;
                    const int use_ticks = context.player.getItemUseTime.call();
                    speed = Vape::Combat::BowAimbot::calculateArrowVelocity(false, bow.getPullProgress.call(use_ticks));
                } else if (right) {
                    auto off = context.player.getOffHandStack.call();
                    auto stack = key.ends_with(".crossbow") ? main : off;
                    if (!item_key(stack).ends_with(".crossbow")) return;
                    vape_bindings::Crossbow crossbow;
                    if (!crossbow.isCharged.is_valid() || crossbow.isCharged.call(stack) != JNI_TRUE) return;
                    speed = Vape::Combat::BowAimbot::calculateArrowVelocity(true, 1.f);
                }
                if (platform::jni_context::clear_exception() || !std::isfinite(speed) || speed <= 0) return;
                r.bow.aimSpeed = config.bow_speed; r.bow.maxRange = config.range; r.bow.maxAngle = config.fov;
                r.bow.predictMovement = config.prediction; r.bow.highArc = config.high_arc;
                std::vector<Vape::Combat::BowAimbot::TargetCandidate> candidates;
                for (const auto& sample : targets(context)) candidates.push_back(sample.bow);
                const auto* target = r.bow.findBestTarget(eye, yaw, candidates);
                if (target) {
                    const auto result = r.bow.calculateBallisticSolution(eye, yaw, pitch, *target, speed);
                    if (result) { yaw = result->smoothedAngle.yaw; pitch = result->smoothedAngle.pitch; }
                }
            }
        }
        if (platform::jni_context::clear_exception() || !enabled || reset_requested.load() ||
            !std::isfinite(yaw) || !std::isfinite(pitch) || pitch < -90 || pitch > 90) { state.reset(); return; }
        if (yaw != original_yaw || pitch != original_pitch) {
            if(kind==vape_kind::aim||kind==vape_kind::bow) {
                const float delta=Vape::Math::RotationUtil::wrapDegrees(yaw-original_yaw);
                // Java's mouse applier adjusts interpolation history too. Keep
                // yaw continuous across +/-180 instead of rendering a 360 turn.
                yaw=original_yaw+delta;
                const bool history=context.player.prevYaw.is_valid()&&context.player.prevPitch.is_valid();
                if(history) {
                    const float previousYaw=context.player.prevYaw.get(),previousPitch=context.player.prevPitch.get();
                    if(platform::jni_context::clear_exception()) {state.reset();return;}
                    context.player.prevYaw.set(previousYaw+delta);
                    context.player.prevPitch.set(previousPitch+pitch-original_pitch);
                }
            }
            context.player.yaw.set(yaw); context.player.pitch.set(pitch);
            if (platform::jni_context::clear_exception()) state.reset();
        }
    } catch (...) { platform::jni_context::clear_exception(); state.reset(); }
}

modules::module* modules::create_vape_aim() { return new vape_module(vape_kind::aim); }
modules::module* modules::create_vape_bow() { return new vape_module(vape_kind::bow); }
modules::module* modules::create_vape_left() { return new vape_module(vape_kind::left); }
modules::module* modules::create_vape_right() { return new vape_module(vape_kind::right); }
modules::module* modules::create_vape_sprint() { return new vape_module(vape_kind::sprint); }
modules::module* modules::create_vape_trigger() { return new vape_module(vape_kind::trigger); }
