#pragma once
#include "target_version.hpp"
#include "../../meta_jni.hpp"
#include "runtime_names.hpp"

namespace platform::adapter::bindings
{
    // Only genuine differences belong here. Unchanged bindings stay in the
    // shared SDK instead of copying every class for every Minecraft patch.
    struct v1_21_1
    {
        using fall_distance = jfloat;
        using fov = jdouble;
        static constexpr auto entity_world = jni::string_litteral(ONESTEP_MC_NAME("method_37908"));
        static constexpr auto entity_position = jni::string_litteral(ONESTEP_MC_NAME("method_19538"));
        static constexpr auto camera_position = jni::string_litteral(ONESTEP_MC_NAME("method_19326"));
        static constexpr auto tick_counter = jni::string_litteral(ONESTEP_MC_NAME("field_1728"));
        static constexpr auto component = jni::string_litteral(ONESTEP_MC_NAME("method_57824"));
        static constexpr auto skin_model = jni::string_litteral(ONESTEP_MC_NAME("net/minecraft/class_8685$class_7920"));
        static constexpr auto profile_name = jni::string_litteral("getName");
        static constexpr auto profile_id = jni::string_litteral("getId");
    };
    struct v1_21_4 : v1_21_1
    {
        using fov = jfloat;
        static constexpr auto tick_counter = jni::string_litteral(ONESTEP_MC_NAME("field_52750"));
    };
    struct v1_21_8 : v1_21_4
    {
        using fall_distance = jdouble;
        static constexpr auto component = jni::string_litteral(ONESTEP_MC_NAME("method_58694"));
    };
    struct v1_21_11 : v1_21_8
    {
        static constexpr auto entity_world = jni::string_litteral(ONESTEP_MC_NAME("method_73183"));
        static constexpr auto entity_position = jni::string_litteral(ONESTEP_MC_NAME("method_73189"));
        static constexpr auto camera_position = jni::string_litteral(ONESTEP_MC_NAME("method_71156"));
        static constexpr auto skin_model = jni::string_litteral(ONESTEP_MC_NAME("net/minecraft/class_7920"));
        static constexpr auto profile_name = jni::string_litteral("name");
        static constexpr auto profile_id = jni::string_litteral("id");
    };
    // 26.1 keeps these scalar types but uses the isolated Mojang spelling table.
    // Calls whose contracts changed are normalized explicitly in the SDK/adapter.
    struct v26_1 : v1_21_11 {};
#if ONESTEP_MINECRAFT_VERSION == 12101
    using selected = v1_21_1;
#elif ONESTEP_MINECRAFT_VERSION == 12104
    using selected = v1_21_4;
#elif ONESTEP_MINECRAFT_VERSION == 12108
    using selected = v1_21_8;
#elif ONESTEP_MINECRAFT_VERSION == 12111
    using selected = v1_21_11;
#elif ONESTEP_MINECRAFT_26_1
    using selected = v26_1;
#endif
}
