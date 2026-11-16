#include "../src/modules/visual/cosmetic_physics.hpp"
#include <cassert>
#include <iostream>
#include <limits>

using namespace cosmetic_physics;
cloth simulate(int fps) {
    signal input; cloth fabric; fabric.reset(.66,1.05);
    wing_rig wings; wings.reset();
    for (int frame=0;frame<=fps*4;++frame) {
        const double t=double(frame)/fps;
        assert(!input.observe({{.12*std::sin(t*2),0,t*4},t,.12*std::sin(t),42,int(t*20)},true) || frame==0);
        const auto count=input.steps(); assert(count>=0 && count<=6);
        fabric.advance(input,count,1); wings.advance(input,count,1);
        for (int x=0;x<cloth::columns;++x) assert(square(fabric.position[x]-fabric.anchor(x))<1e-12);
        for (const auto p:fabric.position) {
            assert(finite(p) && p.y>=-.05 && p.y<=1.46 && square(p)<12);
            if (std::abs(p.x)<.34) assert(p.z<=(p.y>.70 ? cape_back : -.14)+1e-8);
        }
        for (const auto& joint:wings.shoulder) assert(std::isfinite(joint.position) && std::abs(joint.position)<1.3);
    }
    return fabric;
}
int main() {
    // Root sits just behind the torso/jacket, never at the old floating plane.
    assert(std::abs(cape_back + .125) >= .005 && std::abs(cape_back + .125) <= .04);
    for (double width : {.45, .66, .95}) {
        cloth pinned; pinned.reset(width, 1.05);
        for (int x=0;x<cloth::columns;++x) {
            assert(pinned.at(double(x)/(cloth::columns-1),0).z == cape_back);
            assert(pinned.position[x].y == cape_shoulder);
        }
    }
    assert(hat_phase(1, true, false, 1) != hat_phase(2, true, false, 1));
    assert(hat_phase(1, false, false, 1) == hat_phase(2, false, false, 1));
    assert(hat_phase(1, true, true, 1) == hat_phase(2, true, true, 1));
    assert(hat_phase(std::numeric_limits<double>::infinity(),true,false,1) == 0);
    // Idle breeze moves the free fabric, while shoulder pins remain exact.
    signal idle; cloth idle_fabric; idle_fabric.reset(.66,1.05);
    const auto initial_hem=idle_fabric.at(.5,1);
    for (int frame=0;frame<=120;++frame) {
        idle.observe({{},double(frame)/60,0,1,frame/3},true);
        idle_fabric.advance(idle,idle.steps(),1);
        for (int x=0;x<cloth::columns;++x)
            assert(square(idle_fabric.position[x]-idle_fabric.anchor(x))<1e-12);
    }
    assert(square(idle_fabric.at(.5,1)-initial_hem)>1e-6);
    const auto low=simulate(30),normal=simulate(60),high=simulate(144);
    assert(std::sqrt(square(low.at(.5,1)-normal.at(.5,1)))<.10);
    assert(std::sqrt(square(high.at(.5,1)-normal.at(.5,1)))<.10);
    assert(normal.at(.5,1).z<-.30); // Forward movement visibly lifts/trails the hem.
    signal input;
    assert(input.observe({{0,0,0},0,0,1,20},true));
    assert(!input.observe({{0,0,.2},.05,0,1,21},true));
    assert(input.steps()==6);
    assert(input.observe({{20,0,0},.1,0,1,22},true)); // Teleport.
    assert(input.steps()==0);
    assert(input.observe({{20,0,0},.2,0,2,1},true)); // Respawn/new player.
    assert(input.observe({{20,0,0},.6,0,2,2},true)); // Long render pause.
    assert(input.observe({{20,0,0},.5,0,2,2},true)); // Clock regression.
    assert(input.observe({{20,0,0},.7,0,2,3},false));
    assert(input.steps()==0 && input.phase==0 && square(input.velocity)==0);
    assert(!input.observe({{std::numeric_limits<double>::quiet_NaN(),0,0},.8,0,2,4},true));
    assert(!input.initialized);
    cloth resized; resized.reset(.95,1.3);
    assert(std::abs(resized.at(0,0).x+.475)<1e-9 && std::abs(resized.at(1,0).x-.475)<1e-9);
    std::cout<<"Cosmetic physics: pinned cloth, body collision, FPS tolerance, bounded work and reset cases passed\n";
}
