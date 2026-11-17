#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
namespace visual_effects_logic
{
    inline constexpr std::size_t max_particles = 96, max_circles = 8, max_shape_boxes = 24;
    // Bound chord error in framebuffer pixels, with a hard geometry budget.
    inline int circle_segments(double radius_pixels)
    {
        if (!std::isfinite(radius_pixels) || radius_pixels <= 0.) return 12;
        const double angle=std::acos(std::clamp(1.-.35/radius_pixels,-1.,1.));
        if (angle <= 3.14159265358979323846/128.) return 128;
        const int count=static_cast<int>(std::ceil(3.14159265358979323846/angle));
        return std::clamp((count+3)/4*4,12,128);
    }
    inline double smoothstep(double value)
    {
        const double t = std::clamp(value, 0., 1.);
        return t*t*(3.-2.*t);
    }
    inline double ease_out(double value)
    {
        const double t = 1.-std::clamp(value, 0., 1.);
        return 1.-t*t*t;
    }
    // Normalized lifetime: a short soft entrance and a zero-slope fade to zero.
    inline double envelope(double progress, double entrance = .1)
    {
        if (progress <= 0. || progress >= 1.) return 0.;
        return smoothstep(progress / entrance) * (1.-smoothstep(progress));
    }
    // Exponential response composes across frames, including low frame rates.
    inline float response(double elapsed, double speed)
    {
        if (!std::isfinite(elapsed) || elapsed <= 0.) return 0.f;
        return static_cast<float>(-std::expm1(-speed*elapsed));
    }
    inline float smooth_angle(float current, float target, double elapsed, bool snap)
    {
        const float delta=std::remainder(target-current,360.f);
        return std::remainder(current+delta*(snap ? 1.f : response(elapsed,22.)),360.f);
    }
    struct health_animation
    {
        float shown=0.f, trailing=0.f;
        float previous=0.f;
        double hold=0.;
        bool initialized=false;
        void update(float target, double elapsed, bool snap)
        {
            target=std::isfinite(target) ? std::clamp(target,0.f,1.f) : 0.f;
            if (!initialized || snap) {
                shown=trailing=previous=target; hold=0.; initialized=true; return;
            }
            if (target<previous-.002f) hold=.18;
            previous=target;
            const double dt=std::isfinite(elapsed) ? std::max(0.,elapsed) : 0.;
            shown+=(target-shown)*response(dt,14.);
            const double decay=std::max(0.,dt-hold);
            hold=std::max(0.,hold-dt);
            trailing+=(target-trailing)*response(decay,4.5);
            trailing=std::max(trailing,shown);
        }
    };
    class jump_detector
    {
        bool initialized_ = false, grounded_ = false;
    public:
        int sample(bool grounded, bool rising)
        {
            int event = 0;
            if (initialized_) {
                if (grounded_ && !grounded && rising) event = 1;
                else if (!grounded_ && grounded) event = 2;
            }
            initialized_ = true; grounded_ = grounded; return event;
        }
        void reset() { initialized_ = grounded_ = false; }
    };
    class hit_detector
    {
        int ticks_ = -1, id_ = -1, hurt_ = 0;
        float health_ = 0.f;
        double armed_until_ = -1, damage_at_ = -1, time_ = -1;
    public:
        bool pending(double now) const { return now >= 0 && now <= armed_until_; }
        bool observe_attack(int attack_ticks, double now)
        {
            if (!std::isfinite(now) || now < 0 || now < time_ || attack_ticks < 0) {
                reset(); return false;
            }
            const bool attack = ticks_ >= 0 && attack_ticks < ticks_;
            ticks_ = attack_ticks; time_ = now;
            if (attack && id_ >= 0) armed_until_ = now + .5;
            return attack;
        }
        bool sample(int attack_ticks, int id, int hurt, float health, double now)
        {
            if (!std::isfinite(now) || now < 0 || now < time_ || !std::isfinite(health) || health < 0 || hurt < 0 || attack_ticks < 0) {
                reset(); return false;
            }
            observe_attack(attack_ticks, now);
            if (id != id_ || id < 0) {
                id_ = id; hurt_ = hurt; health_ = health; armed_until_ = damage_at_ = -1;
                return false; // Acquiring an already damaged target is not our hit.
            }
            const bool damage = hurt > hurt_ || health < health_ - .01f;
            // Damage and the local cooldown reset can reach the renderer in
            // opposite order. Keep a short observation, not an unbounded hit.
            if (damage) damage_at_ = now;
            const bool hit = pending(now) && damage_at_ >= 0 && now - damage_at_ <= .075;
            hurt_ = hurt; health_ = health;
            if (hit) armed_until_ = damage_at_ = -1;
            return hit;
        }
        void reset() { ticks_ = id_ = -1; hurt_ = 0; health_ = 0; armed_until_ = damage_at_ = time_ = -1; }
    };
}
