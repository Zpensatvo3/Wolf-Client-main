#pragma once
#include "../../core/setting_schema.hpp"

namespace module_settings
{
    struct nametag
    {
        float tag_scale = 1.0f;
        float max_distance = 96.0f;
        bool show_health = true;
        bool show_armor = true;
        bool operator==(const nametag&) const = default;
        static std::span<const modules::settings::descriptor<nametag>> schema()
        {
            using model = nametag;
            using setting = modules::settings::descriptor<model>;
            static const setting fields[]{
                setting::number("scale", {"Scale##nametag", "Ölçek##nametag"}, &model::tag_scale, 0.55f, 1.75f, "%.2f"),
                setting::number("max_distance", {"Max Distance##nametag", "Maksimum Mesafe##nametag"}, &model::max_distance, 16.0f, 192.0f, "%.0fm"),
                setting::toggle("show_health", {"Show Health##nametag", "Canı Göster##nametag"}, &model::show_health),
                setting::toggle("show_armor", {"Show Armor Set##nametag", "Zırh Setini Göster##nametag"}, &model::show_armor),
            };
            return fields;
        }
    };
}
