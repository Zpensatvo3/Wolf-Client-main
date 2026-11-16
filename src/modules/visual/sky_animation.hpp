#pragma once
#include "visual_effects_logic.hpp"

namespace sky_animation
{
    inline int atmosphere(int selected, int panorama)
    {
        if (selected!=1) return std::clamp(selected,0,4);
        return panorama==5 ? 3 : panorama==4 ? 4 : 2;
    }
    struct panorama_transition
    {
        int from=0,to=0;
        double progress=1.;
        void reset(int selected) { from=to=selected; progress=1.; }
        void advance(int requested, double elapsed, bool snap)
        {
            requested=std::clamp(requested,0,5);
            if (snap) { reset(requested); return; }
            // Finish the current pair before accepting the latest request.
            // This bounds texture usage and avoids jumping on rapid changes.
            if (progress>=1. && to!=requested) { from=to; to=requested; progress=0.; }
            if (std::isfinite(elapsed) && elapsed>0.) progress=std::min(1.,progress+elapsed/.65);
        }
        float blend() const { return static_cast<float>(visual_effects_logic::smoothstep(progress)); }
    };
    // Render-thread clock: pause without rewinding, and ignore time spent in
    // another world/menu. Double precision avoids long-session drift jitter.
    struct motion
    {
        double last=-1., time=0., rotation=0.;
        double advance(double now, bool animate, bool drift, bool reduced, float speed)
        {
            const double dt=last>=0. && now>=last && now-last<=.5 ? now-last : 0.;
            last=std::isfinite(now) ? now : -1.;
            const double rate=std::isfinite(speed) ? std::clamp(speed,.1f,3.f) : 1.;
            if (!reduced) {
                // Every shader frequency is a multiple of .01 radians/sec;
                // wrapping at this shared period is continuous for every wave.
                if (animate) time=std::fmod(time+dt*rate,628.3185307179586);
                if (drift) rotation=std::fmod(rotation+dt*rate*.00055,1.);
            }
            return dt;
        }
        void pause() { last=-1.; }
    };
}
