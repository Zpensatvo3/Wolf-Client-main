#include "crystal_aura.hpp"
#include "crystal_aura_engine.hpp"
#include "../../ui/settings_renderer.hpp"
#include "../../gui/gui.hpp"
#include "../../friends/friends.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/overlay_context.hpp"
#include "../../services/game/anti_bot_service.hpp"
#include "../../services/game/inventory_service.hpp"
#include "../../services/game/hotbar_service.hpp"
#include "../../services/game/attack_service.hpp"
#include "../../services/game/attack_coordinator.hpp"
#include "../../services/game/target_service.hpp"
#include "../ui_redirect.hpp"
#include <limits>
#include <numbers>
#include <vector>

namespace
{
    namespace logic = modules::crystal_aura_logic;
    using context = services::game::client_context;
    bool clean() { return !platform::jni_context::clear_exception(); }
    bool read_position(const auto& entity, logic::point& out)
    {
        auto p = entity.getPos.call();
        if (!p || !clean()) return false;
        out = {p.x.get(),p.y.get(),p.z.get()};
        return clean() && logic::finite(out);
    }
    bool read_box(const auto& entity, logic::bounds& out)
    {
        auto b = entity.get_bounding_box();
        if (!b || !clean()) return false;
        out = {{b.minX.get(),b.minY.get(),b.minZ.get()},{b.maxX.get(),b.maxY.get(),b.maxZ.get()}};
        return clean() && logic::valid(out);
    }
    maps::BlockState block_at(const maps::ClientWorld& world, logic::cell c)
    {
        auto p = maps::BlockPos::new_object(&maps::BlockPos::constructor,jint(c.x),jint(c.y),jint(c.z));
        if (!p || !clean()) return {};
        return world.getBlockState.call(p);
    }
    bool air(const maps::ClientWorld& world, logic::cell c)
    {
        auto b = block_at(world,c);
        return b && clean() && b.isAir.call() == JNI_TRUE && clean();
    }
    bool crystal_base(const maps::ClientWorld& world, logic::cell c)
    {
        auto b = block_at(world,c);
        auto obsidian = maps::Blocks{}.OBSIDIAN.get();
        auto bedrock = maps::Blocks{}.BEDROCK.get();
        if (!b || !obsidian || !bedrock || !clean()) return false;
        const bool obs = b.isOf.call(obsidian) == JNI_TRUE;
        const bool bed = b.isOf.call(bedrock) == JNI_TRUE;
        return clean() && (obs || bed);
    }
    maps::BlockHitResult ray(const context& ctx, logic::point eye, logic::point to)
    {
        auto a = maps::Vec3d::new_object(&maps::Vec3d::constructor,eye.x,eye.y,eye.z);
        auto b = maps::Vec3d::new_object(&maps::Vec3d::constructor,to.x,to.y,to.z);
        auto shape = maps::RaycastContext$ShapeType{}.COLLIDER.get();
        auto fluid = maps::RaycastContext$FluidHandling{}.NONE.get();
        if (!a || !b || !shape || !fluid || !clean()) return {};
        auto query = maps::RaycastContext::new_object(&maps::RaycastContext::constructor,
            a,b,shape,fluid,maps::Entity(ctx.player.object_instance,false));
        if (!query || !clean()) return {};
        return ctx.world.raycast.call(query);
    }
    bool visible(const context& ctx, logic::point eye, logic::point to)
    {
        auto hit = ray(ctx,eye,to);
        auto block = maps::HitResult$Type{}.BLOCK.get();
        if (!hit || !block || !clean()) return false;
        auto type = maps::HitResult(hit.object_instance,false).getType.call();
        return type && clean() && !type.is_same_object(block) && clean();
    }
    bool hits_top(const maps::BlockHitResult& hit, logic::cell expected)
    {
        if (!hit || !clean()) return false;
        auto block = maps::HitResult$Type{}.BLOCK.get();
        auto type = maps::HitResult(hit.object_instance,false).getType.call();
        auto p = hit.getBlockPos.call();
        auto side = hit.getSide.call();
        auto up = maps::Direction{}.UP.get();
        if (!block || !type || !p || !side || !up || !clean()) return false;
        const logic::cell actual{p.getX.call(),p.getY.call(),p.getZ.call()};
        return clean() && actual == expected && type.is_same_object(block) && side.is_same_object(up) && clean();
    }
    logic::point top_aim(logic::cell clicked)
    { return {clicked.x+.5,clicked.y+.999,clicked.z+.5}; }
    bool reachable_top(const context& ctx, logic::point eye, logic::cell clicked, double reach)
    {
        const auto to = top_aim(clicked);
        return logic::distance_squared(eye,to) <= reach*reach && hits_top(ray(ctx,eye,to),clicked);
    }
    bool eligible(const context& ctx, const maps::Entity& entity, const module_settings::crystal_aura& config)
    {
        if (!entity) return false;
        logic::target_state target;
        target.self = entity.is_same_object(ctx.player);
        const bool player = entity.is_instance_of<maps::PlayerEntity>();
        const bool mob = !player && entity.is_instance_of<maps::MobEntity>();
        if (!clean()) return false;
        target.kind = player ? logic::target_kind::player : mob ? logic::target_kind::mob : logic::target_kind::other;
        if (!logic::supports_target(config.target_mode,target.kind) || target.self) return false;
        target.alive = entity.isAlive.call() == JNI_TRUE;
        target.spectator = entity.isSpectator.call() == JNI_TRUE;
        maps::LivingEntity living(entity.object_instance,false);
        const float health = living.getHealth.call();
        auto world = entity.getWorld.call();
        if (!world || !clean() || !std::isfinite(health) || health <= 0 || !world.is_same_object(ctx.world) || !clean()) return false;
        if (player)
        {
            target.friendly = friends::is_friend(entity);
            target.bot = services::anti_bot::is_bot(entity);
        }
        return clean() && logic::target_allowed(config.target_mode,target);
    }
    int find_target(const context& ctx, logic::point eye, double seek_range, int locked,
        const module_settings::crystal_aura& config)
    {
        std::vector<logic::candidate> candidates;
        const auto consider = [&](const maps::Entity& entity) {
            logic::bounds box;
            if (!eligible(ctx,entity,config) || !read_box(entity,box)) return;
            const auto center = logic::point{(box.min.x+box.max.x)*.5,box.min.y+(box.max.y-box.min.y)*.6,(box.min.z+box.max.z)*.5};
            const double distance = logic::distance_squared(eye,logic::closest(eye,box));
            if (!std::isfinite(distance) || distance > seek_range*seek_range || !visible(ctx,eye,center)) return;
            const int id = entity.getId.call();
            if (clean()) candidates.push_back({id,distance,true});
        };
        if (config.target_mode == 0)
        {
            auto players = ctx.world.getPlayers.call();
            auto array = players ? players.toArray.call() : jni::array<maps::Object>{nullptr};
            if (!array || !clean() || array.get_length() > 512) return -1;
            for (const auto& object : array.to_vector())
            {
                jni::frame frame{32};
                consider(maps::Entity(object.object_instance,false));
                if (!clean()) return -1;
            }
        }
        else
        {
            auto entities = ctx.world.getEntities.call();
            auto iterator = entities ? entities.iterator.call() : maps::Iterator{};
            if (!iterator || !clean()) return -1;
            for (int count = 0; ; ++count)
            {
                const bool more = iterator.hasNext.call() == JNI_TRUE;
                if (!clean()) return -1;
                if (!more) break;
                if (count >= 4096) return -1;
                jni::frame frame{32};
                auto object = iterator.next.call();
                if (!object || !clean()) return -1;
                consider(maps::Entity(object.object_instance,false));
                if (!clean()) return -1;
            }
        }
        return logic::choose_target(candidates,locked);
    }
    struct entity_shape { int id; logic::bounds box; logic::point position; bool crystal; };
    bool scan_scene(const context& ctx, logic::point eye, std::vector<entity_shape>& scene)
    {
        auto entities = ctx.world.getEntities.call();
        auto iterator = entities ? entities.iterator.call() : maps::Iterator{};
        if (!iterator || !clean()) return false;
        for (int count = 0; ; ++count)
        {
            const bool more = iterator.hasNext.call() == JNI_TRUE;
            if (!clean()) return false;
            if (!more) return true;
            if (count >= 4096) return false; // Never treat a truncated scan as empty space.
            jni::frame frame{24};
            auto object = iterator.next.call();
            maps::Entity entity(object.object_instance,false);
            if (!entity || !clean()) return false;
            const bool alive = entity.isAlive.call() == JNI_TRUE;
            if (!clean()) return false;
            if (!alive) continue;
            logic::point position;
            if (!read_position(entity,position)) return false;
            if (logic::distance_squared(eye,position) > 144.) continue;
            logic::bounds box;
            if (!read_box(entity,box)) return false;
            const bool crystal = entity.is_instance_of<maps::EndCrystalEntity>();
            const int id = entity.getId.call();
            if (!clean()) return false;
            scene.push_back({id,box,position,crystal});
        }
    }
    int crystal_at(std::span<const entity_shape> scene, logic::cell base, int required = -1)
    {
        const logic::point spawn{base.x+.5,base.y+1.,base.z+.5};
        for (const auto& entity : scene)
            if (entity.crystal && (required < 0 || required == entity.id) &&
                logic::distance_squared(entity.position,spawn) < .25) return entity.id;
        return -1;
    }
    bool empty_of_entities(std::span<const entity_shape> scene, logic::bounds volume, int except = -1)
    {
        for (const auto& entity : scene)
            if (entity.id != except && logic::intersects(entity.box,volume)) return false;
        return true;
    }
    bool clear_site(const context& ctx, logic::cell base, std::span<const entity_shape> scene, bool existing)
    {
        if (!air(ctx.world,{base.x,base.y+1,base.z}) || !air(ctx.world,{base.x,base.y+2,base.z})) return false;
        if (!existing && (!air(ctx.world,base) || !empty_of_entities(scene,logic::base_volume(base)))) return false;
        return empty_of_entities(scene,logic::crystal_volume(base));
    }
    std::optional<logic::cell> plan_site(const context& ctx, logic::point eye, logic::bounds target,
        std::span<const entity_shape> scene, double block_reach, double entity_reach)
    {
        const double x = (target.min.x+target.max.x)*.5, z = (target.min.z+target.max.z)*.5;
        if (std::abs(x) > 30000000 || std::abs(z) > 30000000 || std::abs(target.min.y) > 30000000) return std::nullopt;
        const int tx = int(std::floor(x)), ty = int(std::floor(target.min.y)), tz = int(std::floor(z));
        std::optional<logic::cell> best;
        double score = std::numeric_limits<double>::infinity();
        for (int dy = -1; dy <= 0; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
                for (int dz = -2; dz <= 2; ++dz)
                {
                    jni::frame frame{48};
                    const logic::cell base{tx+dx,ty+dy,tz+dz};
                    const bool existing = crystal_base(ctx.world,base);
                    auto clicked = base;
                    if (!existing)
                    {
                        clicked.y--;
                        auto support = block_at(ctx.world,clicked);
                        if (!support || !clean() || support.isOpaque.call() != JNI_TRUE || !clean()) continue;
                    }
                    const int crystal = crystal_at(scene,base);
                    // Reusing an existing crystal is allowed; no occupied site
                    // is used for a NEW placement.
                    const bool clear = clear_site(ctx,base,scene,existing);
                    if (!clear && !(existing && crystal >= 0)) continue;
                    if (!reachable_top(ctx,eye,clicked,block_reach)) continue;
                    auto crystal_box = logic::crystal_volume(base);
                    crystal_box.min.x -= .5; crystal_box.max.x += .5;
                    crystal_box.min.z -= .5; crystal_box.max.z += .5;
                    if (logic::distance_squared(eye,logic::closest(eye,crystal_box)) > entity_reach*entity_reach) continue;
                    const double candidate = logic::distance_squared({x,target.min.y,z},{base.x+.5,base.y+1.,base.z+.5}) + (existing ? 0 : .25);
                    if (candidate < score) { score = candidate; best = base; }
                }
        return best;
    }
    int find_item(const maps::DefaultedList& items, const maps::Item& wanted)
    {
        if (!items || !wanted || !clean()) return -1;
        for (int i = 0; i < 9; ++i)
        {
            jni::frame frame{16};
            auto object = items.get.call(jint(i));
            maps::ItemStack stack(object.object_instance,false);
            if (!stack || !clean()) return -1;
            if (stack.isEmpty.call() == JNI_TRUE) continue;
            auto item = stack.item.get();
            if (!item || !clean()) return -1;
            if (item.is_same_object(wanted) && clean()) return i;
        }
        return -1;
    }
}

struct modules::crystal_aura::runtime
{
    crystal_aura_logic::engine engine;
    services::game::overlay_session session;
    maps::Entity target{nullptr,true}, crystal{nullptr,true};
    std::optional<crystal_aura_logic::cell> base;
    std::optional<module_settings::crystal_aura> config;
    std::optional<crystal_aura_logic::action_kind> aimed_action;
    int previous_slot = -1, owned_slot = -1, aim_tick = -1, status = 0;
    int last_target = -1, last_status = 0;
    crystal_aura_logic::phase last_phase = crystal_aura_logic::phase::melee;
    crystal_aura_logic::clock::time_point switch_ready{}, manual_pause{};
    void clear_cycle()
    { engine.reset(); target.clear_ref(); crystal.clear_ref(); base.reset(); aimed_action.reset(); aim_tick = -1; }
    void record()
    { last_target = engine.target(); last_status = status; last_phase = engine.state(); }
    void reset(const services::game::client_context* context = nullptr)
    {
        if (context && context->player && context->interaction && session.matches({context->world,context->player}))
            services::hotbar::restore_if_owned(context->player,context->interaction,previous_slot,owned_slot);
        services::combat::release_crystal_aura();
        clear_cycle(); session.reset(); previous_slot = owned_slot = -1; status = 0;
    }
    bool prepare(services::game::client_context& context, int slot, crystal_aura_logic::point eye,
        crystal_aura_logic::point to, crystal_aura_logic::clock::time_point now, int delay,
        crystal_aura_logic::action_kind kind)
    {
        if (!services::hotbar::valid_slot(slot)) return false;
        auto inventory = context.player.getInventory.call();
        const int selected = services::hotbar::selected_slot(inventory);
        if (selected < 0) return false;
        bool changed = false;
        if (previous_slot < 0) previous_slot = selected;
        if (selected != slot)
        {
            if (!services::hotbar::select(inventory,context.interaction,slot)) return false;
            changed = true;
            switch_ready = now + std::chrono::milliseconds(std::clamp(delay,50,300));
        }
        owned_slot = slot;
        const double dx = to.x-eye.x, dy = to.y-eye.y, dz = to.z-eye.z;
        if (!crystal_aura_logic::finite(to) || dx*dx+dy*dy+dz*dz < 1e-10) return false;
        constexpr double degrees = 180. / std::numbers::pi;
        const float yaw = float(std::atan2(dz,dx)*degrees - 90.);
        const float pitch = float(-std::atan2(dy,std::hypot(dx,dz))*degrees);
        const float current_yaw = context.player.yaw.get(), current_pitch = context.player.pitch.get();
        const int tick = context.player.age.get();
        if (!clean() || !std::isfinite(current_yaw) || !std::isfinite(current_pitch)) return false;
        if (std::abs(std::remainder(current_yaw-yaw,360.f)) > .25f || std::abs(current_pitch-pitch) > .25f)
        {
            // Visible client rotation; let the next normal Minecraft tick send
            // it. No forged movement packets, silent rotation or reach bypass.
            context.player.yaw.set(yaw);
            context.player.pitch.set(std::clamp(pitch,-90.f,90.f));
        }
        if (!aimed_action || *aimed_action != kind)
        { aimed_action = kind; aim_tick = tick; changed = true; }
        return clean() && !changed && now >= switch_ready && tick > aim_tick;
    }
};

modules::crystal_aura::crystal_aura() : module("crystal aura","Combat","crystal_aura"), runtime_(std::make_unique<runtime>()) {}
modules::crystal_aura::~crystal_aura() { services::combat::release_crystal_aura(); }
void modules::crystal_aura::on_disable(::cache&)
{
    services::combat::release_crystal_aura();
    reset_requested_.store(true,std::memory_order_release);
}

void modules::crystal_aura::render_overlay()
{
    namespace logic = crystal_aura_logic;
    if (!platform::jni_context::bind_current_thread())
    { services::combat::release_crystal_aura(); reset_requested_.store(true,std::memory_order_release); return; }
    struct cleanup { ~cleanup() { platform::jni_context::clear_exception(); } } clear_exception;
    jni::frame frame{256};
    auto& state = *runtime_;
    try
    {
        services::game::client_context ctx;
        const bool captured = services::game::capture_client_context(ctx);
        const auto config = settings_.read();
        if (reset_requested_.exchange(false,std::memory_order_acq_rel) || !state.config || *state.config != config)
        { state.reset(captured ? &ctx : nullptr); state.config = config; }
        const auto window = gui::get_window();
        if (!captured || !enabled || !window || GetForegroundWindow() != window || gui::draw || gui::loader_visible ||
            gui::should_unload || ctx.screen || ctx.player.isAlive.call() != JNI_TRUE || !clean())
        { state.reset(captured ? &ctx : nullptr); return; }
        if (state.session.observe({ctx.world,ctx.player}))
        { state.clear_cycle(); state.previous_slot = state.owned_slot = -1; }
        const auto now = logic::clock::now();
        if (now < state.manual_pause) return;
        auto inventory = ctx.player.getInventory.call();
        const int selected = services::hotbar::selected_slot(inventory);
        if (selected < 0) { state.reset(&ctx); return; }
        if (state.owned_slot >= 0 && selected != state.owned_slot)
        {
            // Never restore over a manual slot change or another module's slot.
            state.reset(); state.manual_pause = now + std::chrono::milliseconds(300); return;
        }
        if (services::combat::trigger_owns_attack() || ctx.player.isUsingItem.call() == JNI_TRUE || !clean())
        { state.reset(&ctx); state.status = 3; state.record(); return; }
        logic::point player_position;
        if (!read_position(ctx.player,player_position)) { state.reset(&ctx); return; }
        logic::point eye = player_position;
        eye.y = ctx.player.getEyeY.call();
        const double entity_reach = ctx.player.getEntityInteractionRange.call();
        const double block_reach = ctx.player.getBlockInteractionRange.call();
        if (!clean() || !logic::finite(eye) || !std::isfinite(entity_reach) || !std::isfinite(block_reach) ||
            entity_reach <= 0 || block_reach <= 0)
        { state.reset(&ctx); return; }
        const int id = find_target(ctx,eye,std::min(8.,std::max(entity_reach,block_reach)+1.),state.engine.target(),config);
        auto target = id >= 0 ? ctx.world.getEntityById.call(jint(id)) : maps::Entity{};
        if (!target || !clean() || !eligible(ctx,target,config)) { state.reset(&ctx); state.record(); return; }
        if (!state.target || !state.target.is_same_object(target) || !clean())
        { state.clear_cycle(); state.target = target; }
        logic::bounds target_box;
        if (!read_box(target,target_box)) { state.reset(&ctx); return; }
        auto items = inventory.main.get();
        auto obsidian_item = maps::Items{}.OBSIDIAN.get(), crystal_item = maps::Items{}.END_CRYSTAL.get();
        const int sword = services::inventory::find_hotbar_slot(items,services::inventory::hotbar_item_kind::sword,selected);
        const int obsidian = find_item(items,obsidian_item), crystal_slot = find_item(items,crystal_item);
        if (!clean()) { state.reset(&ctx); return; }
        const bool already_placed_base = state.engine.state() != logic::phase::melee && state.base && crystal_base(ctx.world,*state.base);
        const bool materials = sword >= 0 && (((obsidian >= 0 || already_placed_base) && crystal_slot >= 0) || state.engine.finishing_crystal());
        if (!materials) { state.reset(&ctx); state.status = 1; state.record(); return; }
        if (!services::combat::claim_crystal_aura(id,state.session.ticket()))
        { state.reset(&ctx); state.status = 3; state.record(); return; }
        if (state.engine.state() == logic::phase::melee)
        { state.base.reset(); state.crystal.clear_ref(); }
        std::vector<entity_shape> scene;
        if (state.engine.state() != logic::phase::melee && state.engine.state() != logic::phase::launch)
        {
            if (!scan_scene(ctx,eye,scene)) { state.reset(&ctx); return; }
            // The local player is not guaranteed to be in every custom world's iterable.
            logic::bounds own_box;
            const int own_id = ctx.player.getId.call();
            if (!read_box(ctx.player,own_box) || !clean()) { state.reset(&ctx); return; }
            scene.push_back({own_id,own_box,player_position,false});
            if (!state.base && state.engine.state() == logic::phase::base)
                state.base = plan_site(ctx,eye,target_box,scene,block_reach,entity_reach);
        }
        logic::observation observed;
        observed.valid = true; observed.materials = materials;
        observed.target_id = id; observed.session = state.session.ticket();
        observed.airborne = target.isOnGround.call() != JNI_TRUE;
        observed.target_is_mob = target.is_instance_of<maps::MobEntity>();
        float cooldown = 0;
        observed.can_melee = logic::distance_squared(eye,logic::closest(eye,target_box)) <= entity_reach*entity_reach &&
            services::attack::read_cooldown(ctx.player,cooldown,0.f) && cooldown >= .9f;
        maps::Entity live_crystal;
        if (state.base)
        {
            observed.site_available = true;
            observed.base_ready = crystal_base(ctx.world,*state.base);
            observed.space_clear = clear_site(ctx,*state.base,scene,observed.base_ready);
            observed.crystal_id = crystal_at(scene,*state.base,state.engine.crystal());
            if (observed.crystal_id >= 0)
            {
                live_crystal = ctx.world.getEntityById.call(jint(observed.crystal_id));
                if (!live_crystal || !clean() || (state.crystal && !state.crystal.is_same_object(live_crystal)))
                { observed.crystal_id = -1; live_crystal = maps::Entity{}; }
                else
                {
                    state.crystal = live_crystal;
                    logic::bounds box;
                    logic::point position;
                    if (!read_box(live_crystal,box) || !read_position(live_crystal,position)) { state.reset(&ctx); return; }
                    position.y += 1.;
                    observed.can_break = logic::distance_squared(eye,logic::closest(eye,box)) <= entity_reach*entity_reach && visible(ctx,eye,position);
                }
            }
        }
        if (!clean()) { state.reset(&ctx); return; }
        const auto action = state.engine.next(observed,now,config);
        state.status = state.engine.state() == logic::phase::base && !state.base ? 2 : 0;
        state.record();
        if (!action) return;
        bool submitted = false;
        if (action->kind == logic::action_kind::sword || action->kind == logic::action_kind::break_crystal)
        {
            auto victim = action->kind == logic::action_kind::sword ? target : live_crystal;
            logic::bounds box;
            if (victim && read_box(victim,box))
            {
                const logic::point aim{(box.min.x+box.max.x)*.5,box.min.y+(box.max.y-box.min.y)*.6,(box.min.z+box.max.z)*.5};
                if (state.prepare(ctx,sword,eye,aim,now,config.switch_delay_ms,action->kind))
                    submitted = services::combat::try_crystal_aura_action(id,state.session.ticket(),[&] {
                        auto fresh = services::target::fresh_crosshair_entity(ctx.instance,ctx.player);
                        if (!fresh || !fresh.is_same_object(victim) || !clean() || !eligible(ctx,target,config)) return false;
                        if (action->kind == logic::action_kind::sword &&
                            (!services::attack::read_cooldown(ctx.player,cooldown,0.f) || cooldown < .9f)) return false;
                        if (!services::inventory::slot_matches(items,services::hotbar::selected_slot(inventory),services::inventory::hotbar_item_kind::sword)) return false;
                        const bool attacked = services::attack::attack_entity(ctx.interaction,ctx.player,fresh);
                        if (attacked) ctx.player.resetLastAttackedTicks.call();
                        return attacked && clean();
                    });
            }
        }
        else if (state.base && eligible(ctx,target,config))
        {
            const bool placing_base = action->kind == logic::action_kind::place_obsidian;
            auto clicked = *state.base;
            if (placing_base) --clicked.y;
            // Revalidate space and support immediately before the actual interaction.
            const bool valid_base = placing_base ? air(ctx.world,*state.base) : crystal_base(ctx.world,*state.base);
            if (valid_base && clear_site(ctx,*state.base,scene,!placing_base) &&
                reachable_top(ctx,eye,clicked,block_reach) &&
                state.prepare(ctx,placing_base ? obsidian : crystal_slot,eye,top_aim(clicked),now,config.switch_delay_ms,action->kind))
            {
                auto fresh = ctx.player.raycast.call(block_reach,1.f,JNI_FALSE);
                maps::BlockHitResult hit(fresh.object_instance,false);
                auto hand = maps::Hand{}.MAIN_HAND.get();
                const int live_slot = find_item(items,placing_base ? obsidian_item : crystal_item);
                if (fresh && hand && clean() && hits_top(hit,clicked) && live_slot == services::hotbar::selected_slot(inventory))
                {
                    submitted = services::combat::try_crystal_aura_action(id,state.session.ticket(),[&] {
                        if (!eligible(ctx,target,config)) return false;
                        auto result = ctx.interaction.interactBlock.call(ctx.player,hand,hit);
                        return result && clean() && result.isAccepted.call() == JNI_TRUE && clean();
                    });
                    if (submitted) services::attack::swing_main_hand(ctx.player);
                }
            }
        }
        state.engine.commit(*action,submitted,now,config);
        state.record();
    }
    catch (...)
    {
        platform::jni_context::clear_exception();
        state.reset();
    }
}

void modules::crystal_aura::render_options()
{
    gui::settings::render(settings_,module_settings::crystal_aura::schema());
    const auto& state = *runtime_;
    ImGui::TextWrapped("%s","Choose players, players + mobs, or mobs only for testing. Sword -> airborne -> obsidian -> crystal -> break. Skip Airborne Wait for Mobs allows golem tests without knockback. No self-damage protection; nearby friends can take explosion damage.");
    ImGui::TextDisabled("%s","Last game update (menu pauses the module):");
    const char* status = "Waiting for target";
    if (state.last_target >= 0) status = "Active";
    if (state.last_status == 1) status = "Hotbar needs sword, obsidian and crystals";
    if (state.last_status == 2) status = "Waiting for a valid placement";
    if (state.last_status == 3) status = "Paused: item use or Triggerbot owns attack";
    ImGui::TextWrapped("%s",status);
    if (state.last_target >= 0)
    {
        using phase = crystal_aura_logic::phase;
        const char* step = "";
        switch (state.last_phase)
        {
        case phase::melee: step = "Sword"; break;
        case phase::launch: step = "Waiting for airborne target"; break;
        case phase::base: step = "Selecting / placing obsidian"; break;
        case phase::wait_base: step = "Checking base block"; break;
        case phase::crystal: step = "Placing crystal"; break;
        case phase::wait_crystal: step = "Waiting for crystal spawn"; break;
        case phase::break_crystal: step = "Breaking crystal"; break;
        case phase::wait_break: step = "Waiting for crystal removal"; break;
        }
        ImGui::TextWrapped("%s: %d | %s","Target",state.last_target,step);
    }
}
