#pragma once
#include "../../core/setting_schema.hpp"
namespace module_settings
{
    struct view_model
    {
        float x = .1f, y = -.05f, z = -.1f, scale = .9f;
        float pitch = 0.f, yaw = 0.f, roll = 0.f;
        bool both_hands = true;
        bool operator==(const view_model&) const = default;
        static std::span<const modules::settings::descriptor<view_model>> schema()
        {
            using d = modules::settings::descriptor<view_model>;
            static const d fields[]{
                d::number("x",{"Horizontal"},&view_model::x,-.8f,.8f,"%.2f"),
                d::number("y",{"Vertical"},&view_model::y,-.8f,.8f,"%.2f"),
                d::number("z",{"Depth"},&view_model::z,-.8f,.8f,"%.2f"),
                d::number("scale",{"Scale"},&view_model::scale,.4f,1.5f,"%.2fx"),
                d::number("pitch",{"Pitch"},&view_model::pitch,-90.f,90.f,"%.0f deg"),
                d::number("yaw",{"Yaw"},&view_model::yaw,-90.f,90.f,"%.0f deg"),
                d::number("roll",{"Roll"},&view_model::roll,-90.f,90.f,"%.0f deg"),
                d::toggle("both_hands",{"Both Hands"},&view_model::both_hands)
            }; return fields;
        }
    };
}
