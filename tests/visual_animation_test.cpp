#include "../src/modules/visual/visual_effects_logic.hpp"
#include "../src/modules/visual/sky_animation.hpp"
#include <cassert>
#include <limits>

using namespace visual_effects_logic;

int main()
{
    assert(circle_segments(0.)==12);
    assert(circle_segments(std::numeric_limits<double>::quiet_NaN())==12);
    assert(circle_segments(1e20)==128);
    int previous_segments=0;
    for (double radius=.5;radius<=4096.;radius*=1.2) {
        const int count=circle_segments(radius);
        assert(count>=12 && count<=128 && count%4==0 && count>=previous_segments);
        if (count<128) assert(radius*(1.-std::cos(3.14159265358979323846/count))<=.350001);
        previous_segments=count;
    }
    assert(envelope(-.1) == 0. && envelope(0.) == 0.);
    assert(envelope(1.) == 0. && envelope(1.1) == 0.);
    assert(smoothstep(-1.) == 0. && smoothstep(2.) == 1.);
    assert(ease_out(-1.) == 0. && ease_out(2.) == 1.);
    double previous_growth = 0.;
    for (int frame = 0; frame <= 1000; ++frame)
    {
        const double t = frame / 1000.;
        const double alpha = envelope(t), growth = ease_out(t);
        assert(std::isfinite(alpha) && alpha >= 0. && alpha <= 1.);
        assert(growth >= previous_growth && growth <= 1.);
        previous_growth = growth;
        if (t >= .1) assert(envelope(t + .001) <= alpha);
    }
    // Both ends fade gently; a new effect never flashes at full opacity.
    assert(envelope(.001) < .001 && envelope(.999) < .00001);
    assert(envelope(.05) < envelope(.1));
    assert(response(0., 14.) == 0.f && response(-1., 14.) == 0.f);
    assert(response(std::numeric_limits<double>::quiet_NaN(), 14.) == 0.f);
    const auto settle = [](int fps) {
        float value = 1.f;
        for (int frame = 0; frame < fps; ++frame)
            value += (.2f - value) * response(1. / fps, 4.5);
        return value;
    };
    // Health/color interpolation takes the same time at 30, 60 and 144 FPS.
    assert(std::abs(settle(30) - settle(60)) < .00001f);
    assert(std::abs(settle(60) - settle(144)) < .00001f);
    assert(std::abs(settle(30) - (.2 + .8 * std::exp(-4.5))) < .00001);
    assert(response(.2, 14.) > .9f); // A slow frame must not slow the animation clock.

    // Crossing +/-180 degrees must take the short route in either direction.
    const float clockwise=smooth_angle(179.f,-179.f,1./60.,false);
    const float counterclockwise=smooth_angle(-179.f,179.f,1./60.,false);
    assert(clockwise>179.f && clockwise<180.f);
    assert(counterclockwise< -179.f && counterclockwise> -180.f);
    assert(smooth_angle(10.f,350.f,1./60.,true)==-10.f);
    assert(smooth_angle(45.f,90.f,0.,false)==45.f);

    health_animation health;
    health.update(1.f,0.,false);
    assert(health.shown==1.f && health.trailing==1.f);
    health.update(.4f,.1,false);
    assert(health.shown>.4f && health.shown<1.f && health.trailing==1.f);
    health.update(.4f,.1,false);
    assert(health.trailing<1.f && health.trailing>health.shown);
    for (int i=0;i<300;++i) health.update(.4f,1./60.,false);
    assert(std::abs(health.shown-.4f)<.00001f && std::abs(health.trailing-.4f)<.00001f);
    health.update(.9f,1./60.,false);
    assert(health.shown>.4f && health.shown<.9f && health.trailing>=health.shown);
    health.update(.1f,1./60.,true);
    assert(health.shown==.1f && health.trailing==.1f && health.hold==0.);
    health.update(2.f,0.,true);
    assert(health.shown==1.f && health.trailing==1.f);
    health.update(-1.f,0.,true);
    assert(health.shown==0.f && health.trailing==0.f);

    const auto damage_settle=[](int fps) {
        health_animation value;
        value.update(1.f,0.,false);
        for (int i=0;i<fps;++i) value.update(.2f,1./fps,false);
        return value;
    };
    const auto slow=damage_settle(30), fast=damage_settle(144);
    assert(std::abs(slow.shown-fast.shown)<.00001f);
    assert(std::abs(slow.trailing-fast.trailing)<.00001f);

    assert(sky_animation::atmosphere(1,5)==3);
    assert(sky_animation::atmosphere(1,4)==4);
    assert(sky_animation::atmosphere(1,0)==2);
    assert(sky_animation::atmosphere(0,5)==0);
    assert(sky_animation::atmosphere(2,5)==2);
    const auto sky_clock=[](int fps) {
        sky_animation::motion clock;
        for (int i=0;i<=fps*10;++i) clock.advance(double(i)/fps,true,true,false,1.f);
        return clock;
    };
    auto sky30=sky_clock(30),sky144=sky_clock(144);
    assert(std::abs(sky30.time-10.)<1e-9);
    assert(std::abs(sky30.time-sky144.time)<1e-9);
    assert(std::abs(sky30.rotation-sky144.rotation)<1e-9);
    const double phase=sky30.time,rotation=sky30.rotation;
    sky30.advance(10.1,true,true,true,1.f);
    assert(sky30.time==phase && sky30.rotation==rotation);
    sky30.advance(10.2,false,false,false,1.f);
    assert(sky30.time==phase && sky30.rotation==rotation);
    sky30.pause(); sky30.advance(100.,true,true,false,1.f);
    assert(sky30.time==phase && sky30.rotation==rotation);
    sky30.advance(101.,true,true,false,1.f);
    assert(sky30.time==phase); // Long interruption must not jump the sky.
    sky30.advance(101.1,true,true,false,2.f);
    assert(std::abs(sky30.time-phase-.2)<1e-9);
    sky30.advance(std::numeric_limits<double>::quiet_NaN(),true,true,false,1.f);
    assert(std::isfinite(sky30.time) && std::isfinite(sky30.rotation));
    sky_animation::motion wrapping;
    wrapping.time=628.3185307179586-.05;
    wrapping.advance(0.,true,false,false,1.f);
    wrapping.advance(.1,true,false,false,1.f);
    assert(std::abs(wrapping.time-.05)<1e-9);
    sky_animation::panorama_transition transition;
    transition.reset(0);
    transition.advance(4,.325,false);
    assert(transition.from==0 && transition.to==4 && std::abs(transition.blend()-.5f)<.00001f);
    transition.advance(5,0.,false); // Rapid changes cannot replace either visible texture.
    assert(transition.from==0 && transition.to==4 && std::abs(transition.blend()-.5f)<.00001f);
    transition.advance(5,.325,false);
    assert(transition.blend()==1.f);
    transition.advance(5,0.,false);
    assert(transition.from==4 && transition.to==5 && transition.blend()==0.f);
    transition.advance(2,0.,true);
    assert(transition.from==2 && transition.to==2 && transition.blend()==1.f);
    for (int fps : {30,60,144}) {
        transition.reset(0);
        for (int i=0;i<fps;++i) transition.advance(3,1./fps,false);
        assert(transition.to==3 && transition.blend()==1.f);
    }
}
