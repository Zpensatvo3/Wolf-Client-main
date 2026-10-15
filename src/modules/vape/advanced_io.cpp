#include "advanced_runtime.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../platform/adapter/game_adapter.hpp"
#include "../../services/game/attack_service.hpp"
#include "../../services/game/hotbar_service.hpp"
#include "../../services/input/input_service.hpp"
#include "../../friends/friends.hpp"
#include "../../gui/gui.hpp"
#include <map>
#include <cmath>

namespace vape_advanced
{
    namespace { thread_local unsigned bypass_depth=0; }
    bypass::bypass(){++bypass_depth;} bypass::~bypass(){--bypass_depth;}
    bool bypassing(){return bypass_depth!=0;}
    bool finite(vec v){return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
    std::string key(const maps::ItemStack& stack)
    {
        if(!stack||stack.isEmpty.call()==JNI_TRUE) return {};
        auto item=stack.item.get(); if(!item)return{};
        auto name=item.getTranslationKey.call();return name?name.to_string():std::string{};
    }
    std::map<std::string,int> enchants(const maps::ItemStack& stack)
    {
        std::map<std::string,int> result;
        if(!stack||stack.isEmpty.call()==JNI_TRUE)return result;
        auto component=vape_bindings::Components{}.enchantments.get();
        if(!component||platform::jni_context::clear_exception())return result;
        auto raw=stack.getComponent.call(component);
        if(!raw||platform::jni_context::clear_exception())return result;
        vape_bindings::Enchantments enchant(raw.object_instance,false);
        if(!enchant.entries.is_valid()||!enchant.level.is_valid())return result;
        auto set=enchant.entries.call();if(!set)return result;
        auto array=set.toArray.call();if(!array)return result;
        for(const auto& object:array.to_vector()) {
            vape_bindings::Entry entry(object.object_instance,false);
            auto optional=entry.key.call();if(!optional||optional.present.call()!=JNI_TRUE)continue;
            auto raw_key=optional.get.call();if(!raw_key)continue;
            auto id=vape_bindings::RegistryKey(raw_key.object_instance,false).value.call();if(!id)continue;
            auto text=id.toString.call();if(!text)continue;
            result[text.to_string()]=enchant.level.call(maps::RegistryEntry(object.object_instance,false));
        }
        return result;
    }
    C::WeaponScoreInfo weapon(const maps::ItemStack& stack)
    {
        C::WeaponScoreInfo out;const auto name=key(stack);
        out.isSword=name.ends_with("_sword");out.isAxe=name.ends_with("_axe");out.isMace=name.ends_with(".mace");
        if(name.find("netherite")!=std::string::npos)out.material=C::ItemMaterial::NETHERITE;
        else if(name.find("diamond")!=std::string::npos)out.material=C::ItemMaterial::DIAMOND;
        else if(name.find("iron")!=std::string::npos)out.material=C::ItemMaterial::IRON;
        else if(name.find("golden")!=std::string::npos)out.material=C::ItemMaterial::GOLDEN;
        else if(name.find("stone")!=std::string::npos)out.material=C::ItemMaterial::STONE;
        else if(name.find("wooden")!=std::string::npos)out.material=C::ItemMaterial::WOODEN;
        auto levels=enchants(stack);
        out.sharpnessLevel=levels["minecraft:sharpness"];out.densityLevel=levels["minecraft:density"];
        out.breachLevel=levels["minecraft:breach"];out.fireAspectLevel=levels["minecraft:fire_aspect"];
        out.knockbackLevel=levels["minecraft:knockback"];return out;
    }
    bool read_body(const maps::Entity& entity,body& out,bool detailed)
    {
        // This is a player snapshot, not a generic entity snapshot. A JNI cast
        // does not check Java inheritance: calling Player/LivingEntity methods
        // on an EndCrystalEntity can crash the VM instead of throwing Java.
        if(!entity||!entity.is_instance_of<maps::PlayerEntity>()||
            platform::jni_context::clear_exception()||
            !entity.getEyeY.is_valid()||!entity.getVelocity.is_valid())return false;
        out={};
        auto b=entity.get_bounding_box();auto v=entity.getVelocity.call();
        if(!b||!v||platform::jni_context::clear_exception())return false;
        const auto p=entity.get_position();out.position={p.x,p.y,p.z};out.eye={p.x,entity.getEyeY.call(),p.z};
        out.motion={v.x.get(),v.y.get(),v.z.get()};
        out.bounds={{b.minX.get(),b.minY.get(),b.minZ.get()},{b.maxX.get(),b.maxY.get(),b.maxZ.get()}};
        out.entity=entity;out.id=entity.getId.call();out.yaw=entity.yaw.get();out.pitch=entity.pitch.get();
        vape_bindings::Player living(entity.object_instance,false);
        if(!living.getHealth.is_valid()||!living.getArmor.is_valid()||
            !living.hurtTime.is_valid()||!living.isBlocking.is_valid()||
            !living.handSwinging.is_valid())return false;
        out.health=living.getHealth.call();out.armor=living.getArmor.call();out.hurt=living.hurtTime.get();
        out.blocking=living.isBlocking.call()==JNI_TRUE;out.swinging=living.handSwinging.get()==JNI_TRUE;
        auto attribute=vape_bindings::Attributes{}.toughness.get();
        if(!attribute||!living.attribute.is_valid())return false;
        out.toughness=static_cast<float>(living.attribute.call(attribute));
        if(detailed) {
            auto resistance=vape_bindings::Effects{}.resistance.get();
            if(!resistance||!living.effect.is_valid())return false;
            out.absorption=living.absorption.call();
            auto effect=living.effect.call(resistance);
            out.resistance=effect?vape_bindings::Effect(effect.object_instance,false).amplifier.call()+1:0;
            auto inventory=living.getInventory.call();auto armor=inventory?inventory.armor.get():maps::DefaultedList{};
            if(!armor)return false;
            for(int i=0;i<4;++i) {
                auto object=armor.get.call(i);if(!object)continue;
                auto levels=enchants(maps::ItemStack(object.object_instance,false));
                out.blast+=levels["minecraft:blast_protection"];
                out.protection+=levels["minecraft:protection"];
            }
        }
        return !platform::jni_context::clear_exception()&&finite(out.position)&&finite(out.eye)&&finite(out.motion)&&
            finite(out.bounds.min)&&finite(out.bounds.max)&&std::isfinite(out.yaw)&&std::isfinite(out.pitch)&&
            std::isfinite(out.health)&&std::isfinite(out.toughness)&&std::isfinite(out.absorption);
    }
    bool physical_binding(const maps::KeyBinding& binding)
    {
        if(!binding)return false;
        auto bound=binding.boundKey.get();if(!bound)return false;
        auto type=bound.getCategory.call();const int code=bound.getCode.call();
        if(!type)return false;
        if(type.is_same_object(maps::InputKeyType{}.SCANCODE.get()))
            return code>0&&code<512&&services::input::physical_scan_down(code);
        if(type.is_same_object(maps::InputKeyType{}.MOUSE.get())) {
            if(code==0)return services::input::physical_button_down(services::input::mouse_button::left);
            if(code==1)return services::input::physical_button_down(services::input::mouse_button::right);
            return false;
        }
        unsigned vk=0;
        if(code>=32&&code<=96)vk=code;
        else if(code>=290&&code<=313)vk=VK_F1+code-290;
        else switch(code) {
        case 256:vk=VK_ESCAPE;break;case 257:vk=VK_RETURN;break;case 258:vk=VK_TAB;break;
        case 262:vk=VK_RIGHT;break;case 263:vk=VK_LEFT;break;case 264:vk=VK_DOWN;break;case 265:vk=VK_UP;break;
        case 340:vk=VK_LSHIFT;break;case 344:vk=VK_RSHIFT;break;case 341:vk=VK_LCONTROL;break;
        case 345:vk=VK_RCONTROL;break;case 342:vk=VK_LMENU;break;case 346:vk=VK_RMENU;break;
        default:break;
        }
        return vk&&services::input::physical_key_down(vk);
    }
    bool read_frame(frame& f,bool allow_menu,const char** error)
    {
        if(error)*error=nullptr;
        auto fail=[&](const char* reason){if(error)*error=reason;return false;};
        if(!platform::jni_context::bind_current_thread()||(!allow_menu&&gui::should_unload)||gui::loader_visible)return false;
        const auto window=gui::get_window();
        if(!window||(!allow_menu&&(GetForegroundWindow()!=window||gui::draw)))return false;
        if(!services::game::capture_client_context(f.game)||(!allow_menu&&f.game.screen))return false;
        auto camera=f.game.instance.getCameraEntity.call();
        if(!camera||!camera.is_same_object(f.game.player)||f.game.player.isAlive.call()!=JNI_TRUE)return false;
        f.options=f.game.instance.options.get();if(!f.options)return fail("Vape: game options unavailable.");
        if(!read_body(maps::Entity(f.game.player.object_instance,false),f.self))return fail("Vape: player health/armor snapshot unavailable.");
        f.age=f.game.player.age.get();f.grounded=f.game.player.isOnGround.call()==JNI_TRUE;
        vape_bindings::Player p(f.game.player.object_instance,false);
        f.liquid=p.inLava.call()==JNI_TRUE||p.isTouchingWater.call()==JNI_TRUE;
        f.using_item=p.isUsingItem.call()==JNI_TRUE;
        f.fall=p.fallDistance.get();f.cooldown=p.getAttackCooldownProgress.call(.5f);
        f.left=services::input::physical_button_down(services::input::mouse_button::left);
        f.right=services::input::physical_button_down(services::input::mouse_button::right);
        f.strafe=(physical_binding(f.options.rightKey.get())?1.f:0.f)-(physical_binding(f.options.leftKey.get())?1.f:0.f);
        auto inventory=p.getInventory.call();if(!inventory)return fail("Vape: player inventory unavailable.");
        f.slot=services::hotbar::selected_slot(inventory);
        auto setting=f.options.getMouseSensitivity.call();auto raw=setting?setting.getValue.call():maps::Object{};
        if(!raw)return fail("Vape: mouse sensitivity unavailable.");
        f.sensitivity=static_cast<float>(maps::Double(raw.object_instance,false).doubleValue.call());
        if(platform::jni_context::clear_exception()||!std::isfinite(f.fall)||!std::isfinite(f.cooldown)||
            !std::isfinite(f.sensitivity)||f.slot<0||f.slot>8)return fail("Vape: invalid cooldown/input/slot snapshot.");
        return true;
    }
    bool valid_target(const maps::Entity& entity,const frame& f)
    {
        if(!entity||!entity.is_instance_of<maps::PlayerEntity>()||entity.is_same_object(f.game.player)||
            entity.isAlive.call()!=JNI_TRUE||entity.isInvisible.call()==JNI_TRUE||friends::is_friend(entity))return false;
        vape_bindings::Player self(f.game.player.object_instance,false);
        return self.canSee.call(entity)==JNI_TRUE&&!platform::jni_context::clear_exception();
    }
    maps::Entity entity_by_id(const frame& f,int id)
    {
        if(id<0)return {};
        return vape_bindings::World(f.game.world.object_instance,false).entity.call(id);
    }
    std::vector<body> targets(const frame& f,bool detailed)
    {
        std::vector<body> result;
        auto players=platform::adapter::get().get_players(f.game.world);if(!players)return result;
        auto array=players.toArray.call();if(!array)return result;
        for(const auto& object:array.to_vector()) {
            jni::frame refs{128};maps::Entity entity(object.object_instance,false);
            body snapshot;
            if(valid_target(entity,f)&&read_body(entity,snapshot,detailed))result.push_back(snapshot);
            platform::jni_context::clear_exception();
        }
        return result;
    }
    weapon_slots hotbar(const frame& f,const body* target)
    {
        weapon_slots out;
        auto inventory=f.game.player.getInventory.call();auto items=inventory?inventory.main.get():maps::DefaultedList{};
        if(!items)return out;
        float best_sword=-1,best_axe=-1,best_mace=-1;int density=-1,breach=-1;
        for(int i=0;i<9;++i) {
            jni::frame refs{96};auto object=items.get.call(i);if(!object)continue;
            maps::ItemStack stack(object.object_instance,false);const auto name=key(stack);
            if(name.ends_with(".end_crystal"))out.crystal=i;
            if(name.ends_with(".obsidian"))out.obsidian=i;
            if(name.ends_with("_chestplate"))out.chest=i;
            auto info=weapon(stack);
            const auto score=C::ItemStackScoreUtil::scoreWeapon(info,f.fall,target?static_cast<float>(target->armor):0.f,target?target->toughness:0.f,target&&target->blocking);
            if(platform::jni_context::clear_exception())continue;
            if(info.isSword&&score>best_sword){out.sword=i;best_sword=score;}
            if(info.isAxe&&score>best_axe){out.axe=i;best_axe=score;}
            if(info.isMace) {
                if(score>best_mace){out.mace=i;best_mace=score;}
                if(info.densityLevel>density){density=info.densityLevel;out.density=i;}
                if(info.breachLevel>breach){breach=info.breachLevel;out.breach=i;}
            }
        }
        // -2 represents a real offhand crystal; -1 still means unavailable.
        if(key(f.game.player.getOffHandStack.call()).ends_with(".end_crystal"))out.crystal=-2;
        return out;
    }
    bool send(const frame& f,const maps::Packet& packet)
    {
        if(!packet)return false;
        auto handler=f.game.instance.getNetworkHandler.call();if(!handler)return false;
        bypass guard;vape_bindings::Network network(handler.object_instance,false);
        if(!network.send.is_valid())return false;
        network.send.call(packet);return !platform::jni_context::clear_exception();
    }
    bool look(const frame& f,angle a)
    {
        if(!std::isfinite(a.yaw)||!std::isfinite(a.pitch)||std::abs(a.pitch)>90)return false;
        auto packet=vape_bindings::construct<vape_bindings::FullMove>(&vape_bindings::FullMove::constructor,
            f.self.position.x,f.self.position.y,f.self.position.z,a.yaw,a.pitch,static_cast<jboolean>(f.grounded));
        return packet&&send(f,maps::Packet(packet.object_instance,false));
    }
    bool select_slot(frame& f,runtime& r,kind owner,int slot)
    {
        if(slot<0||slot>8||(r.slot.owner!=kind::count&&r.slot.owner!=owner))return false;
        if(r.slot.owner==owner&&f.slot!=r.slot.owned)return false;
        if(r.slot.owner==kind::count)r.slot={owner,f.slot,f.slot};
        if(!services::hotbar::select(f.game.player,f.game.interaction,slot))return false;
        r.slot.owned=slot;f.slot=slot;return true;
    }
    void restore_slot(frame& f,runtime& r,kind owner)
    {
        if(r.slot.owner!=owner)return;
        services::hotbar::restore_if_owned(f.game.player,f.game.interaction,r.slot.original,r.slot.owned);
        r.slot={};auto inventory=f.game.player.getInventory.call();
        if(inventory)f.slot=services::hotbar::selected_slot(inventory);
    }
    bool attack(frame& f,runtime& r,const maps::Entity& entity,bool extra)
    {
        if(!entity||gui::draw||gui::should_unload||(!extra&&r.last_attack==f.age)||
            f.game.player.isUsingItem.call()==JNI_TRUE)return false;
        if(entity.isAlive.call()!=JNI_TRUE||platform::jni_context::clear_exception())return false;
        bool result=false;
        {bypass guard;result=services::attack::attack_entity(f.game.interaction,f.game.player,entity,true);}
        if(result) {
            f.game.player.resetLastAttackedTicks.call();r.last_attack=f.age;
            if(!extra&&entity.is_instance_of<maps::PlayerEntity>())post_attack(f,r);
        }
        return result&&!platform::jni_context::clear_exception();
    }
    void restore_input(frame& f,runtime& r)
    {
        if(r.forward_owned||r.forward_forced) {
            auto forward=f.options.forwardKey.get();
            if(forward)forward.setPressed.call(physical_binding(forward)?JNI_TRUE:JNI_FALSE);
        }
        r.forward_owned=r.forward_forced=false;
        // A paused sprint is never re-enabled unless the physical forward key is still held.
        if(r.sprint_paused&&physical_binding(f.options.forwardKey.get())&&!f.using_item)
            f.game.player.setSprinting.call(JNI_TRUE);
        r.sprint_paused=false;
    }
    void flush_blink(const frame& f,runtime& r,bool discard)
    {
        if(!discard) for(auto& packet:r.blink_packets)send(f,maps::Packet(packet.object_instance,false));
        r.blink_packets.clear();r.blink_started={};
    }
}
