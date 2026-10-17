#define main existing_config_fields_round_trip_test
#include "config_fields_test.cpp"
#undef main
#include "../src/modules/vape/vape_config.hpp"
#include <cassert>
#include <limits>
#include <set>

class vape_fixture final : public modules::module
{
public:
    explicit vape_fixture(modules::vape_kind selected)
        : module("vape_fixture"), kind(selected), settings(vape_settings::defaults(selected)) {}
    modules::vape_kind kind;
    vape_settings::config settings;
    int resets = 0;
    void visit_config(modules::config_visitor& visitor) override
    {
        if (vape_settings::visit(visitor, kind, settings)) ++resets;
    }
};

int main()
{
    using modules::vape_kind;
    using modules::settings::control;
    for (const auto kind : {vape_kind::aim, vape_kind::left, vape_kind::right})
    {
        vape_fixture owner(kind);
        const auto original = owner.settings;
        const auto encoded = owner.export_config_settings();
        assert(owner.validate_config_settings(encoded) && owner.settings == original && owner.resets == 0);
        assert(owner.apply_config_settings(encoded) && owner.settings == original && owner.resets == 1);
        assert(owner.apply_config_settings("{}") && owner.settings == original);
        const int resets = owner.resets;
        const auto before = owner.export_config_settings();
        const char* invalid = kind == vape_kind::aim ? R"({"range":2,"engine":2})" : R"({"min_cps":15,"randomization":3})";
        assert(!owner.validate_config_settings(invalid));
        assert(!owner.apply_config_settings(invalid));
        assert(owner.export_config_settings() == before && owner.resets == resets);
        const char* invalid_type = kind == vape_kind::aim ? R"({"horizontal_speed":2,"weapons_only":"bad"})" : R"({"min_cps":2,"jitter":"bad"})";
        assert(!owner.apply_config_settings(invalid_type));
        assert(owner.export_config_settings() == before && owner.resets == resets);
        const char* nonfinite = kind == vape_kind::aim ? R"({"range":1e999})" : R"({"max_cps":1e999})";
        assert(!owner.apply_config_settings(nonfinite));
        assert(owner.export_config_settings() == before && owner.resets == resets);

        std::set<std::string> keys;
        for (const auto& d : vape_settings::schema(kind))
        {
            assert(d.key && *d.key && keys.insert(d.key).second);
            assert(d.title.english && *d.title.english);
            assert(encoded.find(std::string("\"") + d.key + "\"") != std::string::npos);
            if (d.type == control::range) assert(keys.insert(d.upper_key).second);
            if (d.type == control::choice) assert(!d.choices.empty());
        }
        const std::set<std::string> expected = kind == vape_kind::aim
            ? std::set<std::string>{"horizontal_speed", "vertical_speed", "aim_vertical", "range", "fov", "sort",
                "strafe_increase", "engine", "target_area", "require_mouse", "weapons_only", "protect_blocks"}
            : kind == vape_kind::left
            ? std::set<std::string>{"min_cps", "max_cps", "randomization", "jitter", "weapons_only", "protect_blocks"}
            : std::set<std::string>{"min_cps", "max_cps", "randomization", "jitter", "hold_to_click", "blocks_only", "offhand"};
        assert(keys == expected);
    }

    vape_fixture left(vape_kind::left), right(vape_kind::right), aim(vape_kind::aim);
    assert(left.settings.min_cps == 9 && left.settings.max_cps == 13.5f);
    assert(right.settings.min_cps == 12 && right.settings.max_cps == 18);
    assert(left.apply_config_settings(R"({"min_cps":40,"max_cps":-2})"));
    assert(left.settings.min_cps == 1 && left.settings.max_cps == 30);
    assert(right.apply_config_settings(R"({"hold_to_click":false,"blocks_only":false,"offhand":false})"));
    assert(!right.settings.hold_to_click && !right.settings.blocks_only && !right.settings.offhand);
    // Hidden vertical-speed controls remain serialized and importable.
    assert(aim.apply_config_settings(R"({"aim_vertical":false,"vertical_speed":12,"engine":1,"target_area":0,"sort":4})"));
    assert(!aim.settings.aim_vertical && aim.settings.vertical == 12 && aim.settings.aim_mode == 1 && aim.settings.sort == 4);
    assert(aim.export_config_settings().find("\"vertical_speed\":12") != std::string::npos);
    assert(aim.apply_config_settings(R"({"horizontal_speed":0,"vertical_speed":40,"range":99,"fov":-3})"));
    assert(aim.settings.horizontal == .1f && aim.settings.vertical == 20 && aim.settings.range == 8 && aim.settings.fov == 10);
    const auto saved = aim.settings;
    for (const char* invalid : {R"({"target_area":-1})", R"({"sort":5})", R"({"engine":0.5})"})
    {
        assert(!aim.apply_config_settings(invalid) && aim.settings == saved);
    }
    auto numeric = saved;
    numeric.range = std::numeric_limits<float>::quiet_NaN();
    assert(!vape_settings::normalize(vape_kind::aim, numeric));
    std::cout << "Vape schema: config round trips, defaults, hidden fields, limits and atomic invalid-import rejection passed\n";
}
