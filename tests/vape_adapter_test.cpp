#include "../../combat cpp/aimassist/AimAssist.hpp"
#include "../../combat cpp/combat/SilentAura.hpp"
#include "../../combat cpp/combat/CrystalAura.hpp"
#include "../../combat cpp/combat/Triggerbot.hpp"
#include "../../combat cpp/movement/JumpReset.hpp"
#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>

// Small, offline decision check. No game process, injection, server or inputs.
int main()
{
    using namespace Vape;
    const Math::Vector3 eye{0,1.62,0};
    const Math::BoundingBox bounds{{1.7,0,2.7},{2.3,1.8,3.3}};
    Combat::AimAssist aim;
    aim.requireMouseDown=false;aim.weaponsOnly=false;
    const std::vector<Combat::AimTargetCandidate> targets{{7,bounds}};
    float yaw=0,pitch=0;
    const auto* selected=aim.findBestTarget(eye,yaw,targets);
    assert(selected&&selected->entityId==7);
    for(int frame=0;frame<120;++frame) {
        aim.updateFrame(eye,yaw,pitch,0,false,false,false,selected,{},true,1./120);
        aim.applyRenderTick(yaw,pitch,.5f);
    }
    assert(yaw < -1&&std::isfinite(yaw));
    aim.reset();yaw=0;
    aim.updateTick(eye,yaw,pitch,0,false,false,true,selected);
    aim.applyRenderTick(yaw,pitch,.5f);
    assert(yaw==0); // Actual mining still pauses the user's aim engine.
    aim.reset();yaw=0;
    aim.mode=Combat::AimAssist::EngineMode::ADAPTIVE;
    for(int frame=0;frame<120;++frame) {
        aim.updateFrame(eye,yaw,pitch,0,false,false,false,selected,{},true,1./120);
        aim.applyRenderTick(yaw,pitch,.5f);
    }
    assert(yaw < -1&&std::isfinite(yaw));
    aim.reset();yaw=0;
    aim.applyRenderTick(yaw,pitch,.5f);assert(yaw==0);

    Combat::SilentAura aura;
    aura.humanJitter=false;
    const std::vector<Combat::CombatTarget> enemies{{7,bounds}};
    auto action=aura.update(eye,0,0,.5f,1.f,false,false,enemies,.05);
    assert(action.targetEntityId==7&&action.shouldAttack);
    action=aura.update(eye,0,0,.5f,0.f,false,false,enemies,.05);
    assert(action.targetEntityId==7&&!action.shouldAttack);

    Combat::CrystalAura crystal;
    const std::vector<Combat::CrystalPlacement> placements{{{1,0,2},8,2,1,false}};
    auto crystal_action=crystal.update({},20,0,20,8,0,0,2,3,placements,{});
    assert(crystal_action.type==Combat::CrystalAura::ActionType::PLACE_CRYSTAL);
    crystal.maxSelfDamage=1;
    crystal_action=crystal.update({},20,0,20,8,0,0,2,3,placements,{});
    assert(crystal_action.type==Combat::CrystalAura::ActionType::NONE);
    crystal.maxSelfDamage=4;
    crystal_action=crystal.update({},20,0,20,8,0,0,-2,3,placements,{});
    assert(crystal_action.type==Combat::CrystalAura::ActionType::PLACE_CRYSTAL&&crystal_action.hotbarSlot==-2);
    assert(std::abs(Combat::CrystalAura::reduceDamageByEnchants(100,0,0,16)-36)<.001f);

    Combat::JumpReset jump;
    jump.chance=jump.accuracy=100;jump.delayTicks=1;
    assert(!jump.onTick(true)); // No velocity must never produce a jump.
    assert(!jump.onVelocityPacket(.4,.2,true,false));
    assert(jump.onTick(true));assert(!jump.onTick(true));
    assert(!jump.onVelocityPacket(.4,.2,true,false));
    assert(!jump.onTick(false));assert(!jump.onTick(true)); // No stale jump after landing.

    Combat::Triggerbot delayed,early;
    for(auto* engine:{&delayed,&early}) {
        engine->requireMouseDown=false;engine->limitToWeapons=false;
        engine->airCrits=false;engine->mouseOverDelayMaxMs=0;
    }
    delayed.extraDelayMinTicks=delayed.extraDelayMaxTicks=3;
    early.extraDelayMinTicks=early.extraDelayMaxTicks=-2;
    assert(delayed.cooldownOffsetTicks(7)==-3);
    assert(early.cooldownOffsetTicks(7)==2);
    for(int i=0;i<20;++i)assert(delayed.cooldownOffsetTicks(7)==-3);
    // One short wait for the independent engine's existing 50 ms safety timer;
    // all remaining checks are numeric and make no game/system input calls.
    std::this_thread::sleep_for(std::chrono::milliseconds(55));
    auto decide=[](Combat::Triggerbot& engine,float cd) {
        return engine.onTick(true,0,0,0,true,cd,false,true,7,true,false,false,false,false);
    };
    assert(decide(delayed,.85f)==Combat::Triggerbot::TriggerAction::NONE);
    assert(decide(delayed,1.f)==Combat::Triggerbot::TriggerAction::ATTACK);
    assert(decide(early,1.f)==Combat::Triggerbot::TriggerAction::ATTACK);
    early.onAttackProcessed();
    assert(decide(early,1.f)==Combat::Triggerbot::TriggerAction::NONE);
    std::cout<<"Vape decision smoke passed (offline; no game attached).\n";
}
