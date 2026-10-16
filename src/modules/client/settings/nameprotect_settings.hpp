#pragma once
#include "../../core/setting_schema.hpp"
#include "../../../services/game/name_protect.hpp"

namespace module_settings
{
    struct nameprotect
    {
        std::string alias="Protected";
        bool operator==(const nameprotect&) const = default;
        static std::span<const modules::settings::descriptor<nameprotect>> schema() { return {}; }
        static bool visit(modules::config_visitor& visitor,nameprotect& live)
        {
            auto candidate=live;
            if (visitor.importing()) visitor.stage_values(true);
            visitor.field("alias",candidate.alias);
            if (visitor.importing() && !services::privacy::valid_alias(candidate.alias))
                visitor.reject("NameProtect alias must contain 1-16 letters, digits or underscores.");
            if (visitor.importing()) visitor.stage_values(false);
            if (!visitor.reading() || !visitor.valid()) return false;
            live=std::move(candidate); return true;
        }
    };
}
