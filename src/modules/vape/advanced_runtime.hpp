#pragma once
#include "advanced_module.hpp"
#include "game_bindings.hpp"
#include "../../services/game/client_context.hpp"
#include <memory>
#include <deque>
#include <limits>
#include <numbers>
#pragma push_macro("min")
#pragma push_macro("max")
#undef min
#undef max
#include "../../../combat cpp/Combat.hpp"
#pragma pop_macro("max")
#pragma pop_macro("min")

namespace vape_advanced
{
    namespace C = Vape::Combat;
    using vec = Vape::Math::Vector3;
    using box = Vape::Math::BoundingBox;
    using angle = Vape::Math::Angle;
    struct body {
        maps::Entity entity{};
        int id=-1,armor=0,hurt=0,resistance=0,blast=0,protection=0;
        vec position{},eye{},motion{}; box bounds{};
        float health=0,absorption=0,toughness=0,yaw=0,pitch=0;
        bool blocking=false,swinging=false;
    };
    struct weapon_slots {
        int sword=-1,axe=-1,mace=-1,density=-1,breach=-1,chest=-1,crystal=-1,obsidian=-1;
    };
    struct frame {
        services::game::client_context game;
        body self;
        maps::GameOptions options{};
        bool grounded=false,liquid=false,left=false,right=false,using_item=false;
        int age=0,slot=-1;
        float cooldown=0,fall=0,sensitivity=.5f,strafe=0;
    };
    struct engine_set {
        C::Velocity velocity; C::KnockbackDelay knockback; C::JumpReset jump; C::WTap wtap;
        C::ShieldBreaker shield; C::AutoMace mace; C::HitSwap swap; C::HitSelect select;
        C::BlockHit block; C::Reach reach; C::NoItemRelease release; C::HitFlick flick;
        C::SilentAura aura; C::CrystalAura crystal; C::RotationManager rotation;
    };
    struct published { bool enabled=false;configuration config{};std::uint64_t revision=0; };
    struct lease { kind owner=kind::count;int original=-1,owned=-1; };
    struct runtime {
        jni::object_wrapper world{nullptr,true},player{nullptr,true};
        engine_set engines;
        lease slot;
        int shield_target=-1,swap_target=-1,flick_target=-1,prev_hurt=0,last_tick=-1,last_attack=-1;
        int chest_hotbar=-1;
        maps::ItemStack equipped_chest{nullptr,true},stored_elytra{nullptr,true};
        bool shield_double=false,forward_owned=false,forward_forced=false,sprint_paused=false,block_owned=false;
        std::deque<jni::object_wrapper> blink_packets;
        std::chrono::steady_clock::time_point blink_started{};
        std::size_t crystal_scan=0;
        int reach_tick=-1;double sampled_reach=3;
        bool aura_engaged=false,crystal_engaged=false;
        std::chrono::steady_clock::time_point decision_time{};
        bool jump_after_velocity=false;
        enum class crystal_wait { none,obsidian,spawn,removal };
        crystal_wait crystal_pending=crystal_wait::none;
        vec crystal_base{};
        int crystal_entity=-1,crystal_started=-1;
    };
    bool read_frame(frame&,bool allow_menu=false,const char** error=nullptr);
    bool read_body(const maps::Entity&,body&,bool detailed=false);
    bool valid_target(const maps::Entity&,const frame&);
    maps::Entity entity_by_id(const frame&,int);
    std::vector<body> targets(const frame&,bool detailed=false);
    weapon_slots hotbar(const frame&,const body* target=nullptr);
    std::string key(const maps::ItemStack&);
    C::WeaponScoreInfo weapon(const maps::ItemStack&);
    bool physical_binding(const maps::KeyBinding&);
    bool send(const frame&,const maps::Packet&);
    bool look(const frame&,angle);
    bool attack(frame&,runtime&,const maps::Entity&,bool extra=false);
    bool select_slot(frame&,runtime&,kind,int);
    void restore_slot(frame&,runtime&,kind);
    void restore_input(frame&,runtime&);
    void flush_blink(const frame&,runtime&,bool discard=false);
    bool enabled(kind);
    bool bypassing();
    struct bypass { bypass();~bypass(); };
    void post_attack(frame&,runtime&);
    void block_action(frame&,runtime&,C::BlockHit::Action);
    void crystal_tick(frame&,runtime&);
    bool crystal_danger(const frame&,vec,float& self_damage,float& target_damage,const std::vector<body>& enemies);
    void reach_update(frame&,runtime&);
    bool reach_confirms(frame&,runtime&,const maps::Entity&);
    bool prepare_attack(frame&,runtime&,const body&,bool allow_flick=true);
}
