#pragma once
#include "../../modules/client/settings/notifications_settings.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

namespace services::notifications
{
    enum class kind { success, error, info, module_on, module_off };
    struct entry
    {
        std::uint64_t id = 0;
        std::string key, title, body;
        kind type = kind::info;
        double created = 0, updated = 0;
    };
    struct snapshot
    {
        module_settings::notifications settings;
        std::vector<entry> entries; // Newest first; bounded by max_visible.
        double now = 0;
    };
    struct motion { float alpha = 0, slide = 1, remaining = 0; };
    inline motion animation(const entry& item, double now, float duration, bool reduce_motion)
    {
        if (!std::isfinite(now) || !std::isfinite(item.created) || !std::isfinite(item.updated) ||
            !std::isfinite(duration) || duration <= 0 || now < item.created || now < item.updated || now - item.updated >= duration) return {};
        const float age = static_cast<float>(now - item.created);
        const float remaining = std::clamp(duration - static_cast<float>(now - item.updated), 0.f, duration);
        auto ease = [](float t) { t = std::clamp(t, 0.f, 1.f); return 1.f - (1.f - t) * (1.f - t) * (1.f - t); };
        const float exit = std::clamp(remaining / .26f, 0.f, 1.f);
        const float visible = reduce_motion ? 1.f : std::min(ease(age / .22f), exit * exit * (3.f - 2.f * exit));
        return {visible, 1.f - visible, remaining / duration};
    }
    inline float approach(float value, float target, float dt, bool reduce_motion)
    {
        if (reduce_motion || !std::isfinite(value)) return target;
        dt = std::isfinite(dt) ? std::clamp(dt, 0.f, .1f) : 0.f;
        return value + (target - value) * (1.f - std::exp(-14.f * dt));
    }
    inline std::string bounded_text(std::string value, std::size_t maximum)
    {
        if (value.size() > maximum)
        {
            std::size_t end = maximum;
            while (end && (static_cast<unsigned char>(value[end]) & 0xc0u) == 0x80u) --end;
            value.resize(end);
        }
        return value;
    }
    class queue
    {
        std::deque<entry> entries_;
        std::uint64_t next_id_ = 0;
    public:
        static constexpr std::size_t capacity = 16;
        void clear() { entries_.clear(); }
        std::size_t size() const { return entries_.size(); }
        void expire(double now, float duration)
        {
            std::erase_if(entries_, [&](const entry& item) { return now < item.created || now < item.updated || now - item.updated >= duration; });
        }
        void push(entry item, double now, float duration)
        {
            if (!std::isfinite(now) || !std::isfinite(duration) || duration <= 0) return;
            item.key = bounded_text(std::move(item.key), 160);
            item.title = bounded_text(std::move(item.title), 96);
            item.body = bounded_text(std::move(item.body), 768);
            if (item.title.empty() && item.body.empty()) return;
            expire(now, duration);
            if (!item.key.empty())
            {
                auto found = std::find_if(entries_.begin(), entries_.end(), [&](const entry& old) { return old.key == item.key; });
                if (found != entries_.end())
                {
                    item.id = found->id; item.created = found->created; item.updated = now;
                    entries_.erase(found); entries_.push_back(std::move(item)); return;
                }
            }
            item.id = ++next_id_; item.created = item.updated = now;
            if (entries_.size() == capacity) entries_.pop_front();
            entries_.push_back(std::move(item));
        }
        std::vector<entry> collect(double now, const module_settings::notifications& config)
        {
            expire(now, config.duration);
            std::vector<entry> result;
            for (auto it = entries_.rbegin(); it != entries_.rend() && result.size() < static_cast<std::size_t>(std::clamp(config.max_visible, 1, 5)); ++it)
                result.push_back(*it);
            return result;
        }
    };
}
