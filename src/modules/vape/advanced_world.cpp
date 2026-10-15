#include "advanced_runtime.hpp"
#include "../../platform/jni/jni_context.hpp"
#include "../../services/game/hotbar_service.hpp"
#include <algorithm>
#include <cmath>

namespace vape_advanced
{
    namespace {
        maps::Vec3d java_vec(vec v) {
            return vape_bindings::construct<maps::Vec3d>(&maps::Vec3d::constructor,v.x,v.y,v.z);
        }
        maps::BlockState block(const frame& f,int x,int y,int z) {
            auto p=vape_bindings::construct<maps::BlockPos>(&maps::BlockPos::constructor,static_cast<jint>(x),static_cast<jint>(y),static_cast<jint>(z));
            return p?f.game.world.getBlockState.call(p):maps::BlockState{};
        }
        bool air(const frame& f,int x,int y,int z) {
            auto state=block(f,x,y,z);return state&&state.isAir.call()==JNI_TRUE;
        }
        bool base(const maps::BlockState& state) {
            if(!state)return false;
            auto block=state.getBlock.call();if(!block)return false;
            return block.is_same_object(maps::Blocks{}.OBSIDIAN.get())||
                block.is_same_object(vape_bindings::Blocks{}.bedrock.get());
        }
        bool occluded(const frame& f,vec point) {
            auto from=java_vec(f.self.eye),to=java_vec(point);
            auto shape=maps::RaycastContext$ShapeType{}.COLLIDER.get();
            auto fluid=maps::RaycastContext$FluidHandling{}.NONE.get();
            if(!from||!to||!shape||!fluid)return true;
            auto context=vape_bindings::construct<maps::RaycastContext>(&maps::RaycastContext::constructor,
                from,to,shape,fluid,maps::Entity(f.game.player.object_instance,false));
            auto hit=context?f.game.world.raycast.call(context):maps::BlockHitResult{};
            if(!hit||platform::jni_context::clear_exception())return true;
            auto type=maps::HitResult(hit.object_instance,false).getType.call();
            if(!type||platform::jni_context::clear_exception())return true;
            if(!type.is_same_object(maps::HitResult$Type{}.BLOCK.get()))return platform::jni_context::clear_exception();
            auto p=maps::HitResult(hit.object_instance,false).getPos.call();if(!p)return true;
            return f.self.eye.distanceTo({p.x.get(),p.y.get(),p.z.get()})+0.025<f.self.eye.distanceTo(point);
        }
        bool place(frame& f,runtime& r,vec coordinate,bool obsidian,int slot) {
            int x=static_cast<int>(coordinate.x),y=static_cast<int>(coordinate.y),z=static_cast<int>(coordinate.z);
            // Crystal uses the actual obsidian/bedrock base; obsidian uses its supporting block below.
            if(obsidian)--y;
            auto position=vape_bindings::construct<maps::BlockPos>(&maps::BlockPos::constructor,static_cast<jint>(x),static_cast<jint>(y),static_cast<jint>(z));
            auto up=maps::Direction{}.UP.get();vec point{x+.5,y+1.0,z+.5};
            if(!position||!up||occluded(f,point))return false;
            const bool offhand=!obsidian&&slot==-2;
            struct restore_on_exit {frame& f;runtime& r;~restore_on_exit(){restore_slot(f,r,kind::crystal);}} cleanup{f,r};
            if(!offhand&&!select_slot(f,r,kind::crystal,slot))return false;
            auto stack=offhand?f.game.player.getOffHandStack.call():f.game.player.getMainHandStack.call();
            if(!key(stack).ends_with(obsidian?".obsidian":".end_crystal")||platform::jni_context::clear_exception())return false;
            auto vector=java_vec(point);
            auto hit=vape_bindings::construct<maps::BlockHitResult>(&maps::BlockHitResult::constructor,vector,up,position,static_cast<jboolean>(JNI_FALSE));
            auto hand=offhand?maps::Hand{}.OFF_HAND.get():maps::Hand{}.MAIN_HAND.get();if(!hit||!hand)return false;
            bypass guard;auto action=f.game.interaction.interactBlock.call(f.game.player,hand,hit);
            const bool failed=platform::jni_context::clear_exception();
            vape_bindings::Result result(action.object_instance,false);
            const bool success=!failed&&result&&result.accepted.is_valid()&&result.accepted.call()==JNI_TRUE&&
                !platform::jni_context::clear_exception();
            if(success)f.game.player.swingHand.call(hand);
            return success;
        }
        float damage(const frame& f,vec crystal,const body& target) {
            auto point=java_vec(crystal);if(!point)return (std::numeric_limits<float>::quiet_NaN)();
            vape_bindings::Explosion explosion;
            if(!explosion.exposure.is_valid())return (std::numeric_limits<float>::quiet_NaN)();
            const float exposure=explosion.exposure.call(point,target.entity);
            if(platform::jni_context::clear_exception()||!std::isfinite(exposure))return (std::numeric_limits<float>::quiet_NaN)();
            return C::CrystalAura::calculateExplosionDamage(crystal,target.position,
                static_cast<float>(target.eye.y-target.position.y),static_cast<float>(target.armor),
                target.toughness,target.resistance,target.blast,exposure,target.protection);
        }
        bool intersects(box a,box b) {
            return a.max.x>b.min.x&&a.min.x<b.max.x&&a.max.y>b.min.y&&a.min.y<b.max.y&&a.max.z>b.min.z&&a.min.z<b.max.z;
        }
    }
    bool crystal_danger(const frame& f,vec coordinate,float& self_damage,float& target_damage,const std::vector<body>& enemies) {
        self_damage=damage(f,coordinate,f.self);target_damage=0;
        if(!std::isfinite(self_damage))return false;
        for(const auto& enemy:enemies) {
            const auto dealt=damage(f,coordinate,enemy);
            if(!std::isfinite(dealt))return false;
            target_damage=(std::max)(target_damage,dealt);
        }
        return true;
    }
    void crystal_tick(frame& f,runtime& r) {
        if(f.using_item||r.slot.owner!=kind::count)return;
        // Expensive armor/enchantment/exposure inputs belong to the crystal
        // tick, not every unrelated attack and outgoing-packet callback.
        body detailed_self;
        if(!read_body(maps::Entity(f.game.player.object_instance,false),detailed_self,true))return;
        f.self=detailed_self;
        auto enemies=targets(f,true);
        if(enemies.empty()) {r.crystal_pending=runtime::crystal_wait::none;return;}
        using Wait=runtime::crystal_wait;
        const bool timely=f.age>=r.crystal_started&&f.age-r.crystal_started<8;
        if(r.crystal_pending==Wait::obsidian) {
            auto state=block(f,static_cast<int>(r.crystal_base.x),static_cast<int>(r.crystal_base.y),static_cast<int>(r.crystal_base.z));
            if(!base(state)&&timely){r.crystal_engaged=true;return;}
            r.crystal_pending=Wait::none;
        } else if(r.crystal_pending==Wait::removal) {
            auto entity=entity_by_id(f,r.crystal_entity);
            if(entity&&entity.isAlive.call()==JNI_TRUE&&timely){r.crystal_engaged=true;return;}
            r.crystal_pending=Wait::none;
        }
        auto slots=hotbar(f);
        std::vector<std::pair<int,vec>> existing;
        std::vector<float> self_damage;
        std::vector<box> occupied;
        auto entities=f.game.world.getEntities.call();auto iterator=entities?entities.iterator.call():maps::Iterator{};
        if(!iterator)return;
        bool spawned=false;
        for(int count=0;count<4096&&iterator.hasNext.call()==JNI_TRUE;++count) {
            jni::frame refs{64};auto object=iterator.next.call();maps::Entity entity(object.object_instance,false);
            if(!entity||entity.isAlive.call()!=JNI_TRUE)continue;
            auto bounds=entity.get_bounding_box();
            if(bounds)occupied.push_back({{bounds.minX.get(),bounds.minY.get(),bounds.minZ.get()},{bounds.maxX.get(),bounds.maxY.get(),bounds.maxZ.get()}});
            if(!entity.is_instance_of<maps::EndCrystalEntity>())continue;
            const auto raw=entity.get_position();vec position{raw.x,raw.y,raw.z};
            if(r.crystal_pending==Wait::spawn&&position.distanceTo(r.crystal_base+vec{.5,1,.5})<.75)spawned=true;
            if(f.self.eye.distanceTo(position)>r.engines.crystal.breakReach||occluded(f,position+vec{0,.5,0}))continue;
            float self=0,target=0;
            if(crystal_danger(f,position,self,target,enemies)&&target>=r.engines.crystal.minDamage) {
                existing.emplace_back(entity.getId.call(),position);self_damage.push_back(self);
            }
            if(platform::jni_context::clear_exception())return;
        }
        if(r.crystal_pending==Wait::spawn) {
            if(!spawned&&timely){r.crystal_engaged=true;return;}
            r.crystal_pending=Wait::none;
        }
        std::vector<C::CrystalPlacement> placements;
        const int radius=static_cast<int>(std::ceil(r.engines.crystal.placeReach));
        const int width=radius*2+1;const int total=width*width*5;
        const int px=static_cast<int>(std::floor(f.self.position.x)),py=static_cast<int>(std::floor(f.self.position.y)),pz=static_cast<int>(std::floor(f.self.position.z));
        // Bounded rotating scan keeps the client tick responsive. Every action is
        // evaluated again from this tick's live world/health/armor, never stale damage.
        for(int checked=0;checked<(std::min)(total,96);++checked) {
            jni::frame refs{128};
            const int index=static_cast<int>(r.crystal_scan++%total);
            int x=px+index%width-radius,z=pz+(index/width)%width-radius,y=py+index/(width*width)-2;
            vec coordinate{static_cast<double>(x),static_cast<double>(y),static_cast<double>(z)};
            vec explosion{x+.5,y+1.,z+.5};
            if(f.self.eye.distanceTo(explosion)>r.engines.crystal.placeReach)continue;
            auto state=block(f,x,y,z);if(!state||!air(f,x,y+1,z))continue;
            const bool needs_obsidian=!base(state);
            if(needs_obsidian&&(!r.engines.crystal.autoObsidian||slots.obsidian<0||state.isAir.call()!=JNI_TRUE||
                air(f,x,y-1,z)))continue;
            const box volume{{static_cast<double>(x),static_cast<double>(y+1),static_cast<double>(z)},
                {x+1.,y+3.,z+1.}};
            bool blocked=false;
            for(const auto& bounds:occupied)if(intersects(volume,bounds)){blocked=true;break;}
            if(blocked)continue;
            if(needs_obsidian) {
                const box base_volume{{static_cast<double>(x),static_cast<double>(y),static_cast<double>(z)},{x+1.,y+1.,z+1.}};
                for(const auto& bounds:occupied)if(intersects(base_volume,bounds)){blocked=true;break;}
                if(blocked)continue;
            }
            float self=0,target=0;
            if(crystal_danger(f,explosion,self,target,enemies))
                placements.push_back({coordinate,target,self,1,needs_obsidian});
            if(platform::jni_context::clear_exception())return;
        }
        const auto result=r.engines.crystal.update(f.self.position,f.self.health,f.self.absorption,static_cast<float>(f.self.armor),f.self.toughness,
            f.self.resistance,f.self.blast,slots.crystal,slots.obsidian,placements,existing,self_damage);
        using A=C::CrystalAura::ActionType;
        if(result.type==A::DETONATE_CRYSTAL) {
            auto target=entity_by_id(f,result.crystalEntityId);
            if(target&&target.is_instance_of<maps::EndCrystalEntity>()) {
                r.crystal_engaged=attack(f,r,target);
                if(r.crystal_engaged) {r.crystal_pending=Wait::removal;r.crystal_entity=result.crystalEntityId;r.crystal_started=f.age;}
            }
        } else if(result.type==A::PLACE_CRYSTAL||result.type==A::PLACE_OBSIDIAN) {
            r.crystal_engaged=place(f,r,result.targetCoord,result.type==A::PLACE_OBSIDIAN,result.hotbarSlot);
            if(r.crystal_engaged) {
                r.crystal_pending=result.type==A::PLACE_OBSIDIAN?Wait::obsidian:Wait::spawn;
                r.crystal_base=result.targetCoord;r.crystal_started=f.age;
            }
        }
    }
    bool reach_confirms(frame& f,runtime& r,const maps::Entity& wanted) {
        if(!wanted)return false;
        auto& reach=r.engines.reach;
        if(r.reach_tick!=f.age) {
            r.sampled_reach=reach.getEffectiveReach(f.game.player.isSprinting.call()==JNI_TRUE,f.liquid);r.reach_tick=f.age;
        }
        const auto direction=C::RayTraceUtil::getVectorForRotation(f.self.yaw,f.self.pitch);
        double best=r.sampled_reach;maps::Entity closest;
        for(const auto& enemy:targets(f)) {
            auto intersection=reach.checkTargetIntersection(f.self.eye,direction,enemy.bounds,
                f.game.player.isSprinting.call()==JNI_TRUE,f.liquid,r.sampled_reach);
            if(intersection&&*intersection<=best&&!occluded(f,f.self.eye+direction**intersection)) {
                best=*intersection;closest=enemy.entity;
            }
        }
        return closest&&closest.is_same_object(wanted)&&!platform::jni_context::clear_exception();
    }
    void reach_update(frame& f,runtime& r) {
        auto& reach=r.engines.reach;
        if(r.reach_tick!=f.age) {
            r.sampled_reach=reach.getEffectiveReach(f.game.player.isSprinting.call()==JNI_TRUE,f.liquid);
            r.reach_tick=f.age;
        }
        const auto direction=C::RayTraceUtil::getVectorForRotation(f.self.yaw,f.self.pitch);
        double best=r.sampled_reach;
        maps::Entity selected;vec hit_point;
        auto vanilla=f.game.instance.crosshairTarget.get();
        if(vanilla) {
            auto point=vanilla.getPos.call();
            if(point)best=(std::min)(best,f.self.eye.distanceTo({point.x.get(),point.y.get(),point.z.get()}));
        }
        for(const auto& enemy:targets(f)) {
            auto intersection=reach.checkTargetIntersection(f.self.eye,direction,enemy.bounds,
                f.game.player.isSprinting.call()==JNI_TRUE,f.liquid,r.sampled_reach);
            if(intersection&&*intersection<best) {
                auto point=f.self.eye+direction**intersection;
                if(!occluded(f,point)) {best=*intersection;selected=enemy.entity;hit_point=point;}
            }
        }
        if(!selected||platform::jni_context::clear_exception())return;
        auto point=java_vec(hit_point);
        auto hit=vape_bindings::construct<vape_bindings::EntityHit>(&vape_bindings::EntityHit::constructor,selected,point);
        if(hit&&!platform::jni_context::clear_exception()) {
            f.game.instance.crosshairTarget.set(maps::HitResult(hit.object_instance,false));
            vape_bindings::Client(f.game.instance.object_instance,false).targetedEntity.set(selected);
        }
    }
}
