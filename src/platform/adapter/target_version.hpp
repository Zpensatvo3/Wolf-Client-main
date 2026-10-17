#pragma once
#include <array>
#include <string_view>

// One target per binary. The default retains the existing 1.21.1 build.
#ifndef ONESTEP_MINECRAFT_VERSION
#define ONESTEP_MINECRAFT_VERSION 12101
#endif
#if ONESTEP_MINECRAFT_VERSION != 12101 && ONESTEP_MINECRAFT_VERSION != 12104 && \
    ONESTEP_MINECRAFT_VERSION != 12108 && ONESTEP_MINECRAFT_VERSION != 12111 && \
    ONESTEP_MINECRAFT_VERSION != 26100
#error Unsupported OneStep Minecraft target
#endif
#if ONESTEP_MINECRAFT_VERSION == 26100 && !defined(ONESTEP_MINECRAFT_PORTING)
#error Minecraft 26.1 is an incomplete port; only explicit offline porting builds are allowed
#endif

namespace mc
{
    enum class version { unknown, v1_21_1, v1_21_4, v1_21_8, v1_21_11, v26_1 };
    struct version_info { version id; int build_id; std::string_view name; std::string_view yarn; };
    inline constexpr std::array versions{
        version_info{version::v1_21_1, 12101, "1.21.1", "1.21.1+build.3"},
        version_info{version::v1_21_4, 12104, "1.21.4", "1.21.4+build.8"},
        version_info{version::v1_21_8, 12108, "1.21.8", "1.21.8+build.1"},
        version_info{version::v1_21_11, 12111, "1.21.11", "1.21.11+build.6"}
#ifdef ONESTEP_MINECRAFT_PORTING
        , version_info{version::v26_1, 26100, "26.1", ""}
#endif
    };
    constexpr version parse_version(std::string_view text)
    {
        for (const auto& entry : versions) if (entry.name == text) return entry.id;
        return version::unknown;
    }
    constexpr std::string_view version_name(version id)
    {
        for (const auto& entry : versions) if (entry.id == id) return entry.name;
        return "unknown";
    }
    inline constexpr auto target = [] {
        for (const auto& entry : versions)
            if (entry.build_id == ONESTEP_MINECRAFT_VERSION) return entry;
        return version_info{version::unknown, 0, "unknown", ""};
    }();
    constexpr bool matches_target(version detected) { return detected == target.id; }
}
