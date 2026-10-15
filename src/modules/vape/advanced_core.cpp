#include "advanced_runtime.hpp"
#include "../../java_hook/java_hook.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/target_service.hpp"
#include "../../services/game/attack_coordinator.hpp"
#include "../../services/game/hotbar_service.hpp"
#include "../../services/input/input_service.hpp"
#include "../core/module_registry.hpp"
#include "../../gui/gui.hpp"
#include <algorithm>
#include <atomic>

namespace vape_advanced
{
    namespace {
        std::array<published,static_cast<int>(kind::count)> requested{};
        std::unique_ptr<runtime> state;
        DWORD client_thread=0;
        std::string runtime_issue;
        bool same_session(const frame& f) {
            return state&&state->world.is_same_object(f.game.world)&&state->player.is_same_object(f.game.player);
        }
        bool wolf_attack() {
            for(const char* id:{"triggerbot","left_clicker","shield_breaker","mace_swap","hit_crystal","anchor_macro","blatant_anchor_macro","safe_anchor"}) {
                const auto* m=modules::find_module_by_id(id);if(m&&m->enabled)return true;
            }
            // Do not re-enter attack_coordinator's mutex from a vanilla attack
            // invoked inside try_unowned_attack. Enabled Wolf modules already
            // establish precedence above.
            return false;
        }
        double n(kind k,const char* key){return value(k,requested[static_cast<int>(k)].config,key);}
        int ni(kind k,const char* key){return static_cast<int>(n(k,key));}
        long long nl(kind k,const char* key){return static_cast<long long>(n(k,key));}
        void configure(kind k,runtime& r) {
            auto& e=r.engines;
            switch(k) {
            case kind::velocity:e.velocity.horizontalPercent=n(k,"horizontal");e.velocity.verticalPercent=n(k,"vertical");e.velocity.chance=n(k,"chance");
                e.velocity.waterCheck=n(k,"water");e.velocity.kiteMode=n(k,"kite");e.velocity.kiteHorizontal=n(k,"kite_horizontal");e.velocity.kiteVertical=n(k,"kite_vertical");break;
            case kind::knockback:e.knockback.chance=n(k,"chance");e.knockback.groundDelayMinMs=nl(k,"ground_min");e.knockback.groundDelayMaxMs=(std::max)(nl(k,"ground_min"),nl(k,"ground_max"));
                e.knockback.airDelayMinMs=nl(k,"air_min");e.knockback.airDelayMaxMs=(std::max)(nl(k,"air_min"),nl(k,"air_max"));e.knockback.waterCheck=n(k,"water");break;
            case kind::jump:e.jump.chance=n(k,"chance");e.jump.accuracy=n(k,"accuracy");e.jump.delayTicks=ni(k,"delay");e.jump.waterCheck=n(k,"water");e.jump.minHorizontalVelocity=n(k,"minimum");break;
            case kind::wtap:e.wtap.mode=static_cast<C::WTap::Mode>(static_cast<int>(n(k,"mode")));break;
            case kind::shield:e.shield.swapDelayTicks=ni(k,"delay");e.shield.swapBackDelayTicks=ni(k,"restore");e.shield.doubleClick=n(k,"double");e.shield.maxBreakReach=n(k,"range");break;
            case kind::mace:e.mace.maceSelection=static_cast<C::AutoMace::MaceType>(static_cast<int>(n(k,"mode")));e.mace.autoUnequipElytra=n(k,"unequip");e.mace.reEquipElytra=n(k,"reequip");
                e.mace.stunSlam=n(k,"stun");e.mace.stunSlamChance=n(k,"chance");e.mace.smashOnly=n(k,"smash_only");e.mace.minFallDistance=n(k,"fall");e.mace.aimRange=n(k,"range");break;
            case kind::swap:e.swap.swapAxes=n(k,"axes");e.swap.swapMaces=n(k,"maces");e.swap.stunSlam=n(k,"stun");e.swap.autoRestore=n(k,"restore");break;
            case kind::select:e.select.mode=static_cast<C::HitSelect::Mode>(static_cast<int>(n(k,"mode")));e.select.chance=n(k,"chance");break;
            case kind::block:e.block.mode=static_cast<C::BlockHit::Mode>(static_cast<int>(n(k,"mode")));e.block.requireMouseDown=n(k,"mouse");e.block.maxDistance=n(k,"range");e.block.maxAngle=n(k,"fov");
                e.block.maximumHurtTimeMs=n(k,"hurt");e.block.includePing=n(k,"ping");e.block.holdAfterTicks=ni(k,"hold");break;
            case kind::reach:e.reach.minRange=n(k,"min");e.reach.maxRange=(std::max)(n(k,"min"),n(k,"max"));e.reach.chance=n(k,"chance");e.reach.onlyWhileSprinting=n(k,"sprint");
                e.reach.disableInWater=n(k,"water");e.reach.verticalCheck=n(k,"vertical");e.reach.maxVerticalDiff=n(k,"height");e.reach.misplace=n(k,"misplace");break;
            case kind::release:e.release.onlyInGui=n(k,"gui");e.release.bows=n(k,"bows");e.release.shields=n(k,"shields");e.release.foodAndPotions=n(k,"food");break;
            case kind::flick:e.flick.maxFlickAngle=n(k,"angle");e.flick.reach=n(k,"range");e.flick.chance=n(k,"chance");e.flick.blink=n(k,"blink");e.flick.selectHits=n(k,"select");
                e.flick.randomizeOffset=n(k,"random");e.flick.randomizeOffsetRange=n(k,"offset");e.flick.strafeInvert=n(k,"invert");break;
            case kind::aura:e.aura.reach=n(k,"range");e.aura.extraSwingDistance=n(k,"extra");e.aura.maxFov=n(k,"fov");
                e.aura.priority=static_cast<C::SilentAura::Priority>(static_cast<int>(n(k,"priority")));e.aura.targetArea=static_cast<C::SilentAura::TargetArea>(static_cast<int>(n(k,"area")));
                e.aura.aimSpeed=n(k,"speed");e.aura.minCooldownProgress=static_cast<float>(n(k,"cooldown"));e.aura.perfectSwing=n(k,"perfect");e.aura.shieldCheck=n(k,"shield");e.aura.requireMouseDown=n(k,"mouse");e.aura.humanJitter=n(k,"jitter");break;
            case kind::crystal:e.crystal.placeReach=n(k,"place");e.crystal.breakReach=n(k,"break");e.crystal.minDamage=n(k,"damage");e.crystal.maxSelfDamage=n(k,"self");
                e.crystal.preventSuicide=n(k,"suicide");e.crystal.autoObsidian=n(k,"obsidian");break;
            default:break;
            }
        }
        void chest_action(frame&,runtime&,bool);
        void cancel(kind k,frame* f) {
            if(!state)return;auto& r=*state;
            if(f) {
                restore_slot(*f,r,k);
                if(k==kind::flick||k==kind::wtap)restore_input(*f,r);
                if(k==kind::flick) {flush_blink(*f,r);look(*f,{f->self.yaw,f->self.pitch});}
                if(k==kind::block)block_action(*f,r,C::BlockHit::Action::STOP_BLOCK);
                if(k==kind::mace&&r.chest_hotbar>=0&&r.engines.mace.reEquipElytra)chest_action(*f,r,true);
            }
            switch(k) {
            case kind::velocity:r.engines.velocity=C::Velocity{};break;
            case kind::knockback:r.engines.knockback.clear();break;
            case kind::jump:r.engines.jump.reset();r.jump_after_velocity=false;break;
            case kind::wtap:r.engines.wtap=C::WTap{};break;
            case kind::shield:r.engines.shield.reset();r.shield_target=-1;r.shield_double=false;break;
            case kind::mace:r.engines.mace.reset();r.chest_hotbar=-1;r.equipped_chest.clear_ref();r.stored_elytra.clear_ref();break;
            case kind::swap:r.engines.swap.reset();r.swap_target=-1;break;
            case kind::select:r.engines.select=C::HitSelect{};break;
            case kind::block:r.engines.block.reset();break;
            case kind::reach:r.reach_tick=-1;break;
            case kind::flick:r.engines.flick.reset();r.flick_target=-1;r.engines.rotation.clearSilentRotations();break;
            case kind::aura:r.engines.aura.reset();r.engines.rotation.clearSilentRotations();r.aura_engaged=false;break;
            case kind::crystal:r.crystal_engaged=false;r.crystal_pending=runtime::crystal_wait::none;r.crystal_entity=-1;break;
            default:break;
            }
        }
        void jump(frame& f) {
            if(!f.grounded||f.using_item||f.game.player.jumpingCooldown.get()!=0)return;
            auto jump_key=f.options.jumpKey.get();if(physical_binding(jump_key))return;
            f.game.player.jump.call();
        }
        bool cancel_release(const frame& f,runtime& r) {
            if(!enabled(kind::release)||!f.using_item)return false;
            auto stack=f.game.player.getActiveItem.call();auto item=key(stack);C::ItemCategory category=C::ItemCategory::OTHER;
            if(item.ends_with(".bow"))category=C::ItemCategory::BOW;
            else if(item.ends_with(".crossbow"))category=C::ItemCategory::CROSSBOW;
            else if(item.ends_with(".shield"))category=C::ItemCategory::SHIELD;
            else if(item.find("potion")!=std::string::npos)category=C::ItemCategory::POTION;
            else if(stack) {
                auto component=maps::DataComponentTypes{}.FOOD.get();
                if(component&&stack.getComponent.call(component))category=C::ItemCategory::FOOD;
            }
            return !platform::jni_context::clear_exception()&&r.engines.release.shouldCancelRelease(f.game.screen||gui::draw,category);
        }
        bool allowed_attack(const frame& f,const body& target,double range) {
            return valid_target(target.entity,f)&&f.self.eye.distanceTo(target.bounds.getClosestPoint(f.self.eye))<=range;
        }
        void wtap_action(frame& f,runtime& r,C::WTap::Action action) {
            if(action==C::WTap::Action::SEND_SPRINT_RESET_PACKETS) {
                auto modes=vape_bindings::CommandMode{};
                for(auto mode:{modes.stop.get(),modes.start.get()}) {
                    auto packet=vape_bindings::construct<vape_bindings::Command>(&vape_bindings::Command::constructor,
                        maps::Entity(f.game.player.object_instance,false),mode);
                    if(packet)send(f,maps::Packet(packet.object_instance,false));
                }
            } else if(action==C::WTap::Action::RELEASE_FORWARD_KEY) {
                auto key=f.options.forwardKey.get();
                if(key&&physical_binding(key)) {key.setPressed.call(JNI_FALSE);r.forward_owned=true;}
            } else if(action==C::WTap::Action::PRESS_FORWARD_KEY) restore_input(f,r);
        }
        void chest_action(frame& f,runtime& r,bool restore) {
            auto inventory=f.game.player.getInventory.call();auto handler=f.game.player.currentScreenHandler.get();
            auto armor=inventory?inventory.armor.get():maps::DefaultedList{};
            auto items=inventory?inventory.main.get():maps::DefaultedList{};
            if(!armor||!items||!handler||handler.syncId.get()!=0)return;
            auto cursor=handler.getCursorStack.call();if(!cursor||cursor.isEmpty.call()!=JNI_TRUE)return;
            int source=r.chest_hotbar;
            if(!restore)source=hotbar(f).chest;
            if(source<0||source>8)return;
            auto chest_object=armor.get.call(2);auto source_object=items.get.call(source);
            maps::ItemStack chest(chest_object.object_instance,false),replacement(source_object.object_instance,false);
            if(!restore) {
                if(!key(chest).ends_with(".elytra")||!key(replacement).ends_with("_chestplate"))return;
            } else {
                if(!r.equipped_chest||!r.stored_elytra||
                    !maps::ItemStack{}.areItemsAndComponentsEqual.call(chest,r.equipped_chest)||
                    !maps::ItemStack{}.areItemsAndComponentsEqual.call(replacement,r.stored_elytra))return;
            }
            auto swap=maps::SlotActionType{}.SWAP.get();if(!swap)return;
            bypass guard;f.game.interaction.clickSlot.call(0,6,source,swap,f.game.player.toPlayerEntity());
            if(!platform::jni_context::clear_exception()) {
                if(!restore) {r.chest_hotbar=source;r.equipped_chest=replacement.copy.call();r.stored_elytra=chest.copy.call();}
                else {r.chest_hotbar=-1;r.equipped_chest.clear_ref();r.stored_elytra.clear_ref();}
            }
        }
        void tick(frame& f,runtime& r) {
            if(r.last_tick==f.age)return;r.last_tick=f.age;r.engines.rotation.onTick();
            r.decision_time=std::chrono::steady_clock::now();
            r.aura_engaged=false;r.crystal_engaged=false;
            if(r.slot.owner!=kind::count&&f.slot!=r.slot.owned) {
                auto owner=r.slot.owner;r.slot={};cancel(owner,&f);
            }
            if(f.self.hurt>r.prev_hurt) {
                if(enabled(kind::block))r.engines.block.onDamaged();
                if(enabled(kind::select)&&!wolf_attack()&&r.engines.select.onDamageTaken()) {
                    auto target=services::target::fresh_crosshair_entity(f.game.instance,f.game.player);body b;
                    if(read_body(target,b)&&allowed_attack(f,b,3.0)&&prepare_attack(f,r,b,false))attack(f,r,target);
                }
            }
            r.prev_hurt=f.self.hurt;
            if(enabled(kind::select))r.engines.select.onTick(f.self.hurt);
            if(enabled(kind::jump)) {
                if(f.liquid||f.using_item)r.engines.jump.reset();
                else if(r.engines.jump.onTick(f.grounded))jump(f);
            }
            if(enabled(kind::knockback)) {
                vec value;
                // One bounded drain, never leave a same-session delayed velocity stuck forever.
                for(int i=0;i<64&&r.engines.knockback.pollReleasedVelocity(value);++i)
                    f.game.player.setVelocity.call(value.x,value.y,value.z);
            }
            if(enabled(kind::wtap))wtap_action(f,r,r.engines.wtap.onTick());
            if(wolf_attack()) {
                for(auto k:{kind::shield,kind::mace,kind::swap,kind::flick,kind::aura,kind::block})cancel(k,&f);
                return;
            }
            auto target=services::target::crosshair_entity(f.game.instance);body current;
            bool has_target=valid_target(target,f)&&read_body(target,current);
            const auto slots=hotbar(f,has_target?&current:nullptr);
            if(enabled(kind::shield)&&r.engines.shield.isActive()) {
                auto enemy=entity_by_id(f,r.shield_target);body b;
                bool valid=read_body(enemy,b)&&allowed_attack(f,b,r.engines.shield.maxBreakReach);
                const auto action=r.engines.shield.onTick(valid&&b.blocking);
                if(action.action==C::ShieldBreaker::Action::RESTORE_ORIGINAL_SLOT)restore_slot(f,r,kind::shield);
                else if(action.attackRequired&&valid) {
                    attack(f,r,enemy);
                    if(action.doubleClickRequired)attack(f,r,enemy,true);
                } else if(!valid)cancel(kind::shield,&f);
            }
            if(enabled(kind::swap)&&r.slot.owner==kind::swap) {
                const auto action=r.engines.swap.onTick(slots.mace);
                if(action.action==C::HitSwap::Action::RESTORE_SLOT)restore_slot(f,r,kind::swap);
                else if(action.targetSlot>=0&&select_slot(f,r,kind::swap,action.targetSlot)&&action.action==C::HitSwap::Action::TRIGGER_ATTACK) {
                    auto enemy=entity_by_id(f,r.swap_target);body b;
                    if(read_body(enemy,b)&&allowed_attack(f,b,3.0))attack(f,r,enemy);
                }
            }
            if(enabled(kind::mace)&&(r.slot.owner==kind::mace||r.slot.owner==kind::count)&&
                (has_target||r.engines.mace.getOriginalSlot()>=0||r.chest_hotbar>=0)) {
                if(has_target&&!allowed_attack(f,current,r.engines.mace.aimRange))has_target=false;
                if(has_target||f.grounded||r.chest_hotbar>=0) {
                    const auto action=r.engines.mace.updateTick(f.fall,f.self.motion.y,
                        f.game.player.isFallFlying.call()==JNI_TRUE,f.grounded,has_target?current.armor:0,
                        has_target&&current.blocking,f.slot,slots.density,slots.breach,slots.axe,slots.chest>=0);
                    using A=C::AutoMace::Action;
                    if(action==A::RESTORE_WEAPON)restore_slot(f,r,kind::mace);
                    else if(action==A::SWAP_TO_DENSITY_MACE) {
                        if(!select_slot(f,r,kind::mace,slots.density))cancel(kind::mace,&f);
                    }
                    else if(action==A::SWAP_TO_BREACH_MACE) {
                        if(!select_slot(f,r,kind::mace,slots.breach))cancel(kind::mace,&f);
                    }
                    else if(action==A::SWAP_TO_AXE_FOR_SHIELD) {
                        if(has_target&&select_slot(f,r,kind::mace,slots.axe))attack(f,r,target);
                        else cancel(kind::mace,&f);
                    } else if(action==A::EQUIP_CHESTPLATE)chest_action(f,r,false);
                    else if(action==A::RE_EQUIP_ELYTRA)chest_action(f,r,true);
                    else if(action==A::EXECUTE_SMASH&&has_target&&f.slot==r.slot.owned)attack(f,r,target);
                }
            }
            if(enabled(kind::flick)&&r.flick_target>=0) {
                auto enemy=entity_by_id(f,r.flick_target);body b;
                if(!read_body(enemy,b)||!allowed_attack(f,b,r.engines.flick.reach))cancel(kind::flick,&f);
                else {
                    const auto action=r.engines.flick.onTick(f.self.eye,f.self.yaw,f.self.pitch,b.bounds);
                    using A=C::HitFlick::ActionType;
                    if(action.action==A::NONE)cancel(kind::flick,&f);
                    if(action.forceForwardKey) {
                        auto key=f.options.forwardKey.get();
                        if(key&&!physical_binding(key)){key.setPressed.call(JNI_TRUE);r.forward_forced=true;}
                    }
                    if(action.cancelSprint&&f.game.player.isSprinting.call()==JNI_TRUE) {
                        f.game.player.setSprinting.call(JNI_FALSE);r.sprint_paused=true;
                    }
                    if(action.action==A::SEND_FLICK_LOOK_PACKET) {
                        r.engines.rotation.setSilentRotations(action.targetAngle.yaw,action.targetAngle.pitch,2);
                        if(!look(f,action.targetAngle))cancel(kind::flick,&f);
                    } else if(action.action==A::SEND_ATTACK_PACKET) {
                        if(prepare_attack(f,r,b,false))attack(f,r,enemy);
                    }
                    else if(action.action==A::SEND_RESTORE_LOOK_PACKET) {
                        flush_blink(f,r);look(f,{f.self.yaw,f.self.pitch});
                        restore_input(f,r);r.engines.rotation.clearSilentRotations();r.flick_target=-1;
                    }
                }
            }
            if(enabled(kind::aura)&&r.flick_target<0&&!f.using_item) {
                auto enemies=targets(f);std::vector<C::CombatTarget> candidates;
                for(const auto& b:enemies)candidates.push_back({b.id,b.bounds,b.motion,b.health,b.armor,b.blocking,false});
                const auto yaw=r.engines.rotation.isSilentActive()?r.engines.rotation.getTargetYaw():f.self.yaw;
                const auto pitch=r.engines.rotation.isSilentActive()?r.engines.rotation.getTargetPitch():f.self.pitch;
                const bool shield_preparation=slots.axe>=0&&(enabled(kind::shield)||enabled(kind::swap));
                const bool holding_axe=key(f.game.player.getMainHandStack.call()).ends_with("_axe");
                const auto action=r.engines.aura.update(f.self.eye,yaw,pitch,f.sensitivity,f.cooldown,f.left,
                    holding_axe||shield_preparation,candidates,.05);
                if(action.targetEntityId>=0) {
                    r.engines.rotation.setSilentRotations(action.targetAngle.yaw,action.targetAngle.pitch,2);
                    const bool rotated=look(f,action.targetAngle);
                    r.aura_engaged=rotated;
                    if(!rotated)r.engines.rotation.clearSilentRotations();
                    auto enemy=entity_by_id(f,action.targetEntityId);body b;
                    if(rotated&&action.shouldAttack&&read_body(enemy,b)&&allowed_attack(f,b,r.engines.aura.reach)) {
                        const auto direction=C::RayTraceUtil::getVectorForRotation(action.targetAngle.yaw,action.targetAngle.pitch);
                        if(C::Reach::rayIntersectsBox(f.self.eye,direction,b.bounds,r.engines.aura.reach)&&prepare_attack(f,r,b,false))attack(f,r,enemy);
                    }
                } else r.engines.rotation.clearSilentRotations();
            }
            if(enabled(kind::crystal)&&!r.aura_engaged&&r.flick_target<0&&r.last_attack!=f.age)crystal_tick(f,r);
            if(enabled(kind::block)) {
                auto& engine=r.engines.block;bool suitable=false;body enemy;
                if(has_target)enemy=current;
                auto off=f.game.player.getOffHandStack.call();auto main=f.game.player.getMainHandStack.call();
                const bool shield=key(off).ends_with(".shield")||key(main).ends_with(".shield");
                if(has_target) {
                    const auto angle=Vape::Math::RotationUtil::getRotationsToCoords(f.self.eye,enemy.eye);
                    suitable=f.self.eye.distanceTo(enemy.bounds.getClosestPoint(f.self.eye))<=engine.maxDistance&&
                        Vape::Math::RotationUtil::getAngleDifference(f.self.yaw,angle.yaw)<=engine.maxAngle*.5;
                }
                long long ping=0;
                auto handler=f.game.instance.getNetworkHandler.call();
                if(handler) {
                    auto name=f.game.player.getEntityName.call();auto entry=name?handler.getPlayerListEntryByName.call(name):maps::PlayerListEntry{};
                    if(entry)ping=entry.getLatency.call();
                    platform::jni_context::clear_exception();
                }
                auto action=engine.onTick(shield&&suitable&&(!engine.requireMouseDown||f.left),f.self.hurt,ping,
                    suitable,has_target?f.self.eye.distanceTo(enemy.eye):999,has_target&&enemy.swinging);
                block_action(f,r,action);
            }
        }
        bool event(JNIEnv*,int code,jobject,jobject a,jobject b) {
            if(GetCurrentThreadId()!=client_thread||bypassing())return false;
            if(code==9) {
                jni::frame refs{256};frame cleanup;
                if(state&&read_frame(cleanup,true)&&same_session(cleanup)) {
                    for(int i=0;i<static_cast<int>(kind::count);++i)cancel(static_cast<kind>(i),&cleanup);
                    flush_blink(cleanup,*state);
                }
                state.reset();return false;
            }
            bool any=false;for(int i=0;i<static_cast<int>(kind::count);++i)any|=enabled(static_cast<kind>(i));
            if(!any)return false;
            jni::frame refs{512};frame f;const char* issue=nullptr;
            if(!read_frame(f,code==5||code==6,&issue)) {
                if(issue)runtime_issue=issue;
                frame cleanup;
                if(state&&read_frame(cleanup,true)&&same_session(cleanup)) {
                    for(int i=0;i<static_cast<int>(kind::count);++i)cancel(static_cast<kind>(i),&cleanup);
                    flush_blink(cleanup,*state);
                }
                state.reset();platform::jni_context::clear_exception();return false;
            }
            runtime_issue.clear();
            if(!same_session(f)) {
                state=std::make_unique<runtime>();state->world=f.game.world;state->player=f.game.player;
                state->prev_hurt=f.self.hurt;
                for(int i=0;i<static_cast<int>(kind::count);++i)configure(static_cast<kind>(i),*state);
            }
            auto& r=*state;
            if(code==0)tick(f,r);
            else if(code==2&&!wolf_attack()) {
                if(!a||!jni::get_env()->IsSameObject(a,f.game.player.object_instance))return false;
                maps::Entity entity(b,false);body target;
                if(valid_target(entity,f)&&read_body(entity,target))return !prepare_attack(f,r,target);
            } else if(code==3&&!wolf_attack()) {
                // Crystal/item-frame/etc. attacks must not run player-only
                // post-hit preparations or reinterpret the target as a player.
                if(a&&jni::get_env()->IsSameObject(a,f.game.player.object_instance)) {
                    r.last_attack=f.age;
                    if(valid_target(maps::Entity(b,false),f))post_attack(f,r);
                }
            } else if(code==4) {
                vape_bindings::VelocityPacket packet(a,false);
                if(!packet||packet.id.call()!=f.self.id)return false;
                vec v{packet.x.call(),packet.y.call(),packet.z.call()};
                if(platform::jni_context::clear_exception()||!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))return false;
                r.jump_after_velocity=enabled(kind::jump)&&!f.using_item&&
                    r.engines.jump.onVelocityPacket(v.x,v.z,f.grounded,f.liquid);
                bool modified=false;
                if(enabled(kind::velocity)) {
                    auto attacker=f.game.player.getAttacker.call();bool away=false;
                    if(attacker) {
                        const auto p=attacker.get_position();
                        const auto angle=Vape::Math::RotationUtil::getRotationsToCoords(f.self.eye,{p.x,p.y,p.z});
                        away=Vape::Math::RotationUtil::getAngleDifference(f.self.yaw,angle.yaw)>90;
                    }
                    const auto result=r.engines.velocity.processVelocity(v.x,v.y,v.z,f.liquid,away);
                    if(result.cancelPacket){r.jump_after_velocity=false;return true;}
                    modified=result.x!=v.x||result.y!=v.y||result.z!=v.z;v={result.x,result.y,result.z};
                }
                if(enabled(kind::knockback)&&r.engines.knockback.onReceiveVelocity(v,f.grounded,f.liquid)) {
                    r.jump_after_velocity=false;return true;
                }
                if(modified) {
                    f.game.player.setVelocity.call(v.x,v.y,v.z);
                    const bool applied=!platform::jni_context::clear_exception();
                    if(applied&&r.jump_after_velocity)jump(f);
                    r.jump_after_velocity=false;return applied;
                }
            } else if(code==8) {
                if(r.jump_after_velocity) {
                    r.jump_after_velocity=false;
                    if(enabled(kind::jump))jump(f);
                }
            } else if(code==5) {
                if(r.block_owned&&r.engines.block.isBlocking()&&enabled(kind::block))return true;
                return cancel_release(f,r);
            } else if(code==6) {
                maps::Packet packet(a,false);if(!packet)return false;
                if(packet.is_instance_of<vape_bindings::ActionPacket>()) {
                    auto action=vape_bindings::ActionPacket(a,false).action.get();
                    auto release=maps::PlayerActionC2SPacket$Action{}.RELEASE_USE_ITEM.get();
                    if(action&&release&&action.is_same_object(release))return cancel_release(f,r);
                }
                if(gui::draw||f.game.screen||!packet.is_instance_of<vape_bindings::MovePacket>())return false;
                if(enabled(kind::flick)&&r.engines.flick.shouldChokePacket()) {
                    auto now=std::chrono::steady_clock::now();
                    if(r.blink_packets.empty())r.blink_started=now;
                    if(r.blink_packets.size()>=32||now-r.blink_started>std::chrono::milliseconds(300))cancel(kind::flick,&f);
                    else {r.blink_packets.emplace_back(a,true);return true;}
                }
                if(enabled(kind::aura)&&r.engines.rotation.isSilentActive()) {
                    vape_bindings::MovePacket movement(a,false);
                    auto replacement=vape_bindings::construct<vape_bindings::FullMove>(&vape_bindings::FullMove::constructor,
                        movement.x.call(f.self.position.x),movement.y.call(f.self.position.y),movement.z.call(f.self.position.z),
                        r.engines.rotation.getTargetYaw(),r.engines.rotation.getTargetPitch(),movement.ground.call());
                    if(replacement&&!platform::jni_context::clear_exception())return send(f,maps::Packet(replacement.object_instance,false));
                }
            } else if(code==7&&enabled(kind::reach)&&!wolf_attack())reach_update(f,r);
            platform::jni_context::clear_exception();return false;
        }
    }
    bool enabled(kind k) {
        const auto& slot=requested[static_cast<int>(k)];
        auto* module=modules::find_module_by_id(id(k));return slot.enabled&&module&&module->enabled;
    }
    std::string runtime_error(){return runtime_issue;}
    bool owns_attack(){
        if(!java_hook::vape_ready()||!state||GetCurrentThreadId()!=client_thread)return false;
        // Merely enabling Hit Flick must not suppress the very input that
        // starts its sequence. Likewise an idle aura owns no attack channel.
        if(std::chrono::steady_clock::now()-state->decision_time>std::chrono::milliseconds(150))return false;
        return (enabled(kind::flick)&&state->flick_target>=0)||
            (enabled(kind::aura)&&state->aura_engaged)||
            (enabled(kind::crystal)&&state->crystal_engaged);
    }
    bool reach_active(){return java_hook::vape_ready()&&enabled(kind::reach);}
    bool confirm_reach_target(const maps::Entity& target) {
        if(!reach_active()||!state||GetCurrentThreadId()!=client_thread)return false;
        jni::frame refs{256};frame f;
        return read_frame(f)&&same_session(f)&&reach_confirms(f,*state,target);
    }
    void publish(kind k,bool active,const configuration& config,std::uint64_t revision) {
        if(!platform::jni_context::bind_current_thread())return;
        if(!client_thread)client_thread=GetCurrentThreadId();if(GetCurrentThreadId()!=client_thread)return;
        auto& slot=requested[static_cast<int>(k)];
        const bool change=slot.revision!=revision||slot.enabled!=active;
        if(change&&state) {
            jni::frame refs{128};frame f;
            if(read_frame(f,true)&&same_session(f))cancel(k,&f);else cancel(k,nullptr);
            platform::jni_context::clear_exception();
        }
        slot={active,config,revision};if(state&&change)configure(k,*state);
        if(active&&!java_hook::vape_ready()) {
            jni::frame refs{128};services::game::client_context context;
            if(services::game::capture_client_context(context,false,false))java_hook::ensure_vape(jni::get_env(),context.instance.object_instance,&event);
            platform::jni_context::clear_exception();
        }
        // All settings have been published by the final catalog entry. Pump
        // decisions from this known client/render thread as well as Java tick
        // callbacks. The player-age guard prevents double ticks or catch-up hits.
        if(k==kind::crystal&&java_hook::vape_ready())event(jni::get_env(),0,nullptr,nullptr,nullptr);
    }
    void retire(kind k) {
        requested[static_cast<int>(k)].enabled=false;
        bool any=false;for(const auto& entry:requested)any|=entry.enabled;
        if(!any)state.reset();
    }
    void post_attack(frame& f,runtime& r) {
        if(enabled(kind::wtap))wtap_action(f,r,r.engines.wtap.onAttack(f.game.player.isSprinting.call()==JNI_TRUE));
        if(enabled(kind::block))block_action(f,r,r.engines.block.onPostAttack(f.left));
        if(r.shield_double) {
            r.shield_double=false;auto target=entity_by_id(f,r.shield_target);
            if(enabled(kind::shield)&&valid_target(target,f))attack(f,r,target,true);
        }
    }
    void block_action(frame& f,runtime& r,C::BlockHit::Action action) {
        if(action==C::BlockHit::Action::START_BLOCK&&!f.using_item) {
            auto main=f.game.player.getMainHandStack.call(),off=f.game.player.getOffHandStack.call();
            auto hand=key(main).ends_with(".shield")?maps::Hand{}.MAIN_HAND.get():
                key(off).ends_with(".shield")?maps::Hand{}.OFF_HAND.get():maps::Hand{};
            if(!hand)return;
            bypass guard;vape_bindings::Manager manager(f.game.interaction.object_instance,false);
            auto action=manager.use.call(f.game.player.toPlayerEntity(),hand);
            const bool failed=platform::jni_context::clear_exception();
            vape_bindings::Result result(action.object_instance,false);
            r.block_owned=!failed&&result&&result.accepted.is_valid()&&result.accepted.call()==JNI_TRUE&&
                !platform::jni_context::clear_exception();
            if(!r.block_owned)r.engines.block.reset();
        } else if(action==C::BlockHit::Action::STOP_BLOCK&&r.block_owned) {
            if(!f.right) {bypass guard;vape_bindings::Manager(f.game.interaction.object_instance,false).stopUsing.call(f.game.player.toPlayerEntity());}
            r.block_owned=false;
        }
    }
    bool prepare_attack(frame& f,runtime& r,const body& target,bool allow_flick) {
        if(enabled(kind::select)&&r.engines.select.shouldCancelPrematureAttack())return false;
        if(enabled(kind::flick)&&allow_flick&&!enabled(kind::aura)) {
            if(r.flick_target>=0)return false;
            if(allowed_attack(f,target,r.engines.flick.reach)&&r.engines.flick.onAttackInput(target.hurt==0,f.strafe,f.self.yaw)) {
                r.flick_target=target.id;return false;
            }
        }
        const auto slots=hotbar(f,&target);
        if(enabled(kind::shield)&&target.blocking) {
            if(r.engines.shield.isActive())return false;
            if(r.slot.owner!=kind::count&&r.slot.owner!=kind::shield)return false;
            const auto decision=r.engines.shield.onAttackAttempt(true,f.self.eye.distanceTo(target.bounds.getClosestPoint(f.self.eye)),f.slot,slots.axe);
            if(decision.action!=C::ShieldBreaker::Action::NONE) {
                r.shield_target=target.id;
                if(decision.action!=C::ShieldBreaker::Action::EXECUTE_DOUBLE_ATTACK&&
                    !select_slot(f,r,kind::shield,decision.slotToSelect)) {cancel(kind::shield,&f);return false;}
                r.shield_double=decision.attackRequired&&decision.doubleClickRequired;
                return decision.attackRequired;
            }
        }
        if(enabled(kind::swap)&&(r.slot.owner==kind::count||r.slot.owner==kind::swap)) {
            const auto decision=r.engines.swap.onPreAttack(f.slot,target.blocking,f.fall,target.armor,slots.sword,slots.axe,slots.mace);
            if(decision.action==C::HitSwap::Action::SWAP_SLOT) {
                r.swap_target=target.id;
                if(!select_slot(f,r,kind::swap,decision.targetSlot)) {cancel(kind::swap,&f);return false;}
            }
        }
        return true;
    }
}
